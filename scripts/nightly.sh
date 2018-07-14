#!/usr/bin/env bash

# nightly.sh - pull a copy of turtlecoin-development 
# and put it inside a timestamped folder.
# rock made this

sourcefolder=~/Source/turtle-dev-build-$(date +%F)

mkdir ~/Source
mkdir ~/Binaries
echo -e "\n MOVING TO ~/Source"
cd ~/Source/
ls -al

echo -e "\n CLONING TurtleCoin in $sourcefolder"
git clone -b development https://github.com/turtlecoin/turtlecoin $sourcefolder
cd $sourcefolder
mkdir -p $sourcefolder/build && cd $sourcefolder/build
ls -al

echo -e "\n BUILDING TurtleCoin"
cmake .. && make -j8 # remove -j8 for single core
cd src
ls -al

echo -e "\n\n COMPRESSING BINARIES"
zip turtlecoin-dev-bin-$(date +%F)-linux.zip miner poolwallet simplewallet TurtleCoind walletd
mv *.zip ~/Binaries/
cd ~/Binaries
ls -al

echo -e "\n COMPLETE!"
