#!/bin/bash 

sub=$1

path="./output"
command="list"

if [ "$1" = "-l" ]; then
	path=$2
fi

if [ "$3" = "-l" ]; then
	path=$4
fi

if [ "$1" = "-c" ]; then
	command=$2
	if [ "$#" = "5" ]; then
		n=$3
	fi
fi

if [ "$3" = "-c" ]; then
	command=$4
	if [ "$#" = "5" ]; then
		n=$5
	fi
fi



if [ "$command" = "list" ]; then
	action=$(ls $path)
	echo "$action"
elif [ "$command" = "size" ]; then
	if [ -z ${n+x} ]; then 
		action=$(du $path* -hs | sort -hr) 
	else 
		action=$(du $path* -hs | sort -hr | head -$n)
	fi
	
	echo "$action"
elif [ "$command" = "purge" ]; then
	rm -rf ${path}/*
fi

