cmake_minimum_required(VERSION 3.8)

# load some useful stuff (including project settings detection)
list(APPEND CMAKE_MODULE_PATH
  "${CMAKE_CURRENT_SOURCE_DIR}/libraries/fc/CMakeModules"
  "${CMAKE_CURRENT_SOURCE_DIR}/CMakeModules"
)
include(utils)
get_current_project_conf(
  PROJECT
  default_core_sym_name
  PROJECT_URL
  PKG_NAME
  PKG_VENDOR
  PKG_CONTACT
)

###

project("${PROJECT}")

###

if(NOT UNIX)
  message(FATAL_ERROR "Unsupported platform; use GNU/Linux or Apple instead.")
endif()

#TODO: try to replace with `enable_testing()`
include(CTest) # suppresses DartConfiguration.tcl error

find_program(CCACHE_FOUND ccache)
if(CCACHE_FOUND)
  message(WARNING "CCACHE FOUND")
  set(CMAKE_CXX_COMPILER_LAUNCHER ccache)
  set(CMAKE_C_COMPILER_LAUNCHER ccache)
endif()

if("${CMAKE_INSTALL_PREFIX}" STREQUAL "/usr/local")
  message(WARNING "CMAKE_INSTALL_PREFIX is set to /usr/local. This is not recommended.")
endif()

if(APPLE)
  list(APPEND CMAKE_PREFIX_PATH "/usr/local/opt/llvm@4" "/usr/local/opt/gettext")
endif()

include(GNUInstallDirs)
include(VersionMacros)
include(SetupTargetMacros)
include(InstallDirectoryPermissions)
include(MASSigning)

#---------- configuration ----------#
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_EXTENSIONS ON)
set(CXX_STANDARD_REQUIRED ON)

# TODO: rename: CLI_BINARY_NAME, NODE_BINARY_NAME, WALLET_BINARY_NAME
set(CLI_CLIENT_EXECUTABLE_NAME "${PROJECT}-cli")      # haya-cli | daobet-cli
set(NODE_EXECUTABLE_NAME       "${PROJECT}-node")     # haya-node | daobet-node
set(KEY_STORE_EXECUTABLE_NAME  "${PROJECT}-wallet")   # haya-wallet | daobet-wallet
set(LAUNCHER_EXECUTABLE_NAME   "${PROJECT}-launcher") # haya-launcher | daobet-launcher
set(CUSTOM_URL_SCHEME "gcs")
set(INSTALLER_APP_ID "68ad7005-8eee-49c9-95ce-9eed97e5b347")

#---------- version ----------#
get_version_from_git(VERSION_FULL VERSION_DIRTY)

# set VERSION_MAJOR, VERSION_MINOR, VERSION_PATCH
get_version_component("${VERSION_FULL}" 0 VERSION_MAJOR)
get_version_component("${VERSION_FULL}" 1 VERSION_MINOR)
get_version_component("${VERSION_FULL}" 2 VERSION_PATCH)

message(STATUS "${PROJECT} version: ${VERSION_FULL} (semver), ${VERSION_DIRTY} (git)")
message(STATUS "  VERSION_MAJOR = ${VERSION_MAJOR}")
message(STATUS "  VERSION_MINOR = ${VERSION_MINOR}")
message(STATUS "  VERSION_PATCH = ${VERSION_PATCH}")

# http://stackoverflow.com/a/18369825
if("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")
  if(CMAKE_CXX_COMPILER_VERSION VERSION_LESS 7.0)
    message(FATAL_ERROR "GCC version must be at least 7.0!")
  endif()
  if("${CMAKE_GENERATOR}" STREQUAL "Ninja")
    add_compile_options(-fdiagnostics-color=always)
  endif()
elseif("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang" OR "${CMAKE_CXX_COMPILER_ID}" STREQUAL "AppleClang")
  if(CMAKE_CXX_COMPILER_VERSION VERSION_LESS 5.0)
    message(FATAL_ERROR "Clang version must be at least 5.0!")
  endif()
  if("${CMAKE_GENERATOR}" STREQUAL "Ninja")
    add_compile_options(-fcolor-diagnostics)
  endif()
