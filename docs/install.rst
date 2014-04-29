Installing
==========

Installing from rpm
-------------------

Pre-built statically linked versions are available for the following releases:

- RedHatEnterpriseLinux4

::

 # wget -O /etc/yum.repos.d/restraint.repo http://bpeck.fedorapeople.org/restraint/el4.repo

- RedHatEnterpriseLinux(Client|Server)5

::

 # wget -O /etc/yum.repos.d/restraint.repo http://bpeck.fedorapeople.org/restraint/el5.repo

- RedHatEnterpriseLinux6

::

 # wget -O /etc/yum.repos.d/restraint.repo http://bpeck.fedorapeople.org/restraint/el6.repo

- RedHatEnterpriseLinux7

::

 # wget -O /etc/yum.repos.d/restraint.repo http://bpeck.fedorapeople.org/restraint/el7.repo

- Fedora19

::

 # wget -O /etc/yum.repos.d/restraint.repo http://bpeck.fedorapeople.org/restraint/fc19.repo

- Fedora20

::

 # wget -O /etc/yum.repos.d/restraint.repo http://bpeck.fedorapeople.org/restraint/fc20.repo

Once you have the repo on your systen you can install via yum.  Although you can install both client/server
on the same machine it is not recommended.

Install the Client on your machine if you want to run stand alone jobs (ie: outside of beaker).

::

 # yum install restraint-client

Install the Server components on the system that will run the tasks/tests.

::

 # yum install restraint

If you need to run legacy RHTS tests install the -rhts sub-package on the system that will run the tasks/tests.

::

 # yum install restraint-rhts

Building from Source
--------------------

restraint can be built and linked dynamically or statically. To build it dynamically you will need the following packages installed (Minimum versions are listed):

 - zlib-1.2.8
 - bzip2-1.0.6
 - libffi-3.0.11
 - glib-2.38.0
 - libxml2-2.9.0
 - libarchive-3.1.2
 - xz-5.0.4
 - libsoup-2.42.2
 - sqlite-autoconf-3080002
 - intltool-0.35.5

Clone from git::

 % git clone https://github.com/p3ck/restraint.git

Build restraint::

 % make

To build it statically first enter the third-party directory and build the support libraries::

 % pushd third-party
 % make
 % popd

Then build restraint with the following command::

 % pushd src
 % PKG_CONFIG_PATH=third-party/tree/lib/pkgconfig make STATIC=1
 % popd

Installing restraint::

 % make install


Starting the Daemon
===================

Regardless if you installed from rpm or from source you start the daemon one of two ways.  If the
system uses systemd use the following commands::

 Enable the service for next reboot
 # systemctl enable restraintd.service
 Start the service now
 # systemctl start restraintd.service

For SysV init based systems use the following commands::

 Enable the service for next reboot
 # chkconfig --level 345 restraintd on
 Start the service now
 # service restraind start

