/**
 *  @file
 *  @copyright defined in eos/LICENSE
 */
#pragma once
#include <appbase/application.hpp>

#include <eosio/chain_plugin/chain_plugin.hpp>

namespace fc { class variant; }

namespace eosio {
   using chain::transaction_id_type;
   using std::shared_ptr;
   using namespace appbase;
   using chain::name;
   using fc::optional;
   using chain::uint128_t;

   typedef shared_ptr<class bnet_plugin_impl> bnet_ptr;
   typedef shared_ptr<const class bnet_plugin_impl> bnet_const_ptr;



/**
 *  This plugin tracks all actions and keys associated with a set of configured accounts. It enables
 *  wallets to paginate queries for bnet.
 *
 *  An action will be included in the account's bnet if any of the following:
 *     - receiver
 *     - any account named in auth list
 *
 *  A key will be linked to an account if the key is referneced in authorities of updateauth or newaccount
 */
class bnet_plugin : public plugin<bnet_plugin> {
   public:
      APPBASE_PLUGIN_REQUIRES((chain_plugin))

      bnet_plugin();
      virtual ~bnet_plugin();

      virtual void set_program_options(options_description& cli, options_description& cfg) override;

      void plugin_initialize(const variables_map& options);
      void plugin_startup();
      void plugin_shutdown();
      void handle_sighup() override;

      template <typename T, typename Call>
      void subscribe(uint32_t msg_type, Call && cb) {
         subscribe(msg_type, [cb = std::move(cb)](uint32_t session_num, const vector<char> &raw_msg) {
            cb(session_num, fc::raw::unpack<T>(raw_msg));
         });
      }
      void subscribe(uint32_t msg_type, std::function<void(uint32_t, const vector<char>&)>&&);

      template <typename T>
      void bcast(uint32_t msg_type, const T & msg) {
         bcast(msg_type, fc::raw::pack(msg));
      }
      void bcast(uint32_t msg_type, const vector<char>&);

      template <typename T>
      void send(uint32_t session_id, uint32_t msg_type, const T & msg) {
         send(session_id, msg_type, fc::raw::pack(msg));
      }
      void send(uint32_t session_id, uint32_t msg_type, const vector<char>&);

   public:
      using new_peer = channel_decl<struct bnet_new_peer_tag, uint32_t>;

   private:
      bnet_ptr my;
};

} /// namespace eosio