endif()

set(CMAKE_EXPORT_COMPILE_COMMANDS "ON")
set(BUILD_DOXYGEN FALSE CACHE BOOL "Build doxygen documentation on every make")
set(BUILD_MONGO_DB_PLUGIN FALSE CACHE BOOL "Build mongo database plugin")
set(ENABLE_MULTIVERSION_PROTOCOL_TEST FALSE CACHE BOOL "Enable nodeos multiversion protocol test")

# add defaults for openssl
if("${OPENSSL_ROOT_DIR}" STREQUAL "")
  if(NOT "$ENV{OPENSSL_ROOT_DIR}" STREQUAL "")
    set(OPENSSL_ROOT_DIR $ENV{OPENSSL_ROOT_DIR})
    set(OPENSSL_INCLUDE_DIR ${OPENSSL_ROOT_DIR}/include)
  elseif(APPLE)
    set(OPENSSL_ROOT_DIR "/usr/local/opt/openssl@1.1")
    set(OPENSSL_INCLUDE_DIR "/usr/local/opt/openssl@1.1/include")
  else() # GNU
    set(OPENSSL_ROOT_DIR "/usr/include/openssl")
    set(OPENSSL_INCLUDE_DIR "/usr/include/openssl/include")
  endif()
endif()

if(APPLE)
  set(whole_archive_flag "-force_load")
  set(no_whole_archive_flag "")
else()
  set(whole_archive_flag "--whole-archive")
  set(no_whole_archive_flag "--no-whole-archive")
endif()

set(Boost_USE_STATIC_LIBS ON CACHE STRING "ON or OFF")
set(Boost_NO_BOOST_CMAKE ON CACHE STRING "ON or OFF") #TODO: Remove after Boost 1.70 CMake fixes are in place
find_package(Boost 1.67 REQUIRED COMPONENTS
  date_time
  filesystem
  system
  program_options
  chrono
  unit_test_framework
  iostreams
)

# Some new stdlibc++s will #error on <experimental/string_view>; a problem for boost pre-1.69
if(APPLE)
   add_definitions(-DBOOST_ASIO_DISABLE_STD_EXPERIMENTAL_STRING_VIEW)
endif()

set(CMAKE_THREAD_PREFER_PTHREAD TRUE)
set(THREADS_PREFER_PTHREAD_FLAG TRUE)
find_package(Threads)
link_libraries(Threads::Threads)

if(APPLE)
  # Apple Specific Options Here
  message(STATUS "Configuring ${PROJECT} on OS X")
  set(CMAKE_CXX_FLAGS "${CMAKE_C_FLAGS} ${CMAKE_CXX_FLAGS} -Wall -Wno-deprecated-declarations")
else() # GNU
  # Linux Specific Options Here
  message(STATUS "Configuring ${PROJECT} on Linux")
  set(CMAKE_CXX_FLAGS "${CMAKE_C_FLAGS} ${CMAKE_CXX_FLAGS} -Wall")
  if(FULL_STATIC_BUILD)
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -static-libstdc++ -static-libgcc")
  endif()

  if("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang")
    if(CMAKE_CXX_COMPILER_VERSION VERSION_GREATER_EQUAL 4.0.0)
      set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-invalid-partial-specialization")
    endif()
  endif()
endif()

if("${CMAKE_GENERATOR}" STREQUAL "Ninja")
    if("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang")
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fcolor-diagnostics")
    endif()
endif()

# based on http://www.delorie.com/gnu/docs/gdb/gdb_70.html
# uncomment this line to tell GDB about macros (slows compile times)
# set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -gdwarf-2 -g3")

set(ENABLE_COVERAGE_TESTING FALSE CACHE BOOL "Build ${PROJECT} for code coverage analysis")

if(ENABLE_COVERAGE_TESTING)
  set(CMAKE_CXX_FLAGS "--coverage ${CMAKE_CXX_FLAGS}")
  find_program(LCOV_PATH lcov)
  find_program(LLVMCOV_PATH llvm-cov)
  find_program(GENHTML_PATH NAMES genhtml)
endif()

