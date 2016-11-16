#ifndef _PT_H
#define _PT_H

///////////////////////////////////////////////////////////////////////////////////////////////
/// Copyright (c) 2016, Sun Yat-sen University
/// All rights reserved
/// \file ptree.h
/// \brief definition of prefix tree
///
/// Build, insert and delete prefixes in a prefix tree
/// An implementation of the algorithm designed by Michael Berger.
/// Please refer to "IP lookup with low memory requirement and fast update" for more details.
///
/// \author Yi Wu
/// \date 2016.11
///////////////////////////////////////////////////////////////////////////////////////////////

#include "common.h"
#include "utility.h"
#include <queue>


/// \brief Build and update prefix tree.
///
/// Each node in the tree contains two pointers, a prefix field and a nexthop field. 
/// A node at level-$k$ can store a prefix of a length no less than $k$.
template<int W>
struct PNode{

	typedef typename choose_ip_type<W>::ip_type ip_type;

	PNode* lchild; ///< left child

	PNode* rchild; ///< right child

	ip_type prefix; ///< prefix

	uint8 length; ///< length of prefix

	uint32 nexthop; ///< next hop	

	/// \brief ctor
	PNode() : lchild(nullptr), rchild(nullptr), prefix(0), length(0), nexthop(0) {}

};

/// \brief prefix tree, singleton class
/// 
/// The length of each prefix inserted into level $k$ is no less than $k$.
/// 
/// \param W 32 and 128 for IPv4 and IPv6, respectively.
template<int W>
class PTree{
private:

	typedef typename choose_ip_type<W>::ip_type ip_type;

	typedef PNode<W> node_type;

	static PTree<W>* pt;

	node_type* root; ///< ptr to root

	uint32 mNodeNum; ///< number of nodes in total

	uint32 mLevelNodeNum[W + 1]; ///< number of nodes in each level

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
	
		//
		//traverse();

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
	void ins(const ip_type & _prefix, const uint8& _length, const uint32& _nexthop, node_type*& _node, const int _level) {

		if (nullptr == _node) { // create a new node and insert the prefix into the node

			_node = new node_type();

			mNodeNum++;

			mLevelNodeNum[_level]++;

			_node->prefix = _prefix;

			_node->length = _length;
		
			_node->nexthop = _nexthop;
			
			return;
		}	
		else {
			
			if (_length == _level) { // _prefix must be inserted into current node
				
				if (_node->length > _level) { //otherwise, node
					// check if the prefix in current node, say A, is equal to the one to be inserted, say B.
					// If yes substitute A with the one to be inserted; otherwise, A = B
					uint32 prefix = _node->prefix;
		
					uint8 length = _node->length;
		
					uint32 nexthop = _node->nexthop;
		
					_node->prefix = _prefix;
	
					_node->length = _length;
		
					_node->nexthop = _nexthop;
	
					// A must has be longer than _level, recursively insert A into a higher level
					if (0 == utility::getBitValue(prefix, _level)) { // branch to right subtrie 
		
						ins(prefix, length, nexthop, _node->lchild, _level + 1);
					}
					else {
		
						ins(prefix, length, nexthop, _node->rchild, _level + 1);
					}
				}
			}	
			else { // recursively insert _prefix into higher levels
	
				if (0 == utility::getBitValue(_prefix, _level)) { // branch to right subtrie
	
					ins(_prefix, _length, _nexthop, _node->lchild, _level + 1);
				}
				else {

					ins(_prefix, _length, _nexthop, _node->rchild, _level + 1);
				}
			}
		}

		return;
	}

	/// \brief delete a prefix
	void del(const ip_type& _prefix, const uint8& _length, node_type*& _node, int _level) {

		if (nullptr == _node) return; // find nothing

		if (_prefix == _node->prefix && _length == _node->length) { // prefix is found
			
			if (nullptr == _node->lchild && nullptr == _node->rchild) { // a leaf node, delete it directly
				
				delete _node;

				_node = nullptr; // reset parent's child pointer

				return;
			}	
			else { // not a leaf node, substitute the content of current node with the leaf node
			
				node_type* pnode = _node; // parent node of leaf node

				node_type* cnode = nullptr; // child node 

				bool isLeftBranch = true; // true if branch to left

				// at least has a child
				if (nullptr != pnode->lchild) {

					cnode = pnode->lchild;

					isLeftBranch = true;
				}
				else {

					cnode = pnode->rchild;

					isLeftBranch = false;
				}
				
				// find leaf descendant
				while (nullptr != cnode->lchild || nullptr != cnode->rchild) {

					if (nullptr != cnode->lchild) {

						pnode = cnode;

						cnode = cnode->lchild;

						isLeftBranch = true;
					}
					else {

						pnode = cnode;

						cnode = cnode->rchild;

						isLeftBranch = false;
					}
				}	
					
	
				// replace content of _node by that of those in cnode
				_node->prefix = cnode->prefix;

				_node->length = cnode->length;

				_node->nexthop = cnode->nexthop;

				// reset child pointer of pnode
				if (isLeftBranch) {

					pnode->lchild = nullptr;
				}
				else {

					pnode->rchild = nullptr;
				}

				// delete cnode
				delete cnode;

				return;
			}
		}
		else { // attempt to find the prefix in higher levels

			if (0 == utility::getBitValue(_prefix, _level)) {

				del(_prefix, _length, _node->lchild, _level + 1);
			} 
			else {

				del(_prefix, _length, _node->rchild, _level + 1);
			}
		}
				
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

			int bestLength = 0;
			
			while (nullptr != node) {

				if (utility::getBitsValue(_ip, 0, node->length - 1) == utility::getBitsValue(node->prefix, 0, node->length - 1)) { // match

					if (bestLength < node->length) { // if length > current best match

						bestLength = node->length;

						nexthop = node->nexthop;
					}
				}

				if (0 == utility::getBitValue(_ip, level)) { // branch to next level

					node = node->lchild;
				}
				else {

					node = node->rchild;
				}	
			
				++level;
			}
		}	
	
		return nexthop;		
	}


	/// \brief traverse the prefix tree
	void traverse() {

		uint32 nodeNum = 0;

		if (nullptr == root) {

			std::cerr << "node num: " << nodeNum << std::endl;

			return;
		}

		std::queue<node_type*> queue;

		queue.push(root);

		nodeNum++;

		while(!queue.empty()) {

			if (nullptr != queue.front()->lchild) {

				queue.push(queue.front()->lchild);

				nodeNum++;
			}
		
			if (nullptr != queue.front()->rchild) {
			
				queue.push(queue.front()->rchild);

				nodeNum++;
			}
//			printNode(queue.front());

			queue.pop();
		}

		std::cerr << "Scanned node num: " << nodeNum << std::endl;

		return;
	}

	/// \brief print a node
	void printNode(node_type* _node) {

		std::cerr << "lnode: " << _node->lchild << " rnode: " << _node->rchild;

		std::cerr << " prefix: " << _node->prefix << " length: " << (uint32)_node->length << " nexthop: " << _node->nexthop << std::endl;

		return;	
	}


	node_type*& getRoot(){

		return root;
	}
};


template<int W>
PTree<W>* PTree<W>::pt = nullptr;

template<int W>
PTree<W>* PTree<W>::getInstance() {

	if (nullptr == pt) {

		pt = new PTree();
	}

	return pt;
}

#endif // _PT_H

