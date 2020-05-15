#pragma once

#include "network_messages.hpp"
#include "round.hpp"
#include "randpa_logger.hpp"

#include <fc/exception/exception.hpp>
#include <fc/io/json.hpp>

#include <boost/blank.hpp>
#include <boost/circular_buffer.hpp>
#include <boost/compute/detail/lru_cache.hpp>

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <mutex>
#include <queue>
#include <thread>

namespace randpa_finality {

using ::fc::static_variant;

using mutex_guard = std::lock_guard<std::mutex>;

//---------- types ----------//

template <typename message_type>
class message_queue {
public:
    using message_ptr = std::shared_ptr<message_type>;

public:
    /// Add message to the queue.
    template <typename T>
    void push_message(const T& msg) {
        mutex_guard lock(_message_queue_mutex);

        auto randpa_msg = std::make_shared<message_type>(msg);
        _message_queue.push(randpa_msg);

        if (_need_notify) {
            _new_msg_cond.notify_one();
        }
    }

    /// Extract next message, or return nullptr (if empty).
    message_ptr get_next_msg() {
        mutex_guard lock(_message_queue_mutex);

        if (_message_queue.empty()) {
            _need_notify = true;
            return nullptr;
        } else {
            _need_notify = false;
        }

        const auto msg = _message_queue.front();
        _message_queue.pop();
        return msg;
    }

    /// Extract next message (if there is no one, wait until it appears in the queue).
    message_ptr get_next_msg_wait() {
        while (true) {
            if (_need_notify) {
                std::unique_lock<std::mutex> lk(_message_queue_mutex);

                _new_msg_cond.wait(lk, [this](){
                    return !_message_queue.empty() || _done;
                });
            }

            if (_done) {
                return nullptr;
            }

            auto msg = get_next_msg();
            if (msg) {
                return msg;
            }
        }
    }

    /// Finish working with queue.
    void terminate() {
        _done = true;
        _new_msg_cond.notify_one();
    }

    /// Get queue size.
    size_t size() const {
        mutex_guard lock(_message_queue_mutex);
        return _message_queue.size();
    }

private:
    std::atomic<bool> _need_notify { true };
    std::queue<message_ptr> _message_queue;
    std::atomic<bool> _done { false };

    mutable std::mutex _message_queue_mutex;
    std::condition_variable _new_msg_cond;
};


template <typename ...Args>
class channel {
public:
    using cb_type = std::function<void(Args...)>;

    void subscribe(cb_type && cb) {
        cbs.emplace_back(cb);
    }

    void send(Args... args) {
        for (auto && cb: cbs) {
            cb(std::forward<Args>(args)...);
        }
    }

protected:
    std::vector<cb_type> cbs;
};


/// RANDPA-specific network messages.
struct randpa_net_msg {
    uint32_t ses_id;
    randpa_net_msg_data data;
    fc::time_point_sec receive_time;
};

struct on_accepted_block_event {
    block_id_type block_id;
    block_id_type prev_block_id;
    public_key_type creator_key;
    std::set<public_key_type> active_bp_keys;
    bool sync;
};

struct on_irreversible_event {
    block_id_type block_id;
};

struct on_new_peer_event {
    uint32_t ses_id;
};

using randpa_event_data = static_variant<on_accepted_block_event, on_irreversible_event, on_new_peer_event>;

/// External events.
struct randpa_event {
    randpa_event_data data;
};


using randpa_message = static_variant<randpa_net_msg, randpa_event>;
using randpa_message_ptr = std::shared_ptr<randpa_message>;

using net_channel = channel<const randpa_net_msg&>;
using net_channel_ptr = std::shared_ptr<net_channel>;

using event_channel = channel<const randpa_event&>;
using event_channel_ptr = std::shared_ptr<event_channel>;

using finality_channel = channel<const block_id_type&>;
using finality_channel_ptr = std::shared_ptr<finality_channel>;
using lru_cache_type = boost::compute::detail::lru_cache<digest_type, boost::blank>;


class randpa {
public:
    static constexpr uint32_t round_width = 2;
    static constexpr uint32_t prevote_width = 1;
    static constexpr uint32_t msg_expiration_ms = 1000;

public:
    randpa()
            : _peer_messages{_messages_cache_size}
            , _self_messages{_messages_cache_size}
            , _last_proofs{_proofs_cache_size} {
        // 2 cases:
        //   full node:
        //     * sig provider with random-generated private key,
        //     * 0-initialized public key (will not be used for full nodes);
        //   block producer:
        //     * one or more user-defined sig providers,
        //     * corresponding public keys (to private keys stored in sig providers).
        //
        // A node considered to be a full-one, unless at least one signature provider defined.

        const auto default_priv_key = private_key_type::generate();

        _signature_providers.push_back(
            [default_priv_key](const digest_type& digest) {
                return default_priv_key.sign(digest);
            }
        );
        _public_keys.push_back(public_key_type{});
    }

