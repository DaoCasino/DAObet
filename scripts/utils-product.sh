#
# Product-specific functions.
#

# TODO: docs
is_package_installed() {
  local pkg="${1:?}"
  case "$OS_DISTR_ID" in
  (ubuntu)
    dpkg -l | awk '/^ii/ { print $2; }' | grep -P "^$pkg(:.*)?\$" &>/dev/null
    ;;
  (centos)
    rpm -qa | grep -P "^${pkg}-\d" &>/dev/null # TODO: more precise regexp needed
    ;;
  (darwin)
    die "unimplemented" # TODO
    ;;
  esac
}

# install_os_packages dep1 ...
install_os_packages() {
  # TODO: add need_update parameter
  local -a pkgs_to_install=()
  for p in "$@" ; do
    is_package_installed "$p" || pkgs_to_install+=("$p")
  done
  if [[ "${#pkgs_to_install[@]}" == 0 ]]; then
    log "All dependencies are already installed."
    return
  fi

  log "Installing ${pkgs_to_install[@]} ..."
  case "$OS_DISTR_ID" in
  (ubuntu)
    sudo apt-get update
    sudo apt-get install -V -y "${pkgs_to_install[@]}" # --no-install-suggests --no-install-recommends
    ;;
  (centos)
    sudo yum updateinfo
    sudo yum -y install "${pkgs_to_install[@]}" # TODO: don't install recommended & suggested
    ;;
  (darwin)
    die "unimplemented" # TODO
    ;;
  esac
}

# get available space (in MiB) on a partition, that contains a directory passed in the first argument
get_hdd_available_mib() {
  local path="${1:-.}"
  local hdd_available_kib
  case "$OS_DISTR_ID" in
  (darwin) hdd_available_kib="$( df -k "$path" | tail -1 | awk '{ print $4; }' )" ;;
  (*)      hdd_available_kib="$( df -k -l --output=avail "$path" | tail -1 )" ;;
  esac
  echo "$(( hdd_available_kib / 1024 ))"
}

is_local_clang_installed() {
  [[ -x "$CLANG_ROOT"/bin/clang ]] && [[ -x "$CLANG_ROOT"/bin/clang++ ]]
}

# Exports: CC CXX PATH
# Sets:    CC_LOCAL CXX_LOCAL CMAKE_PINNED_TOOLCHAIN_ARGS
find_compilers() {
  log "Searching for C & C++ compilers to build 3rd-party dependencies & toolchain ..."

  if [[ "$OS_DISTR_ID" == centos ]] || \
      { [[ "$OS_DISTR_ID" == ubuntu ]] && [[ "$OS_DISTR_VERSION" == "16.04" ]]; } || \
      { [[ "$OS_DISTR_ID" == ubuntu ]] && [[ "$OS_DISTR_VERSION" == "18.04" ]] && [[ "$LOCAL_CLANG" == y ]]; }; then
    # TODO: try to remove exports!
    export CC=gcc
    export CXX=g++
  else
    export CC=clang
    export CXX=clang++
  fi
  log "  CC  = $CC"
  log "  CXX = $CXX"

  if is_local_clang_installed ; then
    log "  Clang already installed in $CLANG_ROOT."
  fi
  if [[ "$LOCAL_CLANG" == y ]] || is_local_clang_installed ; then
    CC_LOCAL="$CLANG_ROOT"/bin/clang
    CXX_LOCAL="$CLANG_ROOT"/bin/clang++
    export PATH="$CLANG_ROOT/bin:$PATH"
    CMAKE_PINNED_TOOLCHAIN_ARGS=(-D CMAKE_TOOLCHAIN_FILE="$BUILD_DIR"/pinned_toolchain.cmake)
  else
    is_command_available "$CXX" || \
      die "Unable to find $CXX compiler, use --local-clang option if you wish for us to install it, or " \
        "manually install a C++-17 compiler and set \$CC and \$CXX to the proper binary locations."

    readonly bad_compiler_err="There is no appropriate C++-17 support in your version of compiler ($CXX)."
    case "$CXX" in
    (*clang*)
      local clang_ver_maj
      clang_ver_maj="$( get_version_component "$( get_clang_version )" 1 )"
      if [[ "$OS_DISTR_ID" == darwin ]] && [[ "$clang_ver_maj" -lt 10 ]]; then
        die "$bad_compiler_err"
      fi
      ;;
    (*g++*)
      local gcc_ver_maj
      gcc_ver_maj="$( get_version_component "$( get_gcc_version )" 1 )"
      if [[ "$gcc_ver_maj" -lt 7 ]]; then
        die "$bad_compiler_err"
      fi
      # https://github.com/EOSIO/eos/issues/7402
      warn "Your GCC compiler ($CXX) is less performant than clang (https://github.com/EOSIO/eos/issues/7402). " \
        "We suggest running the build script with --local-clang option, or manually install clang compiler " \
        "and set \$CC and \$CXX to the proper binary locations."
      ;;
    (*)
      die "Unsupported compiler: $CXX."
    esac
  fi

  log "  CC_LOCAL  = $CC_LOCAL"
  log "  CXX_LOCAL = $CXX_LOCAL"
  log "Compilers successfully set up."
}

