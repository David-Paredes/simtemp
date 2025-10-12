#!/bin/bash

case "$1" in 
    clean)
        echo "Cleaning the project..."
        cd ../kernel
        make clean
        cd $PWD
        ;;
    *)
        echo "Building the project..."
        cd ../kernel
        make
        cd $PWD
        ;;
esac
