#pragma once

#include "types.hpp"

#include <fc/reflect/reflect.hpp>

#include <typeinfo>

namespace randpa_finality {

template<class T>
class network_msg {
public:
    T data;
    std::vector<signature_type> signatures;
    network_msg() = default;
    network_msg(const T& data_, const std::vector<signature_type>& signatures_): data(data_), signatures(signatures_) {}
    network_msg(const T& data_, std::vector<signature_type>&& signatures_): data(data_), signatures(signatures_) {}
    network_msg(const T& data_, const std::vector<signature_provider_type>& signature_providers) {
        data = data_;
        for(const auto &sig_prov : signature_providers) {
            signatures.push_back(sig_prov(hash()));
        }
    }

    digest_type hash() const {
        digest_type::encoder e;
        fc::raw::pack(e, data);
        fc::raw::pack(e, typeid(T).name());
        return e.result();
    }

    std::vector<public_key_type> public_keys() const {
        std::vector<public_key_type> public_keys;
        for (const auto& sign : signatures) {
            public_keys.push_back(public_key_type(sign, hash()));
        }
        return public_keys;
    }

    bool validate(const std::vector<public_key_type>& pub_keys) const {
        return pub_keys == public_keys();
    }
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
    std::vector<block_id_type> blocks;
};

struct precommit_type {
    uint32_t round_num;
    block_id_type block_id;
};

struct finality_notice_type {
    uint32_t round_num;
    block_id_type best_block;
};

struct finality_req_proof_type {
    uint32_t round_num;
};

using handshake_msg = network_msg<handshake_type>;
using handshake_ans_msg = network_msg<handshake_ans_type>;

using prevote_msg = network_msg<prevote_type>;
using precommit_msg = network_msg<precommit_type>;

struct proof_type {
    uint32_t round_num;
    block_id_type best_block;
    std::vector<prevote_msg> prevotes;
    std::vector<precommit_msg> precommits;
};

using proof_msg = network_msg<proof_type>;
using finality_notice_msg = network_msg<finality_notice_type>;
using finality_req_proof_msg = network_msg<finality_req_proof_type>;

using randpa_net_msg_data = ::fc::static_variant<handshake_msg,
                                                 handshake_ans_msg,
                                                 prevote_msg,
                                                 precommit_msg,
                                                 proof_msg,
                                                 finality_notice_msg,
                                                 finality_req_proof_msg>;

} //namespace randpa_finality

FC_REFLECT(randpa_finality::handshake_type, (lib))
FC_REFLECT(randpa_finality::handshake_ans_type, (lib))

FC_REFLECT(randpa_finality::prevote_type, (round_num)(base_block)(blocks))
FC_REFLECT(randpa_finality::precommit_type, (round_num)(block_id))

FC_REFLECT(randpa_finality::proof_type, (round_num)(best_block)(prevotes)(precommits))
FC_REFLECT(randpa_finality::finality_notice_type, (round_num)(best_block))
FC_REFLECT(randpa_finality::finality_req_proof_type, (round_num))

FC_REFLECT_TEMPLATE((typename T), randpa_finality::network_msg<T>, (data)(signatures))
