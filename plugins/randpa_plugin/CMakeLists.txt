add_library(randpa_plugin
  randpa_logger.cpp
  randpa_plugin.cpp
)

target_link_libraries(randpa_plugin
  appbase
  chain_plugin
  eosio_chain
  fc
  http_plugin
  net_plugin
  telemetry_plugin
)
target_include_directories(randpa_plugin PUBLIC include)

add_subdirectory(tests)
