#!/bin/bash
# This script downloads all external dependencies

set -x

checkLastSuccess() {
  # shellcheck disable=SC2181
  if [[ $? -ne 0 ]]; then
    echo "\033[31mInstall Error: $1\033[0m"
    exit 1
  fi
}

mkdir ~/temp

# preparation
apt-get update -y &&
  apt-get install -y wget tar git build-essential apt-utils &&
  # To work around "E: The method driver /usr/lib/apt/methods/https could not be found." issue
  apt-get install -y apt-transport-https ca-certificates

# download cmake 3.12
cd ~/temp && version=3.12 && build=0 &&
wget https://cmake.org/files/v$version/cmake-$version.$build.tar.gz &&
tar -xzf cmake-$version.$build.tar.gz &&
mv cmake-$version.$build cmake
echo -e "\033[32mcmake 3.12 downloaded successfully.\033[0m"

# download and install clang/clang++ 6.0.1
CLANG6=$(clang-6.0 --version | grep "6.0")
if [ -z "$CLANG6" ]; then
  wget -O - https://apt.llvm.org/llvm-snapshot.gpg.key | apt-key add - &&
    apt-get install -y software-properties-common &&
    apt-add-repository "deb http://apt.llvm.org/xenial/ llvm-toolchain-xenial-6.0 main" &&
    apt-get update -y &&
    apt-get install -y clang-6.0
  checkLastSuccess "install clang 6.0 fails"
  echo -e "\033[32mclang 6.0 installed successfully.\033[0m"
else
  echo "clang 6.0 has been installed, skip"
fi
# download and install gcc/g++ 9.4.0
GCC9=$(g++-9 --version | grep "9.4")
if [ -z "$GCC9" ]; then
  apt-get install -y software-properties-common &&
    add-apt-repository ppa:ubuntu-toolchain-r/test -y &&
    apt-get update -y &&
    apt-get install -y gcc-9 g++-9
  checkLastSuccess "install g++ 9.4 fails"
  echo -e "\033[32mg++ 9.4 installed successfully.\033[0m"
else
  echo "g++ 9.4 has been installed, skip"
fi

# download prometheus cpp client
apt-get install -y libcurl4-gnutls-dev &&
  cd ~/temp && version=v0.7.0 &&
  git clone -b $version https://github.com/jupp0r/prometheus-cpp.git &&
  cd prometheus-cpp/ && git submodule init && git submodule update
echo -e "\033[32mprometheus client ${version} downloaded successfully.\033[0m"

# download and install pre-requisits for protobuf and grpc
apt-get install -y autoconf automake libtool curl make unzip libssl-dev

# download grpc and related components
cd ~/temp && version=1.16 && build=1 &&
  git clone https://github.com/grpc/grpc &&
  cd grpc && git fetch --all --tags --prune &&
  git checkout tags/v$version.$build -b v$version.$build &&
  git submodule update --init
echo -e "\033[32mgrpc ${version} downloaded successfully.\033[0m"  

# download and install pre-requisits for rocksdb
apt-get install -y libgflags-dev libsnappy-dev zlib1g-dev libbz2-dev liblz4-dev libzstd-dev
# download rocksdb
cd ~/temp &&
  git clone https://github.com/facebook/rocksdb.git &&
  cd rocksdb &&
  git checkout v6.5.2 &&
  git submodule update --init --recursive
echo -e "\033[32mrocksdb 6.5.2 downloaded successfully.\033[0m"

# download abseil
cd ~/temp &&
  git clone https://github.com/abseil/abseil-cpp.git &&
  cd abseil-cpp &&
  git checkout 20190808
echo -e "\033[32mabseil-cpp downloaded successfully.\033[0m"

# download and install tools for code coverage
apt-get install -y lcov
echo -e "\033[32mlcov installed successfully.\033[0m"

# download and install tools required by gringofts
apt-get install -y libcrypto++-dev &&
  apt-get install -y doxygen &&
  apt-get install -y python &&
  apt-get install -y gcovr
# download and install sqlite3
apt-get install -y sqlite3 libsqlite3-dev
# download and install boost
apt-get update -y &&
  apt-get install -y libboost-all-dev
# download and install gettext (for envsubst)
apt-get install -y gettext
echo -e "\033[32mtools required by gringofts installed successfully.\033[0m"

echo
echo
echo
echo "Summary:"
echo -e "\033[32mcmake 3.12 downloaded successfully.\033[0m"
echo -e "\033[32mclang 6.0 installed successfully.\033[0m"
echo -e "\033[32mg++ 9.4 installed successfully.\033[0m"
echo -e "\033[32mprometheus client 0.7.0 downloaded successfully.\033[0m"
echo -e "\033[32mgrpc 1.16 downloaded successfully.\033[0m"
echo -e "\033[32mrocksdb 6.5.2 downloaded successfully.\033[0m"
echo -e "\033[32mabseil-cpp downloaded successfully.\033[0m"
echo -e "\033[32mlcov installed successfully.\033[0m"
echo -e "\033[32mtools required by gringofts installed successfully.\033[0m"
