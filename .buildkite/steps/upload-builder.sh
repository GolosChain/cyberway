#!/bin/bash
set -euo pipefail

IMAGETAG=$(git rev-parse HEAD)

docker images
docker login -u=$DHUBU -p=$DHUBP

docker push cyberway/builder:${IMAGETAG}
