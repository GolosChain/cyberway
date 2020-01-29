#!/bin/bash
set -euo pipefail

IMAGETAG=${BUILDKITE_BRANCH:-master}
BRANCHNAME=${BUILDKITE_BRANCH:-master}

GIT_REVISION=$(git rev-parse HEAD)

VERSION_STRING=$(git describe --tags --dirty)
COMPILETYPE=RelWithDebInfo


if [[ "${IMAGETAG}" == "master" ]]; then
    COMPILETYPE=Release
fi

docker build -t cyberway/cyberway:${IMAGETAG} --build-arg=builder=${BRANCHNAME} --build-arg=compiletype=${COMPILETYPE} --build-arg=versionstring=${VERSION_STRING} --build-arg=revision=${GIT_REVISION}  -f Docker/Dockerfile .
