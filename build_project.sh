#!/bin/bash
echo -e \
	"\nusage: ./build_project.sh <project name>\n \
	e.g. ./build_project.sh label_image_tf1.14\n"

if [ $# -ne 1 ]; then
	echo -e "error: an argument is needed\n"
	exit 1
else	
	if [ ! -d "./project/$1" ]; then
		echo -e "error: project must exist in ./project directory\n"
		exit 1
	else	
		make TARGET_EXEC=$1
	fi
fi
