#!/bin/bash

set -e

RESTRAINT_DIR=/restraint
RPMBUILD_DIR="$RESTRAINT_DIR"/tito-test-build
BUILD_OPTS="--without static"

cd $RESTRAINT_DIR

dnf -y install tito

if [ "$1" == "static" ] ; then
	BUILD_OPTS=""
	dnf install -y $(rpmspec --buildrequires -q restraint.spec)
fi

tito build --rpmbuild-options="$BUILD_OPTS" --test --rpm -o "$RPMBUILD_DIR"
