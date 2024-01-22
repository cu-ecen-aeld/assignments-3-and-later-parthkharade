#!/bin/bash

if [ $# -ne 2 ]
then
	echo Invalid number of arguments.
	echo command format : ./writer.sh path_to_file string_to_be_insterted
else
	mkdir -p "$(dirname "$1")" && touch "$1"
	echo $2 > $1
fi
