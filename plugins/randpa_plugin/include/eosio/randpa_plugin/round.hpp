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

        if (!validate_prevote(msg)) {
            randpa_dlog("Invalid prevote for round ${num}", ("num", num));
            return;
        }

        add_prevote(msg);
    }

    void on(const precommit_msg& msg) {
        if (state != state_type::precommit && state != state_type::ready_to_precommit) {
            randpa_dlog("Skipping precommit, round: ${r}", ("r", num));
            return;
        }

        if (!validate_precommit(msg)) {
            randpa_dlog("Invalid precommit for round ${num}", ("num", num));
            return;
        }

        add_precommit(msg);
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
        auto msg = prevote_msg(prevote, signature_providers);
        add_prevote(msg);
        prevote_bcaster(msg);
    }

    void precommit() {
        FC_ASSERT(state == state_type::ready_to_precommit, "state should be `ready_to_precommit`");
        state = state_type::precommit;

        auto precommit = precommit_type { num, best_node->block_id };
        auto msg = precommit_msg(precommit, signature_providers);

        add_precommit(msg);

        precommit_bcaster(msg);
    }

    bool validate_prevote(const prevote_msg& msg) {
        if (num != msg.data.round_num) {
            randpa_dlog("Randpa received prevote for wrong round, received for: ${rr}, expected: ${er}",
                       ("rr", msg.data.round_num)
                       ("er", num)
            );
            return false;
        }

        for (const auto& public_key : msg.public_keys()) {
            if (prevoted_keys.count(public_key)) {
                randpa_dlog("Randpa received prevote second time for key");
                return false;
            }
        }

        auto node = find_last_node(msg.data.base_block, msg.data.blocks);

        if (!node) {
            randpa_dlog("Randpa received prevote for unknown blocks");
            return false;
        }

        for (const auto& public_key : msg.public_keys()) {
            if (!node->active_bp_keys.count(public_key)) {
                randpa_dlog("Randpa received prevote for block from not active producer, id : ${id}",
                           ("id", node->block_id)
                );
                return false;
            }
        }

        return true;
    }

    bool validate_precommit(const precommit_msg& msg) {
        if (num != msg.data.round_num) {
            randpa_dlog("Randpa received precommit for wrong round, received for: ${rr}, expected: ${er}",
                       ("rr", msg.data.round_num)
                       ("er", num)
            );
            return false;
        }

        for (const auto& public_key : msg.public_keys()) {
            if (precommited_keys.count(public_key)) {
                randpa_dlog("Randpa received precommit second time for key");
                return false;
            }
        }

        if (msg.data.block_id != best_node->block_id) {
            randpa_dlog("Randpa received precommit for not best block, id: ${id}, best_id: ${best_id}",
                       ("id", msg.data.block_id)
                       ("best_id", best_node->block_id)
            );
            return false;
        }

        for (const auto& public_key : msg.public_keys()) {
            if (!best_node->has_confirmation(public_key)) {
                randpa_dlog("Randpa received precommit from not prevoted peer");
                return false;
            }
        }

        return true;
    }

    void add_prevote(const prevote_msg& msg) {
        auto public_keys = msg.public_keys();
        for (const auto& public_key : public_keys) {
            auto max_prevote_node = tree->add_confirmations({ msg.data.base_block, msg.data.blocks },
                                    public_key, std::make_shared<prevote_msg>(msg));

            FC_ASSERT(max_prevote_node, "confirmation should be insertable");

            prevoted_keys.insert(public_key);
            randpa_dlog("Prevote inserted, round: ${r}, from: ${f}, max_confs: ${c}",
                       ("r", num)
                       ("f", public_key)
                       ("c", max_prevote_node->confirmation_number())
            );

            if (has_threshold_prevotes(max_prevote_node)) {
                state = state_type::ready_to_precommit;
                best_node = max_prevote_node;
                randpa_dlog("Prevote threshold reached, round: ${r}, best block: ${b}",
                           ("r", num)
                           ("b", best_node->block_id)
                );
                return;
            }
        }
    }

    void add_precommit(const precommit_msg& msg) {
        for (const auto& public_key : msg.public_keys()) {
            precommited_keys.insert(public_key);
            proof.precommits.push_back(msg);

            randpa_dlog("Precommit inserted, round: ${r}, from: ${f}",
                       ("r", num)
                       ("f", public_key)
            );

            if (proof.precommits.size() > 2 * best_node->active_bp_keys.size() / 3) {
                randpa_dlog("Precommit threshold reached, round: ${r}, best block: ${b}",
                           ("r", num)
                           ("b", best_node->block_id)
                );
                state = state_type::done;
                done_cb();
                return;
            }
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

    bool has_threshold_prevotes(const tree_node_ptr& node) {
        return node->confirmation_number() > 2 * node->active_bp_keys.size() / 3;
    }
};

} //namespace randpa_finality
