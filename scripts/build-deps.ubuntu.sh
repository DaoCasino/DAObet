case "$OS_DISTR_VERSION" in
(18.04) ;;
(16.04) warn "Ubuntu-16.04 is not officially supported." ;;
(*)     die "Only Ubuntu 18.04 or higher is supported." ;;
esac

# TODO: remove unused
deps=()
deps+=(autoconf)
deps+=(automake)
deps+=(autotools-dev)
deps+=(build-essential)
deps+=(bzip2)
# deps+=(curl)
deps+=(doxygen)
# deps+=(gettext-base) -- FIXME: install this before calling envsubst!!!
deps+=(git)
deps+=(graphviz)
deps+=(libbz2-dev)
deps+=(libcurl4-gnutls-dev)
deps+=(libgmp3-dev)
deps+=(libicu-dev)
deps+=(libssl-dev)
deps+=(libtool)
deps+=(libusb-1.0-0-dev)
deps+=(patch)
deps+=(pkg-config)
deps+=(python2.7)
deps+=(python2.7-dev)
deps+=(python3)
deps+=(python3-dev)
deps+=(wget)
deps+=(zlib1g-dev)
#deps+=(ruby)

# system clang and build essential for Ubuntu 18 (16 is too old)
if [[ "$OS_DISTR_VERSION" == "18.04" ]] && [[ "$LOCAL_CLANG" == n ]]; then
  deps+=(clang-8)
fi

# TODO: why not newer version??
if [[ "$LOCAL_CLANG" == n ]]; then
  deps+=(llvm-4.0) # llvm-8 or llvm-9
fi

if [[ "$enable_cov_tests" ]]; then
  deps+=(lcov)
fi

install_os_packages "${deps[@]}"

if [[ "$OS_DISTR_VERSION" == "18.04" ]] && [[ "$LOCAL_CLANG" == n ]]; then
  log "Checking clang & clang++ commands are available and point to Clang-8 compiler ..."
  if [[ "$( get_version_component "$( get_clang_version )" 1 )" != 8 ]]; then
    sudo update-alternatives --install /usr/bin/clang clang /usr/bin/clang-8 100
    sudo update-alternatives --install /usr/bin/clang++ clang++ /usr/bin/clang++-8 100
  fi
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
