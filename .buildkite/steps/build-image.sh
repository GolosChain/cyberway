#!/bin/bash
set -euo pipefail

EXPECTED_VERSION=
case "${BUILDKITE_BRANCH}" in
   v*.*.*)
      COMPILETYPE=Release
      EXPECTED_VERSION="${BUILDKITE_BRANCH}"
      git fetch origin --tags "${BUILDKITE_BRANCH}"
      ;;
   master) COMPILETYPE=Release;;
   *) COMPILETYPE=RelWithDebInfo;;
esac

GIT_REVISION=$(git rev-parse HEAD)

if [ -n "$EXPECTED_VERSION" ]; then
    VERSION_STRING=$(git describe --tags --dirty --match "${EXPECTED_VERSION}")
    if [ "${VERSION_STRING}" != "${EXPECTED_VERSION}" ]; then
        echo "Version does not equal to expected. Current: ${VERSION_STRING}, expect: ${EXPECTED_VERSION}"
        exit 1
    fi
else
    VERSION_STRING=$(git describe --tags --dirty)
fi

echo "BUILDKITE_BRANCH: ${BUILDKITE_BRANCH}"
echo "GIT_REVISION: ${GIT_REVISION}"
echo "VERSION_STRING: ${VERSION_STRING}"
echo "COMPILETYPE: ${COMPILETYPE}"

docker build -t cyberway/cyberway:${GIT_REVISION} --build-arg=builder=${GIT_REVISION} --build-arg=compiletype=${COMPILETYPE} --build-arg=revision=${GIT_REVISION} --build-arg=versionstring=${VERSION_STRING} -f Docker/Dockerfile .
