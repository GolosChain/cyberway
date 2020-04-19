#!/bin/bash
set -euo pipefail

GIT_REVISION=$(git rev-parse HEAD)
VERSION_STRING=$(git describe --tags --dirty)

case "${BUILDKITE_BRANCH}" in
   v*.*.*|master) COMPILETYPE=Release;;
   *) COMPILETYPE=RelWithDebInfo;;
esac

echo "BUILDKITE_BRANCH: ${BUILDKITE_BRANCH}"
echo "GIT_REVISION: ${GIT_REVISION}"
echo "VERSION_STRING: ${VERSION_STRING}"
echo "COMPILETYPE: ${COMPILETYPE}"

docker build -t cyberway/cyberway:${GIT_REVISION} --build-arg=builder=${GIT_REVISION} --build-arg=compiletype=${COMPILETYPE} --build-arg=revision=${GIT_REVISION} --build-arg=versionstring=${VERSION_STRING} -f Docker/Dockerfile .
