#/bin/sh

make || exit
rm ./*.zip
tools/cygwin-package/cygwin-package.sh
cd ../node && unzip -o ../node-src/*.zip