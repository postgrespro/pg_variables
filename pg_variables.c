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

static HashPackageEntry *getPackageByName(text *name, bool create, bool strict);
static HashVariableEntry *getVariableInternal(HTAB *variables, text *name,
											  Oid typid, bool strict);
static HashVariableEntry *createVariableInternal(HashPackageEntry *package,
												 text *name, Oid typid,
												 bool is_transactional);

static void releaseSavepoint(HashVariableEntry *variable);
static void rollbackSavepoint(HashPackageEntry *package, HashVariableEntry *variable);
static void createSavepoint(HashPackageEntry *package, HashVariableEntry *variable);

static void mergeChangedVarsStack(void);
static void pushChangedVarsStack(void);
static void popChangedVarsStack(void);
static void addToChangedVars(HashPackageEntry *package, HashVariableEntry *variable);

static bool isVarChangedInCurrentTrans(HashVariableEntry *variable);
static bool isVarChangedInUpperTrans(HashVariableEntry *variable);

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
static HashPackageEntry *LastPackage = NULL;
/* Recent variable */
static HashVariableEntry *LastVariable = NULL;


/* This stack contains lists of changed variables per each subxact level */
static dlist_head *changedVarsStack = NULL;
static MemoryContext changedVarsContext = NULL;

/* Returns a list of of vars changed at current subxact level */
#define get_actual_changed_vars_list() \
	( \
		AssertMacro(changedVarsStack != NULL), \
		(dlist_head_element(ChangedVarsStackNode, \
							node, changedVarsStack))->changedVarsList \
	)


