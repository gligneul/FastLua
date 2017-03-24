# FastLua

## Brief

FastLua is an trace JIT compiler that works as an Lua (5.3) extension.
There are only some minor modifications in the original interpreter.
Currently, FastLua uses the LLVM toolchain to generate native code (there are plans to change the backend).

For further information about Lua, see https://www.lua.org.

**ATENTION: This is still on development state.**

## Requiriments

LLVM (only tested with version 3.9)

## Compilation

Run `make <plataform (eg. linux)>` in project root folder.
The build infrastructure is the same that Lua uses.

## Usage

The usage should be equal to the standard Lua interpreter.
The compilation is done automatically when a hotspot is detected.
Since this is still a prototype, only few instructions will be compiled.
If the compilation fails, FastLua will fallback to the original interpreter and everything should work just fine.

## Tests

FastLua use Lua tests and a custom test suite. Use `runtests.sh` to run the tests.

## Benchmarks

There is also a benchmark suite. Type `runbenchmarks.sh` to run it.
