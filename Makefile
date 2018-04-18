# contrib/pg_variables/Makefile

MODULE_big = pg_variables
OBJS = pg_variables.o pg_variables_record.o $(WIN32RES)

EXTENSION = pg_variables
EXTVERSION = 1.1
DATA = pg_variables--1.0--1.1.sql
DATA_built = $(EXTENSION)--$(EXTVERSION).sql
$(EXTENSION)--$(EXTVERSION).sql: init.sql
	cat $^ > $@

PGFILEDESC = "pg_variables - sessional variables"

REGRESS = pg_variables pg_variables_any pg_variables_trans

ifdef USE_PGXS
PG_CONFIG = pg_config
PGXS := $(shell $(PG_CONFIG) --pgxs)
include $(PGXS)
else
subdir = contrib/pg_variables
top_builddir = ../..
include $(top_builddir)/src/Makefile.global
include $(top_srcdir)/contrib/contrib-global.mk
endif
