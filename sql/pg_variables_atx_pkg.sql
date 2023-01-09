--
-- PGPRO-7614: function pgv_free() inside autonomous transaction
--
SELECT pgv_free();
--
--
-- Functions pgv_free() + pgv_get() inside autonomous transaction; package
-- with regular variable; autonomous transaction with commit.
--
BEGIN;
	SELECT pgv_set('vars', 'int1', 1);
	BEGIN AUTONOMOUS;
		SELECT pgv_get('vars', 'int1', null::int);
		SELECT pgv_free();
-- ERROR:  unrecognized package "vars"
		SELECT pgv_get('vars', 'int1', null::int);
	COMMIT;
-- ERROR:  unrecognized package "vars"
	SELECT pgv_get('vars', 'int1', null::int);
ROLLBACK;
--
--
-- Function pgv_free() inside autonomous transaction; package with
-- regular variable; autonomous transaction with commit.
--
BEGIN;
	SELECT pgv_set('vars', 'int1', 1);
	BEGIN AUTONOMOUS;
		SELECT pgv_free();
	COMMIT;
-- ERROR:  unrecognized package "vars"
	SELECT pgv_get('vars', 'int1', null::int);
ROLLBACK;
--
--
-- Function pgv_free() inside autonomous transaction; package with
-- regular variable; autonomous transaction with rollback.
--
BEGIN;
	SELECT pgv_set('vars', 'int1', 1);
	BEGIN AUTONOMOUS;
		SELECT pgv_free();
	ROLLBACK;
-- ERROR:  unrecognized package "vars"
	SELECT pgv_get('vars', 'int1', null::int);
ROLLBACK;
--
--
-- Function pgv_free() inside autonomous transaction; package with
-- transactional variable; autonomous transaction with rollback.
--
BEGIN;
	SELECT pgv_set('vars', 'int1', 1, true);
	BEGIN AUTONOMOUS;
		SELECT pgv_free();
	ROLLBACK;
	SELECT pgv_get('vars', 'int1', null::int);
ROLLBACK;
--
--
-- Function pgv_free() inside autonomous transaction; package with
-- transactional variable; autonomous transaction with commit.
--
BEGIN;
	SELECT pgv_set('vars', 'int1', 1, true);
	BEGIN AUTONOMOUS;
		SELECT pgv_free();
	COMMIT;
-- ERROR:  unrecognized package "vars"
	SELECT pgv_get('vars', 'int1', null::int);
ROLLBACK;
--
--
-- Function pgv_free() inside recursive autonomous transactions.
--
BEGIN;
	BEGIN AUTONOMOUS;
		SELECT pgv_set('vars', 'int1', 1);
		BEGIN AUTONOMOUS;
			BEGIN AUTONOMOUS;
				SELECT pgv_free();
			COMMIT;
-- ERROR:  unrecognized package "vars"
			SELECT pgv_get('vars', 'int1', null::int);
		COMMIT;
-- ERROR:  unrecognized package "vars"
		SELECT pgv_get('vars', 'int1', null::int);
	COMMIT;
ROLLBACK;
--
--
-- Function pgv_free() inside recursive autonomous transactions;
-- recreating the package after deletion with using regular
-- variable.
--
BEGIN;
	SELECT pgv_set('vars', 'int1', 1);
	BEGIN AUTONOMOUS;
		BEGIN AUTONOMOUS;
			SELECT pgv_get('vars', 'int1', null::int);
			BEGIN AUTONOMOUS;
				SELECT pgv_free();
			COMMIT;
-- ERROR:  unrecognized package "vars"
			SELECT pgv_get('vars', 'int1', null::int);
		COMMIT;
		SELECT pgv_set('vars', 'int1', 2);
	COMMIT;
	SELECT pgv_get('vars', 'int1', null::int);
ROLLBACK;
--
--
-- Function pgv_free() inside recursive autonomous transactions;
-- recreating the package after deletion with using transactional
-- variable.
--
BEGIN;
	SELECT pgv_set('vars', 'int1', 1);
	BEGIN AUTONOMOUS;
		BEGIN AUTONOMOUS;
			SELECT pgv_get('vars', 'int1', null::int);
			BEGIN AUTONOMOUS;
				SELECT pgv_free();
			COMMIT;
-- ERROR:  unrecognized package "vars"
			SELECT pgv_get('vars', 'int1', null::int);
		COMMIT;
		SELECT pgv_set('vars', 'int1', 2, true);
		SELECT pgv_list();
	COMMIT;
-- ERROR:  unrecognized package "vars"
	SELECT pgv_get('vars', 'int1', null::int);
ROLLBACK;
--
--
-- Test for case: do not free hash_seq_search scans of parent transaction
-- at end of the autonomous transaction.
--
BEGIN;
	SELECT pgv_insert('test', 'x', row (1::int, 2::int), false);
	SELECT pgv_insert('test', 'x', row (3::int, 4::int), false);
	DECLARE r1_cur CURSOR FOR SELECT pgv_select('test', 'x');
-- (1,2)
	FETCH 1 IN r1_cur;
	BEGIN AUTONOMOUS;
	ROLLBACK;
-- (3,4)
	FETCH 1 IN r1_cur;
	SELECT pgv_remove('test', 'x');
-- ERROR:  unrecognized package "test"
	FETCH 1 IN r1_cur;
ROLLBACK;
--
--
-- Test for case: pgv_free() should free hash_seq_search scans of all
-- (current ATX + parent) transactions.
--
BEGIN;
	SELECT pgv_insert('test', 'x', row (1::int, 2::int), false);
	DECLARE r1_cur CURSOR FOR SELECT pgv_select('test', 'x');
-- (1,2)
	FETCH 1 IN r1_cur;
	BEGIN AUTONOMOUS;
		SELECT pgv_free();
	ROLLBACK;
-- ERROR:  unrecognized package "test"
	FETCH 1 IN r1_cur;
ROLLBACK;

SELECT pgv_free();