# Sets: CMAKE_CMD
ensure_cmake() {
  # TODO: install system version from package if appropriate
  if is_command_available cmake "$BIN_DIR" ; then
    log "CMake found in PATH."
    return 0
  fi

  log "CMake executable not found; installing it ..."
  pushd "$SRC_DIR"
    local src_archive="cmake-${CMAKE_VERSION}.tar.gz"
    local maj min
    maj="$( get_version_component "$CMAKE_VERSION" 1 )"
    min="$( get_version_component "$CMAKE_VERSION" 2 )"

    wget -c "https://cmake.org/files/v${maj}.${min}/$src_archive"
    tar -pxf "$src_archive"
    pushd "cmake-$CMAKE_VERSION"
      set -x
      ./bootstrap --prefix="$PREFIX" --parallel="$CPU_CORES"
      make -j "$CPU_CORES"
      make install
      set +x
    popd
    rm -f "$src_archive"
  popd

  #TODO: refactoring needed
  CMAKE_CMD="$PREFIX"/bin/cmake

  log "CMake successfully installed."
}

ensure_llvm() {
  log "Installing Clang toolchain ..."
  if [[ -d "$LLVM_ROOT" ]]; then
    log "LLVM already installed in $LLVM_ROOT."
    return 0
  fi

  if [[ "$OS_DISTR_ID" == darwin ]]; then
    # Handle brew installed llvm@4
    ln -s /usr/local/opt/llvm@4 "$LLVM_ROOT"
    log "  symlink created: $LLVM_ROOT -> /usr/local/opt/llvm@4"
    return 0
  fi

  local -a cmake_flags=(
    -D CMAKE_BUILD_TYPE=Release
    -D CMAKE_INSTALL_PREFIX="$LLVM_ROOT"
    -D LLVM_BUILD_TOOLS=n
    -D LLVM_ENABLE_RTTI=y
    -D LLVM_TARGETS_TO_BUILD=host
    -G 'Unix Makefiles'
  )
  if [[ "$LOCAL_CLANG" == y ]]; then
    cmake_flags+=(-D CMAKE_TOOLCHAIN_FILE="$BUILD_DIR"/pinned_toolchain.cmake)
  else
    if [[ "$OS_DISTR_ID" == ubuntu ]]; then
      ln -s "/usr/lib/llvm-4.0" $LLVM_ROOT
      log "  symlink created: $LLVM_ROOT -> /usr/lib/llvm-4.0"
      return 0
    fi
  fi
  # TODO: verify!!!
  log "Building local LLVM ($LLVM_VERSION) ..."
  pushd "$OPT_DIR"
    git clone --depth 1 --single-branch --branch "$LLVM_VERSION" https://github.com/llvm-mirror/llvm.git llvm
    pushd llvm
      mkdir build
      pushd build
        set -x
        "$CMAKE_CMD" "${cmake_flags[@]}" ..
        make -j "$CPU_CORES"
        make install
        set +x
      popd
    popd
  popd
  log "LLVM successfully installed."
}

