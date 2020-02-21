#!/bin/bash
#
# script: bootstrap.sh

readonly RUN_SHELL=/bin/bash

if [ "$1" = "-r" ] ; then
	"$RUN_SHELL" "$2"
	shift 2
fi

cmd=$*

exec ${cmd:-${RUN_SHELL}}
