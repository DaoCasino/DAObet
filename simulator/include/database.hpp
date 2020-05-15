#pragma once

#include <types.hpp>
#include <memory>
#include <vector>
#include <map>
#include <exception>
#include <types.hpp>

using std::vector;
using std::shared_ptr;
using std::weak_ptr;
using std::pair;
using std::make_pair;

struct fork_db_node;
using fork_db_node_ptr = shared_ptr<fork_db_node>;

struct fork_db_node {
    block_id_type block_id;
    vector<fork_db_node_ptr> adjacent_nodes;
    weak_ptr<fork_db_node> parent;
    public_key_type creator_key;

    fork_db_node_ptr get_matching_node(const block_id_type& block_id) {
        auto iterator = find_if(adjacent_nodes.begin(), adjacent_nodes.end(), [&](const auto& node) {
            return node->block_id == block_id;
        });
        return iterator != adjacent_nodes.end() ? *iterator : nullptr;
    }
};

static fork_db_node_ptr deep_copy(fork_db_node_ptr src) {
    fork_db_node_ptr dest(new fork_db_node);
    dest->block_id = src->block_id;
    dest->adjacent_nodes.resize(src->adjacent_nodes.size());
    for (auto src_iter = src->adjacent_nodes.begin(), dest_iter = dest->adjacent_nodes.begin();
                src_iter != src->adjacent_nodes.end(); src_iter++, dest_iter++) {
        *dest_iter = deep_copy(*src_iter);
        (*dest_iter)->parent = dest;
    }
    return dest;
}

struct block_info {
    fork_db_node_ptr node;
    size_t height;
};

struct fork_db_chain_type {
    block_id_type base_block;
    vector<pair<block_id_type, public_key_type>> blocks;
};

using fork_db_chain_type_ptr = shared_ptr<fork_db_chain_type>;

struct ForkDbInsertException : public std::exception {};

class fork_db {
public:
    fork_db() = default;

    explicit fork_db(fork_db_node_ptr root_, size_t conf_number_):
        root(std::move(root_)), conf_number(conf_number_) {}
    explicit fork_db(block_id_type genesys_block, size_t conf_number) {
        set_conf_number(conf_number);
        set_genesys_block(genesys_block);
    }

    fork_db(const fork_db&) = delete;
    fork_db(fork_db&&) = default;

    void set_genesys_block(const block_id_type& genesys_block) {
        root = std::make_shared<fork_db_node>(fork_db_node{genesys_block});
    }

    void set_conf_number(size_t conf_number_) {
        conf_number = conf_number_;
    }

    bool insert(const fork_db_chain_type_ptr& chain) {
        return insert(*chain);
    }

    bool insert(const fork_db_chain_type& chain) {
        auto node = find(chain.base_block);
        if (!node) {
            throw ForkDbInsertException();
        }
        return insert(node, chain.blocks);
    }

    bool insert(fork_db_node_ptr node, const vector<pair<block_id_type, public_key_type>>& blocks) {
        return try_update_lib(insert_blocks(node, blocks));
    }

    fork_db_node_ptr find(const block_id_type& block_id) const {
        return find_node(block_id, root);
    }

    block_id_type fetch_prev_block_id(const block_id_type& block_id) const {
        auto node = find(block_id);
        assert(node && !node->parent.expired());
        return node->parent.lock()->block_id;
    }

    block_id_type last_irreversible_block_id() const {
        return root->block_id;
    }

    block_id_type get_master_block_id() const {
        return get_master_head()->block_id;
    }

    fork_db_node_ptr get_master_head() const {
        return find_master_head(root, 0).node;
    }

    void bft_finalize(const block_id_type& block_id) {
        auto node = find(block_id);
        assert(node);
        set_new_lib(node);
    }

    fork_db_node_ptr get_root() const {
        return root;
    }

    void set_root(fork_db_node_ptr root_) {
        root = root_;
    }

private:
    fork_db_node_ptr root;
    size_t conf_number;

    bool try_update_lib(const fork_db_node_ptr& new_chain_head) {
        auto path_to_chain_head = construct_path(new_chain_head);
        if (path_to_chain_head.size() > conf_number) {
            // we have new lib
            auto new_lib = path_to_chain_head[path_to_chain_head.size() - conf_number - 1];
            set_new_lib(new_lib);
            return true;
        }
        return false;
    }

    void set_new_lib(const fork_db_node_ptr& node) {
        root = node;
    }

    vector<fork_db_node_ptr> construct_path(fork_db_node_ptr node) const {
        vector<fork_db_node_ptr> result;
        while (node != root) {
            result.push_back(node);
            node = node->parent.lock();
        }
        std::reverse(result.begin(), result.end());
        return result;
    }

    block_info find_master_head(const fork_db_node_ptr& node, size_t depth) const {
        auto result = block_info{node, depth};
        for (const auto& adjacent_node : node->adjacent_nodes) {
            const auto head_node = find_master_head(adjacent_node, depth + 1);
            if (head_node.height > result.height) {
                result = head_node;
            }
        }
        return result;
    }

    fork_db_node_ptr find_node(const block_id_type& block_id, const fork_db_node_ptr& node) const {
        if (block_id == node->block_id) {
            return node;
        }
        for (const auto& adjacent_node : node->adjacent_nodes) {
            auto result = find_node(block_id, adjacent_node);
            if (result) {
                return result;
            }
        }
        return nullptr;
    }

    fork_db_node_ptr insert_blocks(fork_db_node_ptr node, const vector<pair<block_id_type, public_key_type>>& blocks) {
        for (const auto& block : blocks) {
            auto next_node = node->get_matching_node(block.first);
            if (!next_node) {
                next_node = std::make_shared<fork_db_node>(fork_db_node{block.first,
                                                                        {},
                                                                        node,
                                                                        block.second});
                node->adjacent_nodes.push_back(next_node);
            }
            node = next_node;
        }
        return node;
    }
};
