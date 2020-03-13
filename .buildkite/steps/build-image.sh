#!/bin/bash
set -euo pipefail

GIT_REVISION=$(git rev-parse HEAD)
VERSION_STRING=$(git describe --tags --dirty)

COMPILETYPE=RelWithDebInfo

if [[ "${BUILDKITE_BRANCH}" == "master" ]]; then
    COMPILETYPE=Release
fi

docker build -t cyberway/cyberway:${GIT_REVISION} --build-arg=builder=${GIT_REVISION} --build-arg=compiletype=${COMPILETYPE} --build-arg=revision=${GIT_REVISION} --build-arg=versionstring=${VERSION_STRING} -f Docker/Dockerfile .
