#!/bin/bash

set -ex

export PATH="$PWD/test-dummies:$PATH"
export MALLOC_CHECK_=2
export G_DEBUG="fatal_warnings fatal_criticals"
export G_SLICE="debug-blocks"

testargs=""

function kill_daemons() {
    test -f git-daemon.pid && kill -TERM $(cat git-daemon.pid) || :
    test -f git-daemon.pid && rm -f git-daemon.pid || :
    test -f thttpd-daemon.pid && kill -TERM $(cat thttpd-daemon.pid) || :
    test -f thttpd-daemon.pid && rm -f thttpd-daemon.pid || :
}

trap kill_daemons EXIT

if [[ -f /usr/libexec/git-core/git-daemon ]] ; then
    git daemon --reuseaddr --listen=127.0.0.1 \
        --base-path=test-data/git-remote --export-all --enable=upload-archive \
        --verbose --detach --pid-file=git-daemon.pid
else
    testargs="-s /fetch_git/success -s /fetch_git/fail $testargs"
fi

if [[ -f /usr/sbin/thttpd ]] ; then
    thttpd -i $PWD/thttpd-daemon.pid -p 8000 -d test-data/http-remote -h 127.0.0.1 -l /dev/null
else
    testargs="-s /fetch_http/success -s /fetch_http/fail $testargs"
fi

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
