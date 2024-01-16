#!/bin/bash

dnf install -y 'dnf-command(config-manager)' \
	       'dnf-command(builddep)' \
	       make \
	       rpm-build \
	       valgrind \
	       git-daemon \
	       python3-flask \
	       python3-requests \
	       python3-pytest \
	       python3.11-six \
	       epel-release

dnf config-manager -y --enable powertools

dnf install -y $(rpmspec -D "_without_static ''"  --buildrequires -q /restraint/restraint.spec)
