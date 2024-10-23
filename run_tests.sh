#!/usr/bin/env bash

#
# Copyright (c) 2018-2024, Postgres Professional
#
# supported levels:
#		* standard
#		* scan-build
#		* hardcore
#		* nightmare
#

set -ux
status=0


# rebuild PostgreSQL with cassert + valgrind support
if [ "$LEVEL" = "hardcore" ] || \
   [ "$LEVEL" = "nightmare" ]; then

	set -e

	CUSTOM_PG_BIN=$PWD/pg_bin
	CUSTOM_PG_SRC=$PWD/postgresql

	# here PG_VERSION is provided by postgres:X-alpine docker image
	curl "https://ftp.postgresql.org/pub/source/v$PG_VERSION/postgresql-$PG_VERSION.tar.bz2" -o postgresql.tar.bz2
	echo "$PG_SHA256 *postgresql.tar.bz2" | sha256sum -c -

	mkdir $CUSTOM_PG_SRC

	tar \
		--extract \
		--file postgresql.tar.bz2 \
		--directory $CUSTOM_PG_SRC \
		--strip-components 1

	cd $CUSTOM_PG_SRC

	# enable Valgrind support
	sed -i.bak "s/\/* #define USE_VALGRIND *\//#define USE_VALGRIND/g" src/include/pg_config_manual.h

	# enable additional options
	./configure \
		CFLAGS='-O0 -ggdb3 -fno-omit-frame-pointer' \
		--enable-cassert --enable-debug \
		--prefix=$CUSTOM_PG_BIN \
		--quiet

	time make -s -j$(nproc) && make -s install

	# override default PostgreSQL instance
	export PATH=$CUSTOM_PG_BIN/bin:$PATH
	export LD_LIBRARY_PATH=$CUSTOM_PG_BIN/lib

	# show pg_config path (just in case)
	which pg_config

	cd -

	set +e
fi

# show pg_config just in case
pg_config

# perform code checks if asked to
if [ "$LEVEL" = "scan-build" ] || \
   [ "$LEVEL" = "hardcore" ] || \
   [ "$LEVEL" = "nightmare" ]; then

	# perform static analyzis
	scan-build --status-bugs make USE_PGXS=1 || status=$?

	# something's wrong, exit now!
	if [ $status -ne 0 ]; then exit 1; fi

	# don't forget to "make clean"
	make USE_PGXS=1 clean
fi


# build and install extension (using PG_CPPFLAGS and SHLIB_LINK for gcov)
make USE_PGXS=1 PG_CPPFLAGS="-coverage" SHLIB_LINK="-coverage" install

# initialize database
initdb -D $PGDATA

# set appropriate port
export PGPORT=55435
echo "port = $PGPORT" >> $PGDATA/postgresql.conf

# restart cluster 'test'
if [ "$LEVEL" = "nightmare" ]; then
	ls $CUSTOM_PG_BIN/bin

	valgrind \
		--tool=memcheck \
		--leak-check=no \
		--time-stamp=yes \
		--track-origins=yes \
		--trace-children=yes \
		--trace-children-skip="/bin/*,/usr/bin/*,/lib/*" \
		--gen-suppressions=all \
		--suppressions=$CUSTOM_PG_SRC/src/tools/valgrind.supp \
		--suppressions=$PWD/valgrind.supp \
		--log-file=/tmp/valgrind-%p.log \
		pg_ctl start -l /tmp/postgres.log -w || status=$?
else
	pg_ctl start -l /tmp/postgres.log -w || status=$?
fi

# something's wrong, exit now!
if [ $status -ne 0 ]; then cat /tmp/postgres.log; exit 1; fi

# run regression tests
export PG_REGRESS_DIFF_OPTS="-w -U3" # for alpine's diff (BusyBox)
make USE_PGXS=1 installcheck || status=$?

# show Valgrind logs if necessary
if [ "$LEVEL" = "nightmare" ]; then
	for f in $(find /tmp -name valgrind-*.log); do
		if grep -q 'Command: [^ ]*/postgres' $f && grep -q 'ERROR SUMMARY: [1-9]' $f; then
			echo "========= Contents of $f"
			cat $f
			status=1
		fi
	done
fi

# show diff if it exists
if test -f regression.diffs; then cat regression.diffs; fi

# something's wrong, exit now!
if [ $status -ne 0 ]; then exit 1; fi

# generate *.gcov files
gcov *.c *.h


set +ux


# send coverage stats to Codecov
bash <(curl -s https://codecov.io/bash)
