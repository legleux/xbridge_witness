#!/usr/bin/env bash

conan profile new --detect default
conan profile update settings.compiler.libcxx=libstdc++11 default
conan profile update settings.compiler.cppstd=20 default

conan remote add --insert 0 ripple http://18.143.149.228:8081/artifactory/api/conan/conan-non-prod
conan remote add --insert 1 redirectory https://conan.jfreeman.dev
