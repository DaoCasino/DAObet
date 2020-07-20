// ./scripts/build.sh --build-type Debug && ./build/simulator/simulator --gtest_filter=randpa_finality.three_nodes

#include "randpa.hpp"
#include "simulator.hpp"

#include <gtest/gtest.h>

#include <cstdlib>
#include <ctime>
#include <iostream>
#include <random>
#include <string>

using std::vector;

static constexpr auto FN = node_type_t::FN;
static constexpr auto BP = node_type_t::BP;

TEST(randpa_finality, three_nodes) {
    auto runner = TestRunner(3);
    vector<pair<int, int>> v0{{1, 2}, {2, 10}};
    graph_type g;
    g.push_back(v0);
    runner.load_graph(g);
    runner.add_stop_task(2 * runner.get_slot_ms());
    runner.run<RandpaNode>();
    EXPECT_EQ(get_block_height(runner.get_db(0).last_irreversible_block_id()), 1);
    EXPECT_EQ(get_block_height(runner.get_db(1).last_irreversible_block_id()), 1);
    EXPECT_EQ(get_block_height(runner.get_db(2).last_irreversible_block_id()), 1);
}

TEST(randpa_finality, three_nodes_large_roundtrip) {
    auto runner = TestRunner(3);
    vector<pair<int, int>> v0{{1, 2}};
    graph_type g;
    g.push_back(v0);
    runner.load_graph(g);
    runner.add_stop_task(5 * runner.get_slot_ms());
    runner.add_update_delay_task(1 * runner.get_slot_ms(), 0, 2, 10);
    runner.run<RandpaNode>();
    EXPECT_GE(get_block_height(runner.get_db(0).last_irreversible_block_id()), 2);
    EXPECT_GE(get_block_height(runner.get_db(1).last_irreversible_block_id()), 2);
    EXPECT_GE(get_block_height(runner.get_db(2).last_irreversible_block_id()), 2);
}

TEST(randpa_finality, many_nodes) {
    size_t nodes_amount = 21;
    auto runner = TestRunner(nodes_amount);
    vector<pair<int, int>> v0{{1, 20}, {2, 10}, {3, 10}, {4, 30}, {5, 30}};
    vector<pair<int, int>> v5{{6, 10}, {7, 30}, {8, 20}, {9, 10}, {10, 30}};
    vector<pair<int, int>> v10{{11, 10}, {12, 10}, {13, 10}, {14, 10}, {15, 30}};
    vector<pair<int, int>> v15{{16, 10}, {17, 10}, {18, 10}, {19, 10}, {20, 30}};
    graph_type g(nodes_amount);
    g[0] = v0;
    g[5] = v5;
    g[10] = v10;
    g[15] = v15;
    runner.load_graph(g);
    runner.add_stop_task(18 * runner.get_slot_ms());
    runner.run<RandpaNode>();
    EXPECT_EQ(get_block_height(runner.get_db(0).last_irreversible_block_id()), 17);
    EXPECT_EQ(get_block_height(runner.get_db(5).last_irreversible_block_id()), 17);
    EXPECT_EQ(get_block_height(runner.get_db(10).last_irreversible_block_id()), 17);
    EXPECT_EQ(get_block_height(runner.get_db(19).last_irreversible_block_id()), 17);
}

TEST(randpa_finality, finalize_long_chain) {
    auto runner = TestRunner(3);
    vector<pair<int, int>> v0{{1, 2}};
    graph_type g;
    g.push_back(v0);
    runner.load_graph(g);
    runner.add_update_delay_task(6 * runner.get_slot_ms(), 0, 2, 10);
    runner.add_stop_task(6 * runner.get_slot_ms());
    runner.run<RandpaNode>();
    EXPECT_EQ(get_block_height(runner.get_db(0).last_irreversible_block_id()), 0);
    EXPECT_EQ(get_block_height(runner.get_db(1).last_irreversible_block_id()), 0);
    EXPECT_EQ(get_block_height(runner.get_db(2).last_irreversible_block_id()), 0);

    runner.add_stop_task(11 * runner.get_slot_ms());
    runner.run_loop();
    EXPECT_EQ(get_block_height(runner.get_db(0).last_irreversible_block_id()), 7);
    EXPECT_EQ(get_block_height(runner.get_db(1).last_irreversible_block_id()), 7);
    EXPECT_EQ(get_block_height(runner.get_db(2).last_irreversible_block_id()), 7);
}

