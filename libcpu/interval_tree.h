/*
 * self-balancing augmented interval tree implementation
 *
 * By ergo720 (2019)
 */

#pragma once

#include <memory>
#include <functional>
#include <set>
#include <tuple>


template<typename key>
struct interval_t {
	key start;
	key end;
	int compare(key &s, key &e);
};

template<typename key, typename val>
struct node_t {
	interval_t<key> i;
	val value;
	key max;
	int height;
	node_t *left, *right;
	node_t(key &start, key &end, val &&data);
};

template<typename key, typename val>
class interval_tree {
public:
	static std::unique_ptr<interval_tree<key, val>> create();
	~interval_tree() { destroy(root); }
	void insert(key &start, key &end, val &&data);
	void erase(key &start, key &end);
	template <typename Comparator = std::less<key>>
	void search(key & start, key & end, std::set<std::tuple<key, key, const val &>, Comparator> & out);

private:
	interval_tree() : root(nullptr) {};
	void destroy(node_t<key, val> *node);
	node_t<key, val> *insert(node_t<key, val> *node, key &start, key &end, val &&data);
	node_t<key, val> *erase(node_t<key, val> *gnode, key &start, key &end);
	template <typename Comparator>
	void search(node_t<key, val> *node, key &start, key &end, std::set<std::tuple<key, key, const val &>, Comparator> &out);
	node_t<key, val> *find_successor(node_t<key, val> *node);
	void replace_parent(node_t<key, val> *parent, node_t<key, val> *child);
	void set_max(node_t<key, val> *node);
	int calc_height(node_t<key, val> *node);
	int calc_balance(node_t<key, val> *node);
	node_t<key, val> *rotate_l(node_t<key, val> *node);
	node_t<key, val> *rotate_r(node_t<key, val> *node);
	node_t<key, val> *rotate_lr(node_t<key, val> *node);
	node_t<key, val> *rotate_rl(node_t<key, val> *node);
	node_t<key, val> *root;
};
