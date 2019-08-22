#pragma once

#include "types.hpp"
#include <fc/reflect/reflect.hpp>

namespace randpa_finality {

using std::vector;


template<class T>
class network_msg {
public:
    T data;
    signature_type signature;
    network_msg() = default;
    network_msg(const T& data_, const signature_type& signature_): data(data_), signature(signature_) {}
    network_msg(const T& data_, signature_type&& signature_): data(data_), signature(signature_) {}
    network_msg(const T& data_, const signature_provider_type& signature_provider) {
        data = data_;
        signature = signature_provider(hash());
    }

    digest_type hash() const {
        return digest_type::hash(data);
    }

    public_key_type public_key() const {
        return public_key_type(signature, hash());
    }

    bool validate(const public_key_type& pub_key) const {
        return public_key() == pub_key;
    }
};


struct block_get_conf_type {
    block_id_type block_id;
};

struct handshake_type {
    block_id_type lib;
};

struct handshake_ans_type {
    block_id_type lib;
};

struct prevote_type {
    uint32_t round_num;
    block_id_type base_block;
    vector<block_id_type> blocks;
};

struct precommit_type {
    uint32_t round_num;
    block_id_type block_id;
};

using handshake_msg = network_msg<handshake_type>;
using handshake_ans_msg = network_msg<handshake_ans_type>;

using block_get_conf_msg = network_msg<block_get_conf_type>;

using prevote_msg = network_msg<prevote_type>;
using precommit_msg = network_msg<precommit_type>;

struct proof_type {
    uint32_t round_num;
    block_id_type best_block;
    std::vector<prevote_msg> prevotes;
    std::vector<precommit_msg> precommits;
};

using proof_msg = network_msg<proof_type>;

using randpa_net_msg_data = ::fc::static_variant<handshake_msg, handshake_ans_msg,
                                                 prevote_msg, precommit_msg, proof_msg>;


} //namespace randpa_finality

FC_REFLECT(randpa_finality::prevote_type, (round_num)(base_block)(blocks))
FC_REFLECT(randpa_finality::precommit_type, (round_num)(block_id))
FC_REFLECT(randpa_finality::proof_type, (round_num)(best_block)(prevotes)(precommits));

FC_REFLECT(randpa_finality::block_get_conf_type, (block_id))
FC_REFLECT(randpa_finality::handshake_type, (lib))
FC_REFLECT(randpa_finality::handshake_ans_type, (lib))

FC_REFLECT_TEMPLATE((typename T), randpa_finality::network_msg<T>, (data)(signature))
