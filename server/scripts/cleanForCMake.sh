#!/bin/bash

WORKING_DIR=$(pwd)
echo "working dir=${WORKING_DIR}"

set -x

find . -type d -name CMakeFiles -print0 | xargs -0 rm -rf CMakeFiles
find . -type f -name CMakeCache.txt -print0 | xargs -0 rm

rm -rf build

# clean up files generated by protobuf

# clean up files generated by test