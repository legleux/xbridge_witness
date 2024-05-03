#!/usr/bin/env bash

source_dir=$PWD
cd .github/actions/build
. ./deps.sh
. ./config.sh
. ./build.sh