    randpa& set_in_net_channel(const net_channel_ptr& ptr) {
        _in_net_channel = ptr;
        return *this;
    }

    randpa& set_out_net_channel(const net_channel_ptr& ptr) {
        _out_net_channel = ptr;
        return *this;
    }

    randpa& set_event_channel(const event_channel_ptr& ptr) {
        _in_event_channel = ptr;
        return *this;
    }

    randpa& set_finality_channel(const finality_channel_ptr& ptr) {
        _finality_channel = ptr;
        return *this;
    }

    randpa& set_signature_providers(const std::vector<signature_provider_type>& signature_providers,
                                    const std::vector<public_key_type>&         public_keys) {
        FC_ASSERT(_is_block_producer, "failed adding signature provider to the full node; use --producer-name option");
        if (!_is_bp_key_provided) {
            // no need to explicitly clear _signature_providers and _public_keys,
            // as they are reassigned in the following lines
            _is_bp_key_provided = true;
        }
        _signature_providers = signature_providers;
        _public_keys = public_keys;

        randpa_dlog("set signature providers for ${p}", ("p", public_keys));
        return *this;
    }

    void add_signature_provider(const signature_provider_type& signature_provider, const public_key_type& public_key) {
        FC_ASSERT(_is_block_producer, "failed adding signature provider to the full node; use --producer-name option");
        if (!_is_bp_key_provided) {
            FC_ASSERT(_signature_providers.size() == 1 && _public_keys.size() == 1,
                "changing from full-node case was expected");
            // remove default values, stored for full-node case
            _signature_providers.clear();
            _public_keys.clear();
            _is_bp_key_provided = true;
        }
        _signature_providers.push_back(signature_provider);
        _public_keys.push_back(public_key);

        randpa_dlog("added signature provider for ${p}", ("p", public_key));
    }

    void start(prefix_tree_ptr tree) {
        FC_ASSERT(_in_net_channel && _in_event_channel, "input channels should be initialized");
        FC_ASSERT(_out_net_channel, "output channel should be initialized");
        FC_ASSERT(_finality_channel, "finality channel should be initialized");

        _prefix_tree = tree;
        _lib = tree->get_root()->block_id;

#ifndef SYNC_RANDPA
        _thread_ptr.reset(new std::thread([this]() {
            randpa_wlog("Randpa thread started");
            loop();
            randpa_wlog("Randpa thread terminated");
        }));
#endif

        subscribe();
    }

    void stop() {
#ifndef SYNC_RANDPA
        _done = true;
        _message_queue.terminate();
        _thread_ptr->join();
#endif
    }

#ifndef SYNC_RANDPA
    const message_queue<randpa_message>& get_message_queue() const {
        return _message_queue;
    }
#endif

    prefix_tree_ptr get_prefix_tree() const {
        return _prefix_tree;
    }

    bool is_syncing() const {
        return _is_syncing;
    }

    bool is_frozen() const {
        return _is_frozen;
    }

    void set_type_block_producer() {
        _is_block_producer = true;
    }

private:
    static constexpr size_t _proofs_cache_size = 2; ///< how much last proofs to keep; @see _last_proofs
    static constexpr size_t _messages_cache_size = 100 * 100 * 100; // network msg cache size
    // See https://bit.ly/2Wp3Nsf
    // 2 / 3 * 102 * 12 (blocks per slot) * 2 rounds * 2 (additional)
    static constexpr int32_t _max_finality_lag_blocks = 69 * 12 * 2 * 2;

