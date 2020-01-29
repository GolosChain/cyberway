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

IMAGETAG=${BUILDKITE_BRANCH:-master}

CDT_IMAGETAG=$([ "$BUILDKITE_BRANCH" == master ]  && echo "stable" || echo "latest")

BRANCHNAME=$([ "$BUILDKITE_BRANCH" == master ]  && echo "master" || echo "develop")

REF=${TAGREF:-"heads/$BUILDKITE_BRANCH"}

docker build -t cyberway/cyberway.api.test:${IMAGETAG} --build-arg=contract_branch=${BRANCHNAME} --build-arg=cw_imagetag=${IMAGETAG}  --build-arg=cdt_imagetag=${CDT_IMAGETAG} --build-arg=ref=${REF} .

echo ":llama: Change docker-compose.yml"
sed -i "s/cyberway\/cyberway.api.test:stable/cyberway\/cyberway.api.test:${IMAGETAG}/g" docker-compose.yml
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