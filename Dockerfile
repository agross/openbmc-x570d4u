FROM ubuntu:24.04

ENV DEBIAN_FRONTEND=noninteractive
RUN apt-get update \
    && apt-get install --yes --no-install-recommends \
        build-essential \
        ca-certificates \
        chrpath \
        cpio \
        debianutils \
        diffstat \
        file \
        gawk \
        git \
        git-lfs \
        gosu \
        iputils-ping \
        libdata-dumper-simple-perl \
        libsdl1.2-dev \
        libthread-queue-any-perl \
        locales \
        lz4 \
        python3 \
        socat \
        subversion \
        texinfo \
        wget \
        zstd \
    && locale-gen en_US.UTF-8 \
    && rm --recursive --force /var/lib/apt/lists/*
ENV LANG=en_US.UTF-8 LC_ALL=en_US.UTF-8

ARG BUILD_UID=1000
ARG BUILD_GID=1000
RUN if id ubuntu >/dev/null 2>&1; then \
        userdel --remove ubuntu; \
    fi \
    && if ! getent group "$BUILD_GID" >/dev/null; then \
        groupadd --gid "$BUILD_GID" builder; \
    fi \
    && useradd --create-home --uid "$BUILD_UID" --gid "$BUILD_GID" builder
ENV HOME=/home/builder

ENTRYPOINT ["/bin/bash", "/adjustments/build.sh", "--container"]