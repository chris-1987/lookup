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
#include <cmath>

/// \brief secondary node in a multi-prefix tree
/// 
/// \param W 32 or 128 for IPv4 or IPv6, respectively
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


/// \brief primary node in a multi-prefix tree
///  
/// \param W 32 or 128 for IPv4 or IPv6, respectively
/// \param K stride
/// \param MAX_PREFIX_NUM number of prefixes in a primary node
/// \param MAX_CHILD_NUM number of childs in a primary node
template<int W, int K, size_t MAX_PREFIX_NUM = 2 * K + 1, size_t MAX_CHILD_NUM = static_cast<size_t>(pow(2, K))>
struct PNode{

	typedef typename choose_ip_type<W>::ip_type ip_type;	
	
	uint8 t; ///< number of prefixes currently stored in the node

	ip_type prefix[MAX_PREFIX_NUM]; ///< prefixes sorted in non-increasing order, at most 2 * K + 1

	uint8 length[MAX_PREFIX_NUM]; ///< corresponding prefix length

	uint32 nexthop[MAX_PREFIX_NUM]; ///< corresponding nexthop

	PNode<W, K>* child[MAX_CHILD_NUM]; ///< child pointers, 2^K

	SNode<W>* sRoot; ///< pointer to the root of auxiliary prefix tree	

