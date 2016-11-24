# file      : buildfile
# copyright : Copyright (c) 2014-2016 Code Synthesis Ltd
# license   : MIT; see accompanying LICENSE file

define sh: file
sh{*}: extension =
sh{*}: install = bin/

s = cl-11-32    cl-12-32    cl-14u0-32    cl-14u2-32    cl-14u2-64    \
    lib-11-32   lib-12-32   lib-14u0-32   lib-14u2-32   lib-14u2-64   \
    link-11-32  link-12-32  link-14u0-32  link-14u2-32  link-14u2-64  \
    mt-11-32    mt-12-32    mt-14u0-32    mt-14u2-32    mt-14u2-64    \
    rc-11-32    rc-12-32    rc-14u0-32    rc-14u2-32    rc-14u2-64    \
                                                                      \
    msvc-dispatch                                                     \
                                                                      \
    msvc-common/{msvc-cl-common msvc-common msvc-lib-common           \
                 msvc-link-common msvc-mt-common msvc-rc-common}      \
                                                                      \
    msvc-11/{msvc-11-32}                                              \
    msvc-12/{msvc-12-32}                                              \
    msvc-14/{msvc-14u0-32 msvc-14u2-32 msvc-14u2-64}


./: msvc-common/exe{msvc-filter} sh{$s}                     \
    doc{INSTALL LICENSE NEWS README version} file{manifest}

msvc-common/:
{
  import libs = libbutl%lib{butl}

  exe{msvc-filter}: cxx{msvc-filter} $libs
}

# Don't install INSTALL file.
#
doc{INSTALL}@./: install = false

install.bin.subdirs = true # Recreate subdirectories.
