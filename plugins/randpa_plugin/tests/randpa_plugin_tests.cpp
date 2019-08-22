#include <eosio/randpa_plugin/prefix_chain_tree.hpp>
#include <eosio/randpa_plugin/network_messages.hpp>
#include <fc/crypto/sha256.hpp>
#include <boost/test/unit_test.hpp>
#include <eosio/testing/tester.hpp>
#include <vector>
#include <iostream>
#include <string>
#include <memory>

using namespace eosio;
using namespace chain;
using std::vector;
using namespace fc::crypto;
using namespace randpa_finality;

using tree_node = prefix_node<uint32_t>;
using prefix_tree = prefix_chain_tree<tree_node>;

static signature_provider_type make_key_signature_provider(const private_key_type& key) {
   return [key]( const digest_type& digest ) {
      return key.sign(digest);
   };
}

inline auto get_pub_key() {
    return private_key::generate().get_public_key();
}

BOOST_AUTO_TEST_SUITE(prefix_chain_tree_tests)

using blocks_type = vector<block_id_type>;

BOOST_AUTO_TEST_CASE(prefix_chain_one_node) try {
    auto lib_block_id = fc::sha256("beef");
    auto root = std::make_shared<tree_node>(tree_node{lib_block_id});
    prefix_tree tree(std::move(root));
    BOOST_REQUIRE_EQUAL(nullptr, tree.get_final_chain_head(1));
} FC_LOG_AND_RETHROW()

BOOST_AUTO_TEST_CASE(prefix_chain_two_nodes) try {
    auto lib_block_id = fc::sha256("beef");
    auto root = std::make_shared<tree_node>(tree_node{lib_block_id});
    auto chain = chain_type{lib_block_id,
                            vector<block_id_type>{fc::sha256("a")}};
    prefix_tree tree(std::move(root));
    tree.insert(chain, get_pub_key(), {});
    tree.add_confirmations(chain, get_pub_key(), 0);
    auto head = tree.get_final_chain_head(1);
    BOOST_TEST(head != nullptr);
    BOOST_TEST(head->block_id == fc::sha256("a"));
} FC_LOG_AND_RETHROW()

BOOST_AUTO_TEST_CASE(prefix_chain_test_longest) try {
    /*
     ++++++++A++++++++++++++
     ++++++++|\+++++++++++++
     ++++++++B+C++++++++++++
     ++++++++++|++++++++++++
     ++++++++++D++++++++++++
     */
    auto pub_key_1 = get_pub_key();
    auto pub_key_2 = get_pub_key();
    auto lib_block_id = fc::sha256("beef");
    auto root = std::make_shared<tree_node>(tree_node{lib_block_id});
    prefix_tree tree(std::move(root));
    std::map<char, block_id_type> blocks;
    for (char c = 'a'; c <= 'd'; c++) {
        blocks[c] = fc::sha256(std::string{c});
    }
    auto chain1 = chain_type { lib_block_id, blocks_type{blocks['a'], blocks['b']} };
    auto chain2 = chain_type { blocks['a'],  blocks_type{blocks['c'], blocks['d']} };
    tree.insert(chain1, pub_key_1, {});
    tree.add_confirmations(chain1, pub_key_1, 0);
    tree.insert(chain2, pub_key_1, {});
    tree.add_confirmations(chain2, pub_key_1, 0);

    auto chain3 = chain_type {lib_block_id, vector<block_id_type>{blocks['a'], blocks['b']}};
    tree.insert(chain3, pub_key_2, {});
    tree.add_confirmations(chain3, pub_key_2, 0);
    BOOST_TEST(blocks['b'] == tree.get_final_chain_head(2)->block_id);

    auto chain4 = chain_type {blocks['c'], blocks_type{blocks['d']}};
    tree.insert(chain4, pub_key_2, {});
    tree.add_confirmations(chain4, pub_key_2, 0);
    BOOST_TEST(blocks['d'] == tree.get_final_chain_head(2)->block_id);

} FC_LOG_AND_RETHROW()

BOOST_AUTO_TEST_CASE(prefix_chain_internal) try {
    auto pub_key_1 = get_pub_key();
    auto pub_key_2 = get_pub_key();

    auto lib_block_id = fc::sha256("beef");
    auto node = tree_node{lib_block_id};

    prefix_tree tree(std::make_shared<tree_node>(node));
    auto chain = chain_type{lib_block_id, blocks_type{fc::sha256("abc"), fc::sha256("def")}};
    tree.insert(chain, pub_key_1, {});
    tree.add_confirmations(chain, pub_key_1, 0);

    const auto root = tree.get_root();
    BOOST_REQUIRE_EQUAL(lib_block_id, root->block_id);
    BOOST_REQUIRE_EQUAL(1, root->adjacent_nodes.size());

    const auto chain_first_node = root->adjacent_nodes[0];
    BOOST_TEST(tree.find(fc::sha256("abc")) == chain_first_node);
    BOOST_TEST(chain_first_node->adjacent_nodes[0] == tree.get_final_chain_head(1));

    // add second chain
    chain = chain_type{fc::sha256("abc"), blocks_type{fc::sha256("bbc")}};
    tree.insert(chain, pub_key_2, {});
    tree.add_confirmations(chain, pub_key_2, 0);

    BOOST_REQUIRE_EQUAL(2, chain_first_node->adjacent_nodes.size());
    BOOST_TEST(chain_first_node == tree.get_final_chain_head(2));

} FC_LOG_AND_RETHROW()

