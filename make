#!/bin/bash

gcc `pkg-config --cflags gtk+-3.0` -o wifiscanner wifiscanner.c `pkg-config --libs gtk+-3.0` -lrt -lm
