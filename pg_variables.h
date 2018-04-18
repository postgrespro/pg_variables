/*-------------------------------------------------------------------------
 *
 * pg_variables.c
 *	  exported definitions for pg_variables.c
 *
 * Copyright (c) 2015-2016, Postgres Professional
 *
 *-------------------------------------------------------------------------
 */
#ifndef __PG_VARIABLES_H__
#define __PG_VARIABLES_H__

#include "pg_config.h"

#include "access/htup.h"
#include "access/tupdesc.h"
#include "datatype/timestamp.h"
#include "utils/date.h"
#include "utils/hsearch.h"
#include "utils/numeric.h"
#include "utils/jsonb.h"
#include "lib/ilist.h"

/* Accessor for the i'th attribute of tupdesc. */
#if PG_VERSION_NUM > 100000
#define GetTupleDescAttr(tupdesc, i) (TupleDescAttr(tupdesc, i))
#else
#define GetTupleDescAttr(tupdesc, i) ((tupdesc)->attrs[(i)])
#endif

/* initial number of packages hashes */
#define NUMPACKAGES 8
#define NUMVARIABLES 16

typedef struct HashPackageEntry
{
	char			name[NAMEDATALEN];
	HTAB		   *variablesHash;
	/* Memory context for package variables for easy memory release */
	MemoryContext	hctx;
} HashPackageEntry;

typedef struct RecordVar
{
	HTAB		   *rhash;
	TupleDesc		tupdesc;
	/* Memory context for records hash table for easy memory release */
	MemoryContext	hctx;
	/* Hash function info */
	FmgrInfo		hash_proc;
	/* Match function info */
	FmgrInfo		cmp_proc;
} RecordVar;

typedef struct ScalarVar
{
	Datum			value;
	bool			is_null;
	bool			typbyval;
	int16			typlen;
} ScalarVar;

/* List node that stores one of the variables states */
typedef struct ValueHistoryEntry{
	dlist_node		node;
	union
	{
		ScalarVar	scalar;
		RecordVar	record;
	}				value;
} ValueHistoryEntry;

typedef dlist_head ValueHistory;

/* Variable by itself */
typedef struct HashVariableEntry
{
	char			name[NAMEDATALEN];
	/* Entry point to list with states of value */
	ValueHistory 	data;
	Oid				typid;
	/*
	 * The flag determines the further behavior of the variable.
	 * Can be specified only when creating a variable.
	 */
	bool			is_transactional;
} HashVariableEntry;

typedef struct HashRecordKey
{
	Datum		value;
	bool		is_null;
	/* Hash function info */
	FmgrInfo   *hash_proc;
	/* Match function info */
	FmgrInfo   *cmp_proc;
} HashRecordKey;

typedef struct HashRecordEntry
{
	HashRecordKey	key;
	HeapTuple		tuple;
} HashRecordEntry;

/* Element of list with variables, changed within transaction */
typedef struct ChangedVarsNode
{
	dlist_node			node;
	HashPackageEntry   *package;
	HashVariableEntry  *variable;
} ChangedVarsNode;

extern void init_attributes(HashVariableEntry* variable, TupleDesc tupdesc,
							MemoryContext topctx);
extern void check_attributes(HashVariableEntry *variable, TupleDesc tupdesc);
extern void check_record_key(HashVariableEntry *variable, Oid typid);

extern void insert_record(HashVariableEntry* variable,
						  HeapTupleHeader tupleHeader);
extern bool update_record(HashVariableEntry *variable,
						  HeapTupleHeader tupleHeader);
extern bool delete_record(HashVariableEntry* variable, Datum value,
						  bool is_null);
extern void clean_records(HashVariableEntry *variable);

extern void insert_savepoint(HashVariableEntry *variable,
							MemoryContext packageContext);

/* Internal macros to manage with dlist structure */
#define get_actual_value_scalar(variable) \
	&((dlist_head_element(ValueHistoryEntry, node, &variable->data))->value.scalar)
#define get_actual_value_record(variable) \
	&((dlist_head_element(ValueHistoryEntry, node, &variable->data))->value.record)
#define get_history_entry(node_ptr) \
	dlist_container(ValueHistoryEntry, node, node_ptr)

#endif   /* __PG_VARIABLES_H__ */
