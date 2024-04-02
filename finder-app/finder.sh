#!/bin/bash
FILESDIR=$1
SEARCHSTR=$2

if [ -z "$FILESDIR" ] || [ -z "$SEARCHSTR" ]
then
        echo "Error: Directory or search string is not specified, usage: finder.sh filesdir searchstr"
	exit 1
fi

if [ -d FILESDIR ]
then
	echo "Error: Directory " "$FILESDIR" " does not exist"
        exit 1
fi

NUMOFSUB="ls ${FILESDIR} | wc -l"
NUMOFMATCHES="grep -rs ${SEARCHSTR} ${FILESDIR} | wc -l"

echo "The number of files are" `eval $NUMOFSUB` "and the number of matching lines are" `eval $NUMOFMATCHES`

exit 0
