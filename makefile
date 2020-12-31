#!/bin/bash

#
# Buildinstructions
#
# $SARCH = $DARCH : ./makefile
# x86_x64 -> arm : ./makefile cross arm
# x86_x64 -> aarch64 : ./makefile cross aarch64
#
# You may need additional devel packages for the headerfiles of your destination $arch
# depending on the crosscompiler used, it's possible that we need an adaption for -I includes. 
# I suggest to simply build it on the destination arch, that works well for the pinephone

CC=$(which gcc)
EXT=""

if [ "cross" == "$1" ]; then

	if [ "arm" == "$2" ]; then 
		CC="arm-linux-gnu-gcc"
		EXT=".arm"
	fi
	if [ "aarch64" == "$2" ]; then 
		CC="aarch64-linux-gnu-gcc"
		EXT=".aarch64"
	fi
fi

# wifiscanner:
	
	$CC `pkg-config --cflags gtk+-3.0` -o wifiscanner$EXT wifiscanner.c `pkg-config --libs gtk+-3.0` -lrt -lm

# install:	
	
	# cp wifiscanner /usr/bin/
