#!/bin/bash
set -e

DEST="${CYBERWAY}/genesis"
SRC="${CYBERWAY}/tests/test_api/genesis"

: ${INITIAL_TIMESTAMP:=$(date +"%FT%T.%3N" --utc)}

err() {
    echo "ERROR: $*" >&2
    exit 1
}

CREATE_GENESIS_CMD="${CYBERWAY}/bin/create-genesis -g $DEST/genesis-info.json -o $DEST/genesis.dat"

rm $DEST -rf && mkdir $DEST && \
sed "s|\${INITIAL_TIMESTAMP}|$INITIAL_TIMESTAMP|; /^#/d" $SRC/genesis.json.tmpl | tee $DEST/genesis.json && \
sed "s|\${GENESIS_DIR}|$DEST|; /^#/d" $SRC/genesis-info.json.tmpl | tee $DEST/genesis-info.json && \
sed -i "s|\${CYBERWAY_CONTRACTS}|$CYBERWAY_CONTRACTS|; /^#/d" $DEST/genesis-info.json && \
$CREATE_GENESIS_CMD 2>&1  && \
GENESIS_DATA_HASH=$(sha256sum $DEST/genesis.dat | cut -f1 -d" ") && \
sed -i "s|\${GENESIS_DATA_HASH}|$GENESIS_DATA_HASH|; /^#/d" $DEST/genesis.json && \
cat $DEST/genesis.json

