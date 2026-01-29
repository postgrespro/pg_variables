/*-------------------------------------------------------------------------
 *
 * pg_variables_record.c
 *	  Functions to work with record types
 *
 * Copyright (c) 2015-2022, Postgres Professional
 *
 *-------------------------------------------------------------------------
 */
#include "postgres.h"
#include "funcapi.h"

#include "access/htup_details.h"
/*
 * See https://git.postgresql.org/gitweb/?p=postgresql.git;a=commit;h=8b94dab06617ef80a0901ab103ebd8754427ef
 *
 * Split tuptoaster.c into three separate files.
 */
#if PG_VERSION_NUM >= 130000
#include "access/detoast.h"
#include "access/heaptoast.h"
#else
#include "access/tuptoaster.h"
#endif

#include "catalog/pg_collation.h"
#include "catalog/pg_type.h"
#include "parser/parse_type.h"
#include "utils/builtins.h"
#include "utils/datum.h"
#include "utils/lsyscache.h"
#include "utils/syscache.h"
#include "utils/memutils.h"
#include "utils/typcache.h"

#include "pg_variables.h"

/*
 * Hash function for records.
 *
 * We use the element type's default hash opclass, and the default collation
 * if the type is collation-sensitive.
 */
static uint32
record_hash(const void *key, Size keysize)
{
	HashRecordKey k = *((const HashRecordKey *) key);
	Datum		h;

	if (k.is_null)
		return 0;

	h = FunctionCall1Coll(k.hash_proc, DEFAULT_COLLATION_OID, k.value);
	return DatumGetUInt32(h);
}

/*
 * Matching function for records, to be used in hashtable lookups.
 */
static int
record_match(const void *key1, const void *key2, Size keysize)
{
	HashRecordKey k1 = *((const HashRecordKey *) key1);
	HashRecordKey k2 = *((const HashRecordKey *) key2);
	Datum		c;

	if (k1.is_null)
	{
		if (k2.is_null)
			return 0;			/* NULL "=" NULL */
		else
			return 1;			/* NULL ">" not-NULL */
	}
	else if (k2.is_null)
		return -1;				/* not-NULL "<" NULL */

	c = FunctionCall2Coll(k1.cmp_proc, DEFAULT_COLLATION_OID,
						  k1.value, k2.value);
	return DatumGetInt32(c);
}

void
init_record(RecordVar *record, TupleDesc tupdesc, Variable *variable)
{
	HASHCTL		ctl;
	char		hash_name[BUFSIZ];
	MemoryContext oldcxt,
				topctx;
	TypeCacheEntry *typentry;
	Oid			keyid;

	Assert(variable->typid == RECORDOID);

	/* First get hash and match functions for key type. */
	keyid = GetTupleDescAttr(tupdesc, 0)->atttypid;
	typentry = lookup_type_cache(keyid,
								 TYPECACHE_HASH_PROC_FINFO |
								 TYPECACHE_CMP_PROC_FINFO);

	/*
	 * In case something went wrong, you need to roll back the changes before
	 * completing the transaction, because the variable may be regular and not
	 * present in list of changed vars.
	 */
	if (!OidIsValid(typentry->hash_proc_finfo.fn_oid))
	{
		/* At this point variable is just created, so we simply remove it. */
		removeObject(&variable->transObject, TRANS_VARIABLE);
		ereport(ERROR,
				(errcode(ERRCODE_UNDEFINED_FUNCTION),
				 errmsg("could not identify a hash function for type %s",
						format_type_be(keyid))));
	}

	if (!OidIsValid(typentry->cmp_proc_finfo.fn_oid))
	{
		removeObject(&variable->transObject, TRANS_VARIABLE);
		ereport(ERROR,
				(errcode(ERRCODE_UNDEFINED_FUNCTION),
				 errmsg("could not identify a matching function for type %s",
						format_type_be(keyid))));
	}

	/* Initialize the record */

	sprintf(hash_name, "Records hash for variable \"%s\"", GetName(variable));

	topctx = variable->is_transactional ?
		variable->package->hctxTransact :
		variable->package->hctxRegular;

#if PG_VERSION_NUM >= 120000
	record->hctx = AllocSetContextCreateInternal(topctx,
												 hash_name,
												 ALLOCSET_DEFAULT_MINSIZE,
												 ALLOCSET_DEFAULT_INITSIZE,
												 ALLOCSET_DEFAULT_MAXSIZE);
#elif PG_VERSION_NUM >= 110000
	record->hctx = AllocSetContextCreateExtended(topctx,
												 hash_name,
												 ALLOCSET_DEFAULT_MINSIZE,
												 ALLOCSET_DEFAULT_INITSIZE,
												 ALLOCSET_DEFAULT_MAXSIZE);
#else
	record->hctx = AllocSetContextCreate(topctx,
										 hash_name,
										 ALLOCSET_DEFAULT_MINSIZE,
										 ALLOCSET_DEFAULT_INITSIZE,
										 ALLOCSET_DEFAULT_MAXSIZE);
#endif

	oldcxt = MemoryContextSwitchTo(record->hctx);
	record->tupdesc = CreateTupleDescCopy(tupdesc);
#if PG_VERSION_NUM < 120000
	record->tupdesc->tdhasoid = false;
#endif
	record->tupdesc->tdtypeid = RECORDOID;
	record->tupdesc->tdtypmod = -1;
	record->tupdesc = BlessTupleDesc(record->tupdesc);

	/* Initialize hash table. */
	ctl.keysize = sizeof(HashRecordKey);
	ctl.entrysize = sizeof(HashRecordEntry);
	ctl.hcxt = record->hctx;
	ctl.hash = record_hash;
	ctl.match = record_match;

	record->rhash = hash_create(hash_name, NUMVARIABLES, &ctl,
								HASH_ELEM | HASH_CONTEXT |
								HASH_FUNCTION | HASH_COMPARE);

	fmgr_info(typentry->hash_proc_finfo.fn_oid, &record->hash_proc);
	fmgr_info(typentry->cmp_proc_finfo.fn_oid, &record->cmp_proc);

	MemoryContextSwitchTo(oldcxt);
}

