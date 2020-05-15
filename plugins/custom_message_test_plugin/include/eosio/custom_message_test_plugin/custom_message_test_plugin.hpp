/**
 *  @file
 *  @copyright defined in eos/LICENSE
 */
#pragma once
#include <appbase/application.hpp>
#include <eosio/http_plugin/http_plugin.hpp>
#include <eosio/net_plugin/net_plugin.hpp>

namespace eosio {

using namespace appbase;

class custom_message_test_plugin : public appbase::plugin<custom_message_test_plugin> {
public:
   custom_message_test_plugin();
   ~custom_message_test_plugin();

   APPBASE_PLUGIN_REQUIRES((http_plugin)(net_plugin))
   virtual void set_program_options(options_description&, options_description& cfg) override;
 
   void plugin_initialize(const variables_map& options);
   void plugin_startup();
   void plugin_shutdown();

private:
   std::unique_ptr<struct custom_message_test_plugin_impl> my;
};

}
