/*-------------------------------------------------------------------------
 *
 * pg_variables.c
 *	  Functions, which get or set variables values
 *
 * Copyright (c) 2015-2016, Postgres Professional
 *
 *-------------------------------------------------------------------------
 */
#include "postgres.h"
#include "fmgr.h"
#include "funcapi.h"

#include "access/htup_details.h"
#include "access/xact.h"
#include "catalog/pg_type.h"
#include "parser/scansup.h"
#include "utils/builtins.h"
#include "utils/datum.h"
#include "utils/lsyscache.h"
#include "utils/memutils.h"
#include "utils/timestamp.h"
#include "utils/typcache.h"

#include "pg_variables.h"

PG_MODULE_MAGIC;

/* Functions to work with records */
PG_FUNCTION_INFO_V1(variable_insert);
PG_FUNCTION_INFO_V1(variable_update);
PG_FUNCTION_INFO_V1(variable_delete);

PG_FUNCTION_INFO_V1(variable_select);
PG_FUNCTION_INFO_V1(variable_select_by_value);
PG_FUNCTION_INFO_V1(variable_select_by_values);

/* Functions to work with packages */
PG_FUNCTION_INFO_V1(variable_exists);
PG_FUNCTION_INFO_V1(package_exists);
PG_FUNCTION_INFO_V1(remove_variable);
PG_FUNCTION_INFO_V1(remove_package);
PG_FUNCTION_INFO_V1(remove_packages);
PG_FUNCTION_INFO_V1(get_packages_and_variables);
PG_FUNCTION_INFO_V1(get_packages_stats);

extern void _PG_init(void);
extern void _PG_fini(void);
static void ensurePackagesHashExists(void);
static void getKeyFromName(text *name, char *key);

static Package *getPackage(text *name, bool strict);
static Package *createPackage(text *name, bool is_trans);
static Variable *getVariableInternal(Package *package, text *name,
									 Oid typid, bool is_record, bool strict);
static Variable *createVariableInternal(Package *package, text *name, Oid typid,
										bool is_record, bool is_transactional);
static void removePackageInternal(Package *package);
static void resetVariablesCache(bool with_package);

/* Functions to work with transactional objects */
static void createSavepoint(TransObject *object, TransObjectType type);
static void releaseSavepoint(TransObject *object, TransObjectType type);
static void rollbackSavepoint(TransObject *object, TransObjectType type);

static void copyValue(VarState *src, VarState *dest, Variable *destVar);
static void freeValue(VarState *varstate, bool is_record);
static void removeState(TransObject *object, TransObjectType type,
						TransState *stateToDelete);
static bool isObjectChangedInCurrentTrans(TransObject *object);
static bool isObjectChangedInUpperTrans(TransObject *object);

static void addToChangesStack(TransObject *object, TransObjectType type);
static void addToChangesStackUpperLevel(TransObject *object,
										TransObjectType type);
static void pushChangesStack(void);

static int numOfRegVars(Package *package);
/* Debug function */
static int _numOfTransVars(Package *package);

/* Constructors */
static void makePackHTAB(Package *package, bool is_trans);
static inline ChangedObject *makeChangedObject(TransObject *object,
											   MemoryContext ctx);
static void initObjectHistory(TransObject *object, TransObjectType type);

/* Hook functions */
static void variable_ExecutorEnd(QueryDesc *queryDesc);

#define CHECK_ARGS_FOR_NULL() \
do { \
	if (fcinfo->argnull[0]) \
		ereport(ERROR, \
				(errcode(ERRCODE_INVALID_PARAMETER_VALUE), \
				 errmsg("package name can not be NULL"))); \
	if (fcinfo->argnull[1]) \
		ereport(ERROR, \
				(errcode(ERRCODE_INVALID_PARAMETER_VALUE), \
				 errmsg("variable name can not be NULL"))); \
} while(0)

static HTAB *packagesHash = NULL;
static MemoryContext ModuleContext = NULL;

/* Recent package */
static Package *LastPackage = NULL;
/* Recent variable */
static Variable *LastVariable = NULL;
/* Recent row type id */
static Oid LastTypeId = InvalidOid;

/*
 * Cache sequentially search through hash table status.  It is necessary for
 * clean up if hash_seq_term() wasn't called or if we didn't scan the whole
 * table. In this case we need to manually call hash_seq_term() within
 * variable_ExecutorEnd().
 */
static HASH_SEQ_STATUS *LastHSeqStatus = NULL;

/* Saved hook values for recall */
static ExecutorEnd_hook_type prev_ExecutorEnd = NULL;

/* This stack contains lists of changed variables and packages per each subxact level */
static dlist_head *changesStack = NULL;
static MemoryContext changesStackContext = NULL;

/* Returns a lists of packages and variables changed at current subxact level */
#define get_actual_changes_list() \
	( \
		AssertMacro(changesStack != NULL), \
		(dlist_head_element(ChangesStackNode, node, changesStack)) \
	)
#define pack_hctx(pack, is_trans) \
			(is_trans ? pack->hctxTransact : pack->hctxRegular)
#define pack_htab(pack, is_trans) \
			(is_trans ? pack->varHashTransact : pack->varHashRegular)

#define PGV_MCXT_MAIN		"pg_variables: main memory context"
#define PGV_MCXT_VARS		"pg_variables: variables hash"
#define PGV_MCXT_STACK		"pg_variables: changesStack"
#define PGV_MCXT_STACK_NODE	"pg_variables: changesStackNode"


#ifndef ALLOCSET_DEFAULT_SIZES
#define ALLOCSET_DEFAULT_SIZES \
	ALLOCSET_DEFAULT_MINSIZE, ALLOCSET_DEFAULT_INITSIZE, ALLOCSET_DEFAULT_MAXSIZE
#endif

#ifndef ALLOCSET_START_SMALL_SIZES
#define ALLOCSET_START_SMALL_SIZES \
	ALLOCSET_SMALL_MINSIZE, ALLOCSET_SMALL_INITSIZE, ALLOCSET_DEFAULT_MAXSIZE
#endif


/*
 * Set value of variable, typlen could be 0 if typbyval == true
 */
static void
variable_set(text *package_name, text *var_name,
			 Oid typid, Datum value, bool is_null, bool is_transactional)
{
	Package	   *package;
	Variable   *variable;
	ScalarVar  *scalar;

	package = createPackage(package_name, is_transactional);
	variable = createVariableInternal(package, var_name, typid, false,
									  is_transactional);

	scalar = &(GetActualValue(variable).scalar);

	/* Release memory for variable */
	if (scalar->typbyval == false && scalar->is_null == false)
		pfree(DatumGetPointer(scalar->value));

	scalar->is_null = is_null;
	if (!scalar->is_null)
	{
		MemoryContext oldcxt;

		oldcxt = MemoryContextSwitchTo(pack_hctx(package, is_transactional));
		scalar->value = datumCopy(value, scalar->typbyval, scalar->typlen);
		MemoryContextSwitchTo(oldcxt);
	}
	else
		scalar->value = 0;
}

static Datum
variable_get(text *package_name, text *var_name,
			 Oid typid, bool *is_null, bool strict)
{
	Package	   *package;
	Variable   *variable;
	ScalarVar  *scalar;

	package = getPackage(package_name, strict);
	if (package == NULL)
	{
		*is_null = true;
		return 0;
	}

	variable = getVariableInternal(package, var_name, typid, false, strict);

	if (variable == NULL)
	{
		*is_null = true;
		return 0;
	}

	scalar = &(GetActualValue(variable).scalar);
	*is_null = scalar->is_null;

	return scalar->value;
}