	PNode() : t(0), sRoot(nullptr) {

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



/// \brief build and update a multi-prefix tree
///
/// 
/// \param W 32 and 128 for IPv4 and IPv6, respectively
template<int W, int K, size_t MAX_PREFIX_NUM = 2 * K + 1, size_t MAX_CHILD_NUM = static_cast<size_t>(pow(2, K))>
class MPTree {
private:

	typedef typename choose_ip_type<W>::ip_type ip_type;

	typedef PNode<W, K, MAX_PREFIX_NUM, MAX_CHILD_NUM> pnode_type; ///< primary node type

	typedef SNode<W> snode_type; ///< secondary node type

	static MPTree* mpt; ///< pointer to mpt

	pnode_type* pRoot; ///< root node of the multi-prefix tree

	uint32 mPNodeNum; ///< number of primary nodes

	uint32 mSNodeNum; ///< number of secondary nodes


private:
	
	/// \brief default ctor
	MPTree() : pRoot(nullptr), mPNodeNum(0), mSNodeNum(0) {}

	MPTree(const MPTree& _mpt) = delete;

	MPTree& operator= (const MPTree&) = delete;

	~MPTree() {

		if (nullptr != pRoot) {

			destroy();
		}
	}

public:

	/// \brief create an instance of MPTree if not yet instantiated
	static MPTree* getInstance();

	/// \brief build BTree for BGP table
	void build(const std::string & _fn) {

		// destroy the existing MPTree (if any)		
		if (nullptr != pRoot) {

			destroy();
		}

		// insert prefixes into MPTree one by one
		std::ifstream fin(_fn, std::ios_base::binary);

		std::string line;

		ip_type prefix;

		uint8 length;

		uint32 nexthop;

		while (getline(fin, line)) {

			utility::retrieveInfo(line, prefix, length);

			nexthop = length; 

			if (0 == length) { // */0

				// do nothing
			}
			else {

				ins(prefix, length, nexthop, pRoot, 0);
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

		std::queue<pnode_type*> pqueue;

		std::queue<snode_type*> squeue;

		pqueue.push(pRoot);

		while(!pqueue.empty()) {

			auto pfront = pqueue.front();
	
			// destroy the auxiliary prefix tree
			if (nullptr != pfront->ptRoot) {

				squeue.push(pfront->ptRoot);

				while (!squeue.empty()) {

					auto sfront = squeue.front();

					if (nullptr != sfront->lchild) squeue.push(sfront->lchild);

					if (nullptr != sfront->rchild) squeue.push(sfront->rchild);

					delete sfront;

					squeue.pop();
				}
			}

			// push child nodes
			for (size_t i = 0; i < MAX_CHILD_NUM; ++i) {

				if (nullptr != pfront->child[i]) {

					pqueue.push(pfront->child[i]);
				}
			}

			delete pfront;

			pqueue.pop();
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
			else { // insert into current node or a higher level
		
				if (_pnode->t < MAX_PREFIX_NUM) { // current pnode is not full, insert into current node
	
					insertPrefixInPNode(_pnode, _prefix, _length, _nexthop);
				}
				else { // current node is full, two situations are considered with respect to len(_prefix)
				
					if (_pnode->length[MAX_PREFIX_NUM - 1] < _length) { // insert prefix into current node

						// copy last prefix in current node
						uint32 prefix = _pnode->prefix[MAX_PREFIX_NUM - 1];
						
						uint8 length = _pnode->length[MAX_PREFIX_NUM - 1];

						uint32 nexthop = _pnode->nexthop[MAX_PREFIX_NUM - 1];

						// delete last prefix from current node
						deletePrefixInPNode(_pnode, MAX_PREFIX_NUM - 1);	
					 
						// insert prefix into current node
						insertPrefixInPNode(_pnode, _prefix, _length, _nexthop);

						// recursively insert the deleted prefix into a higher level	
						ins(prefix, length, nexthop, _pnode->child[utility::getBitsValue(prefix, _level * K, _level * (K + 1) - 1)], _level + 1);
 									
					}
					else { // insert prefix into a higher level

						ins(_prefix, _length, _nexthop, _pnode->child[utility::getBitsValue(_prefix, _level * K, _level * (K + 1) - 1)], _level + 1);
					}
				}
			}
		}
	}


	/// \brief insert prefix into a non-full pnode
	void insertPrefixInPNode(PNode<W, K>* _pnode, const ip_type& _prefix, const uint8& _length, const uint32& _nexthop) {

		int i = 0;

		// find the position to insert the prefix
		for (; i < _pnode->t; ++i) {

			if (_pnode->length[i] < _length) {

				break;				
			}
		}	

		// move elements
		for (int j = _pnode->t - 1; j >= i ; --j) {

			_pnode->prefix[j + 1] = _pnode->prefix[j];

			_pnode->length[j + 1] = _pnode->length[j];

			_pnode->nexthop[j + 1] = _pnode->nexthop[j];
		}

		// insert _prefix
		_pnode->prefix[i] = _prefix;

		_pnode->length[i] = _length;

		_pnode->nexthop[i] = _nexthop;

		// increment t
		_pnode->t++;
	
	}

	/// \brief delete a prefix at certain position from a node
	void deletePrefixInPNode(PNode<W, K>* _pnode, const int _pos){

		// move elements to override the deleted prefix
		for (int i = _pos + 1; i < _pnode->t; ++i) {

			_pnode->prefix[i - 1] = _pnode->prefix[i];

			_pnode->length[i - 1] = _pnode->length[i];

			_pnode->nexthop[i - 1] = _pnode->nexthop[i];
		} 

		// decrement t
		_pnode->t--;
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

			if (_length == _pLevel * K + _sLevel) { // must be inserted into current level

				if (_snode->length > _pLevel * K + _sLevel) { // replace

					uint32 prefix = _snode->prefix;

					uint8 length = _snode->length;

					uint32 nexthop = _snode->nexthop;

					_snode->prefix = _prefix;

					_snode->length = _length;

					_snode->nexthop = _nexthop;


					// recursively insert replaced prefix
					if (0 == utility::getBitValue(prefix, _pLevel * K + _sLevel)) {

						ins(prefix, length, nexthop, _snode->lchild, _sLevel + 1, _pLevel);
					}
					else {

						ins(prefix, length, nexthop, _snode->rchild, _sLevel + 1, _pLevel);
					}	
				}
				else { // prefix in current node must be equal to the one to be inserted

				}
			}
			else { // isnert into a higher level

				if (0 == utility::getBitValue(_prefix, _pLevel * K + _sLevel)) {

					ins(_prefix, _length, _nexthop, _snode->lchild, _sLevel + 1, _pLevel); 
				}
				else {

					ins(_prefix, _length, _nexthop, _snode->rchild, _sLevel + 1, _pLevel);
				}
			}
		}

		return;
	}


	/// \brief delete a prefix in a multi-prefix tree
	///
	/// If len(_prefix) < K * (_level + 1), then attempt to delete the prefix in the auxiliary prefix tree of current node. Otherwise:
	/// (1) If _prefix is in current node, there are two subcases:
	/// (1-1) if current node is an external node, then directly remove the prefix in the node
	/// (1-2) if current node is an internal node, then remove the prefix in the node, insert the longest prefix y in the child nodes to current node, 
	/// 	and recursively delete y in the child node.
	/// (2) If _prefix in not in current node, we branch to one of the child node to continue the delete operation.
	void del(const ip_type& _prefix, const uint8& _length, PNode<W, K>*& _pnode, const int _level){

		if (nullptr == _pnode) return;

		if (_length < _level * (K + 1)) { // prefix is in the auxiliary prefix tree if exists

			del(_prefix, _length, _pnode->ptRoot, 0, _level);

			// 
			if (nullptr == _pnode->ptRoot && 0 == _pnode->t) {

				bool hasChild = false;
	
				for (int i = 0; i < MAX_CHILD_NUM; ++i) {

					if (nullptr != _pnode->child[i]) {
	
						hasChild = true;
	
						break;
					}
				}
				
				if (false == hasChild) {

					delete _pnode;

					_pnode = nullptr;
				}

			}
		}
		else { // search in primary node and higher levels

			int _pos = findPrefixInPNode(_pnode, _prefix, _length);

			if (_pos < _pnode->t) { // find the prefix in the primary node
	
				// delete prefix in current node
				deletePrefixInPNode(_pnode, _pos);

				// check if an external primary node
				bool hasChild = false;

				for (int i = 0; i < MAX_CHILD_NUM; ++i) {
		
					if (nullptr != _pnode->child[i]) {

						hasChild = true;

						break;
					}			
				}
			
				if (true == hasChild) {

					// find the longest prefix in its child nodes
					ip_type long_prefix;

					uint8 long_length;
		
					uint32 long_nexthop;

					size_t childIdx;
		
					findLongestPrefixInChild(_pnode, childIdx, long_prefix, long_length, long_nexthop);	

					insertPrefixInPNode(_pnode, long_prefix, long_length, long_nexthop);

					del(long_prefix, long_length, _pnode->child[childIdx], _level + 1);
				}
				else {

					if (0 == _pnode->t && nullptr == _pnode->ptRoot) {

						delete _pnode;
						
						_pnode = nullptr;
					}
				}



				ip_type long_prefix;

				uint8 long_length;

				uint32 long_nexthop;
 
				for (int i = 0; i < MAX_CHILD_NUM; ++i) {

					if (nullptr != _pnode->child[i]) {

						

					}
				}
			}
			else {	// search in higher levels
		
				del(_prefix, _length, _pnode->child[utility::getBitsValue(_prefix, _level * K , _level * (K + 1) - 1)], _level + 1);	
			}	
		}

		return;
	}

	/// \brief try to find a prefix in the primary node
	///
	/// \return position of prefix in the node if any; otherwise, t.
	int findPrefixInPNode(PNode<W, K>* _pnode, const ip_type& _prefix, const uint8 _length){

		for (int i = 0; i < _pnode->t; ++i) {

			if(_length == _pnode->length[i] && _prefix == _pnode->prefix[i]) {

				return i;
			}
		}
		
		return _pnode->t;
	}

	/// \brief delete a prefix in the auxiliary prefix tree
	void del(const ip_type & _prefix, const uint8& _length, const uint32& _nexthop, SNode<W>*& _snode, const int _sLevel, const int _pLevel){

		if (nullptr == _snode) {

			return;
		}

		if (_length == _snode->length && _prefix == _snode->prefix) {

			if (nullptr == _snode->lchild && nullptr == _snode->rchild) {

				delete _snode;

				_snode  = nullptr;

				return;
			}
			else {
			
				snode_type* parent_node = _snode;

				snode_type* child_node = nullptr;

				bool isLeftBranch = true;

				if (nullptr != parent_node->lchild) {

					isLeftBranch = true;

					child_node = parent_node->lchild;
				}
				else {

					isLeftBranch = false;

					child_node = parent_node->rchild;
				}

				// find leaf descendant
				while (nullptr != child_node->lchild || nullptr != child_node->rchild) {

					if (nullptr != child_node->lchild) {

						parent_node = child_node;

						child_node = child_node->lchild;

						isLeftBranch = true;
					}
					else {

						parent_node = child_node;

						child_node = child_node->rchild;

						isLeftBranch = false;
					}
				}


				// replace
				_snode->prefix = child_node->prefix;

				_snode->length = child_node->length;

				_snode->nexthop = child_node->nexthop;

				// reset child pointers
				if (isLeftBranch) {

					parent_node->lchild = nullptr;
				}
				else {

					parent_node->rchild = nullptr;
				}

				delete child_node;

				return;
			}
			
		}
		else {

			if (0 == utility::getBitValue(_prefix, _sLevel + 1, _pLevel) {

				del(_prefix, _length, _nexthop, _snode->lchild, _sLevel + 1, _pLevel);
			}
			else {

				del(_prefix, _length, _nexthop, _snode->rchild, _sLevel + 1, _pLevel);
			}
		}
	}

	/// \brief 
	void findLongestPrefixInChild(PNode<W, K>* _pnode, size_t& _childIdx, ip_type& _longPefix, uint8& _longLength, uint32 _longNexthop){

		_longLength = 0;

		for (size_t i = 0; i < MAX_CHILD_NUM; ++i) {

			// try to find the longest in pnode
			if (nullptr != _pnode->child[i]) {

				if (0 != _pnode->t) {

					if (_pnode->length[0] > _longLength) {

						_longLength = _pnode->length[0];
			
						_longPrefix = _pnode->prefix[0];

						_longNexthop = _pnode->nexthop[0];

						_childIdx = i;
					}
				}				
			}
		}

		if (0 == _longLength) {

			for (size_t i = 0; i < MAX_CHILD_NUM; ++i) {

				if (nullptr != _pnode->ptRoot) {

					snode_type* snode = _pnode->ptRoot;

					std::queue<snode_type*> queue;
			
					queue.push(_pnode->ptRoot);

					while (!queue.empty()) {

						auto front = queue.front();

						if (nullptr != front->lchild) queue.push(front->lchild);

						if (nullptr != front->rchild) queue.push(front->rchild);

						if (front->length > _longLength) {

							_longLength = front->length;

							_longPrefix = front->prefix;

							_longNexthop = front->nexthop;

							_childIdx = i;
						}						

						queue.pop();
					}
				}
			}
		}

		return;
	}
	
	/// \brief traverse a multi-prefix tree
	void traverse() {

		
		return;
	}
	
	/// \brief print a pnode	 
	void printPNode(pnode_type* _pnode) {


	}

	/// \brief print an snode
	void printSNode(snode_type* _snode) {


	}

	/// \brief search LPM for the given IP address
	uint32 search(const ip_type& _ip) {

		return;
	}

	/// \brief arrange order of prefixes in a primary node
	///
	/// \param
	void arrangeOrder(PNode<W, K, MAX_PREFIX_NUM, MAX_CHILD_NUM>* _pnode, const int _pos, bool _isDel) {

		if (_isDel) { // delete a prefix at _pos

			for (size_t i = _pos + 1; i < _pnode->t; ++i) {

				_pnode->prefix[i - 1] = _pnode->prefix[i];

				_pnode->length[i - 1] = _pnode->length[i];

				_pnode->nexthop[i - 1] = _pnode->nexthop[i]; 
			}
		}
		else { // insert a prefix at _pos

			for (size_t i = 0; i < _pos; ++i) {

			}
		}
		
	}
};

template<int W, int K, size_t MAX_PREFIX_NUM, size_t MAX_CHILD_NUM>
MPTree<W, K, MAX_PREFIX_NUM, MAX_CHILD_NUM>* MPTree<W, K, MAX_PREFIX_NUM, MAX_CHILD_NUM>::mpt = nullptr;

template<int W, int K, size_t MAX_PREFIX_NUM, size_t MAX_CHILD_NUM>
MPTree<W, K, MAX_PREFIX_NUM, MAX_CHILD_NUM>* MPTree<W, K, MAX_PREFIX_NUM, MAX_CHILD_NUM>::getInstance() {

	if (nullptr == mpt) {

		mpt = new MPTree();
	}

	return mpt;
}



#endif
