#!/bin/bash

cmake -S .. -B build/IvyQt -DCMAKE_BUILD_TYPE=Debug -DCMAKE_INSTALL_PREFIX=build/install
cmake --build build/IvyQt
cmake --install build/IvyQt


cmake -S . -B build -DCMAKE_PREFIX_PATH=build/install
cmake --build build/

