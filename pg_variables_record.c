/*-------------------------------------------------------------------------
 *
 * pg_variables_record.c
 *	  Functions to work with record types
 *
 * Copyright (c) 2015-2016, Postgres Professional
 *
 *-------------------------------------------------------------------------
 */
#include "postgres.h"

#include "access/htup_details.h"
#include "catalog/pg_collation.h"
#include "catalog/pg_type.h"
#include "utils/builtins.h"
#include "utils/datum.h"
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
	 * completing the transaction, because the variable may be regular
	 * and not present in list of changed vars.
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
	record->tupdesc = CreateTupleDescCopyConstr(tupdesc);

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

/*
 * New record structure should be the same as the first record.
 */
void
check_attributes(Variable *variable, TupleDesc tupdesc)
{
	int			i;
	RecordVar  *record;

	Assert(variable->typid == RECORDOID);

	record = &(GetActualValue(variable).record);
	/* First, check columns count. */
	if (record->tupdesc->natts != tupdesc->natts)
		ereport(ERROR,
				(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
				 errmsg("new record structure differs from variable \"%s\" "
						"structure", GetName(variable))));

	/* Second, check columns type. */
	for (i = 0; i < tupdesc->natts; i++)
	{
		Form_pg_attribute attr1 = GetTupleDescAttr(record->tupdesc, i),
					attr2 = GetTupleDescAttr(tupdesc, i);

		if ((attr1->atttypid != attr2->atttypid)
			|| (attr1->attndims != attr2->attndims)
			|| (attr1->atttypmod != attr2->atttypmod))
			ereport(ERROR,
					(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
					 errmsg("new record structure differs from variable \"%s\" "
							"structure", GetName(variable))));
	}
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

/*
 * Insert a new record. New record key should be unique in the variable.
 */
void
insert_record(Variable *variable, HeapTupleHeader tupleHeader)
{
	TupleDesc	tupdesc;
	HeapTuple	tuple;
	int			tuple_len;
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

	tupdesc = record->tupdesc;

	/* Build a HeapTuple control structure */
	tuple_len = HeapTupleHeaderGetDatumLength(tupleHeader);

	tuple = (HeapTuple) palloc(HEAPTUPLESIZE + tuple_len);
	tuple->t_len = tuple_len;
	ItemPointerSetInvalid(&(tuple->t_self));
	tuple->t_tableOid = InvalidOid;
	tuple->t_data = (HeapTupleHeader) ((char *) tuple + HEAPTUPLESIZE);
	memcpy((char *) tuple->t_data, (char *) tupleHeader, tuple_len);

	/* Inserting a new record */
	value = fastgetattr(tuple, 1, tupdesc, &isnull);
	/* First, check if there is a record with same key */
	k.value = value;
	k.is_null = isnull;
	k.hash_proc = &record->hash_proc;
	k.cmp_proc = &record->cmp_proc;

	item = (HashRecordEntry *) hash_search(record->rhash, &k,
										   HASH_ENTER, &found);
	if (found)
	{
		heap_freetuple(tuple);
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
	TupleDesc	tupdesc;
	HeapTuple	tuple;
	int			tuple_len;
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

	tupdesc = record->tupdesc;

	/* Build a HeapTuple control structure */
	tuple_len = HeapTupleHeaderGetDatumLength(tupleHeader);

	tuple = (HeapTuple) palloc(HEAPTUPLESIZE + tuple_len);
	tuple->t_len = tuple_len;
	ItemPointerSetInvalid(&(tuple->t_self));
	tuple->t_tableOid = InvalidOid;
	tuple->t_data = (HeapTupleHeader) ((char *) tuple + HEAPTUPLESIZE);
	memcpy((char *) tuple->t_data, (char *) tupleHeader, tuple_len);

	/* Update a record */
	value = fastgetattr(tuple, 1, tupdesc, &isnull);
	k.value = value;
	k.is_null = isnull;
	k.hash_proc = &record->hash_proc;
	k.cmp_proc = &record->cmp_proc;

	item = (HashRecordEntry *) hash_search(record->rhash, &k,
										   HASH_FIND, &found);
	if (!found)
	{
		heap_freetuple(tuple);
		MemoryContextSwitchTo(oldcxt);
		return false;
	}

	/* Release old tuple */
	heap_freetuple(item->tuple);
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
		heap_freetuple(item->tuple);

	return found;
}
