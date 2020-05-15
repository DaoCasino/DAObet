#!/bin/bash

set -eu
set -o pipefail

. "${BASH_SOURCE[0]%/*}/utils.sh"
. "$PROGPATH/config-product.sh"
. "$PROGPATH/config.sh"
. "$PROGPATH/utils-product.sh"

CMAKE_PINNED_TOOLCHAIN_ARGS=()

#---------- command-line options ----------#

no_checks=n
build_type=RelWithDebInfo
enable_mongo=n
INSTALL_MONGO=n
enable_doxygen=n
enable_cov_tests=n
LOCAL_CLANG=n
deps_only=n
verbose=n

usage() {
  echo "Install and build node dependencies, and build node itself."
  echo
  echo "Usage:"
  echo
  echo "  $PROGNAME [ OPTIONS ]"
  echo
  echo "Options:"
  echo
  echo "  --prefix <path>      : set installation root ($PREFIX by default)"
  echo "  --build-type <type>  : build type: Debug, Release, RelWithDebInfo, or MinSizeRel;"
  echo "                         default: $build_type"
  echo "  --enable-<COMPONENT> : enable component: mongo, doxygen, or coverage-testing"
  echo "  --install-mongo      : install mongo"
  echo "  --local-clang        : build and use a partucular version of Clang toolchain locally"
  echo "  --deps-only          : only prepare $PRODUCT_NAME_OFFICIAL dependencies, not node's code itself"
  echo "  --verbose            : verbose build"
  echo
  echo "  --no-checks          : omit some strict checks (for advanced users)"
  echo
  echo "  -h, --help           : print this message"
}

OPTS=$( getopt -o "h" -l "\
prefix:,\
build-type:,\
enable-mongo,\
enable-doxygen,\
enable-coverage-testing,\
install-mongo,\
local-clang,\
deps-only,\
verbose,\
no-checks,\
help" -n "$PROGNAME" -- "$@" )
eval set -- "$OPTS"
while true; do
  case "${1:-}" in
  (--prefix)                  PREFIX="$2"        ; shift 2 ; readonly PREFIX ;;
  (--build-type)              build_type="$2"    ; shift 2 ; readonly build_type ;;
  (--enable-mongo)            enable_mongo=y     ; shift   ; readonly enable_mongo ;;
  (--install-mongo)           INSTALL_MONGO=y    ; shift   ; readonly INSTALL_MONGO ;;
  (--enable-doxygen)          enable_doxygen=y   ; shift   ; readonly enable_doxygen ;;
  (--enable-coverage-testing) enable_cov_tests=y ; shift   ; readonly enable_cov_tests ;;
  (--local-clang)             LOCAL_CLANG=y      ; shift   ; readonly LOCAL_CLANG ;;
  (--deps-only)               deps_only=y        ; shift   ; readonly deps_only ;;
  (--verbose)                 verbose=y          ; shift   ; readonly verbose ;;
  (--no-checks)               no_checks=y        ; shift   ; readonly no_checks ;;
  (-h|--help)                 usage ; exit 0 ;;
  (--)                        shift ; break ;;
  (*)                         die "Invalid option: ${1:-}." ;;
  esac
done
unset OPTS

#---------- helpers ----------#

preflight_checks() {
  log "Performing some preflignt checks ..."

  case "$OS_DISTR_NAME" in
  ("Ubuntu" | "CentOS Linux") ;;
  (*) warn_or_die "$no_checks" "Unsupported OS distribution: $OS_DISTR_NAME." ;;
  esac

  case "$PRODUCT_NAME" in
  (haya|daobet) ;;
  (*) die "Invalid product name: $PRODUCT_NAME" ;;
  esac

  # HDD
  local hdd_available_mib
  hdd_available_mib="$( get_hdd_available_mib "$PREFIX" )"
  [[ "$hdd_available_mib" -gt "$HDD_MIN_AVAILABLE_MIB" ]] || \
    die "Not enough space on the current partition; you have: $hdd_available_mib MiB," \
      "required: $HDD_MIN_AVAILABLE_MIB MiB."

  # TODO: add RAM check if needed
}

#---------- main ----------#

CMAKE_CMD=
if [[ -x "$PREFIX"/bin/cmake ]]; then
  CMAKE_CMD="$PREFIX"/bin/cmake
elif is_command_available cmake ; then
  CMAKE_CMD="$( command -v cmake )"
fi

cmake_prefix_path="$PREFIX"
if [[ "$OS_DISTR_ID" == darwin ]]; then
  cmake_prefix_path="/usr/local/opt/gettext;$PREFIX"
fi
platform_script="build-deps.${OS_DISTR_ID}.sh"

