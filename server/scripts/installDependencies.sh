#!/bin/bash
# This script installs all external dependencies
set -x

checkLastSuccess() {
  # shellcheck disable=SC2181
  if [[ $? -ne 0 ]]; then
    echo "\033[31mInstall Error: $1\033[0m"
    exit 1
  fi
}

# install cmake 3.12
CMAKE=$(cmake --version | grep "3.12")
if [ -z "$CMAKE" ]; then
  cd ~/temp/cmake &&
    ./bootstrap && make -j4 && make install
  checkLastSuccess "install cmake 3.12 fails"
  echo -e "\033[32mcmake 3.12 installed successfully.\033[0m"
else
  echo "cmake 3.12 has been installed, skip"
fi
# install prometheus cpp client
PROMETHEUS=$(find /usr -name '*libprometheus-cpp*')
if [ -z "$PROMETHEUS" ]; then
  cd ~/temp/prometheus-cpp/ &&
    CXX=g++-9 CC=gcc-9 cmake CMakeLists.txt && make -j 4 && make install
  checkLastSuccess "install prometheus cpp client fails"
  echo -e "\033[32mprometheus client 0.7.0 installed successfully.\033[0m"
else
  echo "prometheus client has been installed, skip"
fi
# install grpc and related components
# 1. install cares
CARES=$(find /usr -name '*c-ares*')
if [ -z "$CARES" ]; then
  cd ~/temp/grpc/third_party/cares/cares &&
    CXX=g++-9 CC=gcc-9 cmake -DCMAKE_BUILD_TYPE=Debug &&
    make && make install
  checkLastSuccess "install cares fails"
  echo -e "\033[32mc-ares installed successfully.\033[0m"
else
  echo "c-ares has been installed, skip"
fi
# 2. install protobuf
PROTOBUF=$(protoc --version | grep "3.6")
if [ -z "$PROTOBUF" ]; then
  cd ~/temp/grpc/third_party/protobuf/cmake &&
    mkdir -p build && cd build &&
    # use cmake instead of autogen.sh so that protobuf-config.cmake can be installed
    CXX=g++-9 CC=gcc-9 cmake -Dprotobuf_BUILD_TESTS=OFF -DCMAKE_BUILD_TYPE=Debug .. &&
    make && make install && make clean && ldconfig
  checkLastSuccess "install protobuf fails"
  echo -e "\033[32mprotobuf installed successfully.\033[0m"
else
  echo "protobuf v3.6 has been installed, skip"
fi
# 3. install grpc
# install libssl-dev to skip installing boringssl
GRPC=$(grep "1.16" /usr/local/lib/cmake/grpc/gRPCConfigVersion.cmake)
if [ -z "$GRPC" ]; then
  cd ~/temp/grpc &&
    sed -i -E "s/(gRPC_ZLIB_PROVIDER.*)module(.*CACHE)/\1package\2/" CMakeLists.txt &&
    sed -i -E "s/(gRPC_CARES_PROVIDER.*)module(.*CACHE)/\1package\2/" CMakeLists.txt &&
    sed -i -E "s/(gRPC_SSL_PROVIDER.*)module(.*CACHE)/\1package\2/" CMakeLists.txt &&
    sed -i -E "s/(gRPC_PROTOBUF_PROVIDER.*)module(.*CACHE)/\1package\2/" CMakeLists.txt &&
    echo "src/core/lib/gpr/log_linux.cc,src/core/lib/gpr/log_posix.cc,src/core/lib/iomgr/ev_epollex_linux.cc" | tr "," "\n" | xargs -L1 sed -i "s/gettid/sys_gettid/" &&
    CXX=g++-9 CC=gcc-9 cmake -DCMAKE_BUILD_TYPE=Debug &&
    make && make install && make clean && ldconfig
  checkLastSuccess "install grpc fails"
  echo -e "\033[32mgrpc 1.16 installed successfully.\033[0m"
else
  echo "gRPC v1.16 has been installed, skip"
fi
# install rocksdb
ROCKSDB=$(find /usr -name '*librocksdb*')
if [ -z "$ROCKSDB" ]; then
  cd ~/temp/rocksdb &&
    # enable portable due to https://github.com/benesch/cockroach/commit/0e5614d54aa9a11904f59e6316cfabe47f46ce02
    export PORTABLE=1 && export FORCE_SSE42=1 && export CXX=g++-9 && export CC=gcc-9 &&
    make static_lib &&
    make install-static
  checkLastSuccess "install rocksdb fails"
  echo -e "\033[32mrocksdb 6.5.2 installed successfully.\033[0m"
else
  echo "RocksDB has been installed, skip"
fi
# install abseil-cpp
ABSL=$(find /usr -name '*libabsl*')
if [ -z "$ABSL" ]; then
  cd ~/temp/abseil-cpp &&
    # explicitly set DCMAKE_CXX_STANDARD due to https://github.com/abseil/abseil-cpp/issues/218
    CXX=g++-9 CC=gcc-9 cmake -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_STANDARD=17 &&
    make && make install
  checkLastSuccess "install abseil-cpp fails"
  echo -e "\033[32mabseil-cpp installed successfully.\033[0m"
else
  echo "abseil has been installed, skip"
fi
# give read access to cmake modules
chmod o+rx -R /usr/local/lib/cmake
chmod o+rx -R /usr/local/include/

echo
echo
echo
echo "Summary:"
echo -e "\033[32mcmake 3.12 installed successfully.\033[0m"
echo -e "\033[32mprometheus client 0.7.0 downloaded successfully.\033[0m"
echo -e "\033[32mcares installed successfully.\033[0m"
echo -e "\033[32mprotobuf installed successfully.\033[0m"
echo -e "\033[32mcgrpc 1.16 installed successfully.\033[0m"
echo -e "\033[32mrocksdb 6.5.2 installed successfully.\033[0m"
echo -e "\033[32mabseil-cpp installed successfully.\033[0m"
