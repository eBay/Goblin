#!/bin/bash

BASEDIR=$(dirname "$0")
echo "base dir=${BASEDIR}"

rm -rf conf
ln -s ${BASEDIR}/config conf

set -x

rm -rf os/node
mkdir -p os/node/snapshots
cp ./log os/node/log

