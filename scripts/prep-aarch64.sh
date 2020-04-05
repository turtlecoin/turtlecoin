#!/bin/bash
# Copyright (c) 2019 The TurtleCoin Developers
#
# Please see the included LICENSE file for more information.

#
# This script is used to set up the cross-compilation environment
# for building aarch64 on x64_64 systems which provides a very
# fast build experience for the aarch64 platform at this time
#

# Save our current working location so that we can return here
# when we are ready
export BASEDIR=`pwd`

# Set our toolchain folder
export TOOLCHAIN_DIR=$HOME/toolchain

# Make and change to a toolchain directory for storage of all
# tools that we will use as part of the build process
mkdir -p $TOOLCHAIN_DIR && cd $TOOLCHAIN_DIR

# Check to see if we have the aarch64 compiler, if not download
# the prebuilt binaries from a trusted source
echo -n "Checking for aarch64-linux-gnu-gcc... "
if [ ! -f $TOOLCHAIN_DIR/gcc-arm-8.2-2018.08-x86_64-aarch64-linux-gnu/bin/aarch64-linux-gnu-gcc ]; then
  echo "Not found... Installing..."
  wget https://armkeil.blob.core.windows.net/developer/Files/downloads/gnu-a/8.2-2018.08/gcc-arm-8.2-2018.08-x86_64-aarch64-linux-gnu.tar.xz
  tar xfv gcc-arm-8.2-2018.08-x86_64-aarch64-linux-gnu.tar.xz >/dev/null
  echo "aarch64-linux-gnu-gcc: Installed"
else
  echo "Found"
fi

# Set our environment variables to use the aarch64 compiler
export CC=$TOOLCHAIN_DIR/gcc-arm-8.2-2018.08-x86_64-aarch64-linux-gnu/bin/aarch64-linux-gnu-gcc
export CXX=$TOOLCHAIN_DIR/gcc-arm-8.2-2018.08-x86_64-aarch64-linux-gnu/bin/aarch64-linux-gnu-g++
export RANLIB=$TOOLCHAIN_DIR/gcc-arm-8.2-2018.08-x86_64-aarch64-linux-gnu/bin/aarch64-linux-gnu-ranlib
export LD=$TOOLCHAIN_DIR/gcc-arm-8.2-2018.08-x86_64-aarch64-linux-gnu/bin/aarch64-linux-gnu-ld
export MAKEDEPPROG=$TOOLCHAIN_DIR/gcc-arm-8.2-2018.08-x86_64-aarch64-linux-gnu/bin/aarch64-linux-gnu-gcc
export CUSTOM_TOOLCHAIN_FILE=../scripts/cross-aarch64.cmake

# Check to see if we have Boost 1.55 ready in our toolchain, if
# not we'll download and build it
echo -n "Checking for Boost 1.55... "
if [ ! -f $TOOLCHAIN_DIR/boost_1_55_0/stage/lib/libboost_system.a ]; then
  echo "Not found... Installing..."
  wget http://sourceforge.net/projects/boost/files/boost/1.55.0/boost_1_55_0.tar.gz
  tar zxvf boost_1_55_0.tar.gz >/dev/null
  cd boost_1_55_0
  echo -n "Bootstrapping Boost 1.55 build... "
  ./bootstrap.sh > /dev/null
  echo "Complete"
  echo "using gcc : aarch64 : ${TOOLCHAIN_DIR}/gcc-arm-8.2-2018.08-x86_64-aarch64-linux-gnu/bin/aarch64-linux-gnu-g++ ; " >> tools/build/v2/user-config.jam
  echo -n "Building Boost 1.55... "
  ./b2 toolset=gcc-aarch64 --with-system --with-filesystem --with-thread --with-date_time --with-chrono --with-regex --with-serialization --with-program_options >/dev/null
  cd $TOOLCHAIN_DIR
  echo "Complete"
else
  echo "Found"
fi

# Set our environment variable to use the new Boost root
export BOOST_ROOT=$TOOLCHAIN_DIR/boost_1_55_0

# Check to see if we have OpenSSL ready in our toolchain, if
# not we'll download and build it
echo -n "Checking for OpenSSL 1.1.1f..."
if [ ! -f $TOOLCHAIN_DIR/openssl/lib/libcrypto.a ]; then
  echo "Not found... Installing..."
  wget https://www.openssl.org/source/openssl-1.1.1f.tar.gz
  tar zxvf openssl-1.1.1f.tar.gz >/dev/null
  cd openssl-1.1.1f
  ./Configure linux-aarch64 --prefix=$TOOLCHAIN_DIR/openssl --openssldir=$TOOLCHAIN_DIR/openssl >/dev/null
  echo -n "Building OpenSSL 1.1.1f... "
  make PROCESSOR=ARM install >/dev/null
  echo "Complete"
else
  echo "Found"
fi

# Set our environment variable to use the new OpenSSL root
export OPENSSL_ROOT_DIR=$TOOLCHAIN_DIR/openssl

# Set an environment variable that contains our custom CMake toolchain
# commands for our project and other env vars that we'll need
export STRIP=$TOOLCHAIN_DIR/gcc-arm-8.2-2018.08-x86_64-aarch64-linux-gnu/bin/aarch64-linux-gnu-strip

# Return to the path we started at
cd $BASEDIR
