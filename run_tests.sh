#!/usr/bin/env bash

# Copyright (c) 2018, Postgres Professional


set -ux


status=0

# perform static analyzis
scan-build --status-bugs make USE_PGXS=1 || status=$?

# something's wrong, exit now!
if [ $status -ne 0 ]; then exit 1; fi

# don't forget to "make clean"
make USE_PGXS=1 clean

# initialize database
initdb -D $PGDATA

# build and install extension (using PG_CPPFLAGS and SHLIB_LINK for gcov)
make USE_PGXS=1 PG_CPPFLAGS="-coverage" SHLIB_LINK="-coverage"
make USE_PGXS=1 install

# restart cluster 'test'
echo "port = 55435" >> $PGDATA/postgresql.conf
pg_ctl start -l /tmp/postgres.log -w || status=$?

# something's wrong, exit now!
if [ $status -ne 0 ]; then cat /tmp/postgres.log; exit 1; fi

# run regression tests
export PG_REGRESS_DIFF_OPTS="-w -U3" # for alpine's diff (BusyBox)
PGPORT=55435 make USE_PGXS=1 installcheck || status=$?

# show diff if it exists
if test -f regression.diffs; then cat regression.diffs; fi

# something's wrong, exit now!
if [ $status -ne 0 ]; then exit 1; fi

# generate *.gcov files
rm -f *serialize.{gcda,gcno}
gcov *.c *.h


set +ux


# send coverage stats to Codecov
bash <(curl -s https://codecov.io/bash)
