/**
 *  @file
 *  @copyright defined in daobet/LICENSE
 */
#pragma once
#include <appbase/application.hpp>

namespace eosio {

using namespace appbase;

/**
 *  This is a plugin, intended to create a prometheus server for getting telemetry (using prometheus PULL model)
 */
class telemetry_plugin : public appbase::plugin<telemetry_plugin> {
public:
   telemetry_plugin();
   virtual ~telemetry_plugin();

   APPBASE_PLUGIN_REQUIRES()
   virtual void set_program_options(options_description&, options_description& cfg) override;

   void plugin_initialize(const variables_map& options);
   void plugin_startup();
   void plugin_shutdown();

   void add_counter(const std::string& metric_name);
   void update_counter(const std::string& metric_name, double value = 1.);
   void add_gauge(const std::string& metric_name);
   void update_gauge(const std::string& metric_name, const double value);
   void add_histogram(const std::string& metric_name, const std::vector<double>& keypoints);
   void update_histogram(const std::string& metric_name, const double value);
private:
   std::unique_ptr<class telemetry_plugin_impl> my;
};

}
