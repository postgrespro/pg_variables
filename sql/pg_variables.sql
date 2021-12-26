CREATE EXTENSION pg_variables;

-- Test packages - sanity checks
SELECT pgv_free();
SELECT pgv_exists(NULL); -- fail
SELECT pgv_remove(NULL); -- fail
SELECT pgv_remove('vars'); -- fail
SELECT pgv_exists('vars111111111111111111111111111111111111111111111111111111111111'); -- fail

-- Integer variables
SELECT pgv_get_int('vars', 'int1');
SELECT pgv_get_int('vars', 'int1', false);

SELECT pgv_set_int('vars', 'int1', 101);
SELECT pgv_set_int('vars', 'int2', 102);

SELECT pgv_get_int('vars', 'int1');
SELECT pgv_get_int('vars', 'int2');
SELECT pgv_set_int('vars', 'int1', 103);
SELECT pgv_get_int('vars', 'int1');

SELECT pgv_get_int('vars', 'int3');
SELECT pgv_get_int('vars', 'int3', false);
SELECT pgv_exists('vars', 'int3');
SELECT pgv_exists('vars', 'int1');
SELECT pgv_exists('vars2');
SELECT pgv_exists('vars');

SELECT pgv_set_int('vars', 'intNULL', NULL);
SELECT pgv_get_int('vars', 'intNULL');

-- Text variables
SELECT pgv_set_text('vars', 'str1', 's101');
SELECT pgv_set_text('vars', 'int1', 's101');
SELECT pgv_set_int('vars', 'str1', 101);
SELECT pgv_set_text('vars', 'str2', 's102');

SELECT pgv_get_text('vars', 'str1');
SELECT pgv_get_text('vars', 'str2');
SELECT pgv_set_text('vars', 'str1', 's103');
SELECT pgv_get_text('vars', 'str1');

SELECT pgv_get_text('vars', 'str3');
SELECT pgv_get_text('vars', 'str3', false);
SELECT pgv_exists('vars', 'str3');
SELECT pgv_exists('vars', 'str1');
SELECT pgv_get_text('vars', 'int1');
SELECT pgv_get_int('vars', 'str1');

SELECT pgv_set_text('vars', 'strNULL', NULL);
SELECT pgv_get_text('vars', 'strNULL');

-- Numeric variables
SELECT pgv_set_numeric('vars', 'num1', 1.01);
SELECT pgv_set_numeric('vars', 'num2', 1.02);
SELECT pgv_set_numeric('vars', 'str1', 1.01);

SELECT pgv_get_numeric('vars', 'num1');
SELECT pgv_get_numeric('vars', 'num2');
SELECT pgv_set_numeric('vars', 'num1', 1.03);
SELECT pgv_get_numeric('vars', 'num1');

SELECT pgv_get_numeric('vars', 'num3');
SELECT pgv_get_numeric('vars', 'num3', false);
SELECT pgv_exists('vars', 'num3');
SELECT pgv_exists('vars', 'num1');
SELECT pgv_get_numeric('vars', 'str1');

SELECT pgv_set_numeric('vars', 'numNULL', NULL);
SELECT pgv_get_numeric('vars', 'numNULL');

SET timezone = 'Europe/Moscow';

-- Timestamp variables
SELECT pgv_set_timestamp('vars', 'ts1', '2016-03-30 10:00:00');
SELECT pgv_set_timestamp('vars', 'ts2', '2016-03-30 11:00:00');
SELECT pgv_set_timestamp('vars', 'num1', '2016-03-30 12:00:00');

SELECT pgv_get_timestamp('vars', 'ts1');
SELECT pgv_get_timestamp('vars', 'ts2');
SELECT pgv_set_timestamp('vars', 'ts1', '2016-03-30 12:00:00');
SELECT pgv_get_timestamp('vars', 'ts1');

SELECT pgv_get_timestamp('vars', 'ts3');
SELECT pgv_get_timestamp('vars', 'ts3', false);
SELECT pgv_exists('vars', 'ts3');
SELECT pgv_exists('vars', 'ts1');
SELECT pgv_get_timestamp('vars', 'num1');

SELECT pgv_set_timestamp('vars', 'tsNULL', NULL);
SELECT pgv_get_timestamp('vars', 'tsNULL');

-- TimestampTZ variables

SELECT pgv_set_timestamptz('vars', 'tstz1', '2016-03-30 10:00:00 GMT+01');
SELECT pgv_set_timestamptz('vars', 'tstz2', '2016-03-30 11:00:00 GMT+02');
SELECT pgv_set_timestamptz('vars', 'ts1', '2016-03-30 12:00:00 GMT+03');

