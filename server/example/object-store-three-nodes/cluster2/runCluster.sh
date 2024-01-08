#!/bin/bash

BASEDIR=$(dirname "$0")
echo "base dir=${BASEDIR}"

rm conf
ln -s ${BASEDIR}/config conf

set -x

rm -rf os2/node1 os2/node2 os2/node3
mkdir -p os2/node1/snapshots os2/node2/snapshots os2/node3/snapshots

./build/ObjectStoreMain conf/kv_app_config1.ini > os2/node1/log 2>&1 &
./build/ObjectStoreMain conf/kv_app_config2.ini > os2/node2/log 2>&1 &
./build/ObjectStoreMain conf/kv_app_config3.ini > os2/node3/log 2>&1 &
pgrep ObjectStoreMain
