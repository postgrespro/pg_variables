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

BEGIN;
SAVEPOINT sp_to_rollback;
SELECT pgv_set('vars', 'any1', 'text value'::text, true);
SELECT pgv_set('vars', 'any2', 'text value'::text);
SELECT pgv_set_int('vars', 'int1', 401, true);
SELECT pgv_set_int('vars', 'int2', 402);
SELECT pgv_set_text('vars', 'str1', 's401', true);
SELECT pgv_set_text('vars', 'str2', 's402');
SELECT pgv_set_numeric('vars', 'num1', 4.01, true);
SELECT pgv_set_numeric('vars', 'num2', 4.02);
SELECT pgv_set_timestamp('vars', 'ts1', '2016-04-30 20:00:00', true);
SELECT pgv_set_timestamp('vars', 'ts2', '2016-04-30 21:00:00');
SELECT pgv_set_timestamptz('vars', 'tstz1', '2016-04-30 20:00:00 GMT+01', true);
SELECT pgv_set_timestamptz('vars', 'tstz2', '2016-04-30 21:00:00 GMT+02');
SELECT pgv_set_date('vars', 'd1', '2016-04-29', true);
SELECT pgv_set_date('vars', 'd2', '2016-04-30');
SELECT pgv_set_jsonb('vars2', 'j1', '[1, 2, "foo4", null]', true);
SELECT pgv_set_jsonb('vars2', 'j2', '{"bar": "baz4", "balance": 4.44, "active": false}');
SELECT pgv_insert('vars3', 'r1', row(6 :: integer, 'str44' :: varchar), true);
SELECT pgv_insert('vars3', 'r2', row(6 :: integer, 'str44' :: varchar));
ROLLBACK TO sp_to_rollback;
COMMIT;
SELECT pgv_get('vars', 'any1',NULL::text);
SELECT pgv_get('vars', 'any2',NULL::text);
SELECT pgv_get_int('vars', 'int1');
SELECT pgv_get_int('vars', 'int2');
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
SELECT pgv_select('vars3', 'r1');
SELECT pgv_select('vars3', 'r2');

SELECT pgv_free();


-- CHECK ROLLBACK AFTER COMMITING SUBTRANSACTION
BEGIN;
SELECT pgv_set('vars', 'any1', 'before savepoint sp1'::text, true);
SELECT pgv_set_int('vars', 'int1', 100, true);
SELECT pgv_set_text('vars', 'str1', 's100', true);
SELECT pgv_set_numeric('vars', 'num1', 1.00, true);
SELECT pgv_set_timestamp('vars', 'ts1', '2016-01-01 10:00:00', true);
SELECT pgv_set_timestamptz('vars', 'tstz1', '2016-01-01 10:00:00 GMT+01', true);
SELECT pgv_set_date('vars', 'd1', '2016-01-01', true);
SELECT pgv_set_jsonb('vars', 'j1', '[1, 0, "foo", null]', true);

SAVEPOINT sp1;
SELECT pgv_set('vars', 'any1', 'after savepoint sp1'::text, true);
SELECT pgv_set_int('vars', 'int1', 101, true);
SELECT pgv_set_text('vars', 'str1', 's101', true);
SELECT pgv_set_numeric('vars', 'num1', 1.01, true);
SELECT pgv_set_timestamp('vars', 'ts1', '2016-01-01 11:00:00', true);
SELECT pgv_set_timestamptz('vars', 'tstz1', '2016-01-01 11:00:00 GMT+01', true);
SELECT pgv_set_date('vars', 'd1', '2016-01-11', true);
SELECT pgv_set_jsonb('vars', 'j1', '[1, 1, "foo", null]', true);

