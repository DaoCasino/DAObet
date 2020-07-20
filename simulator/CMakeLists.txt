add_executable(simulator
  main.cpp
  tests/eos_finality_tests.cpp
  tests/randpa_finality_tests.cpp

  ${CMAKE_SOURCE_DIR}/plugins/randpa_plugin/randpa_logger.cpp
)


include(ExternalProject)
ExternalProject_Add(gtest
  URL              https://github.com/google/googletest/archive/release-1.8.1.zip
  PREFIX           ${CMAKE_CURRENT_BINARY_DIR}/gtest
  INSTALL_COMMAND  ""
  CMAKE_ARGS       -D CMAKE_TOOLCHAIN_FILE=${CMAKE_TOOLCHAIN_FILE}
  BUILD_BYPRODUCTS "${CMAKE_CURRENT_BINARY_DIR}/gtest/src/gtest-build/googlemock/gtest/libgtest.a"
)
ExternalProject_Get_Property(gtest source_dir binary_dir)

target_include_directories(simulator PUBLIC
  include
  ${CMAKE_SOURCE_DIR}/plugins/randpa_plugin/include
)
target_include_directories(simulator SYSTEM PUBLIC
  ${source_dir}/googletest/include
)
target_link_libraries(simulator
  ${binary_dir}/googlemock/gtest/libgtest.a
  pthread
  fc
)
add_dependencies(simulator gtest)

##################################
# Just make the test runnable with
#   $ make test

enable_testing()
add_test(NAME simulator COMMAND simulator)
