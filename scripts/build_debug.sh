#!/bin/bash

# TODO - point these to the correct binary locations on your system.
CMAKE=$(which cmake)
NINJA=$(which ninja)

mkdir -p cmake-build-debug
$CMAKE -DCMAKE_BUILD_TYPE=Debug -DCMAKE_MAKE_PROGRAM=$NINJA -G Ninja -S . -B cmake-build-debug

$CMAKE --build cmake-build-debug --target clean -j 4
$CMAKE --build cmake-build-debug --target all -j 4