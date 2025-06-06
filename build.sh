#!/bin/bash
mkdir build && cd build
cmake ..
make
echo "Run server: ./build/server"
echo "Run client: ./build/client/client [-host YOUR_SERVER_IP] [-port YOUR_SERVER_PORT]"
