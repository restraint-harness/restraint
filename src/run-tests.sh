#!/bin/bash

set -ex

export PATH="$PWD/test-dummies:$PWD/../scripts:$PATH"
export RSTRNT_PKG_RETRIES=1
export MALLOC_CHECK_=2
export G_DEBUG="fatal_warnings fatal_criticals"
export G_SLICE="debug-blocks"

GITD_PID_FILE=git-daemon.pid
HTTPD_PID_FILE=http-daemon.pid

testargs=""

kill_daemon()
{
    pid_file=$1

    if [ -f "${pid_file}" ] ; then
        kill -TERM "$(cat "${pid_file}")" || :
        rm -f "${pid_file}" || :
    fi
}

cleanup()
{
    kill_daemon ${GITD_PID_FILE}
    kill_daemon ${HTTPD_PID_FILE}
}

trap cleanup EXIT

if [[ -f /usr/libexec/git-core/git-daemon ]] ; then
    git daemon --reuseaddr --listen=127.0.0.1 \
        --base-path=test-data/git-remote --export-all --enable=upload-archive \
        --verbose --detach --pid-file=${GITD_PID_FILE}
else
    testargs="-s /fetch_git/success -s /fetch_git/fail $testargs"
fi

function start_httpd() {
    pidfile=$PWD/${HTTPD_PID_FILE}
    httpd=$PWD/httpserver.py
    (cd test-data/http-remote && python2.7 ${httpd} -i ${pidfile} -p 8000 --host 127.0.0.1 &> /dev/null)
}
start_httpd

if [[ $1 == "--valgrind" ]] ; then
    shift
    G_DEBUG="gc-friendly $G_DEBUG"
    G_SLICE="always-malloc"
    for test in $* ; do
        valgrind --leak-check=full --num-callers=50 --error-exitcode=1 \
            --suppressions=valgrind.supp ./$test $testargs
    done
else
    ${GTESTER:-gtester} --verbose $testargs $*
fi
