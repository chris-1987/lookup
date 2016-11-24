#ifndef _RMPT_H
#define _RMPT_H

///////////////////////////////////////////////////////////////////////////////////////////////
/// Copyright (c) 2016, Sun Yat-sen University
/// All rights reserved.
/// \file rmptree.h
/// \brief Definition of an IP lookup index based on multi-prefix tree.
///
/// This index consists of three parts: a fast lookup table containing route information
/// of short prefixes (shorter than U bits), a root table pointing to a forest of multi-prefix
/// trees, and a forest of multi-prefix trees.
///
/// \author Yi Wu
/// \date 2016.11
///////////////////////////////////////////////////////////////////////////////////////////////


#include "../common/common.h"
#include "../utility/utility.h"
#include <queue>
#include <cmath>
#include <chrono>

#define DEBUG_RMPT


/// \brief Secondary node in a multi-prefix tree.
/// 
/// Nodes in a multi-prefix tree are divided into two categories: primary node and secondary node. A secondary node is
/// a 5-tuple consisting of <prefix, length of prefix, nexthop, pointer to left child, pointer to right child>.
///
/// \param W length of IP address, 32 or 128 for IPv4 or IPv6, respectively.
template<int W>
struct SNode{

	typedef typename choose_ip_type<W>::ip_type ip_type;

	static const size_t size; /// size of a secondary node

	ip_type prefix; ///< prefix
	
	uint8 length; ///< length of prefix

	uint32 nexthop; ///< corresponding nexthop

	SNode* lchild; ///< ptr to left child

	SNode* rchild; ///< ptr to right child

	int stageidx; ///< pipe stage number

	SNode() : prefix(0), length(0), nexthop(0), lchild(nullptr), rchild(nullptr), stageidx(0) {}
};

template<int W>
const size_t SNode<W>::size = sizeof(ip_type) + sizeof(uint8) + sizeof(uint32) + sizeof(SNode*) + sizeof(SNode*); // stageidx is excluded


/// \brief Primary node in a multi-prefix tree.
///  
/// Nodes in a multi-prefix tree are divided into two categories: primary node and secondary node. A secondary node is
/// a tuple that sotres MP prefixes and MC child pointers (MP = 2 * K + 1, MC = pow(2, K).
///
/// \param W Length of IP address, 32 or 128 for IPv4 or IPv6, respectively.
/// \param K Stride of the tree. This argument determines MC and MP.
/// \param MP number of prefixes in a primary node, which is equal to  2 * K + 1. Please do not change it.
/// \param MC number of child pointers in a primary node, which is equal to pow(2, K). Please do not change it.
template<int W, int K, size_t MP = 2 * K + 1, size_t MC = static_cast<size_t>(pow(2, K))>
struct PNode{

	typedef typename choose_ip_type<W>::ip_type ip_type;	
	
	typedef PNode<W,K> pnode_type;

	typedef SNode<W> snode_type; 

	static const size_t size; ///< size of a primary node

	uint8 t; ///< number of prefixes currently stored in the primary node, at most 2K + 1

	int stageidx; ///< pipe stage number

	/// \brief Prefix entry.
	///
	/// A primary node consists of multiple prefix esntries, each entry records a prefix, the length of the prefix and its nexthop.
	/// A primary node also contains multiple child pointers.
	///
	struct PrefixEntry{
		
		ip_type prefix;
			
		uint8 length;

		uint32 nexthop;	

		PrefixEntry() : prefix(0), length(0), nexthop(0) {}
	};
	
	PrefixEntry prefixEntries[MP]; ///< store at most MP prefix entries

	pnode_type* childEntries[MC]; ///< store at most MC childs

	snode_type* sRoot; ///< pointer to the root of auxiliary prefix tree.

	PNode() : t(0), sRoot(nullptr), stageidx(0) {

		for (size_t i = 0; i < MC; ++i) {

			childEntries[i] = nullptr;
		}
	}

	~PNode() {

	}
};

const size_t PNode<W, K>::size = sizeof(uint8) + sizeof(PrefixEntry) * MP + sizeof(pnode_type*) * MC + sizeof(snode_type*); // t, prefixes, childs, sRoot





