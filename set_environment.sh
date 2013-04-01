#!/bin/bash

scripts/feeds update -a
scripts/feeds install -a
cp -f configs/ar71xx/config .config
cp -rf configs/ar71xx/files ./
