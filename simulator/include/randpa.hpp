#pragma once
#include "simulator.hpp"
#include "database.hpp"
#define SYNC_RANDPA //SYNC mode
#include <eosio/randpa_plugin/randpa.hpp>
#include <fc/time.hpp>
#include <mutex>
#include <queue>

using namespace randpa_finality;

using std::queue;
using std::unique_ptr;
using std::make_unique;
using randpa_ptr = std::unique_ptr<randpa>;

static signature_provider_type make_key_signature_provider(const private_key_type& key) {
   return [key]( const digest_type& digest ) {
      return key.sign(digest);
   };
}

class RandpaNode: public Node {
public:
    explicit RandpaNode(int id, Network && net, fork_db && db_, private_key_type private_key):
        Node(id, std::move(net), std::move(db_), std::move(private_key))
    {
        init();
        prefix_tree_ptr tree(new prefix_tree(std::make_shared<tree_node>(tree_node {
            db.last_irreversible_block_id()
        })));
        randpa_impl->start(tree);
    }

    void init() {
        init_channels();
        init_randpa();
    }

    virtual void restart() override {
        cout << "[Node] #" << id << " restarted " << endl;
        init();
        randpa_impl->start(copy_fork_db());
        auto runner = get_runner();
        auto from = this->id;
        for (uint32_t to = 0; to < runner->get_instances(); to++) {
            int delay = runner->get_delay_matrix()[from][to];
            if (from != to && delay != -1) {
                on_new_peer_event(to);
            }
        }
    }

    prefix_tree_ptr copy_fork_db() {
        tree_node_ptr root = std::make_shared<tree_node>(tree_node { db.last_irreversible_block_id() });;
        prefix_tree_ptr tree(new prefix_tree(std::move(root)));
        queue<fork_db_node_ptr> q;
        q.push(db.get_root());
        while (!q.empty()) {
            auto node = q.front();
            q.pop();
            for (const auto& adjacent_node : node->adjacent_nodes) {
                auto new_block_id = adjacent_node->block_id;
                tree->insert(chain_type{node->block_id, {new_block_id}},
                        adjacent_node->creator_key,
                        get_runner()->get_active_bp_keys());
                q.push(adjacent_node);
            }
        }
        return tree;
    }

    ~RandpaNode() {
        randpa_impl->stop();
    }

    void on_receive(uint32_t from, void* msg) override {
        cout << "[Node] #" << this->id << " on_receive " << endl;
        auto data = *static_cast<randpa_net_msg*>(msg);
        data.ses_id = from;
        data.receive_time = fc::time_point::now();
        in_net_ch->send(data);
    }

    void on_new_peer_event(uint32_t id) override {
        cout << "[Node] #" << this->id << " on_new_peer_event " << endl;
        ev_ch->send(randpa_event { ::on_new_peer_event { id } });
    }

    void on_accepted_block_event(pair<block_id_type, public_key_type> block) override {
        cout << "[Node] #" << this->id << " on_accepted_block_event " << endl;
        ev_ch->send(randpa_event { ::on_accepted_block_event { block.first, db.fetch_prev_block_id(block.first),
                                                                block.second, get_active_bp_keys()
                                                                } });
    }

private:
    void init_channels() {
        in_net_ch = std::make_shared<net_channel>();
        out_net_ch = std::make_shared<net_channel>();
        ev_ch = std::make_shared<event_channel>();
        finality_ch = std::make_shared<finality_channel>();

        out_net_ch->subscribe([this](const randpa_net_msg& msg) {
            send<randpa_net_msg>(msg.ses_id, msg);
        });

        finality_ch->subscribe([this](const block_id_type& id) {
            db.bft_finalize(id);
        });
    }

    void init_randpa() {
        randpa_impl = unique_ptr<randpa>(new randpa());
        (*randpa_impl)
            .set_event_channel(ev_ch)
            .set_in_net_channel(in_net_ch)
            .set_out_net_channel(out_net_ch)
            .set_finality_channel(finality_ch)
            .set_signature_provider(make_key_signature_provider(private_key), private_key.get_public_key());
    }

    net_channel_ptr in_net_ch;
    net_channel_ptr out_net_ch;
    event_channel_ptr ev_ch;
    finality_channel_ptr finality_ch;

    randpa_ptr randpa_impl;
};

