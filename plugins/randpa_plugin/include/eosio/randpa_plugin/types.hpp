#pragma once

#include <fc/container/flat_fwd.hpp>
#include <fc/io/varint.hpp>
#include <fc/io/enum_type.hpp>
#include <fc/crypto/sha224.hpp>
#include <fc/optional.hpp>
#include <fc/safe.hpp>
#include <fc/container/flat.hpp>
#include <fc/string.hpp>
#include <fc/io/raw.hpp>
#include <fc/static_variant.hpp>
#include <fc/smart_ref_fwd.hpp>
#include <fc/crypto/ripemd160.hpp>
#include <fc/fixed_string.hpp>
#include <fc/crypto/private_key.hpp>
#include <fc/bitutil.hpp>

namespace randpa_finality {

using public_key_type = fc::crypto::public_key;
using private_key_type = fc::crypto::private_key;
using signature_type = fc::crypto::signature;
using block_id_type = fc::sha256;
using digest_type = fc::sha256;
using signature_provider_type = std::function<signature_type(digest_type)>;

uint32_t get_block_num(const block_id_type& id) {
    return fc::endian_reverse_u32(id._hash[0]);
}

} //namespace randpa_finality