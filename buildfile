# file      : buildfile
# copyright : Copyright (c) 2014-2017 Code Synthesis Ltd
# license   : MIT; see accompanying LICENSE file

define sh: file
sh{*}: extension =
sh{*}: install = bin/

./: msvc-common/exe{msvc-filter}                            \
    sh{cl-* lib-* link-* mt-* rc-*}                         \
    sh{msvc-** -msvc-common/msvc-filter*}                   \
    doc{INSTALL LICENSE NEWS README version} file{manifest}

# The version file is auto-generated (by the version module) from manifest.
# Include it in distribution and don't remove when cleaning in src (so that
# clean results in a state identical to distributed).
#
doc{version}: file{manifest}
doc{version}: dist  = true
doc{version}: clean = ($src_root != $out_root)

msvc-common/
{
  import libs = libbutl%lib{butl}

  exe{msvc-filter}: {hxx ixx txx cxx}{* -version} hxx{version} $libs

  hxx{version}: in{version} $src_root/file{manifest}
}

# Don't install INSTALL file.
#
doc{INSTALL}@./: install = false

install.bin.subdirs = true # Recreate subdirectories.
