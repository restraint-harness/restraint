#!/bin/bash

RESTRAINT_DIR=/restraint
RPMBUILD_DIR="$RESTRAINT_DIR"/tito-test-build

cd $RESTRAINT_DIR || exit

dnf -y install tito || exit

tito build --rpmbuild-options="--without static" --test --rpm -o "$RPMBUILD_DIR"
