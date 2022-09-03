#!/bin/bash
set -ex

SCRIPT_DIR=$( cd -- "$( dirname -- "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )
NAME=gr-dvbs2rx
ARCH=$(dpkg --print-architecture || rpmbuild --eval %{_arch})
DATESTR=$(date +"%a, %d %b %Y %T %z")
DIST=$(lsb_release -s -c || rpmbuild --eval %{dist})
BUILD_DIR=$SCRIPT_DIR/../build/$DIST-$ARCH/

# Cleanup
rm -rf $BUILD_DIR
mkdir -p $BUILD_DIR

# Scrape the version number from CMakeLists.txt
VERSION_MAJOR="$(cat CMakeLists.txt | grep "set(VERSION_MAJOR" | tr -s ' ' | cut -d' ' -f2 | cut -d')' -f1)"
VERSION_API="$(cat CMakeLists.txt | grep "set(VERSION_API" | tr -s ' ' | cut -d' ' -f2 | cut -d')' -f1)"
VERSION_ABI="$(cat CMakeLists.txt | grep "set(VERSION_ABI" | tr -s ' ' | cut -d' ' -f2 | cut -d')' -f1)"
VERSION=$VERSION_MAJOR"."$VERSION_API"."$VERSION_ABI
NAME_VER=$NAME-$VERSION
echo "Creating build for $NAME_VER"

# Archive the sources and git submodules
git archive --output=$BUILD_DIR/$NAME_VER.tar --prefix=$NAME_VER/ $VERSION
git submodule foreach "git archive --output $BUILD_DIR/$NAME_VER-\$name.tar --prefix $NAME_VER/\$path/ \$sha1"
git submodule foreach "tar --concatenate --file $BUILD_DIR/$NAME_VER.tar $BUILD_DIR/$NAME_VER-\$name.tar"

# Prepare the build directory and tarball
cd $BUILD_DIR
gzip --no-name $NAME_VER.tar