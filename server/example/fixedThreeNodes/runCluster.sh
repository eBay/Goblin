#!/bin/bash

WORKING_DIR=$(pwd)
echo "working dir=${WORKING_DIR}"

set -x

rm -rf node1 node2 node3
mkdir -p node1/snapshots node2/snapshots node3/snapshots

./build/ObjectStoreMain node1/log conf/kv_app_config1.ini &
./build/ObjectStoreMain node2/log conf/kv_app_config2.ini &
./build/ObjectStoreMain node3/log conf/kv_app_config3.ini &
pgrep ObjectStoreMain