TEST(randpa_finality, random_delays) {
    auto nodes_cnt = rand() % 9 + 1;
    auto runner = TestRunner(nodes_cnt);

    auto max_delay = 500 / nodes_cnt;
    auto min_delay = 10;

    auto random = [&](){ return (rand() % (max_delay - min_delay)) + min_delay; };

    graph_type g;
    for (auto i = 0; i < nodes_cnt; i++) {
        vector<pair<int, int>> pairs;
        for (auto j = i + 1; j < nodes_cnt; j++) {
            pairs.push_back({ j, random() });
        }
        g.push_back(pairs);
    }
    runner.load_graph(g);

    runner.add_stop_task(5 * runner.get_slot_ms());
    runner.run<RandpaNode>();

    for (auto i = 0; i < nodes_cnt; i++) {
        EXPECT_EQ(get_block_height(runner.get_db(i).last_irreversible_block_id()), 3);
    }
}

TEST(randpa_finality, star_topology) {
    auto nodes_cnt = 10;
    auto runner = TestRunner(nodes_cnt);

    auto delay = 100;

    graph_type g;
    g.push_back({});
    for (auto i = 1; i < nodes_cnt; i++) {
        g.push_back({{ 0, delay }});
    }
    runner.load_graph(g);

    runner.add_stop_task(5 * runner.get_slot_ms());
    runner.run<RandpaNode>();

    for (auto i = 0; i < nodes_cnt; i++) {
        EXPECT_EQ(get_block_height(runner.get_db(i).last_irreversible_block_id()), 3);
    }
}

TEST(randpa_finality, chain_topology) {
    auto nodes_cnt = 10;
    auto runner = TestRunner(nodes_cnt);

    auto delay = 30;

    graph_type g;
    for (auto i = 0; i < nodes_cnt - 1; i++) {
        g.push_back({{ i + 1, delay }});
    }
    runner.load_graph(g);

    runner.add_stop_task(5 * runner.get_slot_ms());
    runner.run<RandpaNode>();

    for (auto i = 0; i < nodes_cnt; i++) {
        EXPECT_EQ(get_block_height(runner.get_db(i).last_irreversible_block_id()), 3);
    }
}

TEST(randpa_finality, no_threshold) {
    auto nodes_cnt = 10;
    auto runner = TestRunner(nodes_cnt);

    auto delay = 30;

    graph_type g;
    g.push_back({});
    for (auto i = 1; i < nodes_cnt * 2 / 3; i++) {
        g.push_back({{ 0, delay }});
    }
    runner.load_graph(g);

    runner.add_stop_task(5 * runner.get_slot_ms());
    runner.run<RandpaNode>();

    for (auto i = 0; i < nodes_cnt; i++) {
        EXPECT_EQ(get_block_height(runner.get_db(i).last_irreversible_block_id()), 0);
    }
}


