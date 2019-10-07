#pragma once
#include <appbase/application.hpp>
#include <eosio/net_plugin/net_plugin.hpp>

namespace eosio {

using namespace appbase;

class randpa_plugin : public appbase::plugin<randpa_plugin> {
public:
    randpa_plugin();
    virtual ~randpa_plugin();

    APPBASE_PLUGIN_REQUIRES((net_plugin)(chain_plugin))
    virtual void set_program_options(options_description& /*cli*/, options_description& /*cfg*/) override {}

    void plugin_initialize(const variables_map& options);
    void plugin_startup();
    void plugin_shutdown();

private:
    std::unique_ptr<class randpa_plugin_impl> my;
};

}
