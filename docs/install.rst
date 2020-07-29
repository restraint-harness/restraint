Installing
==========

Installing from RPM
-------------------

Pre-built statically linked versions are available for the following OSes:

- RedHatEnterpriseLinux
- Fedora
- CentOS

To get the appropriate repo file for your OS, use one of the commands listed
below:

- RedHatEnterpriseLinux

::

 # sudo wget -O /etc/yum.repos.d/beaker-harness.repo https://beaker-project.org/yum/beaker-harness-RedHatEnterpriseLinux.repo

- Fedora

::

 # sudo wget -O /etc/yum.repos.d/beaker-harness.repo https://beaker-project.org/yum/beaker-harness-Fedora.repo

- CentOS

::

 # sudo wget -O /etc/yum.repos.d/beaker-harness.repo https://beaker-project.org/yum/beaker-harness-CentOS.repo

Once you have the appropriate repo file on your system you can install Restraint
via dnf (or yum on older systems). Although you can install both the server and
the client on the same machine it is not recommended.

Install the Restraint client on your machine if you want to run stand-alone jobs
(i.e.: outside of Beaker)::

 # sudo dnf install restraint-client

Install the Restraint server on the systems that will run the tasks/tests::

 # sudo dnf install restraint

Building from Source
--------------------

Source code is located at
https://github.com/beaker-project/restraint/. Restraint can be built
and linked dynamically or statically. To build it dynamically you will
need the development libraries for the following packages installed (minimum
versions are listed):

 - zlib-1.2.11
 - bzip2-1.0.8
 - libffi-3.1
 - glib2-2.56.4
 - libxml2-2.9.10
 - libarchive-3.4.0
 - xz-5.2.4
 - libsoup-2.52.2
 - intltool-0.51.0
 - selinux-2.7
 - curl-7.68.0
 - json-c-0.13.1
 - openssl-1.0.1m


Commands that will make sure most of the development libraries required are
installed::

 # sudo dnf install zlib-devel bzip2-devel libffi-devel glib2-devel libxml2-devel
 # sudo dnf install libarchive-devel xz-devel libsoup-devel selinux-devel json-c-devel
 # sudo dnf install intltool openssl-devel libcurl-devel

Once you have all the development libraries installed, you can clone Restraint
from git::

 % git clone git@github.com:beaker-project/restraint.git
 % cd restraint

Build Restraint::

 % make

To build it statically first enter the third-party directory and build the
support libraries::

 % pushd third-party
 % make
 % popd

Then build Restraint with the following command::

 % pushd src
 % PKG_CONFIG_PATH=../third-party/tree/lib/pkgconfig make STATIC=1
 % popd

Installing Restraint::

 % make install


Starting the Daemon
===================

Regardless if you installed from RPM or from source you start the daemon one of
two ways. If the system uses systemd use the following commands::

 Enable the service for next reboot
 # systemctl enable restraintd.service
 Start the service now
 # systemctl start restraintd.service

For SysV init based systems use the following commands::

 Enable the service for next reboot
 # chkconfig --level 345 restraintd on
 Start the service now
 # service restraintd start

When Restraint runs as a system service it listens on the port 8081.
