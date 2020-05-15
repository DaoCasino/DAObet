#
# Build configuration.
# This file should be sourced after utils.sh.
#

#----- paths -----#

# sources & build paths
REPO_ROOT="$(readlink -f "$PROGPATH"/..)"
readonly REPO_ROOT

readonly BUILD_DIR="$REPO_ROOT/build"

# installation paths
PREFIX="${PREFIX:-"$HOME/$PRODUCT_NAME-build"}"
readonly BIN_DIR="$PREFIX"/bin
readonly DATA_DIR="$PREFIX"/data
readonly ETC_DIR="$PREFIX"/etc
readonly OPT_DIR="$PREFIX"/opt
readonly SRC_DIR="$PREFIX"/src
readonly VAR_DIR="$PREFIX"/var

# Local Clang installation # TODO: remove
CC_LOCAL=
CXX_LOCAL=

#----- platform settings -----#

CPU_CORES="$(getconf _NPROCESSORS_ONLN || sysctl -n hw.ncpu)"
readonly CPU_CORES

readonly HDD_MIN_AVAILABLE_MIB=7000

# "Linux" or "Darwin" supported
readonly OS_NAME="$( uname )"

# "Ubuntu", "CentOS Linux", "Mac OS X", ...
OS_DISTR_NAME=
# "18.04", "10.12.6", "7", ...
OS_DISTR_VERSION=
case "$OS_NAME" in
(Linux)
  OS_DISTR_NAME="$( get_gnu_os_distrib_name )"
  OS_DISTR_VERSION="$( get_gnu_os_distrib_version || true )"
  ;;
(Darwin)
  OS_DISTR_NAME="$( sw_vers -productName )"
  OS_DISTR_VERSION="$( sw_vers -productVersion )"
  ;;
(*)
  die "Unsupported OS: $OS_NAME."
  ;;
esac
readonly OS_DISTR_NAME OS_DISTR_VERSION

# "darwin", "ubuntu", "centos", ...
OS_DISTR_ID=
case "$OS_NAME" in
(Darwin) OS_DISTR_ID=darwin ;;
(*)      OS_DISTR_ID="$( get_gnu_os_id )" ;;
esac
readonly OS_DISTR_ID

#----- 3rd-party dependencies -----#

readonly CMAKE_VERSION=3.13.2

if [[ "$OS_DISTR_ID" == ubuntu ]] && [[ "$( get_version_component "$OS_DISTR_VERSION" 1 )" == 18 ]]; then
  # Ubuntu-18 doesn't have MongoDB-3.6.3
  readonly MONGODB_VERSION=4.1.1
else
  readonly MONGODB_VERSION=3.6.3
fi
readonly MONGODB_ROOT="$OPT_DIR/mongodb-$MONGODB_VERSION"
readonly MONGODB_CONF="$ETC_DIR"/mongod.conf
readonly MONGODB_LOG_DIR="$VAR_DIR"/log/mongodb
readonly MONGODB_LINK_DIR="$OPT_DIR"/mongodb
readonly MONGODB_DATA_DIR="$DATA_DIR"/mongodb
readonly MONGO_C_DRIVER_VERSION=1.13.0
readonly MONGO_C_DRIVER_ROOT="$SRC_DIR/mongo-c-driver-$MONGO_C_DRIVER_VERSION"
readonly MONGO_CXX_DRIVER_VERSION=3.4.0
readonly MONGO_CXX_DRIVER_ROOT="$SRC_DIR/mongo-cxx-driver-r$MONGO_CXX_DRIVER_VERSION"

readonly BOOST_VERSION_MAJOR=1
readonly BOOST_VERSION_MINOR=70
readonly BOOST_VERSION_PATCH=0
readonly BOOST_VERSION_INTEGER=107000
readonly BOOST_VERSION="${BOOST_VERSION_MAJOR}_${BOOST_VERSION_MINOR}_${BOOST_VERSION_PATCH}"
readonly BOOST_ROOT="${BOOST_ROOT:-"$SRC_DIR/boost_${BOOST_VERSION}"}" # TODO
readonly BOOST_LINK_DIR="$OPT_DIR/boost" # TODO

readonly LLVM_VERSION=release_40
readonly LLVM_ROOT="$OPT_DIR"/llvm

readonly DOXYGEN_VERSION=1_8_14
readonly DOXYGEN_ROOT="$SRC_DIR/doxygen-$DOXYGEN_VERSION"

readonly CLANG_ROOT="$OPT_DIR"/clang8
readonly CLANG_VERSION=8.0.1
readonly PINNED_COMPILER_BRANCH=release_80
readonly PINNED_COMPILER_LLVM_COMMIT=18e41dc
readonly PINNED_COMPILER_CLANG_COMMIT=a03da8b
readonly PINNED_COMPILER_LLD_COMMIT=d60a035
readonly PINNED_COMPILER_POLLY_COMMIT=1bc06e5
readonly PINNED_COMPILER_CLANG_TOOLS_EXTRA_COMMIT=6b34834
readonly PINNED_COMPILER_LIBCXX_COMMIT=1853712
readonly PINNED_COMPILER_LIBCXXABI_COMMIT=d7338a4
readonly PINNED_COMPILER_LIBUNWIND_COMMIT=57f6739
readonly PINNED_COMPILER_COMPILER_RT_COMMIT=5bc7979
