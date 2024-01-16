#!/bin/bash

dnf install -y 'dnf-command(config-manager)' \
	       'dnf-command(builddep)' \
	       make \
	       rpm-build \
	       valgrind \
	       git-daemon \
	       python3-requests \
	       python3-pytest \
	       python3.11-six \
	       epel-release

# Flask is in the EPEL repo.
dnf install -y python3-flask

# CodeReady Builder, the PowerTools equivalent.
dnf config-manager --set-enabled crb

dnf install -y $(rpmspec -D "_without_static ''"  --buildrequires -q /restraint/restraint.spec)