# Exports: CPATH
ensure_boost() {
  log "Installing Boost ..."
  if [[ "$OS_DISTR_ID" == darwin ]]; then
    # Boost has trouble with finding pyconfig.h
    CPATH="$( python-config --includes | awk '{print $1}' | cut -dI -f2 ):$CPATH"
    export CPATH
  fi
  local boost_version=
  if [[ -f "$BOOST_ROOT"/include/boost/version.hpp ]]; then
    boost_version="$( grep -P '\s*#\s*define\s+BOOST_VERSION\b' "$BOOST_ROOT"/include/boost/version.hpp | grep -Po '\d+' )"
  fi
  if [[ "$boost_version" == "$BOOST_VERSION_INTEGER" ]]; then
    log "Boost already installed in $BOOST_ROOT."
    return 0
  fi

  local -a b2_flags=(-q -j"$CPU_CORES" --with-iostreams --with-date_time --with-filesystem --with-system --with-program_options --with-chrono --with-test)
  local -a bootstrap_flags=()
  if [[ "$OS_NAME" == Linux ]] && [[ "$LOCAL_CLANG" == y ]]; then
    b2_flags+=(toolset=clang cxxflags="-stdlib=libc++ -D__STRICT_ANSI__ -nostdinc++ -I$CLANG_ROOT/include/c++/v1"
      linkflags="-stdlib=libc++" link=static threading=multi)
    bootstrap_flags+=(--with-toolset=clang)
  fi

  pushd "$SRC_DIR"
    local boost_archive="boost_$BOOST_VERSION.tar.bz2"
    wget -c "https://dl.bintray.com/boostorg/release/$BOOST_VERSION_MAJOR.$BOOST_VERSION_MINOR.$BOOST_VERSION_PATCH/source/$boost_archive"
    tar -pxf "$boost_archive"
    pushd "$BOOST_ROOT"
      set -x
      # TODO: there is a bug in bash:
      # https://git.savannah.gnu.org/cgit/bash.git/tree/CHANGES?id=3ba697465bc74fab513a26dea700cc82e9f4724e#n878
      # Using this ugly version until upgrading to bash-4.2 to >=bash-4.4 in centos-7.
      #./bootstrap.sh "${bootstrap_flags[@]}" --prefix="$BOOST_ROOT"
      ./bootstrap.sh "${bootstrap_flags[@]+"${bootstrap_flags[@]}"}" --prefix="$BOOST_ROOT"
      ./b2 "${b2_flags[@]}" install
      set +x
    popd
    rm -f "$boost_archive"
    rm -rf "$BOOST_LINK_DIR"
  popd
  log "Boost library successfully installed in $BOOST_ROOT."
}

