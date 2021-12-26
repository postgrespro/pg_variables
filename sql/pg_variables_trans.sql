SET timezone = 'Europe/Moscow'; -- Need to proper output of datetime variables
--CHECK SAVEPOINT RELEASE
BEGIN;
-- Declare variables
SELECT pgv_set('vars', 'any1', 'some value'::text, true);
SELECT pgv_set('vars', 'any2', 'some value'::text);
SELECT pgv_set_int('vars', 'int1', 101, true);
SELECT pgv_set_int('vars', 'int2', 102);
SELECT pgv_set_int('vars', 'intNULL', NULL, true);
SELECT pgv_set_text('vars', 'str1', 's101', true);
SELECT pgv_set_text('vars', 'str2', 's102');
SELECT pgv_set_numeric('vars', 'num1', 1.01, true);
SELECT pgv_set_numeric('vars', 'num2', 1.02);
SELECT pgv_set_timestamp('vars', 'ts1', '2016-03-30 10:00:00', true);
SELECT pgv_set_timestamp('vars', 'ts2', '2016-03-30 11:00:00');
SELECT pgv_set_timestamptz('vars', 'tstz1', '2016-03-30 10:00:00 GMT+01', true);
SELECT pgv_set_timestamptz('vars', 'tstz2', '2016-03-30 11:00:00 GMT+02');
SELECT pgv_set_date('vars', 'd1', '2016-03-29', true);
SELECT pgv_set_date('vars', 'd2', '2016-03-30');
SELECT pgv_set_jsonb('vars2', 'j1', '[1, 2, "foo", null]', true);
SELECT pgv_set_jsonb('vars2', 'j2', '{"bar": "baz", "balance": 7.77, "active": false}');

SAVEPOINT comm;
-- Set new values
SELECT pgv_set('vars', 'any1', 'another value'::text, true);
SELECT pgv_set('vars', 'any2', 'another value'::text);
SELECT pgv_set_int('vars', 'int1', 103, true);
SELECT pgv_set_int('vars', 'int2', 103);
SELECT pgv_set_int('vars', 'intNULL', 104, true);
SELECT pgv_set_text('vars', 'str1', 's103', true);
SELECT pgv_set_text('vars', 'str2', 's103');
SELECT pgv_set_numeric('vars', 'num1', 1.03, true);
SELECT pgv_set_numeric('vars', 'num2', 1.03);
SELECT pgv_set_timestamp('vars', 'ts1', '2016-03-30 12:00:00', true);
SELECT pgv_set_timestamp('vars', 'ts2', '2016-03-30 12:00:00');
SELECT pgv_set_timestamptz('vars', 'tstz1', '2016-03-30 12:00:00 GMT+03', true);
SELECT pgv_set_timestamptz('vars', 'tstz2', '2016-03-30 12:00:00 GMT+03');
SELECT pgv_set_date('vars', 'd1', '2016-04-02', true);
SELECT pgv_set_date('vars', 'd2', '2016-04-02');
SELECT pgv_set_jsonb('vars2', 'j1', '{"foo": [true, "bar"], "tags": {"a": 1, "b": null}}', true);
SELECT pgv_set_jsonb('vars2', 'j2', '{"foo": [true, "bar"], "tags": {"a": 1, "b": null}}');

-- Check values before releasing savepoint
SELECT pgv_get('vars', 'any1',NULL::text);
SELECT pgv_get('vars', 'any2',NULL::text);
SELECT pgv_get_int('vars', 'int1');
SELECT pgv_get_int('vars', 'int2');
SELECT pgv_get_int('vars', 'intNULL');
SELECT pgv_get_text('vars', 'str1');
SELECT pgv_get_text('vars', 'str2');
SELECT pgv_get_numeric('vars', 'num1');
SELECT pgv_get_numeric('vars', 'num2');
SELECT pgv_get_timestamp('vars', 'ts1');
SELECT pgv_get_timestamp('vars', 'ts2');
SELECT pgv_get_timestamptz('vars', 'tstz1');
SELECT pgv_get_timestamptz('vars', 'tstz2');
SELECT pgv_get_date('vars', 'd1');
SELECT pgv_get_date('vars', 'd2');
SELECT pgv_get_jsonb('vars2', 'j1');
SELECT pgv_get_jsonb('vars2', 'j2');

-- Check values after releasing savepoint
RELEASE comm;
SELECT pgv_get('vars', 'any1',NULL::text);
SELECT pgv_get('vars', 'any2',NULL::text);
SELECT pgv_get_int('vars', 'int1');
SELECT pgv_get_int('vars', 'int2');
SELECT pgv_get_int('vars', 'intNULL');
SELECT pgv_get_text('vars', 'str1');
SELECT pgv_get_text('vars', 'str2');
SELECT pgv_get_numeric('vars', 'num1');
SELECT pgv_get_numeric('vars', 'num2');
SELECT pgv_get_timestamp('vars', 'ts1');
SELECT pgv_get_timestamp('vars', 'ts2');
SELECT pgv_get_timestamptz('vars', 'tstz1');
SELECT pgv_get_timestamptz('vars', 'tstz2');
SELECT pgv_get_date('vars', 'd1');
SELECT pgv_get_date('vars', 'd2');
SELECT pgv_get_jsonb('vars2', 'j1');
SELECT pgv_get_jsonb('vars2', 'j2');
COMMIT;

CREATE TABLE tab (id int, t varchar);
INSERT INTO tab VALUES (0, 'str00'), (1, 'str33'), (2, NULL), (NULL, 'strNULL');

BEGIN;
SELECT pgv_insert('vars3', 'r1', tab, true) FROM tab;
SELECT pgv_insert('vars3', 'r2', tab) FROM tab;
SAVEPOINT comm;
SELECT pgv_insert('vars3', 'r1', row(5 :: integer, 'str55' :: varchar),true);
SELECT pgv_insert('vars3', 'r2', row(5 :: integer, 'str55' :: varchar));
SELECT pgv_select('vars3', 'r1');
SELECT pgv_select('vars3', 'r2');
RELEASE comm;
SELECT pgv_select('vars3', 'r1');
SELECT pgv_select('vars3', 'r2');
COMMIT;