/* Check if any attributes of type UNKNOWNOID are in given tupdesc */
static int
is_unknownoid_in_tupdesc(TupleDesc tupdesc)
{
	int 	i = 0;
	for (i = 0; i < tupdesc->natts; i++)
	{
		Form_pg_attribute attr = GetTupleDescAttr(tupdesc, i);

		if (attr->atttypid == UNKNOWNOID)
			return true;

	}
	return false;
}

/* Replace all attributes of type UNKNOWNOID to TEXTOID in given tupdesc */
static void
coerce_unknown_rewrite_tupdesc(TupleDesc old_tupdesc, TupleDesc *return_tupdesc)
{
	int 		i;

	(*return_tupdesc) = CreateTupleDescCopy(old_tupdesc);

	for (i = 0; i < old_tupdesc->natts; i++)
	{
		Form_pg_attribute attr = GetTupleDescAttr(old_tupdesc, i);

		if (attr->atttypid == UNKNOWNOID)
		{
			FormData_pg_attribute new_attr = *attr;

			new_attr.atttypid = TEXTOID;
			new_attr.attlen = -1;
			new_attr.atttypmod = -1;
			memcpy(TupleDescAttr((*return_tupdesc), i), &new_attr, sizeof(FormData_pg_attribute));
			populate_compact_attribute(*return_tupdesc, i);
		}
	}
}

/*
 * Deform tuple with old_tupdesc, coerce values of type UNKNOWNOID to TEXTOID, form tuple with new_tupdesc.
 * new_tupdesc must have the same attributes as old_tupdesc except such of types UNKNOWNOID -- they must be of TEXTOID type
 */
static void
reconstruct_tuple(TupleDesc old_tupdesc, TupleDesc new_tupdesc, HeapTupleHeader *rec)
{
	HeapTupleData tuple;
	HeapTuple	newtup;
	Datum 	   *values = (Datum*)palloc(old_tupdesc->natts * sizeof(Datum));
	bool	   *isnull = (bool*)palloc(old_tupdesc->natts * sizeof(bool));
	Oid			baseTypeId = UNKNOWNOID;
	int32		baseTypeMod = -1;
	int32		inputTypeMod = -1;
	Type		baseType = NULL;
	int 		i;

	baseTypeId = getBaseTypeAndTypmod(TEXTOID, &baseTypeMod);
	baseType = typeidType(baseTypeId);
	/* Build a temporary HeapTuple control structure */
	tuple.t_len = HeapTupleHeaderGetDatumLength(*rec);
	tuple.t_data = *rec;
	heap_deform_tuple(&tuple, old_tupdesc, values, isnull);

	for (i = 0; i < old_tupdesc->natts; i++)
	{
		Form_pg_attribute attr = GetTupleDescAttr(old_tupdesc, i);

		if (attr->atttypid == UNKNOWNOID)
		{
			values[i] = stringTypeDatum(baseType,
										DatumGetCString(values[i]),
										inputTypeMod);
		}
	}

	newtup = heap_form_tuple(new_tupdesc, values, isnull);
	(*rec) = newtup->t_data;
	pfree(isnull);
	pfree(values);
	ReleaseSysCache(baseType);
}

/*
 * Used in pg_variables.c insert_record for coercing types in first record in variable.
 * If there are UNKNOWNOIDs in tupdesc, rewrites it and reconstructs tuple with new tupdesc.
 * Replaces given tupdesc with the new one.
 */
