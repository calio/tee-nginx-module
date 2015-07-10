#!/bin/bash

# this file is mostly meant to be used by the author himself.

root=`pwd`
version=$1
home=~
force=$2

ngx-build $force $version \
          --with-ld-opt="-L$PCRE_LIB -Wl,-rpath,$PCRE_LIB:$LUAJIT_LIB:/usr/local/lib" \
          --with-cc-opt='-O0' \
          --add-module=$root/../echo-nginx-module \
          --add-module=$root/../ndk-nginx-module \
          --add-module=$root $opts \
            --with-debug \
          || exit 1
          #--with-debug || exit 1
          #--with-cc-opt="-g3 -O0"

