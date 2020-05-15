#! /bin/bash
set -e

EOSIO_CDT_CMAKE=${EOSIO_CDT_CMAKE:-/usr/lib/cmake/eosio.cdt}

if [[ ! -d ${EOSIO_CDT_CMAKE} ]]; then
    echo "Invalid EOSIO_CDT_CMAKE: dir doesn't exists"
    exit 1
fi

EOSIO_CDT_TOOLCHAIN="${EOSIO_CDT_CMAKE}/EosioWasmToolchain.cmake"

if [[ ! -d "./build" ]]; then
    mkdir build
fi

cd build
cmake -DEOSIO_COMPILE_TEST_CONTRACTS=1 -DCMAKE_TOOLCHAIN_FILE=${EOSIO_CDT_TOOLCHAIN} ..
make install