#define PGV_MCXT_MAIN		"pg_variables: main memory context"
#define PGV_MCXT_VARS		"pg_variables: variables hash"
#define PGV_MCXT_STACK		"pg_variables: changedVarsStack"
#define PGV_MCXT_STACK_NODE	"pg_variables: changedVarsStackNode"


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
	HashPackageEntry *package;
	HashVariableEntry *variable;
	ScalarVar  *scalar;
	MemoryContext oldcxt;

	package = getPackageByName(package_name, true, false);
	variable = createVariableInternal(package, var_name, typid,
									  is_transactional);

	scalar = get_actual_value_scalar(variable);

	/* Release memory for variable */
	if (scalar->typbyval == false && scalar->is_null == false)
		pfree(DatumGetPointer(scalar->value));

	scalar->is_null = is_null;
	if (!scalar->is_null)
	{
		oldcxt = MemoryContextSwitchTo(package->hctx);
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
	HashPackageEntry *package;
	HashVariableEntry *variable;
	ScalarVar  *scalar;

	package = getPackageByName(package_name, false, strict);
	if (package == NULL)
	{
		*is_null = true;
		return 0;
	}

	variable = getVariableInternal(package->variablesHash,
								   var_name, typid, strict);

	if (variable == NULL)
	{
		*is_null = true;
		return 0;
	}
	scalar = get_actual_value_scalar(variable);
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


Datum
variable_insert(PG_FUNCTION_ARGS)
{
	text	   *package_name;
	text	   *var_name;
	HeapTupleHeader rec;
	HashPackageEntry *package;
	HashVariableEntry *variable;
	bool		is_transactional;

	Oid			tupType;
	int32		tupTypmod;
	TupleDesc	tupdesc;

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
		VARSIZE_ANY_EXHDR(package_name) != strlen(LastPackage->name) ||
		strncmp(VARDATA_ANY(package_name), LastPackage->name,
				VARSIZE_ANY_EXHDR(package_name)) != 0)
	{
		package = getPackageByName(package_name, true, false);
		LastPackage = package;
		LastVariable = NULL;
	}
	else
		package = LastPackage;

	/* Get cached variable */
	if (LastVariable == NULL ||
		VARSIZE_ANY_EXHDR(var_name) != strlen(LastVariable->name) ||
		strncmp(VARDATA_ANY(var_name), LastVariable->name,
				VARSIZE_ANY_EXHDR(var_name)) != 0)
	{
		variable = createVariableInternal(package, var_name, RECORDOID,
										  is_transactional);
		LastVariable = variable;
	}
	else
	{
		if (LastVariable->is_transactional == is_transactional)
			variable = LastVariable;
		else
		{
			char		key[NAMEDATALEN];

			getKeyFromName(var_name, key);
			ereport(ERROR,
					(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
					 errmsg("variable \"%s\" already created as %sTRANSACTIONAL",
							key, LastVariable->is_transactional ? "" : "NOT ")));
		}
		if (!isVarChangedInCurrentTrans(variable) && variable->is_transactional)
		{
			createSavepoint(package, variable);
			addToChangedVars(package, variable);
		}
	}

	/* Insert a record */
	tupType = HeapTupleHeaderGetTypeId(rec);
	tupTypmod = HeapTupleHeaderGetTypMod(rec);
	tupdesc = lookup_rowtype_tupdesc(tupType, tupTypmod);

	if (!(get_actual_value_record(variable))->tupdesc)
	{
		/*
		 * This is the first record for the var_name. Initialize attributes.
		 */
		init_attributes(variable, tupdesc, package->hctx);
	}
	else
		check_attributes(variable, tupdesc);

	insert_record(variable, rec);

	/* Release resources */
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
	HashPackageEntry *package;
	HashVariableEntry *variable;
	bool		res;

	Oid			tupType;
	int32		tupTypmod;
	TupleDesc	tupdesc;

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
		VARSIZE_ANY_EXHDR(package_name) != strlen(LastPackage->name) ||
		strncmp(VARDATA_ANY(package_name), LastPackage->name,
				VARSIZE_ANY_EXHDR(package_name)) != 0)
	{
		package = getPackageByName(package_name, false, true);
		LastPackage = package;
		LastVariable = NULL;
	}
	else
		package = LastPackage;

	/* Get cached variable */
	if (LastVariable == NULL ||
		VARSIZE_ANY_EXHDR(var_name) != strlen(LastVariable->name) ||
		strncmp(VARDATA_ANY(var_name), LastVariable->name,
				VARSIZE_ANY_EXHDR(var_name)) != 0)
	{
		variable = getVariableInternal(package->variablesHash,
									   var_name, RECORDOID, true);
		LastVariable = variable;
	}
	else
		variable = LastVariable;

	if (variable->is_transactional && !isVarChangedInCurrentTrans(variable))
	{
		createSavepoint(package, variable);
		addToChangedVars(package, variable);
	}

	/* Update a record */
	tupType = HeapTupleHeaderGetTypeId(rec);
	tupTypmod = HeapTupleHeaderGetTypMod(rec);
	tupdesc = lookup_rowtype_tupdesc(tupType, tupTypmod);

	check_attributes(variable, tupdesc);
	res = update_record(variable, rec);

	/* Release resources */
	ReleaseTupleDesc(tupdesc);

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
	HashPackageEntry *package;
	HashVariableEntry *variable;
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
		VARSIZE_ANY_EXHDR(package_name) != strlen(LastPackage->name) ||
		strncmp(VARDATA_ANY(package_name), LastPackage->name,
				VARSIZE_ANY_EXHDR(package_name)) != 0)
	{
		package = getPackageByName(package_name, false, true);
		LastPackage = package;
		LastVariable = NULL;
	}
	else
		package = LastPackage;

	/* Get cached variable */
	if (LastVariable == NULL ||
		VARSIZE_ANY_EXHDR(var_name) != strlen(LastVariable->name) ||
		strncmp(VARDATA_ANY(var_name), LastVariable->name,
				VARSIZE_ANY_EXHDR(var_name)) != 0)
	{
		variable = getVariableInternal(package->variablesHash,
									   var_name, RECORDOID, true);
		LastVariable = variable;
	}
	else
		variable = LastVariable;

	if (variable->is_transactional && !isVarChangedInCurrentTrans(variable))
	{
		createSavepoint(package, variable);
		addToChangedVars(package, variable);
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
		HashPackageEntry *package;
		HashVariableEntry *variable;
		MemoryContext oldcontext;
		RecordVar  *record;

		CHECK_ARGS_FOR_NULL();

		/* Get arguments */
		package_name = PG_GETARG_TEXT_PP(0);
		var_name = PG_GETARG_TEXT_PP(1);

		package = getPackageByName(package_name, false, true);
		variable = getVariableInternal(package->variablesHash,
									   var_name, RECORDOID, true);

		record = get_actual_value_record(variable);

		funcctx = SRF_FIRSTCALL_INIT();
		oldcontext = MemoryContextSwitchTo(funcctx->multi_call_memory_ctx);

		funcctx->tuple_desc = CreateTupleDescCopy(record->tupdesc);

		rstat = (HASH_SEQ_STATUS *) palloc0(sizeof(HASH_SEQ_STATUS));
		hash_seq_init(rstat, record->rhash);
		funcctx->user_fctx = rstat;

		MemoryContextSwitchTo(oldcontext);
		PG_FREE_IF_COPY(package_name, 0);
		PG_FREE_IF_COPY(var_name, 1);
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
	HashPackageEntry *package;
	HashVariableEntry *variable;

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

	package = getPackageByName(package_name, false, true);
	variable = getVariableInternal(package->variablesHash,
								   var_name, RECORDOID, true);

	if (!value_is_null)
		check_record_key(variable, value_type);

	record = get_actual_value_record(variable);

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
	HashVariableEntry *variable;
	ArrayIterator iterator;
} VariableIteratorRec;

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
		HashPackageEntry *package;
		HashVariableEntry *variable;
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

		package = getPackageByName(package_name, false, true);
		variable = getVariableInternal(package->variablesHash,
									   var_name, RECORDOID, true);

		check_record_key(variable, ARR_ELEMTYPE(values));

		funcctx = SRF_FIRSTCALL_INIT();
		oldcontext = MemoryContextSwitchTo(funcctx->multi_call_memory_ctx);

		funcctx->tuple_desc = CreateTupleDescCopy(
									get_actual_value_record(variable)->tupdesc);

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

		record = get_actual_value_record(var->variable);
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
 * Remove one entry from history of states of arg 'variable'
 */