    std::unique_ptr<std::thread> _thread_ptr;
    std::atomic<bool> _done { false };
    std::vector<signature_provider_type> _signature_providers;
    std::vector<public_key_type> _public_keys;
    bool _is_bp_key_provided { false };      ///< true if user explicitly set at least one sig provider
    bool _is_block_producer { false };       ///< node is a block producer if run with --producer-name option
    prefix_tree_ptr _prefix_tree;
    randpa_round_ptr _round;
    block_id_type _lib;                      ///< last irreversible block
    uint32_t _last_prooved_block_num { 0 };
    std::map<public_key_type, uint32_t> _peers;
    lru_cache_type _peer_messages;
    lru_cache_type _self_messages;
    /// Proof data is invalidated after each round is finished, but other nodes will want to request
    /// proofs for that round; this cache holds some proofs to reply such requests.
    boost::circular_buffer<proof_type> _last_proofs;
    bool _is_syncing { false };              ///< syncing blocks from peers
    bool _is_frozen { false };               ///< freeze if dpos finality stops working

#ifndef SYNC_RANDPA
    message_queue<randpa_message> _message_queue;
#endif

    net_channel_ptr _in_net_channel;
    net_channel_ptr _out_net_channel;
    event_channel_ptr _in_event_channel;
    finality_channel_ptr _finality_channel;

    //

    void subscribe() {
        _in_net_channel->subscribe([&](const randpa_net_msg& msg) {
#ifdef SYNC_RANDPA
            process_msg(std::make_shared<randpa_message>(msg));
#else
            _message_queue.push_message(msg);
#endif
        });

        _in_event_channel->subscribe([&](const randpa_event& event) {
            randpa_dlog("Randpa received event, type: ${type}", ("type", event.data.which()));
#ifdef SYNC_RANDPA
            process_msg(std::make_shared<randpa_message>(event));
#else
            _message_queue.push_message(event);
#endif
        });
    }

    template <typename T>
    void send(uint32_t ses_id, const T & msg) {
        auto net_msg = randpa_net_msg { ses_id, msg };
        _out_net_channel->send(net_msg);
    }

    template <typename T>
    void bcast(const T & msg) {
        auto msg_hash = digest_type::hash(msg);
        if (_peer_messages.contains(msg_hash)) {
            return;
        }
        for (const auto& peer : _peers) {
            send(peer.second, msg);
        }
        _peer_messages.insert(msg_hash, {});
    }

#ifndef SYNC_RANDPA
    void loop() {
        while (true) {
            auto msg = _message_queue.get_next_msg_wait();

            if (_done) {
                break;
            }

            process_msg(msg);
        }
    }
#endif

    template <typename T>
    bool check_is_valid_msg(const T& msg) const {
        try {
            msg.public_keys();
        } catch (const fc::exception&) {
            return false;
        }
        return true;
    }

    // need handle all messages
    void process_msg(randpa_message_ptr msg_ptr) {
        const auto msg = *msg_ptr;
        switch (msg.which()) {
        case randpa_message::tag<randpa_net_msg>::value:
            process_net_msg(msg.get<randpa_net_msg>());
            break;
        case randpa_message::tag<randpa_event>::value:
            process_event(msg.get<randpa_event>());
            break;
        default:
            randpa_wlog("Randpa received unknown message, type: ${type}", ("type", msg.which()));
            break;
        }
    }

    /// Visitor for RANDPA various message types.
    class net_msg_handler: public fc::visitor<void> {
    public:
        net_msg_handler(randpa& randpa_ref, uint32_t ses_id)
            : _randpa(randpa_ref)
            , _ses_id(ses_id)
        {}

        template <typename T>
        void operator() (const T& msg) {
            if (_randpa.check_is_valid_msg(msg)) {
                _randpa.on(_ses_id, msg);
            }
        }

    private:
        randpa& _randpa;
        uint32_t _ses_id;
    };

    void process_net_msg(const randpa_net_msg& msg) {
        if (fc::time_point::now() - msg.receive_time > fc::milliseconds(msg_expiration_ms)) {
            randpa_wlog("Network message dropped, msg age: ${age}", ("age", fc::time_point::now() - msg.receive_time));
            return;
        }

        auto visitor = net_msg_handler(*this, msg.ses_id);
        msg.data.visit(visitor);
    }

    void process_event(const randpa_event& event) {
        const auto& data = event.data;
        switch (data.which()) {
        case randpa_event_data::tag<on_accepted_block_event>::value:
            on(data.get<on_accepted_block_event>());
            break;
        case randpa_event_data::tag<on_irreversible_event>::value:
            on(data.get<on_irreversible_event>());
            break;
        case randpa_event_data::tag<on_new_peer_event>::value:
            on(data.get<on_new_peer_event>());
            break;
        default:
            randpa_wlog("Randpa event received, but handler not found, type: ${type}", ("type", data.which()));
            break;
        }
    }

