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

docker pull cuberway/cyberway:${IMAGETAG}
docker tag cyberway/cyberway:${IMAGETAG} cyberway/cyberway:${TAG}
docker push cyberway/cyberway:${TAG}


