#!/bin/bash

error() {
	echo "<ERROR>"
	echo "$@"
	exit 1
}

TARGET=${1-ar71xx}

echo "Updating feeds"
scripts/feeds update -a
scripts/feeds install -a

scripts/env switch $TARGET || error environment for this target does not exist
