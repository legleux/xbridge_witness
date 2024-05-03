#!/usr/bin/env bash

source_dir=$PWD
build_dir=$PWD/build
cd .github/actions/build
. ./deps.sh
. ./config.sh
. ./build.sh