--CHECK SAVEPOINT ROLLBACK
BEGIN;
-- Variables are already declared
SAVEPOINT comm2;
-- Set new values
SELECT pgv_set('vars', 'any1', 'one more value'::text, true);
SELECT pgv_set('vars', 'any2', 'one more value'::text);
SELECT pgv_set_int('vars', 'int1', 101, true);
SELECT pgv_set_int('vars', 'int2', 102);
SELECT pgv_set_int('vars', 'intNULL', NULL, true);
SELECT pgv_set_text('vars', 'str1', 's101', true);
SELECT pgv_set_text('vars', 'str2', 's102');
SELECT pgv_set_numeric('vars', 'num1', 1.01, true);
SELECT pgv_set_numeric('vars', 'num2', 1.02);
SELECT pgv_set_timestamp('vars', 'ts1', '2016-03-30 10:00:00', true);
SELECT pgv_set_timestamp('vars', 'ts2', '2016-03-30 11:00:00');
SELECT pgv_set_timestamptz('vars', 'tstz1', '2016-03-30 10:00:00 GMT+01', true);
SELECT pgv_set_timestamptz('vars', 'tstz2', '2016-03-30 11:00:00 GMT+02');
SELECT pgv_set_date('vars', 'd1', '2016-03-29', true);
SELECT pgv_set_date('vars', 'd2', '2016-03-30');
SELECT pgv_set_jsonb('vars2', 'j1', '[1, 2, "foo", null]', true);
SELECT pgv_set_jsonb('vars2', 'j2', '{"bar": "baz", "balance": 7.77, "active": false}');

-- Check values before rollback to savepoint
SELECT pgv_get('vars', 'any1',NULL::text);
SELECT pgv_get('vars', 'any2',NULL::text);
SELECT pgv_get_int('vars', 'int1');
SELECT pgv_get_int('vars', 'int2');
SELECT pgv_get_int('vars', 'intNULL');
SELECT pgv_get_text('vars', 'str1');
SELECT pgv_get_text('vars', 'str2');
SELECT pgv_get_numeric('vars', 'num1');
SELECT pgv_get_numeric('vars', 'num2');
SELECT pgv_get_timestamp('vars', 'ts1');
SELECT pgv_get_timestamp('vars', 'ts2');
SELECT pgv_get_timestamptz('vars', 'tstz1');
SELECT pgv_get_timestamptz('vars', 'tstz2');
SELECT pgv_get_date('vars', 'd1');
SELECT pgv_get_date('vars', 'd2');
SELECT pgv_get_jsonb('vars2', 'j1');
SELECT pgv_get_jsonb('vars2', 'j2');

-- Check values after rollback to savepoint
ROLLBACK TO comm2;
SELECT pgv_get('vars', 'any1',NULL::text);
SELECT pgv_get('vars', 'any2',NULL::text);
SELECT pgv_get_int('vars', 'int1');
SELECT pgv_get_int('vars', 'int2');
SELECT pgv_get_int('vars', 'intNULL');
SELECT pgv_get_text('vars', 'str1');
SELECT pgv_get_text('vars', 'str2');
SELECT pgv_get_numeric('vars', 'num1');
SELECT pgv_get_numeric('vars', 'num2');
SELECT pgv_get_timestamp('vars', 'ts1');
SELECT pgv_get_timestamp('vars', 'ts2');
SELECT pgv_get_timestamptz('vars', 'tstz1');
SELECT pgv_get_timestamptz('vars', 'tstz2');
SELECT pgv_get_date('vars', 'd1');
SELECT pgv_get_date('vars', 'd2');
SELECT pgv_get_jsonb('vars2', 'j1');
SELECT pgv_get_jsonb('vars2', 'j2');
COMMIT;


-- Record variables
BEGIN;
SAVEPOINT comm2;
SELECT pgv_delete('vars3', 'r1', 5);
SELECT pgv_delete('vars3', 'r2', 5);
SELECT pgv_select('vars3', 'r1');
SELECT pgv_select('vars3', 'r2');
ROLLBACK to comm2;
SELECT pgv_select('vars3', 'r1');
SELECT pgv_select('vars3', 'r2');
COMMIT;


-- TRYING TO CHANGE FLAG 'IS_TRANSACTIONAL'
SELECT pgv_set('vars', 'any1', 'value'::text);
SELECT pgv_set('vars', 'any2', 'value'::text, true);
SELECT pgv_set_int('vars', 'int1', 301);
SELECT pgv_set_int('vars', 'int2', 302, true);
SELECT pgv_set_text('vars', 'str1', 's301');
SELECT pgv_set_text('vars', 'str2', 's302', true);
SELECT pgv_set_numeric('vars', 'num1', 3.01);
SELECT pgv_set_numeric('vars', 'num2', 3.02, true);
SELECT pgv_set_timestamp('vars', 'ts1', '2016-03-30 20:00:00');
SELECT pgv_set_timestamp('vars', 'ts2', '2016-03-30 21:00:00', true);
SELECT pgv_set_timestamptz('vars', 'tstz1', '2016-03-30 20:00:00 GMT+01');
SELECT pgv_set_timestamptz('vars', 'tstz2', '2016-03-30 21:00:00 GMT+02', true);
SELECT pgv_set_date('vars', 'd1', '2016-04-29');
SELECT pgv_set_date('vars', 'd2', '2016-04-30', true);
SELECT pgv_set_jsonb('vars2', 'j1', '[1, 2, "foo2", null]');
SELECT pgv_set_jsonb('vars2', 'j2', '{"bar": "baz2", "balance": 7.77, "active": true}', true);
SELECT pgv_insert('vars3', 'r1', row(6 :: integer, 'str66' :: varchar));
SELECT pgv_insert('vars3', 'r2', row(6 :: integer, 'str66' :: varchar),true);

-- CHECK pgv_list() WHILE WE HAVE A LOT OF MISCELLANEOUS VARIABLES
SELECT * FROM pgv_list() order by package, name;

SELECT pgv_free();

-- VARIABLES DECLARED IN SUBTRANSACTION SHOULD BE DESTROYED AFTER ROLLBACK TO SAVEPOINT
-- For better readability we don't use deprecated api functions in test below
BEGIN;
SAVEPOINT sp_to_rollback;
SELECT pgv_set('vars', 'any1', 'text value'::text, true);
SELECT pgv_set('vars', 'any2', 'text value'::text);
SELECT pgv_insert('vars3', 'r1', row(6 :: integer, 'str44' :: varchar), true);
SELECT pgv_insert('vars3', 'r2', row(6 :: integer, 'str44' :: varchar));
ROLLBACK TO sp_to_rollback;
COMMIT;
SELECT pgv_get('vars', 'any1',NULL::text);
SELECT pgv_get('vars', 'any2',NULL::text);
SELECT pgv_select('vars3', 'r1');
SELECT pgv_select('vars3', 'r2');

SELECT pgv_free();