void
coerce_unknown_first_record(TupleDesc *tupdesc, HeapTupleHeader *rec)
{
	TupleDesc	new_tupdesc = NULL;

	if (!is_unknownoid_in_tupdesc(*tupdesc))
		return;

	coerce_unknown_rewrite_tupdesc(*tupdesc, &new_tupdesc);
	reconstruct_tuple(*tupdesc, new_tupdesc, rec);

	ReleaseTupleDesc(*tupdesc);
	(*tupdesc) = new_tupdesc;
}

/*
 * New record structure should be the same as the first record.
 */
void
check_attributes(Variable *variable, HeapTupleHeader *rec, TupleDesc tupdesc)
{
	int			i;
	RecordVar  *record;
	bool		unknowns = false;

	Assert(variable->typid == RECORDOID);

	record = &(GetActualValue(variable).record);
	/* First, check columns count. */
	if (record->tupdesc->natts != tupdesc->natts)
		ereport(ERROR,
				(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
				 errmsg("new record structure have %d attributes, but variable "
						"\"%s\" structure have %d.",
						tupdesc->natts, GetName(variable), record->tupdesc->natts)));

	/* Second, check columns type. */
	for (i = 0; i < tupdesc->natts; i++)
	{
		Form_pg_attribute attr1 = GetTupleDescAttr(record->tupdesc, i),
					attr2 = GetTupleDescAttr(tupdesc, i);

		/*
		 * For the sake of convenience, we consider all the unknown types are to be
		 * a text type.
		 */
		if (convert_unknownoid && (attr1->atttypid == TEXTOID) && (attr2->atttypid == UNKNOWNOID))
		{
			unknowns = true;
			continue;
		}

		if ((attr1->atttypid != attr2->atttypid)
			|| (attr1->attndims != attr2->attndims)
			|| (attr1->atttypmod != attr2->atttypmod))
			ereport(ERROR,
					(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
					 errmsg("new record attribute type for attribute number %d "
							"differs from variable \"%s\" structure.",
							i + 1, GetName(variable)),
					 errhint("You may need explicit type casts.")));
	}

	if (unknowns)
		reconstruct_tuple(tupdesc, record->tupdesc, rec);
}

/*
 * Check record key type. If not same then throw a error.
 */
void
check_record_key(Variable *variable, Oid typid)
{
	RecordVar  *record;

	Assert(variable->typid == RECORDOID);
	record = &(GetActualValue(variable).record);

	if (GetTupleDescAttr(record->tupdesc, 0)->atttypid != typid)
		ereport(ERROR,
				(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
				 errmsg("requested value type differs from variable \"%s\" "
						"key type", GetName(variable))));
}

static Datum
copy_record_tuple(RecordVar *record, HeapTupleHeader tupleHeader)
{
	TupleDesc	tupdesc;
	HeapTupleHeader result;
	int			tuple_len;

	tupdesc = record->tupdesc;

	/*
	 * If the tuple contains any external TOAST pointers, we have to inline
	 * those fields to meet the conventions for composite-type Datums.
	 */
	if (HeapTupleHeaderHasExternal(tupleHeader))
		return toast_flatten_tuple_to_datum(tupleHeader,
											HeapTupleHeaderGetDatumLength(tupleHeader),
											tupdesc);

	/*
	 * Fast path for easy case: just make a palloc'd copy and insert the
	 * correct composite-Datum header fields (since those may not be set if
	 * the given tuple came from disk, rather than from heap_form_tuple).
	 */
	tuple_len = HeapTupleHeaderGetDatumLength(tupleHeader);
	result = (HeapTupleHeader) palloc(tuple_len);
	memcpy((char *) result, (char *) tupleHeader, tuple_len);

	HeapTupleHeaderSetDatumLength(result, tuple_len);
	HeapTupleHeaderSetTypeId(result, tupdesc->tdtypeid);
	HeapTupleHeaderSetTypMod(result, tupdesc->tdtypmod);

	return PointerGetDatum(result);
}

static Datum
get_record_key(Datum tuple, TupleDesc tupdesc, bool *isnull)
{
	HeapTupleHeader th = (HeapTupleHeader) DatumGetPointer(tuple);
	bool		hasnulls = th->t_infomask & HEAP_HASNULL;
	bits8	   *bp = th->t_bits;	/* ptr to null bitmap in tuple */
	char	   *tp;				/* ptr to tuple data */
	long		off;			/* offset in tuple data */
	int			keyatt = 0;
	Form_pg_attribute attr = GetTupleDescAttr(tupdesc, keyatt);

	if (hasnulls && att_isnull(keyatt, bp))
	{
		*isnull = true;
		return (Datum) NULL;
	}

	tp = (char *) th + th->t_hoff;
	off = 0;
	if (attr->attlen == -1)
		off = att_align_pointer(off, attr->attalign, -1, tp + off);
	else
	{
		/* not varlena, so safe to use att_align_nominal */
		off = att_align_nominal(off, attr->attalign);
	}

	*isnull = false;
	return fetchatt(attr, tp + off);
}

