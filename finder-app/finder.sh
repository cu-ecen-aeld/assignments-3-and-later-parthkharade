#!/bin/bash
filecount=0
matchcount=0
maindir=$1
searchstr=$2
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


if [ $# -ne 2 ]
then
	echo Invalid number of inputs.
	echo command format : ./finder.sh directory search_str
	exit  1
elif [ ! -d $1 ]
then
	echo The first argument must be a directory.
	exit 1
else
	search $1 $2
	echo The number of files are $filecount and the number of matching lines are $matchcount
fi	
