#!/bin/bash

cd $(dirname $(readlink -f $0))

if [[ "$2" -eq "clenup" ]]; then
    docker-compose -f docker-compose-events.yml down || exit 1
    docker volume rm cyberway-mongodb-data || exit 1
    docker volume rm cyberway-nodeos-data || exit 1
    docker volume rm cyberway-queue || exit 1
fi

docker volume create cyberway-mongodb-data || true
docker volume create cyberway-nodeos-data || true
docker volume create cyberway-queue || true

EXPORTER_PASS=$(cat /dev/urandom | tr -dc 'a-zA-Z0-9' | fold -w 32 | head -n 1)
EXPORTER_USER=$(cat /dev/urandom | tr -dc 'a-zA-Z0-9' | fold -w 8 | head -n 1)

if [[ ! -e .env ]]; then
  echo "EXPORTER_USER=$EXPORTER_USER" >> .env
  echo "EXPORTER_PASS=$EXPORTER_PASS" >> .env
fi 

docker-compose -f docker-compose-events.yml up -d
