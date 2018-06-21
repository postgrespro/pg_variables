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

typedef struct RecordVar
{
	HTAB	   *rhash;
	TupleDesc	tupdesc;
	/* Memory context for records hash table for easy memory release */
	MemoryContext hctx;
	/* Hash function info */
	FmgrInfo	hash_proc;
	/* Match function info */
	FmgrInfo	cmp_proc;
} RecordVar;

typedef struct ScalarVar
{
	Datum		value;
	bool		is_null;
	bool		typbyval;
	int16		typlen;
} ScalarVar;

/* State of TransObject instance */
typedef struct TransState
{
	dlist_node	node;
	bool		is_valid;
	int			level;
} TransState;

/* List node that stores one of the package's states */
typedef struct PackState
{
	TransState	state;
} PackState;

/* List node that stores one of the variable's states */
typedef struct VarState
{
	TransState	state;
	union
	{
		ScalarVar scalar;
		RecordVar record;
	}		value;
} VarState;

/* Transactional object */
typedef struct TransObject
{
	char		name[NAMEDATALEN];
	dlist_head	states;
} TransObject;

/* Transactional package */
typedef struct Package
{
	TransObject transObject;
	HTAB	   *varHashRegular,
			   *varHashTransact;
	/* Memory context for package variables for easy memory release */
	MemoryContext hctxRegular,
				  hctxTransact;
} Package;

/* Transactional variable */
typedef struct Variable
{
	TransObject	transObject;
	Package	   *package;
	Oid			typid;
	/*
	 * The flag determines the further behavior of the variable.
	 * Can be specified only when creating a variable.
	 */
	bool		is_transactional;
} Variable;

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
	HashRecordKey key;
	HeapTuple	tuple;
} HashRecordEntry;

/* Element of list with objects created, changed or removed within transaction */
typedef struct ChangedObject
{
	dlist_node		node;
	TransObject	   *object;
} ChangedObject;

/* Type of transactional object instance */
typedef enum TransObjectType
{
	TOP_PACKAGE,
	TOP_VARIABLE
} TransObjectType;

/* Element of stack with 'changedVars' and 'changedPacks' list heads*/
typedef struct ChangesStackNode
{
	dlist_node	node;
	dlist_head *changedVarsList;
	dlist_head *changedPacksList;
	MemoryContext ctx;
} ChangesStackNode;

extern void init_record(RecordVar *record, TupleDesc tupdesc, Variable *variable);
extern void check_attributes(Variable *variable, TupleDesc tupdesc);
extern void check_record_key(Variable *variable, Oid typid);

extern void insert_record(Variable* variable,
						  HeapTupleHeader tupleHeader);
extern bool update_record(Variable *variable,
						  HeapTupleHeader tupleHeader);
extern bool delete_record(Variable* variable, Datum value,
						  bool is_null);

#define GetActualState(object) \
	(dlist_head_element(TransState, node, &((TransObject *) object)->states))

#define GetActualValue(variable) \
	(((VarState *) GetActualState(variable))->value)

#define GetName(object) \
	(AssertVariableIsOfTypeMacro(object->transObject, TransObject), \
	 object->transObject.name)

#define GetStateStorage(object) \
	(AssertVariableIsOfTypeMacro(object->transObject, TransObject), \
	 &(object->transObject.states))

#endif   /* __PG_VARIABLES_H__ */