static void
cleanVariableCurrentState(HashVariableEntry *variable)
{
	ValueHistory *history;
	ValueHistoryEntry *historyEntryToDelete;

	if (variable->typid == RECORDOID)
		clean_records(variable);
	else
	{
		ScalarVar  *scalar = get_actual_value_scalar(variable);

		if (scalar->typbyval == false && scalar->is_null == false)
			pfree(DatumGetPointer(scalar->value));
	}

	history = &variable->data;
	historyEntryToDelete = get_history_entry(dlist_pop_head_node(history));
	pfree(historyEntryToDelete);
}

/*
 * Remove all entries from history of states of arg 'variable'.
 * DOES NOT remove 'variable' itself.
 */
static void
cleanVariableAllStates(HashVariableEntry *variable)
{
	while(!dlist_is_empty(&variable->data))
	{
		cleanVariableCurrentState(variable);
	}
}

/*
 * Check if variable exists.
 */
Datum
variable_exists(PG_FUNCTION_ARGS)
{
	text	   *package_name;
	text	   *var_name;
	HashPackageEntry *package;
	char		key[NAMEDATALEN];
	bool		found;

	CHECK_ARGS_FOR_NULL();

	package_name = PG_GETARG_TEXT_PP(0);
	var_name = PG_GETARG_TEXT_PP(1);

	package = getPackageByName(package_name, false, false);
	if (package == NULL)
	{
		PG_FREE_IF_COPY(package_name, 0);
		PG_FREE_IF_COPY(var_name, 1);

		PG_RETURN_BOOL(false);
	}

	getKeyFromName(var_name, key);

	hash_search(package->variablesHash, key, HASH_FIND, &found);

	PG_FREE_IF_COPY(package_name, 0);
	PG_FREE_IF_COPY(var_name, 1);

	PG_RETURN_BOOL(found);
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

	res = getPackageByName(package_name, false, false) != NULL;

	PG_FREE_IF_COPY(package_name, 0);
	PG_RETURN_BOOL(res);
}

