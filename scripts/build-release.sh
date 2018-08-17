#!/bin/sh

set -o errexit

TAG_VERSION=$1
BUILD_DIRECTORY=$2

function usage()
{
    echo "This script builds the dynamically and statically linked version"
    echo "and generates the checksum files of the TurtleCoin tag provided."
    echo
    echo "USAGE: $0 <tag> <build-directory>"
    echo
    exit 1
}

function checkout_tag()
{
    echo "cloning Github repository $GITHUB_REPO to $CLONE_DIR .."
    git clone $GITHUB_REPO $CLONE_DIR

    echo "checking out tag $TAG_VERSION .."
    cd $CLONE_DIR
    git checkout $TAG_VERSION
}

function build_dynamic_linked_version()
{
    echo "starting dynamic build .."

    cd $CLONE_DIR
    mkdir -p build/release
    cd build/release
    cmake -DCMAKE_BUILD_TYPE=Release -DCMAKE_C_FLAGS="-fassociative-math" -DCMAKE_CXX_FLAGS="-fassociative-math" -DDO_TESTS=OFF ../..
    make
    cd ../..

    echo "dynamic build done .."
    generate_tarball $DYNAMIC_RELEASE
}

function build_static_linked_version()
{
    echo "starting static build .."

    cd $CLONE_DIR
    make clean
    mkdir -p build/release
    cd build/release
    cmake -DCMAKE_BUILD_TYPE=Release -DCMAKE_C_FLAGS="-fassociative-math" -DCMAKE_CXX_FLAGS="-fassociative-math" -DSTATIC=true -DDO_TESTS=OFF ../..
    make
    cd ../..

    echo "static build done .."
    generate_tarball $STATIC_RELEASE
}

function generate_tarball()
{
    RELEASE_NAME=$1

    if [ ! -d "$TARGET_DIR" ];
    then
        mkdir -p "$TARGET_DIR"
    fi

    TARBALL="$TARGET_DIR/$RELEASE_NAME.tar.gz"

    echo "generating tarball $TARBALL .."
    tar --transform "s,^,$RELEASE_NAME/," -c -f $TARBALL -z -C "$CLONE_DIR/build/release/src" \
        miner \
        zedwallet \
        TurtleCoind \
        turtle-service

    generate_checksums $TARBALL
}

function generate_checksums()
{
    FILE_TO_CHECK=$1

    echo "generating md5sum .."
    md5sum $FILE_TO_CHECK > $FILE_TO_CHECK.md5

    echo "generating sha512sum .."
    sha512sum $FILE_TO_CHECK > $FILE_TO_CHECK.sha512
}

function cleanup()
{
    if [ -d "$CLONE_DIR" ];
    then
        echo "removing git clone directory .."
        echo "rm -rf $CLONE_DIR"
    fi
}

if [ -z $TAG_VERSION ];
then
    usage
fi

if [ -z $BUILD_DIRECTORY ];
then
    echo "No build directory given, will build in $HOME/build .."
    BUILD_DIRECTORY="$HOME/build"
fi

if [ ! -d "$BUILD_DIRECTORY" ];
then
    echo "creating build directory $BUILD_DIRECTOR .."
    mkdir -p "$BUILD_DIRECTORY"
fi

# -- Config
GITHUB_REPO="https://github.com/turtlecoin/turtlecoin.git"
CLONE_DIR="$BUILD_DIRECTORY/turtlecoin-buildall"
TARGET_DIR="$BUILD_DIRECTORY/turtlecoin-releases"
DYNAMIC_RELEASE="turtlecoin-${TAG_VERSION}-linux-CLI"
STATIC_RELEASE="turtlecoin-${TAG_VERSION}-linux-staticboost-CLI"

checkout_tag
build_static_linked_version
build_dynamic_linked_version
cleanup

