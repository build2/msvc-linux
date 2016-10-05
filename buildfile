# file      : buildfile
# copyright : Copyright (c) 2014-2016 Code Synthesis Ltd
# license   : MIT; see accompanying LICENSE file

define sh: file
sh{*}: extension =
sh{*}: install = bin/

s = cl-11    cl-12    cl-14    cl-14u2             \
    lib-11   lib-12   lib-14   lib-14u2            \
    link-11  link-12  link-14  link-14u2           \
    msvc-11  msvc-12  msvc-14  msvc-14u2           \
    mt-11    mt-12    mt-14    mt-14u2             \
    rc-11    rc-12    rc-14    rc-14u2             \
                                                   \
    msvc-cl-common msvc-common msvc-lib-common     \
    msvc-link-common msvc-mt-common msvc-rc-common

./: exe{msvc-filter} sh{$s} doc{INSTALL LICENSE NEWS README version} \
    file{manifest}

import libs = libbutl%lib{butl}

exe{msvc-filter}: cxx{msvc-filter} $libs

# Don't install INSTALL file.
#
doc{INSTALL}@./: install = false