-- CHECK ROLLBACK AFTER COMMITTING SUBTRANSACTION
BEGIN;
SELECT pgv_set('vars', 'any1', 'before savepoint sp1'::text, true);
SAVEPOINT sp1;
SELECT pgv_set('vars', 'any1', 'after savepoint sp1'::text, true);
SAVEPOINT sp2;
SELECT pgv_set('vars', 'any1', 'after savepoint sp2'::text, true);
RELEASE sp2;
SELECT pgv_get('vars', 'any1',NULL::text);
ROLLBACK TO sp1;
SELECT pgv_get('vars', 'any1',NULL::text);
COMMIT;

BEGIN;
SAVEPOINT sp1;
SAVEPOINT sp2;
SELECT pgv_set('vars2', 'any1', 'variable exists'::text, true);
RELEASE sp2;
SELECT pgv_get('vars2', 'any1',NULL::text);
ROLLBACK TO sp1;
COMMIT;
SELECT pgv_get('vars2', 'any1',NULL::text);
SELECT pgv_free();

--CHECK TRANSACTION COMMIT
-- Declare variables
SELECT pgv_set('vars', 'any1', 'some value'::text, true);
SELECT pgv_set('vars', 'any2', 'some value'::text);

BEGIN;
-- Set new values
SELECT pgv_set('vars', 'any1', 'another value'::text, true);
SELECT pgv_set('vars', 'any2', 'another value'::text);
-- Check values before committing transaction
SELECT pgv_get('vars', 'any1',NULL::text);
SELECT pgv_get('vars', 'any2',NULL::text);
-- Check values after committing transaction
COMMIT;
SELECT pgv_get('vars', 'any1',NULL::text);
SELECT pgv_get('vars', 'any2',NULL::text);


SELECT pgv_insert('vars3', 'r1', tab, true) FROM tab;
SELECT pgv_insert('vars3', 'r2', tab) FROM tab;
BEGIN;
SELECT pgv_insert('vars3', 'r1', row(5 :: integer, 'str55' :: varchar),true);
SELECT pgv_insert('vars3', 'r2', row(5 :: integer, 'str55' :: varchar));
SELECT pgv_select('vars3', 'r1');
SELECT pgv_select('vars3', 'r2');
COMMIT;
SELECT pgv_select('vars3', 'r1');
SELECT pgv_select('vars3', 'r2');


-- CHECK TRANSACTION ROLLBACK
-- Variables are already declared
BEGIN;
-- Set new values
SELECT pgv_set('vars', 'any1', 'one more value'::text, true);
SELECT pgv_set('vars', 'any2', 'one more value'::text);

-- Check values before rollback
SELECT pgv_get('vars', 'any1',NULL::text);
SELECT pgv_get('vars', 'any2',NULL::text);

-- Check values after rollback
ROLLBACK;
SELECT pgv_get('vars', 'any1',NULL::text);
SELECT pgv_get('vars', 'any2',NULL::text);

-- Record variables
BEGIN;
SELECT pgv_delete('vars3', 'r1', 5);
SELECT pgv_delete('vars3', 'r2', 5);
SELECT pgv_select('vars3', 'r1');
SELECT pgv_select('vars3', 'r2');
ROLLBACK;
SELECT pgv_select('vars3', 'r1');
SELECT pgv_select('vars3', 'r2');

SELECT pgv_free();


-- VARIABLES DECLARED IN TRANSACTION SHOULD BE DESTROYED AFTER ROLLBACK
BEGIN;
SELECT pgv_set('vars', 'any1', 'text value'::text, true);
SELECT pgv_set('vars', 'any2', 'text value'::text);
SELECT pgv_insert('vars', 'r1', row(6 :: integer, 'str44' :: varchar), true);
SELECT pgv_insert('vars', 'r2', row(6 :: integer, 'str44' :: varchar));
ROLLBACK;
SELECT pgv_get('vars', 'any1',NULL::text);
SELECT pgv_get('vars', 'any2',NULL::text);
SELECT pgv_select('vars', 'r1');
SELECT pgv_select('vars', 'r2');

SELECT pgv_remove('vars');


-- CHECK ROLLBACK AFTER COMMITTING SUBTRANSACTION
SELECT pgv_set('vars', 'any1', 'before transaction block'::text, true);
BEGIN;
SELECT pgv_set('vars', 'any1', 'before savepoint sp1'::text, true);
SAVEPOINT sp1;
SELECT pgv_set('vars', 'any1', 'after savepoint sp1'::text, true);
SAVEPOINT sp2;
SELECT pgv_set('vars', 'any1', 'after savepoint sp2'::text, true);
RELEASE sp2;
SELECT pgv_get('vars', 'any1',NULL::text);
ROLLBACK TO sp1;
SELECT pgv_get('vars', 'any1',NULL::text);
ROLLBACK;
SELECT pgv_get('vars', 'any1',NULL::text);

BEGIN;
SAVEPOINT sp1;
SELECT pgv_set('vars2', 'any1', 'variable exists'::text, true);
RELEASE sp1;
SELECT pgv_get('vars2', 'any1',NULL::text);
ROLLBACK;
SELECT pgv_get('vars2', 'any1',NULL::text);

SELECT pgv_free();

-- Additional tests
SELECT pgv_insert('vars3', 'r1', tab, true) FROM tab;
BEGIN;
SELECT pgv_insert('vars3', 'r1', row(5 :: integer, 'before savepoint sp1' :: varchar),true);
SAVEPOINT sp1;
SELECT pgv_update('vars3', 'r1', row(5 :: integer, 'after savepoint sp1' :: varchar));
SAVEPOINT sp2;
SELECT pgv_insert('vars3', 'r1', row(7 :: integer, 'row after sp2 to remove in sp4' :: varchar),true);
SAVEPOINT sp3;
SAVEPOINT sp4;
SELECT pgv_delete('vars3', 'r1', 7);
SAVEPOINT sp5;
SELECT pgv_select('vars3', 'r1');
ROLLBACK TO sp5;
SELECT pgv_select('vars3', 'r1');
RELEASE sp4;
SELECT pgv_select('vars3', 'r1');
ROLLBACK TO sp3;
SELECT pgv_select('vars3', 'r1');
RELEASE sp2;
SELECT pgv_select('vars3', 'r1');
ROLLBACK TO sp1;
SELECT pgv_select('vars3', 'r1');
COMMIT;
SELECT pgv_select('vars3', 'r1');

