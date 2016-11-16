#ifndef _MPT_H
#define _MPT_H

///////////////////////////////////////////////////////////////////////////////////////////////
/// Copyright (c) 2016, Sun Yat-sen University
/// All rights reserved
/// \file ptree.h
/// \brief definition of multi-prefix tree
///
/// Build, insert and delete prefixes in a multi-bit tree
/// An implementation of the algorithm designed by Sun-Yuan Hsieh.
/// Please refer to "A novel dynamic router-tables design for IP lookup and update" for more details.
///
/// \author Yi Wu
/// \date 2016.11
///////////////////////////////////////////////////////////////////////////////////////////////


#include "common.h"
#include "utility.h"

#include <queue>



/// \brief primary node in a multi-prefix tree
///  
/// \param W 32 or 128 for IPv4 or IPv6, respectively
template<int W, int K>
struct PNode{

	static const size_t MAX_PREFIX_NUM;

	static const size_t MAX_CHILD_NUM;

	typedef typename choose_ip_type<W>::ip_type ip_type;	
	
	uint8 t; ///< number of prefixes currently stored in the node

	ip_type prefix[MAX_PREFIX_NUM]; ///< prefixes sorted in non-increasing order, at most 2 * K + 1

	uint8 length[MAX_PREFIX_NUM]; ///< correspnding prefix length

	uint32 nexthop[MAX_PREFIX_NUM]; ///< corresponding nexthop

	PNode<W, K>* child[MAX_CHILD_NUM]; ///< child pointers, 2^K

	SNode<W>* sRoot; ///< pointer to the root of auxiliary prefix tree	

	PNode() {

		for (size_t i = 0; i < MAX_PREFIX_NUM; ++i) {

			prefix[i] = 0;		
		}

		for (size_t i = 0; i < MAX_PREFIX_NUM; ++i) {

			length[i] = 0;
		}

		for (size_t i = 0; i < MAX_PREFIX_NUM; ++i) {

			nexthop[i] = 0;
		}

		for (size_t i = 0; i < MAX_CHILD_NUM; ++i) {

			child[i] = nullptr;
		}

	}

	~PNode() {

		
	}
};

template<int W>
struct SNode{

	typedef typename choose_ip_type<W>::ip_type ip_type;

	ip_type prefix; ///< prefix
	
	uint8 length; ///< length of prefix

	uint32 nexthop; ///< corresponding nexthop

	SNode* lchild; ///< ptr to left child

	SNode* rchild; ///< ptr to right child

	SNode() : prefix(0), length(0), nexthop(0), lchild(nullptr), rchild(nullptr) {}
};

template<int W, int K>
size_t PNode<W, K>::MAX_PREFIX_NUM = 2 * K + 1;

template<int W, int K>
size_t PNode<W, K>::MAX_CHILD_NUM = static_cast<size_t>(pow(2, K));


/// \brief build and update a multi-prefix tree
///
/// 
/// \param W 32 and 128 for IPv4 and IPv6, respectively
template<int W>
class MPTree {
private:

	typedef typename choose_ip_type<W>::ip_type ip_type;

	typedef PNode<W> pnode_type; ///< primary node type

	typedef SNode<W> snode_type; ///< secondary node type

	pnode_type* pRoot; ///< root node of the multi-prefix tree

	uint32 mPNodeNum; ///< number of primary nodes

	uint32 mPnodeNum; ///< number of prefix nodes

private:
	
	/// \brief default ctor
	MPTree() : pRoot(nullptr), mPNodeNum(0), mSNodeNum(0) {}

	MPTree(const MPTree& _mpt) = delete;

	MPTree& operator= (const MPTree&) = delete;

	~MPTree() {

		if (nullptr != MPTRoot) {

			destroy();
		}
	}

public:

	/// \brief create an instance of MPTree if not yet instantiated
	static MPTree* getInstance();

