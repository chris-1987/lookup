#ifndef _PT_H
#ifndef _PT_H
#define _PT_H

//////////////////////////////////////////////////////////////////////////
/// Copyright (c) 2016, Sun Yat-sen University
/// All rights reserved
/// \file ptree.h
/// \brief definition of prefix tree
/// Build, insert and delete prefixes in a prefix tree
/// An implementation of the algorithm designed by Michael Berger.
/// @author Yi Wu
/// \date 2016.11

#include "common.h"
#include "utility.h"
#include <queue>


/// \brief Build and update prefix tree.
///
/// Each node in the tree contains two pointers, a prefix field and a nexthop field. 
/// A node at level-$k$ can store a prefix of a length no less than $k$.
template<typename W>
struct PNode{

	typedef typename choose_ip_type<W>::ip_type ip_type;

	PNode* lchild; ///< left child

	PNode* rchild; ///< right child

	ip_type prefix; ///< prefix

	uint8 length; ///< prefix length

	uint32 nexthop; ///< next hop	

	/// \brief ctor
	PNode() : lchild(nullptr), rchild(nullptr), prefix(0), length(0), nexthop(0) {}

};

/// \brief 
template<typename W>
class PTree{
private:

	typedef typename choose_ip_type<W>::ip_type ip_type;

	typedef PNode<W> node_type;

	static PTree<W>* pt;

	node_type* root; ///< ptr to root

	uint32 mNodeNum; ///< node num

	uint32 mLevelNodeNum[W + 1]; ///< node num in levels

private:

	/// \brief default ctor
	PTree() : root(nullptr), mNodeNum(0) {

		for (int i = 0; i < W + 1; ++i) {

			mLevelNodeNum[i] = 0;
		}
	}

	/// \brief copy ctor, disabled
	PTree(const PTree& _pt) = delete;

	/// \brief assignment op, disabled
	PTree& operator= (const PTree&) = delete;

	/// \brief dtor
	~PTree() {

		if (nullptr != root) {

			destroy();
		}
	}

public:

	/// \brief return singleton
	static PTree* getInstance();

	/// \brief build PTree for the given BGP table
	void build(const std::string& _fn) {

		// destroy the old tree if there exists any
		if (nullptr != root) {

			destroy();
		}

		// insert prefixes into ptree one by one
		std::ifstream fin(_fn, std::ios_base::binary);

		std::string line;

		ip_type prefix;

		uint8 length;

		uint32 nexthop;

		while(getline(fin, line)) {

			// retrieve prefix and length
			utility::retrieveInfo(line, prefix, length);

			nexthop = length;

			if (0 == length) { // */0

				// do nothing
			}
			else {

				ins(prefix, length, nexthop, root, 0);
			}
		}

		std::cerr << "created node num: " << mNodeNum << std::endl;

		for (int i = 0; i < W + 1; ++i) {

			std::cerr << "the " << i << "'s level--node num: " << mLevelNodeNum[i] << std::endl;
		}

	
		return;
	}


	void destroy() {

		if (nullptr == root) {

			return;
		}

		std::queue<node_type*> queue;

		queue.push(root);

		while(!queue.empty()) {

			if (nullptr != queue.front()->lchild) queue.push(queue.front()->lchild);

			if (nullptr != queue.front()->rchild) queue.push(queue.front()->rchild);

			delete queue.front();

			queue.pop();

		}

		root = nullptr;

		mNodeNum = 0;

		for (int i = 0; i < W + 1; ++i) {

			mLevelNodeNum[i] = 0;
		}

		return;
	}

	/// \brief insert a prefix
	void ins (const ip_type & _prefix, const uint8& _length, const uint32& _nexthop, node_type&* _node, const int _level) {

		if (nullptr == _node) {

			_node = new node_type(_prefix, _length, _nexthop);
			
			if (nullptr == root) {

				root = _node;
			}

			return;
		}	

		if (_length == _level) {

			uint32 prefix = _node->prefix;

			uint8 length = _node->length;

			uint32 nexthop = _node->nexthop;

			if (utility::getBit(prefix, _level)) {

				insert(prefix, length, nexthop, _node->rchild);
			}
			else {

				insert(prefix, length, nexthop, _node->lchild);
			}
		}	
		else {

			if (utility::getBit(_prefix, _level)) {

				insert(prefix, length, nexthop, _node->rchild);
			}
			else {

				insert(prefix, length, nexthop, _node->lchild);
			}
		}
		return;
	}

	/// \brief delete a prefix
	void del(const ip_type& _prefix, const uint8& _length, node_type * _node, int _level) {

		if (_prefix == _node->prefix && _length == _node->length) {

			node_type* leafNode = findLeafNode(_node);

			if (nullptr == leafNode) {

				delete _node;

				return;
			}	
			else {

				leafNode->rchild = _node->rchild;

				leafNode->lchild = _node->lchild;

				delete _node;

				_node = leafNode;

				return;
			}
		}
		else {

			if (utility::getBit(_prefix, _level)) {

				del(_prefix, _length, _node->rchild, _level + 1);
			} 
			else {

				del(_prefix, _length, _node->lchild, _level + 1);
			}
		}
				
	}

	/// \brief traverse the prefix tree
	void traverse() {

		if (nullptr == root) return;

		std::queue<node_type*> queue;

		queue.push(root);

		while(!queue.empty()) {

			if (nullptr != queue.front()->lchild) queue.push(queue.front()->lchild);

			if (nullptr != queue.front()->rchild) queue.push(queue.front()->rchild);

			printNode(queue.front());

			queue.pop();
		}

		return;
	}


	/// \brief search the LPM for the given IP address
	uint32 search(const ip_type& _ip) {
		
		uint32 nexthop = 0; 

		if (nullptr == root) {

			// do nothing	
		}
		else {
			int level = 0;

			node_type* node = root;

			int bestLength;
			
			while (nullptr != node) {

				if (utility::getBitsValue(_ip, 0, node->length) == utility::getBitsValue(node->prefix, 0, node->length)) {

					if (bestLength < node->length) {

						bestLength = node->length;

						nexthop = node->nexthop;
					}
				}

				if (utility::getBitValue(_ip, _level)) {

					node = node->rchild;
				}
				else {

					node = node->lchild;
				}	
			}
		}	
	
		return nexthop;		
	}


	/// \brief print a node
	void printNode(node_type* _node) {

		std::cerr << "lnode: " << _node->lchild << "rnode: " << _node->rchild << " nexthop: " << _node->nexthop << std::endl;

		return;	
	}

};



#endif // _PT_H