TEST(randpa_finality, randpa_disabled_nodes) {
    int nodes_amount = 3;
    auto dpos_threshold = 2 * (nodes_amount * 2 / 3 + 1);
    auto runner = TestRunner(nodes_amount);

    // model network failure with unrealistically large number of confirmations
    auto big_conf_number = 100'000;

    for (int i = 0; i < nodes_amount; i++) {
        if (i & 1) {
            runner.add_node<RandpaNode>(big_conf_number);
        } else {
            runner.add_node<Node>(big_conf_number);
        }
    }

    vector<pair<int, int>> v0{{1, 2}, {2, 10}};
    graph_type g;
    g.push_back(v0);
    runner.load_graph(g);

    // dpos finality fails and randpa frozen
    const int32_t _max_finality_lag_blocks = 69 * 12 * 2 * 2;
    runner.add_stop_task((_max_finality_lag_blocks + 1) * runner.get_slot_ms());
    runner.run_initialized_nodes();

    auto check_randpa_frozen = [&](bool frozen) {
        for (auto i = 0; i < nodes_amount; i++) {
            const auto node_ptr = std::dynamic_pointer_cast<RandpaNode>(runner.get_node(i));
            if (node_ptr) {
                ASSERT_EQ(frozen, node_ptr->get_randpa().is_frozen());
            }
        }
    };

    check_randpa_frozen(true);

    // change conf_number to dpos_threshold
    for (int i = 0; i < nodes_amount; i++) {
        const auto node_ptr = runner.get_node(i);
        node_ptr->db.set_conf_number(dpos_threshold);
    }

    runner.add_stop_task(4000 * runner.get_slot_ms());
    runner.run_loop();
    // dpos finality should be back and randpa active
    check_randpa_frozen(false);

    // head block num: 4000
    for (int i = 0; i < nodes_amount; i++) {
        EXPECT_EQ(3994, get_block_height(runner.get_db(i).last_irreversible_block_id()));
    }
}

TEST(randpa_finality, fullnodes_over_fullnodes) {
    //                 +-- [3] -- 5 -- 6
    //                 |    |
    // 0 -- 1 -- [2] --+    |
    //                 |    |
    //                 +-- [4] -- 7 -- 8

    auto runner = TestRunner(9);
    node_types_t nodetypes { FN, FN, BP, BP, BP, FN, FN, FN, FN };
    runner.load_nodetypes(nodetypes);
    graph_type g;
    /* 0 */g.push_back({{1, 10}});
    /* 1 */g.push_back({{2, 10}});
    /* 2 */g.push_back({{3, 20}, {4, 20}});
    /* 3 */g.push_back({{4, 20}, {5, 30}});
    /* 4 */g.push_back({{7, 10}});
    /* 5 */g.push_back({{6, 30}});
    /* 6 */g.push_back({{}});
    /* 7 */g.push_back({{8, 10}});
    /* 8 */g.push_back({{}});
    runner.load_graph(g);
    runner.add_stop_task(8 * runner.get_slot_ms());
    runner.run<RandpaNode>();
    for (auto i = 0; i < 6; ++i) {
        EXPECT_EQ(get_block_height(runner.get_db(i).last_irreversible_block_id()), 7);
    }
    EXPECT_EQ(get_block_height(runner.get_db(7).last_irreversible_block_id()), 7);
}

TEST(randpa_finality, fullnodes_over_five_fullnodes) {
    //                            +-- [7] -- 13 -- 14 -- 15 -- 16 -- 17
    //                           /     |
    // 0 -- 1 -- 2 -- 3 -- 4 -- [5]    |
    //                           \     |
    //                            +-- [6] -- 8 -- 9 -- 10 -- 11 -- 12

    size_t nodes_amount = 18;
    auto runner = TestRunner(nodes_amount);

    node_types_t nodetypes(18, FN);
    nodetypes[5] = BP;
    nodetypes[6] = BP;
    nodetypes[7] = BP;
    runner.load_nodetypes(nodetypes);

    graph_type g;
    g.push_back({{1, 10}});
    g.push_back({{2, 20}});
    g.push_back({{3, 10}});
    g.push_back({{4, 30}});
    g.push_back({{5, 10}});
    g.push_back({{6, 15}, {7, 20}});
    g.push_back({{7, 30}, {8, 20}});
    g.push_back({{13, 30}});
    g.push_back({{9, 10}});
    g.push_back({{10, 10}});
    g.push_back({{11, 10}});
    g.push_back({{12, 10}});
    g.push_back({{}});
    g.push_back({{14, 10}});
    g.push_back({{15, 10}});
    g.push_back({{16, 10}});
    g.push_back({{17, 10}});
    g.push_back({{}});

    runner.load_graph(g);
    runner.add_stop_task(14 * runner.get_slot_ms());
    runner.run<RandpaNode>();
    for (auto i = 0; i < 17; ++i) {
        if (i != 12) {
            EXPECT_EQ(get_block_height(runner.get_db(i).last_irreversible_block_id()), 13);
        }
    }
}