SELECT pgv_get_timestamptz('vars', 'tstz1');
SELECT pgv_get_timestamptz('vars', 'tstz2');
SELECT pgv_set_timestamptz('vars', 'tstz1', '2016-03-30 12:00:00 GMT+01');
SELECT pgv_get_timestamptz('vars', 'tstz1');

SELECT pgv_get_timestamptz('vars', 'tstz3');
SELECT pgv_get_timestamptz('vars', 'tstz3', false);
SELECT pgv_exists('vars', 'tstz3');
SELECT pgv_exists('vars', 'tstz1');
SELECT pgv_get_timestamptz('vars', 'ts1');

SELECT pgv_set_timestamptz('vars', 'tstzNULL', NULL);
SELECT pgv_get_timestamptz('vars', 'tstzNULL');

-- Date variables
SELECT pgv_set_date('vars', 'd1', '2016-03-29');
SELECT pgv_set_date('vars', 'd2', '2016-03-30');
SELECT pgv_set_date('vars', 'tstz1', '2016-04-01');

SELECT pgv_get_date('vars', 'd1');
SELECT pgv_get_date('vars', 'd2');
SELECT pgv_set_date('vars', 'd1', '2016-04-02');
SELECT pgv_get_date('vars', 'd1');

SELECT pgv_get_date('vars', 'd3');
SELECT pgv_get_date('vars', 'd3', false);
SELECT pgv_exists('vars', 'd3');
SELECT pgv_exists('vars', 'd1');
SELECT pgv_get_date('vars', 'tstz1');

SELECT pgv_set_date('vars', 'dNULL', NULL);
SELECT pgv_get_date('vars', 'dNULL');

-- Jsonb variables
SELECT pgv_set_jsonb('vars2', 'j1', '[1, 2, "foo", null]');
SELECT pgv_set_jsonb('vars2', 'j2', '{"bar": "baz", "balance": 7.77, "active": false}');
SELECT pgv_set_jsonb('vars', 'd1', '[1, 2, "foo", null]');

SELECT pgv_get_jsonb('vars2', 'j1');
SELECT pgv_get_jsonb('vars2', 'j2');
SELECT pgv_set_jsonb('vars2', 'j1', '{"foo": [true, "bar"], "tags": {"a": 1, "b": null}}');
SELECT pgv_get_jsonb('vars2', 'j1');

SELECT pgv_get_jsonb('vars2', 'j3');
SELECT pgv_get_jsonb('vars2', 'j3', false);
SELECT pgv_exists('vars2', 'j3');
SELECT pgv_exists('vars2', 'j1');
SELECT pgv_get_jsonb('vars', 'd1');

SELECT pgv_set_jsonb('vars', 'jNULL', NULL);
SELECT pgv_get_jsonb('vars', 'jNULL');

-- Record variables
CREATE TABLE tab (id int, t varchar);
INSERT INTO tab VALUES (0, 'str00'), (1, 'str11'), (2, NULL), (NULL, 'strNULL');

SELECT pgv_insert('vars3', 'r1', tab) FROM tab;
SELECT pgv_insert('vars2', 'j1', tab) FROM tab;
SELECT pgv_insert('vars3', 'r1', tab) FROM tab;

SELECT pgv_insert('vars3', 'r1', row(1, 'str1', 'str2'));
SELECT pgv_insert('vars3', 'r1', row(1, 1));
SELECT pgv_insert('vars3', 'r1', row('str1', 'str1'));
SELECT pgv_select('vars3', 'r1', ARRAY[[1,2]]); -- fail

-- Test variables caching
SELECT pgv_insert('vars3', 'r2', row(1, 'str1', 'str2'));
SELECT pgv_update('vars3', 'r1', row(3, 'str22'::varchar));
SELECT pgv_update('vars4', 'r1', row(3, 'str22'::varchar)); -- fail
select pgv_delete('vars3', 'r2', NULL::int);
select pgv_delete('vars4', 'r2', NULL::int); -- fail

-- Test NULL values
SELECT pgv_insert('vars3', 'r2', NULL); -- fail
SELECT pgv_update('vars3', 'r2', NULL); -- fail
select pgv_delete('vars3', 'r2', NULL::int);
SELECT pgv_select('vars3', 'r1', NULL::int[]); -- fail

SELECT pgv_select('vars3', 'r1');
SELECT pgv_select('vars3', 'r1', 1);
SELECT pgv_select('vars3', 'r1', 1::float); -- fail
SELECT pgv_select('vars3', 'r1', 0);
SELECT pgv_select('vars3', 'r1', NULL::int);
SELECT pgv_select('vars3', 'r1', ARRAY[1, 0, NULL]);

