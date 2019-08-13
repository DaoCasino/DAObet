#pragma once
#include "types.hpp"
#include "prefix_chain_tree.hpp"
#include "network_messages.hpp"

namespace randpa_finality {

using tree_node = prefix_node<prevote_msg>;
using prefix_tree = prefix_chain_tree<tree_node>;

using tree_node_ptr = std::shared_ptr<tree_node>;
using prefix_tree_ptr = std::shared_ptr<prefix_tree>;

using randpa_round_ptr = std::shared_ptr<class randpa_round>;

class randpa_round {
public:
    enum class state {
        init, // init state
        prevote, // prevote state (init -> prevote)
        ready_to_precommit, // ready to precommit (prevote -> ready_to_precommit)
        precommit, // precommit stage (ready_to_precommit -> precommit)
        done,   // we have supermajority (precommit -> done)
        fail,   // we failed (precommit -> fail | prevote -> fail)
        finished, // after finish
    };


private:
    using prevote_bcaster_type = std::function<void(const prevote_msg&)>;
    using precommit_bcaster_type = std::function<void(const precommit_msg&)>;
    using done_cb_type = std::function<void()>;

public:
    randpa_round(uint32_t num,
        const public_key_type& primary,
        const prefix_tree_ptr& tree,
        const private_key_type& private_key,
        prevote_bcaster_type && prevote_bcaster,
        precommit_bcaster_type && precommit_bcaster,
        done_cb_type && done_cb
    ) :
        num(num),
        primary(primary),
        tree(tree),
        private_key(private_key),
        prevote_bcaster(std::move(prevote_bcaster)),
        precommit_bcaster(std::move(precommit_bcaster)),
        done_cb(std::move(done_cb))
    {
        dlog("Randpa round started, num: ${n}, primary: ${p}",
            ("n", num)
            ("p", primary)
        );

        prevote();
    }

    uint32_t get_num() const {
        return num;
    }

    state get_state() const {
        return state;
    }

    void set_state(const state& s) {
        state = s;
    }

    proof_type get_proof() {
        FC_ASSERT(state == state::done, "state should be `done`");

        return proof;
    }

    void on(const prevote_msg& msg) {
        dlog("Received prevote for round ${num}",
             ("num", num));
        if (state != state::prevote && state != state::ready_to_precommit) {
            dlog("Prevote while wrong state, round: ${r}", ("r", num));
            return;
        }

        if (!validate_prevote(msg)) {
            dlog("Invalid prevote for round ${num}: ${msg}",
                 ("num", num)
                 ("msg", msg));
            return;
        }

        add_prevote(msg);
    }

    void on(const precommit_msg& msg) {
        dlog("Received precommit for round ${num}",
             ("num", num));

        if (state != state::precommit && state != state::ready_to_precommit) {
            dlog("Precommit while wrong state, round: ${r}", ("r", num));
            return;
        }

        if (!validate_precommit(msg)) {
            dlog("Invalid precommit for round ${num}: ${msg}",
                 ("num", num)
                 ("msg", msg));
            return;
        }

        add_precommit(msg);
    }

    void end_prevote() {
        if (state != state::ready_to_precommit) {
            dlog("Round failed, num: ${n}, state: ${s}",
                ("n", num)
                ("s", static_cast<uint32_t>(state))
            );
            state = state::fail;
            return;
        }

        proof.round_num = num;
        proof.best_block = best_node->block_id;

        std::transform(best_node->confirmation_data.begin(), best_node->confirmation_data.end(),
            std::back_inserter(proof.prevotes), [](const auto& item) -> prevote_msg { return *item.second; });

        precommit();
    }

    bool finish() {
        if (state != state::done) {
            dlog("Round failed, num: ${n}, state: ${s}",
                ("n", num)
                ("s", static_cast<uint32_t>(state))
            );
            state = state::fail;
            return false;
        }
        return true;
    }

private:
    void prevote() {
        FC_ASSERT(state == state::init, "state should be `init`");
        dlog("Round sending prevote, num: ${n}", ("n", num));
        state = state::prevote;

        auto last_node = tree->get_last_inserted_block(primary);

        if (!last_node) {
            wlog("Not found last node in tree for primary, primary: ${p}", ("p", primary));
            return;
        }

        auto chain = tree->get_branch(last_node->block_id);

        auto prevote = prevote_type { num, chain.base_block, std::move(chain.blocks) };
        auto msg = prevote_msg(prevote, private_key);
        add_prevote(msg);
        prevote_bcaster(msg);
    }

