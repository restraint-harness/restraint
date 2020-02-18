#!/bin/bash

set -ex

export PATH="$PWD/test-dummies:$PWD/../scripts:$PATH"
export RSTRNT_PKG_RETRIES=1
export MALLOC_CHECK_=2
export G_DEBUG="fatal_warnings fatal_criticals"
export G_SLICE="debug-blocks"

GITD_PID_FILE=git-daemon.pid
HTTPD_PID_FILE=http-daemon.pid
HTTPD_LOG_FILE=httserver.log

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

    if [ -f  ${HTTPD_LOG_FILE} ] ; then
        rm -f ${HTTPD_LOG_FILE} || :
    fi
}

trap cleanup EXIT

if [[ -f /usr/libexec/git-core/git-daemon ]] ; then
    git daemon --reuseaddr --listen=127.0.0.1 \
        --base-path=test-data/git-remote --export-all --enable=upload-archive \
        --verbose --detach --pid-file=${GITD_PID_FILE}
else
    testargs="-s /fetch_git/success -s /fetch_git/fail -s /fetch_git/keepchanges $testargs"
fi

guess_python_version() {
    if command -v python3 >/dev/null ; then
      echo "python3"
    elif [ -f /usr/libexec/platform-python ] && /usr/libexec/platform-python --version 2>&1 | grep -q "Python 3" ; then
      echo "/usr/libexec/platform-python"
    else
      echo "python2.7"
    fi
}

start_httpd() {
    pidfile=$PWD/${HTTPD_PID_FILE}
    httpd=$PWD/httpserver.py
    httpd_log=$PWD/${HTTPD_LOG_FILE}

    (cd test-data/http-remote && $(guess_python_version) ${httpd} -i ${pidfile} -p 8000 --host 127.0.0.1 &> "${httpd_log}") \
        || (cat "${httpd_log}" && exit 1)
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
