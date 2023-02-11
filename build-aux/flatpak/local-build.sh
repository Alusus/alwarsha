#!/usr/bin/env bash

rm -f org.alusus.Alwarsha.flatpak
rm -rf _build ; mkdir _build
rm -rf _repo ; mkdir _repo

STATE_DIR=~/.cache/alwarsha/flatpak-builder
BRANCH=master

flatpak-builder \
    --ccache --force-clean \
    --repo=_repo --state-dir=$STATE_DIR \
    --default-branch=$BRANCH \
    _build org.alusus.Alwarsha.json

flatpak build-bundle \
    _repo org.alusus.Alwarsha.flatpak org.alusus.Alwarsha $BRANCH

