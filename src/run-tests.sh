#!/bin/bash

set -ex

export PATH="$PWD/test-dummies:$PATH"
export MALLOC_CHECK_=2
export G_DEBUG="fatal_warnings fatal_criticals"
export G_SLICE="debug-blocks"

testargs=""

if [[ -f /usr/libexec/git-core/git-daemon ]] ; then
    git daemon --reuseaddr --listen=127.0.0.1 \
        --base-path=test-data/git-remote --export-all --enable=upload-archive \
        --verbose --detach --pid-file=git-daemon.pid
    function kill_git_daemon() {
        test -f git-daemon.pid && kill -TERM $(cat git-daemon.pid) || :
        rm -f git-daemon.pid
    }
    trap kill_git_daemon EXIT
else
    testargs="-s /task/fetch_git -s /task/fetch_git/negative $testargs"
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