/// \brief Build the index (fast table + forest of multi-prefix trees).
///
/// Shorter prefixes are stored in the fast table while longer prefixes are stroed in the forest of multi-prefix trees.
/// 
/// \param W 32 and 128 for IPv4 and IPv6, respectively
/// \param K Stride of each multi-prefix tree.
/// \param MP at most MP prefixes can be stored in a primary node, MP = 2 * K + 1
/// \param MC at most MC child pointers can be stored in a primary node, MC = pow(2, K)
/// \param U A threshold for classifying shorter and longer prefixes. Specifically, the length of a shorter prefix is smaller than U,
/// while the length of a longer prefix is no less than U.
/// \param V Number of prefix trees at large.
template<int W, int K, size_t MP = 2 * K + 1, size_t MC = static_cast<size_t>(pow(2, K)), int U, size_t V = static_cast<size_t>(pow(2, U))>
class RMPTree {
private:

	typedef typename choose_ip_type<W>::ip_type ip_type;

	typedef PNode<W, K> pnode_type; ///< primary node type

	typedef SNode<W> snode_type; ///< secondary node type

	pnode_type* mRootTable[V]; ///< pointers to a forest of multi-prefix tree

	uint32 mPNodeNum[V]; ///< number of pnodes in each multi-prefix tree

	uint32 mSNodeNum[V]; ///< number of snodes in each multi-prefix tree

	FastTable<W, U - 1> *ft; ///< pointer to the fast lookup table

private:
	
	/// \brief default ctor
	RMPTree() {

		initializeParameters();
	}
	
	/// \brief initialize parameters
	void initializeParameters() {

		for (size_t i = 0; i < V; ++i) {

			mRootTable[i] = 0;
		}

		for (size_t i = 0; i < V; ++i) {

			mPNodeNum[i] = 0;
		}

		for (size_t i = 0; i < V; ++i) {

			mSNodeNum[i] = 0;
		}

		ft = new FastTable<W, U - 1>();
	}

	/// \brief disable copy-ctor
	RMPTree(const MPTree& _mpt) = delete;

	/// \brief disable assignment op
	RMPTree& operator= (const MPTree&) = delete;


	/// \brief dtor
	///
	/// destroy index
	~MPTree() {

		clear();
	}


	/// \brief clear
	void clear() {

		for (size_t i = 0; i < V; ++i) {

			if (nullptr != mRootTable[i]) {

				destroy(i); // destory the multi-prefix tree along with the auxiliary prefix trees

				mRootTable[i] = nullptr;
			}
		}
		
		delete ft;

		ft = nullptr;
	}

public:

