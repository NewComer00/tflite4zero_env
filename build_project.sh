#!/bin/bash

# change the directory to tflite4zero_env
cd "$(dirname "$(readlink -f "$0")")"

echo -e "\nusage: ./build_project.sh <project name>\n" \
	"e.g. ./build_project.sh label_image_tf1.14\n"

# this script needs only one arg 
if [ $# -ne 1 ]; then
	echo -e "error: an argument <project name> is needed\n" \
	"all projects in ./project directory are listed below:\n" \
	"\n$(ls -1 ./project)\n"
	exit 1
else
	# the directory of the given <project name> must exist
	if [ ! -d "./project/$1" ]; then
		echo -e "error: project must exist in ./project directory\n" \
		"all projects in ./project directory are listed below:\n" \
		"\n$(ls -1 ./project)\n"
		exit 1
	else
		# build the project
		make -f ./project/$1/Makefile
	fi
fi

exit 0