## About

This script allows to quickly package a binary nodejs distribution for the Cygwin (windows) platform.
It creates a zip file containing node.exe, required libraries and some useful utilities such as bash, nano and cygrunsrv.

## Prerequisites

Obvously you'll need nodejs and the cygwin packages required to compile nodejs.
Additionaly these cygwin packages are required:

* Archive -> zip

## Installing

Just clone this repository somewhere _inside_ the nodejs directory (e.g. in tools/).

    git clone git://github.com/piscisaureus/nodejs-cygwin-package.git cygwin-package

## Packaging node

Make sure is compiled. If it isn't, do that first. From the node dir:

    ./configure
    make

Then just invoke the script. Depending on where you put it:

    path/to/cygwin-package.sh