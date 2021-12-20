#!/bin/sh

time gcc -ldl -lX11 -lGL -lm -O2 -o game game.c