UPDATE tab SET t = 'str33' WHERE id = 1;
SELECT pgv_update('vars3', 'r1', tab) FROM tab;
SELECT pgv_update('vars3', 'r1', row(4, 'str44'::varchar));
SELECT pgv_select('vars3', 'r1');

SELECT pgv_delete('vars3', 'r1', 1);
SELECT pgv_select('vars3', 'r1');
SELECT pgv_delete('vars3', 'r1', 100);

SELECT pgv_select('vars3', 'r3');
SELECT pgv_exists('vars3', 'r3');
SELECT pgv_exists('vars3', 'r1');
SELECT pgv_select('vars2', 'j1');

-- PGPRO-2601 - Test pgv_select() on TupleDesc of dropped table
DROP TABLE tab;
SELECT pgv_select('vars3', 'r1');

-- Tests for SRF's sequential scan of an internal hash table
DO
$$BEGIN
    PERFORM pgv_select('vars3', 'r1') LIMIT 2 OFFSET 2;
    PERFORM pgv_select('vars3', 'r3');
END$$;
-- Check that the hash table was cleaned up after rollback
SET client_min_messages to 'ERROR';
SELECT pgv_select('vars3', 'r1', 1);
SELECT pgv_select('vars3', 'r1') LIMIT 2; -- warning
SELECT pgv_select('vars3', 'r1') LIMIT 2 OFFSET 2;

-- PGPRO-2601 - Test a cursor with the hash table
BEGIN;
DECLARE r1_cur CURSOR FOR SELECT pgv_select('vars3', 'r1');
FETCH 1 in r1_cur;
SELECT pgv_select('vars3', 'r1');
FETCH 1 in r1_cur;
CLOSE r1_cur;
COMMIT; -- warning
RESET client_min_messages;

-- Clean memory after unsuccessful creation of a variable
SELECT pgv_insert('vars4', 'r1', row (('str1'::text, 'str1'::text))); -- fail
SELECT package FROM pgv_stats() WHERE package = 'vars4';

-- Remove package if it is empty
SELECT pgv_insert('vars4', 'r2', row(1, 'str1', 'str2'));
SELECT pgv_remove('vars4', 'r2');
SELECT package FROM pgv_stats() WHERE package = 'vars4';

-- Record variables as scalar
SELECT pgv_set('vars5', 'r1', row(1, 'str11'));
SELECT pgv_get('vars5', 'r1', NULL::record);
SELECT pgv_set('vars5', 'r1', row(1, 'str11'), true); -- fail

SELECT pgv_insert('vars5', 'r1', row(1, 'str11')); -- fail
SELECT pgv_select('vars5', 'r1'); -- fail

SELECT pgv_get('vars3', 'r1', NULL::record); -- fail

-- Manipulate variables
SELECT * FROM pgv_list() order by package, name;
SELECT package FROM pgv_stats() order by package;

SELECT pgv_remove('vars', 'int3');
SELECT pgv_remove('vars', 'int1');
SELECT pgv_get_int('vars', 'int1');
SELECT pgv_exists('vars');

SELECT pgv_remove('vars2');
SELECT pgv_get_jsonb('vars2', 'j1');
SELECT pgv_exists('vars2');

SELECT * FROM pgv_list() order by package, name;

SELECT pgv_free();
SELECT pgv_exists('vars');

SELECT * FROM pgv_list() order by package, name;
-- Check insert of record with various amount of fields
CREATE TEMP TABLE foo(id int, t text);
INSERT INTO foo VALUES (0, 'str00');

SELECT pgv_insert('vars', 'r1', row(1, 'str1'::text, 'str2'::text));
SELECT pgv_select('vars', 'r1');
SELECT pgv_insert('vars', 'r1', foo) FROM foo;
SELECT pgv_select('vars', 'r1');

SELECT pgv_insert('vars', 'r2', row(1, 'str1')); -- ok, UNKNOWNOID of 'str1' converts to TEXTOID
SELECT pgv_insert('vars', 'r2', foo) FROM foo; -- ok
SELECT pgv_select('vars', 'r2');

SELECT pgv_insert('vars', 'r3', row(1, 'str1'::text));
SELECT pgv_insert('vars', 'r3', foo) FROM foo; -- ok, no conversions
SELECT pgv_select('vars', 'r3');

SELECT pgv_insert('vars', 'r4', row(1, 2::int));
SELECT pgv_insert('vars', 'r4', row(0, 'str1')); -- fail, UNKNOWNOID of 'str1' can't be converted to int
SELECT pgv_select('vars', 'r4');

SELECT pgv_insert('vars', 'r5', foo) FROM foo; -- types: int, text
SELECT pgv_insert('vars', 'r5', row(1, 'str1')); -- ok, UNKNOWNOID of 'str1' converts to TEXTOID
SELECT pgv_select('vars', 'r5');
