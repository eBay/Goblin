#!/bin/bash

BASEDIR=$(dirname "$0")
echo "base dir=${BASEDIR}"

rm -rf conf
ln -s ${BASEDIR}/config conf

set -x

rm -rf om/node1
mkdir -p om/node1/snapshots

./build/ObjectManagerMain om/node1/log conf/kv_app_config.ini &
pgrep ObjectManagerMain
