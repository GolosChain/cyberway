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

pushd Docker/api-test

REVISION=$(git rev-parse HEAD)
MASTER_REVISION=$(git rev-parse origin/master)

if [[ ${REVISION} == ${MASTER_REVISION} ]]; then
    BUILDTYPE="stable"
else
    BUILDTYPE="latest"
fi

SYSTEM_CONTRACTS_TAG=${SYSTEM_CONTRACTS_TAG:-$BUILDTYPE}

docker pull cyberway/cyberway.contracts:${SYSTEM_CONTRACTS_TAG}

docker build -t cyberway/cyberway.api.test:${REVISION} --build-arg=cw_imagetag=${REVISION} --build-arg=system_contracts_tag=${SYSTEM_CONTRACTS_TAG} .

echo ":llama: Change docker-compose.yml"
sed    "s/\${IMAGETAG}/${REVISION}/g" docker-compose.yml.tmpl > docker-compose.yml
echo "----------------------------------------------"
cat docker-compose.yml
echo "----------------------------------------------"

trap "rm -f docker-compose.yml" EXIT 

docker-compose up -d || true

sleep 45s

NODE_STATUS=$(docker inspect --format "{{.State.Status}}" nodeosd)
NODE_EXIT=$(docker inspect --format "{{.State.ExitCode}}" nodeosd)

if [[ "$NODE_STATUS" == "exited" ]] || [[ "$NODE_EXIT" != "0" ]]; then
    docker logs nodeosd
    exit 1
fi

docker logs nodeosd

docker-compose exec nodeosd /bin/bash -c "PYTHONPATH=/opt/cyberway/tests/test_api python3 /opt/cyberway/tests/test_api/Tests/Cases/Runner.py --cleos cleos --skip BootSequenceTest ApiTest"

result=$?

docker-compose down

popd 

exit $result