#!/usr/bin/env bash
set -ex

root_dir=$( cd -- "$( dirname -- "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )
source_dir=$(realpath ${source_dir:-$root_dir})
build_dir=$(realpath ${build_dir:-$root_dir/build})
build_config=Release

NPROC=$(nproc)

conan install ${source_dir} \
    --install-folder=${build_dir} \
    --output-folder=${build_dir} \
    --build=missing \
    --settings build_type=${build_config}

cmake -S ${source_dir} -B ${build_dir} \
    -DCMAKE_BUILD_TYPE=${build_config} \
    -DCMAKE_TOOLCHAIN_FILE=${build_dir}/conan_toolchain.cmake

cmake --build ${build_dir} --parallel $NPROC

cmake --install ${build_dir} --prefix ${root_dir} --strip
