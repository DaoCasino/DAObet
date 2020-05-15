if [[ "$OS_DISTR_VERSION" -lt 7 ]]; then
  die "Only CentOS 7 or higher is supported."
fi

# TODO: remove unused: libX*, *-fonts, xorg-*, mesa-*, adobe-*, graphviz, gtk*, ... !!!
deps=()
deps+=(autoconf)
deps+=(automake)
deps+=(bzip2)
deps+=(bzip2-devel)
deps+=(centos-release-scl) # for rh-python3 and devtoolset-8
deps+=(devtoolset-8) # gcc-8 for centos (for Cmake installation)
deps+=(doxygen)
deps+=(file)
deps+=(gettext-devel)
deps+=(git)
deps+=(gmp-devel)
deps+=(graphviz)
deps+=(libcurl-devel)
deps+=(libicu-devel)
deps+=(libtool)
deps+=(libusbx-devel)
deps+=(make)
deps+=(ocaml)
deps+=(openssl-devel)
deps+=(patch)
deps+=(python)
deps+=(python-devel)
deps+=(rh-python36)
deps+=(wget)

install_os_packages "${deps[@]}"

if [[ -d /opt/rh/devtoolset-8 ]]; then
	log "Enabling devtoolset-8 (for gcc-8) ..."
  set +u
	. /opt/rh/devtoolset-8/enable
  set -u
  log "Done."
fi

# TODO: remove export!!!
export PYTHON3PATH="/opt/rh/rh-python36"
if [[ -d "$PYTHON3PATH" ]]; then
	log "Enabling python36 ..."
  set +u
	. $PYTHON3PATH/enable
  set -u
	log "Done."
fi

find_compilers
ensure_cmake
if [[ "$LOCAL_CLANG" == y ]]; then
  install_clang_and_set_cc_cxx
fi
ensure_llvm
ensure_boost

if [[ "$INSTALL_MONGO" == y ]]; then
  install_mongo
fi

# TODO:
#~if [[ "$OS_DISTR_VERSION" == "18.04" ]] && [[ "$LOCAL_CLANG" == n ]]; then
#~  log "Checking clang & clang++ commands are available and point to Clang-8 compiler ..."
#~  if [[ "$( get_version_component "$( get_clang_version )" 1 )" != 8 ]]; then
#~    sudo update-alternatives --install /usr/bin/clang clang /usr/bin/clang-8 100
#~    sudo update-alternatives --install /usr/bin/clang++ clang++ /usr/bin/clang++-8 100
#~  fi
#~fi
