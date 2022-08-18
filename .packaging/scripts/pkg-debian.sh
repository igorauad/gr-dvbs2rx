#!/bin/bash
set -ex

DEB_REVISION=1 # should start with 1
SCRIPT_DIR=$( cd -- "$( dirname -- "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )
source $SCRIPT_DIR/pkg-prep.sh

mv $NAME_VER.tar.gz $NAME\_$VERSION.orig.tar.gz
tar xf $NAME\_$VERSION.orig.tar.gz
cd $NAME_VER

# Move the debian dir to the root dir and fill in the changelog template
mv .packaging/debian/ .
sed -i "s/\[%version%\]/$VERSION/g" debian/changelog
sed -i "s/\[%rev%\]/$DEB_REVISION/g" debian/changelog
sed -i "s/\[%distroseries%\]/$DIST/g" debian/changelog
sed -i "s/\[%date%\]/$DATESTR/g" debian/changelog

debuild -uc -us
