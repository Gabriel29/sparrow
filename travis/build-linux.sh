#!/bin/sh
cd build
ls -lsa /usr/
ls -lsa /usr/llvm35
ls -lsa /usr/local
ls -lsa /usr/local/llvm35
ls -lsa /usr/local/share
ls -lsa /usr/local/bin
cmake -DLLVM_DIR=/usr/local/ ..
make