SELECT pgv_set('vars', 'any1', 'outer'::text, true);
BEGIN;
SELECT pgv_set('vars', 'any1', 'begin'::text, true);
SAVEPOINT sp1;
SELECT pgv_set('vars', 'any1', 'sp1'::text, true);
SAVEPOINT sp2;
SELECT pgv_set('vars', 'any1', 'sp2'::text, true);
SAVEPOINT sp3;
SAVEPOINT sp4;
SELECT pgv_set('vars', 'any1', 'sp4'::text, true);
SAVEPOINT sp5;
SELECT pgv_get('vars', 'any1',NULL::text);
ROLLBACK TO sp5;
SELECT pgv_get('vars', 'any1',NULL::text);
RELEASE sp4;
SELECT pgv_get('vars', 'any1',NULL::text);
ROLLBACK TO sp3;
SELECT pgv_get('vars', 'any1',NULL::text);
RELEASE sp2;
SELECT pgv_get('vars', 'any1',NULL::text);
ROLLBACK TO sp1;
SELECT pgv_get('vars', 'any1',NULL::text);
ROLLBACK;
SELECT pgv_get('vars', 'any1',NULL::text);

BEGIN;
SELECT pgv_set('vars', 'any1', 'wrong type'::varchar, true);
COMMIT;

-- THE REMOVAL OF THE VARIABLE MUST BE CANCELED ON ROLLBACK
SELECT pgv_set('vars', 'any1', 'variable exists'::text, true);
BEGIN;
SELECT pgv_remove('vars', 'any1');
SELECT pgv_exists('vars', 'any1');
ROLLBACK;
SELECT pgv_exists('vars', 'any1');
SELECT pgv_get('vars', 'any1',NULL::text);

BEGIN;
SELECT pgv_remove('vars', 'any1');
SELECT pgv_exists('vars', 'any1');
COMMIT;
SELECT pgv_exists('vars', 'any1');
SELECT pgv_get('vars', 'any1',NULL::text);

SELECT * FROM pgv_list() ORDER BY package, name;
BEGIN;
SELECT pgv_free();
ROLLBACK;
SELECT * FROM pgv_list() ORDER BY package, name;

BEGIN;
SELECT pgv_free();
COMMIT;
SELECT * FROM pgv_list() ORDER BY package, name;

SELECT pgv_set('vars', 'regular', 'regular variable exists'::text);
SELECT pgv_set('vars', 'trans1', 'trans1 variable exists'::text, true);
BEGIN;
SELECT pgv_free();
SELECT pgv_free(); -- Check sequential package removal in one subtransaction
SELECT * FROM pgv_list() ORDER BY package, name;
SELECT pgv_set('vars', 'trans2', 'trans2 variable exists'::text, true);
SELECT * FROM pgv_list() ORDER BY package, name;
SELECT pgv_remove('vars');
SELECT * FROM pgv_list() ORDER BY package, name;
ROLLBACK;
SELECT * FROM pgv_list() ORDER BY package, name;

BEGIN;
SAVEPOINT sp1;
SAVEPOINT sp2;
SAVEPOINT sp3;
SELECT pgv_set('vars2', 'trans2', 'trans2 variable exists'::text, true);
SAVEPOINT sp4;
SAVEPOINT sp5;
SELECT pgv_free();
SELECT package FROM pgv_stats() ORDER BY package;
SELECT * FROM pgv_list() ORDER BY package, name;
RELEASE sp5;
SELECT package FROM pgv_stats() ORDER BY package;
SELECT * FROM pgv_list() ORDER BY package, name;
RELEASE sp4;
SELECT package FROM pgv_stats() ORDER BY package;
SELECT * FROM pgv_list() ORDER BY package, name;
COMMIT;
SELECT package FROM pgv_stats() ORDER BY package;

BEGIN;
SELECT pgv_set('vars', 'trans1', 'package created'::text, true);
SELECT pgv_remove('vars');
SELECT * FROM pgv_list() ORDER BY package, name;
SELECT pgv_set('vars', 'trans1', 'package restored'::text, true);
SELECT * FROM pgv_list() ORDER BY package, name;
COMMIT;
SELECT pgv_remove('vars');

-- REMOVED TRANSACTIONAL VARIABLE SHOULD BE NOT ACCESSIBLE THROUGH LastVariable
SELECT pgv_insert('package', 'errs',row(n), true)
FROM generate_series(1,5) AS gs(n) WHERE 1.0/(n-3)<>0;
SELECT pgv_insert('package', 'errs',row(1), true);

-- Variable should not exists in case when error occurs during creation
SELECT pgv_insert('vars4', 'r1', row (('str1'::text, 'str1'::text)));
SELECT pgv_select('vars4', 'r1', 0);

-- If variable created and removed in same transaction level,
-- it should be totally removed and should not be present
-- in changes list and cache.
BEGIN;
SELECT pgv_set('vars', 'any1', 'some value'::text, true);
SAVEPOINT comm;
SELECT pgv_remove('vars', 'any1');
RELEASE comm;
SELECT pgv_get('vars', 'any1',NULL::text);
COMMIT;

-- Tests for PGPRO-2440
SELECT pgv_insert('vars3', 'r3', row(1 :: integer, NULL::varchar), true);
BEGIN;
SELECT pgv_insert('vars3', 'r3', row(2 :: integer, NULL::varchar), true);
SAVEPOINT comm;
SELECT pgv_insert('vars3', 'r3', row(3 :: integer, NULL::varchar), true);
COMMIT;
SELECT pgv_delete('vars3', 'r3', 3);

BEGIN;
SELECT pgv_set('vars1', 't1', ''::text);
SELECT pgv_set('vars2', 't2', ''::text, true);
SAVEPOINT sp1;
SAVEPOINT sp2;
SELECT pgv_free();
ERROR;
COMMIT;

BEGIN;
SELECT pgv_set('vars', 'any1', 'some value'::text, true);
SELECT pgv_free();
SAVEPOINT sp_to_rollback;
SELECT pgv_set('vars', 'any1', 'some value'::text, true);
ROLLBACK TO sp_to_rollback;
COMMIT;
SELECT package FROM pgv_stats() ORDER BY package;

-- Package should exist after rollback if it contains regular variable
BEGIN;
SELECT pgv_set('vars', 'any1', 'some value'::text);
ROLLBACK;
SELECT package FROM pgv_stats() ORDER BY package;

-- Package should not exist if it becomes empty in rolled back transaction
BEGIN;
SAVEPOINT comm2;
SELECT pgv_remove('vars');
ROLLBACK TO comm2;
SELECT pgv_exists('vars');
SELECT package FROM pgv_stats() ORDER BY package;
COMMIT;
SELECT package FROM pgv_stats() ORDER BY package;