#define VARIABLE_GET_TEMPLATE(pkg_arg, var_arg, strict_arg, type, typid) \
	PG_FUNCTION_INFO_V1(variable_get_##type); \
	Datum \
	variable_get_##type(PG_FUNCTION_ARGS) \
	{ \
		text	   *package_name; \
		text	   *var_name; \
		bool		strict; \
		bool		isnull; \
		Datum		value; \
		\
		CHECK_ARGS_FOR_NULL(); \
		\
		package_name = PG_GETARG_TEXT_PP(pkg_arg); \
		var_name = PG_GETARG_TEXT_PP(var_arg); \
		strict = PG_GETARG_BOOL(strict_arg); \
		\
		value = variable_get(package_name, var_name, \
							 (typid), &isnull, strict); \
		\
		PG_FREE_IF_COPY(package_name, pkg_arg); \
		PG_FREE_IF_COPY(var_name, var_arg); \
		\
		if (!isnull) \
			PG_RETURN_DATUM(value); \
		else \
			PG_RETURN_NULL(); \
	}

/* deprecated functions */
VARIABLE_GET_TEMPLATE(0, 1, 2, int, INT4OID)
VARIABLE_GET_TEMPLATE(0, 1, 2, text, TEXTOID)
VARIABLE_GET_TEMPLATE(0, 1, 2, numeric, NUMERICOID)
VARIABLE_GET_TEMPLATE(0, 1, 2, timestamp, TIMESTAMPOID)
VARIABLE_GET_TEMPLATE(0, 1, 2, timestamptz, TIMESTAMPTZOID)
VARIABLE_GET_TEMPLATE(0, 1, 2, date, DATEOID)
VARIABLE_GET_TEMPLATE(0, 1, 2, jsonb, JSONBOID)

/* current API */
VARIABLE_GET_TEMPLATE(0, 1, 3, any, get_fn_expr_argtype(fcinfo->flinfo, 2))
VARIABLE_GET_TEMPLATE(0, 1, 3, array, get_fn_expr_argtype(fcinfo->flinfo, 2))


#define VARIABLE_SET_TEMPLATE(type, typid) \
	PG_FUNCTION_INFO_V1(variable_set_##type); \
	Datum \
	variable_set_##type(PG_FUNCTION_ARGS) \
	{ \
		text	   *package_name; \
		text	   *var_name; \
		bool		is_transactional; \
		\
		CHECK_ARGS_FOR_NULL(); \
		\
		package_name = PG_GETARG_TEXT_PP(0); \
		var_name = PG_GETARG_TEXT_PP(1); \
		is_transactional = PG_GETARG_BOOL(3); \
		\
		variable_set(package_name, var_name, (typid), \
					 PG_ARGISNULL(2) ? 0 : PG_GETARG_DATUM(2), \
					 PG_ARGISNULL(2), is_transactional); \
		\
		PG_FREE_IF_COPY(package_name, 0); \
		PG_FREE_IF_COPY(var_name, 1); \
		PG_RETURN_VOID(); \
	}


/* deprecated functions */
VARIABLE_SET_TEMPLATE(int, INT4OID)
VARIABLE_SET_TEMPLATE(text, TEXTOID)
VARIABLE_SET_TEMPLATE(numeric, NUMERICOID)
VARIABLE_SET_TEMPLATE(timestamp, TIMESTAMPOID)
VARIABLE_SET_TEMPLATE(timestamptz, TIMESTAMPTZOID)
VARIABLE_SET_TEMPLATE(date, DATEOID)
VARIABLE_SET_TEMPLATE(jsonb, JSONBOID)

/* current API */
VARIABLE_SET_TEMPLATE(any, get_fn_expr_argtype(fcinfo->flinfo, 2))
VARIABLE_SET_TEMPLATE(array, get_fn_expr_argtype(fcinfo->flinfo, 2))


Datum
variable_insert(PG_FUNCTION_ARGS)
{
	text	   *package_name;
	text	   *var_name;
	HeapTupleHeader rec;
	Package	   *package;
	Variable   *variable;
	bool		is_transactional;

	Oid			tupType;
	int32		tupTypmod;
	TupleDesc	tupdesc = NULL;
	RecordVar  *record;

	/* Checks */
	CHECK_ARGS_FOR_NULL();

	if (PG_ARGISNULL(2))
		ereport(ERROR,
				(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
				 errmsg("record argument can not be NULL")));

	/* Get arguments */
	package_name = PG_GETARG_TEXT_PP(0);
	var_name = PG_GETARG_TEXT_PP(1);
	rec = PG_GETARG_HEAPTUPLEHEADER(2);
	is_transactional = PG_GETARG_BOOL(3);

	/* Get cached package */
	if (LastPackage == NULL ||
		VARSIZE_ANY_EXHDR(package_name) != strlen(GetName(LastPackage)) ||
		strncmp(VARDATA_ANY(package_name), GetName(LastPackage),
				VARSIZE_ANY_EXHDR(package_name)) != 0 ||
		!pack_htab(LastPackage, is_transactional))
	{
		package = createPackage(package_name, is_transactional);
		LastPackage = package;
		LastVariable = NULL;
	}
	else
		package = LastPackage;

	/* Get cached variable */
	if (LastVariable == NULL ||
		VARSIZE_ANY_EXHDR(var_name) != strlen(GetName(LastVariable)) ||
		strncmp(VARDATA_ANY(var_name), GetName(LastVariable),
				VARSIZE_ANY_EXHDR(var_name)) != 0)
	{
		variable = createVariableInternal(package, var_name, RECORDOID,
										  true, is_transactional);
		LastVariable = variable;
	}
	else
	{
		TransObject *transObj;

		if (LastVariable->is_transactional != is_transactional)
		{
			char		key[NAMEDATALEN];

			getKeyFromName(var_name, key);
			ereport(ERROR,
					(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
					 errmsg("variable \"%s\" already created as %sTRANSACTIONAL",
							key, LastVariable->is_transactional ? "" : "NOT ")));
		}

		variable = LastVariable;
		transObj = &variable->transObject;

		if (variable->is_transactional &&
			!isObjectChangedInCurrentTrans(transObj))
		{
			createSavepoint(transObj, TRANS_VARIABLE);
			addToChangesStack(transObj, TRANS_VARIABLE);
		}
	}

	/* Insert a record */
	tupType = HeapTupleHeaderGetTypeId(rec);
	tupTypmod = HeapTupleHeaderGetTypMod(rec);

	record = &(GetActualValue(variable).record);
	if (!record->tupdesc)
	{
		/*
		 * This is the first record for the var_name. Initialize record.
		 */
		tupdesc = lookup_rowtype_tupdesc(tupType, tupTypmod);
		init_record(record, tupdesc, variable);
	}
	else if (LastTypeId == RECORDOID || !OidIsValid(LastTypeId) ||
		LastTypeId != tupType)
	{
		/*
		 * We need to check attributes of the new row if this is a transient
		 * record type or if last record has different id.
		 */
		tupdesc = lookup_rowtype_tupdesc(tupType, tupTypmod);
		check_attributes(variable, tupdesc);
	}

	LastTypeId = tupType;

	insert_record(variable, rec);

	/* Release resources */
	if (tupdesc)
		ReleaseTupleDesc(tupdesc);

	PG_FREE_IF_COPY(package_name, 0);
	PG_FREE_IF_COPY(var_name, 1);

	PG_RETURN_VOID();
}

Datum
variable_update(PG_FUNCTION_ARGS)
{
	text	   *package_name;
	text	   *var_name;
	HeapTupleHeader rec;
	Package	   *package;
	Variable   *variable;
	TransObject *transObject;
	bool		res;
	Oid			tupType;
	int32		tupTypmod;

	/* Checks */
	CHECK_ARGS_FOR_NULL();

	if (PG_ARGISNULL(2))
		ereport(ERROR,
				(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
				 errmsg("record argument can not be NULL")));

	/* Get arguments */
	package_name = PG_GETARG_TEXT_PP(0);
	var_name = PG_GETARG_TEXT_PP(1);
	rec = PG_GETARG_HEAPTUPLEHEADER(2);

	/* Get cached package */
	if (LastPackage == NULL ||
		VARSIZE_ANY_EXHDR(package_name) != strlen(GetName(LastPackage)) ||
		strncmp(VARDATA_ANY(package_name), GetName(LastPackage),
				VARSIZE_ANY_EXHDR(package_name)) != 0)
	{
		package = getPackage(package_name, true);
		LastPackage = package;
		LastVariable = NULL;
	}
	else
		package = LastPackage;

	/* Get cached variable */
	if (LastVariable == NULL ||
		VARSIZE_ANY_EXHDR(var_name) != strlen(GetName(LastVariable)) ||
		strncmp(VARDATA_ANY(var_name), GetName(LastVariable),
				VARSIZE_ANY_EXHDR(var_name)) != 0)
	{
		variable = getVariableInternal(package, var_name, RECORDOID, true,
									   true);
		LastVariable = variable;
	}
	else
		variable = LastVariable;

	transObject = &variable->transObject;
	if (variable->is_transactional &&
		!isObjectChangedInCurrentTrans(transObject))
	{
		createSavepoint(transObject, TRANS_VARIABLE);
		addToChangesStack(transObject, TRANS_VARIABLE);
	}

	/* Update a record */
	tupType = HeapTupleHeaderGetTypeId(rec);
	tupTypmod = HeapTupleHeaderGetTypMod(rec);

	if (LastTypeId == RECORDOID || !OidIsValid(LastTypeId) ||
		LastTypeId != tupType)
	{
		TupleDesc	tupdesc = NULL;

		tupdesc = lookup_rowtype_tupdesc(tupType, tupTypmod);
		check_attributes(variable, tupdesc);
		ReleaseTupleDesc(tupdesc);
	}

	LastTypeId = tupType;

	res = update_record(variable, rec);

	/* Release resources */
	PG_FREE_IF_COPY(package_name, 0);
	PG_FREE_IF_COPY(var_name, 1);

	PG_RETURN_BOOL(res);
}

Datum
variable_delete(PG_FUNCTION_ARGS)
{
	text	   *package_name;
	text	   *var_name;
	Oid			value_type;
	Datum		value;
	bool		value_is_null = PG_ARGISNULL(2);
	Package	   *package;
	Variable   *variable;
	TransObject *transObject;
	bool		res;

	CHECK_ARGS_FOR_NULL();

	/* Get arguments */
	package_name = PG_GETARG_TEXT_PP(0);
	var_name = PG_GETARG_TEXT_PP(1);

	if (!value_is_null)
	{
		value_type = get_fn_expr_argtype(fcinfo->flinfo, 2);
		value = PG_GETARG_DATUM(2);
	}
	else
	{
		value_type = InvalidOid;
		value = 0;
	}

	/* Get cached package */
	if (LastPackage == NULL ||
		VARSIZE_ANY_EXHDR(package_name) != strlen(GetName(LastPackage)) ||
		strncmp(VARDATA_ANY(package_name), GetName(LastPackage),
				VARSIZE_ANY_EXHDR(package_name)) != 0)
	{
		package = getPackage(package_name, true);
		LastPackage = package;
		LastVariable = NULL;
	}
	else
		package = LastPackage;

	/* Get cached variable */
	if (LastVariable == NULL ||
		VARSIZE_ANY_EXHDR(var_name) != strlen(GetName(LastVariable)) ||
		strncmp(VARDATA_ANY(var_name), GetName(LastVariable),
				VARSIZE_ANY_EXHDR(var_name)) != 0)
	{
		variable = getVariableInternal(package, var_name, RECORDOID, true,
									   true);
		LastVariable = variable;
	}
	else
		variable = LastVariable;

	transObject = &variable->transObject;
	if (variable->is_transactional &&
		!isObjectChangedInCurrentTrans(transObject))
	{
		createSavepoint(transObject, TRANS_VARIABLE);
		addToChangesStack(transObject, TRANS_VARIABLE);
	}

	/* Delete a record */
	if (!value_is_null)
		check_record_key(variable, value_type);
	res = delete_record(variable, value, value_is_null);

	/* Release resources */
	PG_FREE_IF_COPY(package_name, 0);
	PG_FREE_IF_COPY(var_name, 1);

	PG_RETURN_BOOL(res);
}

Datum
variable_select(PG_FUNCTION_ARGS)
{
	FuncCallContext *funcctx;
	HASH_SEQ_STATUS *rstat;
	HashRecordEntry *item;

	if (SRF_IS_FIRSTCALL())
	{
		text	   *package_name;
		text	   *var_name;
		Package	   *package;
		Variable   *variable;
		MemoryContext oldcontext;
		RecordVar  *record;

		CHECK_ARGS_FOR_NULL();

		/* Get arguments */
		package_name = PG_GETARG_TEXT_PP(0);
		var_name = PG_GETARG_TEXT_PP(1);

		package = getPackage(package_name, true);
		variable = getVariableInternal(package, var_name, RECORDOID, true,
									   true);

		record = &(GetActualValue(variable).record);

		funcctx = SRF_FIRSTCALL_INIT();
		oldcontext = MemoryContextSwitchTo(funcctx->multi_call_memory_ctx);

		funcctx->tuple_desc = CreateTupleDescCopy(record->tupdesc);

		rstat = (HASH_SEQ_STATUS *) palloc0(sizeof(HASH_SEQ_STATUS));
		hash_seq_init(rstat, record->rhash);
		funcctx->user_fctx = rstat;

		MemoryContextSwitchTo(oldcontext);
		PG_FREE_IF_COPY(package_name, 0);
		PG_FREE_IF_COPY(var_name, 1);

		LastHSeqStatus = rstat;
	}

	funcctx = SRF_PERCALL_SETUP();

	/* Get next hash record */
	rstat = (HASH_SEQ_STATUS *) funcctx->user_fctx;
	item = (HashRecordEntry *) hash_seq_search(rstat);
	if (item != NULL)
	{
		Datum		result;

		result = HeapTupleGetDatum(item->tuple);

		SRF_RETURN_NEXT(funcctx, result);
	}
	else
	{
		LastHSeqStatus = NULL;
		pfree(rstat);
		SRF_RETURN_DONE(funcctx);
	}
}

Datum
variable_select_by_value(PG_FUNCTION_ARGS)
{
	text	   *package_name;
	text	   *var_name;
	Oid			value_type;
	Datum		value;
	bool		value_is_null = PG_ARGISNULL(2);
	Package	   *package;
	Variable   *variable;

	HashRecordEntry *item;
	RecordVar  *record;
	HashRecordKey k;
	bool		found;

	CHECK_ARGS_FOR_NULL();

	/* Get arguments */
	package_name = PG_GETARG_TEXT_PP(0);
	var_name = PG_GETARG_TEXT_PP(1);

	if (!value_is_null)
	{
		value_type = get_fn_expr_argtype(fcinfo->flinfo, 2);
		value = PG_GETARG_DATUM(2);
	}
	else
	{
		value_type = InvalidOid;
		value = 0;
	}

	package = getPackage(package_name, true);
	variable = getVariableInternal(package, var_name, RECORDOID, true, true);

	if (!value_is_null)
		check_record_key(variable, value_type);

	record = &(GetActualValue(variable).record);

	/* Search a record */
	k.value = value;
	k.is_null = value_is_null;
	k.hash_proc = &record->hash_proc;
	k.cmp_proc = &record->cmp_proc;

	item = (HashRecordEntry *) hash_search(record->rhash, &k,
										   HASH_FIND, &found);

	PG_FREE_IF_COPY(package_name, 0);
	PG_FREE_IF_COPY(var_name, 1);

	if (found)
		PG_RETURN_DATUM(HeapTupleGetDatum(item->tuple));
	else
		PG_RETURN_NULL();
}

/* Structure for variable_select_by_values() */
typedef struct
{
	Variable   *variable;
	ArrayIterator iterator;
}			VariableIteratorRec;

Datum
variable_select_by_values(PG_FUNCTION_ARGS)
{
	FuncCallContext *funcctx;
	VariableIteratorRec *var;
	Datum		value;
	HashRecordEntry *item;
	bool		isnull;

	if (SRF_IS_FIRSTCALL())
	{
		text	   *package_name;
		text	   *var_name;
		ArrayType  *values;
		Package	   *package;
		Variable   *variable;
		MemoryContext oldcontext;

		/* Checks */
		CHECK_ARGS_FOR_NULL();

		if (PG_ARGISNULL(2))
			ereport(ERROR,
					(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
					 errmsg("array argument can not be NULL")));

		values = PG_GETARG_ARRAYTYPE_P(2);
		if (ARR_NDIM(values) > 1)
			ereport(ERROR,
					(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
					 errmsg("searching for elements in multidimensional arrays is not supported")));

		/* Get arguments */
		package_name = PG_GETARG_TEXT_PP(0);
		var_name = PG_GETARG_TEXT_PP(1);

		package = getPackage(package_name, true);
		variable = getVariableInternal(package, var_name, RECORDOID, true,
									   true);

		check_record_key(variable, ARR_ELEMTYPE(values));

		funcctx = SRF_FIRSTCALL_INIT();
		oldcontext = MemoryContextSwitchTo(funcctx->multi_call_memory_ctx);

		funcctx->tuple_desc = CreateTupleDescCopy(GetActualValue(variable).record.tupdesc);

		var = (VariableIteratorRec *) palloc(sizeof(VariableIteratorRec));
		var->iterator = array_create_iterator(values, 0, NULL);
		var->variable = variable;
		funcctx->user_fctx = var;

		MemoryContextSwitchTo(oldcontext);
		PG_FREE_IF_COPY(package_name, 0);
		PG_FREE_IF_COPY(var_name, 1);
	}

	funcctx = SRF_PERCALL_SETUP();
	var = (VariableIteratorRec *) funcctx->user_fctx;

	/* Get next array element */
	while (array_iterate(var->iterator, &value, &isnull))
	{
		HashRecordKey k;
		bool		found;
		RecordVar  *record;

		record = &(GetActualValue(var->variable).record);
		/* Search a record */
		k.value = value;
		k.is_null = isnull;
		k.hash_proc = &record->hash_proc;
		k.cmp_proc = &record->cmp_proc;

		item = (HashRecordEntry *) hash_search(record->rhash, &k,
											   HASH_FIND, &found);
		if (found)
		{
			Datum		result;

			result = HeapTupleGetDatum(item->tuple);

			SRF_RETURN_NEXT(funcctx, result);
		}
	}

	array_free_iterator(var->iterator);
	pfree(var);
	SRF_RETURN_DONE(funcctx);
}

/*
 * Check if variable exists.
 */
Datum
variable_exists(PG_FUNCTION_ARGS)
{
	text	   *package_name;
	text	   *var_name;
	Package	   *package;
	Variable   *variable = NULL;
	char		key[NAMEDATALEN];
	bool		found = false;

	CHECK_ARGS_FOR_NULL();

	package_name = PG_GETARG_TEXT_PP(0);
	var_name = PG_GETARG_TEXT_PP(1);

	package = getPackage(package_name, false);
	if (package == NULL)
	{
		PG_FREE_IF_COPY(package_name, 0);
		PG_FREE_IF_COPY(var_name, 1);

		PG_RETURN_BOOL(false);
	}

	getKeyFromName(var_name, key);

	if (package->varHashRegular)
		variable = (Variable *) hash_search(package->varHashRegular,
											key, HASH_FIND, &found);
	if (!found && package->varHashTransact)
		variable = (Variable *) hash_search(package->varHashTransact,
											key, HASH_FIND, &found);

	PG_FREE_IF_COPY(package_name, 0);
	PG_FREE_IF_COPY(var_name, 1);

	PG_RETURN_BOOL(variable ? GetActualState(variable)->is_valid : false);
}

/*
 * Check if package exists.
 */
Datum
package_exists(PG_FUNCTION_ARGS)
{
	text	   *package_name;
	bool		res;

	if (PG_ARGISNULL(0))
		ereport(ERROR,
				(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
				 errmsg("package name can not be NULL")));

	package_name = PG_GETARG_TEXT_PP(0);

	res = getPackage(package_name, false) != NULL;

	PG_FREE_IF_COPY(package_name, 0);
	PG_RETURN_BOOL(res);
}

/*
 * Remove variable from package by name.
 */
Datum
remove_variable(PG_FUNCTION_ARGS)
{
	text		   *package_name;
	text		   *var_name;
	Package		   *package;
	Variable	   *variable;
	TransObject	   *transObject;

	CHECK_ARGS_FOR_NULL();

	package_name = PG_GETARG_TEXT_PP(0);
	var_name = PG_GETARG_TEXT_PP(1);

	package = getPackage(package_name, true);
	variable = getVariableInternal(package, var_name, InvalidOid, false, true);

	/* Add package to changes list, so we can remove it if it is empty */
	if (!isObjectChangedInCurrentTrans(&package->transObject))
	{
		createSavepoint(&package->transObject, TRANS_PACKAGE);
		addToChangesStack(&package->transObject, TRANS_PACKAGE);
	}

	transObject = &variable->transObject;
	if (variable->is_transactional)
	{
		if (!isObjectChangedInCurrentTrans(transObject))
		{
			createSavepoint(transObject, TRANS_VARIABLE);
			addToChangesStack(transObject, TRANS_VARIABLE);
		}
		GetActualState(variable)->is_valid = false;
		numOfTransVars(package)--;
	}
	else
		removeObject(&variable->transObject, TRANS_VARIABLE);

	Assert (numOfTransVars(package) == _numOfTransVars(package));

	resetVariablesCache(false);

	PG_FREE_IF_COPY(package_name, 0);
	PG_FREE_IF_COPY(var_name, 1);

	PG_RETURN_VOID();
}

/*
 * Remove package by name.
 */
Datum
remove_package(PG_FUNCTION_ARGS)
{
	Package	   *package;
	text	   *package_name;

	if (PG_ARGISNULL(0))
		ereport(ERROR,
				(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
				 errmsg("package name can not be NULL")));

	package_name = PG_GETARG_TEXT_PP(0);

	package = getPackage(package_name, true);
	removePackageInternal(package);

	resetVariablesCache(true);

	PG_FREE_IF_COPY(package_name, 0);
	PG_RETURN_VOID();
}

static void
removePackageInternal(Package *package)
{
	TransObject *transObject;

	/* All regular variables will be freed */
	if (package->hctxRegular)
	{
		MemoryContextDelete(package->hctxRegular);
		package->hctxRegular = NULL;
		package->varHashRegular = NULL;
	}

	/* Add to changes list */
	transObject = &package->transObject;
	if (!isObjectChangedInCurrentTrans(transObject))
	{
		createSavepoint(transObject, TRANS_PACKAGE);
		addToChangesStack(transObject, TRANS_PACKAGE);
	}
	GetActualState(package)->is_valid = false;
	numOfTransVars(package) = 0;
}

static bool
isPackageEmpty(Package *package)
{
	int var_num = 0;
	
	if (package->varHashTransact)
		var_num += hash_get_num_entries(package->varHashTransact);

	if (package->varHashRegular)
		var_num += hash_get_num_entries(package->varHashRegular);

	return var_num == 0;
}

/*
 * Reset cache variables to their default values. It is necessary to do in case
 * of some changes: removing, rollbacking, etc.
 */
static void
resetVariablesCache(bool with_package)
{
	/* Remove package and variable from cache */
	if (with_package)
		LastPackage = NULL;
	LastVariable = NULL;
	LastTypeId = InvalidOid;
}

/*
 * Remove all packages and variables.
 * Memory context will be released after committing.
 */
Datum
remove_packages(PG_FUNCTION_ARGS)
{
	Package	   *package;
	HASH_SEQ_STATUS pstat;

	/* There is no any packages and variables */
	if (packagesHash == NULL)
		PG_RETURN_VOID();

	/* Get packages list */
	hash_seq_init(&pstat, packagesHash);
	while ((package = (Package *) hash_seq_search(&pstat)) != NULL)
	{
		removePackageInternal(package);
	}

	resetVariablesCache(true);

	PG_RETURN_VOID();
}

/*
 * Structure for get_packages_and_variables().
 */
typedef struct
{
	char	   *package;
	char	   *variable;
	bool		is_transactional;
}			VariableRec;

/*
 * Get list of assigned packages and variables.
 */
Datum
get_packages_and_variables(PG_FUNCTION_ARGS)
{
	FuncCallContext *funcctx;
	VariableRec *recs;
	MemoryContext oldcontext;

	if (SRF_IS_FIRSTCALL())
	{
		TupleDesc	tupdesc;

		funcctx = SRF_FIRSTCALL_INIT();
		oldcontext = MemoryContextSwitchTo(funcctx->multi_call_memory_ctx);

		/* Build a tuple descriptor for our result type */
		if (get_call_result_type(fcinfo, NULL, &tupdesc) != TYPEFUNC_COMPOSITE)
			ereport(ERROR,
					(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
					 errmsg("function returning record called in context "
							"that cannot accept type record")));

		funcctx->tuple_desc = BlessTupleDesc(tupdesc);

		/*
		 * Get all packages and variables names and save them into
		 * funcctx->user_fctx.
		 */
		if (packagesHash)
		{
			Package	   *package;
			HASH_SEQ_STATUS pstat;
			int			mRecs = NUMVARIABLES,
						nRecs = 0;

			recs = (VariableRec *) palloc0(sizeof(VariableRec) * mRecs);

			/* Get packages list */
			hash_seq_init(&pstat, packagesHash);
			while ((package = (Package *) hash_seq_search(&pstat)) != NULL)
			{
				Variable   *variable;
				HASH_SEQ_STATUS vstat;
				int			i;

				/* Skip packages marked as deleted */
				if (!GetActualState(package)->is_valid)
					continue;

				/* Get variables list for package */
				for (i = 0; i < 2; i++)
				{
					HTAB *htab = pack_htab(package, i);
					if (!htab)
						continue;
					hash_seq_init(&vstat, htab);
					while ((variable =
							(Variable *) hash_seq_search(&vstat)) != NULL)
					{
						if (!GetActualState(variable)->is_valid)
							continue;

						/* Resize recs if necessary */
						if (nRecs >= mRecs)
						{
							mRecs *= 2;
							recs = (VariableRec *) repalloc(recs,
															sizeof(VariableRec) * mRecs);
						}

						recs[nRecs].package = GetName(package);
						recs[nRecs].variable = GetName(variable);
						recs[nRecs].is_transactional = variable->is_transactional;
						nRecs++;
					}
				}
			}

			funcctx->user_fctx = recs;
			funcctx->max_calls = nRecs;
		}
		else
			funcctx->max_calls = 0;

		MemoryContextSwitchTo(oldcontext);
	}

	funcctx = SRF_PERCALL_SETUP();

	/* Get the saved recs */
	recs = (VariableRec *) funcctx->user_fctx;

	if (funcctx->call_cntr < funcctx->max_calls)
	{
		Datum		values[3];
		bool		nulls[3];
		HeapTuple	tuple;
		Datum		result;
		int			i = funcctx->call_cntr;

		memset(nulls, 0, sizeof(nulls));

		oldcontext = MemoryContextSwitchTo(funcctx->multi_call_memory_ctx);

		values[0] = PointerGetDatum(cstring_to_text(recs[i].package));
		values[1] = PointerGetDatum(cstring_to_text(recs[i].variable));
		values[2] = recs[i].is_transactional;

		tuple = heap_form_tuple(funcctx->tuple_desc, values, nulls);
		result = HeapTupleGetDatum(tuple);

		MemoryContextSwitchTo(oldcontext);

		SRF_RETURN_NEXT(funcctx, result);
	}
	else
		SRF_RETURN_DONE(funcctx);
}

static void
getMemoryTotalSpace(MemoryContext context, int level, Size *totalspace)
{
#if PG_VERSION_NUM >= 90600
	MemoryContext child;
	MemoryContextCounters totals;

	AssertArg(MemoryContextIsValid(context));

	/* Examine the context itself */
	memset(&totals, 0, sizeof(totals));
#if PG_VERSION_NUM >= 110000
	(*context->methods->stats) (context, NULL, NULL, &totals);
#else
	(*context->methods->stats) (context, level, false, &totals);
#endif
	*totalspace += totals.totalspace;

	/*
	 * Examine children.
	 */
	for (child = context->firstchild; child != NULL; child = child->nextchild)
		getMemoryTotalSpace(child, level + 1, totalspace);
#else
	*totalspace = 0;
#endif
}

/*
 * Get list of assigned packages and used memory in bytes.
 */
Datum
get_packages_stats(PG_FUNCTION_ARGS)
{
	FuncCallContext *funcctx;
	MemoryContext oldcontext;
	HASH_SEQ_STATUS *pstat;
	Package	   *package;

	if (SRF_IS_FIRSTCALL())
	{
		TupleDesc	tupdesc;

		funcctx = SRF_FIRSTCALL_INIT();
		oldcontext = MemoryContextSwitchTo(funcctx->multi_call_memory_ctx);

		/* Build a tuple descriptor for our result type */
		if (get_call_result_type(fcinfo, NULL, &tupdesc) != TYPEFUNC_COMPOSITE)
			ereport(ERROR,
					(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
					 errmsg("function returning record called in context "
							"that cannot accept type record")));

		funcctx->tuple_desc = BlessTupleDesc(tupdesc);

		/*
		 * Get all packages and variables names and save them into
		 * funcctx->user_fctx.
		 */
		if (packagesHash)
		{
			pstat = (HASH_SEQ_STATUS *) palloc0(sizeof(HASH_SEQ_STATUS));
			/* Get packages list */
			hash_seq_init(pstat, packagesHash);

			funcctx->user_fctx = pstat;

			LastHSeqStatus = pstat;
		}
		else
			funcctx->user_fctx = NULL;

		MemoryContextSwitchTo(oldcontext);
	}

	funcctx = SRF_PERCALL_SETUP();
	if (funcctx->user_fctx == NULL)
		SRF_RETURN_DONE(funcctx);

	/* Get packages list */
	pstat = (HASH_SEQ_STATUS *) funcctx->user_fctx;

	package = (Package *) hash_seq_search(pstat);
	if (package != NULL)
	{
		Datum		values[2];
		bool		nulls[2];
		HeapTuple	tuple;
		Datum		result;
		Size		totalSpace = 0,
					regularSpace = 0,
					transactSpace = 0;

		memset(nulls, 0, sizeof(nulls));

		/* Fill data */
		values[0] = PointerGetDatum(cstring_to_text(GetName(package)));

		if (package->hctxRegular)
			getMemoryTotalSpace(package->hctxRegular, 0, &regularSpace);
		if (package->hctxTransact)
			getMemoryTotalSpace(package->hctxTransact, 0, &transactSpace);

		totalSpace = regularSpace + transactSpace;
		values[1] = Int64GetDatum(totalSpace);

		/* Data are ready */
		tuple = heap_form_tuple(funcctx->tuple_desc, values, nulls);
		result = HeapTupleGetDatum(tuple);

		SRF_RETURN_NEXT(funcctx, result);
	}
	else
	{
		LastHSeqStatus = NULL;
		pfree(pstat);
		SRF_RETURN_DONE(funcctx);
	}
}

/*
 * Static functions
 */

static void
getKeyFromName(text *name, char *key)
{
	int			key_len = VARSIZE_ANY_EXHDR(name);

	if (key_len >= NAMEDATALEN - 1)
		ereport(ERROR,
				(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
				 errmsg("name \"%.*s\" is too long",
						key_len, VARDATA_ANY(name))));

	strncpy(key, VARDATA_ANY(name), key_len);
	key[key_len] = '\0';
}

static void
ensurePackagesHashExists(void)
{
	HASHCTL		ctl;

	if (packagesHash)
		return;

	ModuleContext = AllocSetContextCreate(CacheMemoryContext,
										  PGV_MCXT_MAIN,
										  ALLOCSET_DEFAULT_SIZES);

	ctl.keysize = NAMEDATALEN;
	ctl.entrysize = sizeof(Package);
	ctl.hcxt = ModuleContext;

	packagesHash = hash_create("Packages hash",
							   NUMPACKAGES, &ctl,
							   HASH_ELEM | HASH_CONTEXT);
}

/*
 * Initialize a hash table with proper vars type
 */
static void
makePackHTAB(Package *package, bool is_trans)
{
	HASHCTL			ctl;
	char			hash_name[BUFSIZ];
	HTAB		  **htab;
	MemoryContext  *context;

	htab = is_trans ? &package->varHashTransact : &package->varHashRegular;
	context = is_trans ? &package->hctxTransact : &package->hctxRegular;

	*context = AllocSetContextCreate(ModuleContext, PGV_MCXT_VARS,
									 ALLOCSET_DEFAULT_SIZES);

	snprintf(hash_name, BUFSIZ, "%s variables hash for package \"%s\"",
			 is_trans ? "Transactional" : "Regular", GetName(package));
	ctl.keysize = NAMEDATALEN;
	ctl.entrysize = sizeof(Variable);
	ctl.hcxt = *context;

	*htab = hash_create(hash_name, NUMVARIABLES, &ctl,
						HASH_ELEM | HASH_CONTEXT);
}

static void
initObjectHistory(TransObject *object, TransObjectType type)
{
	/* Initialize history */
	TransState *state;
	int			size;

	size = (type == TRANS_PACKAGE ? sizeof(PackState) : sizeof(VarState));
	dlist_init(&object->states);
	state = MemoryContextAllocZero(ModuleContext, size);
	dlist_push_head(&object->states, &(state->node));

	/* Initialize state */
	state->is_valid = true;
	if (type == TRANS_PACKAGE)
		((PackState *)state)->trans_var_num = 0;
	else
	{
		Variable *variable = (Variable *) object;
		if (!variable->is_record)
		{
			VarState * varState = (VarState *) state;
			ScalarVar  *scalar = &(varState->value.scalar);

			get_typlenbyval(variable->typid, &scalar->typlen, 
							&scalar->typbyval);
			varState->value.scalar.is_null = true;
		}
	}
}

static Package *
getPackage(text *name, bool strict)
{
	Package	   *package;
	char		key[NAMEDATALEN];
	bool		found;

	getKeyFromName(name, key);

	/* Find a package entry */
	if (packagesHash)
	{
		package = (Package *) hash_search(packagesHash, key, HASH_FIND, &found);

		if (found && GetActualState(package)->is_valid && 
			numOfTransVars(package) + numOfRegVars(package))
			return package;
	}
	/* Package not found or it's current state is "invalid" */
	if (strict)
		ereport(ERROR,
				(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
					errmsg("unrecognized package \"%s\"", key)));

	return NULL;
}

static Package *
createPackage(text *name, bool is_trans)
{
	Package	   *package;
	char		key[NAMEDATALEN];
	bool		found;

	getKeyFromName(name, key);
	ensurePackagesHashExists();

	/* Find or create a package entry */
	package = (Package *) hash_search(packagesHash, key, HASH_ENTER, &found);

	if (found)
	{
		if (!GetActualState(package)->is_valid)
		/* Make package valid again */
		{
			HASH_SEQ_STATUS vstat;
			Variable   *variable;
			TransObject *transObj = &package->transObject;

			/* Make new history entry of package */
			if (!isObjectChangedInCurrentTrans(transObj))
				createSavepoint(transObj, TRANS_PACKAGE);

			GetActualState(package)->is_valid = true;

			/* Mark all transactional variables in package as removed */
			if (package->varHashTransact)
			{
				hash_seq_init(&vstat, package->varHashTransact);
				while ((variable =
						(Variable *) hash_seq_search(&vstat)) != NULL)
				{
					transObj = &variable->transObject;

					if (!isObjectChangedInCurrentTrans(transObj))
					{
						createSavepoint(transObj, TRANS_VARIABLE);
						addToChangesStack(transObj, TRANS_VARIABLE);
					}
					GetActualState(variable)->is_valid = false;
				}
			}
		}
	}
	else
	{
		/* Package entry was created, so initialize it. */
		package->varHashRegular = NULL;
		package->varHashTransact = NULL;
		package->hctxRegular = NULL;
		package->hctxTransact = NULL;
		initObjectHistory(&package->transObject, TRANS_PACKAGE);
		/* Add to changes list */
		if (!isObjectChangedInCurrentTrans(&package->transObject))
			addToChangesStack(&package->transObject, TRANS_PACKAGE);
	}

	/* Create corresponding HTAB if not exists */
	if (!pack_htab(package, is_trans))
		makePackHTAB(package, is_trans);

	return package;
}

/*
 * Return a pointer to existing variable.
 * Function is useful to request a value of existing variable and
 * flag 'is_transactional' of this variable is unknown.
 */
static Variable *
getVariableInternal(Package *package, text *name, Oid typid, bool is_record,
					bool strict)
{
	Variable   *variable = NULL;
	char		key[NAMEDATALEN];
	bool		found = false;

	getKeyFromName(name, key);

	if (package->varHashRegular)
		variable = (Variable *) hash_search(package->varHashRegular,
											key, HASH_FIND, &found);
	if (!found && package->varHashTransact)
		variable = (Variable *) hash_search(package->varHashTransact,
											key, HASH_FIND, &found);

	/* Check variable type */
	if (found)
	{
		if (typid != InvalidOid)
		{
			if (variable->typid != typid)
			{
				char	   *var_type = DatumGetCString(
										DirectFunctionCall1(regtypeout,
											ObjectIdGetDatum(variable->typid)));

				ereport(ERROR,
						(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
						errmsg("variable \"%s\" requires \"%s\" value",
								key, var_type)));
			}

			if (variable->is_record != is_record)
				ereport(ERROR,
						(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
						errmsg("\"%s\" isn't a %s variable",
								key, is_record ? "record" : "scalar")));
		}
		if (!GetActualState(variable)->is_valid && strict)
			ereport(ERROR,
					(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
					 errmsg("unrecognized variable \"%s\"", key)));
	}
	else
	{
		if (strict)
			ereport(ERROR,
					(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
					 errmsg("unrecognized variable \"%s\"", key)));
	}

	return variable;
}

/*
 * Create a variable or return a pointer to existing one.
 * Function is useful to set new value to variable and flag 'is_transactional'
 * is known.
 */
static Variable *
createVariableInternal(Package *package, text *name, Oid typid, bool is_record,
					   bool is_transactional)
{
	Variable   *variable;
	TransObject *transObject;
	HTAB	   *htab;
	char		key[NAMEDATALEN];
	bool		found;

	getKeyFromName(name, key);

	/*
	 * Reverse check: for non-transactional variable search in regular table
	 * and vice versa.
	 */
	htab = pack_htab(package, !is_transactional);
	if (htab)
	{
		hash_search(htab, key, HASH_FIND, &found);
		if (found)
			ereport(ERROR,
					(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
					errmsg("variable \"%s\" already created as %sTRANSACTIONAL",
							key, is_transactional ? "NOT " : "")));
	}

	variable = (Variable *) hash_search(pack_htab(package, is_transactional),
										key, HASH_ENTER, &found);
	Assert(variable);
	transObject = &variable->transObject;

	/* Check variable type */
	if (found)
	{
		if (variable->typid != typid)
		{
			char	   *var_type = DatumGetCString(DirectFunctionCall1(regtypeout,
																	   ObjectIdGetDatum(variable->typid)));

			ereport(ERROR,
					(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
					 errmsg("variable \"%s\" requires \"%s\" value",
							key, var_type)));
		}

		if (variable->is_record != is_record)
			ereport(ERROR,
					(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
					 errmsg("\"%s\" isn't a %s variable",
					 		key, is_record ? "record" : "scalar")));

		/*
		 * Savepoint must be created when variable changed in current
		 * transaction. For each transaction level there should be a
		 * corresponding savepoint. New value should be stored in a last
		 * state.
		 */
		if (is_transactional &&
			!isObjectChangedInCurrentTrans(transObject))
		{
			createSavepoint(transObject, TRANS_VARIABLE);
		}
	}
	else
	{
		/* Variable entry was created, so initialize new variable. */
		variable->typid = typid;
		variable->package = package;
		variable->is_record = is_record;
		variable->is_transactional = is_transactional;
		initObjectHistory(transObject, TRANS_VARIABLE);

		if (!isObjectChangedInCurrentTrans(&package->transObject))
		{
			createSavepoint(&package->transObject, TRANS_PACKAGE);
			addToChangesStack(&package->transObject, TRANS_PACKAGE);
		}
	}

	if (is_transactional &&
		(!found || !GetActualState(variable)->is_valid))
		numOfTransVars(package)++;
	GetActualState(variable)->is_valid = true;
	
	Assert (numOfTransVars(package) == _numOfTransVars(package));
	
	/* If it is necessary, put variable to changedVars */
	if (is_transactional)
		addToChangesStack(transObject, TRANS_VARIABLE);

	return variable;
}

static void
copyValue(VarState *src, VarState *dest, Variable *destVar)
{
	MemoryContext oldcxt;

	oldcxt = MemoryContextSwitchTo(destVar->package->hctxTransact);

	if (destVar->is_record)
		/* copy record value */
	{
		HASH_SEQ_STATUS rstat;
		HashRecordEntry *item_src;
		RecordVar  *record_src = &src->value.record;
		RecordVar  *record_dest = &dest->value.record;

		init_record(record_dest, record_src->tupdesc, destVar);

		/* Copy previous history entry into the new one */
		hash_seq_init(&rstat, record_src->rhash);
		while ((item_src = (HashRecordEntry *) hash_seq_search(&rstat)) != NULL)
			copy_record(record_dest, item_src->tuple, destVar);
	}
	else
		/* copy scalar value */
	{
		ScalarVar  *scalar = &dest->value.scalar;

		*scalar = src->value.scalar;
		if (!scalar->is_null)
			scalar->value = datumCopy(src->value.scalar.value,
									  scalar->typbyval, scalar->typlen);
		else
			scalar->value = 0;
	}

	MemoryContextSwitchTo(oldcxt);
}

static void
freeValue(VarState *varstate, bool is_record)
{
	if (is_record && varstate->value.record.hctx)
	{
		/* All records will be freed */
		MemoryContextDelete(varstate->value.record.hctx);
	}
	else if (!is_record && varstate->value.scalar.typbyval == false &&
			 varstate->value.scalar.is_null == false &&
			 varstate->value.scalar.value)
		pfree(DatumGetPointer(varstate->value.scalar.value));
}

static void
removeState(TransObject *object, TransObjectType type, TransState *stateToDelete)
{
	if (type == TRANS_VARIABLE)
	{
		Variable   *var = (Variable *) object;

		freeValue((VarState *) stateToDelete, var->is_record);
	}
	dlist_delete(&stateToDelete->node);
	pfree(stateToDelete);
}

/* Remove package or variable (either transactional or regular) */
void
removeObject(TransObject *object, TransObjectType type)
{
	bool		found;
	HTAB	   *hash;
	Package	   *package = NULL;

	if (type == TRANS_PACKAGE)
	{
		package = (Package *) object;

		/* Regular variables had already removed */
		if (package->hctxRegular)
			MemoryContextDelete(package->hctxRegular);
		if (package->hctxTransact)
			MemoryContextDelete(package->hctxTransact);
		hash = packagesHash;
	}
	else
	{
		Variable *var = (Variable *) object;
		package = var->package;
		hash = var->is_transactional ?
			   var->package->varHashTransact :
			   var->package->varHashRegular;
	}

	/* Remove all object's states */
	while (!dlist_is_empty(&object->states))
		removeState(object, type, GetActualState(object));

	/* Remove object from hash table */
	hash_search(hash, object->name, HASH_REMOVE, &found);

	/* Remove package if it became empty */
	if (type == TRANS_VARIABLE && isPackageEmpty(package))
	{
		Assert(isObjectChangedInCurrentTrans(&package->transObject));
		GetActualState(&package->transObject)->is_valid = false;
	}

	resetVariablesCache(true);
}

/*
 * Create a new state of object
 */
static void
createSavepoint(TransObject *object, TransObjectType type)
{
	TransState *newState,
			   *prevState;

	prevState = GetActualState(object);
	if (type == TRANS_PACKAGE)
	{
		newState = (TransState *) MemoryContextAllocZero(ModuleContext,
														 sizeof(PackState));
		((PackState *)newState)->trans_var_num = ((PackState *)prevState)->trans_var_num;
	}
	else
	{
		Variable   *var = (Variable *) object;

		newState = (TransState *) MemoryContextAllocZero(var->package->hctxTransact,
														 sizeof(VarState));
		copyValue((VarState *) prevState, (VarState *) newState, var);
	}
	dlist_push_head(&object->states, &newState->node);
	newState->is_valid = prevState->is_valid;
}

static int
numOfRegVars(Package *package)
{
	if (package->varHashRegular)
		return hash_get_num_entries(package->varHashRegular);
	else
		return 0;
}

/*
 * Rollback object to its previous state
 */
static void
rollbackSavepoint(TransObject *object, TransObjectType type)
{
	TransState *state;

	state = GetActualState(object);
	removeState(object, type, state);

	if (dlist_is_empty(&object->states))
	{
		if (type == TRANS_PACKAGE && numOfRegVars((Package *)object))
		{
			initObjectHistory(object, type);
			GetActualState(object)->level = GetCurrentTransactionNestLevel() - 1;
			if (!dlist_is_empty(changesStack))
				addToChangesStackUpperLevel(object, type);
		}
		else
			removeObject(object, type);
	}
}

/*
 * Remove previous state of object
 */
static void
releaseSavepoint(TransObject *object, TransObjectType type)
{
	dlist_head *states = &object->states;
	Assert(GetActualState(object)->level == GetCurrentTransactionNestLevel());

	/*
	 * If the object is not valid and does not exist at a higher level
	 * (or if we complete the transaction) - remove object.
	 */
	if (!GetActualState(object)->is_valid &&
			(!dlist_has_next(states, dlist_head_node(states)) ||
			dlist_is_empty(changesStack))
		)
	{
		removeObject(object, type);
		return;
	}

 	/* If object has been changed in upper level -
	 * replace state of that level with the current one. */
	if (isObjectChangedInUpperTrans(object))
	{
		TransState *stateToDelete;
		dlist_node *nodeToDelete;
		nodeToDelete = dlist_next_node(states, dlist_head_node(states));
		stateToDelete = dlist_container(TransState, node, nodeToDelete);
		removeState(object, type, stateToDelete);
	}
	/* If the object does not yet have a record in previous level changesStack,
	 * create it. */
	else if (!dlist_is_empty(changesStack))
		addToChangesStackUpperLevel(object, type);

	/* Change subxact level due to release */
	GetActualState(object)->level--;
	if (type == TRANS_PACKAGE)
	{
		Package *package = (Package *)object;
		Assert (numOfTransVars(package) == _numOfTransVars(package));
	}
}

static void
addToChangesStackUpperLevel(TransObject *object, TransObjectType type)
{
	ChangedObject *co_new;
	ChangesStackNode *csn;
	/*
		* Impossible to push in upper list existing node
		* because it was created in another context
		*/
	csn = dlist_head_element(ChangesStackNode, node, changesStack);
	co_new = makeChangedObject(object, csn->ctx);
	dlist_push_head(type == TRANS_PACKAGE ? csn->changedPacksList :
											csn->changedVarsList,
											&co_new->node);
}

/*
 * Check if object was changed in current transaction level
 */
static bool
isObjectChangedInCurrentTrans(TransObject *object)
{
	TransState *state;

	if (!changesStack)
		return false;

	state = GetActualState(object);
	return state->level == GetCurrentTransactionNestLevel();
}

/*
 * Check if object was changed in parent transaction level
 */
static bool
isObjectChangedInUpperTrans(TransObject *object)
{
	TransState *cur_state,
			   *prev_state;

	cur_state = GetActualState(object);
	if (dlist_has_next(&object->states, &cur_state->node))
	{
		prev_state = dlist_container(TransState, node, cur_state->node.next);
		return prev_state->level == GetCurrentTransactionNestLevel() - 1;
	}

	return false;
}

/*
 * Create a new list of variables, changed in current transaction level
 */
static void
pushChangesStack(void)
{
	MemoryContext oldcxt;
	ChangesStackNode *csn;

	/*
	 * Initialize changesStack and create MemoryContext for it if not done
	 * before.
	 */
	if (!changesStackContext)
		changesStackContext = AllocSetContextCreate(ModuleContext,
													PGV_MCXT_STACK,
													ALLOCSET_START_SMALL_SIZES);
	Assert(changesStackContext);
	oldcxt = MemoryContextSwitchTo(changesStackContext);

	if (!changesStack)
	{
		changesStack = palloc0(sizeof(dlist_head));
		dlist_init(changesStack);
	}
	Assert(changesStack);
	csn = palloc0(sizeof(ChangesStackNode));
	csn->changedVarsList = palloc0(sizeof(dlist_head));
	csn->changedPacksList = palloc0(sizeof(dlist_head));

	csn->ctx = AllocSetContextCreate(changesStackContext,
									 PGV_MCXT_STACK_NODE,
									 ALLOCSET_START_SMALL_SIZES);

	dlist_init(csn->changedVarsList);
	dlist_init(csn->changedPacksList);
	dlist_push_head(changesStack, &csn->node);

	MemoryContextSwitchTo(oldcxt);
}

/*
 * Create a changesStack with the required depth.
 */
static void
prepareChangesStack(void)
{
	if (!changesStack)
	{
		int			level = GetCurrentTransactionNestLevel();

		while (level-- > 0)
		{
			pushChangesStack();
		}
	}
}

/*
 * Initialize an instance of ChangedObject datatype
 */
static inline ChangedObject *
makeChangedObject(TransObject *object, MemoryContext ctx)
{
	ChangedObject *co;

	co = MemoryContextAllocZero(ctx, sizeof(ChangedObject));
	co->object = object;

	return co;
}

/*
 * Add an object to the list of created, removed, or changed objects
 * in current transaction level
 */
static void
addToChangesStack(TransObject *object, TransObjectType type)
{
	prepareChangesStack();

	if (!isObjectChangedInCurrentTrans(object))
	{
		ChangesStackNode *csn;
		ChangedObject *co;

		csn = get_actual_changes_list();
		co = makeChangedObject(object, csn->ctx);
		dlist_push_head(type == TRANS_PACKAGE ? csn->changedPacksList :
						csn->changedVarsList, &co->node);

		/* Give this object current subxact level */
		GetActualState(object)->level = GetCurrentTransactionNestLevel();
	}
}

/*
 * Possible actions on variables.
 * Savepoints are created in setters so we don't need a CREATE_SAVEPOINT action.
 */
typedef enum Action
{
	RELEASE_SAVEPOINT,
	ROLLBACK_TO_SAVEPOINT
}			Action;

/*
 * Iterate variables and packages from list of changes and
 * apply corresponding action on them
 */
static void
processChanges(Action action)
{
	ChangesStackNode *bottom_list;
	int			i;

	Assert(changesStack && changesStackContext);
	/* List removed from stack but we still can use it */
	bottom_list = dlist_container(ChangesStackNode, node,
								  dlist_pop_head_node(changesStack));

	/*
	 * i:
	 * 1 - manage variables
	 * 0 - manage packages
	 */
	for (i = 1; i > -1; i--)
	{
		dlist_iter	iter;

		dlist_foreach(iter, i ? bottom_list->changedVarsList :
					  bottom_list->changedPacksList)
		{
			ChangedObject *co = dlist_container(ChangedObject, node, iter.cur);
			TransObject *object = co->object;

			switch (action)
			{
				case ROLLBACK_TO_SAVEPOINT:
					rollbackSavepoint(object, i ? TRANS_VARIABLE : TRANS_PACKAGE);
					break;
				case RELEASE_SAVEPOINT:

					/*
					 * If package was removed in current transaction level
					 * mark var as removed. We do not check pack_state->level,
					 * because var cannot get in list of changes until pack is
					 * removed.
					 */
					if (i)
					{
						Variable   *variable = (Variable *) object;
						Package	   *package = variable->package;

						if (!GetActualState(package)->is_valid)
							GetActualState(variable)->is_valid = false;
					}

					releaseSavepoint(object, i ? TRANS_VARIABLE : TRANS_PACKAGE);
					break;
			}
		}
	}

	/* Remove changes list of current level */
	MemoryContextDelete(bottom_list->ctx);
	/* Remove the stack if it is empty */
	if (dlist_is_empty(changesStack))
	{
		MemoryContextDelete(changesStackContext);
		changesStack = NULL;
		changesStackContext = NULL;
	}
	if (!hash_get_num_entries(packagesHash))
	{
		MemoryContextDelete(ModuleContext);
		packagesHash = NULL;
		ModuleContext = NULL;
		resetVariablesCache(true);
		changesStack = NULL;
		changesStackContext = NULL;
	}
}

/*
 * Intercept execution during subtransaction processing
 */
static void
pgvSubTransCallback(SubXactEvent event, SubTransactionId mySubid,
					SubTransactionId parentSubid, void *arg)
{
	if (changesStack)
	{
		switch (event)
		{
			case SUBXACT_EVENT_START_SUB:
				pushChangesStack();
				break;
			case SUBXACT_EVENT_COMMIT_SUB:
				processChanges(RELEASE_SAVEPOINT);
				break;
			case SUBXACT_EVENT_ABORT_SUB:
				processChanges(ROLLBACK_TO_SAVEPOINT);
				break;
			case SUBXACT_EVENT_PRE_COMMIT_SUB:
				break;
		}
	}
}

/*
 * Intercept execution during transaction processing
 */
static void
pgvTransCallback(XactEvent event, void *arg)
{
	if (changesStack)
	{
		switch (event)
		{
			case XACT_EVENT_PRE_COMMIT:
				processChanges(RELEASE_SAVEPOINT);
				break;
			case XACT_EVENT_ABORT:
				processChanges(ROLLBACK_TO_SAVEPOINT);
				break;
			case XACT_EVENT_PARALLEL_PRE_COMMIT:
				processChanges(RELEASE_SAVEPOINT);
				break;
			case XACT_EVENT_PARALLEL_ABORT:
				processChanges(ROLLBACK_TO_SAVEPOINT);
				break;
			default:
				break;
		}
	}

	if (event == XACT_EVENT_PARALLEL_ABORT || event == XACT_EVENT_ABORT)
		if (LastHSeqStatus)
			LastHSeqStatus = NULL;
}

/*
 * ExecutorEnd hook: clean up hash table sequential scan status
 */
static void
variable_ExecutorEnd(QueryDesc *queryDesc)
{
	if (LastHSeqStatus)
	{
		hash_seq_term(LastHSeqStatus);
		LastHSeqStatus = NULL;
	}
	if (prev_ExecutorEnd)
		prev_ExecutorEnd(queryDesc);
	else
		standard_ExecutorEnd(queryDesc);
}

/*
 * Register callback function when module starts
 */
void
_PG_init(void)
{
	RegisterXactCallback(pgvTransCallback, NULL);
	RegisterSubXactCallback(pgvSubTransCallback, NULL);

	/* Install hooks. */
	prev_ExecutorEnd = ExecutorEnd_hook;
	ExecutorEnd_hook = variable_ExecutorEnd;
}

/*
 * Unregister callback function when module unloads
 */
void
_PG_fini(void)
{
	UnregisterXactCallback(pgvTransCallback, NULL);
	UnregisterSubXactCallback(pgvSubTransCallback, NULL);
	ExecutorEnd_hook = prev_ExecutorEnd;
}

/* Get exact count of valid variables in package. For debug only. */
static int
_numOfTransVars(Package *package)
{
	HASH_SEQ_STATUS vstat;
	Variable	   *variable;
	unsigned long res = 0;

	if (package->varHashTransact)
	{
		hash_seq_init(&vstat, package->varHashTransact);
		while ((variable = (Variable *) hash_seq_search(&vstat)) != NULL)
		{
			if (GetActualState(variable)->is_valid &&
				GetActualState(package)->is_valid)
				res++;
		}
	}

	return res;
}
