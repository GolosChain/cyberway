#!/bin/bash
set -euo pipefail

case "${BUILDKITE_BRANCH}" in
   v*.*.*)
      COMPILETYPE=Release
      git fetch origin --tags "${BUILDKITE_BRANCH}"
      ;;
   master) COMPILETYPE=Release;;
   *) COMPILETYPE=RelWithDebInfo;;
esac

GIT_REVISION=$(git rev-parse HEAD)
VERSION_STRING=$(git describe --tags --dirty)

echo "BUILDKITE_BRANCH: ${BUILDKITE_BRANCH}"
echo "GIT_REVISION: ${GIT_REVISION}"
echo "VERSION_STRING: ${VERSION_STRING}"
echo "COMPILETYPE: ${COMPILETYPE}"

docker build -t cyberway/cyberway:${GIT_REVISION} --build-arg=builder=${GIT_REVISION} --build-arg=compiletype=${COMPILETYPE} --build-arg=revision=${GIT_REVISION} --build-arg=versionstring=${VERSION_STRING} -f Docker/Dockerfile .
