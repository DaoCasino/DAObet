file(GLOB HEADERS "include/eosio/net_plugin/*.hpp" )
add_library( net_plugin
             net_plugin.cpp
             ${HEADERS} )

target_link_libraries( net_plugin chain_plugin producer_plugin appbase fc
# HAYA: [cyb-277] add net msg count metrics
                       telemetry_plugin)
# HAYA

target_include_directories( net_plugin PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include ${CMAKE_CURRENT_SOURCE_DIR}/../chain_interface/include  "${CMAKE_CURRENT_SOURCE_DIR}/../../libraries/appbase/include")
