#!/usr/bin/env bash
#
# Copyright 2022 The AMReX Community
#
# License: BSD-3-Clause-LBNL
# Authors: Axel Huebl

set -eu -o pipefail

sudo apt-get -qqq update
sudo apt-get install -y \
    bison               \
    build-essential     \
    ca-certificates     \
    gnupg               \
    flex                \
    libice-dev          \
    libmotif-dev        \
    libsm-dev           \
    libx11-dev          \
    libxext-dev         \
    libxpm-dev          \
    libxt-dev           \
    make                \
    pkg-config          \
    python3             \
    python3-pip         \
    wget

#python3 -m pip install -U pip setuptools wheel
#python3 -m pip install -r requirements.txt
