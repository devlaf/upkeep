#!/bin/sh

cd "$(dirname "$0")"
cd ../../libuv/
./autogen.sh && ./configure && make
cp ./.libs/libuv.a ./out/Debug/