SELECT pgv_set('vars', 'any1', 'some value'::text);
BEGIN;
SELECT pgv_remove('vars');
ROLLBACK;
SELECT package FROM pgv_stats() ORDER BY package;

SELECT pgv_free();

-- Variables should be insertable after pgv_remove (variable)
BEGIN;
SELECT pgv_insert('test', 'x', ROW (1::int, 2::int), TRUE);
SELECT pgv_select('test', 'x');
SELECT pgv_remove('test', 'x');
SELECT pgv_insert('test', 'x', ROW (1::int, 3::int), TRUE);
SELECT pgv_select('test', 'x');
SELECT pgv_insert('test', 'x', ROW (2::int, 4::int), TRUE);
SELECT pgv_select('test', 'x');
ROLLBACK;

SELECT * FROM pgv_list() order by package, name;

BEGIN;
SELECT pgv_insert('test', 'x', ROW (1::int, 2::int), TRUE);
SELECT pgv_select('test', 'x');
SELECT pgv_remove('test', 'x');
SELECT pgv_insert('test', 'x', ROW (1::int, 3::int), TRUE);
SELECT pgv_select('test', 'x');
SELECT pgv_insert('test', 'x', ROW (2::int, 4::int), TRUE);
SELECT pgv_select('test', 'x');
COMMIT;

SELECT * FROM pgv_list() order by package, name;
SELECT pgv_select('test', 'x');

-- Variables should be insertable after pgv_remove (package)
BEGIN;
SELECT pgv_insert('test', 'y', ROW (1::int, 2::int), TRUE);
SELECT pgv_select('test', 'y');
SELECT pgv_remove('test');
SELECT pgv_insert('test', 'y', ROW (1::int, 3::int), TRUE);
SELECT pgv_select('test', 'y');
SELECT pgv_insert('test', 'y', ROW (2::int, 4::int), TRUE);
SELECT pgv_select('test', 'y');
ROLLBACK;

SELECT * FROM pgv_list() order by package, name;

BEGIN;
SELECT pgv_insert('test', 'y', ROW (1::int, 2::int), TRUE);
SELECT pgv_select('test', 'y');
SELECT pgv_remove('test');
SELECT pgv_insert('test', 'y', ROW (1::int, 3::int), TRUE);
SELECT pgv_select('test', 'y');
SELECT pgv_insert('test', 'y', ROW (2::int, 4::int), TRUE);
SELECT pgv_select('test', 'y');
COMMIT;

SELECT * FROM pgv_list() order by package, name;
SELECT pgv_select('test', 'y');

-- Variables should be insertable after pgv_free
BEGIN;
SELECT pgv_insert('test', 'z', ROW (1::int, 2::int), TRUE);
SELECT pgv_select('test', 'z');
SELECT pgv_free();
SELECT pgv_insert('test', 'z', ROW (1::int, 3::int), TRUE);
SELECT pgv_select('test', 'z');
SELECT pgv_insert('test', 'z', ROW (2::int, 4::int), TRUE);
SELECT pgv_select('test', 'z');
ROLLBACK;

SELECT * FROM pgv_list() order by package, name;

BEGIN;
SELECT pgv_insert('test', 'z', ROW (1::int, 2::int), TRUE);
SELECT pgv_select('test', 'z');
SELECT pgv_free();
SELECT pgv_insert('test', 'z', ROW (1::int, 3::int), TRUE);
SELECT pgv_select('test', 'z');
SELECT pgv_insert('test', 'z', ROW (2::int, 4::int), TRUE);
SELECT pgv_select('test', 'z');
COMMIT;

SELECT * FROM pgv_list() order by package, name;
SELECT pgv_select('test', 'z');

SELECT pgv_free();

-- Variables should be rollbackable if transactional
SELECT pgv_insert('test', 'x', ROW (1::int, 2::int), TRUE);
SELECT pgv_insert('test', 'x', ROW (2::int, 3::int), TRUE);
SELECT pgv_select('test', 'x');

BEGIN;
SELECT pgv_remove('test', 'x');
ROLLBACK;

SELECT pgv_select('test', 'x');

BEGIN;
SELECT pgv_remove('test');
ROLLBACK;

SELECT pgv_select('test', 'x');

BEGIN;
SELECT pgv_free();
ROLLBACK;

SELECT pgv_select('test', 'x');

---
--- Variables should not be rollbackable if not transactional
---
-- case 1 (remove var)
SELECT pgv_insert('test', 'y', ROW (1::int, 2::int), FALSE);
SELECT pgv_insert('test', 'y', ROW (2::int, 3::int), FALSE);
SELECT pgv_select('test', 'y');

BEGIN;
SELECT pgv_remove('test', 'y');
ROLLBACK;

SELECT pgv_select('test', 'y');
-- case 2 (remove pack)
SELECT pgv_insert('test', 'y', ROW (1::int, 2::int), FALSE);
SELECT pgv_insert('test', 'y', ROW (2::int, 3::int), FALSE);
SELECT pgv_select('test', 'y');

BEGIN;
SELECT pgv_remove('test');
ROLLBACK;

SELECT pgv_select('test', 'y');
-- case 3 (free)
SELECT pgv_insert('test', 'y', ROW (1::int, 2::int), FALSE);
SELECT pgv_insert('test', 'y', ROW (2::int, 3::int), FALSE);
SELECT pgv_select('test', 'y');

BEGIN;
SELECT pgv_free();
ROLLBACK;

SELECT pgv_select('test', 'y');
-- clear all
SELECT pgv_free();

---
--- Cursors test #1 (remove var)
---
SELECT pgv_insert('test', 'x', ROW (1::int, 2::int), TRUE);
BEGIN;
DECLARE r1_cur CURSOR FOR SELECT pgv_select('test', 'x');
SELECT pgv_remove('test', 'x');
FETCH 1 in r1_cur;
ROLLBACK;
SELECT pgv_select('test', 'x');

SELECT pgv_insert('test', 'y', ROW (1::int, 2::int), FALSE);
BEGIN;
DECLARE r1_cur CURSOR FOR SELECT pgv_select('test', 'y');
SELECT pgv_remove('test', 'y');
FETCH 1 in r1_cur;
ROLLBACK;
SELECT pgv_select('test', 'y');

SELECT pgv_free();
---
--- Cursors test #2 (remove pack)
---
SELECT pgv_insert('test', 'x', ROW (1::int, 2::int), TRUE);
BEGIN;
DECLARE r1_cur CURSOR FOR SELECT pgv_select('test', 'x');
SELECT pgv_remove('test');
FETCH 1 in r1_cur;
ROLLBACK;
SELECT pgv_select('test', 'x');

