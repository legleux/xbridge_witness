#!/usr/bin/env bash

set -ex

root_dir=$( cd -- "$( dirname -- "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )
owner=${owner:-rippled}
source_dir=$(realpath ${source_dir:-$root_dir})
build_dir=$(realpath ${build_dir:-$root_dir/build})
build_config=Release
git clone --depth 1 https://github.com/${owner}/${repo}.git

NPROC=$(nproc)

conan install ${source_dir} \
    --output-folder ${build_dir} \
    --build missing \
    --settings build_type=${build_config}

cmake -S ${source_dir} -B ${build_dir} \
    -DCMAKE_BUILD_TYPE=${build_config} \
    -DCMAKE_TOOLCHAIN_FILE=${build_dir}/conan_toolchain.cmake

cmake --build ${build_dir} --parallel $NPROC

cmake --install ${build_dir} --prefix /opt/ripple --strip
