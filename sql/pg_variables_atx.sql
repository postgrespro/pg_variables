select pgv_free();

------------------------------
-- Non-transactional variables
------------------------------
select pgv_set('vars', 'int1', 101);
begin;
	select pgv_set('vars', 'int2', 102);
	begin autonomous;
		select pgv_set('vars', 'int3', 103);
-- 101, 102, 103:
		select pgv_get('vars', 'int1', null::int), pgv_get('vars', 'int2', null::int), pgv_get('vars', 'int3', null::int);
		select pgv_set('vars', 'int1', 1001);
		begin autonomous;
-- 1001, 102, 103:
			select pgv_get('vars', 'int1', null::int), pgv_get('vars', 'int2', null::int), pgv_get('vars', 'int3', null::int);
			select pgv_set('vars', 'int2', 1002);
		commit;
	commit;
-- 1001, 1002, 103:
	select pgv_get('vars', 'int1', null::int), pgv_get('vars', 'int2', null::int), pgv_get('vars', 'int3', null::int);
	select pgv_set('vars', 'int3', 1003);
rollback;

-- 1001, 1002, 1003:
select pgv_get('vars', 'int1', null::int), pgv_get('vars', 'int2', null::int), pgv_get('vars', 'int3', null::int);
-- vars:int1, vars:int2, vars:int3:
select * from pgv_list() order by package, name;

select pgv_free();

--------------------------
-- Transactional variables
--------------------------
select pgv_set('vars', 'int1', 101, true);
begin;
	select pgv_set('vars', 'int2', 102, true);
	begin autonomous;
		select pgv_set('vars', 'int3', 103, true);
-- 103:
		select pgv_get('vars', 'int3', null::int);
		begin autonomous;
			select pgv_set('vars', 'int2', 1002, true);
-- 1002:
			select pgv_get('vars', 'int2', null::int);
		commit;
-- 103:
		select pgv_get('vars', 'int3', null::int);
	commit;
	select pgv_set('vars', 'int1', 1001, true);
-- 1001:
	select pgv_get('vars', 'int1', null::int);
-- 102:
	select pgv_get('vars', 'int2', null::int);
rollback;
-- 101:
select pgv_get('vars', 'int1', null::int);
-- vars:int1:
select * from pgv_list() order by package, name;

select pgv_free();

----------
-- Cursors
----------
select pgv_insert('test', 'x', row (1::int, 2::int), false);
select pgv_insert('test', 'x', row (2::int, 3::int), false);
select pgv_insert('test', 'x', row (3::int, 4::int), false);

select pgv_insert('test', 'y', row (10::int, 20::int), true);
select pgv_insert('test', 'y', row (20::int, 30::int), true);
select pgv_insert('test', 'y', row (30::int, 40::int), true);

begin;
	declare r1_cur cursor for select pgv_select('test', 'x');
	begin autonomous;
		begin autonomous;
			begin autonomous;
				begin autonomous;
					begin autonomous;
						select pgv_insert('test', 'z', row (11::int, 22::int), false);
						select pgv_insert('test', 'z', row (22::int, 33::int), false);
						select pgv_insert('test', 'z', row (33::int, 44::int), false);

						declare r11_cur cursor for select pgv_select('test', 'x');
-- (1,2),(2,3):
						fetch 2 in r11_cur;
						declare r2_cur cursor for select pgv_select('test', 'y');
-- correct error: unrecognized variable "y"
						fetch 2 in r2_cur;
					rollback;
				rollback;
			rollback;
		rollback;
	rollback;
	declare r2_cur cursor for select pgv_select('test', 'y');
	declare r3_cur cursor for select pgv_select('test', 'z');
-- (1,2),(2,3):
	fetch 2 in r1_cur;
-- (10,20),(20,30):
	fetch 2 in r2_cur;
-- (11,22),(22,33):
	fetch 2 in r3_cur;
rollback;

select pgv_free();

------------------------------------------
-- Savepoint: rollback in main transaction
------------------------------------------
begin;
	select pgv_set('vars', 'trans_int', 101, true);
-- 101:
	select pgv_get('vars', 'trans_int', null::int);
	savepoint sp1;
	select pgv_set('vars', 'trans_int', 102, true);
-- 102:
	select pgv_get('vars', 'trans_int', null::int);
	begin autonomous;
		select pgv_set('vars', 'trans_int', 103, true);
-- 103:
		select pgv_get('vars', 'trans_int', null::int);
	commit;
-- 102:
	select pgv_get('vars', 'trans_int', null::int);
	rollback to sp1;
commit;
-- 101:
select pgv_get('vars', 'trans_int', null::int);

select pgv_free();

------------------------------------------------
-- Savepoint: rollback in autonomous transaction
------------------------------------------------
begin;
	select pgv_set('vars', 'trans_int', 1, true);
	savepoint sp1;
	select pgv_set('vars', 'trans_int', 100, true);
	begin autonomous;
		begin autonomous;
			select pgv_set('vars1', 'int1', 2);
			select pgv_set('vars1', 'trans_int1', 3, true);
			savepoint sp2;
			select pgv_set('vars1', 'trans_int1', 4, true);
-- 2
			select pgv_get('vars1', 'int1', null::int);
-- 4
			select pgv_get('vars1', 'trans_int1', null::int);
			rollback to sp2;
-- 3
			select pgv_get('vars1', 'trans_int1', null::int);
-- vars1:int1, vars1:trans_int1:
			select * from pgv_list() order by package, name;
			select pgv_set('vars1', 'trans_int2', 4, true);
			select pgv_set('vars1', 'trans_int3', 5, true);
			select pgv_set('vars1', 'int2', 3);
		rollback;
	commit;
	rollback to sp1;
-- 1
	select pgv_get('vars', 'trans_int', null::int);
-- 2
	select pgv_get('vars1', 'int1', null::int);
-- 3
	select pgv_get('vars1', 'int2', null::int);
-- vars:trans_int, vars1:int1, vars1:int2:
	select * from pgv_list() order by package, name;
commit;

select pgv_free();

------------------------------------------------------------
-- Sample with (subxact inside ATX) == (subxact outside ATX)
------------------------------------------------------------
select pgv_set('vars1', 'int1', 0);
select pgv_set('vars1', 'trans_int1', 0, true);
begin;
	begin autonomous;
		select pgv_set('vars1', 'int1', 1);
		select pgv_set('vars1', 'trans_int1', 2, true);
		savepoint sp2;
		select pgv_set('vars1', 'trans_int1', 3, true);
		rollback to sp2;
-- 2
		select pgv_get('vars1', 'trans_int1', null::int);
	commit;
rollback;
-- vars1:int1, vars1:trans_int1
select * from pgv_list() order by package, name;
-- 1
select pgv_get('vars1', 'int1', null::int);
-- 0
select pgv_get('vars1', 'trans_int1', null::int);

select pgv_free();