if(ENABLE_TX_SPONSORSHIP)
  add_definitions(-DENABLE_TX_SPONSORSHIP)
endif()

###

if("${CORE_SYMBOL_NAME}" STREQUAL "")
  set(CORE_SYMBOL_NAME "${default_core_sym_name}")
endif()
string(TOUPPER "${CORE_SYMBOL_NAME}" CORE_SYMBOL_NAME)
string(LENGTH "${CORE_SYMBOL_NAME}" CORE_SYMBOL_NAME_LENGTH)
if(CORE_SYMBOL_NAME_LENGTH GREATER 7)
  message(FATAL_ERROR "CORE_SYMBOL_NAME length must be between 1 and 7 characters")
endif()
message(STATUS "Using '${CORE_SYMBOL_NAME}' as CORE symbol name")

###

if("${EOSIO_ROOT_KEY}" STREQUAL "")
  set(EOSIO_ROOT_KEY "EOS6MRyAjQq8ud7hVNYcfnVPJqcVpscN5So8BhtHuGYqET5GDW5CV")
endif()

message(STATUS "Using '${EOSIO_ROOT_KEY}' as public key for 'eosio' account")

message(STATUS "CFLAGS:   ${CMAKE_C_FLAGS}")
message(STATUS "CXXFLAGS: ${CMAKE_CXX_FLAGS}")

add_subdirectory(libraries)
add_subdirectory(plugins)
add_subdirectory(programs)
add_subdirectory(scripts)
add_subdirectory(unittests)
add_subdirectory(tests)
add_subdirectory(tools)
add_subdirectory(debian)
add_subdirectory(simulator)

install_directory_permissions(DIRECTORY ${CMAKE_INSTALL_FULL_SYSCONFDIR}/${PROJECT})

install(FILES testnet.template DESTINATION ${CMAKE_INSTALL_FULL_SYSCONFDIR}/${PROJECT}/launcher)
install_directory_permissions(DIRECTORY ${CMAKE_INSTALL_FULL_SYSCONFDIR}/${PROJECT})
install_directory_permissions(DIRECTORY ${CMAKE_INSTALL_FULL_SYSCONFDIR}/${PROJECT}/launcher)
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/testnet.template ${CMAKE_CURRENT_BINARY_DIR}/etc/${PROJECT}/launcher/testnet.template COPYONLY)

# TODO: remove
#~configure_file(${CMAKE_CURRENT_SOURCE_DIR}/${PROJECT}.version.in ${CMAKE_CURRENT_BINARY_DIR}/${PROJECT}.version.hpp)
#~install(FILES ${CMAKE_CURRENT_BINARY_DIR}/${PROJECT}.version.hpp DESTINATION ${CMAKE_INSTALL_FULL_INCLUDEDIR})

set(EOS_ROOT_DIR ${CMAKE_BINARY_DIR})
configure_file(${CMAKE_SOURCE_DIR}/CMakeModules/eosio-config.cmake.in     ${CMAKE_BINARY_DIR}/lib/cmake/${PROJECT}/eosio-config.cmake @ONLY)
configure_file(${CMAKE_SOURCE_DIR}/CMakeModules/EosioTesterBuild.cmake.in ${CMAKE_BINARY_DIR}/lib/cmake/${PROJECT}/EosioTester.cmake @ONLY)

set(EOS_ROOT_DIR ${CMAKE_INSTALL_PREFIX})
configure_file(${CMAKE_SOURCE_DIR}/CMakeModules/eosio-config.cmake.in ${CMAKE_BINARY_DIR}/modules/eosio-config.cmake @ONLY)
install(FILES ${CMAKE_BINARY_DIR}/modules/eosio-config.cmake DESTINATION ${CMAKE_INSTALL_FULL_LIBDIR}/cmake/${PROJECT})
configure_file(${CMAKE_SOURCE_DIR}/CMakeModules/EosioTester.cmake.in ${CMAKE_BINARY_DIR}/modules/EosioTester.cmake @ONLY)
install(FILES ${CMAKE_BINARY_DIR}/modules/EosioTester.cmake DESTINATION ${CMAKE_INSTALL_FULL_LIBDIR}/cmake/${PROJECT})

