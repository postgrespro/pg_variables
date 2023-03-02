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
--
--
-- Test for case: pgv_set() created a regular variable; rollback
-- removes package state and creates a new state to make package valid.
-- Commit of next autonomous transaction should not replace this new
-- state (this is not allowed for autonomous transaction).
--
BEGIN;
	BEGIN AUTONOMOUS;
		SELECT pgv_set('vars', 'int1', 1);
	ROLLBACK;
	BEGIN AUTONOMOUS;
		SELECT pgv_set('vars', 'int1', 2);
	COMMIT;
ROLLBACK;
SELECT pgv_remove('vars', 'int1');
--
--
-- Test for case: pgv_set() created a regular variable and package with
-- (atxlevel=1, level=1). COMMIT changes this level to (atxlevel=1, level=0).
-- In the next autonomous transaction (atxlevel=1, level=1) we erroneously
-- detect that the package changed in upper transaction and remove the
-- package state (this is not allowed for autonomous transaction).
--
BEGIN;
	BEGIN AUTONOMOUS;
		SELECT pgv_set('vars', 'int1', 2);
	COMMIT;
	BEGIN AUTONOMOUS;
		SELECT pgv_free();
		SELECT pgv_set('vars', 'int1', 2, true);
	COMMIT;
ROLLBACK;
--
--
-- Test for case: pgv_set() created a regular variable and package with
-- (atxlevel=1, level=1). ROLLBACK changes this level to (atxlevel=0, level=0).
-- But ROLLBACK shouldn't change atxlevel in case rollback of sub-transaction.
--
BEGIN;
	BEGIN AUTONOMOUS;
		SAVEPOINT sp1;
		SELECT pgv_set('vars1', 'int1', 0);
		ROLLBACK TO sp1;
	COMMIT;
ROLLBACK;
SELECT pgv_remove('vars1', 'int1');

SELECT pgv_free();
--
--
-- PGPRO-7856
-- Test for case: we don't remove the package object without any variables at
-- the end of autonomous transaction but need to move the state of this object
-- to upper level.
--
BEGIN;
	BEGIN AUTONOMOUS;
		SAVEPOINT sp1;
		SELECT pgv_set('vars2', 'any1', 'variable exists'::text, true);
		SELECT pgv_free();
		RELEASE sp1;
	ROLLBACK;

	BEGIN AUTONOMOUS;
		SAVEPOINT sp2;
		SAVEPOINT sp3;
		SELECT pgv_free();
	COMMIT;
ROLLBACK;