SELECT pgv_insert('test', 'y', ROW (1::int, 2::int), FALSE);
BEGIN;
DECLARE r1_cur CURSOR FOR SELECT pgv_select('test', 'y');
SELECT pgv_remove('test');
FETCH 1 in r1_cur;
ROLLBACK;
SELECT pgv_select('test');

SELECT pgv_free();
---
--- Cursors test #3 (free)
---
SELECT pgv_insert('test', 'x', ROW (1::int, 2::int), TRUE);
BEGIN;
DECLARE r1_cur CURSOR FOR SELECT pgv_select('test', 'x');
SELECT pgv_free();
FETCH 1 in r1_cur;
ROLLBACK;
SELECT pgv_select('test', 'x');

SELECT pgv_insert('test', 'y', ROW (1::int, 2::int), FALSE);
BEGIN;
DECLARE r1_cur CURSOR FOR SELECT pgv_select('test', 'y');
SELECT pgv_free();
FETCH 1 in r1_cur;
ROLLBACK;
SELECT pgv_select('test');

SELECT pgv_free();
---
--- Cursor test #4
---

-- non transactional, remove var
SELECT pgv_insert('test', 'x', ROW (1::int, 2::int), FALSE);
BEGIN;
DECLARE r1_cur CURSOR FOR SELECT pgv_select('test', 'x');
SELECT pgv_remove('test', 'x');
FETCH 1 in r1_cur;
ROLLBACK;
SELECT pgv_select('test', 'x');

-- non transactional, remove pac
SELECT pgv_insert('test', 'x', ROW (1::int, 2::int), FALSE);
BEGIN;
DECLARE r1_cur CURSOR FOR SELECT pgv_select('test', 'x');
SELECT pgv_remove('test');
FETCH 1 in r1_cur;
ROLLBACK;
SELECT pgv_select('test', 'x');

-- non transactional, free
SELECT pgv_insert('test', 'x', ROW (1::int, 2::int), FALSE);
BEGIN;
DECLARE r1_cur CURSOR FOR SELECT pgv_select('test', 'x');
SELECT pgv_free();
FETCH 1 in r1_cur;
ROLLBACK;
SELECT pgv_select('test', 'x');

-- transactional, remove var
SELECT pgv_insert('test', 'y', ROW (1::int, 2::int), TRUE);
BEGIN;
DECLARE r1_cur CURSOR FOR SELECT pgv_select('test', 'y');
SELECT pgv_remove('test', 'y');
FETCH 1 in r1_cur;
ROLLBACK;
SELECT pgv_select('test', 'y');

-- transactional, remove pack
SELECT pgv_insert('test', 'y', ROW (1::int, 2::int), TRUE);
BEGIN;
DECLARE r1_cur CURSOR FOR SELECT pgv_select('test', 'y');
SELECT pgv_remove('test');
FETCH 1 in r1_cur;
ROLLBACK;
SELECT pgv_select('test', 'y');

-- transactional, free
SELECT pgv_insert('test', 'y', ROW (1::int, 2::int), TRUE);
BEGIN;
DECLARE r1_cur CURSOR FOR SELECT pgv_select('test', 'y');
SELECT pgv_free();
FETCH 1 in r1_cur;
ROLLBACK;
SELECT pgv_select('test', 'y');

SELECT pgv_free();
---
--- Cursor test #5
---

-- non transactional
SELECT pgv_insert('test', 'x', ROW (1::int, 2::int), FALSE);
BEGIN;
DECLARE r1_cur CURSOR FOR SELECT pgv_select('test', 'x');
FETCH 1 in r1_cur;
CLOSE r1_cur;
SELECT pgv_remove('test', 'x');
ROLLBACK;
SELECT pgv_select('test', 'x');

SELECT pgv_insert('test', 'x', ROW (1::int, 2::int), FALSE);
BEGIN;
DECLARE r1_cur CURSOR FOR SELECT pgv_select('test', 'x');
FETCH 1 in r1_cur;
CLOSE r1_cur;
SELECT pgv_remove('test');
ROLLBACK;
SELECT pgv_select('test', 'x');

SELECT pgv_insert('test', 'x', ROW (1::int, 2::int), FALSE);
BEGIN;
DECLARE r1_cur CURSOR FOR SELECT pgv_select('test', 'x');
FETCH 1 in r1_cur;
CLOSE r1_cur;
SELECT pgv_free();
ROLLBACK;
SELECT pgv_select('test', 'x');

-- transactional
SELECT pgv_insert('test', 'x', ROW (1::int, 2::int), TRUE);
BEGIN;
DECLARE r1_cur CURSOR FOR SELECT pgv_select('test', 'x');
FETCH 1 in r1_cur;
SELECT pgv_remove('test', 'x');
FETCH 1 in r1_cur;
ROLLBACK;
SELECT pgv_select('test', 'x');

BEGIN;
DECLARE r1_cur CURSOR FOR SELECT pgv_select('test', 'x');
FETCH 1 in r1_cur;
SELECT pgv_remove('test');
FETCH 1 in r1_cur;
ROLLBACK;
SELECT pgv_select('test', 'x');

BEGIN;
DECLARE r1_cur CURSOR FOR SELECT pgv_select('test', 'x');
FETCH 1 in r1_cur;
SELECT pgv_free();
FETCH 1 in r1_cur;
ROLLBACK;
SELECT pgv_select('test', 'x');

---
--- Cursor test #6
---
--SELECT pgv_insert('test', 'x', ROW (1::int, 2::int), TRUE);
--BEGIN;
--DECLARE r1_cur CURSOR FOR SELECT pgv_select('test', 'x');
--FETCH 1 in r1_cur;
--CLOSE r1_cur;
--SELECT pgv_remove('test', 'x');
--COMMIT;
SELECT pgv_free();
---
--- Tests for "leaked hash_seq_search scan for hash table"
---
SELECT pgv_insert('test', 'x', ROW (1::int, 2::int), FALSE);
SELECT pgv_insert('test', 'x', ROW (2::int, 3::int), FALSE);
SELECT pgv_insert('test', 'x', ROW (3::int, 4::int), FALSE);
SELECT pgv_select('test', 'x') LIMIT 1;
SELECT pgv_select('test', 'x') LIMIT 2;
SELECT pgv_select('test', 'x') LIMIT 3;
BEGIN;
SELECT pgv_select('test', 'x') LIMIT 1;
SELECT pgv_select('test', 'x') LIMIT 2;
SELECT pgv_select('test', 'x') LIMIT 3;
ROLLBACK;
BEGIN;
SELECT pgv_select('test', 'x') LIMIT 1;
SELECT pgv_select('test', 'x') LIMIT 2;
SELECT pgv_select('test', 'x') LIMIT 3;
COMMIT;