    void precommit() {
        FC_ASSERT(state == state::ready_to_precommit, "state should be `ready_to_precommit`");
        dlog("Round sending precommit, num: ${n}", ("n", num));
        state = state::precommit;

        auto precommit = precommit_type { num, best_node->block_id };
        auto msg = precommit_msg(precommit, private_key);

        add_precommit(msg);

        precommit_bcaster(msg);
    }

    bool validate_prevote(const prevote_msg& msg) {
        if (num != msg.data.round_num) {
            dlog("Randpa received prevote for wrong round, received for: ${rr}, expected: ${er}",
                ("rr", msg.data.round_num)
                ("er", num)
            );
            return false;
        }

        if (prevoted_keys.count(msg.public_key())) {
            dlog("Randpa received prevote second time for key");
            return false;
        }

        auto node = find_last_node(msg.data.base_block, msg.data.blocks);

        if (!node) {
            dlog("Randpa received prevote for unknown blocks");
            return false;
        }

        if (!node->active_bp_keys.count(msg.public_key())) {
            dlog("Randpa received prevote for block from not active producer, id : ${id}",
                ("id", node->block_id)
            );
            return false;
        }

        return true;
    }

    bool validate_precommit(const precommit_msg& msg) {
        if (num != msg.data.round_num) {
            dlog("Randpa received precommit for wrong round, received for: ${rr}, expected: ${er}",
                ("rr", msg.data.round_num)
                ("er", num)
            );
            return false;
        }

        if (precommited_keys.count(msg.public_key())) {
            dlog("Randpa received precommit second time for key");
            return false;
        }

        if (msg.data.block_id != best_node->block_id) {
            dlog("Randpa received precommit for not best block, id: ${id}, best_id: ${best_id}",
                ("id", msg.data.block_id)
                ("best_id", best_node->block_id)
            );
            return false;
        }

        if (!best_node->has_confirmation(msg.public_key())) {
            dlog("Randpa received precommit from not prevoted peer");
            return false;
        }

        return true;
    }

    void add_prevote(const prevote_msg& msg) {
        if (state == state::ready_to_precommit) {
            return;
        }

        auto max_prevote_node = tree->add_confirmations({ msg.data.base_block, msg.data.blocks },
                                msg.public_key(), std::make_shared<prevote_msg>(msg));

        FC_ASSERT(max_prevote_node, "confirmation should be insertable");

        prevoted_keys.insert(msg.public_key());
        dlog("Prevote inserted, round: ${r}, from: ${f}, max_confs: ${c}",
            ("r", num)
            ("f", msg.public_key())
            ("c", max_prevote_node->confirmation_number())
        );

        if (has_threshold_prevotes(max_prevote_node)) {
            state = state::ready_to_precommit;
            best_node = max_prevote_node;
            dlog("Prevote threshold reached, round: ${r}, best block: ${b}",
                ("r", num)
                ("b", best_node->block_id)
            );
            return;
        }
    }

    void add_precommit(const precommit_msg& msg) {
        precommited_keys.insert(msg.public_key());
        proof.precommits.push_back(msg);

        if (proof.precommits.size() > 2 * best_node->active_bp_keys.size() / 3) {
            dlog("Precommit threshold reached, round: ${r}, best block: ${b}",
                ("r", num)
                ("b", best_node->block_id)
            );
            state = state::done;
            done_cb();
        }
    }

    tree_node_ptr find_last_node(const block_id_type& base_block, const vector<block_id_type>& blocks) {
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

    uint32_t num { 0 };
    public_key_type primary;
    prefix_tree_ptr tree;
    state state { state::init };
    proof_type proof;
    tree_node_ptr best_node;
    private_key_type private_key;
    prevote_bcaster_type prevote_bcaster;
    precommit_bcaster_type precommit_bcaster;
    done_cb_type done_cb;

    std::set<public_key_type> prevoted_keys;
    std::set<public_key_type> precommited_keys;
};

} //namespace randpa_finality