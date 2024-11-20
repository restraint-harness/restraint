#!/bin/bash
#
# script: bootstrap.sh

readonly RUN_SHELL=/bin/bash

if [ "$1" = "-r" ] ; then
	"$RUN_SHELL" "$2"
	shift 2
fi

dnf install -y openssh-server psmisc
ssh-keygen -A
cat ./keys/*.pub >> /root/.ssh/authorized_keys

cmd=$*

exec ${cmd:-${RUN_SHELL}}
