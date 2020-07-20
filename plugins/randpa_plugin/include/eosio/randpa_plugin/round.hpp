#pragma once

#include "types.hpp"
#include "prefix_chain_tree.hpp"
#include "network_messages.hpp"
#include "randpa_logger.hpp"

namespace randpa_finality {

using tree_node = prefix_node<prevote_msg>;
using prefix_tree = prefix_chain_tree<tree_node>;

using tree_node_ptr = std::shared_ptr<tree_node>;
using tree_node_unique_ptr = std::unique_ptr<tree_node>;
using prefix_tree_ptr = std::shared_ptr<prefix_tree>;

using randpa_round_ptr = std::shared_ptr<class randpa_round>;

class randpa_round {
public:
    enum class state_type {
        init,               // init -> prevote
        prevote,            // prevote -> ready_to_precommit | fail
        ready_to_precommit, // ready_to_precommit -> precommit
        precommit,          // precommit -> done | fail
        done,               // (gained supermajority)
        fail,               // (failed)
    };

private:
    using prevote_bcaster_type = std::function<void(const prevote_msg&)>;
    using precommit_bcaster_type = std::function<void(const precommit_msg&)>;
    using done_cb_type = std::function<void()>;

    uint32_t num { 0 };
    public_key_type primary;
    prefix_tree_ptr tree;
    state_type state { state_type::init };
    proof_type proof;
    tree_node_ptr best_node;
    std::vector<signature_provider_type> signature_providers;
    prevote_bcaster_type prevote_bcaster;
    precommit_bcaster_type precommit_bcaster;
    done_cb_type done_cb;

    std::set<public_key_type> prevoted_keys;
    std::set<public_key_type> precommited_keys;

public:
    randpa_round(uint32_t num,
                 const public_key_type& primary,
                 const prefix_tree_ptr& tree,
                 const std::vector<signature_provider_type>& signature_providers,
                 prevote_bcaster_type && prevote_bcaster,
                 precommit_bcaster_type && precommit_bcaster,
                 done_cb_type && done_cb)
        : num{num}
        , primary{primary}
        , tree{tree}
        , signature_providers{signature_providers}
        , prevote_bcaster{std::move(prevote_bcaster)}
        , precommit_bcaster{std::move(precommit_bcaster)}
        , done_cb{std::move(done_cb)}
    {
        randpa_dlog("Randpa round started, num: ${n}, primary: ${p}",
                   ("n", num)
                   ("p", primary)
        );

        prevote();
    }

    uint32_t get_num() const {
        return num;
    }

    state_type get_state() const {
        return state;
    }

    void set_state(const state_type& s) {
        state = s;
    }

    proof_type get_proof() {
        FC_ASSERT(state == state_type::done, "state should be `done`");

        return proof;
    }

    void on(const prevote_msg& msg) {
        if (state != state_type::prevote && state != state_type::ready_to_precommit) {
            randpa_dlog("Skipping prevote, round: ${r}", ("r", num));
            return;
        }

        const auto& msg_signatures = msg.signatures;
        const auto& msg_pub_keys = msg.public_keys();

        // split msg with n keys (n >= 1) into n msg's with a single key each
        // in order to validate and add prevote for each key independenlty
        for (size_t i = 0; i < msg_signatures.size(); i++) {
            if (!validate_prevote(msg, msg_pub_keys[i])) {
                randpa_dlog("Invalid prevote for round ${num}", ("num", num));
                continue;
            }
            // use msg with a single key
            add_prevote(prevote_msg(msg.data, { msg_signatures[i] }));
        }
    }

    void on(const precommit_msg& msg) {
        if (state != state_type::precommit && state != state_type::ready_to_precommit) {
            randpa_dlog("Skipping precommit, round: ${r}", ("r", num));
            return;
        }

        const auto& msg_signatures = msg.signatures;
        const auto& msg_pub_keys = msg.public_keys();

        // see on(prevote_msg&) handler
        for (size_t i = 0; i < msg_signatures.size(); i++) {
            if (!validate_precommit(msg, msg_pub_keys[i])) {
                randpa_dlog("Invalid precommit for round ${num}", ("num", num));
                continue;
            }
            auto msg_with_single_key = precommit_msg(msg.data, { msg_signatures[i] });
            add_precommit(msg_with_single_key);
        }
    }

    void end_prevote() {
        if (state != state_type::ready_to_precommit) {
            randpa_dlog("Round failed, num: ${n}, state: ${s}",
                       ("n", num)
                       ("s", static_cast<uint32_t>(state))
            );
            state = state_type::fail;
            return;
        }

        randpa_dlog("Prevote finished for round ${r}, best_block: ${id}", ("r", num)("id", best_node->block_id));

        proof.round_num = num;
        proof.best_block = best_node->block_id;

        std::transform(best_node->confirmation_data.begin(), best_node->confirmation_data.end(),
            std::back_inserter(proof.prevotes), [](const auto& item) -> prevote_msg { return *item.second; });

        precommit();
    }

