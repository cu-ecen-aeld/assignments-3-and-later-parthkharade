#!/bin/bash
# Check if the correct number of arguments are present.
if [ $# -ne 2 ]
then
	echo Invalid number of arguments.
	echo command format : ./writer.sh path_to_file string_to_be_insterted
	exit 1
# If inputs are valid then create and write to the required file.
else
	mkdir -p "$(dirname "$1")" && touch "$1"
	echo $2 > $1
fi
