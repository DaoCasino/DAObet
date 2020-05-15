#pragma once

#include <eosio/eosio.hpp>
#include <eosio/transaction.hpp>

extern "C" {
    __attribute__((eosio_wasm_import))
    int rsa_verify( const capi_checksum256* digest, const char* sig,
                   size_t siglen, const char* pub, size_t publen );
}

namespace daobet {
    bool rsa_verify(const eosio::checksum256& digest, const std::string& sig, const std::string& pubkey) {
        auto digest_data = digest.extract_as_byte_array();
        return ::rsa_verify(reinterpret_cast<const eosio::checksum256*>(digest_data.data()),
                            sig.c_str(), sig.size(), pubkey.c_str(), pubkey.size());
    }
}


class [[eosio::contract]] rsa : public eosio::contract {
public:
    using eosio::contract::contract;

    [[eosio::action]]
    void verify(const eosio::checksum256& digest, const std::string& sig, const std::string& pubkey);
};
