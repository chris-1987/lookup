#ifndef _RBTREE_H
#define _RBTREE_H

////////////////////////////////////////////////////////////
/// Copyright (c) 2016, Sun Yat-sen University,
/// All rights reserved
/// \file rbtree.h
/// \brief Definition of a IP lookup index based on binary-tree.
///
/// The lookup index consists of three parts: a fast lookup table that contains route information for prefixes shorter than U bits,
/// a table that consists of 2^U entries and each entry stores a pointer to the root of a binary tree, 
/// and a set of binary tree.
/// @author Yi Wu
/// \date 2016.11
///////////////////////////////////////////////////////////


#include "../common/common.h"
#include "../common/utility.h"

#include "fasttable.h"

#include <queue>
#include <deque>
#include <stack>
#include <cmath>

/// \brief Nodes in a binary tree.
/// 
/// Two pointers that indicate the child nodes and a nexthop information field.
///
/// \param W 32 or 128 for IPv4 or IPv6 address, respectively.
template<int W>
struct BNode {

	typedef typename choose_ip_type<W>::ip_type ip_type;

	BNode* lchild; ///< pointer to left child

	BNode* rchild; ///< pointer to right child
		
	uint32 nexthop; ///< next hop information
	
	/// \brief ctor
	BNode() : lchild(nullptr), rchild(nullptr), nexthop(0) {}
};


/// \brief Build and update the index.
///
/// Prefixes shorter than U bits are stored in a fast lookup table.
/// Prefixes not shorter than U bits are stored in the set of binary trees.
///
/// \param W 32 for IPV4 and 128 for IPV6.
/// \param U A threshold for classifying shorter and longer prefixes ( < U and >=U).
/// \param V Number of binary trees at large.
template<int W, int U, size_t V = static_cast<size_t>(pow(2, U))>
class RBTree {
private:
	
	typedef typename choose_ip_type<W>::ip_type ip_type;

	typedef BNode<W> node_type;

	node_type* mRootTable[V]; ///< each entry points to a binary tree

	uint32 mNodeNum[V]; ///< number of nodes in each binary tree

	uint32 mLevelNodeNum[V][W - U + 1]; ///< number of nodes in each level of a binary tree

	uint32 mTotalNodeNum; ///< number of nodes in total

	FastTable<W, U - 1> *ft; ///< pointer to fast table, a fast table is used to store shorter prefixes and provide search, insert and delete interfaces

public:

	/// \brief default ctor
	RBTree() : mTotalNodeNum(0), ft(nullptr) {
	
		for (int i = 0; i < V; ++i) {

			mRootTable[i] = nullptr;
		}

		for (int i = 0; i < V; ++i) {

			mNodeNum[i] = 0;
		}
	
		for (int i = 0; i < V; ++i) {

			for (int j = 0; j < W - U + 1; ++j) {

				mLevelNodeNum[i][j] = 0;
			}
		}

		ft = new FastTable<W, U - 1>();
	}

	/// \brief disable copy-ctor
	RBTree(const RBTree& _rbt) = delete;

	/// \brief disable assignment op
	RBTree& operator= (const RBTree&) = delete;

	/// \brief dtor
	///
	/// destory all binary trees
	///
	~RBTree() {

		for (int i = 0; i < V; ++i) {

			if (nullptr != mRootTable[i]) {

				destroy(i);

				mRootTable[i] = nullptr;
			}
		}

		delete ft;
	
		ft = nullptr;
	}

	/// \brief Build the index.
	void build(const std::string & _fn) {

		// clear old index if there exists any
		for (int i = 0; i < V; ++i) {

			if (nullptr != mRootTable[i]) {

				destroy(i);

				mRootTable[i] = nullptr;
			}
		}

		for (int i = 0; i < V; ++i) {

			mNodeNum[i] = 0;
		}

		for (int i = 0; i < V; ++i) {

			for (int j = 0; j < W - U + 1; ++j) {

				mLevelNodeNum[i][j] = 0;
			}
		}
	
		// insert prefixes one by one into index
		std::ifstream fin(_fn, std::ios_base::binary);

		std::string line; 

		ip_type prefix;

		uint8 length;

		uint32 nexthop;

		while(getline(fin, line)) {

			// retrieve prefix and length
			utility::retrieveInfo(line, prefix, length);

			nexthop = length;

			if (0 == length) { // insert */0
		
				// do nothing
			}
			else { // insert into index
			
				ins(prefix, length, nexthop);
			}
		}

		//report();

		//traverse();

		return;
	}
	
	/// \brief destroy a binary tree
	void destroy(size_t _idx) {

		if (nullptr == mRootTable[_idx]) {
		
			return;
		}

		node_type* node = mRootTable[_idx];

		std::queue<node_type*> queue;
		
		queue.push(node);

		while(!queue.empty()) {

			auto front = queue.front();

			if (nullptr != front->lchild) queue.push(front->lchild);

			if (nullptr != front->rchild) queue.push(front->rchild);

			delete front;

			front = nullptr;	

			queue.pop();
		}

		return;
	}			
	
	/// \brief Report statistical inforamtion collected at the time point.
	void report() {

		mTotalNodeNum = 0;

		for (int i = 0; i < V; ++i) {

			std::cerr << "\n------node num in the " << i << "'s tree: " << mNodeNum[i] << std::endl;

//			for (int j = 0; j < W - U + 1; ++j) {

//				std::cerr << "level " << j << ": " << mLevelNodeNum[i][j]<< "\t";
//			}

			mTotalNodeNum += mNodeNum[i];	
		}

		std::cerr << "node num in total: " << mTotalNodeNum << std::endl;
	}



