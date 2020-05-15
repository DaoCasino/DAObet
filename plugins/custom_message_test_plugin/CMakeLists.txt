file(GLOB HEADERS "include/eosio/custom_message_test_plugin/*.hpp")
add_library( custom_message_test_plugin
             custom_message_test_plugin.cpp
             ${HEADERS} )

target_link_libraries( custom_message_test_plugin appbase fc http_plugin net_plugin )
target_include_directories( custom_message_test_plugin PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include" )
target_include_directories( custom_message_test_plugin PUBLIC ${CMAKE_BINARY_DIR}/contracts )