SAVEPOINT sp2;
SELECT pgv_set('vars', 'any1', 'after savepoint sp2'::text, true);
SELECT pgv_set_int('vars', 'int1', 102, true);
SELECT pgv_set_text('vars', 'str1', 's102', true);
SELECT pgv_set_numeric('vars', 'num1', 1.02, true);
SELECT pgv_set_timestamp('vars', 'ts1', '2016-01-01 12:00:00', true);
SELECT pgv_set_timestamptz('vars', 'tstz1', '2016-01-01 12:00:00 GMT+01', true);
SELECT pgv_set_date('vars', 'd1', '2016-01-21', true);
SELECT pgv_set_jsonb('vars', 'j1', '[1, 2, "foo", null]', true);

RELEASE sp2;
SELECT pgv_get('vars', 'any1',NULL::text);
SELECT pgv_get_int('vars', 'int1');
SELECT pgv_get_text('vars', 'str1');
SELECT pgv_get_numeric('vars', 'num1');
SELECT pgv_get_timestamp('vars', 'ts1');
SELECT pgv_get_timestamptz('vars', 'tstz1');
SELECT pgv_get_date('vars', 'd1');
SELECT pgv_get_jsonb('vars', 'j1');

ROLLBACK TO sp1;
SELECT pgv_get('vars', 'any1',NULL::text);
SELECT pgv_get_int('vars', 'int1');
SELECT pgv_get_text('vars', 'str1');
SELECT pgv_get_numeric('vars', 'num1');
SELECT pgv_get_timestamp('vars', 'ts1');
SELECT pgv_get_timestamptz('vars', 'tstz1');
SELECT pgv_get_date('vars', 'd1');
SELECT pgv_get_jsonb('vars', 'j1');

COMMIT;


BEGIN;
SAVEPOINT sp1;
SAVEPOINT sp2;
SELECT pgv_set('vars2', 'any1', 'variable exists'::text, true);
SELECT pgv_set_int('vars2', 'int1', 102, true);
SELECT pgv_set_text('vars2', 'str1', 's102', true);
SELECT pgv_set_numeric('vars2', 'num1', 1.02, true);
SELECT pgv_set_timestamp('vars2', 'ts1', '2016-01-01 12:00:00', true);
SELECT pgv_set_timestamptz('vars2', 'tstz1', '2016-01-01 12:00:00 GMT+01', true);
SELECT pgv_set_date('vars2', 'd1', '2016-01-21', true);
SELECT pgv_set_jsonb('vars2', 'j1', '[1, 2, "foo", null]', true);

RELEASE sp2;
SELECT pgv_get('vars2', 'any1',NULL::text);
SELECT pgv_get_int('vars2', 'int1');
SELECT pgv_get_text('vars2', 'str1');
SELECT pgv_get_numeric('vars2', 'num1');
SELECT pgv_get_timestamp('vars2', 'ts1');
SELECT pgv_get_timestamptz('vars2', 'tstz1');
SELECT pgv_get_date('vars2', 'd1');
SELECT pgv_get_jsonb('vars2', 'j1');

ROLLBACK TO sp1;
COMMIT;
SELECT pgv_get('vars2', 'any1',NULL::text);
SELECT pgv_get_int('vars2', 'int1');
SELECT pgv_get_text('vars2', 'str1');
SELECT pgv_get_numeric('vars2', 'num1');
SELECT pgv_get_timestamp('vars2', 'ts1');
SELECT pgv_get_timestamptz('vars2', 'tstz1');
SELECT pgv_get_date('vars2', 'd1');
SELECT pgv_get_jsonb('vars2', 'j1');

SELECT pgv_free();

--CHECK TRANSACTION COMMIT
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

BEGIN;
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

-- Check values before committing transaction
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

-- Check values after committing transaction
COMMIT;
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

-- Check values before rollback
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

