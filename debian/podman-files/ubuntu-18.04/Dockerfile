FROM docker.io/library/ubuntu:18.04
ENV DEBIAN_FRONTEND=noninteractive

RUN apt update && apt upgrade -y && apt install -y \
    aptitude \
    build-essential \
    curl \
    debhelper \
    devscripts \
    gcc \
    gettext \
    git \
    libarchive-dev \
    libcurl4 \
    libcurl4-gnutls-dev \
    libglib2.0-dev \
    libselinux1-dev \
    libsoup2.4-dev \
    libssh-dev \
    libxml-sax-expat-perl \
    libxml2-dev \
    make \
    vim-nox

RUN mkdir /root/restraint-build/
WORKDIR /root/restraint-build/
COPY get-restraint.sh .


# RUN make

# Remove it from the environment
ENV DEBIAN_FRONTEND=
