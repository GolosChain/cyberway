#!/bin/bash

set -euo pipefail

docker stop mongo || true
docker rm mongo || true
docker volume rm cyberway-mongodb-data || true
docker volume rm cyberway-nodeos-data || true
docker volume create --name=cyberway-mongodb-data
docker volume create --name=cyberway-nodeos-data

cd Docker

IMAGETAG=$(git rev-parse HEAD)

docker-compose up -d mongo

# Run unit-tests
sleep 10s
docker run --rm --network docker_cyberway-net -ti cyberway/cyberway:$IMAGETAG  /bin/bash -c '/opt/cyberway/tests/python_tests/run_long_tests.sh'
result=$?

docker-compose down

exit $result