/*
 * Remove variable from package by name.
 */
Datum
remove_variable(PG_FUNCTION_ARGS)
{
	text	   *package_name;
	text	   *var_name;
	HashPackageEntry *package;
	HashVariableEntry *variable;
	bool		found;
	char		key[NAMEDATALEN];

	CHECK_ARGS_FOR_NULL();

	package_name = PG_GETARG_TEXT_PP(0);
	var_name = PG_GETARG_TEXT_PP(1);

	package = getPackageByName(package_name, false, true);
	getKeyFromName(var_name, key);

	variable = (HashVariableEntry *) hash_search(package->variablesHash,
												 key, HASH_REMOVE, &found);
	if (!found)
		ereport(ERROR,
				(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
				 errmsg("unrecognized variable \"%s\"", key)));

	/* Remove variable from cache */
	LastVariable = NULL;

	cleanVariableAllStates(variable);

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
	text	   *package_name;
	HashPackageEntry *package;
	bool		found;
	char		key[NAMEDATALEN];

	if (PG_ARGISNULL(0))
		ereport(ERROR,
				(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
				 errmsg("package name can not be NULL")));

	package_name = PG_GETARG_TEXT_PP(0);
	getKeyFromName(package_name, key);

	if (!packagesHash)
		ereport(ERROR,
				(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
				 errmsg("unrecognized package \"%s\"", key)));

	package = (HashPackageEntry *) hash_search(packagesHash, key,
											   HASH_REMOVE, &found);
	if (!found)
		ereport(ERROR,
				(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
				 errmsg("unrecognized package \"%s\"", key)));

	/* Remove package and variable from cache */
	LastPackage = NULL;
	LastVariable = NULL;

	/* All variables will be freed */
	MemoryContextDelete(package->hctx);

	PG_FREE_IF_COPY(package_name, 0);

	PG_RETURN_VOID();
}

/*
 * Remove all packages and variables.
 */