-- Check values after rollback
ROLLBACK;
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
SELECT pgv_set_int('vars', 'int1', 401, true);
SELECT pgv_set_int('vars', 'int2', 402);
SELECT pgv_set_text('vars', 'str1', 's401', true);
SELECT pgv_set_text('vars', 'str2', 's402');
SELECT pgv_set_numeric('vars', 'num1', 4.01, true);
SELECT pgv_set_numeric('vars', 'num2', 4.02);
SELECT pgv_set_timestamp('vars', 'ts1', '2016-04-30 20:00:00', true);
SELECT pgv_set_timestamp('vars', 'ts2', '2016-04-30 21:00:00');
SELECT pgv_set_timestamptz('vars', 'tstz1', '2016-04-30 20:00:00 GMT+01', true);
SELECT pgv_set_timestamptz('vars', 'tstz2', '2016-04-30 21:00:00 GMT+02');
SELECT pgv_set_date('vars', 'd1', '2016-04-29', true);
SELECT pgv_set_date('vars', 'd2', '2016-04-30');
SELECT pgv_set_jsonb('vars2', 'j1', '[1, 2, "foo4", null]', true);
SELECT pgv_set_jsonb('vars2', 'j2', '{"bar": "baz4", "balance": 4.44, "active": false}');
SELECT pgv_insert('vars3', 'r1', row(6 :: integer, 'str44' :: varchar), true);
SELECT pgv_insert('vars3', 'r2', row(6 :: integer, 'str44' :: varchar));
ROLLBACK;
SELECT pgv_get('vars', 'any1',NULL::text);
SELECT pgv_get('vars', 'any2',NULL::text);
SELECT pgv_get_int('vars', 'int1');
SELECT pgv_get_int('vars', 'int2');
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
SELECT pgv_select('vars3', 'r1');
SELECT pgv_select('vars3', 'r2');

SELECT pgv_free();


-- CHECK ROLLBACK AFTER COMMITING SUBTRANSACTION
SELECT pgv_set('vars', 'any1', 'before transaction block'::text, true);
SELECT pgv_set_int('vars', 'int1', 100, true);
SELECT pgv_set_text('vars', 'str1', 's100', true);
SELECT pgv_set_numeric('vars', 'num1', 1.00, true);
SELECT pgv_set_timestamp('vars', 'ts1', '2016-01-01 10:00:00', true);
SELECT pgv_set_timestamptz('vars', 'tstz1', '2016-01-01 10:00:00 GMT+01', true);
SELECT pgv_set_date('vars', 'd1', '2016-01-01', true);
SELECT pgv_set_jsonb('vars', 'j1', '[1, 0, "foo", null]', true);

BEGIN;
SELECT pgv_set('vars', 'any1', 'before savepoint sp1'::text, true);
SELECT pgv_set_int('vars', 'int1', 100, true);
SELECT pgv_set_text('vars', 'str1', 's100', true);
SELECT pgv_set_numeric('vars', 'num1', 1.00, true);
SELECT pgv_set_timestamp('vars', 'ts1', '2016-01-01 10:00:00', true);
SELECT pgv_set_timestamptz('vars', 'tstz1', '2016-01-01 10:00:00 GMT+01', true);
SELECT pgv_set_date('vars', 'd1', '2016-01-01', true);
SELECT pgv_set_jsonb('vars', 'j1', '[1, 0, "foo", null]', true);

SAVEPOINT sp1;
SELECT pgv_set('vars', 'any1', 'after savepoint sp1'::text, true);
SELECT pgv_set_int('vars', 'int1', 101, true);
SELECT pgv_set_text('vars', 'str1', 's101', true);
SELECT pgv_set_numeric('vars', 'num1', 1.01, true);
SELECT pgv_set_timestamp('vars', 'ts1', '2016-01-01 11:00:00', true);
SELECT pgv_set_timestamptz('vars', 'tstz1', '2016-01-01 11:00:00 GMT+01', true);
SELECT pgv_set_date('vars', 'd1', '2016-01-11', true);
SELECT pgv_set_jsonb('vars', 'j1', '[1, 1, "foo", null]', true);

