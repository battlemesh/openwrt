#!/bin/bash

error() {
	echo "<ERROR>"
	echo "$@"
	exit 1
}

TARGET=${1-ar71xx}

[ ! -d configs/$TARGET ] && error target does not exist

echo "Updating feeds"
scripts/feeds update -a
scripts/feeds install -a

echo "Copying config"
cp -f configs/ar71xx/config .config || error config for this target does not exist

echo "Copying files"
[ ! -e files ] && mkdir files
[ -d configs/$TARGET/files ] && cp -rf configs/ar71xx/files/* files/ 2>/dev/null
