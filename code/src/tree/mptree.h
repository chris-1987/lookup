#ifndef _MPT_H
#define _MPT_H

///////////////////////////////////////////////////////////////////////////////////////////////
/// Copyright (c) 2016, Sun Yat-sen University
/// All rights reserved
/// \file mptree.h
/// \brief definition of multi-prefix tree
///
/// Build, insert and delete prefixes in a multi-bit tree
/// An implementation of the algorithm designed by Sun-Yuan Hsieh.
/// Please refer to "A novel dynamic router-tables design for IP lookup and update" for more details.
///
/// \author Yi Wu
/// \date 2016.11
///////////////////////////////////////////////////////////////////////////////////////////////


#include "../common/common.h"
#include "../common/utility.h"
#include <queue>
#include <cmath>

#define DEBUG_MPT

/// \brief Secondary node in a multi-prefix tree.
/// 
/// Each secondary node is a 5-tuple consisting of <prefix, length of prefix, nexthop, ptr to left child, ptr to right child>.
///
/// \param W length of IP address, 32 or 128 for IPv4 or IPv6, respectively
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


/// \brief Primary node in a multi-prefix tree.
///  
/// Each node can store at most MP prefixes and MC child pointers 
///
/// \param W length of IP address, 32 or 128 for IPv4 or IPv6, respectively
/// \param K stride
/// \param MP number of prefixes in a primary node, 2 * K + 1 in default
/// \param MC number of child pointers in a primary node, pow(2, K) in default
template<int W, int K, size_t MP = 2 * K + 1, size_t MC = static_cast<size_t>(pow(2, K))>
struct PNode{

	typedef typename choose_ip_type<W>::ip_type ip_type;	
	
	typedef PNode<W,K> pnode_type;

	typedef SNode<W> snode_type; 

	uint8 t; ///< number of prefixes currently stored in the primary node, at most 2K + 1

	/// \brief Prefix entry.
	///
	/// Each entry consists of three items, which are prefix, length of prefix and nexthop.
	///
	struct PrefixEntry{
		
		ip_type prefix;
			
		uint8 length;

		uint32 nexthop;	

		PrefixEntry() : prefix(0), length(0), nexthop(0) {}
	};
	
	PrefixEntry prefixEntries[MP]; ///< store at most MP prefix entries

	pnode_type* childEntries[MC]; ///< store at most MC childs

	snode_type* sRoot; ///< pointer to the root of auxiliary prefix tree	

	PNode() : t(0), sRoot(nullptr) {

		for (size_t i = 0; i < MC; ++i) {

			childEntries[i] = nullptr;
		}
	}

	~PNode() {

	}
};



/// \brief Build and update a multi-prefix tree.
///
/// Two kinds of nodes appear in a multi-prefix tree, namely primary node and secondary node.
/// 
/// \param W 32 and 128 for IPv4 and IPv6, respectively
/// \param K stride
/// \param MP at most MP prefixes can be stored in a primary node, MP = 2 * K + 1
/// \param MC at most MC child pointers can be stored in a primary node, MC = pow(2, K)
template<int W, int K, size_t MP = 2 * K + 1, size_t MC = static_cast<size_t>(pow(2, K))>
class MPTree {
private:

	typedef typename choose_ip_type<W>::ip_type ip_type;

	typedef PNode<W, K> pnode_type; ///< primary node type

	typedef SNode<W> snode_type; ///< secondary node type

	static MPTree* mpt; ///< pointer to mpt

	pnode_type* pRoot; ///< root node of the multi-prefix tree, a primary node

	uint32 mPNodeNum; ///< number of primary nodes in total

	uint32 mSNodeNum; ///< number of secondary nodes in total

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

