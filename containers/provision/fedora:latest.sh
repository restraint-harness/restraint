#!/bin/bash

dnf -y install 'dnf-command(builddep)' \
	valgrind \
	git-daemon \
	python3-flask \
	python3-requests \
	python3-pytest \
	python3-six \
	procps-ng

dnf -y builddep --spec /restraint/specfiles/restraint-upstream.spec
