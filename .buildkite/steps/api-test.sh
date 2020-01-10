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

REF=${TAGREF:-"heads/$BUILDKITE_BRANCH"}

docker build -t cyberway/cyberway.api.test:${REVISION} --build-arg=contract_branch=master --build-arg=cw_imagetag=${REVISION}  --build-arg=cdt_imagetag=${BUILDTYPE} --build-arg=ref=${REF} .

echo ":llama: Change docker-compose.yml"
sed -i "s/cyberway\/cyberway.api.test:stable/cyberway\/cyberway.api.test:${REVISION}/g" docker-compose.yml
sed -i "s/\${PWD}\/config.ini/\${PWD}\/config-standalone.ini/g" docker-compose.yml
echo "----------------------------------------------"
cat docker-compose.yml
echo "----------------------------------------------"

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