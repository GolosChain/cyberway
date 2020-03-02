#!/bin/bash

set -euo pipefail

docker stop keosd || true
docker stop nodeosd || true
docker stop mongo || true
docker rm keosd || true
docker rm nodeosd || true
docker rm mongo || true
docker volume rm cyberway-keosd-data || true
docker volume rm cyberway-mongodb-data || true
docker volume rm cyberway-nodeos-data || true
docker volume create --name=cyberway-nodeos-data

REVISION=$(git rev-parse HEAD)

if [[ ${BUILDKITE_BRANCH} == "master" ]]; then
    BUILDTYPE="stable"
else
    BUILDTYPE="latest"
fi

SYSTEM_CONTRACTS_TAG=${SYSTEM_CONTRACTS_TAG:-$BUILDTYPE}

docker pull cyberway/cyberway.contracts:${SYSTEM_CONTRACTS_TAG}

docker build -t cyberway/cyberway.api.test:${REVISION} --build-arg=cw_tag=${REVISION} --build-arg=system_contracts_tag=${SYSTEM_CONTRACTS_TAG} -f  Docker/api-test/Dockerfile .

export IMAGETAG=${REVISION} 
docker-compose --file Docker/api-test/docker-compose.yml up -d || true

trap "docker-compose  --file Docker/api-test/docker-compose.yml down" EXIT 

sleep 45s

NODE_STATUS=$(docker inspect --format "{{.State.Status}}" nodeosd)
NODE_EXIT=$(docker inspect --format "{{.State.ExitCode}}" nodeosd)

if [[ "$NODE_STATUS" == "exited" ]] || [[ "$NODE_EXIT" != "0" ]]; then
    docker logs nodeosd
    exit 1
fi

docker logs nodeosd

docker-compose --file Docker/api-test/docker-compose.yml exec nodeosd /bin/bash -c "PYTHONPATH=/opt/cyberway/tests/test_api python3 /opt/cyberway/tests/test_api/Tests/Cases/Runner.py --cleos cleos --skip BootSequenceTest ApiTest"

result=$?

exit $result