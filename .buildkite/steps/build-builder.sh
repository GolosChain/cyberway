#!/bin/bash
set -euo pipefail

IMAGETAG=$(git rev-parse HEAD)

cd Docker/builder
docker build -t cyberway/builder:${IMAGETAG} .