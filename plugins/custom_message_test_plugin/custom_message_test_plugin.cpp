/**
 *  @file
 *  @copyright defined in eos/LICENSE
 */
#include <eosio/custom_message_test_plugin/custom_message_test_plugin.hpp>

#include <fc/variant.hpp>
#include <fc/io/json.hpp>
#include <fc/exception/exception.hpp>
#include <fc/reflect/reflect.hpp>
#include <fc/reflect/variant.hpp>
#include <thread>

namespace fc {
   extern std::unordered_map<std::string,logger>& get_logger_map();
   class variant;
}

constexpr uint32_t struct_message_type = 0;
struct struct_message {
   std::string a;
   uint64_t b;
   uint64_t c;
};
FC_REFLECT( struct_message, (a)(b)(c))

constexpr uint32_t string_message_type = 1;
using string_message = std::string;

struct empty_result {};

FC_REFLECT(empty_result, );

namespace eosio {

using namespace chain;
using namespace chain::plugin_interface;
static appbase::abstract_plugin& _custom_message_test_plugin = app().register_plugin<custom_message_test_plugin>();



#define CALL(api_name, api_handle, call_name, INVOKE, http_response_code) \
{std::string("/v1/" #api_name "/" #call_name), \
   [this](string, string body, url_response_callback cb) mutable { \
          try { \
             if (body.empty()) body = "{}"; \
             INVOKE \
             cb(http_response_code, fc::json::to_string(result, fc::time_point::maximum())); \
          } catch (...) { \
             http_plugin::handle_exception(#api_name, #call_name, body, cb); \
          } \
       }}

#define INVOKE_V_R_R_R(api_handle, call_name, in_param0, in_param1, in_param2) \
     const auto& vs = fc::json::json::from_string(body).as<fc::variants>(); \
     api_handle->call_name(vs.at(0).as<in_param0>(), vs.at(1).as<in_param1>(), vs.at(2).as<in_param2>()); \
     empty_result result;

#define INVOKE_V_R_R(api_handle, call_name, in_param0, in_param1) \
     const auto& vs = fc::json::json::from_string(body).as<fc::variants>(); \
     api_handle->call_name(vs.at(0).as<in_param0>(), vs.at(1).as<in_param1>()); \
     empty_result result;



struct custom_message_test_plugin_impl {

   void send_struct_message(uint32_t ses_id, const struct_message & msg) {
      app().get_plugin<net_plugin>().send(ses_id, struct_message_type, msg);
   }

   void send_string_message(uint32_t ses_id, const string_message & msg) {
      app().get_plugin<net_plugin>().send(ses_id, string_message_type, msg);
   }

   void send_default_message(uint32_t ses_id, uint32_t type) {
      app().get_plugin<net_plugin>().send(ses_id, type, std::string("default msg send"));
   }

   void send(uint32_t session_id, uint32_t type, const variant& var) {
      try {
         switch (type) {
            case struct_message_type:
               send_struct_message(session_id, var.as<struct_message>());
               break;

            case string_message_type:
               send_string_message(session_id, var.as<string_message>());
               break;

            default:
               send_default_message(session_id, type);
               break;
         }

         wlog("send custom message, type: ${type}, message: ${msg}, to: ${to}",
            ("type", type)
            ("msg", fc::json::to_string(var, fc::time_point::maximum()))
            ("to", session_id)
         );
      }
      catch (std::exception ex) {
         elog("error on send, e: ${e}", ("e", ex.what()));
      }
   }

   void subscribe() {
      app().get_plugin<net_plugin>().subscribe<struct_message>(struct_message_type,
      [](uint32_t ses_id, const struct_message & msg) {
         wlog("received custom message, session_id: ${ses_id}, type: ${type}, thread: ${thread}",
            ("ses_id", ses_id)
            ("type", struct_message_type)
            ("thread", get_thread_id())
         );
      });

      app().get_plugin<net_plugin>().subscribe<string_message>(string_message_type,
      [](uint32_t ses_id, const string_message & msg) {
         wlog("received custom message, session_id: ${ses_id}, type: ${type}, thread: ${thread}",
            ("ses_id", ses_id)
            ("type", string_message_type)
            ("thread", get_thread_id())
         );
      });
   }

   static std::string get_thread_id() {
      std::stringstream ss;
      ss << std::this_thread::get_id();
      return ss.str();
   }
};

custom_message_test_plugin::custom_message_test_plugin() {}
custom_message_test_plugin::~custom_message_test_plugin() {}

void custom_message_test_plugin::set_program_options(options_description&, options_description& cfg) { }

void custom_message_test_plugin::plugin_initialize(const variables_map& options) {
   try {
      my.reset( new custom_message_test_plugin_impl );
   } FC_LOG_AND_RETHROW()
}

void custom_message_test_plugin::plugin_startup() {
   my->subscribe();

   app().get_plugin<http_plugin>().add_api({
      CALL(custom_message_test, my, send, INVOKE_V_R_R_R(my, send, uint32_t, uint32_t, variant), 200),
   });

   wlog("Custom message test plugin started");
}

void custom_message_test_plugin::plugin_shutdown() { }

}
