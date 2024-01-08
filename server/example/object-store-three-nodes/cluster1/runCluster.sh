#!/bin/bash

BASEDIR=$(dirname "$0")
echo "base dir=${BASEDIR}"

rm conf
ln -s ${BASEDIR}/config conf

set -x

rm -rf os1/node1 os1/node2 os1/node3
mkdir -p os1/node1/snapshots os1/node2/snapshots os1/node3/snapshots

./build/ObjectStoreMain os1/node1/log conf/kv_app_config1.ini &
./build/ObjectStoreMain os1/node2/log conf/kv_app_config2.ini &
./build/ObjectStoreMain os1/node3/log conf/kv_app_config3.ini &
pgrep ObjectStoreMain