    bool validate_prevote(const prevote_type& prevote,
                          const public_key_type& prevoter_key,
                          const block_id_type& best_block,
                          const std::set<public_key_type>& bp_keys) const {
        if (prevote.base_block != best_block
            && std::find(prevote.blocks.begin(), prevote.blocks.end(), best_block) == prevote.blocks.end()) {
            randpa_dlog("Best block: ${id} was not found in prevote blocks", ("id", best_block));
        } else if (!bp_keys.count(prevoter_key)) {
            randpa_dlog("Prevoter public key is not in active bp keys: ${pub_key}", ("pub_key", prevoter_key));
        } else {
            return true;
        }

        return false;
    }

    bool validate_precommit(const precommit_type& precommit,
                            const public_key_type& precommiter_key,
                            const block_id_type& best_block,
                            const std::set<public_key_type>& bp_keys) const {
        if (precommit.block_id != best_block) {
            randpa_dlog("Precommit block ${pbid}, best block: ${bbid}",
                ("pbid", precommit.block_id)
                ("bbid", best_block));
        } else if (!bp_keys.count(precommiter_key)) {
            randpa_dlog("Precommitter public key is not in active bp keys: ${pub_key}", ("pub_key", precommiter_key));
        } else {
            return true;
        }

        return false;
    }

    bool validate_proof(const proof_type& proof) const {
        const auto best_block = proof.best_block;
        const auto node = _prefix_tree->find(best_block);

        if (!node) {
            randpa_dlog("Received proof for unknown block: ${block_id}", ("block_id", best_block));
            return false;
        }

        std::set<public_key_type> prevoted_keys, precommited_keys;
        const auto& bp_keys = node->active_bp_keys;

        for (const auto& prevote : proof.prevotes) {
            for (const auto& prevoter_pub_key : prevote.public_keys()) {
                if (!validate_prevote(prevote.data, prevoter_pub_key, best_block, bp_keys)) {
                    randpa_dlog("Prevote validation failed, base_block: ${id}, blocks: ${blocks}",
                        ("id", prevote.data.base_block)
                        ("blocks", prevote.data.blocks));
                    return false;
                }
                prevoted_keys.insert(prevoter_pub_key);
            }
        }

        for (const auto& precommit : proof.precommits) {
            for (const auto& precommiter_pub_key : precommit.public_keys()) {
                if (!prevoted_keys.count(precommiter_pub_key)) {
                    randpa_dlog("Precommiter has not prevoted, pub_key: ${pub_key}", ("pub_key", precommiter_pub_key));
                    return false;
                }

                if (!validate_precommit(precommit.data, precommiter_pub_key, best_block, bp_keys)) {
                    randpa_dlog("Precommit validation failed for ${id}", ("id", precommit.data.block_id));
                    return false;
                }
                precommited_keys.insert(precommiter_pub_key);
            }
        }
        return precommited_keys.size() > node->active_bp_keys.size() * 2 / 3;
    }

    void on(uint32_t ses_id, const prevote_msg& msg) {
        process_round_msg(ses_id, msg);
    }

    void on(uint32_t ses_id, const precommit_msg& msg) {
        process_round_msg(ses_id, msg);
    }

    void on(uint32_t ses_id, const finality_notice_msg& msg) {
        const auto& data = msg.data;
        randpa_dlog("Randpa finality_notice_msg received for block ${bid}", ("bid", data.best_block));
        if (is_active_bp(data.best_block) && get_block_num(data.best_block) <= _last_prooved_block_num) {
            randpa_dlog("no need to get finality proof for block producer node");
            return;
        }
        send(ses_id, finality_req_proof_msg{{data.round_num}, _signature_providers});
    }

    void on(uint32_t ses_id, const finality_req_proof_msg& msg) {
        const auto& data = msg.data;
        randpa_dlog("Randpa finality_req_proof_msg received for round ${r}", ("r", data.round_num));
        for (const auto& proof : _last_proofs) {
            if (proof.round_num == data.round_num) {
                randpa_dlog("proof found; sending it");
                send(ses_id, proof_msg{proof, _signature_providers});
                break;
            }
        }
    }

