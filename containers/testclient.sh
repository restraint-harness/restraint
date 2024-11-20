#!/bin/bash
#
# script: bootstrap.sh

readonly RUN_SHELL=/bin/bash

if [ "$1" = "-r" ] ; then
	"$RUN_SHELL" "$2"
	shift 2
fi

dnf install -y openssh-clients libxslt psmisc nginx
cp ./keys/testkey /root/.ssh/id_rsa
cp ./keys/testkey.pub /root/.ssh/id_rsa.pub

#setup webserver
echo "server { root /restraint/tests/daemon/www/html; }" > /etc/nginx/conf.d/tc.conf

cmd=$*

exec ${cmd:-${RUN_SHELL}}
