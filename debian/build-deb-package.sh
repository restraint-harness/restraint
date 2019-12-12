#!/bin/bash

set -u

TOP_DIR=$(cd $(dirname "$0")/.. && pwd)

cd ${TOP_DIR}

VERSION=$(cat ${TOP_DIR}/restraint.spec | grep '^Version:' | cut -f 2-)

cat ${TOP_DIR}/debian/changelog.template | sed "s/RST_VERSION/${VERSION}/" > ${TOP_DIR}/debian/changelog

dpkg-buildpackage -us -uc