    void on(uint32_t ses_id, const proof_msg& msg) {
        const auto& proof = msg.data;
        randpa_dlog("Received proof for round ${num}", ("num", proof.round_num));

        if (_last_prooved_block_num >= get_block_num(proof.best_block)) {
            randpa_dlog("Skipping proof for ${id} cause last prooved block ${lpb} is higher",
                ("id", proof.best_block)
                ("lpb", _last_prooved_block_num));
            return;
        }

        if (get_block_num(_lib) >= get_block_num(proof.best_block)) {
            randpa_dlog("Skipping proof for ${id} cause lib ${lib} is higher",
                ("id", proof.best_block)
                ("lib", _lib));
            return;
        }

        if (_round && _round->get_state() == randpa_round::state_type::done) {
            randpa_dlog("Skipping proof for ${id} cause round ${num} is finished",
                ("id", proof.best_block)
                ("num", _round->get_num()));
            return;
        }

        if (!validate_proof(proof)) {
            for (const auto& public_key : msg.public_keys()) {
                randpa_ilog("Invalid proof among ${peer}", ("peer", public_key));
            }
            randpa_dlog("Proof msg: ${msg}", ("msg", msg));
            return;
        }

        randpa_ilog("Successfully validated proof for block ${id}", ("id", proof.best_block));

        if (_round && _round->get_num() == proof.round_num) {
            randpa_dlog("Gotta proof for round ${num}", ("num", _round->get_num()));
            _round->set_state(randpa_round::state_type::done);
        }
        on_proof_gained(proof);
    }

    void on(uint32_t ses_id, const handshake_msg& msg) {
        for (const auto& public_key : msg.public_keys()) {
            randpa_ilog("Randpa handshake_msg received, ses_id: ${ses_id}, from: ${pk}", ("ses_id", ses_id)("pk", public_key));
            try {
                _peers[public_key] = ses_id;
                send(ses_id, handshake_ans_msg(handshake_ans_type { _lib }, _signature_providers));
            } catch (const fc::exception& e) {
                randpa_elog("Randpa handshake_msg handler error, reason: ${e}", ("e", e.what()));
            }
        }
    }

    void on(uint32_t ses_id, const handshake_ans_msg& msg) {
        for (const auto& public_key : msg.public_keys()) {
            randpa_ilog("Randpa handshake_ans_msg received, ses_id: ${ses_id}, from: ${pk}", ("ses_id", ses_id)("pk", public_key));
            try {
                _peers[public_key] = ses_id;
            } catch (const fc::exception& e) {
                randpa_elog("Randpa handshake_ans_msg handler error, reason: ${e}", ("e", e.what()));
            }
        }
    }

    void on(const on_accepted_block_event& event) {
        randpa_dlog("Randpa on_accepted_block_event event handled, block_id: ${id}, num: ${num}, creator: ${c}, bp_keys: ${bpk}",
            ("id", event.block_id)
            ("num", get_block_num(event.block_id))
            ("c", event.creator_key)
            ("bpk", event.active_bp_keys)
        );

        try {
            _prefix_tree->insert({event.prev_block_id, {event.block_id}},
                                event.creator_key, event.active_bp_keys);
        } catch (const NodeNotFoundError& e) {
            randpa_elog("Randpa cannot insert block into tree, base_block: ${base_id}, block: ${id}",
                ("base_id", event.prev_block_id)
                ("id", event.block_id)
            );
            return;
        }

        _is_syncing = event.sync;
        _is_frozen = get_block_num(event.block_id) - get_block_num(_lib) > _max_finality_lag_blocks;

        if (event.sync) {
            randpa_ilog("Randpa omit block while syncing, id: ${id}", ("id", event.block_id));
            return;
        }

        if (should_start_round(event.block_id)) {
            randpa_dlog("starting new round");
            remove_round();
            randpa_dlog("current round removed");

            if (is_active_bp(event.block_id)) {
                new_round(round_num(event.block_id), event.creator_key);
                randpa_dlog("new round (${n}) started", ("n", _round->get_num()));
            }
        }

        if (should_end_prevote(event.block_id)) {
            _round->end_prevote();
        }
    }

