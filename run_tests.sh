#!/usr/bin/env bash

# Copyright (c) 2018, Postgres Professional


# provide a decent default level
if [ -z ${LEVEL+x} ]; then
	LEVEL=scan-build
fi

set -ux

status=0


# show pg_config just in case
pg_config


# perform code checks if asked to
if [ "$LEVEL" = "scan-build" ]; then

	# perform static analyzis
	scan-build --status-bugs make USE_PGXS=1 || status=$?

	# something's wrong, exit now!
	if [ $status -ne 0 ]; then exit 1; fi

	# don't forget to "make clean"
	make USE_PGXS=1 clean
fi

# build with cassert + valgrind support
if [ "$LEVEL" = "hardcore" ]; then

	set -e

	CUSTOM_PG_PATH=$PWD/pg_bin

	# here PG_VERSION is provided by postgres:X-alpine docker image
	wget -O postgresql.tar.bz2 "https://ftp.postgresql.org/pub/source/v$PG_VERSION/postgresql-$PG_VERSION.tar.bz2"
	echo "$PG_SHA256 *postgresql.tar.bz2" | sha256sum -c -

	mkdir postgresql

	tar \
		--extract \
		--file postgresql.tar.bz2 \
		--directory postgresql \
		--strip-components 1

	cd postgresql

	# enable Valgrind support
	sed -i.bak "s/\/* #define USE_VALGRIND *\//#define USE_VALGRIND/g" src/include/pg_config_manual.h

	# enable additional options
	eval ./configure \
		--with-gnu-ld \
		--enable-debug \
		--enable-cassert \
		--prefix=$CUSTOM_PG_PATH

	# TODO: -j$(nproc)
	make -s -j1 && make install

	# override default PostgreSQL instance
	export PATH=$CUSTOM_PG_PATH/bin:$PATH

	# show pg_config path (just in case)
	which pg_config

	cd -

	set +e
fi

# build and install extension (using PG_CPPFLAGS and SHLIB_LINK for gcov)
make USE_PGXS=1 PG_CPPFLAGS="-coverage" SHLIB_LINK="-coverage"
make USE_PGXS=1 install

# initialize database
initdb -D $PGDATA

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
