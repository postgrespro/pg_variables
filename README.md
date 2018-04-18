# pg_variables - session variables with various types

[![Build Status](https://travis-ci.org/postgrespro/pg_variables.svg?branch=master)](https://travis-ci.org/postgrespro/pg_variables)
[![codecov](https://codecov.io/gh/postgrespro/pg_variables/branch/master/graph/badge.svg)](https://codecov.io/gh/postgrespro/pg_variables)
[![GitHub license](https://img.shields.io/badge/license-PostgreSQL-blue.svg)](https://raw.githubusercontent.com/postgrespro/pg_variables/master/README.md)

## Introduction

The **pg_variables** module provides functions to work with variables of various
types. Created variables live only in the current user session.

Note that the module does **not support transactions and savepoints by default**. For
example:

```sql
SELECT pgv_set('vars', 'int1', 101);
BEGIN;
SELECT pgv_set('vars', 'int2', 102);
ROLLBACK;

SELECT * FROM pgv_list() order by package, name;
 package | name | is_transactional
---------+------+------------------
 vars    | int1 | f
 vars    | int2 | f
(2 rows)
```

But if variable created with flag **is_transactional**, it does:
```sql
BEGIN;
SELECT pgv_set('vars', 'trans_int', 101, true);
SAVEPOINT sp1;
SELECT pgv_set('vars', 'trans_int', 102, true);
ROLLBACK TO sp1;
COMMIT;
SELECT pgv_get('vars', 'trans_int', NULL::int);

 pgv_get
---------
     101
(1 row)
```

## License

This module available under the same license as
[PostgreSQL](http://www.postgresql.org/about/licence/).

## Installation

Typical installation procedure may look like this:

    $ cd pg_variables
    $ make USE_PGXS=1
    $ sudo make USE_PGXS=1 install
    $ make USE_PGXS=1 installcheck
    $ psql DB -c "CREATE EXTENSION pg_variables;"

## Module functions

The functions provided by the **pg_variables** module are shown in the tables
below. The module supports the following scalar and record types.

To use **pgv_get()** function required package and variable must exists. It is
necessary to set variable with **pgv_set()** function to use **pgv_get()**
function.

If a package does not exists you will get the following error:

```sql
SELECT pgv_get('vars', 'int1', NULL::int);
ERROR:  unrecognized package "vars"
```

If a variable does not exists you will get the following error:

```sql
SELECT pgv_get('vars', 'int1', NULL::int);
ERROR:  unrecognized variable "int1"
```

**pgv_get()** function check the variable type. If the variable type does not
match with the function type the error will be raised:

```sql
SELECT pgv_get('vars', 'int1', NULL::text);
ERROR:  variable "int1" requires "integer" value
```

## Scalar variables functions

Function | Returns
-------- | -------
`pgv_set(package text, name text, value anynonarray, is_transactional bool default false)` | `void`
`pgv_get(package text, name text, var_type anynonarray, strict bool default true)` | `anynonarray`

## **Deprecated** scalar variables functions

### Integer variables

Function | Returns
-------- | -------
`pgv_set_int(package text, name text, value int, is_transactional bool default false)` | `void`
`pgv_get_int(package text, name text, strict bool default true)` | `int`

### Text variables

Function | Returns
-------- | -------
`pgv_set_text(package text, name text, value text, is_transactional bool default false)` | `void`
`pgv_get_text(package text, name text, strict bool default true)` | `text`

### Numeric variables

Function | Returns
-------- | -------
`pgv_set_numeric(package text, name text, value numeric, is_transactional bool default false)` | `void`
`pgv_get_numeric(package text, name text, strict bool default true)` | `numeric`

### Timestamp variables

Function | Returns
-------- | -------
`pgv_set_timestamp(package text, name text, value timestamp, is_transactional bool default false)` | `void`
`pgv_get_timestamp(package text, name text, strict bool default true)` | `timestamp`

### Timestamp with timezone variables

Function | Returns
-------- | -------
`pgv_set_timestamptz(package text, name text, value timestamptz, is_transactional bool default false)` | `void`
`pgv_get_timestamptz(package text, name text, strict bool default true)` | `timestamptz`

### Date variables

Function | Returns
-------- | -------
`pgv_set_date(package text, name text, value date, is_transactional bool default false)` | `void`
`pgv_get_date(package text, name text, strict bool default true)` | `date`

### Jsonb variables

Function | Returns
-------- | -------
`pgv_set_jsonb(package text, name text, value jsonb, is_transactional bool default false)` | `void`
`pgv_get_jsonb(package text, name text, strict bool default true)` | `jsonb`

## Record variables functions

The following functions are provided by the module to work with collections of
record types.

To use **pgv_update()**, **pgv_delete()** and **pgv_select()** functions
required package and variable must exists. Otherwise the error will be raised.
It is necessary to set variable with **pgv_insert()** function to use these
functions.

**pgv_update()**, **pgv_delete()** and **pgv_select()** functions check the
variable type. If the variable type does not **record** type the error will be
raised.

Function | Returns | Description
-------- | ------- | -----------
`pgv_insert(package text, name text, r record, is_transactional bool default false)` | `void` | Inserts a record to the variable collection. If package and variable do not exists they will be created. The first column of **r** will be a primary key. If exists a record with the same primary key the error will be raised. If this variable collection has other structure the error will be raised.
`pgv_update(package text, name text, r record)` | `boolean` | Updates a record with the corresponding primary key (the first column of **r** is a primary key). Returns **true** if a record was found. If this variable collection has other structure the error will be raised.
`pgv_delete(package text, name text, value anynonarray)` | `boolean` | Deletes a record with the corresponding primary key (the first column of **r** is a primary key). Returns **true** if a record was found.
`pgv_select(package text, name text)` | `set of record` | Returns the variable collection records.
`pgv_select(package text, name text, value anynonarray)` | `record` | Returns the record with the corresponding primary key (the first column of **r** is a primary key).
`pgv_select(package text, name text, value anyarray)` | `set of record` | Returns the variable collection records with the corresponding primary keys (the first column of **r** is a primary key).

### Miscellaneous functions

Function | Returns | Description
-------- | ------- | -----------
`pgv_exists(package text, name text)` | `bool` | Returns **true** if package and variable exists.
`pgv_exists(package text)` | `bool` | Returns **true** if package exists.
`pgv_remove(package text, name text)` | `void` | Removes the variable with the corresponding name. Required package and variable must exists, otherwise the error will be raised.
`pgv_remove(package text)` | `void` | Removes the package and all package variables with the corresponding name. Required package must exists, otherwise the error will be raised.
`pgv_free()` | `void` | Removes all packages and variables.
`pgv_list()` | `table(package text, name text, is_transactional bool)` | Returns set of records of assigned packages and variables.
`pgv_stats()` | `table(package text, used_memory bigint)` | Returns list of assigned packages and used memory in bytes.

Note that **pgv_stats()** works only with the PostgreSQL 9.6 and newer.

## Examples

It is easy to use functions to work with scalar variables:

```sql
SELECT pgv_set('vars', 'int1', 101);
SELECT pgv_set('vars', 'int2', 102);

SELECT pgv_get('vars', 'int1', NULL::int);
 pgv_get_int
-------------
         101
(1 row)

SELECT pgv_get('vars', 'int2', NULL::int);
 pgv_get_int
-------------
         102
(1 row)
```

Let's assume we have a **tab** table:

```sql
CREATE TABLE tab (id int, t varchar);
INSERT INTO tab VALUES (0, 'str00'), (1, 'str11');
```

Then you can use functions to work with record variables:

```sql
SELECT pgv_insert('vars', 'r1', tab) FROM tab;

SELECT pgv_select('vars', 'r1');
 pgv_select
------------
 (1,str11)
 (0,str00)
(2 rows)

SELECT pgv_select('vars', 'r1', 1);
 pgv_select
------------
 (1,str11)
(1 row)

SELECT pgv_select('vars', 'r1', 0);
 pgv_select
------------
 (0,str00)
(1 row)

SELECT pgv_select('vars', 'r1', ARRAY[1, 0]);
 pgv_select
------------
 (1,str11)
 (0,str00)
(2 rows)

SELECT pgv_delete('vars', 'r1', 1);

SELECT pgv_select('vars', 'r1');
 pgv_select
------------
 (0,str00)
(1 row)
```

You can list packages and variables:

```sql
SELECT * FROM pgv_list() order by package, name;
 package | name
---------+------
 vars    | int1
 vars    | int2
 vars    | r1
(3 rows)
```

And get used memory in bytes:

```sql
SELECT * FROM pgv_stats() order by package;
 package | used_memory
---------+-------------
 vars    |       16736
(1 row)
```

You can delete variables or whole packages:

```sql
SELECT pgv_remove('vars', 'int1');
SELECT pgv_remove('vars');
```

You can delete all packages and variables:
```sql
SELECT pgv_free();
```

If you want variables with support of transactions and savepoints, you should add flag
`is_transactional = true` as the last argument in functions `pgv_set()`
or `pgv_insert()`.
Following use cases describe behavior of transactional variables:
```sql
SELECT pgv_set('pack', 'var_text', 'before transaction block'::text, true);
BEGIN;
SELECT pgv_set('pack', 'var_text', 'before savepoint'::text, true);
SAVEPOINT sp1;
SELECT pgv_set('pack', 'var_text', 'savepoint sp1'::text, true);
SAVEPOINT sp2;
SELECT pgv_set('pack', 'var_text', 'savepoint sp2'::text, true);
RELEASE sp2;
SELECT pgv_get('pack', 'var_text', NULL::text);
    pgv_get
---------------
 savepoint sp2

ROLLBACK TO sp1;
SELECT pgv_get('pack', 'var_text', NULL::text);
     pgv_get
------------------
 before savepoint
(1 row)

ROLLBACK;
SELECT pgv_get('pack', 'var_text', NULL::text);
         pgv_get
--------------------------
 before transaction block

```
If you create variable after `BEGIN` or `SAVEPOINT` and than rollback to previous state - variable will not be exist:
```sql
BEGIN;
SAVEPOINT sp1;
SAVEPOINT sp2;
SELECT pgv_set('pack', 'var_int', 122, true);
RELEASE SAVEPOINT sp2;
SELECT pgv_get('pack', 'var_int', NULL::int);
pgv_get
---------
     122
(1 row)

ROLLBACK TO sp1;
SELECT pgv_get('pack','var_int', NULL::int);
ERROR:  unrecognized variable "var_int"
COMMIT;
```
If you created transactional variable once, you should use flag `is_transactional` every time when you want to change variable value by functions `pgv_set()`, `pgv_insert()` and deprecated setters (i.e. `pgv_set_int()`). If you try to change this option, you'll get an error:
```sql
SELECT pgv_insert('pack', 'var_record', row(123::int, 'text'::text), true);
 pgv_insert
------------

(1 row)

SELECT pgv_insert('pack', 'var_record', row(456::int, 'another text'::text));
ERROR:  variable "var_record" already created as TRANSACTIONAL
```
Functions `pgv_update()` and `pgv_delete()` do not require this flag.
