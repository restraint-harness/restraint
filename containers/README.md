# Containers for checks

Wraps Podman to run checks in containers.

## How it works

The `run.sh` script creates a container with a `/restraint` mount point
that targets restraint's source directory in the user's host.
The positional arguments passed are run as a command inside the
container. If no arguments are passed, an interactive shell is executed
to work inside the container.

## Prerequisites

Ensure that you are able to run Podman rootless. As a regular user,

```
$ podman run --rm -ti fedora:latest
```

If you face issues running Podman, refer to common issues and solutions
in [Podman's Troubleshooting](https://github.com/containers/libpod/blob/master/troubleshooting.md).

## How to use

From restraint source directory,

```
$ containers/run.sh make clean check valgrind
```

The arguments, `make clean check valgrind`, are executed as command
inside the container, where `/restraint` is the working directory.

The container image can be specified using the `-i` option,

```
$ containers/run.sh -i fedora:rawhide make clean check valgrind`
```

The default image is `fedora:latest`.

The container is provisioned automatically with the corresponding
script in `containers/provision/image.sh`, where image is the
name of the container image.

Alternative provision scripts can be used with the `-p` option,

```
$ containers/run.sh -i fedora:rawhide -p /restraint/containers/provision/script.sh
```

The path for the script is specified within the container.

Containers are persisted and reused for each run. Provisioning only
takes place if the container doesn't exist or if the `-p` option is
used.

Containers are named as `restraint-checks-repository-tag`, where
repository and tag are the ones from the image used for the container.
For a container using `fedora:latest` image,

```
restraint-checks-fedora-latest
```

## Podman commands

For the use cases that are not covered in the `run.sh` script you can
use Podman commands.

To show the current containers for Restraint checks, use the `podman container list`
command,

```
$ podman container list -a --filter "name=restraint-checks-.+"
```

To remove a container use the `podman rm` command with the name or the
ID of the container,

```
$ podman rm restraint-checks-fedora-latest
```

Refer to Podman documentation for further use cases.
