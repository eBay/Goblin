#!/bin/bash
# run this script under project's dir
set -x
DIR=$(pwd)
# this script sets up all dependent submodules
SUMMARY=$(git submodule summary)
# gringofts
GRINGOFTS="Gringofts"
if [[ "$SUMMARY" == *"$GRINGOFTS"* ]]; then
  echo "Skipping $GRINGOFTS"
else
  cd "$DIR" || return
  git submodule add -f https://github.com/eBay/Gringofts "$GRINGOFTS"
  cd "$GRINGOFTS" || return
  git checkout master
fi

git submodule update --init --recursive

cd $DIR/Gringofts || return
./scripts/addSubmodules.sh