TEST(randpa_finality, fullnodes_over_round_ring) {
    //                           63      60             33
    //                           |       |              |
    //                           62      59             32
    //                           |       |              |
    //                      +-- [61] -- [58] -- ... -- [31] --+
    //                      |                                 |
    // 0 -- 1 -- 2 -- [3] --+                                 |
    //                      |                                 |
    //                      +-- [4] -- [7] -- ..... -- [28] --+
    //                           |      |               |
    //                           5      8               29
    //                           |      |               |
    //                           6      9               30

    size_t nodes_amount = 64;
    auto runner = TestRunner(nodes_amount);
    node_types_t nodetypes(nodes_amount, FN);
    nodetypes[3] = BP;
    for (auto i = 4; i < nodes_amount; i += 3) {
        nodetypes[i] = BP;
    }
    runner.load_nodetypes(nodetypes);

    auto max_delay = 40;
    auto min_delay = 10;
    auto random = [&](){ return (rand() % (max_delay - min_delay)) + min_delay; };

    graph_type g(nodes_amount);
    g[0] = {{ 1, 10 }};
    g[1] = {{ 2, 20 }};
    g[2] = {{ 3, 10 }};
    g[3] = {{ 4, 30 }};
    for (size_t i = 4; i < nodes_amount - 3; i += 3) {
        g[i] = {{ i + 1, random() }, { i + 3, random() }};
        g[i+1] = {{ i + 2, random() }};
        // g[i+2] is empty
    }
    g[61] = {{ 62, 30 }, { 3, random() }};
    g[62] = {{ 63, 30 }};
    // g[63] is empty

    runner.load_graph(g);
    runner.add_stop_task(25 * runner.get_slot_ms() - 1);
    runner.run<RandpaNode>();
    for (auto i = 0; i < nodes_amount; ++i) {
        EXPECT_EQ(get_block_height(runner.get_db(i).last_irreversible_block_id()), 23);
    }
}

TEST(randpa_finality, fullnodes_over_smash_ring) {
    size_t nodes_amount = 64;
    auto runner = TestRunner(nodes_amount);
    node_types_t nodetypes(nodes_amount, FN);
    nodetypes[3] = BP;
    for (auto i = 4; i < nodes_amount; i += 3) {
        nodetypes[i] = BP;
    }
    runner.load_nodetypes(nodetypes);

    auto max_delay = 40;
    auto min_delay = 10;
    auto random = [&](){ return (rand() % (max_delay - min_delay)) + min_delay; };

    graph_type g;
    vector<pair<int, int>> empty_vector;
    g.push_back({{1, 10}});
    g.push_back({{2, 20}});
    g.push_back({{3, 10}});
    g.push_back({{4, 30}});
    for (auto i = 4; i < nodes_amount - 3; i += 3) {
        vector<pair<int, int>> pairs{{3, random()}, {i + 1, random()}};
        for (auto j = i + 3; j < nodes_amount; j += 3) {
            pairs.push_back({j, random()});
        }
        g.push_back(pairs);
        g.push_back({{ i + 2, random() }});
        g.push_back(empty_vector);
    }
    g.push_back({{62, 30}});
    g.push_back({{63, 30}});
    g.push_back(empty_vector);

    runner.load_graph(g);
    runner.add_stop_task(24 * runner.get_slot_ms());
    runner.run<RandpaNode>();
    for (auto i = 0; i < nodes_amount; ++i) {
        if (g[i] != empty_vector) {
            EXPECT_EQ(get_block_height(runner.get_db(i).last_irreversible_block_id()), 23);
        }
    }
}