	/// \brief build BTree for BGP table
	void build(const std::string & _fn) {

		// destroy the existing MPTree (if any)		
		if (nullptr != root) {

			destroy();
		}

		// insert prefixes into MPTree one by one
		std::ifstream fin(_fn, std::ios_base::binary);

		std::string line;

		ip_type prefix;

		uint8 length;

		uint32 nexthop;

		while (getline(fin, line) {

			utility::retrieveInfo(line, prefix, length);

			nexthop = length; 

			if (0 == length) { // */0

				// do nothing
			}
			else {

				ins(prefix, length, nexthop, MPTRoot, 0);
			}
		}
		
		std::cerr << "created pnode num: " << mPNodeNum << std::endl;

		std::cerr << "craeted snode num: " << mSNodeNum << std::endl;

		return;
	}
		
	void destroy() {

		if (nullptr == pRoot) {

			return;
		}

		std::queue<pnode_type*> mqueue;

		std::queue<snode_type*> pqueue;

		mqueue.push(pRoot);

		while(!mqueue.empty()) {

			auto mfront = mqueue.front();
	
			// destroy the auxiliary prefix tree
			if (nullptr != mfront->ptRoot) {

				pqueue.push(mfront->ptRoot);

				while (!pqueue.empty()) {

					auto pfront = pqueue.front();

					if (nullptr != pfront->lchild) pqueue.push(pfront->lchild);

					if (nullptr != pfront->rchild) pqueue.push(pfront->rchild);

					delete pfront;

					pqueue.pop();
				}
			}

			// push child nodes
			for (size_t i = 0; i < MAX_CHILD_NUM; ++i) {

				if (nullptr != mfront->child[i]) {

					mqueue.push(mfront->child[i]);
				}
			}

			delete mfront;

			mqueue.pop();
		}
		
		return;
	}

	/// \brief insert a prefix into multi-prefix tree
	///
	/// If len(_prefix) < K * (_level + 1), then insert it into the auxiliary prefix tree of current node. Otherwise:
	/// (1) If _node.t != MAX_PREFIX_NUM, then insert it into current node.
	/// (2) If _node.t == MAX_PREFIX_NUM && len(_node.prefix[MAX_PREFIX_NUM - 1]) < len(_prefix), then replace _node.prefix[MAX_PREFIX_NUM - 1] with
	///     _prefix and recursively insert _node.prefix[MAX_PREFIX_NUM - 1] into a higher level.
	/// (3) If _node.t == MAX_PREFIX_NUM && len(_node.prefix[MAX_PREFIX_NUM - 1]) >= len(_prefix), then continue to insert _prefix into a higher level.
	void ins(const ip_type& _prefix, const uint8& _length, const uint32& _nexthop, pnode_type*& _pnode, const int _level) {

		// create a new node		
		if (nullptr == _pnode) {

			_pnode = new pnode_type();

			mPNodeNum++;

			if (_length < (_level + 1) * K) { // if insert into the auxiliary prefix tree
	
				ins(_prefix, _length, _nexthop, _pnode->sRoot, 0, _level);
			}
			else { 
		
				if (_pnode->t < MAX_PREFIX_NUM) { // current pnode is not full, insert into current node
		
					_pnode->prefix[t] = _prefix;

					_pnode->length[t] = _length;			
	
					_pnode->nexthop[t] = _nexthop;

					arrangeOrder();

					_pnode_t++;
				}
				else { // current node is full, two situations are considered with respect to len(_prefix)
				
					if (_pnode->length[MAX_PREFIX_NUM - 1] < _length) {

						uint32 prefix = _pnode->prefix[MAX_PREFIX_NUM - 1];
						
						uint8 length = _pnode->length[MAX_PREFIX_NUM - 1];

						uint32 nexthop = _pnode->nexthop[MAX_PREFIX_NUM - 1];

						_pnode->prefix[MAX_PREFIX_NUM - 1] = _prefix;

						_pnode->length[MAX_PREFIX_NUM - 1] = _length;

						_pnode->nexthop[MAX_PREFIX_NUM - 1] = _nexthop;

						rearrangeOrder();
					
						ins(prefix, length, nexthop, _pnode->child[utility::getBitsValue(prefix, _level * K, _level * (K + 1))], _level + 1);
 									
					}
					else {

						ins(_prefix, _length, _nexthop, _pnode->child[utility::getBitsValue(_prefix, _level * K, _level * (K + 1))], _level + 1);
					}
				}
			}
		}
	}


	/// \brief insert a prefix into the auxiliary tree
	///
	/// If len(_prefix) < K * (_level + 1), then insert it into the auxiliary prefix tree of current node. Otherwise:
	/// (1) If _node.t != MAX_PREFIX_NUM, then insert it into current node.
	/// (2) If _node.t == MAX_PREFIX_NUM && len(_node.prefix[MAX_PREFIX_NUM - 1]) < len(_prefix), then replace _node.prefix[MAX_PREFIX_NUM - 1] with
	///     _prefix and recursively insert _node.prefix[MAX_PREFIX_NUM - 1] into a higher level.
	/// (3) If _node.t == MAX_PREFIX_NUM && len(_node.prefix[MAX_PREFIX_NUM - 1]) >= len(_prefix), then continue to insert _prefix into a higher level.
	void ins(const ip_type& _prefix, const uint8& _length, const uint32& _nexthop, snode_type*& _snode, const int _sLevel, const int _pLevel) {

		// create new node if necessary
		if (nullptr == _snode) {

			_snode = new snode_type();

			mSNodeNum++;

			_snode->prefix = _prefix;

			_snode->length = _length;

			_snode->nexthop = _nexthop;

			return;
		}
		else {

			if (length == _pLevel * K + _sLevel) {

				if (_snode->length > _pLevel * K + _sLevel) {

					uint32 prefix = _node->prefix;

					uint8 length = _node->length;

					uint32 nexthop = _node->nexthop;

					_node->prefix = _prefix;

					_node->length = _length;

					_node->nexthop = _nexthop;

					if (0 == utility::getBitValue(prefix, _pLevel * K + _sLevel)) {

						ins(prefix, length, nexthop, _node->lchild, _sLevel + 1, _pLevel);
					}
					else {

						ins(prefix, length, nexthop, _node->rchild, _sLevel + 1, _pLevel);
					}
				}
				else {

					if (0 == utility::getBitValue(prefix, _pLevel * K + _sLevel)) {

						ins(_prefix, _length, nexthop, _node->lchild, _sLevel + 1, _pLevel); 
					}
					else {

						ins(_prefix, _length, nexthop, _node->rchild, _sLevel + 1, _pLevel);
					}
				}
			}
		}

		return;
	}

	/// \brief delete a prefix in a multi-prefix tree
	///
	/// 
	void del(){


	}


	void del(){


	}
	void traverse() {

		return;
	}
	 
	void printNode(node_type* _node) {


	}

	uin32 search(const ip_type& _ip) {

		return;
	}

	voi del(){

		return;
	}

};

template<int W>
MPTree<W>* MPTree<W>::mpt = nullptr;

template<int W>
MPTree<W>* MPTree<W>::getInstance() {

	if (nullptr == mpt) {

		mpt = new MPTree();
	}

	return mpt;
}



#endif