# for contracts repository
install(FILES ${CMAKE_SOURCE_DIR}/CMakeModules/utils.cmake DESTINATION ${CMAKE_INSTALL_FULL_LIBDIR}/cmake/${PROJECT}/ RENAME ${PROJECT}-utils.cmake)

configure_file(${CMAKE_SOURCE_DIR}/LICENSE                                 ${CMAKE_BINARY_DIR}/licenses/${PROJECT}/LICENSE COPYONLY)
configure_file(${CMAKE_SOURCE_DIR}/libraries/wabt/LICENSE                  ${CMAKE_BINARY_DIR}/licenses/${PROJECT}/LICENSE.wabt COPYONLY)
configure_file(${CMAKE_SOURCE_DIR}/libraries/softfloat/COPYING.txt         ${CMAKE_BINARY_DIR}/licenses/${PROJECT}/LICENSE.softfloat COPYONLY)
configure_file(${CMAKE_SOURCE_DIR}/libraries/wasm-jit/LICENSE              ${CMAKE_BINARY_DIR}/licenses/${PROJECT}/LICENSE.wavm COPYONLY)
configure_file(${CMAKE_SOURCE_DIR}/libraries/fc/secp256k1/upstream/COPYING ${CMAKE_BINARY_DIR}/licenses/${PROJECT}/LICENSE.secp256k1 COPYONLY)
configure_file(${CMAKE_SOURCE_DIR}/libraries/fc/src/network/LICENSE.go     ${CMAKE_BINARY_DIR}/licenses/${PROJECT}/LICENSE.go COPYONLY)
configure_file(${CMAKE_SOURCE_DIR}/libraries/yubihsm/LICENSE               ${CMAKE_BINARY_DIR}/licenses/${PROJECT}/LICENSE.yubihsm COPYONLY)

install(FILES LICENSE                                 DESTINATION ${CMAKE_INSTALL_FULL_DATAROOTDIR}/licenses/${PROJECT}/ COMPONENT base)
install(FILES libraries/wabt/LICENSE                  DESTINATION ${CMAKE_INSTALL_FULL_DATAROOTDIR}/licenses/${PROJECT}/ COMPONENT base RENAME LICENSE.wabt)
install(FILES libraries/softfloat/COPYING.txt         DESTINATION ${CMAKE_INSTALL_FULL_DATAROOTDIR}/licenses/${PROJECT}/ COMPONENT base RENAME LICENSE.softfloat)
install(FILES libraries/wasm-jit/LICENSE              DESTINATION ${CMAKE_INSTALL_FULL_DATAROOTDIR}/licenses/${PROJECT}/ COMPONENT base RENAME LICENSE.wavm)
install(FILES libraries/fc/secp256k1/upstream/COPYING DESTINATION ${CMAKE_INSTALL_FULL_DATAROOTDIR}/licenses/${PROJECT}/ COMPONENT base RENAME LICENSE.secp256k1)
install(FILES libraries/fc/src/network/LICENSE.go     DESTINATION ${CMAKE_INSTALL_FULL_DATAROOTDIR}/licenses/${PROJECT}/ COMPONENT base)
install(FILES libraries/yubihsm/LICENSE               DESTINATION ${CMAKE_INSTALL_FULL_DATAROOTDIR}/licenses/${PROJECT}/ COMPONENT base RENAME LICENSE.yubihsm)

add_custom_target(base-install
  COMMAND "${CMAKE_COMMAND}" --build "${CMAKE_BINARY_DIR}"
  COMMAND "${CMAKE_COMMAND}" -DCMAKE_INSTALL_COMPONENT=base -P "${CMAKE_BINARY_DIR}/cmake_install.cmake"
  USES_TERMINAL
)

get_property(_CTEST_CUSTOM_TESTS_IGNORE GLOBAL PROPERTY CTEST_CUSTOM_TESTS_IGNORE)
file(WRITE "${CMAKE_BINARY_DIR}/CTestCustom.cmake" "set(CTEST_CUSTOM_TESTS_IGNORE ${_CTEST_CUSTOM_TESTS_IGNORE})")

include(package)
include(doxygen)