TEST(randpa_finality, bp_over_six_fn) {
    size_t nodes_amount = 8;
    auto runner = TestRunner(nodes_amount);
    node_types_t nodetypes(nodes_amount, FN);
    nodetypes[0] = BP;
    nodetypes[7] = BP;
    runner.load_nodetypes(nodetypes);

    auto max_delay = 40;
    auto min_delay = 10;
    auto random = [&](){ return (rand() % (max_delay - min_delay)) + min_delay; };

    graph_type g;
    vector<pair<int, int>> empty_vector;
    g.push_back({{1, random()}, {2, random()}, {3, random()}});
    for (auto i = 0; i < 3; ++i) {
        g.push_back({{4, random()}, {5, random()}, {6, random()}});
    }
    for (auto i = 0; i < 3; ++i) {
        g.push_back({{7, random()}});
    }
    g.push_back(empty_vector);

    runner.load_graph(g);
    runner.add_stop_task(7 * runner.get_slot_ms());
    runner.run<RandpaNode>();
    for (auto i = 0; i < nodes_amount; ++i) {
        if (g[i] != empty_vector) {
            EXPECT_EQ(get_block_height(runner.get_db(i).last_irreversible_block_id()), 5);
        }
    }
}

TEST(randpa_finality, bp_over_spider_net_fn) {
    size_t nodes_amount = 11;
    auto runner = TestRunner(nodes_amount);
    node_types_t nodetypes(nodes_amount, FN);
    nodetypes[0] = BP;
    nodetypes[10] = BP;
    runner.load_nodetypes(nodetypes);

    auto max_delay = 40;
    auto min_delay = 10;
    auto random = [&](){ return (rand() % (max_delay - min_delay)) + min_delay; };

    graph_type g(nodes_amount);
    vector<pair<int, int>> empty_vector;
    g[0] = {{1, random()}};
    g[1] = {{2, random()}, {9, random()}};
    g[2] = {{3, random()}, {4, random()}};
    g[3] = {{7, random()}, {8, random()}, {10, random()}};
    g[4] = {{5, random()}, {6, random()}};
    g[6] = {{7, random()}};

    runner.load_graph(g);
    runner.add_stop_task(9 * runner.get_slot_ms());
    runner.run<RandpaNode>();
    for (auto i = 0; i < nodes_amount; ++i) {
        if (g[i] != empty_vector) {
            EXPECT_EQ(get_block_height(runner.get_db(i).last_irreversible_block_id()), 7);
        }
    }
}

TEST(randpa_finality, bp_over_vertical_lines_fn) {
    size_t nodes_amount = 42;
    auto runner = TestRunner(nodes_amount);
    node_types_t nodetypes(nodes_amount, FN);
    nodetypes[0] = BP;
    nodetypes[41] = BP;
    runner.load_nodetypes(nodetypes);

    auto max_delay = 40;
    auto min_delay = 10;
    auto random = [&](){ return (rand() % (max_delay - min_delay)) + min_delay; };

    graph_type g;
    vector<pair<int, int>> empty_vector;
    vector<pair<int, int>> temp_pairs;
    for (auto i = 1; i < 11; ++i) {
        temp_pairs.push_back({i, random()});
    }
    g.push_back(temp_pairs);
    for (auto i = 1; i < 4; ++i) {
        for (auto j = 1; j < 11; ++j) {
            vector<pair<int, int>> pairs;
            for (auto k = 1; k < 11; ++k) {
                pairs.push_back({i * 10 + k, random()});
            }
            g.push_back(pairs);
        }
    }
    for (auto i = 0; i < 10; ++i) {
        g.push_back({{41, random()}});
    }
    g.push_back(empty_vector);

    runner.load_graph(g);
    runner.add_stop_task(15 * runner.get_slot_ms());
    runner.run<RandpaNode>();
    for (auto i = 0; i < nodes_amount; ++i) {
        if (g[i] != empty_vector) {
            EXPECT_EQ(get_block_height(runner.get_db(i).last_irreversible_block_id()), 13);
        }
    }
}

