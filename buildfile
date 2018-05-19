# file      : buildfile
# copyright : Copyright (c) 2014-2018 Code Synthesis Ltd
# license   : MIT; see accompanying LICENSE file

define sh: file
sh{*}: extension =
sh{*}: install = bin/

./: msvc-common/exe{msvc-filter}              \
    sh{cl-* lib-* link-* mt-* rc-*}           \
    sh{msvc-** -msvc-common/msvc-filter*}     \
    doc{INSTALL LICENSE NEWS README} manifest

msvc-common/
{
  import libs = libbutl%lib{butl}

  exe{msvc-filter}: {hxx ixx txx cxx}{* -version} hxx{version} $libs

  hxx{version}: in{version} $src_root/manifest
}

# Don't install INSTALL file.
#
doc{INSTALL}@./: install = false

install.bin.subdirs = true # Recreate subdirectories.
