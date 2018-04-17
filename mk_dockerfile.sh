set -eu
sed -e 's/${PG_VERSION}/'${PG_VERSION}/g -e 's/${LEVEL}/'${LEVEL}/g Dockerfile.tmpl > Dockerfile
