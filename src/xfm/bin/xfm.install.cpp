XCOMM! /bin/sh

XCOMM-------------------------------------------------------------------------
XCOMM xfm.install
XCOMM
XCOMM (c) Simon Marlow 1994, Albert Graef 1997
XCOMM-------------------------------------------------------------------------

XCOMM (-n: run non-interactively)

if [ -d $HOME/.xfm ]; then
    if [ "$1" != "-n" ]; then
	echo You already have a ~/.xfm directory, would you like it
	echo -n 'replaced with the default configuration? [n] '
	read ANS
	if [ "$ANS" != "y" -a "$ANS" != "Y" ]; then
	    echo Aborting.
	    exit 1
	fi
	rm -rf $HOME/.xfm
    else
	echo ~/.xfm already exists. Aborting.
	exit 1
    fi
fi

mkdir $HOME/.xfm && cp LIBDIR/dot.xfm/?* $HOME/.xfm

if [ $? != 0 ]; then
    echo Installation failed for some reason. Please consult your
    echo system administrator.
    exit 1
fi

chmod u+w $HOME/.xfm/?*

if [ ! -d $HOME/.trash ]; then
    mkdir $HOME/.trash
fi

echo Default configuration files installed.
