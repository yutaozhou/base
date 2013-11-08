#!/bin/sh

# Get the number of CPUs on the system
# This is not portable
unamestr=`uname`
if [[ "$unamestr" == 'Linux' ]]; then
  MULTICPUS="-j`grep -c ^processor /proc/cpuinfo`"
elif [[ "$unamestr" == 'Darwin' ]]; then
  MULTICPUS="-j`sysctl -n hw.ncpu`"
  export CC=`which clang`
  export CXX=`which clang++`
fi

BASE=`dirname ${0}`

pushd $BASE

if [[ -e ".git" ]]; then
    # Setup submodules
    git submodule init
else
    if [[ ! -e "yaml-cpp/CMakeLists.txt" ]]; then
        # Manually clone yaml-cpp, removing the directory first
        rm -r yaml-cpp
        git clone git@github.com:argiopetech/yaml-cpp.git --depth 1
    fi
fi

cp cmake/yaml-cpp-CMakeLists.txt yaml-cpp/CMakeLists.txt

pushd ./BUILD
cmake -DCMAKE_INSTALL_PREFIX='.' -DCMAKE_BUILD_TYPE="RELWITHDEBINFO" ..
make $MULTICPUS

make install

popd
popd
