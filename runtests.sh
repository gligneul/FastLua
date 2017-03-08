#!/bin/bash

echo "FL tests"
for f in fltests/*; do
  lua $f > luaresult.txt
  src/lua $f > flresult.txt
  if ! cmp --silent luaresult.txt flresult.txt; then
    echo "test failed: $f"
    echo "diff:"
    diff -u luaresult.txt flresult.txt
  fi
done
rm -f luaresult.txt flresult.txt

# lua tests:
# TODO fix
# cd tests
# ../src/lua -e"_U=true" all.lua
# cd -
