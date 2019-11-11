#pragma once
#include "network_messages.hpp"
#include "round.hpp"

#include <fc/exception/exception.hpp>
#include <fc/io/json.hpp>

#include <boost/circular_buffer.hpp>

#include <queue>
#include <chrono>
#include <atomic>
#include <mutex>
#include <thread>
#include <condition_variable>

namespace randpa_finality {

using ::fc::static_variant;
using std::shared_ptr;
using std::unique_ptr;
using std::pair;

using mutex_guard = std::lock_guard<std::mutex>;


template <typename message_type>
class message_queue {
public:
    using message_ptr = std::shared_ptr<message_type>;

public:
    template <typename T>
    void push_message(const T& msg) {
        mutex_guard lock(_message_queue_mutex);

        auto randpa_msg = std::make_shared<message_type>(msg);
        _message_queue.push(randpa_msg);

        if (_need_notify) {
            _new_msg_cond.notify_one();
        }
    }

    message_ptr get_next_msg() {
        mutex_guard lock(_message_queue_mutex);

        if (!_message_queue.size()) {
            _need_notify = true;
            return nullptr;
        } else {
            _need_notify = false;
        }

        auto msg = _message_queue.front();
        _message_queue.pop();

        return msg;
    }

    message_ptr get_next_msg_wait() {
        while (true) {
            if (_need_notify) {
                std::unique_lock<std::mutex> lk(_message_queue_mutex);

                _new_msg_cond.wait(lk, [this](){
                    return (bool)_message_queue.size() || _done;
                });
            }

            if (_done)
                return nullptr;

            auto msg = get_next_msg();

            if (msg) {
                return msg;
            }
        }
    }

    void terminate() {
        _done = true;
        _new_msg_cond.notify_one();
    }

