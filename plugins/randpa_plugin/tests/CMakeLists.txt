set( CMAKE_CXX_STANDARD 17 )

add_executable( randpa_plugin_unit_test main.cpp randpa_plugin_tests.cpp )
target_link_libraries( randpa_plugin_unit_test randpa_plugin eosio_chain chainbase eosio_testing fc )

enable_testing()
add_test(NAME randpa_plugin_unit_test
        COMMAND randpa_plugin_unit_test)
