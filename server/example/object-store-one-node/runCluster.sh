#!/bin/bash

BASEDIR=$(dirname "$0")
echo "base dir=${BASEDIR}"

rm -rf conf
ln -s ${BASEDIR}/config conf

set -x

rm -rf os/node1
mkdir -p os/node1/snapshots

./build/ObjectStoreMain os/node1/log conf/kv_app_config.ini &
pgrep ObjectStoreMain
