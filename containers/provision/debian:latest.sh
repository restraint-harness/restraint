#!/bin/bash

DEPS_TOOL="apt-get -o Debug::pkgProblemResolver=yes -y --no-install-recommends"

apt -y update

apt -y install --no-install-recommends build-essential \
				       devscripts \
				       equivs \
				       python3-six \
				       python3-flask \
				       python3-pytest \
				       git-daemon-run \
				       valgrind

mk-build-deps --install --remove --tool "$DEPS_TOOL" /restraint/debian/control
