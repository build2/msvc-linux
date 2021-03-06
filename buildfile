# file      : buildfile
# license   : MIT; see accompanying LICENSE file

define sh: file

sh{*}:
{
  extension =
  install = bin/
}

./: msvc-common/exe{msvc-filter}                    \
    sh{cl-* lib-* link-* mt-* rc-*}                 \
    sh{msvc-** -msvc-common/msvc-filter*}           \
    doc{INSTALL NEWS README} legal{LICENSE AUTHORS} \
    manifest

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
