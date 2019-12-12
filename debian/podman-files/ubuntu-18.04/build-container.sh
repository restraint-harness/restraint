#!/bin/bash

set -u

podman build -t restraint/ubuntu-devel:18.04 .

echo "Run the container with the command:"
echo "podman run -it restraint/ubuntu-devel:18.04 bash"