SELECT pgv_insert('test', 'y', ROW (1::int, 2::int), TRUE);
SELECT pgv_insert('test', 'y', ROW (2::int, 3::int), TRUE);
SELECT pgv_insert('test', 'y', ROW (3::int, 4::int), TRUE);
SELECT pgv_select('test', 'y') LIMIT 1;
SELECT pgv_select('test', 'y') LIMIT 2;
SELECT pgv_select('test', 'y') LIMIT 3;
BEGIN;
SELECT pgv_select('test', 'y') LIMIT 1;
SELECT pgv_select('test', 'y') LIMIT 2;
SELECT pgv_select('test', 'y') LIMIT 3;
ROLLBACK;
BEGIN;
SELECT pgv_select('test', 'y') LIMIT 1;
SELECT pgv_select('test', 'y') LIMIT 2;
SELECT pgv_select('test', 'y') LIMIT 3;
COMMIT;

BEGIN;
DECLARE r1_cur CURSOR FOR SELECT pgv_select('test', 'x');
DECLARE r2_cur CURSOR FOR SELECT pgv_select('test', 'y');
DECLARE r3_cur CURSOR FOR SELECT pgv_select('test', 'x');
FETCH 1 in r1_cur;
FETCH 1 in r2_cur;
FETCH 1 in r3_cur;
ROLLBACK;
BEGIN;
DECLARE r1_cur CURSOR FOR SELECT pgv_select('test', 'x');
DECLARE r2_cur CURSOR FOR SELECT pgv_select('test', 'y');
DECLARE r3_cur CURSOR FOR SELECT pgv_select('test', 'x');
FETCH 2 in r1_cur;
FETCH 2 in r2_cur;
FETCH 2 in r3_cur;
ROLLBACK;
BEGIN;
DECLARE r1_cur CURSOR FOR SELECT pgv_select('test', 'x');
DECLARE r2_cur CURSOR FOR SELECT pgv_select('test', 'y');
DECLARE r3_cur CURSOR FOR SELECT pgv_select('test', 'x');
FETCH 3 in r1_cur;
FETCH 3 in r2_cur;
FETCH 3 in r3_cur;
ROLLBACK;

BEGIN;
DECLARE r1_cur CURSOR FOR SELECT pgv_select('test', 'x');
DECLARE r2_cur CURSOR FOR SELECT pgv_select('test', 'y');
DECLARE r3_cur CURSOR FOR SELECT pgv_select('test', 'x');
FETCH 1 in r1_cur;
FETCH 1 in r2_cur;
FETCH 1 in r3_cur;
COMMIT;
BEGIN;
DECLARE r1_cur CURSOR FOR SELECT pgv_select('test', 'x');
DECLARE r2_cur CURSOR FOR SELECT pgv_select('test', 'y');
DECLARE r3_cur CURSOR FOR SELECT pgv_select('test', 'x');
FETCH 2 in r1_cur;
FETCH 2 in r2_cur;
FETCH 2 in r3_cur;
COMMIT;
BEGIN;
DECLARE r1_cur CURSOR FOR SELECT pgv_select('test', 'x');
DECLARE r2_cur CURSOR FOR SELECT pgv_select('test', 'y');
DECLARE r3_cur CURSOR FOR SELECT pgv_select('test', 'x');
FETCH 3 in r1_cur;
FETCH 3 in r2_cur;
FETCH 3 in r3_cur;
COMMIT;

BEGIN;
DECLARE r1_cur CURSOR FOR SELECT pgv_select('test', 'x');
DECLARE r2_cur CURSOR FOR SELECT pgv_select('test', 'y');
DECLARE r3_cur CURSOR FOR SELECT pgv_select('test', 'x');
FETCH 1 in r1_cur;
FETCH 2 in r2_cur;
FETCH 3 in r3_cur;
ROLLBACK;

BEGIN;
DECLARE r1_cur CURSOR FOR SELECT pgv_select('test', 'x');
DECLARE r2_cur CURSOR FOR SELECT pgv_select('test', 'y');
DECLARE r3_cur CURSOR FOR SELECT pgv_select('test', 'x');
FETCH 1 in r1_cur;
FETCH 2 in r2_cur;
FETCH 3 in r3_cur;
COMMIT;

---
--- Some special cases
---
-- take #1
SELECT pgv_insert('test', 'z1', ROW (2::int, 2::int), TRUE);
BEGIN;
DECLARE r1_cur CURSOR FOR SELECT pgv_select('test', 'z1');
FETCH 1 in r1_cur;
SELECT pgv_remove('test', 'z1');
FETCH 1 in r1_cur;
ROLLBACK;
SELECT pgv_select('test', 'z1');
-- take #2
SELECT pgv_insert('test', 'z2', ROW (2::int, 2::int), FALSE);
BEGIN;
DECLARE r1_cur CURSOR FOR SELECT pgv_select('test', 'z2');
FETCH 1 in r1_cur;
CLOSE r1_cur;
SELECT pgv_remove('test', 'z2');
FETCH 1 in r1_cur;
ROLLBACK;
SELECT pgv_select('test', 'z2');
SELECT pgv_insert('test', 'z2', ROW (1::int, 2::int), FALSE);
-- take #3
SELECT pgv_insert('test', 'z3', ROW (1::int, 2::int), TRUE);

BEGIN;
DECLARE r1_cur CURSOR FOR SELECT pgv_select('test', 'z3');
FETCH 1 in r1_cur;
CLOSE r1_cur;
SELECT pgv_remove('test', 'z3');
FETCH 1 in r1_cur;
ROLLBACK;
SELECT pgv_select('test', 'z3');

--BEGIN;
--DECLARE r1_cur CURSOR FOR SELECT pgv_select('test', 'z3');
--FETCH 1 in r1_cur;
--SELECT pgv_remove('test', 'z3');
--COMMIT;
--SELECT pgv_select('test', 'z3');

SELECT pgv_free();
-- take #4
SELECT pgv_insert('test', 'x', ROW (1::int, 2::int), TRUE);
SELECT pgv_insert('test', 'x', ROW (2::int, 3::int), TRUE);

BEGIN;
DECLARE r1_cur CURSOR FOR SELECT pgv_select('test', 'x');
SAVEPOINT sp1;
FETCH 1 in r1_cur;
ROLLBACK TO SAVEPOINT sp1;
COMMIT;