Datum
remove_packages(PG_FUNCTION_ARGS)
{
	/* There is not any packages and variables */
	if (packagesHash == NULL)
		PG_RETURN_VOID();

	/* Remove package and variable from cache */
	LastPackage = NULL;
	LastVariable = NULL;

	/* All packages and variables will be freed */
	MemoryContextDelete(ModuleContext);

	packagesHash = NULL;
	ModuleContext = NULL;
	changedVarsStack = NULL;

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
} VariableRec;

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
			HashPackageEntry *package;
			HASH_SEQ_STATUS pstat;
			int			mRecs = NUMVARIABLES,
						nRecs = 0;

			recs = (VariableRec *) palloc0(sizeof(VariableRec) * mRecs);

			/* Get packages list */
			hash_seq_init(&pstat, packagesHash);
			while ((package =
				(HashPackageEntry *) hash_seq_search(&pstat)) != NULL)
			{
				HashVariableEntry *variable;
				HASH_SEQ_STATUS vstat;

				/* Get variables list for package */
				hash_seq_init(&vstat, package->variablesHash);
				while ((variable =
					(HashVariableEntry *) hash_seq_search(&vstat)) != NULL)
				{
					/* Resize recs if necessary */
					if (nRecs >= mRecs)
					{
						mRecs *= 2;
						recs = (VariableRec *) repalloc(recs,
												sizeof(VariableRec) * mRecs);
					}

					recs[nRecs].package = package->name;
					recs[nRecs].variable = variable->name;
					recs[nRecs].is_transactional = variable->is_transactional;
					nRecs++;
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
	HashPackageEntry *package;

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

	package = (HashPackageEntry *) hash_seq_search(pstat);
	if (package != NULL)
	{
		Datum		values[2];
		bool		nulls[2];
		HeapTuple	tuple;
		Datum		result;
		Size		totalspace = 0;

		memset(nulls, 0, sizeof(nulls));

		/* Fill data */
		values[0] = PointerGetDatum(cstring_to_text(package->name));

		getMemoryTotalSpace(package->hctx, 0, &totalspace);
		values[1] = Int64GetDatum(totalspace);

		/* Data are ready */
		tuple = heap_form_tuple(funcctx->tuple_desc, values, nulls);
		result = HeapTupleGetDatum(tuple);

		SRF_RETURN_NEXT(funcctx, result);
	}
	else
	{
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
	HASHCTL ctl;

	if (packagesHash)
		return;

	ModuleContext = AllocSetContextCreate(CacheMemoryContext,
										  PGV_MCXT_MAIN,
										  ALLOCSET_DEFAULT_SIZES);

	ctl.keysize = NAMEDATALEN;
	ctl.entrysize = sizeof(HashPackageEntry);
	ctl.hcxt = ModuleContext;

	packagesHash = hash_create("Packages hash",
							   NUMPACKAGES, &ctl,
							   HASH_ELEM | HASH_CONTEXT);
}

static HashPackageEntry *
getPackageByName(text* name, bool create, bool strict)
{
	HashPackageEntry *package;
	char		key[NAMEDATALEN];
	bool		found;

	getKeyFromName(name, key);

	if (create)
		ensurePackagesHashExists();
	else
	{
		if (!packagesHash)
		{
			if (strict)
				ereport(ERROR,
						(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
						 errmsg("unrecognized package \"%s\"", key)));

			return NULL;
		}
	}

	/* Find or create a package entry */
	package = (HashPackageEntry *) hash_search(packagesHash, key,
											   (create ? HASH_ENTER : HASH_FIND),
											   &found);

	/* Package entry was created, so we need create hash table for variables. */
	if (!found)
	{
		if (create)
		{
			HASHCTL ctl;
			char	hash_name[BUFSIZ];

			package->hctx = AllocSetContextCreate(ModuleContext,
												  PGV_MCXT_VARS,
												  ALLOCSET_DEFAULT_SIZES);

			sprintf(hash_name, "Variables hash for package \"%s\"", key);

			ctl.keysize = NAMEDATALEN;
			ctl.entrysize = sizeof(HashVariableEntry);
			ctl.hcxt = package->hctx;
			package->variablesHash = hash_create(hash_name,
												 NUMVARIABLES, &ctl,
												 HASH_ELEM | HASH_CONTEXT);
		}
		else if (strict)
			ereport(ERROR,
					(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
					 errmsg("unrecognized package \"%s\"", key)));
	}

	return package;
}

/*
 * Return a pointer to existing variable.
 * Function is useful to request a value of existing variable and
 * flag 'is_transactional' of this variable is unknown.
 */
static HashVariableEntry *
getVariableInternal(HTAB *variables, text *name, Oid typid, bool strict)
{
	HashVariableEntry *variable;
	char		key[NAMEDATALEN];
	bool		found;

	getKeyFromName(name, key);

	variable = (HashVariableEntry *) hash_search(variables,
												 key, HASH_FIND, &found);

	/* Check variable type */
	if (found)
	{
		if (variable->typid != typid)
		{
			char	   *var_type = DatumGetCString(DirectFunctionCall1(
								regtypeout, ObjectIdGetDatum(variable->typid)));

			ereport(ERROR,
					(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
					 errmsg("variable \"%s\" requires \"%s\" value",
							key, var_type)));
		}
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
 * Function is useful to set new value to variable and
 * flag 'is_transactional' is known.
 */
static HashVariableEntry *
createVariableInternal(HashPackageEntry *package, text *name, Oid typid,
						bool is_transactional)
{
	HashVariableEntry *variable;
	char		key[NAMEDATALEN];
	bool		found;

	getKeyFromName(name, key);

	variable = (HashVariableEntry *) hash_search(package->variablesHash,
												 key, HASH_ENTER, &found);

	/* Check variable type */
	if (found)
	{
		if (variable->typid != typid)
		{
			char	   *var_type = DatumGetCString(DirectFunctionCall1(
								regtypeout, ObjectIdGetDatum(variable->typid)));

			ereport(ERROR,
					(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
					 errmsg("variable \"%s\" requires \"%s\" value",
							key, var_type)));
		}

		if (variable->is_transactional != is_transactional)
		{
			ereport(ERROR,
					(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
					 errmsg("variable \"%s\" already created as %sTRANSACTIONAL",
							key, variable->is_transactional ? "" : "NOT ")));
		}

		/*
		 * Savepoint must be created when variable changed in current
		 * transaction.
		 * For each transaction level there should be a corresponding savepoint.
		 * New value should be stored in a last state.
		 */
		if (variable->is_transactional && !isVarChangedInCurrentTrans(variable))
		{
			createSavepoint(package, variable);
		}
	}
	else
	{
		/* Variable entry was created, so initialize new variable. */
		if (variable)
		{
			ValueHistoryEntry *historyEntry;

			variable->typid = typid;
			variable->is_transactional = is_transactional;
			dlist_init(&(variable->data));
			historyEntry = MemoryContextAllocZero(package->hctx,
												  sizeof(ValueHistoryEntry));

			dlist_push_head(&variable->data, &historyEntry->node);
			if (typid != RECORDOID)
			{
				ScalarVar  *scalar = get_actual_value_scalar(variable);

				get_typlenbyval(variable->typid, &scalar->typlen,
								&scalar->typbyval);
				scalar->is_null = true;
			}
		}
	}
	/* If it is necessary, put variable to changedVars */
	if (is_transactional)
		addToChangedVars(package, variable);

	return variable;
}

/*
 * Create a new history point of variable and copy value from
 * previous state
 */
static void
createSavepoint(HashPackageEntry *package, HashVariableEntry *variable)
{

	if (variable->typid == RECORDOID)
	{
		insert_savepoint(variable, package->hctx);
	}
	else
	{
		ScalarVar  *scalar;
		ValueHistory *history;
		ValueHistoryEntry *history_entry_new,
				   *history_entry_prev;
		MemoryContext oldcxt;

		oldcxt = MemoryContextSwitchTo(package->hctx);
		history = &variable->data;

		/* Release memory for variable */
		history_entry_new = palloc0(sizeof(ValueHistoryEntry));
		history_entry_prev = dlist_head_element(ValueHistoryEntry, node, history);
		scalar = &history_entry_new->value.scalar;
		*scalar = history_entry_prev->value.scalar;

		if (!scalar->is_null)
		{
			scalar->value = datumCopy(history_entry_prev->value.scalar.value,
									  scalar->typbyval, scalar->typlen);
		}
		else
			scalar->value = 0;

		dlist_push_head(history, &history_entry_new->node);
		MemoryContextSwitchTo(oldcxt);
	}
}

/*
 * Remove previous state of variable
 */
static void
releaseSavepoint(HashVariableEntry *variable)
{
	ValueHistory *history;

	history = &variable->data;
	if (dlist_has_next(history, dlist_head_node(history)))
	{
		ValueHistoryEntry *historyEntryToDelete;
		dlist_node *nodeToDelete;

		nodeToDelete = dlist_next_node(history, dlist_head_node(history));
		historyEntryToDelete = get_history_entry(nodeToDelete);

		if (variable->typid == RECORDOID)
		{
			/* All records will be freed */
			MemoryContextDelete(historyEntryToDelete->value.record.hctx);
		}
		else if (historyEntryToDelete->value.scalar.typbyval == false &&
				 historyEntryToDelete->value.scalar.is_null == false)
		{
			pfree(DatumGetPointer(historyEntryToDelete->value.scalar.value));
		}

		dlist_delete(nodeToDelete);
		pfree(historyEntryToDelete);
	}

	/* Change subxact level due to release */
	get_actual_value(variable)->level--;
}

/*
 * Rollback variable to previous state and remove current value
 */
static void
rollbackSavepoint(HashPackageEntry *package, HashVariableEntry *variable)
{
	cleanVariableCurrentState(variable);

	/* Remove variable if it was created in rolled back transaction */
	if (dlist_is_empty(&variable->data))
	{
		bool		found;

		hash_search(package->variablesHash, variable->name, HASH_REMOVE, &found);
	}
}

/*
 * Initialize an instance of ChangedVarsNode datatype
 */
static inline ChangedVarsNode *
makeChangedVarsNode(MemoryContext ctx, HashPackageEntry *package, HashVariableEntry *variable)
{
	ChangedVarsNode *cvn;

	cvn = MemoryContextAllocZero(ctx, sizeof(ChangedVarsNode));
	cvn->package = package;
	cvn->variable = variable;
	return cvn;
}

/*
 * Check if variable was changed in current transaction level
 */
static bool
isVarChangedInCurrentTrans(HashVariableEntry *variable)
{
	ValueHistoryEntry   *var_state;

	if (!changedVarsStack)
		return false;

	var_state = get_actual_value(variable);
	return var_state->level == GetCurrentTransactionNestLevel();
}

/*
 * Check if variable was changed in parent transaction level
 */
static bool
isVarChangedInUpperTrans(HashVariableEntry *variable)
{
	ValueHistoryEntry   *var_state,
						*var_prev_state;

	var_state = get_actual_value(variable);

	if (dlist_has_next(&variable->data, &var_state->node))
	{
		var_prev_state = get_history_entry(var_state->node.next);
		return var_prev_state->level == GetCurrentTransactionNestLevel() - 1;
	}

	return false;
}

/*
 * Create a new list of variables, changed in current transaction level
 */
static void
pushChangedVarsStack(void)
{
	MemoryContext oldcxt;
	ChangedVarsStackNode *cvsn;

	/*
	 * Initialize changedVarsStack and create MemoryContext for it
	 * if not done before.
	 */
	if (!changedVarsContext)
		changedVarsContext = AllocSetContextCreate(ModuleContext,
												   PGV_MCXT_STACK,
												   ALLOCSET_START_SMALL_SIZES);

	oldcxt = MemoryContextSwitchTo(changedVarsContext);

	if (!changedVarsStack)
	{
		changedVarsStack = palloc0(sizeof(dlist_head));
		dlist_init(changedVarsStack);
	}

	cvsn = palloc0(sizeof(ChangedVarsStackNode));
	cvsn->changedVarsList = palloc0(sizeof(dlist_head));

	cvsn->ctx = AllocSetContextCreate(changedVarsContext,
									  PGV_MCXT_STACK_NODE,
									  ALLOCSET_START_SMALL_SIZES);

	dlist_init(cvsn->changedVarsList);
	dlist_push_head(changedVarsStack, &cvsn->node);

	MemoryContextSwitchTo(oldcxt);
}

/*
 * Remove current list of variables, changed in current transaction level
 */
static void
popChangedVarsStack(void)
{
	if (changedVarsStack)
	{
		ChangedVarsStackNode *cvsn;

		Assert(!dlist_is_empty(changedVarsStack));
		cvsn = dlist_container(ChangedVarsStackNode, node,
							   dlist_pop_head_node(changedVarsStack));
		MemoryContextDelete(cvsn->ctx);
		if (dlist_is_empty(changedVarsStack))
		{
			MemoryContextDelete(changedVarsContext);
			changedVarsStack = NULL;
			changedVarsContext = NULL;
		}
	}
}

/*
 * Pop current list of variables and add missing changed vars to upper list
 */
static void
mergeChangedVarsStack(void)
{
	if (changedVarsStack)
	{
		dlist_iter	iter;
		ChangedVarsStackNode *bottom_list;

		/* List removed from stack but we still can use it */
		bottom_list = dlist_container(ChangedVarsStackNode, node,
									  dlist_pop_head_node(changedVarsStack));

		/* There must be at least one parent level */
		Assert(!dlist_is_empty(changedVarsStack));

		dlist_foreach(iter, bottom_list->changedVarsList)
		{
			ChangedVarsNode *cvn_old = dlist_container(ChangedVarsNode, node, iter.cur);

			/* Did this variable change at parent level? */
			if (isVarChangedInUpperTrans(cvn_old->variable))
			{
				/* We just have to drop this state */
				releaseSavepoint(cvn_old->variable);
			}
			else
			{
				ChangedVarsNode *cvn_new;
				ChangedVarsStackNode *cvsn;

				/*
				 * Impossible to push in upper list existing node because
				 * it was created in another context
				 */
				cvsn = dlist_head_element(ChangedVarsStackNode, node, changedVarsStack);
				cvn_new = makeChangedVarsNode(cvsn->ctx, cvn_old->package, cvn_old->variable);
				dlist_push_head(cvsn->changedVarsList, &cvn_new->node);

				/* Change subxact level due to release */
				get_actual_value(cvn_new->variable)->level--;
			}
		}
		MemoryContextDelete(bottom_list->ctx);
	}
}

/*
 * Add a variable to list of changed vars in current transaction level
 */
static void
addToChangedVars(HashPackageEntry *package, HashVariableEntry *variable)
{
	ChangedVarsStackNode *cvsn;

	if (!changedVarsStack)
	{
		int level = GetCurrentTransactionNestLevel();

		while (level-- > 0)
		{
			pushChangedVarsStack();
		}
	}

	Assert(changedVarsStack && changedVarsContext);

	if (!isVarChangedInCurrentTrans(variable))
	{
		ChangedVarsNode *cvn;

		cvsn = dlist_head_element(ChangedVarsStackNode, node, changedVarsStack);
		cvn = makeChangedVarsNode(cvsn->ctx, package, variable);
		dlist_push_head(cvsn->changedVarsList, &cvn->node);

		/* Give this variable current subxact level */
		get_actual_value(cvn->variable)->level = GetCurrentTransactionNestLevel();
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
} Action;

/*
 * Iterate variables from list of changes and
 * apply corresponding action on them
 */
static void
applyActionOnChangedVars(Action action)
{
	dlist_head *changedVars = get_actual_changed_vars_list();
	dlist_mutable_iter miter;

	dlist_foreach_modify(miter, changedVars)
	{
		ChangedVarsNode *cvn = dlist_container(ChangedVarsNode, node, miter.cur);

		switch(action)
		{
			case RELEASE_SAVEPOINT:
				releaseSavepoint(cvn->variable);
				break;
			case ROLLBACK_TO_SAVEPOINT:
				rollbackSavepoint(cvn->package, cvn->variable);
				break;
		}
	}
}

/*
 * Intercept execution during subtransaction processing
 */
static void
pgvSubTransCallback(SubXactEvent event, SubTransactionId mySubid,
					SubTransactionId parentSubid, void *arg)
{
	if (changedVarsStack)
	{
		switch (event)
		{
			case SUBXACT_EVENT_START_SUB:
				pushChangedVarsStack();
				break;
			case SUBXACT_EVENT_COMMIT_SUB:
				mergeChangedVarsStack();
				break;
			case SUBXACT_EVENT_ABORT_SUB:
				applyActionOnChangedVars(ROLLBACK_TO_SAVEPOINT);
				popChangedVarsStack();
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
	if (changedVarsStack)
	{
		switch (event)
		{
			case XACT_EVENT_PRE_COMMIT:
				applyActionOnChangedVars(RELEASE_SAVEPOINT);
				popChangedVarsStack();
				break;
			case XACT_EVENT_ABORT:
				applyActionOnChangedVars(ROLLBACK_TO_SAVEPOINT);
				popChangedVarsStack();
				break;
			case XACT_EVENT_PARALLEL_PRE_COMMIT:
				applyActionOnChangedVars(RELEASE_SAVEPOINT);
				popChangedVarsStack();
				break;
			case XACT_EVENT_PARALLEL_ABORT:
				applyActionOnChangedVars(ROLLBACK_TO_SAVEPOINT);
				popChangedVarsStack();
				break;
			default:
				break;
		}
	}
}

/*
 * Register callback function when module starts
 */
void
_PG_init(void)
{
	RegisterXactCallback(pgvTransCallback, NULL);
	RegisterSubXactCallback(pgvSubTransCallback, NULL);
}

/*
 * Unregister callback function when module unloads
 */
void
_PG_fini(void)
{
	UnregisterXactCallback(pgvTransCallback, NULL);
	UnregisterSubXactCallback(pgvSubTransCallback, NULL);
}