/*
 * Insert a new record. New record key should be unique in the variable.
 */
void
insert_record(Variable *variable, HeapTupleHeader tupleHeader)
{
	Datum		tuple;
	Datum		value;
	bool		isnull;
	RecordVar  *record;
	HashRecordKey k;
	HashRecordEntry *item;
	bool		found;
	MemoryContext oldcxt;

	Assert(variable->typid == RECORDOID);

	record = &(GetActualValue(variable).record);

	oldcxt = MemoryContextSwitchTo(record->hctx);

	tuple = copy_record_tuple(record, tupleHeader);

	/* Inserting a new record */
	value = get_record_key(tuple, record->tupdesc, &isnull);
	/* First, check if there is a record with same key */
	k.value = value;
	k.is_null = isnull;
	k.hash_proc = &record->hash_proc;
	k.cmp_proc = &record->cmp_proc;

	item = (HashRecordEntry *) hash_search(record->rhash, &k,
										   HASH_ENTER, &found);
	if (found)
	{
		pfree(DatumGetPointer(tuple));
		MemoryContextSwitchTo(oldcxt);
		ereport(ERROR,
				(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
				 errmsg("there is a record in the variable \"%s\" with same "
						"key", GetName(variable))));
	}
	/* Second, insert a new record */
	item->tuple = tuple;

	MemoryContextSwitchTo(oldcxt);
}

/*
 * Insert a record. New record key should be unique in the variable.
 */
bool
update_record(Variable *variable, HeapTupleHeader tupleHeader)
{
	Datum		tuple;
	Datum		value;
	bool		isnull;
	RecordVar  *record;
	HashRecordKey k;
	HashRecordEntry *item;
	bool		found;
	MemoryContext oldcxt;

	Assert(variable->typid == RECORDOID);

	record = &(GetActualValue(variable).record);

	oldcxt = MemoryContextSwitchTo(record->hctx);

	tuple = copy_record_tuple(record, tupleHeader);

	/* Update a record */
	value = get_record_key(tuple, record->tupdesc, &isnull);
	k.value = value;
	k.is_null = isnull;
	k.hash_proc = &record->hash_proc;
	k.cmp_proc = &record->cmp_proc;

	item = (HashRecordEntry *) hash_search(record->rhash, &k,
										   HASH_FIND, &found);
	if (!found)
	{
		pfree(DatumGetPointer(tuple));
		MemoryContextSwitchTo(oldcxt);
		return false;
	}

	/* Release old tuple */
	pfree(DatumGetPointer(item->tuple));
	item->tuple = tuple;

	MemoryContextSwitchTo(oldcxt);
	return true;
}

bool
delete_record(Variable *variable, Datum value, bool is_null)
{
	HashRecordKey k;
	HashRecordEntry *item;
	bool		found;
	RecordVar  *record;

	Assert(variable->typid == RECORDOID);

	record = &(GetActualValue(variable).record);

	/* Delete a record */
	k.value = value;
	k.is_null = is_null;
	k.hash_proc = &record->hash_proc;
	k.cmp_proc = &record->cmp_proc;

	item = (HashRecordEntry *) hash_search(record->rhash, &k,
										   HASH_REMOVE, &found);
	if (found)
		pfree(DatumGetPointer(item->tuple));

	return found;
}

/*
 * Copy record using src_tuple.
 */
void
insert_record_copy(RecordVar *dest_record, Datum src_tuple, Variable *variable)
{
	Datum		tuple;
	Datum		value;
	bool		isnull;
	HashRecordKey k;
	HashRecordEntry *item;
	bool		found;
	MemoryContext oldcxt;

	oldcxt = MemoryContextSwitchTo(dest_record->hctx);

	/* Inserting a new record into dest_record */
	tuple = copy_record_tuple(dest_record,
							  (HeapTupleHeader) DatumGetPointer(src_tuple));
	value = get_record_key(tuple, dest_record->tupdesc, &isnull);

	k.value = value;
	k.is_null = isnull;
	k.hash_proc = &dest_record->hash_proc;
	k.cmp_proc = &dest_record->cmp_proc;

	item = (HashRecordEntry *) hash_search(dest_record->rhash, &k,
										   HASH_ENTER, &found);
	if (found)
	{
		pfree(DatumGetPointer(tuple));
		MemoryContextSwitchTo(oldcxt);
		ereport(ERROR,
				(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
				 errmsg("there is a record in the variable \"%s\" with same "
						"key", GetName(variable))));
	}
	item->tuple = tuple;

	MemoryContextSwitchTo(oldcxt);
}
