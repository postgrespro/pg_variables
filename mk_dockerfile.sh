set -eu
sed -e 's/${PG_VERSION}/'${PG_VERSION}/g Dockerfile.tmpl > Dockerfile
