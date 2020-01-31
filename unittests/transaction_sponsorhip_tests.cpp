#include <boost/test/unit_test.hpp>

#include <eosio/testing/tester.hpp>
#include <eosio/chain/exceptions.hpp>
#include <eosio/chain/account_object.hpp>
#include <eosio/chain/config.hpp>
#include <eosio/testing/chainbase_fixture.hpp>
#include <eosio/chain/resource_limits.hpp>

#include "eosio_system_tester.hpp"

#include <fc/exception/exception.hpp>

using namespace eosio::testing;
using namespace eosio::chain;
using namespace eosio_system;

auto get_sponsored_transaction(
    tester& chain,
    account_name sender, private_key_type sender_pk,
    account_name payer,  private_key_type payer_pk) {

    action act;
    act.account = sender;
    act.name = N(null);
    act.authorization = vector<permission_level>{
        {sender, config::active_name},
    };

    signed_transaction trx;
    trx.actions.emplace_back(std::move(act));
    chain.set_transaction_headers(trx);
    trx.transaction_extensions.push_back(extension_type{0, fc::raw::pack<trx_sponsor_ext>({payer})});

    trx.sign(sender_pk, chain.control->get_chain_id());
    trx.sign(payer_pk, chain.control->get_chain_id());
    return trx;
}

BOOST_AUTO_TEST_SUITE(sponsorship_tests)
    BOOST_AUTO_TEST_CASE(sponsor_success) try {
        tester chain;

        const auto& alice = N(alice);
        const auto& bob = N(bob);

        chain.produce_blocks();
        chain.create_account(alice);
        chain.create_account(bob);

        chain.push_action(config::system_account_name, N(setalimits), config::system_account_name, fc::mutable_variant_object()
        ("account", name(alice).to_string())
        ("ram_bytes", 10000)
        ("net_weight", 1000)
        ("cpu_weight", 1000));

        chain.push_action(config::system_account_name, N(setalimits), config::system_account_name, fc::mutable_variant_object()
        ("account", name(bob).to_string())
        ("ram_bytes", 10000)
        ("net_weight", 1000)
        ("cpu_weight", 1000));

        const resource_limits_manager& mgr = chain.control->get_resource_limits_manager();

        chain.produce_blocks();

        auto alice_cpu_limit_before = mgr.get_account_cpu_limit_ex(alice);
        auto bob_cpu_limit_before = mgr.get_account_cpu_limit_ex(bob);

        auto trx = get_sponsored_transaction(
            chain,
            alice, tester::get_private_key(alice, "active"),
            bob, tester::get_private_key(bob, "active")
        );

        chain.push_transaction(trx);

        auto alice_cpu_limit_after = mgr.get_account_cpu_limit_ex(alice);
        auto bob_cpu_limit_after = mgr.get_account_cpu_limit_ex(bob);
        BOOST_CHECK(alice_cpu_limit_before.used == alice_cpu_limit_after.used);
        // bob's bandwidth was consumed
        BOOST_CHECK(bob_cpu_limit_before.used < bob_cpu_limit_after.used);
    } FC_LOG_AND_RETHROW();

    BOOST_AUTO_TEST_CASE(sponsor_wrong_signature) try {
        tester chain;

        const auto& alice = N(alice);
        const auto& bob = N(bob);

        chain.produce_blocks();
        chain.create_account(alice);
        chain.create_account(bob);

        chain.push_action(config::system_account_name, N(setalimits), config::system_account_name, fc::mutable_variant_object()
        ("account", name(alice).to_string())
        ("ram_bytes", 10000)
        ("net_weight", 1000)
        ("cpu_weight", 1000));

        chain.push_action(config::system_account_name, N(setalimits), config::system_account_name, fc::mutable_variant_object()
        ("account", name(bob).to_string())
        ("ram_bytes", 10000)
        ("net_weight", 1000)
        ("cpu_weight", 1000));

        const resource_limits_manager& mgr = chain.control->get_resource_limits_manager();

        chain.produce_blocks();

        auto alice_cpu_limit_before = mgr.get_account_cpu_limit_ex(alice);
        auto bob_cpu_limit_before = mgr.get_account_cpu_limit_ex(bob);

        auto trx = get_sponsored_transaction(
            chain,
            alice, tester::get_private_key(alice, "active"),
            bob, tester::get_private_key(config::system_account_name, "active")
        );
        
        BOOST_CHECK_THROW(chain.push_transaction(trx), unsatisfied_authorization);
    } FC_LOG_AND_RETHROW()

    BOOST_AUTO_TEST_CASE(sponsor_is_actor) try {
        tester chain;

        const auto& alice = N(alice);

        chain.produce_blocks();
        chain.create_account(alice);

        chain.push_action(config::system_account_name, N(setalimits), config::system_account_name, fc::mutable_variant_object()
        ("account", name(alice).to_string())
        ("ram_bytes", 10000)
        ("net_weight", 1000)
        ("cpu_weight", 1000));

        const resource_limits_manager& mgr = chain.control->get_resource_limits_manager();

        chain.produce_blocks();

        auto alice_cpu_limit_before = mgr.get_account_cpu_limit_ex(alice);

        auto trx = get_sponsored_transaction(
            chain,
            alice, tester::get_private_key(alice, "active"),
            alice, tester::get_private_key(alice, "active")
        );

        BOOST_CHECK_THROW(chain.push_transaction(trx), tx_duplicate_sig);
    } FC_LOG_AND_RETHROW()


    BOOST_AUTO_TEST_CASE(actor_wrong_signature) try {
        tester chain;

        const auto& alice = N(alice);
        const auto& bob = N(bob);

        chain.produce_blocks();
        chain.create_account(alice);
        chain.create_account(bob);

        chain.push_action(config::system_account_name, N(setalimits), config::system_account_name, fc::mutable_variant_object()
        ("account", name(alice).to_string())
        ("ram_bytes", 10000)
        ("net_weight", 1000)
        ("cpu_weight", 1000));

        chain.push_action(config::system_account_name, N(setalimits), config::system_account_name, fc::mutable_variant_object()
        ("account", name(bob).to_string())
        ("ram_bytes", 10000)
        ("net_weight", 1000)
        ("cpu_weight", 1000));

        const resource_limits_manager& mgr = chain.control->get_resource_limits_manager();

        chain.produce_blocks();

        auto alice_cpu_limit_before = mgr.get_account_cpu_limit_ex(alice);
        auto bob_cpu_limit_before = mgr.get_account_cpu_limit_ex(bob);

        auto trx = get_sponsored_transaction(
            chain,
            alice, tester::get_private_key(config::system_account_name, "active"),
            bob, tester::get_private_key(bob, "active")
        );
        BOOST_CHECK_THROW(chain.push_transaction(trx), unsatisfied_authorization);
    } FC_LOG_AND_RETHROW()

BOOST_AUTO_TEST_SUITE_END()