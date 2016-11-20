#!/bin/sh

cd tests
./../src/lua -e"_U=true" all.lua
cd -
