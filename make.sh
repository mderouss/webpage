#!/bin/bash

echo Making webpage...

gcc -L/usr/lib/x86_64-linux-gnu -o webpage webpage.c -lcmark

echo Done making webpage
