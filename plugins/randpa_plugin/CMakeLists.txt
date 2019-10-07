add_library( randpa_plugin randpa_plugin.cpp )

target_link_libraries( randpa_plugin telemetry_plugin chain_plugin eosio_chain appbase fc net_plugin http_plugin)
target_include_directories( randpa_plugin PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include" )
add_subdirectory(tests)
