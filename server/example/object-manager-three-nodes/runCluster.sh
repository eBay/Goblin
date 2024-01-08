#!/bin/bash

BASEDIR=$(dirname "$0")
echo "base dir=${BASEDIR}"

rm conf
ln -s ${BASEDIR}/config conf

set -x

rm -rf om/node1 om/node2 om/node3
mkdir -p om/node1/snapshots om/node2/snapshots om/node3/snapshots

./build/ObjectManagerMain om/node1/log conf/kv_app_config1.ini &
./build/ObjectManagerMain om/node2/log conf/kv_app_config2.ini &
./build/ObjectManagerMain om/node3/log conf/kv_app_config3.ini &
pgrep ObjectManagerMain