    void on(const on_irreversible_event& event) {
        randpa_dlog("Randpa on_irreversible_event event handled, block_id: ${bid}, num: ${num}",
            ("bid", event.block_id)
            ("num", get_block_num(event.block_id))
        );

        if (get_block_num(event.block_id) <= get_block_num(_prefix_tree->get_root()->block_id)) {
            randpa_dlog("Randpa handled on_irreversible for old block: block_num: ${blk_num}",
                       ("blk_num", get_block_num(event.block_id))
            );
            return;
        }

        update_lib(event.block_id);
    }

    void on(const on_new_peer_event& event) {
        randpa_dlog("Randpa on_new_peer_event event handled, ses_id: ${ses_id}", ("ses_id", event.ses_id));
        auto msg = handshake_msg(handshake_type{_lib}, _signature_providers);
        send(event.ses_id, msg);
    }

    void on_proof_gained(const proof_type& proof) {
        _last_proofs.push_front(proof);
        randpa_dlog("cached proof for block ${b}", ("b", proof.best_block));

        _last_prooved_block_num = get_block_num(proof.best_block);
        _finality_channel->send(proof.best_block);
        bcast(finality_notice_msg{{proof.round_num, proof.best_block}, _signature_providers});
    }

    template <typename T>
    void process_round_msg(uint32_t ses_id, const T& msg) {
        if (_is_syncing || _is_frozen) {
            randpa_dlog("Randpa syncing or frozen");
            return;
        }

        const auto msg_hash = digest_type::hash(msg);
        if (_self_messages.contains(msg_hash)) {
            return;
        }

        _self_messages.insert(msg_hash, {});

        const auto last_round_num = round_num(_prefix_tree->get_head()->block_id);
        if (last_round_num == msg.data.round_num) {
            bcast(msg);
        }

        if (!_round) {
            randpa_dlog("Randpa round does not exist");
            return;
        }

        _round->on(msg);
    }

    uint32_t round_num(const block_id_type& block_id) const {
        return (get_block_num(block_id) - 1) / round_width;
    }

    uint32_t num_in_round(const block_id_type& block_id) const {
        return (get_block_num(block_id) - 1) % round_width;
    }

    bool should_start_round(const block_id_type& block_id) const {
        if (get_block_num(block_id) < 1) {
            return false;
        }

        if (!_round) {
            return true;
        }

        return round_num(block_id) > _round->get_num();
    }

    bool should_end_prevote(const block_id_type& block_id) const {
        if (!_round) {
            return false;
        }

        return round_num(block_id) == _round->get_num()
            && num_in_round(block_id) == prevote_width;
    }

    bool is_active_bp(const block_id_type& block_id) const {
        if (!_is_bp_key_provided) {
            return false;
        }

        randpa_dlog("bp key provided");

        auto node_ptr = _prefix_tree->find(block_id);

        if (!node_ptr) {
            randpa_dlog("block not found");
            return false;
        }

        for (const auto& public_key : _public_keys) {
            if (node_ptr->active_bp_keys.count(public_key)) {
                return true;
            }
        }
        return false;
    }

    void finish_round() {
        if (!_round) {
            return;
        }

        if (!_round->finish()) {
            return;
        }

        const auto& proof = _round->get_proof();
        randpa_ilog("Randpa round reached supermajority, round num: ${n}, best block id: ${b}, best block num: ${bn}",
            ("n", proof.round_num)
            ("b", proof.best_block)
            ("bn", get_block_num(proof.best_block))
        );

        if (get_block_num(_lib) < get_block_num(proof.best_block)) {
            on_proof_gained(proof);
            update_lib(proof.best_block);
        }
        randpa_dlog("round ${r} finished", ("r", _round->get_num()));
    }

    void new_round(uint32_t round_num, const public_key_type& primary) {
        _round.reset(new randpa_round(round_num, primary, _prefix_tree, _signature_providers,
            [this](const prevote_msg& msg) { bcast(msg); },
            [this](const precommit_msg& msg) { bcast(msg); },
            [this]() { finish_round(); }
        ));
    }

    void remove_round() {
        _peer_messages.clear();
        _self_messages.clear();
        _prefix_tree->remove_confirmations();
        _round.reset();
    }

    void update_lib(const block_id_type& lib_id) {
        auto node_ptr = _prefix_tree->find(lib_id);

        if (node_ptr) {
            _prefix_tree->set_root(node_ptr);
        } else {
            auto new_irb = std::make_shared<tree_node>(tree_node { lib_id });
            _prefix_tree->set_root(new_irb);
        }

        _lib = lib_id;
    }
};

} //namespace randpa_finality
