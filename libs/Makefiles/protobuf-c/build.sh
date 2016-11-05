#!/bin/sh

cd "$(dirname "$0")"
cd ../../protobuf-c/
./autogen.sh && ./configure && make