			pRoot = nullptr;
		}

		// read prefixes from BGPtable and insert them one by one into MPTree
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

		return;
	}

	/// \brief destroy a multi-prefix tree
	void destroy() {

		if (nullptr == pRoot) {

			return;
		}

		std::queue<pnode_type*> pqueue;

		std::queue<snode_type*> squeue;

		pqueue.push(pRoot);

		while(!pqueue.empty()) {

			auto pfront = pqueue.front();
	
			// first destroy the auxiliary prefix tree if there exists any
			if (nullptr != pfront->sRoot) {

				squeue.push(pfront->sRoot);

				while (!squeue.empty()) {

					auto sfront = squeue.front();

					if (nullptr != sfront->lchild) squeue.push(sfront->lchild);

					if (nullptr != sfront->rchild) squeue.push(sfront->rchild);

					delete sfront;

					squeue.pop();
				}
			}

			// push child nodes
			for (size_t i = 0; i < MC; ++i) {

				if (nullptr != pfront->childEntries[i]) {

					pqueue.push(pfront->childEntries[i]);
				}
			}

			delete pfront;

			pqueue.pop();
		}
		
		return;
	}

	/// \brief Insert a prefix into multi-prefix tree.
	///
	/// If len(_prefix) < K * (_level + 1), then insert _prefix into the auxiliary prefix tree rooted at _node->sRoot. Otherwise:
	/// (1) If _node.t != MP, then _node is not full, insert _prefix into current node.
	/// (2) If _node.t == MP && len(_node.prefix[MP - 1]) < len(_prefix), then replace _node.prefix[MP - 1] with
	///     _prefix, reorde prefixes in _node and recursively insert the replaced prefix into a higher level.
	/// (3) If _node.t == MP && len(_node.prefix[MP - 1]) >= len(_prefix), then continue to insert _prefix into a higher level.
	void ins(const ip_type& _prefix, const uint8& _length, const uint32& _nexthop, pnode_type*& _pnode, const int _level) {

		if (nullptr == _pnode) { // create a new node

			_pnode = new pnode_type();

			mPNodeNum++;
		}

		if (_length < (_level + 1) * K) { // insert _prefix into auxiliary prefix tree rooted at sRoot
	
			ins(_prefix, _length, _nexthop, _pnode->sRoot, 0, _level); 
		}
		else {
		
			if (_pnode->t < MP) { // _pnode is not full, insert _prefix into _pnode
	
				insertPrefixInPNode(_pnode, _prefix, _length, _nexthop);
	
//				printPNode(_pnode);
			}
			else { // _pnode is full
	
				if (_pnode->prefixEntries[MP - 1].length < _length) { // shortest prefix in _pnode is shorter than _length, replace
				
					// copy shortest prefix in _pnode
					uint32 prefix = _pnode->prefixEntries[MP - 1].prefix;
					
					uint8 length = _pnode->prefixEntries[MP - 1].length;

					uint32 nexthop = _pnode->prefixEntries[MP - 1].nexthop;

					// delete shortest prefix (located at MP - 1) from current pnode
					deletePrefixInPNode(_pnode, MP - 1);	
				 
					// insert _prefix into current pnode
					insertPrefixInPNode(_pnode, _prefix, _length, _nexthop);

					// recursively insert the deleted shorted prefix into a higher level	
					ins(prefix, length, nexthop, _pnode->childEntries[utility::getBitsValue(prefix, _level * K, (_level + 1) * K - 1)], _level + 1);	
				}
				else { // insert _prefix into a higher level

					ins(_prefix, _length, _nexthop, _pnode->childEntries[utility::getBitsValue(_prefix, _level * K, (_level + 1) * K - 1)], _level + 1);
				}
			}
		}

		return;
	}


	/// \brief insert prefix into a non-full pnode
	void insertPrefixInPNode(pnode_type* _pnode, const ip_type& _prefix, const uint8& _length, const uint32& _nexthop) {

		int i = 0;

		// find the position to insert the prefix
		for (; i < _pnode->t; ++i) {

			if (_pnode->prefixEntries[i].length < _length) {

				break;				
			}
		}	

		// move elements one slot leftward
		for (int j = _pnode->t - 1; j >= i ; --j) {

			_pnode->prefixEntries[j + 1].prefix = _pnode->prefixEntries[j].prefix;

			_pnode->prefixEntries[j + 1].length = _pnode->prefixEntries[j].length;

			_pnode->prefixEntries[j + 1].nexthop = _pnode->prefixEntries[j].nexthop;
		}

		// insert _prefix
		_pnode->prefixEntries[i].prefix = _prefix;

		_pnode->prefixEntries[i].length = _length;

		_pnode->prefixEntries[i].nexthop = _nexthop;

		// increment t
		_pnode->t++;
	}


	/// \brief delete a prefix at certain position from a primary node
	void deletePrefixInPNode(PNode<W, K>* _pnode, const int _pos){

		// move elements to override the deleted prefix
		for (int i = _pos + 1; i < _pnode->t; ++i) {
	
			_pnode->prefixEntries[i - 1].prefix = _pnode->prefixEntries[i].prefix;

			_pnode->prefixEntries[i - 1].length = _pnode->prefixEntries[i].length;

			_pnode->prefixEntries[i - 1].nexthop = _pnode->prefixEntries[i].nexthop;
		} 

		// decrement t
		_pnode->t--;
	}
			
	

	/// \brief Insert a prefix into the auxiliary tree.
	void ins(const ip_type& _prefix, const uint8& _length, const uint32& _nexthop, snode_type*& _snode, const int _sLevel, const int _pLevel) {

		if (nullptr == _snode) { // create a new snode and insert the prefix

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

					// copy prefix in current snode
					uint32 prefix = _snode->prefix;

					uint8 length = _snode->length;

					uint32 nexthop = _snode->nexthop;

					// replace 
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

					// do nothing
				}
			}
			else { // insert into a higher level

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


	/// \brief search LPM for the given IP address
	uint32 search(const ip_type& _ip) {

		if (nullptr == pRoot) {

			return 0;
		}

		pnode_type* pnode = pRoot;

		int pLevel = 0;

		int sBestLength = 0; // record length of LPM

		int nexthop = 0;

		while (nullptr != pnode) {

			// search in pnode, if there exist a match, then it must be LPM
			for (size_t i = 0; i < pnode->t; ++i) {

				if (utility::getBitsValue(_ip, 0, pnode->prefixEntries[i].length - 1) == utility::getBitsValue(pnode->prefixEntries[i].prefix, 0, pnode->prefixEntries[i].length - 1)) {

					return pnode->prefixEntries[i].nexthop;
				}
			}

			// search in snode, if find a match, record it in (bestLength, nexthop)
			if (nullptr != pnode->sRoot) {

				int sLevel = 0;

				snode_type* snode = pnode->sRoot;

				while (nullptr != snode) {

					if (utility::getBitsValue(_ip, 0, snode->length - 1) == utility::getBitsValue(snode->prefix, 0, snode->length - 1)) {

						if (sBestLength < snode->length) {

							sBestLength = snode->length;

							nexthop = snode->nexthop;
						}					
					}

					if (0 == utility::getBitValue(_ip, K * pLevel + sLevel)) {

						snode = snode->lchild;
					}
					else {

						snode = snode->rchild;
					}

					++sLevel;
				}
			}

			// search in higher level
			pnode = pnode->childEntries[utility::getBitsValue(_ip, pLevel * K, (pLevel + 1) * K - 1)];

			++pLevel;
		}

		return nexthop;
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

		if (_length < (_level + 1) * K) { // if _prefix exists in the multi-prefix tree, then it must be in the auxiliary prefix tree

			// delete _prefix in the auxiliary prefix tree
			del(_prefix, _length, _pnode->sRoot, 0, _level);

			// delete operation is finshed
			// if _pnode is an external node and there remains no prefixes in _pnode and the subtrie rooted at sRoot, then remove the pnode. 
			if (nullptr == _pnode->sRoot && 0 == _pnode->t) { // if 0 == _pnode->t, then _pnode must be an external node

				delete _pnode;

				_pnode = nullptr;

				--mPNodeNum;
			}
		}
		else { // search in pnode and higher levels

			// try to find the prefix in pnode
			int _pos = findPrefixInPNode(_pnode, _prefix, _length);

			if (_pos != _pnode->t + 1) { // prefix is found in the primary node
	
				// delete prefix from _pnode
				deletePrefixInPNode(_pnode, _pos);

				// check whether or not _pnode is an external node
				bool hasChild = false;

				for (size_t i = 0; i < MC; ++i) {

					if (nullptr != _pnode->childEntries[i]) {

						hasChild = true;

						break;
					}
				}
			
				if (true == hasChild) { // an internal node

					ip_type long_prefix = 0;

					uint8 long_length = 0;
		
					uint32 long_nexthop = 0;

					size_t childIdx = 0;
		
					// find longest prefix in child nodes
					findLongestPrefixInChild(_pnode, childIdx, long_prefix, long_length, long_nexthop);	

					// insert the longest prefix in _pnode
					insertPrefixInPNode(_pnode, long_prefix, long_length, long_nexthop);

					// delete the longest prefix in child node
					del(long_prefix, long_length, _pnode->childEntries[childIdx], _level + 1);
				}
				else { // an external node
					
					//delete operation is finished
					// if there remains no prefixes in _pnode and the subtrie rooted at sRoot, then remove _pnode 
					if (0 == _pnode->t && nullptr == _pnode->sRoot) {

						delete _pnode;

						_pnode = nullptr;

						--mPNodeNum;						
					}
				}
			}
			else {	// search in higher levels
		
				del(_prefix, _length, _pnode->childEntries[utility::getBitsValue(_prefix, _level * K , (_level + 1) * K - 1)], _level + 1);
			}	
		}

		return;
	}


	/// \brief try to find a prefix in the primary node
	///
	/// \return position of prefix in the node if any; otherwise, t.
	int findPrefixInPNode(pnode_type* _pnode, const ip_type& _prefix, const uint8& _length){

		for (int i = 0; i < _pnode->t; ++i) {

			if (_length == _pnode->prefixEntries[i].length && _prefix == _pnode->prefixEntries[i].prefix) {

				return i;
			}
		}
		
		return _pnode->t + 1;
	}

	/// \brief delete a prefix in the auxiliary prefix tree
	void del(const ip_type& _prefix, const uint8& _length, snode_type*& _snode, const int _sLevel, const int _pLevel){

		if (nullptr == _snode) return;

		if (_length == _snode->length && _prefix == _snode->prefix) { // find the prefix

			if (nullptr == _snode->lchild && nullptr == _snode->rchild) { // external node, delete it

				delete _snode;

				_snode = nullptr;

				--mSNodeNum;

				return;
			}
			else { // internal node, find a leaf node to replace it
			
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

				child_node = nullptr;

				--mSNodeNum;

				return;
			}	
		}
		else { // try to find in a higher level

			if (0 == utility::getBitValue(_prefix, _pLevel * K + _sLevel)) {

				del(_prefix, _length, _snode->lchild, _sLevel + 1, _pLevel);
			}
			else {

				del(_prefix, _length, _snode->rchild, _sLevel + 1, _pLevel);
			}
		}
	}

	/// \brief find the longest prefix in the child nodes of a pnode
	///
	/// If all the child nodes have no prefixes, then they are external nodes and the longest prefix is in one of the auxiliary prefix tree
	/// If at least one child node is an internal node, then the longest prefix can be found in one of the child node.
	void findLongestPrefixInChild(PNode<W, K>* _pnode, size_t& _childIdx, ip_type& _longPrefix, uint8& _longLength, uint32& _longNexthop){

		_longLength = 0;

		// find longest in child nodes
		for (size_t i = 0; i < MC; ++i) {

			pnode_type* cnode = _pnode->childEntries[i];

			if (nullptr != cnode) {

				if (0 != cnode->t) {

					if (cnode->prefixEntries[0].length > _longLength) {

						_longLength = cnode->prefixEntries[0].length;
			
						_longPrefix = cnode->prefixEntries[0].prefix;

						_longNexthop = cnode->prefixEntries[0].nexthop;

						_childIdx = i;
					}
				}				
			}
		}

		// All primary nodes have no prefixes, indicating that they are all external nodes. 
		// Find the longest prefix in the auxiliary prefix trees
		if (0 == _longLength) { 

			for (size_t i = 0; i < MC; ++i) {

				pnode_type* cnode = _pnode->childEntries[i];

				if (nullptr != cnode && nullptr != cnode->sRoot) {

					std::queue<snode_type*> queue;
			
					queue.push(cnode->sRoot);

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
	
	void printPNode(const pnode_type* _pnode) const {

		for (size_t i = 0; i < MP; ++i) {

			std::cerr << "prefix: " << _pnode->prefixEntries[i].prefix << " lengh: " << (uint32)_pnode->prefixEntries[i].length << std::endl;
		}

		std::cerr << std::endl;
	}	

	pnode_type*& getRoot() {

		return pRoot;
	}

	void report(){

		std::cerr << "mSNodeNum: " << mSNodeNum << std::endl;

		std::cerr << "mPNodeNum: " << mPNodeNum << std::endl;
	}
};

template<int W, int K, size_t MP, size_t MC>
MPTree<W, K, MP, MC>* MPTree<W, K, MP, MC>::mpt = nullptr;

template<int W, int K, size_t MP, size_t MC>
MPTree<W, K, MP, MC>* MPTree<W, K, MP, MC>::getInstance() {

	if (nullptr == mpt) {

		mpt = new MPTree();
	}

	return mpt;
}



#endif
