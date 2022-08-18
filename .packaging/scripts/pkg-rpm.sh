#!/bin/bash
set -ex

RELEASE=1
SCRIPT_DIR=$( cd -- "$( dirname -- "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )
source $SCRIPT_DIR/pkg-prep.sh

mkdir -p $HOME/rpmbuild/{SOURCES,SPECS}
mv $NAME_VER.tar.gz $HOME/rpmbuild/SOURCES/
cd $SCRIPT_DIR/../../

# Move the spec template and fill it in
SPEC_DIR=$HOME/rpmbuild/SPECS
cp .packaging/rpm/$NAME.spec $SPEC_DIR
sed -i "s/\[%version%\]/$VERSION/g" $SPEC_DIR/$NAME.spec
sed -i "s/\[%release%\]/$RELEASE/g" $SPEC_DIR/$NAME.spec

rpmbuild --eval %{dist}
rpmbuild -bb -v $HOME/rpmbuild/SPECS/gr-dvbs2rx.spec