    bool finish() {
        if (state != state_type::done) {
            randpa_dlog("Round failed, num: ${n}, state: ${s}",
                       ("n", num)
                       ("s", static_cast<uint32_t>(state))
            );
            state = state_type::fail;
            return false;
        }
        return true;
    }

private:
    void prevote() {
        FC_ASSERT(state == state_type::init, "state should be `init`");
        state = state_type::prevote;

        auto last_node = tree->get_last_inserted_block(primary);
        if (!last_node) {
            randpa_wlog("Not found last node in tree for primary, primary: ${p}", ("p", primary));
            return;
        }
        auto chain = tree->get_branch(last_node->block_id);

        auto prevote = prevote_type { num, chain.base_block, std::move(chain.blocks) };

        for (const auto& sig_prov : signature_providers) {
            auto msg = prevote_msg(prevote, { sig_prov });
            add_prevote(msg);
        }
        prevote_bcaster(prevote_msg(prevote, signature_providers));
    }

    void precommit() {
        FC_ASSERT(state == state_type::ready_to_precommit, "state should be `ready_to_precommit`");
        state = state_type::precommit;

        auto precommit = precommit_type { num, best_node->block_id };

        for (const auto& sig_prov : signature_providers) {
            auto msg = precommit_msg(precommit, { sig_prov });
            add_precommit(msg);
        }
        precommit_bcaster(precommit_msg(precommit, signature_providers));
    }

    bool validate_prevote(const prevote_msg& msg, const public_key_type& key) {
        if (num != msg.data.round_num) {
            randpa_dlog("Randpa received prevote for wrong round, received for: ${rr}, expected: ${er}",
                       ("rr", msg.data.round_num)
                       ("er", num)
            );
            return false;
        }

        if (prevoted_keys.count(key)) {
            randpa_dlog("Randpa received prevote second time for key ${k}", ("k", key));
            return false;
        }

        auto node = find_last_node(msg.data.base_block, msg.data.blocks);

        if (!node) {
            randpa_dlog("Randpa received prevote for unknown blocks");
            return false;
        }

        if (!node->active_bp_keys.count(key)) {
            randpa_dlog("Randpa received prevote for block from not active producer, id : ${id}",
                       ("id", node->block_id)
            );
            return false;
        }

        return true;
    }

    bool validate_precommit(const precommit_msg& msg, const public_key_type& key) {
        if (num != msg.data.round_num) {
            randpa_dlog("Randpa received precommit for wrong round, received for: ${rr}, expected: ${er}",
                       ("rr", msg.data.round_num)
                       ("er", num)
            );
            return false;
        }

        if (precommited_keys.count(key)) {
            randpa_dlog("Randpa received precommit second time for key");
            return false;
        }

        if (msg.data.block_id != best_node->block_id) {
            randpa_dlog("Randpa received precommit for not best block, id: ${id}, best_id: ${best_id}",
                       ("id", msg.data.block_id)
                       ("best_id", best_node->block_id)
            );
            return false;
        }

        if (!best_node->has_confirmation(key)) {
            randpa_dlog("Randpa received precommit from not prevoted peer: ${k}", ("k", key));
            return false;
        }

        return true;
    }

    void add_prevote(const prevote_msg& msg) {
        const auto pub_keys = msg.public_keys();
        FC_ASSERT(pub_keys.size() == 1, "invalid number of public keys in msg; should be 1");
        const public_key_type& key = pub_keys[0];

        auto max_prevote_node = tree->add_confirmations(
            { msg.data.base_block, msg.data.blocks }, key, std::make_shared<prevote_msg>(msg));

        FC_ASSERT(max_prevote_node, "confirmation should be insertable");

        prevoted_keys.insert(key);
        randpa_dlog("Prevote inserted, round: ${r}, from: ${f}, max_confs: ${c}",
                   ("r", num)
                   ("f", key)
                   ("c", max_prevote_node->confirmation_number())
        );

        if (state != state_type::ready_to_precommit && is_prevote_threshold_reached(max_prevote_node)) {
            state = state_type::ready_to_precommit;
            best_node = max_prevote_node;
            randpa_dlog("Prevote threshold reached, round: ${r}, best block: ${b}",
                       ("r", num)
                       ("b", best_node->block_id)
            );
        }
    }

    void add_precommit(const precommit_msg& msg) {
        FC_ASSERT(msg.public_keys().size() == 1, "invalid number of public keys in msg; should be 1");

        const auto key = msg.public_keys()[0];
        precommited_keys.insert(key);
        proof.precommits.push_back(msg);

        randpa_dlog("Precommit inserted, round: ${r}, from: ${f}",
                   ("r", num)
                   ("f", key)
        );

        if (state != state_type::done && is_precommit_threshold_reached()) {
            randpa_dlog("Precommit threshold reached, round: ${r}, best block: ${b}",
                       ("r", num)
                       ("b", best_node->block_id)
            );
            state = state_type::done;
            done_cb();
        }
    }

    tree_node_ptr find_last_node(const block_id_type& base_block, const block_ids_type& blocks) {
        auto block_itr = std::find_if(blocks.rbegin(), blocks.rend(),
            [&](const auto& block_id) {
                return (bool) tree->find(block_id);
            });

        if (block_itr == blocks.rend()) {
            return tree->find(base_block);
        }

        return tree->find(*block_itr);
    }

    static bool is_prevote_threshold_reached(const tree_node_ptr& node) {
        return node->confirmation_number() > 2 * node->active_bp_keys.size() / 3;
    }

    bool is_precommit_threshold_reached() const {
        return proof.precommits.size() > 2 * best_node->active_bp_keys.size() / 3;
    }
};

} //namespace randpa_finality