install_clang_and_set_cc_cxx() {
  log "Installing Clang toolchain ..."
  if [[ ! -d "$CLANG_ROOT" ]]; then
    pushd "$SRC_DIR"
      local -a git_clone_opts=(--single-branch --branch "$PINNED_COMPILER_BRANCH")
      rm -rf clang8
      git clone "${git_clone_opts[@]}" https://git.llvm.org/git/llvm.git clang8
      pushd clang8
        git checkout "$PINNED_COMPILER_LLVM_COMMIT"
        pushd tools
          git clone "${git_clone_opts[@]}" https://git.llvm.org/git/lld.git
          pushd lld
            git checkout "$PINNED_COMPILER_LLD_COMMIT"
          popd
          git clone "${git_clone_opts[@]}" https://git.llvm.org/git/polly.git
          pushd polly
            git checkout "$PINNED_COMPILER_POLLY_COMMIT"
          popd
          git clone "${git_clone_opts[@]}" https://git.llvm.org/git/clang.git clang
          pushd clang
            git checkout "$PINNED_COMPILER_CLANG_COMMIT"
            patch -p2 -i "$REPO_ROOT"/scripts/clang-devtoolset8-support.patch
            pushd tools
              mkdir extra
              pushd extra
                git clone "${git_clone_opts[@]}" https://git.llvm.org/git/clang-tools-extra.git
                pushd clang-tools-extra
                  git checkout "$PINNED_COMPILER_CLANG_TOOLS_EXTRA_COMMIT"
                popd
              popd
            popd
          popd
        popd

        pushd projects
          git clone "${git_clone_opts[@]}" https://git.llvm.org/git/libcxx.git
          pushd libcxx
            git checkout "$PINNED_COMPILER_LIBCXX_COMMIT"
          popd
          git clone "${git_clone_opts[@]}" https://git.llvm.org/git/libcxxabi.git
          pushd libcxxabi
            git checkout "$PINNED_COMPILER_LIBCXXABI_COMMIT"
          popd
          git clone "${git_clone_opts[@]}" https://git.llvm.org/git/libunwind.git
          pushd libunwind
            git checkout "$PINNED_COMPILER_LIBUNWIND_COMMIT"
          popd
          git clone "${git_clone_opts[@]}" https://git.llvm.org/git/compiler-rt.git
          pushd compiler-rt
            git checkout "$PINNED_COMPILER_COMPILER_RT_COMMIT"
          popd
          pushd "$SRC_DIR"/clang8
            mkdir build
            pushd build
              set -x
              "$CMAKE_CMD" \
                -D CMAKE_BUILD_TYPE=Release \
                -D CMAKE_INSTALL_PREFIX="$CLANG_ROOT" \
                -D LLVM_BUILD_EXTERNAL_COMPILER_RT=y \
                -D LLVM_BUILD_LLVM_DYLIB=y \
                -D LLVM_ENABLE_LIBCXX=y \
                -D LLVM_ENABLE_RTTI=y \
                -D LLVM_INCLUDE_DOCS=n \
                -D LLVM_OPTIMIZED_TABLEGEN=y \
                -D LLVM_TARGETS_TO_BUILD=all \
                -G "Unix Makefiles" \
                ..
              make -j "$CPU_CORES"
              make install
              set +x
            popd
          popd
        popd
      popd
    popd
    rm -rf "$SRC_DIR"/clang8

    log "Clang successfully installed in $CLANG_ROOT."
  else
    log "Clang already installed in $CLANG_ROOT."
  fi

  # TODO: exports
  export CC="${CC_LOCAL:?"CC_LOCAL is not set while it should be"}"
  export CXX="${CXX_LOCAL:?"CXX_LOCAL is not set while it should be"}"
  log "CC  = $CC"
  log "CXX = $CXX"
}

