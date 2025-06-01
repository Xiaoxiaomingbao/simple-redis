#!/bin/bash
mkdir build && cd build
cmake ..
make
echo "Run server: ./server"
echo "Run client: ./client [-host YOUR_SERVER_IP] [-port YOUR_SERVER_PORT]"
