#include "interval_tree.h"
#include <algorithm>


template<typename key>
int interval_t<key>::compare(key &s, key &e)
{
	if (start < s) {
		return -1;
	}
	else if (start == s) {
		return end == e ? 0 : end < e ? -1 : 1;
	}
	else {
		return 1;
	}
}

template<typename key, typename val>
node_t<key, val>::node_t(key &start, key &end, val &&data)
{
	i.start = start;
	i.end = end;
	value = std::move(data);
	max = end;
	height = 0;
	left = right = nullptr;
}

template<typename key, typename val>
std::unique_ptr<interval_tree<key, val>> interval_tree<key, val>::create()
{
	return std::unique_ptr<interval_tree<key, val>>(new interval_tree<key, val>);
}

template<typename key, typename val>
void interval_tree<key, val>::insert(key &start, key &end, val &&data)
{
	if (start <= end) {
		root = insert(root, start, end, std::move(data));
	}
}

template<typename key, typename val>
void interval_tree<key, val>::erase(key &start, key &end)
{
	if (start <= end) {
		root = erase(root, start, end);
	}
}

template<typename key, typename val>
template <typename Comparator>
void interval_tree<key, val>::search(key &start, key &end, std::set<std::tuple<key, key, const val &>, Comparator> &out)
{
	out.clear();
	search(root, start, end, out);
}

template<typename key, typename val>
void interval_tree<key, val>::destroy(node_t<key, val> *node)
{
	if (node == nullptr) {
		return;
	}

	destroy(node->left);
	destroy(node->right);
	delete node;
}

template<typename key, typename val>
node_t<key, val> *interval_tree<key, val>::insert(node_t<key, val> *node, key &start, key &end, val &&data)
{
	if (node == nullptr) {
		node = new node_t(start, end, std::move(data));
		return node;
	}

	if (end > node->max) {
		node->max = end;
	}

	int ret = node->i.compare(start, end);
	if (ret < 0) {
		node->right = insert(node->right, start, end, std::move(data));
	}
	else if (ret == 0) {
		// Don't allow duplicate intervals
		node->value = std::move(data);
		return node;
	}
	else {
		node->left = insert(node->left, start, end, std::move(data));
	}

	node->height = calc_height(node);

	int balance = calc_balance(node);

	if (balance > 1 && calc_balance(node->left) >= 0) {
		return rotate_r(node);
	}

	if (balance > 1 && calc_balance(node->left) < 0) {
		return rotate_lr(node);
	}

	if (balance < -1 && calc_balance(node->right) <= 0) {
		return rotate_l(node);
	}

	if (balance < -1 && calc_balance(node->right) > 0) {
		return rotate_rl(node);
	}

	return node;
}

template<typename key, typename val>
node_t<key, val> *interval_tree<key, val>::erase(node_t<key, val> *node, key &start, key &end)
{
	if (node == nullptr) {
		return nullptr;
	}

	int ret = node->i.compare(start, end);
	if (ret < 0) {
		node->right = erase(node->right, start, end);
	}
	else if (ret > 0) {
		node->left = erase(node->left, start, end);
	}
	else {
		if (node->left && node->right) {
			node_t<key, val> *successor = find_successor(node->right);
			node->i.start = successor->i.start;
			node->i.end = successor->i.end;
			node->right = erase(node->right, successor->i.start, successor->i.end);
		}
		else if (node->left) {
			replace_parent(node, node->left);
		}
		else if (node->right) {
			replace_parent(node, node->right);
		}
		else {
			delete node;
			node = nullptr;
		}
	}

	if (node != nullptr) {
		node->height = calc_height(node);

		int balance = calc_balance(node);

		if (balance > 1 && calc_balance(node->left) >= 0) {
			return rotate_r(node);
		}

		if (balance > 1 && calc_balance(node->left) < 0) {
			return rotate_lr(node);
		}

		if (balance < -1 && calc_balance(node->right) <= 0) {
			return rotate_l(node);
		}

		if (balance < -1 && calc_balance(node->right) > 0) {
			return rotate_rl(node);
		}
	}

	set_max(node);

	return node;
}

template<typename key, typename val>
template <typename Comparator>
void interval_tree<key, val>::search(node_t<key, val> *node, key &start, key &end, std::set<std::tuple<key, key, const val &>, Comparator> &out)
{
	if (node == nullptr) {
		return;
	}

	if ((node->i.start <= end) && (node->i.end >= start)) {
		out.emplace(node->i.start, node->i.end, node->value);
	}

	if ((node->left != nullptr) && (node->left->max >= start)) {
		search(node->left, start, end, out);
	}

	search(node->right, start, end, out);
}

template<typename key, typename val>
node_t<key, val> *interval_tree<key, val>::find_successor(node_t<key, val> *node)
{
	node_t<key, val> *current_node = node;

	while (current_node->left) {
		current_node = current_node->left;
	}

	return current_node;
}

template<typename key, typename val>
void interval_tree<key, val>::replace_parent(node_t<key, val> *parent, node_t<key, val> *child)
{
	if (parent->left == child) {
		parent->i.start = child->i.start;
		parent->i.end = child->i.end;
		parent->value = std::move(child->value);
		delete child;
		parent->left = nullptr;
	}
	else if (parent->right == child) {
		parent->i.start = child->i.start;
		parent->i.end = child->i.end;
		parent->value = std::move(child->value);
		delete child;
		parent->right = nullptr;
	}
}

template<typename key, typename val>
void interval_tree<key, val>::set_max(node_t<key, val> *node)
{
	if (node != nullptr) {
		key max = node->i.end;
		if (node->left) {
			max = std::max(max, node->left->max);
		}
		if (node->right) {
			max = std::max(max, node->right->max);
		}
		node->max = max;
	}
}

template<typename key, typename val>
int interval_tree<key, val>::calc_height(node_t<key, val> *node)
{
	int l_height = node->left ? node->left->height : -1;
	int r_height = node->right ? node->right->height : -1;

	return std::max(l_height, r_height) + 1;
}

template<typename key, typename val>
int interval_tree<key, val>::calc_balance(node_t<key, val> *node)
{
	int l_height = node->left ? node->left->height : -1;
	int r_height = node->right ? node->right->height : -1;

	return l_height - r_height;
}

template<typename key, typename val>
node_t<key, val> *interval_tree<key, val>::rotate_l(node_t<key, val> *node)
{
	node_t<key, val> *node_r = node->right;
	node_t<key, val> *node_rl = node_r->left;
	node_r->left = node;
	node->right = node_rl;

	node->height = calc_height(node);
	node_r->height = calc_height(node_r);

	set_max(node);
	set_max(node_r);

	return node_r;
}

template<typename key, typename val>
node_t<key, val> *interval_tree<key, val>::rotate_r(node_t<key, val> *node)
{
	node_t<key, val> *node_l = node->left;
	node_t<key, val> *node_lr = node_l->right;
	node_l->right = node;
	node->left = node_lr;

	node->height = calc_height(node);
	node_l->height = calc_height(node_l);

	set_max(node);
	set_max(node_l);

	return node_l;
}

template<typename key, typename val>
node_t<key, val> *interval_tree<key, val>::rotate_lr(node_t<key, val> *node)
{
	node->left = (node->left);
	return rotate_r(node);
}

template<typename key, typename val>
node_t<key, val> *interval_tree<key, val>::rotate_rl(node_t<key, val> *node)
{
	node->right = rotate_r(node->right);
	return rotate_l(node);
}
