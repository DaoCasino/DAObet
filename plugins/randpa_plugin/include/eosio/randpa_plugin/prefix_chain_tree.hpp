#pragma once

#include "types.hpp"
#include <memory>
#include <vector>
#include <map>
#include <set>
#include <queue>

namespace randpa_finality {

using std::vector;
using std::shared_ptr;
using std::weak_ptr;
using std::pair;
using std::make_pair;
using std::set;
using std::map;

template <typename ConfType>
class prefix_node {
public:
    using conf_type = ConfType;
private:
    using node_type = prefix_node<conf_type>;
    using node_ptr = shared_ptr<node_type>;

public:
    block_id_type block_id;
    std::map<public_key_type, shared_ptr<conf_type>> confirmation_data;
    vector<node_ptr> adjacent_nodes;
    weak_ptr<node_type> parent;
    public_key_type creator_key;
    set<public_key_type> active_bp_keys;

    size_t confirmation_number() const {
        return confirmation_data.size();
    }

    node_ptr get_matching_node(block_id_type block_id) {
        for (const auto& node : adjacent_nodes) {
            if (node->block_id == block_id) {
                return node;
            }
        }
        return nullptr;
    }

    bool has_confirmation(const public_key_type& pub_key ) {
        return confirmation_data.find(pub_key) != confirmation_data.end();
    }
};

struct chain_type {
    block_id_type base_block;
    vector<block_id_type> blocks;
};

class NodeNotFoundError : public std::exception {};

template <typename NodeType>
class prefix_chain_tree {
private:
    using node_ptr = shared_ptr<NodeType>;
    using node_weak_ptr = weak_ptr<NodeType>;
    using conf_ptr = shared_ptr<typename NodeType::conf_type>;

    struct node_info {
        node_ptr node;
        size_t height;
    };

public:
    explicit prefix_chain_tree(node_ptr&& root_) {
        // some dummy code for root to actually be erased from block_index
        NodeType *raw_ptr = new NodeType(*root_);
        root = shared_ptr<NodeType>(raw_ptr, [this](NodeType *ptr) {
            this->block_index.erase(ptr->block_id);
            delete ptr;
        });
        block_index[root->block_id] = node_weak_ptr(root);
    }
    prefix_chain_tree() = delete;
    prefix_chain_tree(const prefix_chain_tree&) = delete;

    auto find(const block_id_type& block_id) const {
        auto itr = block_index.find(block_id);
        return itr != block_index.end() ? itr->second.lock() : nullptr;
    }

    node_ptr add_confirmations(const chain_type& chain, const public_key_type& sender_key, const conf_ptr& conf) {
        node_ptr node = nullptr;
        vector<block_id_type> blocks;
        std::tie(node, blocks) = get_tree_node(chain);
        if (!node) {
            return nullptr;
        }
        return _add_confirmations(node, blocks, sender_key, conf);
    }

    void remove_confirmations() {
        _remove_confirmations(root);
    }

    void insert(const chain_type& chain, const public_key_type& creator_key, const set<public_key_type>& active_bp_keys) {
        node_ptr node = nullptr;
        vector<block_id_type> blocks;
        std::tie(node, blocks) = get_tree_node(chain);

        if (!node) {
            throw NodeNotFoundError();
        }

        insert_blocks(node, blocks, creator_key, active_bp_keys);
    }

    auto get_final_chain_head(size_t confirmation_number) const {
        auto head = get_chain_head(root, confirmation_number, 0).node;
        return head != root ? head : nullptr;
    }

    auto get_root() const {
        return root;
    }

    auto set_root(const node_ptr& new_root) {
        root = new_root;
        root->parent.reset();
    }

    auto get_head() const {
        if (!head_block.lock()) {
            return root;
        }
        return head_block.lock();
    }

    node_ptr get_last_inserted_block(const public_key_type& pub_key) {
        auto iterator = last_inserted_block.find(pub_key);
        if (iterator != last_inserted_block.end()) {
            auto node = iterator->second;
            return node.lock();
        }
        return nullptr;
    }

    chain_type get_branch(const block_id_type& head_block_id) const {
        auto last_node = find(head_block_id);

        chain_type chain { root->block_id, {} };
        while (last_node != root) {
            chain.blocks.push_back(last_node->block_id);
            FC_ASSERT(!last_node->parent.expired(), "parent should be exists");
            last_node = last_node->parent.lock();
        }
        std::reverse(chain.blocks.begin(), chain.blocks.end());

        return chain;
    }

private:
    map<public_key_type, node_weak_ptr> last_inserted_block;
    map<block_id_type, node_weak_ptr> block_index;
    // lifetime(node) < lifetime(block_index) => destroy the node first
    node_ptr root;
    node_weak_ptr head_block;

    pair<node_ptr, vector<block_id_type> > get_tree_node(const chain_type& chain) {
        auto node = find(chain.base_block);
        const auto& blocks = chain.blocks;

        if (node) {
            return {node, std::move(chain.blocks)};
        }

        auto block_itr = std::find_if(blocks.begin(), blocks.end(), [&](const auto& block) {
            return (bool) find(block);
        });

        if (block_itr != blocks.end()) {
            return { find(*block_itr), vector<block_id_type>(block_itr + 1, blocks.end()) };
        }
        return {nullptr, {} };
    }

    node_info get_chain_head(const node_ptr& node, size_t confirmation_number, size_t depth) const {
        auto result = node_info{node, depth};
        for (const auto& adjacent_node : node->adjacent_nodes) {
            if (adjacent_node->confirmation_number() < confirmation_number) {
                continue;
            }
            const auto head_node = get_chain_head(adjacent_node, confirmation_number, depth + 1);
            if (head_node.height > result.height) {
                result = head_node;
            }
        }
        return result;
    }

    void insert_blocks(node_ptr node, const vector<block_id_type>& blocks, const public_key_type& creator_key,
            const set<public_key_type>& active_bp_keys) {
        for (const auto& block_id : blocks) {
            auto next_node = node->get_matching_node(block_id);
            if (!next_node) {
                next_node = shared_ptr<NodeType>(new NodeType{block_id,
                                                              {},
                                                              {},
                                                              node,
                                                              creator_key,
                                                              active_bp_keys},
                                                 [this](NodeType *node) {
                                                        this->block_index.erase(node->block_id);
                                                        delete node;
                                                 });
                block_index[block_id] = weak_ptr<NodeType>(next_node);
                node->adjacent_nodes.push_back(next_node);
            }
            node = next_node;
        }
        last_inserted_block[creator_key] = node;

        if (get_block_num(node->block_id) > get_block_num(get_head()->block_id)) {
            head_block = node;
        }
    }

    node_ptr _add_confirmations(node_ptr node, const vector<block_id_type>& blocks, const public_key_type& sender_key,
                           const conf_ptr& conf) {
        auto max_conf_node = node;
        node->confirmation_data[sender_key] = conf;

        for (const auto& block_id : blocks) {
            node = node->get_matching_node(block_id);
            if (!node) {
                break;
            }
            node->confirmation_data[sender_key] = conf;
            if (max_conf_node->confirmation_data.size() <= node->confirmation_data.size()) {
                max_conf_node = node;
            }
        }

        return max_conf_node;
    }

    void _remove_confirmations(node_ptr root) {
        if (!root) {
            return;
        }
        root->confirmation_data.clear();
        for (const auto& node : root->adjacent_nodes) {
            _remove_confirmations(node);
        }
    }
};

} //namespace randpa_finality