# TODO: verify!!!
install_mongo() {
  # TODO: support darwin!!!
  log "Installing MongoDB ..."

  if [[ -x "$BIN_DIR"/mongod ]]; then
    log "MongoDB already installed in $MONGODB_ROOT."
  else
    pushd "$SRC_DIR"

      case "$OS_DISTR_ID" in
      (ubuntu)
        local ubuntu_mongo_ver
        ubuntu_mongo_ver="$( echo "$OS_DISTR_VERSION" | tr -d '.' )"
        local mongodb_src_dir="mongodb-linux-x86_64-ubuntu${ubuntu_mongo_ver}-${MONGODB_VERSION}"
        local mongodb_archive="$mongodb_src_dir.tgz"
        local mongo_url="http://downloads.mongodb.org/linux/$mongodb_archive"
        ;;
      (centos)
        local mongodb_src_dir="mongodb-linux-x86_64-amazon-$MONGODB_VERSION"
        local mongodb_archive="$mongodb_src_dir.tgz"
        local mongo_url="https://fastdl.mongodb.org/linux/mongodb-linux-x86_64-amazon-$MONGODB_VERSION.tgz"
        ;;
      esac

      wget -c "$mongo_url"
      tar -pxf "$mongodb_archive"
      mv "$SRC_DIR/$mongodb_src_dir" "$MONGODB_ROOT"
      mkdir -p "${MONGODB_LOG_DIR}"
      touch "$MONGODB_LOG_DIR"/mongod.log
      rm -f "$mongodb_archive"
      mkdir -p "${MONGODB_CONF%/*}"
      cp -f "$REPO_ROOT"/scripts/mongod.conf "$MONGODB_CONF"
      mkdir -p "$MONGODB_DATA_DIR"
      rm -rf "$MONGODB_LINK_DIR" "$BIN_DIR"/mongod
      mkdir -p "$BIN_DIR"
      ln -s "$MONGODB_ROOT" "$MONGODB_LINK_DIR"
      ln -s "$MONGODB_LINK_DIR"/bin/mongod "$BIN_DIR"/mongod
    popd
    log "MongoDB successfully installed in $MONGODB_ROOT (symlinked to $MONGODB_LINK_DIR)."
  fi

  log "Installing MongoDB C driver ..."
  if [[ -d "$MONGO_C_DRIVER_ROOT" ]]; then
    log "MongoDB C driver already installed in $MONGO_C_DRIVER_ROOT."
  else
    pushd "$SRC_DIR"
      local c_drv_src_dir="mongo-c-driver-$MONGO_C_DRIVER_VERSION"
      local c_drv_archive="$c_drv_src_dir.tar.gz"

      wget -c "https://github.com/mongodb/mongo-c-driver/releases/download/$MONGO_C_DRIVER_VERSION/$c_drv_archive"
      tar -pxf "$c_drv_archive"
      pushd "$c_drv_src_dir"
        mkdir -p cmake-build
        pushd cmake-build
          set -x
          # TODO: check ENABLE_SSL type; fix to 'y'/'n' if bool
          "$CMAKE_CMD" \
            -D CMAKE_BUILD_TYPE=Release \
            -D CMAKE_INSTALL_PREFIX="$PREFIX" \
            -D ENABLE_AUTOMATIC_INIT_AND_CLEANUP=n \
            -D ENABLE_BSON=ON \
            -D ENABLE_ICU=OFF \
            -D ENABLE_SNAPPY=OFF \
            -D ENABLE_SSL=OPENSSL \
            -D ENABLE_STATIC=ON \
            "${CMAKE_PINNED_TOOLCHAIN_ARGS[@]}" \
            ..
          make -j "$CPU_CORES"
          make install
          set +x
        popd
      popd
      rm -f "$c_drv_archive"
      log "MongoDB C driver successfully installed in $MONGO_C_DRIVER_ROOT."
  fi

  log "Installing MongoDB C++ driver ..."
  if [[ -d "$MONGO_CXX_DRIVER_ROOT" ]]; then
    log "MongoDB C++ driver already installed in $MONGO_CXX_DRIVER_ROOT."
  else
    pushd "$SRC_DIR"
    local cxx_drv_src_dir="mongo-cxx-driver-r$MONGO_CXX_DRIVER_VERSION"
    local cxx_drv_archive="$cxx_drv_src_dir.tar.gz"

    wget -c -O "$cxx_drv_archive" "https://github.com/mongodb/mongo-cxx-driver/archive/r$MONGO_CXX_DRIVER_VERSION.tar.gz"
    tar -pxf "$cxx_drv_archive"
    pushd "$cxx_drv_src_dir"
      sed -i 's/\"maxAwaitTimeMS\", count/\"maxAwaitTimeMS\", static_cast<int64_t>(count)/' src/mongocxx/options/change_stream.cpp
      sed -i 's/add_subdirectory(test)//' src/mongocxx/CMakeLists.txt src/bsoncxx/CMakeLists.txt
      pushd build
        "$CMAKE_CMD" \
          -D BUILD_SHARED_LIBS=OFF \
          -D CMAKE_BUILD_TYPE=Release \
          -D CMAKE_INSTALL_PREFIX="$PREFIX" \
          -D CMAKE_PREFIX_PATH="$PREFIX" \
          "${CMAKE_PINNED_TOOLCHAIN_ARGS[@]}" \
          ..
        make -j "$CPU_CORES"
        make install
      popd
    popd
    rm -f "$cxx_drv_archive"
    log "MongoDB C++ driver successfully installed in ${MONGO_CXX_DRIVER_ROOT}."
  fi
}
