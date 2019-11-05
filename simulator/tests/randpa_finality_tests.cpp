#include <gtest/gtest.h>

#include <string>
#include <iostream>

#include <simulator.hpp>
#include <cstdlib>
#include <ctime>
#include <random>
#include <randpa.hpp>

using namespace std;

using std::string;


TEST(randpa_finality, two_full_nodes) {
    auto runner = TestRunner(2);
    vector<node_type> nodetypes {node_type::FN, node_type::FN};
    runner.load_nodetypes(nodetypes);
    vector<pair<int, int> >  v0{{1, 10}};
    graph_type g;
    g.push_back(v0);
    runner.load_graph(g);
    runner.add_stop_task(2 * runner.get_slot_ms());
    runner.run<RandpaNode>();
    EXPECT_EQ(get_block_height(runner.get_db(0).last_irreversible_block_id()), 0);
    EXPECT_EQ(get_block_height(runner.get_db(1).last_irreversible_block_id()), 0);
}

TEST(randpa_finality, fullnodes_over_fullnodes) {
    auto runner = TestRunner(9);
    vector<node_type> nodetypes {node_type::FN, node_type::FN, node_type::BP, node_type::BP, node_type::BP, node_type::FN, node_type::FN, node_type::FN, node_type::FN};
    runner.load_nodetypes(nodetypes);
    vector<pair<int, int> >  v0{{1, 10}};
    vector<pair<int, int> >  v1{{2, 10}};
    vector<pair<int, int> >  v2{{3, 20}, {4, 20}};
    vector<pair<int, int> >  v3{{4, 20}, {5, 30}};
    vector<pair<int, int> >  v4{{7, 10}};
    vector<pair<int, int> >  v5{{6, 30}};
    vector<pair<int, int> >  v7{{8, 10}};
    graph_type g(9);
    g[0] = v0;
    g[1] = v1;
    g[2] = v2;
    g[3] = v3;
    g[4] = v4;
    g[5] = v5;
    g[7] = v7;
    runner.load_graph(g);
    runner.add_stop_task(8 * runner.get_slot_ms());
    runner.run<RandpaNode>();
    EXPECT_EQ(get_block_height(runner.get_db(0).last_irreversible_block_id()), 7);
    EXPECT_EQ(get_block_height(runner.get_db(1).last_irreversible_block_id()), 7);
    EXPECT_EQ(get_block_height(runner.get_db(2).last_irreversible_block_id()), 7);
    EXPECT_EQ(get_block_height(runner.get_db(3).last_irreversible_block_id()), 7);
    EXPECT_EQ(get_block_height(runner.get_db(4).last_irreversible_block_id()), 7);
    EXPECT_EQ(get_block_height(runner.get_db(5).last_irreversible_block_id()), 7);
    EXPECT_EQ(get_block_height(runner.get_db(7).last_irreversible_block_id()), 7);
}

TEST(randpa_finality, fullnodes_over_five_fullnodes) {
    size_t nodes_amount = 18;
    auto runner = TestRunner(nodes_amount);
    vector<bool> temp_nodetypes{1, 1, 1, 1, 1, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1};
    vector<node_type> nodetypes;
    for (auto t : temp_nodetypes)
        nodetypes.push_back(t ? node_type::FN : node_type::BP);
    runner.load_nodetypes(nodetypes);
    vector<pair<int, int> >  v0{{1, 10}};
    vector<pair<int, int> >  v1{{2, 20}};
    vector<pair<int, int> >  v2{{3, 10}};
    vector<pair<int, int> >  v3{{4, 30}};
    vector<pair<int, int> >  v4{{5, 10}};
    vector<pair<int, int> >  v5{{6, 15}, {7, 20}};
    vector<pair<int, int> >  v6{{7, 30}, {8, 20}};
    vector<pair<int, int> >  v8{{9, 10}};
    vector<pair<int, int> >  v9{{10, 10}};
    vector<pair<int, int> >  v10{{11, 10}};
    vector<pair<int, int> >  v11{{12, 10}};
    vector<pair<int, int> >  v7{{13, 30}};
    vector<pair<int, int> >  v13{{14, 10}};
    vector<pair<int, int> >  v14{{15, 10}};
    vector<pair<int, int> >  v15{{16, 10}};
    vector<pair<int, int> >  v16{{17, 10}};
    graph_type g(nodes_amount);
    g[0] = v0;
    g[1] = v1;
    g[2] = v2;
    g[3] = v3;
    g[4] = v4;
    g[5] = v5;
    g[6] = v6;
    g[8] = v8;
    g[9] = v9;
    g[10] = v10;
    g[11] = v11;
    g[7] = v7;
    g[13] = v13;
    g[14] = v14;
    g[15] = v15;
    g[16] = v16;
    runner.load_graph(g);
    runner.add_stop_task(14 * runner.get_slot_ms());
    runner.run<RandpaNode>();
    EXPECT_EQ(get_block_height(runner.get_db(0).last_irreversible_block_id()), 13);
    EXPECT_EQ(get_block_height(runner.get_db(1).last_irreversible_block_id()), 13);
    EXPECT_EQ(get_block_height(runner.get_db(2).last_irreversible_block_id()), 13);
    EXPECT_EQ(get_block_height(runner.get_db(3).last_irreversible_block_id()), 13);
    EXPECT_EQ(get_block_height(runner.get_db(4).last_irreversible_block_id()), 13);
    EXPECT_EQ(get_block_height(runner.get_db(5).last_irreversible_block_id()), 13);
    EXPECT_EQ(get_block_height(runner.get_db(6).last_irreversible_block_id()), 13);
    EXPECT_EQ(get_block_height(runner.get_db(8).last_irreversible_block_id()), 13);
    EXPECT_EQ(get_block_height(runner.get_db(9).last_irreversible_block_id()), 13);
    EXPECT_EQ(get_block_height(runner.get_db(10).last_irreversible_block_id()), 13);
    EXPECT_EQ(get_block_height(runner.get_db(11).last_irreversible_block_id()), 13);
    EXPECT_EQ(get_block_height(runner.get_db(7).last_irreversible_block_id()), 13);
    EXPECT_EQ(get_block_height(runner.get_db(13).last_irreversible_block_id()), 13);
    EXPECT_EQ(get_block_height(runner.get_db(14).last_irreversible_block_id()), 13);
    EXPECT_EQ(get_block_height(runner.get_db(15).last_irreversible_block_id()), 13);
    EXPECT_EQ(get_block_height(runner.get_db(16).last_irreversible_block_id()), 13);
}


TEST(randpa_finality, three_nodes) {
    auto runner = TestRunner(3);
    vector<pair<int, int> > v0{{1, 2}, {2, 10}};
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
    vector<pair<int, int> > v0{{1, 2}};
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
    vector<pair<int, int> > v0{{1, 20}, {2, 10}, {3, 10}, {4, 30}, {5, 30}};
    vector<pair<int, int> > v5{{6, 10}, {7, 30}, {8, 20}, {9, 10}, {10, 30}};
    vector<pair<int, int> > v10{{11, 10}, {12, 10}, {13, 10}, {14, 10}, {15, 30}};
    vector<pair<int, int> > v15{{16, 10}, {17, 10}, {18, 10}, {19, 10}, {20, 30}};
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
    vector<pair<int, int> > v0{{1, 2}};
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

    auto max_delay = 400;
    auto min_delay = 10;

    auto random = [&](){ return (rand() % (max_delay - min_delay)) + min_delay; };

    graph_type g;
    for (auto i = 0; i < nodes_cnt; i++) {
        vector<pair<int, int> > pairs;
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
