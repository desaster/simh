#!/bin/bash

set -e

BB_DIR=$(realpath $(mktemp -p /tmp -d bb_XXXX))
SIMH_DIR=$(realpath $(mktemp -p /tmp -d simh_XXXX))

if [ "$BB_DIR" = "" ]
then
    echo "Could not create a temp directory for blinkenbone"
    exit 1
fi

if [ "$SIMH_DIR" = "" ]
then
    echo "Could not create a temp directory for simh!"
    exit 2
fi

if ! gcc -v >/dev/null 2>&1
then
    echo "gcc is missing! Try running:"
    echo
    echo "    apt-get install build-essential && apt-get build-dep simh"
    exit 3
fi

if ! git --version >/dev/null 2>&1
then
    echo "git is missing! Try running:"
    echo
    echo "    apt-get install git"
    exit 4
fi

if ! pcap-config >/dev/null 2>&1
then
    echo "libpcap is missing! try running:"
    echo
    echo "    apt-get install build-essential && apt-get build-dep simh"
    exit 5
fi

echo ":: Downloading BlinkenBone"

git clone https://github.com/j-hoppe/BlinkenBone.git "$BB_DIR"

echo ":: Downloading SIMH with realcons features"

git clone https://github.com/desaster/simh-realcons-pdp11 -b realcons "$SIMH_DIR"

echo ":: Starting SIMH build process"

make \
    -C "$SIMH_DIR" \
    BLINKENLIGHT_COMMON_DIR="$BB_DIR/projects/00_common" \
    BLINKENLIGHT_API_DIR="$BB_DIR/projects/07.0_blinkenlight_api" \
    pdp11

if [ ! -f "$SIMH_DIR/BIN/pdp11" ]
then
    echo "Something went wrong, the build did not result in a pdp11 binary :("
    exit 9
fi

echo "The build is complete."
echo "Two temporary directories were created. Make sure to remove them, if you"
echo "don't need them, since they take up quite a bit of space."
echo
echo "    $BB_DIR"
echo "    $SIMH_DIR"
echo
echo "The simh binary is located in:"
echo
echo "    $SIMH_DIR/BIN/pdp11"
echo
echo "Maybe you should copy it somewhere!"