BOOST_AUTO_TEST_SUITE_END()


BOOST_AUTO_TEST_SUITE(prevote_tests)

BOOST_AUTO_TEST_CASE(prevote_validate_success) try {
    auto priv_key = private_key::generate();
    auto pub_key = priv_key.get_public_key();

    auto prevote = prevote_type {
        0,
        fc::sha256("a"),
        { fc::sha256("b"), fc::sha256("c"), fc::sha256("d") }
    };

    auto msg = prevote_msg(prevote, make_key_signature_provider(priv_key));

    BOOST_TEST(prevote.round_num == msg.data.round_num);
    BOOST_TEST(prevote.base_block == msg.data.base_block);
    BOOST_TEST(prevote.blocks == msg.data.blocks);
    BOOST_TEST(true == msg.validate(pub_key));

} FC_LOG_AND_RETHROW()

BOOST_AUTO_TEST_CASE(prevote_validate_fail) try {
    auto priv_key = private_key::generate();
    auto pub_key = priv_key.get_public_key();

    auto prevote = prevote_type {
        0,
        fc::sha256("a"),
        { fc::sha256("b"), fc::sha256("c"), fc::sha256("d") }
    };

    auto msg = prevote_msg(prevote, make_key_signature_provider(priv_key));

    auto priv_key_2 = private_key::generate();
    auto pub_key_2 = priv_key_2.get_public_key();

    BOOST_TEST(prevote.round_num == msg.data.round_num);
    BOOST_TEST(prevote.base_block == msg.data.base_block);
    BOOST_TEST(prevote.blocks == msg.data.blocks);
    BOOST_TEST(false == msg.validate(pub_key_2));

} FC_LOG_AND_RETHROW()

BOOST_AUTO_TEST_SUITE_END()


BOOST_AUTO_TEST_SUITE(precommit_tests)

BOOST_AUTO_TEST_CASE(precommit_validate_success) try {
    auto priv_key = private_key::generate();
    auto pub_key = priv_key.get_public_key();

    auto precommit = precommit_type {
        0,
        fc::sha256("a")
    };

    auto msg = precommit_msg(precommit, make_key_signature_provider(priv_key));

    BOOST_TEST(precommit.round_num == msg.data.round_num);
    BOOST_TEST(precommit.block_id == msg.data.block_id);
    BOOST_TEST(true == msg.validate(pub_key));

} FC_LOG_AND_RETHROW()

BOOST_AUTO_TEST_CASE(precommit_validate_fail) try {
    auto priv_key = private_key::generate();
    auto pub_key = priv_key.get_public_key();

    auto precommit = precommit_type {
        0,
        fc::sha256("a")
    };

    auto msg = precommit_msg(precommit, make_key_signature_provider(priv_key));

    auto priv_key_2 = private_key::generate();
    auto pub_key_2 = priv_key_2.get_public_key();

    BOOST_TEST(precommit.round_num == msg.data.round_num);
    BOOST_TEST(precommit.block_id == msg.data.block_id);
    BOOST_TEST(false == msg.validate(pub_key_2));

} FC_LOG_AND_RETHROW()

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE(last_inserted_block_test)

BOOST_AUTO_TEST_CASE(get_last_inserted_block) try {
    auto lib_block_id = fc::sha256("beef");
    auto root = std::make_shared<tree_node>(tree_node{lib_block_id});
    auto chain1 = chain_type{lib_block_id, {fc::sha256("a")}};
    auto chain2 = chain_type{fc::sha256("a"), {fc::sha256("b")}};
    auto pub_key1 = get_pub_key();
    auto pub_key2 = get_pub_key();
    auto unknown_pub_key = get_pub_key();
    prefix_tree tree(std::move(root));
    tree.insert(chain1, pub_key1, {});
    tree.insert(chain2, pub_key2, {});
    BOOST_TEST(tree.get_last_inserted_block(pub_key1)->block_id == fc::sha256("a"));
    BOOST_TEST(tree.get_last_inserted_block(pub_key2)->block_id == fc::sha256("b"));
    BOOST_TEST(not tree.get_last_inserted_block(unknown_pub_key));
    tree.set_root(tree.find(fc::sha256("b")));
    BOOST_TEST(not tree.get_last_inserted_block(pub_key1));
} FC_LOG_AND_RETHROW()

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE(remove_confirmations)

BOOST_AUTO_TEST_CASE(remove_confirmations_test) try {
    auto lib_block_id = fc::sha256("beef");
    auto root = std::make_shared<tree_node>(tree_node{lib_block_id});
    auto chain = chain_type{lib_block_id,
                            vector<block_id_type>{fc::sha256("a")}};
    prefix_tree tree(std::move(root));
    tree.insert(chain, get_pub_key(), {});
    tree.add_confirmations(chain, get_pub_key(), 0);
    auto head = tree.get_final_chain_head(1);
    BOOST_TEST(head);
    BOOST_TEST(head->block_id == fc::sha256("a"));

    tree.remove_confirmations();
    head = tree.get_final_chain_head(1);
    BOOST_TEST(!head);
} FC_LOG_AND_RETHROW()

BOOST_AUTO_TEST_SUITE_END()