	/// \brief Build the index.
	void build(const std::string & _fn) {

		// clear old index if there exists any
		clear();

		// initialize
		initializeParameters();

		// insert prefixes one by one into index
		std::ifstream fin(_fn, std::ios_base::binary);

		std::string line

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

				ins(prefix, length, nexthop);
			}
		}		

		report();

		// traverse();

		return;
	}

	/// \brief destroy a multi-prefix tree
	void destroy(size_t _idx) {

		if (nullptr == mRootTable[_idx]) {

			return;
		}

		pnode_type* pnode = mRootTable[_idx];

		std::queue<pnode_type*> pqueue;
			
		std::queue<snode_type*> squeue;

		pqueue.push(pnode);
		
		while (!pqueue.empty()) {

			auto pfront = pqueue.front();

			if (nullptr != pfront->sRoot) {

				squeue.push(pfront->sRoot);

				while (!squeue.empty()) {

					auto sfront = squeue.front();

					if (nullptr != sfront->lchild) squeue.push(sfront->lchild);

					if (nullptr != sfront->rchild) squeue.push(sfront->rchild);

					delete sfront;

					sfront = nullptr;

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

			pfront = nullptr;

			pqueue.pop();
		}
			
		return;
	}

	/// \brief Insert a prefix into the index. 
	///
	/// If the prefix is shorter than U bits, then insert it into the fast lookup table,
	/// otherwise, insert it into the the forest of multi-prefix trees.
	void ins(const ip_type& _prefix, const uint8& _length, const uint32& _nexthop) {

		if (_length < U) { // smaler than U bits, insert into the fast lookup table

			ft->ins(_prefix, _length, _nexthop);
		}
		else { // no fewer than U bits, insert into the forest of the multi-prefix trees

			// the starting level is 0 (not U bits)
			ins(_prefix, _length, _nexthop, mRootTable[utility::getBitsValue(_prefix, 0, U - 1)], 0, utility::getBitsValue(_prefix, 0, U - 1));
		}
		
		return;
	} 


	/// \brief Insert a prefix into the forest of multi-prefix trees.
	/// 
	/// The forest consists of 2^U multi-prefix trees. Determine into which a prefix is inserted by checking the first U bits of the prefix.
	/// When attempting to insert a prefix P into a node N at level L in a multi-prefix tree:
	/// (1) if len(P) < U + K * (_level + 1), then insert it into the auxiliary prefix tree;
	/// (2) if N is not full, then insert it into current node;
	/// (3) if N is full, then recursively insert it into a higher level.
	void ins(const ip_type& _prefix, const uint8& _length, const uint32& _nexthop, pnode_type*& _pnode, const int _level, const uint32 _treeIdx) {

		if (nullptr == _pnode) { // node is empty, create the node

			_pnode = new pnode_type();
	
			++mPNodeNum[_treeIdx];
		}

		if (_length < U + (_level + 1) * K) { // _level starts from 0, required to plus U

			// current prefix must be inserted into the auxiliary prefix tree
			ins(_prefix, _length, _nexthop, _pnode->sRoot, 0, _level, _treeIdx); // sLevel = 0, pLevel = _level	
		}
		else { // insert the prefix into current primary node or a node in a higher level

			if (_pnode->t < MP) { // current primary node is not full, insert prefix into current primary node

				insertPrefixInPNode(_pnode, _prefix,_length, _nexthop, _treeIdx);
			}
			else {
					
				// if the shortest prefix in current primary node is also shorter than the prefix to be inserted
				if (_pnode->prefixEntries[MP - 1].length < _length) { 

					// cache the shortest prefix in current primary node
					uint32 prefix = _pnode->prefixEntries[MP - 1].prefix;

					uint8 length = _pnode->prefixEntries[MP - 1].length;

					uint32 nexthop = _pnode->prefixEntries[MP - 1].nexthop;

					// delete shortest prefix in current primary node
					deletePrefixInPNode(_pnode, MP - 1);
					
					// insert new comer
					insertPrefixInPNode(_pnode, _prefix, _length, _nexthop);

					// recursively insert the prefix deleted from the primary node into a higher level
					ins(prefix, length, nexthop, _pnode->childEntries[utility::getBitsValue(prefix, U + _level * K, U + (_level + 1) * K - 1)], _level + 1, _treeIdx);
						
				}
				else {
				
					// insert prefix into a node in a higher level

					ins(_prefix, _length, _nexthop, _pnode->childEntries[utility::getBitsValue(_prefix, U + _level * K, U + (_level + 1) * K - 1)], _level + 1, _treeIdx);	
				}
			}
		}

		return;
	}	



	/// \brief Insert prefix into a non-full pnode
	///
	/// Prefixes in the node are sorted in non-decreasing order by their lengths. 
	/// This variant is maintained during the insertion.
	/// \note It is assumed that the primary node is not full yet.
	void insertPrefixInPNode(pnode_type* _pnode, const ip_type& _prefix, const uint8& _length, const uint32& _nexthop) {

		int i = 0;

		// find the position to insert the prefix
		for (; i < _pnode->t; ++i) {

			if (_pnode->prefixEntries[i].length < _length) {

				break;				
			}
		}	

		// move elements one slot rightward
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
	void ins(const ip_type& _prefix, const uint8& _length, const uint32& _nexthop, snode_type*& _snode, const int _sLevel, const int _pLevel, const uint32 _treeIdx) {

		if (nullptr == _snode) { // empty

			// create a new secondary node
			_snode = new snode_type();

			mSNodeNum[_treeIdx]++;	

			// insert the prefix
			_snode->prefix = _prefix;

			_snode->length = _length;

			_snode->nexthop = _nexthop;

			return;
		}
		else {

			if (_length == U + _pLevel * K + _sLevel) { // must be inserted into current level

				if (_snode->length > U + _pLevel * K + _sLevel) { // prefix in curretn node is longer than the one to be inserted

					// copy prefix in current secondary node
					uint32 prefix = _snode->prefix;

					uint8 length = _snode->length;

					uint32 nexthop = _snode->nexthop;

					// replaced by _prefix
					_snode->prefix = _prefix;

					_snode->length = _length;

					_snode->nexthop = _nexthop;

					// recursively insert the replaced prefix
					if (0 == utility::getBitValue(prefix, U + _pLevel * K + _sLevel)) {

						ins(prefix, length, nexthop, _snode->lchild, _sLevel + 1, _pLevel, _treeIdx);
					}
					else {

						ins(prefix, length, nexthop, _snode->rchild, _sLevel + 1, _pLevel, _treeIdx);
					}	
				}
				else { // prefix in current node must be equal to the one to be inserted

					// do nothing
				}
			}
			else { // insert into a node in a higher level

				if (0 == utility::getBitValue(_prefix, U + _pLevel * K + _sLevel)) {

					ins(_prefix, _length, _nexthop, _snode->lchild, _sLevel + 1, _pLevel, _treeIdx); 
				}
				else {

					ins(_prefix, _length, _nexthop, _snode->rchild, _sLevel + 1, _pLevel, _treeIdx);
				}
			}
		}

		return;
	}


	/// \brief Search LPM for the input IP address.
	/// 
	/// Check both the fast lookup table and the forest of the multi-prefix trees. A match in the latter has a higher priority than the one in the former.
	///
	uint32 search(const ip_type& _ip) {

		// try to find a match in the fast lookup table
		uint32 nexthop1 = 0;

		nexthop1 = ft->search(_ip);

		// try to find a match in the forest of the multi-prefix trees
		uint32 nexthop2 = 0;

		nexthop2 = 0;

		pnode_type* pnode = mRootTable[utility::getBitsValue(_ip, 0, U - 1)];	

		int pLevel = 0;

		int sBestlength; // record best match in auxiliary prefix trees	

		while (nullptr != pnode) {

			// if there exists a match in the primary node, then it must be the LPM
			for (size_t i = 0; i < pnode->t; ++i) {

				if (utility::getBitsValue(_ip, 0, pnode->prefixEntries[i].length - 1) == utility::getBitsValue(pnode->prefixEntries[i].prefix, 0, pnode->prefixEntries[i].length - 1)) {

					return pnode->prefixEntries[i].nexthop;
				}
			}
				
			// if there exists a match in the auxiliary tree, then records it.
			if (nullptr != pnode->sRoot) {

				int sLevel = 0;

				snode_type* snode = pnode->sRoot;

				while (nullptr != snode) {

					if (utility::getBitsValue(_ip, 0, snode->length - 1) == utility::getBitsValue(snode->prefix, 0, snode->length - 1)) {

						if (sBestLength < snode->length) {

							sBestLength = snode->length;

							nexthop2 = snode->nexthop;
						}					
					}

					if (0 == utility::getBitValue(_ip, U + K * pLevel + sLevel)) {

						snode = snode->lchild;
					}
					else {

						snode = snode->rchild;
					}

					++sLevel;
				}				
			}

			// search in higher levels
			pnode = pnode->childEntries[utility::getBitsValue(_ip, U + pLevel * K, U + (pLevel + 1) * K - 1)];

			++pLevel;
		}


		// find a match in an auxiliary tree
		if (0 != nexthop2) return nexthop2;

		// find a match in the fast lookup table
		if (0 != nexthop1) return nexthop1;

		// return default route (0)
		return 0;
	}


	/// \brief Delete a prefix from the index.
	///
	/// A short prefix (< U bits) is deleted from the fast lookup table if exists.
	/// A long prefix (>=U bits) is deleted from the forest of multi-prefix tree.
	void del(const ip_type& _prefix, const uint8& _length) {

		if (_length < U) {

			ft->del(_prefix, _length);	
		}
		else {

			del(_prefix, _length, mRootTable[utility::getBitsValue(_prefix, 0, U - 1)], 0, utility::getBitsValue(_prefix, 0, U - 1));
		}

	}

	/// \brief Delete a prefix in the index.
	///
	/// Given a prefix P to be deleted from the index and a primary node P at level L:
	/// (1) if len(P) < U + K * (L + 1), then try to find the prefix in the auxiliary tree. Delete it if exists.
	/// (2) otherwise, try to find the prefix in the current primary node. If current node contains prefix, then remove the prefix following one 
	/// of two cases:
	/// (2-1): if current node is an external node, then directly remove the prefix in the node.
	/// (2-2): if currentnode is an internal node, then remove the prefix in the node and fetch the longest prefix in the child nodes to fill up 
	/// current node again. Then recursively delete the prefix fetched from the child node. 
	void del(const ip_type& _prefix, const uint8& _length, PNode<W, K>*& _pnode, const int _level, uint32 _treeIdx) {

		if (nullptr == _pnode) return;

		if (_length < U + (_level + 1) * K) { // if exists, must be in the auxiliary prefix tree

			// delete
			del(_prefix, _length, _pnode->sRoot, 0, _level);				

			// adjust the prefix tree, if necessary
			if (nullptr == _pnode->sRoot && 0 == _pnode->t) {
		
			_pnode = nullptr;

				--mPNodeNum[_treeIdx];
			}
		}
		else {

			// try to find the prefix in current primary node
			int _pos = findPrefixInPNode(_pnode, _prefix, _length);

			if (_pos != _pnode->t + 1) { // found

				// delete
				deletePrefixInPNode(_pnode, _pos);

				// if current primary node is an internal node, then find the longest prefix in them and insert it into current primary node
				for (size_t i = 0; i < MC; ++i) {

					if (nullptr != _pnode->childEntries[i]) {

						hasChild = true;

						break;
					}
				}
	
				if (true == hasChild) { // internal node

					ip_type long_prefix = 0;

					uint8 long_length = 0;

					uint32 long_nexthop = 0;

					size_t childIdx = 0;

					// find longest prefix in child nodes
					findLongestPrefixInChild(_pnode, childIdx, long_prefix, long_length, long_nexthop);

					// insert the longest prefix into current primary node
					insertPrefixInPNode(_pnode, long_prefix, long_length, long_nexthop);

					// delete the longest prefix in the child node
					del(long_prefix, long_length, _pnode->childEntries[childIdx], _level + 1);
				}
				else { // external node

					// after delete, we must check if current pnode is empty (no prefix in the primary node and auxiliary tree)
					if (0 == _pnode->t && nullptr == _pnode->sRoot) {

						delete _pnode;
			
						_pnode = nullptr;

						--mPNodeNum[_treeIdx];
					}
				}
			}	
			else { // not found in current primary node

				del(_prefix, _length, _pnode->childEntries[utility::getBitsValue(_prefix, U + _level * K, U + (_level + 1) * K - 1)], _level + 1, _treeIdx);
			}
		}				

		return;
	}



	/// \brief Try to find a prefix in a primary node.
	///
	/// \return position of prefix in the node if any; otherwise, t + 1.
	int findPrefixInPNode(pnode_type* _pnode, const ip_type& _prefix, const uint8& _length){

		for (int i = 0; i < _pnode->t; ++i) {

			if (_length == _pnode->prefixEntries[i].length && _prefix == _pnode->prefixEntries[i].prefix) {

				return i;
			}
		}
		
		return _pnode->t + 1;
	}

	/// \brief delete a prefix in an auxiliary prefix tree
	void del(const ip_type& _prefix, const uint8& _length, snode_type*& _snode, const int _sLevel, const int _pLevel, const uint32 _treeIdx){

		if (nullptr == _snode) return;

		if (_length == _snode->length && _prefix == _snode->prefix) { // find the prefix

			if (nullptr == _snode->lchild && nullptr == _snode->rchild) { // external node, delete it

				delete _snode;

				_snode = nullptr;

				--mSNodeNum[_sLevel + K * _pLevel];

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

				--mSNodeNum[_sLevel + K * _pLevel];

				return;
			}	
		}
		else { // try to find in a higher level

			if (0 == utility::getBitValue(_prefix, U + _pLevel * K + _sLevel)) {

				del(_prefix, _length, _snode->lchild, _sLevel + 1, _pLevel, _treeIdx);
			}
			else {

				del(_prefix, _length, _snode->rchild, _sLevel + 1, _pLevel, _treeIdx);
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

	/// \brief print a primary node and 	
	void printPNode(const pnode_type* _pnode) const {

		for (size_t i = 0; i < MP; ++i) {

			std::cerr << "prefix: " << _pnode->prefixEntries[i].prefix << " lengh: " << (uint32)_pnode->prefixEntries[i].length << std::endl;
		}

		std::cerr << std::endl;
	}	


	/// \brief Report collected information.
	///
	void report(){

		for (size_t i = 0; i < V; ++i) {

			std::cerr << "tree " << i << ": \n"'
		
			std::cerr << "primary node num: " << mPNodeNum[i] << std::endl;

			std::cerr << "secondary node num: " << mSNodeNum[i] << std::endl;
		}
	}

	
	/// \brief Scatter in a random pipeline.
	///
	/// Use two different random seed to generate random number for primary
	/// and secondary nodes, respectively.
	void ran(int _stagenum) {

		size_t* pnodeNumInStage = new size_t[_stagenum];

		size_t* snodeNumInStage = new size_t[_stagenum];

		for (int i = 0; i < _stagenum; ++i) {

			pnodeNumInStage[i] = 0;

			snodeNumInstage[i] = 0;
		}

		// random generator for pnode
		unsigned seed_pnode = std::chrono::system_clock::now().time_since_epoch().count();

		std::default_random_engine generator_pnode(seed_pnode);

		std::uniform_int_distribution<int> distribution_pnode(0, _stagenum - 1);

		auto roll_pnode = std::bind(distribution_pnode, generator_pnode);

		// random generator for snode
		unsigned seed_snode = std::system_clock::now().time_since_epoch().count() + 1000000;

		std::default_random_engine generator_snode(seed_snode);

		std::uniform_int_distribution<int> distribution_snode(0, _stagenum - 1);

		auto roll_snode = std::bind(distribution_snode, generator_snode);

		// start numbering nodes
		for (size_t i = 0; i < V; ++i) {

			if (nullptr != mRootTable[i]) {

				std::queue<pnode_type*> pqueue;

				mRootTable[i]->stageidx = 0;

				pnodeNumInStage[0]++;

				pqueue.push(mRootTable[i]);
			
				while (!pqueue.empty()) {

					auto pfront = pqueue.front();

					// auxiliary tree
					if (nullptr != pfront->sRoot) {

						std::queue,snode_type*> squeue;

						pfront->sRoot->stageidx = pfront->stageidx; // root of auxiliary tree and the primary node at same level
				
						squeue.push(pfront->sRoot);

						while (!squeue.empty()) {

							auto sfront = squeue.top();
							
							if (nullptr != sfront->lchild) {

								sfront->lchild->stageidx = sfront->stageidx + 1;

								squueue.push(sfront->lchild);
							}

							if (nullptr != sfront->rchild) {

								sfront->rchild->stageidx = sfront->stageidx + 1;

								squeue.push(sfront->rchild);
							}

							squeue.pop();
						}
					}
					
					// child nodes
					for (size_t j = 0; j < MC; ++j) {

						if(nullptr != pfront->childEntries[j]) {

							pfront->childEntries[j]->stageidx = pfront->stageidx + K;

							pqueue.push(pfront->childEntries[j]);
						}
					}

					pqueue.pop();
				}	
			}
		}	

		// report
		size_t pnodeNumInAllStages = 0;

		size_t snodeNumInAllStages = 0;

		for (size_t i = 0; i < _stagenum; ++i) {

			pnodeNumInAllStages[i] += pnodeNumInStages[i];

			snodeNumInAllStages[i] += snodeNumInStages[i];

			std::cerr << "pnodenum in stages " << i << ": " << pnodenumInStages[i] << std::endl;

			std::cerr << "snodenum in stages " << i << ": " << snodenumInStages[i] << std::endl;
		}

		std::cerr << "pnode num in all stages: " << pnodeNumInAllStages << std::endl;

		std::cerr << "snode num in all stages: " << snodeNumInAllStages << std::endl;


		delete pnodeNumInStages[i];

		delete snodeNumInStages[i];

		return;
	}

	
	/// \brief element to be sorted
	struct SortElem{

		size_t nodeNum; ///< number of nodes in the tree

		size_t treeIdx; ///< index of tree

		SortElem(const size_t _nodeNum, const size_t _treeIdx) : nodeNum(_nodeNum), treeIdx(_treeIdx) {}

		bool operator< (const SortElem& _a) const {

			if (nodeNum == _a.nodeNum) {

				return (treeIdx < _a.treeIdx);
			}
			else {

				return (nodeNum < _a.nodeNum);
			}
		}		
	};
};


#endif // RMPT_H
