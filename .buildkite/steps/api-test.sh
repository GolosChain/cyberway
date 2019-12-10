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
docker volume create --name=cyberway-keosd-data
docker volume create --name=cyberway-mongodb-data
docker volume create --name=cyberway-nodeos-data

cd Docker

IMAGETAG=${BUILDKITE_BRANCH:-master}

echo ":llama: Change docker-compose-api-test.yml"
sed -i "s/cyberway\/cyberway:stable/cyberway\/cyberway:${IMAGETAG}/g" docker-compose-api-test.yml
sed -i "s/\${PWD}\/config.ini/\${PWD}\/config-standalone.ini/g" docker-compose-api-test.yml
echo "----------------------------------------------"
cat docker-compose-api-test.yml
echo "----------------------------------------------"

docker-compose -f docker-compose-api-test.yml up -d || true

sleep 25s

NODE_STATUS=$(docker inspect --format "{{.State.Status}}" nodeosd)
NODE_EXIT=$(docker inspect --format "{{.State.ExitCode}}" nodeosd)

if [[ "$NODE_STATUS" == "exited" ]] || [[ "$NODE_EXIT" != "0" ]]; then
    docker logs nodeosd
    exit 1
fi

docker logs nodeosd

docker-compose -f docker-compose-api-test.yml exec nodeosd /bin/bash -c "PYTHONPATH=/opt/cyberway/tests/test_api python3 /opt/cyberway/tests/test_api/Tests/Cases/Runner.py --cleos cleos --skip BootSequenceTest ApiTest"

result=$?

docker-compose  -f docker-compose-api-test.yml down

exit $result