#!/bin/bash

set -eu
set -x

readonly os="${1:?}"
readonly project="${2:?}"
readonly build_type="${3:?}"
readonly tests="${4:-}"

readonly root="/work/$project"
readonly build_dir="$root/build"

#XXX: updating PATH for using local cmake (centos-7)
readonly deps_bin_dir="/root/$project-build/bin/"
if [[ -d "$deps_bin_dir" ]]; then
  export PATH="$deps_bin_dir:$PATH"
fi

case "$os" in
(ubuntu) pkg_extension=deb ;;
(centos) pkg_extension=rpm ;;
(*)      echo "Bad os: $os." ; exit 1 ;;
esac

rm -rf $build_dir/*
mkdir -p $build_dir

"$root"/scripts/build.sh --build-type "$build_type" --install-mongo --enable-mongo

if [[ -x "$( command -v ccache &>/dev/null )" ]]; then
  ccache -s
fi

if [[ -n "$tests" ]]; then
  if [[ "$os" == centos ]]; then
    set +u
    source /opt/rh/rh-python36/enable
    set -u
  fi
  pushd "$build_dir"
    ctest --output-on-failure -E mongo -R "$tests"
  popd
fi

pushd "$build_dir"/packages
  ./generate_package.sh "$pkg_extension"
  # remove generate_*.sh scripts and tarball
  rm -f *.sh *.tar.gz
popd