log "Configuration:"
log
log "  product name                 = $PRODUCT_NAME"
log "  core symbol name             = $CORE_SYMBOL_NAME"
log
log "  OS                           = $OS_DISTR_NAME-$OS_DISTR_VERSION, $OS_NAME"
log "  installation prefix          = $PREFIX"
log "  # of available CPU cores     = $CPU_CORES"
log
log "  build type                   = $build_type"
log "  cmake executable             = ${CMAKE_CMD:-"<not found>"}"
log "  boost root                   = $BOOST_ROOT"
log
log "  enable coverage tests        = $enable_cov_tests"
log "  enable Mongo                 = $enable_mongo"
log "  install Mongo                = $INSTALL_MONGO"
log "  enable Doxygen               = $enable_doxygen"
log "  use local Clang installation = $LOCAL_CLANG"
log "  prepare dependencies only    = $deps_only"
log
log "  verbose                      = $verbose"
if [[ "$no_checks" == y ]]; then
  log
  warn "Strict checks disabled."
fi
log

mkdir -p "$BUILD_DIR" "$PREFIX"
preflight_checks

log "Creating base directory structure in $PREFIX ..."
mkdir -p "$OPT_DIR" "$SRC_DIR"
if [[ "$enable_mongo" == y ]]; then
  mkdir -p "$MONGODB_LOG_DIR" "$MONGODB_DATA_DIR"
fi

env __opt_dir__="$OPT_DIR" \
  envsubst '$__opt_dir__' < "$PROGPATH"/pinned_toolchain.cmake > "$BUILD_DIR"/pinned_toolchain.cmake

log
log "Installing & building dependencies ..."
log
. "$PROGPATH/$platform_script"

if [[ "$deps_only" == y ]]; then
  log "Dependencies successfully installed, nothing to do more."
  exit
fi

log
log "Building $PRODUCT_NAME_OFFICIAL ..."
log
log "Using compilers:"
log "  CC  = $CC\t($( which "$CC" ))"
log "  CXX = $CXX\t($( which "$CXX" ))"
log

cmake_additional_flags=()
[[ "$enable_mongo" == n ]]     || cmake_additional_flags+=(-D BUILD_MONGO_DB_PLUGIN=y)
[[ "$enable_doxygen" == n ]]   || cmake_additional_flags+=(-D BUILD_DOXYGEN=y)
[[ "$enable_cov_tests" == n ]] || cmake_additional_flags+=(-D ENABLE_COVERAGE_TESTING=y)

if [[ -d "$LLVM_ROOT" ]]; then
  cmake_prefix_path="${cmake_prefix_path};${LLVM_ROOT}"
fi
cmake_additional_flags+=(-D CMAKE_PREFIX_PATH="$cmake_prefix_path")
if is_local_clang_installed; then
  cmake_additional_flags+=("${CMAKE_PINNED_TOOLCHAIN_ARGS[@]+"${CMAKE_PINNED_TOOLCHAIN_ARGS[@]}"}") # TODO: fix array expansion on >=centos-8
else
  cmake_additional_flags+=(-D CMAKE_CXX_COMPILER="$CXX" -D CMAKE_C_COMPILER="$CC")
fi

make_flags=(-j "$CPU_CORES")
if [[ "$verbose" == y ]]; then
  make_flags+=(VERBOSE=1)
fi
pushd "$BUILD_DIR"
  set -x
  "$CMAKE_CMD" \
    -D BOOST_ROOT="$BOOST_ROOT" \
    -D CMAKE_BUILD_TYPE="$build_type" \
    -D CMAKE_INSTALL_PREFIX="$PREFIX" \
    -D CORE_SYMBOL_NAME="$CORE_SYMBOL_NAME" \
    -D ENABLE_TX_SPONSORSHIP=1 \
    "${cmake_additional_flags[@]}" \
    "$REPO_ROOT"
  make "${make_flags[@]}"
  set +x
popd

log "$PRODUCT_NAME_OFFICIAL successfully built."

#TODO
#~echo "${COLOR_CYAN}If you wish to perform tests to ensure functional code:${COLOR_NC}"
#~if $ENABLE_MONGO; then
#~   echo "${BIN_DIR}/mongod --dbpath ${MONGODB_DATA_DIR} -f ${MONGODB_CONF} --logpath ${MONGODB_LOG_DIR}/mongod.log &"
#~   PATH_TO_USE=" PATH=\$PATH:$OPT_DIR/mongodb/bin"
#~fi
#~echo "cd ${BUILD_DIR} && ${PATH_TO_USE} make test" # PATH is set as currently 'mongo' binary is required for the mongodb test
