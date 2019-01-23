-- Integer variables
SELECT pgv_get('vars', 'int1', NULL::int);
SELECT pgv_get('vars', 'int1', NULL::int, false);

SELECT pgv_set('vars', 'int1', 101);
SELECT pgv_set('vars', 'int2', 102);

SELECT pgv_get('vars', 'int1', NULL::int);
SELECT pgv_get('vars', 'int2', NULL::int);
SELECT pgv_set('vars', 'int1', 103);
SELECT pgv_get('vars', 'int1', NULL::int);

SELECT pgv_get('vars', 'int3', NULL::int);
SELECT pgv_get('vars', 'int3', NULL::int, false);
SELECT pgv_exists('vars', 'int3');
SELECT pgv_exists('vars', 'int1');
SELECT pgv_exists('vars2');
SELECT pgv_exists('vars');

SELECT pgv_set('vars', 'intNULL', NULL::int);
SELECT pgv_get('vars', 'intNULL', NULL::int);

-- Text variables
SELECT pgv_set('vars', 'str1', 's101'::text);
SELECT pgv_set('vars', 'int1', 's101'::text);
SELECT pgv_set('vars', 'str1', 101);
SELECT pgv_set('vars', 'str2', 's102'::text);

SELECT pgv_get('vars', 'str1', NULL::text);
SELECT pgv_get('vars', 'str2', NULL::text);
SELECT pgv_set('vars', 'str1', 's103'::text);
SELECT pgv_get('vars', 'str1', NULL::text);

SELECT pgv_get('vars', 'str3', NULL::text);
SELECT pgv_get('vars', 'str3', NULL::text, false);
SELECT pgv_exists('vars', 'str3');
SELECT pgv_exists('vars', 'str1');
SELECT pgv_get('vars', 'int1', NULL::text);
SELECT pgv_get('vars', 'str1', NULL::int);

SELECT pgv_set('vars', 'strNULL', NULL::text);
SELECT pgv_get('vars', 'strNULL', NULL::text);

-- Numeric variables
SELECT pgv_set('vars', 'num1', 1.01::numeric);
SELECT pgv_set('vars', 'num2', 1.02::numeric);
SELECT pgv_set('vars', 'str1', 1.01::numeric);

SELECT pgv_get('vars', 'num1', NULL::numeric);
SELECT pgv_get('vars', 'num2', NULL::numeric);
SELECT pgv_set('vars', 'num1', 1.03::numeric);
SELECT pgv_get('vars', 'num1', NULL::numeric);

SELECT pgv_get('vars', 'num3', NULL::numeric);
SELECT pgv_get('vars', 'num3', NULL::numeric, false);
SELECT pgv_exists('vars', 'num3');
SELECT pgv_exists('vars', 'num1');
SELECT pgv_get('vars', 'str1', NULL::numeric);

SELECT pgv_set('vars', 'numNULL', NULL::numeric);
SELECT pgv_get('vars', 'numNULL', NULL::numeric);

SET timezone = 'Europe/Moscow';

-- Timestamp variables
SELECT pgv_set('vars', 'ts1', '2016-03-30 10:00:00'::timestamp);
SELECT pgv_set('vars', 'ts2', '2016-03-30 11:00:00'::timestamp);
SELECT pgv_set('vars', 'num1', '2016-03-30 12:00:00'::timestamp);

SELECT pgv_get('vars', 'ts1', NULL::timestamp);
SELECT pgv_get('vars', 'ts2', NULL::timestamp);
SELECT pgv_set('vars', 'ts1', '2016-03-30 12:00:00'::timestamp);
SELECT pgv_get('vars', 'ts1', NULL::timestamp);

SELECT pgv_get('vars', 'ts3', NULL::timestamp);
SELECT pgv_get('vars', 'ts3', NULL::timestamp, false);
SELECT pgv_exists('vars', 'ts3');
SELECT pgv_exists('vars', 'ts1');
SELECT pgv_get('vars', 'num1', NULL::timestamp);

SELECT pgv_set('vars', 'tsNULL', NULL::timestamp);
SELECT pgv_get('vars', 'tsNULL', NULL::timestamp);

-- TimestampTZ variables

SELECT pgv_set('vars', 'tstz1', '2016-03-30 10:00:00 GMT+01'::timestamptz);
SELECT pgv_set('vars', 'tstz2', '2016-03-30 11:00:00 GMT+02'::timestamptz);
SELECT pgv_set('vars', 'ts1', '2016-03-30 12:00:00 GMT+03'::timestamptz);