	/// \brief insert into index	
	void ins(const ip_type& _prefix, const uint8& _length, const uint32& _nexthop) {

		if (_length < U){ // insert into a fast lookup table (use prefix + length as index entry) 
	
			ft->ins(_prefix, _length, _nexthop);	
					
		}
		else { // insert into a binary tree

			ins(_prefix, _length, _nexthop, mRootTable[utility::getBitsValue(_prefix, 0, U - 1)], U, utility::getBitsValue(_prefix, 0, U - 1)); 
		}

		return;
	}	
		

	/// \brief insert into a binary tree
	void ins(const ip_type& _prefix, const uint8& _length, const uint32& _nexthop, node_type*& _node, const int _level, const size_t _treeIdx) {

		if (nullptr == _node) {

			_node = new node_type();

			++mNodeNum[_treeIdx];

			++mLevelNodeNum[_treeIdx][_level];
		}

		if (_length == _level) { // insert into current node
			
			_node->nexthop = _nexthop;
		}
		else {

			if (0 == utility::getBitValue(_prefix, _level)) {
				
				ins(_prefix, _length, _nexthop, _node->lchild, _level + 1, _treeIdx);
			}
			else {

				ins(_prefix, _length, _nexthop, _node->rchild, _level + 1, _treeIdx);
			}
		}

		return;
	}


	/// \brief traverse all binary trees in bridth-first order
	void traverse() {

		uint32 nodeNum = 0;

		for (int i = 0; i < V; ++i) {

			if (nullptr == mRootTable[i]) {
				
				// do nothing
			}
			else {
		
				std::queue<node_type*> queue;

				queue.push(mRootTable[i]);
			
				while(!queue.empty()) {

					nodeNum++;

					if (nullptr != queue.front()->lchild) queue.push(queue.front()->lchild);

					if (nullptr != queue.front()->rchild) queue.push(queue.front()->rchild);
					
					printNode(queue.front());

					queue.pop();
				}
			}
		}

		return;
	}

	void printNode(node_type* _node) {

		std::cerr << "nexthop: " << _node->nexthop << std::endl;

		return;
	}
	
	/// \brief search for _ip
	uint32 search(const ip_type& _ip) {

		// try to find a match in the fast lookup table
		uint32 nexthop1 = 0;

		nexthop1 = ft->search(_ip);

		// try to find a match in the binary trees
		uint32 nexthop2 = 0;

		node_type* node = mRootTable[utility::getBitsValue(_ip, 0, U - 1)]; 

		int level = U;

		while (nullptr != node) {

			if (node->nexthop != 0) { // contains a valid prefix

				nexthop2 = node->nexthop;
			}
		
			// branch 
			if (0 == utility::getBitValue(_ip, level)) {

				node = node->lchild;
			}		
			else {

				node = node->rchild;
			}

			++level;
		}				

		if (0 != nexthop2) return nexthop2; // prefixes found in binary trees must be longer than those found in the fast table

		if (0 != nexthop1) return nexthop1;

		return 0;
	}


	/// \brief delete a prefix in the index
	void del(const ip_type& _prefix, const uint8& _length) {

		if (_length < U) { // process in an alternatively way

			// delete from the fast lookup table
			ft->del(_prefix, _length);
		}
		else { // delete from a binary tree

			del(_prefix, _length, mRootTable[utility::getBitsValue(_prefix, 0, U - 1)], utility::getBitsValue(_prefix, 0, U - 1));
		}

		return;	
	}	

	/// \brief delete a prefix in a binary tree
	///
	/// If _prefix is located in a leaf node, then directly delete the node; otherwise,
	/// clear the nexthop field.
	/// \note After delete a leaf node, its parent may become a leaf node as well. 
	/// If the parent has no prefix, we must recursively delete the parent node as well. 
	void del(const ip_type& _prefix, const uint8& _length, node_type*& _root, const size_t _treeIdx){
	
		std::stack<node_type*> stack;

		node_type* node = _root;

		int level = U; // start from level U

		while (level < _length && nullptr != node) {
	
			stack.push(node);
			
			if (0 == utility::getBitValue(_prefix, level)) {

				node = node->lchild;
			}			
			else {
	
				node = node->rchild;
			}

			++level;
		}

		if (level == _length && nullptr != node) { // find the node

			node->nexthop = 0;

			if (nullptr == node->lchild && nullptr == node->rchild) { // if leaf node, then delete the node

				delete node;

				node = nullptr;

				--mNodeNum[_treeIdx];

				--mLevelNodeNum[_treeIdx][level];
					
				// modify parent
				while (!stack.empty()) {

					// modify pointer
					auto top = stack.top();

					if (0 == utility::getBitValue(_prefix, level - 1)) {

						top->lchild = nullptr;
					}			
					else {
						
						top->rchild = nullptr;
					}

					// parent becomes a leaf node, delete it
					if (nullptr == top->lchild && nullptr == top->rchild && top->nexthop == 0) {
			
						delete top;

						top = nullptr;

						stack.pop();

						--mNodeNum[_treeIdx];
	
						--mLevelNodeNum[_treeIdx][level - 1];
					}
					else {

						break;
					}

					--level;
				}

				if (stack.empty()) { // stack is empty, then the root is also deleted

					_root = nullptr;
				}
			}
		}
		
		return;
	}
};

#endif // _BTree_H
