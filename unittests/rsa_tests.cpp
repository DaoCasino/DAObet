/**
 *  @file
 *  @copyright defined in eos/LICENSE.txt
 */
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wsign-compare"
#include <boost/test/unit_test.hpp>
#pragma GCC diagnostic pop

#include <eosio/testing/tester.hpp>
#include <eosio/chain/abi_serializer.hpp>

#include <Runtime/Runtime.h>

#include <fc/variant_object.hpp>
#include <fc/io/json.hpp>
#include <fc/crypto/sha256.hpp>

#include <openssl/rsa.h>
#include <openssl/pem.h>
#include <openssl/bio.h>
#include <openssl/x509.h>

#include <boost/algorithm/string/predicate.hpp>

#include <contracts.hpp>

#ifdef NON_VALIDATING_TEST
#define TESTER tester
#else
#define TESTER validating_tester
#endif

using namespace eosio;
using namespace eosio::chain;
using namespace eosio::testing;
using namespace fc;

using mvo = fc::mutable_variant_object;
using bytes = std::vector<char>;

using RSA_ptr = std::unique_ptr<RSA, decltype(&::RSA_free)>;
using BIO_ptr = std::unique_ptr<BIO, decltype(&::BIO_free)>;
using EVP_PKEY_ptr = std::unique_ptr<EVP_PKEY, decltype(&::EVP_PKEY_free)>;

class rsa_tester : public TESTER {
public:
    void deploy_contract() {
        create_accounts( {N(rsa)} );
        set_code( N(rsa), contracts::rsa_wasm() );
        set_abi( N(rsa), contracts::rsa_abi().data() );
    }

    RSA_ptr new_keys() {
        static bool init = true;
        if( init ) {
            ERR_load_crypto_strings();
            init = false;
        }

        auto rsa = RSA_ptr(RSA_generate_key(2048, 65537, NULL, NULL), ::RSA_free);
        return rsa;
    }

    std::string sign(const RSA_ptr& rsa, const sha256& digest) {
        bytes signature;
        signature.resize(RSA_size(rsa.get()));
        uint32_t len;
        RSA_sign(NID_sha256, (uint8_t*)digest.data(), 32,
                 (unsigned char*)signature.data(), &len, rsa.get());
        return fc::base64_encode(signature.data(), signature.size());
    }

    std::string pem_pubkey(const RSA_ptr& rsa) {
        auto pkey = EVP_PKEY_ptr(EVP_PKEY_new(), ::EVP_PKEY_free);
        EVP_PKEY_set1_RSA(pkey.get(), rsa.get());

        auto mem = BIO_ptr(BIO_new(BIO_s_mem()), ::BIO_free);
        PEM_write_bio_PUBKEY(mem.get(), pkey.get());

        BUF_MEM *bio_buf;
        BIO_get_mem_ptr(mem.get(), &bio_buf);

        std::stringstream ss({bio_buf->data, bio_buf->length});
        std::string pem;

        while (ss) {
            std::string tmp;
            std::getline(ss, tmp);
            if (tmp[0] != '-') {
                pem += tmp;
            }
        }

        return pem;
    }
};

BOOST_AUTO_TEST_SUITE(rsa_tests)

BOOST_FIXTURE_TEST_CASE( success, rsa_tester ) {
    deploy_contract();

    std::string message = "I'm message";
    fc::sha256 digest = fc::sha256::hash(message);
    auto keys = new_keys();
    auto signature = sign(keys, digest);

    push_action(N(rsa), N(verify), N(rsa), mvo()
        ("digest", digest)
        ("sig", signature)
        ("pubkey", pem_pubkey(keys))
    );
}

BOOST_FIXTURE_TEST_CASE( invalid_signature, rsa_tester ) {
    deploy_contract();

    std::string message = "I'm new message";
    fc::sha256 digest = fc::sha256::hash(message);
    auto keys = new_keys();
    auto keys2 = new_keys();
    auto signature = sign(keys2, digest); // <- signature by another key

    BOOST_CHECK_THROW(push_action(N(rsa), N(verify), N(rsa), mvo()
        ("digest", digest)
        ("sig", signature)
        ("pubkey", pem_pubkey(keys))
    ), fc::exception);
}

BOOST_FIXTURE_TEST_CASE( invalid_key, rsa_tester ) {
    deploy_contract();

    std::string message = "I'm new message";
    fc::sha256 digest = fc::sha256::hash(message);
    auto keys = new_keys();
    auto keys2 = new_keys();
    auto signature = sign(keys, digest);
    auto pem = pem_pubkey(keys2); // <- another key

    BOOST_CHECK_THROW(push_action(N(rsa), N(verify), N(rsa), mvo()
        ("digest", digest)
        ("sig", signature)
        ("pubkey", pem)
    ), fc::exception);
}

BOOST_FIXTURE_TEST_CASE( invalid_digest, rsa_tester ) {
    deploy_contract();

    std::string message = "I'm new message";
    std::string message2 = "I'm another new message";

    fc::sha256 digest = fc::sha256::hash(message);
    fc::sha256 digest2 = fc::sha256::hash(message2); // <- digest from another message

    auto keys = new_keys();
    auto signature = sign(keys, digest);

    BOOST_CHECK_THROW(push_action(N(rsa), N(verify), N(rsa), mvo()
        ("digest", digest2)
        ("sig", signature)
        ("pubkey", pem_pubkey(keys))
    ), fc::exception);
}

BOOST_FIXTURE_TEST_CASE( long_message, rsa_tester ) {
    deploy_contract();

    std::string message;
    for (auto i = 0; i < 1024; ++i) { // <- generate long message
        message += "A";
    }

    auto digest = fc::sha256::hash(message);
    auto keys = new_keys();
    auto signature = sign(keys, digest);

    push_action(N(rsa), N(verify), N(rsa), mvo()
        ("digest", digest)
        ("sig", signature)
        ("pubkey", pem_pubkey(keys))
    );
}

BOOST_AUTO_TEST_SUITE_END()