SAVEPOINT sp2;
SELECT pgv_set('vars', 'any1', 'after savepoint sp2'::text, true);
SELECT pgv_set_int('vars', 'int1', 102, true);
SELECT pgv_set_text('vars', 'str1', 's102', true);
SELECT pgv_set_numeric('vars', 'num1', 1.02, true);
SELECT pgv_set_timestamp('vars', 'ts1', '2016-01-01 12:00:00', true);
SELECT pgv_set_timestamptz('vars', 'tstz1', '2016-01-01 12:00:00 GMT+01', true);
SELECT pgv_set_date('vars', 'd1', '2016-01-21', true);
SELECT pgv_set_jsonb('vars', 'j1', '[1, 2, "foo", null]', true);

RELEASE sp2;
SELECT pgv_get('vars', 'any1',NULL::text);
SELECT pgv_get_int('vars', 'int1');
SELECT pgv_get_text('vars', 'str1');
SELECT pgv_get_numeric('vars', 'num1');
SELECT pgv_get_timestamp('vars', 'ts1');
SELECT pgv_get_timestamptz('vars', 'tstz1');
SELECT pgv_get_date('vars', 'd1');
SELECT pgv_get_jsonb('vars', 'j1');

ROLLBACK TO sp1;
SELECT pgv_get('vars', 'any1',NULL::text);
SELECT pgv_get_int('vars', 'int1');
SELECT pgv_get_text('vars', 'str1');
SELECT pgv_get_numeric('vars', 'num1');
SELECT pgv_get_timestamp('vars', 'ts1');
SELECT pgv_get_timestamptz('vars', 'tstz1');
SELECT pgv_get_date('vars', 'd1');
SELECT pgv_get_jsonb('vars', 'j1');

ROLLBACK;
SELECT pgv_get('vars', 'any1',NULL::text);
SELECT pgv_get_int('vars', 'int1');
SELECT pgv_get_text('vars', 'str1');
SELECT pgv_get_numeric('vars', 'num1');
SELECT pgv_get_timestamp('vars', 'ts1');
SELECT pgv_get_timestamptz('vars', 'tstz1');
SELECT pgv_get_date('vars', 'd1');
SELECT pgv_get_jsonb('vars', 'j1');


BEGIN;
SAVEPOINT sp1;
SELECT pgv_set('vars2', 'any1', 'variable exists'::text, true);
SELECT pgv_set_int('vars2', 'int1', 102, true);
SELECT pgv_set_text('vars2', 'str1', 's102', true);
SELECT pgv_set_numeric('vars2', 'num1', 1.02, true);
SELECT pgv_set_timestamp('vars2', 'ts1', '2016-01-01 12:00:00', true);
SELECT pgv_set_timestamptz('vars2', 'tstz1', '2016-01-01 12:00:00 GMT+01', true);
SELECT pgv_set_date('vars2', 'd1', '2016-01-21', true);
SELECT pgv_set_jsonb('vars2', 'j1', '[1, 2, "foo", null]', true);

RELEASE sp1;
SELECT pgv_get('vars2', 'any1',NULL::text);
SELECT pgv_get_int('vars2', 'int1');
SELECT pgv_get_text('vars2', 'str1');
SELECT pgv_get_numeric('vars2', 'num1');
SELECT pgv_get_timestamp('vars2', 'ts1');
SELECT pgv_get_timestamptz('vars2', 'tstz1');
SELECT pgv_get_date('vars2', 'd1');
SELECT pgv_get_jsonb('vars2', 'j1');

ROLLBACK;
SELECT pgv_get('vars2', 'any1',NULL::text);
SELECT pgv_get_int('vars2', 'int1');
SELECT pgv_get_text('vars2', 'str1');
SELECT pgv_get_numeric('vars2', 'num1');
SELECT pgv_get_timestamp('vars2', 'ts1');
SELECT pgv_get_timestamptz('vars2', 'tstz1');
SELECT pgv_get_date('vars2', 'd1');
SELECT pgv_get_jsonb('vars2', 'j1');

SELECT pgv_free();