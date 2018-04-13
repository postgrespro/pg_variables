/* contrib/pg_variables/pg_variables--1.0--1.1.sql */

-- complain if script is sourced in psql, rather than via ALTER EXTENSION
\echo Use "ALTER EXTENSION pg_variables UPDATE TO '1.1'" to load this file. \quit

-- Delete previous vresion of functions.
DROP FUNCTION pgv_set(package text, name text, value anynonarray);
DROP FUNCTION pgv_set_int(package text, name text, value int);
DROP FUNCTION pgv_set_text(package text, name text, value text);
DROP FUNCTION pgv_set_numeric(package text, name text, value numeric);
DROP FUNCTION pgv_set_timestamp(package text, name text, value timestamp);
DROP FUNCTION pgv_set_timestamptz(package text, name text, value timestamptz);
DROP FUNCTION pgv_set_date(package text, name text, value date);
DROP FUNCTION pgv_set_jsonb(package text, name text, value jsonb);
DROP FUNCTION pgv_insert(package text, name text, r record);
DROP FUNCTION pgv_list();

-- Create new versions of setters
CREATE FUNCTION pgv_set(package text, name text, value anynonarray, is_transactional bool default false)
RETURNS void
AS 'MODULE_PATHNAME', 'variable_set_any'
LANGUAGE C VOLATILE;

CREATE FUNCTION pgv_set_int(package text, name text, value int, is_transactional bool default false)
RETURNS void
AS 'MODULE_PATHNAME', 'variable_set_int'
LANGUAGE C VOLATILE;

CREATE FUNCTION pgv_set_text(package text, name text, value text, is_transactional bool default false)
RETURNS void
AS 'MODULE_PATHNAME', 'variable_set_text'
LANGUAGE C VOLATILE;

CREATE FUNCTION pgv_set_numeric(package text, name text, value numeric, is_transactional bool default false)
RETURNS void
AS 'MODULE_PATHNAME', 'variable_set_numeric'
LANGUAGE C VOLATILE;

CREATE FUNCTION pgv_set_timestamp(package text, name text, value timestamp, is_transactional bool default false)
RETURNS void
AS 'MODULE_PATHNAME', 'variable_set_timestamp'
LANGUAGE C VOLATILE;

CREATE FUNCTION pgv_set_timestamptz(package text, name text, value timestamptz, is_transactional bool default false)
RETURNS void
AS 'MODULE_PATHNAME', 'variable_set_timestamptz'
LANGUAGE C VOLATILE;

CREATE FUNCTION pgv_set_date(package text, name text, value date, is_transactional bool default false)
RETURNS void
AS 'MODULE_PATHNAME', 'variable_set_date'
LANGUAGE C VOLATILE;

CREATE FUNCTION pgv_set_jsonb(package text, name text, value jsonb, is_transactional bool default false)
RETURNS void
AS 'MODULE_PATHNAME', 'variable_set_jsonb'
LANGUAGE C VOLATILE;

CREATE FUNCTION pgv_insert(package text, name text, r record, is_transactional bool default false)
RETURNS void
AS 'MODULE_PATHNAME', 'variable_insert'
LANGUAGE C VOLATILE;

-- pgv_list() changed output
CREATE FUNCTION pgv_list()
RETURNS TABLE(package text, name text, is_transactional bool)
AS 'MODULE_PATHNAME', 'get_packages_and_variables'
LANGUAGE C VOLATILE;
