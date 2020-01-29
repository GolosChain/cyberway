#!/bin/bash
set -euo pipefail

IMAGETAG=$(git rev-parse HEAD)

docker images
docker login -u=$DHUBU -p=$DHUBP

if [[ "${BUILDKITE_BRANCH}" == "master" ]]; then
    TAG=stable
elif [[ "${BUILDKITE_BRANCH}" == "develop" ]]; then
    TAG=latest
else 
    TAG=${BUILDKITE_BRANCH}
fi

docker tag cyberway/builder:${IMAGETAG} cyberway/builder:${TAG}
docker push cyberway/builder:${TAG}