TEST(randpa_finality, bp_over_horizontal_lines_fn) {
    size_t nodes_amount = 42;
    auto runner = TestRunner(nodes_amount);
    node_types_t nodetypes(nodes_amount, FN);
    nodetypes[0] = BP;
    nodetypes[41] = BP;
    runner.load_nodetypes(nodetypes);

    auto max_delay = 40;
    auto min_delay = 10;
    auto random = [&](){ return (rand() % (max_delay - min_delay)) + min_delay; };

    graph_type g;
    vector<pair<int, int>> empty_vector;
    g.push_back({{1, random()}, {11, random()}, {21, random()}, {31, random()}});
    for (auto i = 1; i < 4; ++i) {
        for (auto j = 1; j < 11; ++j) {
            vector<pair<int, int>> pairs;
            pairs.push_back({(j == 10 ? 41 : (i - 1) * 10 + j + 1), random()});
            for (auto k = 1; k < 11; ++k) {
                pairs.push_back({i * 10 + k, random()});
            }
            g.push_back(pairs);
        }
    }
    for(auto i = 31; i < nodes_amount - 1; ++i) {
        g.push_back({{i + 1, random()}});
    }
    g.push_back(empty_vector);

    runner.load_graph(g);
    runner.add_stop_task(15 * runner.get_slot_ms());
    runner.run<RandpaNode>();
    for (auto i = 0; i < nodes_amount; ++i) {
        if (g[i] != empty_vector) {
            EXPECT_EQ(get_block_height(runner.get_db(i).last_irreversible_block_id()), 13);
        }
    }
}

TEST(randpa_finality, large_multisig_BP_only) {
    constexpr size_t n_nodes = 6;
    constexpr size_t sig_provs_per_node = 10;

    // TODO: use this for testing set_signature_providers()
    //~// generate arrays of signature providers & public keys (one array for each physical node)
    //~vector<vector<signature_provider_type>> sig_provs;
    //~vector<vector<public_key_type>> pub_keys;
    //~
    //~for (size_t i = 0; i < n_nodes; i++) {
    //~    sig_provs.push_back(vector<signature_provider_type>());
    //~    pub_keys.push_back(vector<public_key_type>());
    //~
    //~    for (size_t j = 0; j < sig_provs_per_node; j++) {
    //~        const auto priv_key = ::get_priv_key();
    //~
    //~        sig_provs[i].push_back(
    //~            [priv_key](const digest_type& digest) {
    //~                return priv_key.sign(digest);
    //~            });
    //~        pub_keys[i].push_back(priv_key.get_public_key());
    //~    }
    //~}

    auto runner = TestRunner(n_nodes);
    runner.load_nodetypes({ BP, BP, BP, BP, BP, BP });

    // topology:
    //
    // +-- [0] -- [1] -- [2] --+
    // |                       |
    // +-- [3] -- [4] -- [5] --+
    graph_type g;
    // TODO: results are somehow unstable on the following set:
    // n_nodes = 6; sig_provs_per_node = 15; deltas = {555, 555, 90, 90, 90, 200}; (i < 12) in the inner loop
    g.push_back({{ 1, 40 }});
    g.push_back({{ 2, 40 }});
    g.push_back({{ 3, 40 }});
    g.push_back({{ 4, 40 }});
    g.push_back({{ 5, 40 }});
    g.push_back({{ 0, 40 }});


    runner.load_graph(g);
    runner.add_stop_task(24 * runner.get_slot_ms());

    // initialize nodes
    runner.init_nodes<RandpaNode>(runner.get_instances());
    //~ASSERT_EQ(runner.get_nodes().size(), sig_provs.size());

    // set miltiple signature providers for each node
    //~size_t sig_index = 0;
    for (const auto node : runner.get_nodes()) {
        const auto node_ptr = std::dynamic_pointer_cast<RandpaNode>(node);

        //~const auto& sps = sig_provs[sig_index];
        //~const auto& keys = pub_keys[sig_index];
        //~sig_index++;

        for (size_t i = 1; i < sig_provs_per_node; i++) {
            const auto priv_key = ::get_priv_key();
            node_ptr->get_randpa().add_signature_provider(
                [priv_key](const digest_type& digest) {
                    return priv_key.sign(digest);
                },
                // some bad publick keys => invalid signature providers
                (i < 3 ? ::get_priv_key().get_public_key() : priv_key.get_public_key())
            );
        }
        //~node_ptr->get_randpa().set_signature_providers(sps, keys);
    }

    // run
    runner.run_initialized_nodes();

    for (size_t i = 0; i < n_nodes; i++) {
        EXPECT_EQ(get_block_height(runner.get_db(0).last_irreversible_block_id()), 23);
    }
}
