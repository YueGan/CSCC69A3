#!/bin/sh

MAKEPRG=bmake
MAKEFLAGS="-j4"
kernel="ASST3"
current=`pwd`

# A Simple bash script to configure and compile os/161
echo "Configuring your tree for the machine on which you are working"
# cd ./src
# -------------------CHANGE ROOT PATH HERE ----------------------
./configure --ostree=$HOME/cscc69/root

echo
echo "Configuring kernel named $kernel"
cd kern/conf
./config $kernel || exit

echo
echo "Building the $kernel kernel"
cd ../compile/$kernel
${MAKEPRG} ${MAKEFLAGS} depend || exit
${MAKEPRG} ${MAKEFLAGS} || exit 

echo
echo "Installing $kernel kernel"
${MAKEPRG} ${MAKEFLAGS} install || exit

echo
echo "Building user-level utilities"
cd ../../..
bmake clean
${MAKEPRG} ${MAKEFLAGS} || exit
${MAKEPRG} ${MAKEFLAGS} install ||exit
#ctags -R .

echo
echo "Copying sys161.conf file to the root"
# -------------------CHANGE ROOT PATH HERE ----------------------
cp $current/sys161.conf /cmshome/ganyue/cscc69/root