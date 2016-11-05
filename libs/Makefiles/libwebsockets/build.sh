#!/bin/sh

cd "$(dirname "$0")"
mkdir -p ../../libwebsockets/build
yes | cp -rf ./Makefile ../../libwebsockets/build/
cd ../../libwebsockets/build/
make
