#!/bin/sh
# This file is meant to be ran during the BOOST build process using a 
# cross-compile toolchain for AARCH64

sed -i 's/using gcc/using gcc : aarch64 : aarch64-linux-gnu-g++/g' project-config.jam
