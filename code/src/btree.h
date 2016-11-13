#ifndef _BTREE_H
#define _BTREE_H

////////////////////////////////////////////////////////////
/// Copyright (c) 2016, Sun Yat-sen University,
/// All rights reserved
/// @file btree.h
/// @brief definition of binary (one-bit) tree
/// build, insert and delete prefixes in a binary tree
/// @author Yi Wu
///////////////////////////////////////////////////////////

#include "common.h"
#include "utility.h"
#include "btree.h"
#include <queue>
#include <deque>
#include <stack>


/// binary node
///
/// two pointers to children and one field for storing nexthop

template<int W>
struct BNode {

	typedef typename choose_ip_type<W>::ip_type ip_type;

	BNode* lchild; // left child

	BNode* rchild; // right child
		
	uint32 nexthop; // next hop
	
	/// default ctor
	BNode() : lchild(nullptr), rchild(nullptr), nexthop(0) {}
};


/// binary tree, singleton
///
/// build : create a btree, each node of which contains three fields (two pointers along with a data field).
/// update: support two kinds of update, namely withdraw/insert a prefix and alter the nexthop information
/// @param W 32 for IPV4 and 128 for IPV6

template<int W>
class BTree {
private:
	
	typedef typename choose_ip_type<W>::ip_type ip_type;

	typedef BNode<W> node_type;

	static BTree<W>* bt; 

	node_type* root; /// ptr to root

	uint32 nodenum;

	uint32 levelnodenum[W + 1]; // root node contains */0 and locates at level 0

private:

	/// default ctor
	BTree() : root(nullptr), nodenum(0) {

		for (int i = 0; i < W + 1; ++i) {

			levelnodenum[i] = 0;
		}
	}

	BTree(const BTree& _bt) = delete;

	BTree& operator= (const BTree&) = delete;

	/// destroyer
	~BTree() {

		if (nullptr != root) {

			destroy();
		}	
	}

public: 

	/// create an instance of BTree if not yet instantiated; otherwise, return the instance.
	static BTree* getInstance();

	/// build BTree for BGP table
	void build(const std::string& _fn) {

		// destroy the old tree if there exists
		if (nullptr != root) {

			destroy();
		}

		// create a new root node
		root = new node_type();

		++nodenum;

		++levelnodenum[0]; // root node is at level 0
		
		// insert prefixes into btree one by one
		std::ifstream fin(_fn, std::ios_base::binary);

		std::string line; 

		ip_type prefix;
					
		uint8 length;

		uint32 nexthop;
	
		while (getline(fin, line)) {	

			// retrieve prefix and length
			utility::retrieveInfo(line, prefix, length);

			nexthop = length; //! note that we represent the nexthop information by length (for test only)
	
			if (0 == length) { // must be */0

			//	std::cerr << "1 line: " << line << " prefix: " << prefix << " length: " << (uint32)length << std::endl;

				root->nexthop = nexthop; 
			}
			else { // insert the prefix and length

			//	std::cerr << "2 line: " << line << " prefix: " << prefix << " length: " << (uint32)length << std::endl;

				ins(prefix, length, nexthop, root, 1); // insert nodes from level 1

				//std::cin.get();
			}
		}	 

		std::cerr << "created node num: " << nodenum << std::endl;

		for (int i = 0; i < W + 1; ++i) {

			std::cerr << "the " << i << "'s level--node num: " << levelnodenum[i] << std::endl;
		}

		return;
	}
	

	/// destroy btree to free memory (bridth-first order)
	void destroy() {
			
		if (nullptr == root) {

			return;
		}	

		std::queue<node_type*> queue;

		queue.push(root);

		while (!queue.empty()) {
	
			if (nullptr != queue.front()->lchild) queue.push(queue.front()->lchild);

			if (nullptr != queue.front()->rchild) queue.push(queue.front()->rchild);

			delete queue.front();

			queue.pop();
		}

		root = nullptr;

		nodenum = 0;

		for (int i = 0; i < W + 1; ++i) {

			levelnodenum[i] = 0;
		}

		return;
	}


