/**
 *  @file
 *  @copyright defined in eos/LICENSE
 */
#pragma once
#include <appbase/application.hpp>
#include <eosio/chain_plugin/chain_plugin.hpp>
#include <eosio/net_plugin/protocol.hpp>

namespace eosio {
   using namespace appbase;

   struct connection_status {
      string            peer;
      bool              connecting = false;
      bool              syncing    = false;
      handshake_message last_handshake;
   };

   using subs_cb = std::function<void (uint32_t, const custom_message&)>;

   class net_plugin : public appbase::plugin<net_plugin>
   {
      public:
        net_plugin();
        virtual ~net_plugin();

        APPBASE_PLUGIN_REQUIRES((chain_plugin))
        virtual void set_program_options(options_description& cli, options_description& cfg) override;
        void handle_sighup() override;

        void plugin_initialize(const variables_map& options);
        void plugin_startup();
        void plugin_shutdown();

        void   broadcast_block(const chain::signed_block &sb);

        string                       connect( const string& endpoint );
        string                       disconnect( const string& endpoint );
        optional<connection_status>  status( const string& endpoint )const;
        vector<connection_status>    connections()const;

        template <typename T>
        void subscribe(uint32_t msg_type, std::function<void(uint32_t peer_num, const T& msg)>&& cb) {
           subscribe(msg_type, [cb = std::move(cb)](uint32_t peer_num, const custom_message& raw_msg) {
             cb(peer_num, fc::raw::unpack<T>(raw_msg.data));
           });
        }
        void subscribe(uint32_t msg_type, subs_cb&&);

        template <typename T>
        void send(uint32_t peer_num, uint32_t msg_type, const T& msg) {
          send(peer_num, custom_message { msg_type, fc::raw::pack(msg) });
        }
        void send(uint32_t peer_num, const custom_message&);

        size_t num_peers() const;


      public:
        using new_peer = channel_decl<struct net_new_peer_tag, uint32_t>;

      private:
        std::shared_ptr<class net_plugin_impl> my;
   };

}

FC_REFLECT( eosio::connection_status, (peer)(connecting)(syncing)(last_handshake) )
