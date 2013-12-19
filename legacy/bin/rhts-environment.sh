#!/bin/bash
# Copyright (c) 2006 Red Hat, Inc. All rights reserved. This copyrighted material 
# is made available to anyone wishing to use, modify, copy, or 
# redistribute it subject to the terms and conditions of the GNU General 
# Public License v.2.
# 
# This program is distributed in the hope that it will be useful, but WITHOUT ANY
# WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A
# PARTICULAR PURPOSE. See the GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
#
# Authors: Bill Peck

if [ -z "$OUTPUTFILE" ]; then
        export OUTPUTFILE=`mktemp /mnt/testarea/tmp.XXXXXX`
fi

function report_result {
        rstrnt-report-result $@
}

function runuser_ {
        $(which runuser 2>/dev/null || which /sbin/runuser 2>/dev/null || echo /bin/su) "$@"
}

function runas_ {
        local user=$1 cmd=$2
        if [ -n "$user" ]; then
                echo "As $user: "
                HOME=$(eval "echo ~$user") LOGNAME=$user USER=$user runuser_ -m -c "$cmd" $user
        else
                echo "As $(whoami): "
                $cmd
        fi
}