	/// insert a prefix
	void ins (const ip_type& _prefix, const uint8& _length, const uint32& _nexthop, node_type* _pnode, const int _level){
	
		node_type* node = nullptr;

		// create lchild or rchild for _pnode with respect to whether bit = 0 or 1

		if (0 == utility::getBit(_prefix, _level - 1)) { // _prefix[_level - 1] is used to branch

			if (nullptr == _pnode->lchild) {

				_pnode->lchild = new node_type();

				++nodenum;
			
				++levelnodenum[_level];
			}
			
			node = _pnode->lchild;
		}		
		else {

			if (nullptr == _pnode->rchild) {

				_pnode->rchild = new node_type();

				++nodenum;

				++levelnodenum[_level];
			}
		
			node = _pnode->rchild;
		}

		// insert _prefix into _node if _level == _length; otherwise, recursively call insert() with _level = _level + 1
		if (_level == _length) {

			node->nexthop = _nexthop;
		}
		else {

			ins(_prefix, _length, _nexthop, node, _level + 1);
		}

		return;
	}


	// traverse bt in bridth-first order
	void traverse() {
		
		if (nullptr == root) return;
		
		std::queue<node_type*> queue;

		queue.push(root);

		while (!queue.empty()) {
	
			if (nullptr != queue.front()->lchild) queue.push(queue.front()->lchild);

			if (nullptr != queue.front()->rchild) queue.push(queue.front()->rchild);

			printNode(queue.front());

			queue.pop();
		}	

		return;
	}


	/// print a node
	void printNode(node_type* _node) {

		std::cerr << "lnode: " << _node->lchild << " rnode: " << _node->rchild << " nexthop: " << _node->nexthop << std::endl;

		return;
	}

	/// search the LPM for the given IPv4 address
	uint32 search(const ip_type& _ip) {

		// root contains */0, which is a prefix matching any address
		node_type* node = root;

		uint32 nexthop = 0; 

		int level = 0;

		// search from root to a leaf node
		while (node != nullptr) {

			// find a valid prefix, update nexthop
			if (node->nexthop != 0) {

				nexthop = node->nexthop;
			}
		
			++level;

			// branch to left or right subtree according to bit value
			if (utility::getBit(_ip, level - 1)) {

				node = node->rchild;
			}
			else {

				node = node->lchild;
			}
		}		

		//std::cerr << "original ip: " << _ip << " nexthop of LPM: " << nexthop << std::endl;

		return nexthop;
	}

	/// delete a prefix from btree. The idea is to first find the location of the prefix node in btree (if any) and then conduct the delete operation
	/// according to two cases: 
	/// (1) a leaf node, then delete the node and recursively check if the parent node is required to be deleted; 
	/// (2) a non-leaf node, then clear the nexthop field if not empty.
	/// note that the prefix is assumed not to be */0
	void del (const ip_type& _prefix, const uint8& _length){
	
		//std::cerr << "prefix: " << _prefix << " length: " << (uint32)_length << std::endl;

		std::stack<node_type*> stack;
		
		// root contains */0, which is a prefix matching any address
		node_type* node = root;

		// search prefix in btree
		int level = 0;

		do {
			stack.push(node); // push current node

			++level;

			if (utility::getBit(_prefix, level - 1)) {

				node = node->rchild;
			}
			else {

				node = node->lchild;
			}

		} while (node != nullptr && level != _length);

		//update 
		if (nullptr == node || 0 == node->nexthop) { // not found or root node, do nothing

			return;
		}
		else {

			node->nexthop = 0; //delete prefix

			if (nullptr == node->lchild && nullptr == node->rchild) { //delete if it becomes an empty leaf node

				delete node;

				--nodenum;

				--levelnodenum[level];

				//update parent/ancestors

				while (!stack.empty()) {
	
					--level;

					if (utility::getBit(_prefix, level)) {
	
						stack.top()->rchild = nullptr;
					}
					else {
	
						stack.top()->lchild = nullptr;
					}
			
					// parent node has become an empty leaf node (it is assumemd not to be root)
					if ((stack.top()->nexthop == 0 && stack.top() != root) && 
						(nullptr == stack.top()->lchild) && 
						(nullptr == stack.top()->rchild)) {
					
						delete stack.top();

						--nodenum;

						--levelnodenum[level];
	
						stack.pop();
					}
					else {
	
						break;
					}	
				}	
			}
		}

		return;	
	}

	uint32 getLevelNodeNum(const int _level) const {

		return levelnodenum[_level];
	}
};


template<int W>
BTree<W>* BTree<W>::bt = nullptr;

template<int W>
BTree<W>* BTree<W>::getInstance() {

	if (nullptr == bt) {

		bt = new BTree();
	}

	return bt;
}


#endif // _BTree_H
