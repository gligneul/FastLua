#!/bin/bash

echo "FL tests"
for f in fltests/*; do
  echo -n "testing $f... "
  lua $f &> luaresult.txt
  src/lua $f &> flresult.txt
  if ! cmp --silent luaresult.txt flresult.txt; then
    echo "failed"
    echo "diff:"
    diff -u luaresult.txt flresult.txt
    rm -f luaresult.txt flresult.txt
    exit 1
  fi
  echo "done"
done
rm -f luaresult.txt flresult.txt

# lua tests:
# TODO fix
# cd tests
# ../src/lua -e"_U=true" all.lua
# cd -