    auto size() {
        mutex_guard lock(_message_queue_mutex);
        return _message_queue.size();
    }

private:
    std::mutex _message_queue_mutex;
    bool _need_notify = true;
    std::condition_variable _new_msg_cond;
    std::queue<message_ptr> _message_queue;
    std::atomic<bool> _done { false };
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
struct randpa_event {
    randpa_event_data data;
};

using randpa_message = static_variant<randpa_net_msg, randpa_event>;
using randpa_message_ptr = shared_ptr<randpa_message>;


using net_channel = channel<const randpa_net_msg&>;
using net_channel_ptr = std::shared_ptr<net_channel>;

using event_channel = channel<const randpa_event&>;
using event_channel_ptr = std::shared_ptr<event_channel>;

using finality_channel = channel<const block_id_type&>;
using finality_channel_ptr = std::shared_ptr<finality_channel>;

class randpa {
public:
    static constexpr uint32_t round_width = 2;
    static constexpr uint32_t prevote_width = 1;
    static constexpr uint32_t msg_expiration_ms = 1000;

public:
    randpa()
        : _last_proofs{_proofs_cache_size}
    {
        auto private_key = private_key_type::generate();
        _signature_provider = [private_key](const digest_type& digest) {
            return private_key.sign(digest);
        };
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

    randpa& set_signature_provider(const signature_provider_type& signature_provider,
        const public_key_type& public_key) {
        _signature_provider = signature_provider;
        _public_key = public_key;
        _provided_bp_key = true;
        dlog("set signature provider: ${p}", ("p", public_key));
        return *this;
    }

    void start(prefix_tree_ptr tree) {
        FC_ASSERT(_in_net_channel && _in_event_channel, "in channels should be inited");
        FC_ASSERT(_out_net_channel, "out channels should be inited");
        FC_ASSERT(_finality_channel, "finality channel should be inited");

        _prefix_tree = tree;
        _lib = tree->get_root()->block_id;

#ifndef SYNC_RANDPA
        _thread_ptr.reset(new std::thread([this]() {
            wlog("Randpa thread started");
            loop();
            wlog("Randpa thread terminated");
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
    auto& get_message_queue() {
        return _message_queue;
    }
#endif

    prefix_tree_ptr get_prefix_tree() const {
        return _prefix_tree;
    }

private:
    static constexpr size_t _proofs_cache_size = 2; ///< how much last proofs to keep; @see _last_proofs

    std::unique_ptr<std::thread> _thread_ptr;
    std::atomic<bool> _done { false };
    signature_provider_type _signature_provider;
    public_key_type _public_key;
    prefix_tree_ptr _prefix_tree;
    randpa_round_ptr _round;
    block_id_type _lib;                              // last irreversible block
    uint32_t _last_prooved_block_num { 0 };
    std::map<public_key_type, uint32_t> _peers;
    std::map<public_key_type, std::set<digest_type>> _known_messages;
    bool _provided_bp_key { false };
    /// Proof data is invalidated after each round is finished, but other nodes will want to request
    /// proofs for that round; this cache holds some proofs to reply such requests.
    boost::circular_buffer<proof_type> _last_proofs;

#ifndef SYNC_RANDPA
    message_queue<randpa_message> _message_queue;
#endif

    net_channel_ptr _in_net_channel;
    net_channel_ptr _out_net_channel;
    event_channel_ptr _in_event_channel;
    finality_channel_ptr _finality_channel;

    void subscribe() {
        _in_net_channel->subscribe([&](const randpa_net_msg& msg) {
            dlog("Randpa received net message, type: ${type}", ("type", msg.data.which()));
#ifdef SYNC_RANDPA
            process_msg(std::make_shared<randpa_message>(msg));
#else
            _message_queue.push_message(msg);
#endif
        });

        _in_event_channel->subscribe([&](const randpa_event& event) {
            dlog("Randpa received event, type: ${type}", ("type", event.data.which()));
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
        dlog("Randpa net message sent, type: ${type}, ses_id: ${ses_id}",
            ("type", net_msg.data.which())
            ("ses_id", ses_id)
        );
        _out_net_channel->send(net_msg);
    }

    template <typename T>
    void bcast(const T & msg) {
        auto msg_hash = digest_type::hash(msg);
        for (const auto& peer: _peers) {
            if (!_known_messages[peer.first].count(msg_hash)) {
                send(peer.second, msg);
                _known_messages[peer.first].insert(msg_hash);
            }
        }
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
            msg.public_key();
        } catch (const fc::exception&) {
            return false;
        }
        return true;
    }

    // need handle all messages
    void process_msg(randpa_message_ptr msg_ptr) {
        auto msg = *msg_ptr;
        switch (msg.which()) {
            case randpa_message::tag<randpa_net_msg>::value:
                process_net_msg(msg.get<randpa_net_msg>());
                break;
            case randpa_message::tag<randpa_event>::value:
                process_event(msg.get<randpa_event>());
                break;
            default:
                wlog("Randpa received unknown message, type: ${type}", ("type", msg.which()));
                break;
        }
    }

    class net_msg_handler: public fc::visitor<void> {
    public:
        net_msg_handler(randpa& randpa_ref, uint32_t ses_id):
            _randpa(randpa_ref),
            _ses_id(ses_id)
        { }

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
            wlog("Network message dropped, msg age: ${age}", ("age", fc::time_point::now() - msg.receive_time));
            return;
        }

        auto visitor = net_msg_handler(*this, msg.ses_id);
        msg.data.visit(visitor);
    }

    void process_event(const randpa_event& event){
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
                wlog("Randpa event received, but handler not found, type: ${type}",
                    ("type", data.which())
                );
                break;
        }
    }

    bool validate_prevote(const prevote_type& prevote, const public_key_type& prevoter_key,
            const block_id_type& best_block, const set<public_key_type>& bp_keys) {
        if (prevote.base_block != best_block
            && std::find(prevote.blocks.begin(), prevote.blocks.end(), best_block) == prevote.blocks.end()) {
            dlog("Best block: ${id} was not found in prevote blocks", ("id", best_block));
        } else if (!bp_keys.count(prevoter_key)) {
            dlog("Prevoter public key is not in active bp keys: ${pub_key}",
                 ("pub_key", prevoter_key));
        } else {
            return true;
        }

        return false;
    }

    bool validate_precommit(const precommit_type& precommit, const public_key_type& precommiter_key,
            const block_id_type& best_block, const set<public_key_type>& bp_keys) {
        if (precommit.block_id != best_block) {
            dlog("Precommit block ${pbid}, best block: ${bbid}",
                 ("pbid", precommit.block_id)
                 ("bbid", best_block));
        } else if (!bp_keys.count(precommiter_key)) {
            dlog("Precommitter public key is not in active bp keys: ${pub_key}",
                 ("pub_key", precommiter_key));
        } else {
            return true;
        }

        return false;
    }

    bool validate_proof(const proof_type& proof) {
        auto best_block = proof.best_block;
        auto node = _prefix_tree->find(best_block);

        if (!node) {
            dlog("Received proof for unknown block: ${block_id}", ("block_id", best_block));
            return false;
        }

        set<public_key_type> prevoted_keys, precommited_keys;
        const auto& bp_keys = node->active_bp_keys;

        for (const auto& prevote : proof.prevotes) {
            const auto& prevoter_pub_key = prevote.public_key();
            if (!validate_prevote(prevote.data, prevoter_pub_key, best_block, bp_keys)) {
                dlog("Prevote validation failed, base_block: ${id}, blocks: ${blocks}",
                     ("id", prevote.data.base_block)
                     ("blocks", prevote.data.blocks));
                return false;
            }
            prevoted_keys.insert(prevoter_pub_key);
        }

        for (const auto& precommit : proof.precommits) {
            const auto& precommiter_pub_key = precommit.public_key();
            if (!prevoted_keys.count(precommiter_pub_key)) {
                dlog("Precommiter has not prevoted, pub_key: ${pub_key}", ("pub_key", precommiter_pub_key));
                return false;
            }

            if (!validate_precommit(precommit.data, precommiter_pub_key, best_block, bp_keys)) {
                dlog("Precommit validation failed for ${id}", ("id", precommit.data.block_id));
                return false;
            }
            precommited_keys.insert(precommiter_pub_key);
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
        dlog("Randpa finality_notice_msg received for block ${bid}", ("bid", data.best_block));
        if (is_active_bp(data.best_block) && get_block_num(data.best_block) <= _last_prooved_block_num) {
            dlog("no need to get finality proof for block producer node");
            return;
        }
        send(ses_id, finality_req_proof_msg{{data.round_num}, _signature_provider});
    }

    void on(uint32_t ses_id, const finality_req_proof_msg& msg) {
        const auto& data = msg.data;
        dlog("Randpa finality_req_proof_msg received for round ${r}", ("r", data.round_num));
        for (const auto& proof : _last_proofs) {
            if (proof.round_num == data.round_num) {
                dlog("proof found; sending it");
                send(ses_id, proof_msg{proof, _signature_provider});
                break;
            }
        }
    }

    void on(uint32_t ses_id, const proof_msg& msg) {
        const auto& proof = msg.data;
        dlog("Received proof for round ${num}", ("num", proof.round_num));

        if (_last_prooved_block_num >= get_block_num(proof.best_block)) {
            dlog("Skipping proof for ${id} cause last prooved block ${lpb} is higher",
                    ("id", proof.best_block)
                    ("lpb", _last_prooved_block_num));
            return;
        }

        if (get_block_num(_lib) >= get_block_num(proof.best_block)) {
            dlog("Skipping proof for ${id} cause lib ${lib} is higher",
                    ("id", proof.best_block)
                    ("lib", _lib));
            return;
        }

        if (_round && _round->get_state() == randpa_round::state::done) {
            dlog("Skipping proof for ${id} cause round ${num} is finished",
                 ("id", proof.best_block)
                 ("num", _round->get_num()));
            return;
        }

        if (!validate_proof(proof)) {
            ilog("Invalid proof from ${peer}", ("peer", msg.public_key()));
            dlog("Proof msg: ${msg}", ("msg", msg));
            return;
        }

        ilog("Successfully validated proof for block ${id}", ("id", proof.best_block));

        if (_round && _round->get_num() == proof.round_num) {
            dlog("Gotta proof for round ${num}", ("num", _round->get_num()));
            _round->set_state(randpa_round::state::done);
        }
        on_proof_gained(proof);
    }

    void on(uint32_t ses_id, const handshake_msg& msg) {
        ilog("Randpa handshake_msg received, ses_id: ${ses_id}, from: ${pk}", ("ses_id", ses_id)("pk", msg.public_key()));
        try {
            _peers[msg.public_key()] = ses_id;

            send(ses_id, handshake_ans_msg(handshake_ans_type { _lib }, _signature_provider));
        } catch (const fc::exception& e) {
            elog("Randpa handshake_msg handler error, reason: ${e}", ("e", e.what()));
        }
    }

    void on(uint32_t ses_id, const handshake_ans_msg& msg) {
        ilog("Randpa handshake_ans_msg received, ses_id: ${ses_id}, from: ${pk}", ("ses_id", ses_id)("pk", msg.public_key()));
        try {
            _peers[msg.public_key()] = ses_id;
        } catch (const fc::exception& e) {
            elog("Randpa handshake_ans_msg handler error, reason: ${e}", ("e", e.what()));
        }
    }

    void on(const on_accepted_block_event& event) {
        dlog("Randpa on_accepted_block_event event handled, block_id: ${id}, num: ${num}, creator: ${c}, bp_keys: ${bpk}",
            ("id", event.block_id)
            ("num", get_block_num(event.block_id))
            ("c", event.creator_key)
            ("bpk", event.active_bp_keys)
        );

        try {
            _prefix_tree->insert({event.prev_block_id, {event.block_id}},
                                event.creator_key, event.active_bp_keys);
        }
        catch (const NodeNotFoundError& e) {
            elog("Randpa cannot insert block into tree, base_block: ${base_id}, block: ${id}",
                ("base_id", event.prev_block_id)
                ("id", event.block_id)
            );
            return;
        }

        if (event.sync) {
            ilog("Randpa omit block while syncing, id: ${id}", ("id", event.block_id));
            return;
        }

        if (should_start_round(event.block_id)) {
            dlog("starting new round");
            remove_round();

            if (is_active_bp(event.block_id)) {
                new_round(round_num(event.block_id), event.creator_key);
            }
        }

        if (should_end_prevote(event.block_id)) {
            _round->end_prevote();
        }
    }

    void on(const on_irreversible_event& event) {
        dlog("Randpa on_irreversible_event event handled, block_id: ${bid}, num: ${num}",
            ("bid", event.block_id)
            ("num", get_block_num(event.block_id))
        );

        if (get_block_num(event.block_id) <= get_block_num(_prefix_tree->get_root()->block_id)) {
            wlog("Randpa handled on_irreversible for old block: block_num: ${blk_num}",
                ("blk_num", get_block_num(event.block_id))
            );
            return;
        }

        update_lib(event.block_id);
    }

    void on(const on_new_peer_event& event) {
        dlog("Randpa on_new_peer_event event handled, ses_id: ${ses_id}", ("ses_id", event.ses_id));
        auto msg = handshake_msg(handshake_type{_lib}, _signature_provider);
        send(event.ses_id, msg);
    }

    void on_proof_gained(const proof_type& proof) {
        _last_proofs.push_front(proof);
        dlog("cached proof for block ${b}", ("b", proof.best_block));

        _last_prooved_block_num = get_block_num(proof.best_block);
        _finality_channel->send(proof.best_block);
        bcast(finality_notice_msg{{proof.round_num, proof.best_block}, _signature_provider});
    }

    template <typename T>
    void process_round_msg(uint32_t ses_id, const T& msg) {
        auto last_round_num = round_num(_prefix_tree->get_head()->block_id);

        if (last_round_num == msg.data.round_num) {
            bcast(msg);
        }

        auto msg_hash = digest_type::hash(msg);

        if (_known_messages[_public_key].count(msg_hash)) {
            return;
        }

        if (!_round) {
            dlog("Randpa round does not exist");
            return;
        }

        _round->on(msg);
        _known_messages[_public_key].insert(msg_hash);
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
        if (!_provided_bp_key)
            return false;

        auto node_ptr = _prefix_tree->find(block_id);

        if (!node_ptr) {
            return false;
        }

        return node_ptr->active_bp_keys.count(_public_key);
    }

    void finish_round() {
        if (!_round) {
            return;
        }

        if (!_round->finish()) {
            return;
        }

        const auto& proof = _round->get_proof();
        ilog("Randpa round reached supermajority, round num: ${n}, best block id: ${b}, best block num: ${bn}",
            ("n", proof.round_num)
            ("b", proof.best_block)
            ("bn", get_block_num(proof.best_block))
        );

        if (get_block_num(_lib) < get_block_num(proof.best_block)) {
            on_proof_gained(proof);
            update_lib(proof.best_block);
        }
        dlog("round ${r} finished", ("r", _round->get_num()));
    }

    void new_round(uint32_t round_num, const public_key_type& primary) {
        _round.reset(new randpa_round(round_num, primary, _prefix_tree, _signature_provider,
        [this](const prevote_msg& msg) {
            bcast(msg);
        },
        [this](const precommit_msg& msg) {
            bcast(msg);
        },
        [this]() {
            finish_round();
        }));
    }

    void remove_round() {
        _known_messages.clear();
        _prefix_tree->remove_confirmations();
        _round.reset();
    }

    void update_lib(const block_id_type& lib_id) {
        auto node_ptr = _prefix_tree->find(lib_id);

        if (node_ptr) {
            _prefix_tree->set_root(node_ptr);
        }
        else {
            auto new_irb = std::make_shared<tree_node>(tree_node { lib_id });
            _prefix_tree->set_root(new_irb);
        }

        _lib = lib_id;
    }
};

} //namespace randpa_finality
