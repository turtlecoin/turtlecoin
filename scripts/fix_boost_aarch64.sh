#!/bin/sh
sed -i 's/using gcc/using gcc : aarch64 : aarch64-linux-gnu-g++/g' project-config.jam
