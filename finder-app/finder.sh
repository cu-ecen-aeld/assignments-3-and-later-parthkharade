#!/bin/bash
# Global variables to retain value between recursive calls.
filecount=0
matchcount=0
maindir=$1
searchstr=$2
# Search function performs a recursive search on the directory structure counting files and looking for matches.
search(){
	for file in $1/*
	do
		if [ -d $file ]
		then
			search $file
		elif [ -r $file ]
		then
			((filecount+=1))
			((matchcount+=$(cat ${file}|grep "$searchstr"|wc -l)))
		else
			((filecount+=1))
		fi
	done
}

# Check if the number of arguments are not equal to 2. Throw an error in that case.
if [ $# -ne 2 ]
then
	echo Invalid number of inputs.
	echo command format : ./finder.sh directory search_str
	exit  1
# Check if the first argument is a valid directory
elif [ ! -d $1 ]
then
	echo The first argument must be a directory.
	exit 1
# If All checks pass then perform the search and match operation
else
	search $1 $2
	echo The number of files are $filecount and the number of matching lines are $matchcount
fi	
