#!/bin/bash

if (( $# > 0 )); then
    swc-launch -n -t /dev/tty2 -- "$@"
else
    swc-launch -n -t /dev/tty2 -- build/tablecloth
fi