SELECT pgv_get('vars', 'tstz1', NULL::timestamptz);
SELECT pgv_get('vars', 'tstz2', NULL::timestamptz);
SELECT pgv_set('vars', 'tstz1', '2016-03-30 12:00:00 GMT+01'::timestamptz);
SELECT pgv_get('vars', 'tstz1', NULL::timestamptz);

SELECT pgv_get('vars', 'tstz3', NULL::timestamptz);
SELECT pgv_get('vars', 'tstz3', NULL::timestamptz, false);
SELECT pgv_exists('vars', 'tstz3');
SELECT pgv_exists('vars', 'tstz1');
SELECT pgv_get('vars', 'ts1', NULL::timestamptz);

SELECT pgv_set('vars', 'tstzNULL', NULL::timestamptz);
SELECT pgv_get('vars', 'tstzNULL', NULL::timestamptz);

-- Date variables
SELECT pgv_set('vars', 'd1', '2016-03-29'::date);
SELECT pgv_set('vars', 'd2', '2016-03-30'::date);
SELECT pgv_set('vars', 'tstz1', '2016-04-01'::date);

SELECT pgv_get('vars', 'd1', NULL::date);
SELECT pgv_get('vars', 'd2', NULL::date);
SELECT pgv_set('vars', 'd1', '2016-04-02'::date);
SELECT pgv_get('vars', 'd1', NULL::date);

SELECT pgv_get('vars', 'd3', NULL::date);
SELECT pgv_get('vars', 'd3', NULL::date, false);
SELECT pgv_exists('vars', 'd3');
SELECT pgv_exists('vars', 'd1');
SELECT pgv_get('vars', 'tstz1', NULL::date);

SELECT pgv_set('vars', 'dNULL', NULL::date);
SELECT pgv_get('vars', 'dNULL', NULL::date);

-- Jsonb variables
SELECT pgv_set('vars2', 'j1', '[1, 2, "foo", null]'::jsonb);
SELECT pgv_set('vars2', 'j2', '{"bar": "baz", "balance": 7.77, "active": false}'::jsonb);
SELECT pgv_set('vars', 'd1', '[1, 2, "foo", null]'::jsonb);

SELECT pgv_get('vars2', 'j1', NULL::jsonb);
SELECT pgv_get('vars2', 'j2', NULL::jsonb);
SELECT pgv_set('vars2', 'j1', '{"foo": [true, "bar"], "tags": {"a": 1, "b": null}}'::jsonb);
SELECT pgv_get('vars2', 'j1', NULL::jsonb);

SELECT pgv_get('vars2', 'j3', NULL::jsonb);
SELECT pgv_get('vars2', 'j3', NULL::jsonb, false);
SELECT pgv_exists('vars2', 'j3');
SELECT pgv_exists('vars2', 'j1');
SELECT pgv_get('vars', 'd1', NULL::jsonb);

SELECT pgv_set('vars', 'jNULL', NULL::jsonb);
SELECT pgv_get('vars', 'jNULL', NULL::jsonb);

-- Array variables
SELECT pgv_set('vars', 'arr1', '{1, 2, null}'::int[]);
SELECT pgv_set('vars', 'arr2', '{"bar", "balance", "active"}'::text[]);
SELECT pgv_set('vars2', 'j1', '{1, 2, null}'::int[]);

SELECT pgv_get('vars', 'arr1', NULL::int[]);
SELECT pgv_get('vars', 'arr2', NULL::int[]);
SELECT pgv_set('vars', 'arr1', '{"bar", "balance", "active"}'::text[]);
SELECT pgv_set('vars', 'arr1', '{3, 4, 5}'::int[]);
SELECT pgv_get('vars', 'arr1', NULL::int[]);

SELECT pgv_get('vars', 'arr3', NULL::int[]);
SELECT pgv_get('vars', 'arr3', NULL::int[], false);
SELECT pgv_exists('vars', 'arr3');
SELECT pgv_exists('vars', 'arr1');
SELECT pgv_get('vars2', 'j1', NULL::int[]);

SELECT pgv_set('vars', 'arrNULL', NULL::int[]);
SELECT pgv_get('vars', 'arrNULL', NULL::int[]);

-- Manipulate variables
SELECT * FROM pgv_list() order by package, name;

SELECT pgv_remove('vars', 'int3');
SELECT pgv_remove('vars', 'int1');
SELECT pgv_get('vars', 'int1', NULL::int);
SELECT pgv_exists('vars');

SELECT pgv_remove('vars2');
SELECT pgv_get('vars2', 'j1', NULL::jsonb);
SELECT pgv_exists('vars2');

SELECT * FROM pgv_list() order by package, name;

SELECT pgv_free();
SELECT pgv_exists('vars');

SELECT * FROM pgv_list() order by package, name;