BEGIN;
DECLARE r1_cur CURSOR FOR SELECT pgv_select('test', 'x');
SAVEPOINT sp1;
FETCH 2 in r1_cur;
ROLLBACK TO SAVEPOINT sp1;
COMMIT;

BEGIN;
SAVEPOINT sp1;
SELECT pgv_select('test', 'x') LIMIT 1;
ROLLBACK TO SAVEPOINT sp1;
COMMIT;

---
--- Test cases for pgv_stats
---
--- Amount of allocated memory may vary from version to version, as well as from
--- platform to platform. Moreover, postgres versions less than 90600 always
--- show zero allocated memory. So, it's much easier to check if allocated
--- memory size is multiple of 8k since we use ALLOCSET_DEFAULT_INITSIZE
--- (see memutils.h), insted of creating multiple outputs files.
---
CREATE TEMP VIEW pgv_stats_view(pack, mem_mult) AS
    SELECT package, allocated_memory % 8192 as allocated_multiple_8192
      FROM pgv_stats()
     ORDER BY 1;

SELECT pgv_insert('test1', 'y', ROW (2::float, 1::float), TRUE);
SELECT pgv_insert('test1', 'x', ROW (2::float, 1::float), TRUE);
BEGIN;
DECLARE r1_cur CURSOR FOR SELECT pack, mem_mult FROM pgv_stats_view;
DECLARE r2_cur CURSOR FOR SELECT pack, mem_mult FROM pgv_stats_view;
FETCH 1 in r1_cur;
FETCH 1 in r2_cur;
COMMIT;

SELECT pgv_insert('test1', 'y', ROW (2::float, 1::float), TRUE);

BEGIN;
DECLARE r1_cur CURSOR FOR SELECT pack, mem_mult FROM pgv_stats_view;
DECLARE r2_cur CURSOR FOR SELECT pack, mem_mult FROM pgv_stats_view;
FETCH 1 in r1_cur;
FETCH 1 in r2_cur;
ROLLBACK;

SELECT pgv_insert('test1', 'y', ROW (2::float, 1::float), FALSE);

SELECT pgv_insert('test1', 'y', ROW (2::float, 1::float), FALSE);
SELECT pgv_insert('test1', 'x', ROW (2::float, 1::float), FALSE);
BEGIN;
DECLARE r1_cur CURSOR FOR SELECT pack, mem_mult FROM pgv_stats_view;
DECLARE r2_cur CURSOR FOR SELECT pack, mem_mult FROM pgv_stats_view;
FETCH 1 in r1_cur;
FETCH 1 in r2_cur;
COMMIT;

SELECT pgv_insert('test1', 'y', ROW (2::float, 1::float), FALSE);

BEGIN;
DECLARE r1_cur CURSOR FOR SELECT pack, mem_mult FROM pgv_stats_view;
DECLARE r2_cur CURSOR FOR SELECT pack, mem_mult FROM pgv_stats_view;
FETCH 1 in r1_cur;
FETCH 1 in r2_cur;
ROLLBACK;

SELECT pgv_insert('test1', 'y', ROW (2::float, 1::float), FALSE);

SELECT pgv_free();

---
--- Some special cases
---
-- 1
BEGIN;
SAVEPOINT comm2;
SELECT pgv_insert('test', 'x1', ROW (2::float, 1::float), TRUE);
DECLARE r1_cur CURSOR FOR SELECT pack, mem_mult FROM pgv_stats_view;
DECLARE r2_cur CURSOR FOR SELECT pack, mem_mult FROM pgv_stats_view;
FETCH 1 in r1_cur;
FETCH 1 in r2_cur;
COMMIT;

-- 2
BEGIN;
SELECT pgv_insert('test', 'x2', ROW (2::float, 1::float), TRUE);
SAVEPOINT comm2;
DECLARE r1_cur CURSOR FOR SELECT pack, mem_mult FROM pgv_stats_view;
DECLARE r2_cur CURSOR FOR SELECT pack, mem_mult FROM pgv_stats_view;
FETCH 1 in r1_cur;
FETCH 1 in r2_cur;
COMMIT;

-- 3
BEGIN;
SELECT pgv_insert('test', 'x3', ROW (2::float, 1::float), TRUE);
DECLARE r1_cur CURSOR FOR SELECT pack, mem_mult FROM pgv_stats_view;
DECLARE r2_cur CURSOR FOR SELECT pack, mem_mult FROM pgv_stats_view;
SAVEPOINT comm2;
FETCH 1 in r1_cur;
FETCH 1 in r2_cur;
COMMIT;

-- 4
BEGIN;
SELECT pgv_insert('test', 'x4', ROW (2::float, 1::float), TRUE);
DECLARE r1_cur CURSOR FOR SELECT pack, mem_mult FROM pgv_stats_view;
DECLARE r2_cur CURSOR FOR SELECT pack, mem_mult FROM pgv_stats_view;
FETCH 1 in r1_cur;
SAVEPOINT comm2;
FETCH 1 in r2_cur;
COMMIT;

-- 5
BEGIN;
SELECT pgv_insert('test', 'x5', ROW (2::float, 1::float), TRUE);
DECLARE r1_cur CURSOR FOR SELECT pack, mem_mult FROM pgv_stats_view;
DECLARE r2_cur CURSOR FOR SELECT pack, mem_mult FROM pgv_stats_view;
FETCH 1 in r1_cur;
FETCH 1 in r2_cur;
SAVEPOINT comm2;
COMMIT;

DROP VIEW pgv_stats_view;
SELECT pgv_free();

---
--- Test case for issue #32 [PGPRO-4456]
---
CREATE TEMP TABLE tab (id int, t varchar);
INSERT INTO tab VALUES (0, 'str00');

SELECT pgv_insert('vars', 'r1', row(1, 'str1', 'str2'));
SELECT pgv_insert('vars', 'a', tab) FROM tab;
SELECT pgv_insert('vars', 'r1', tab) FROM tab;
SELECT pgv_select('vars', 'r1');

SELECT pgv_insert('vars', 'r2', row(1, 'str1'::varchar));
SELECT pgv_insert('vars', 'b', tab) FROM tab;
SELECT pgv_insert('vars', 'r2', tab) FROM tab;
SELECT pgv_select('vars', 'r2');

SELECT pgv_free();


--
-- Test case for issue #38 [PGPRO-4676]
--
SELECT pgv_insert('test', 'x5', ROW ((2::int, 1::int)), TRUE);
