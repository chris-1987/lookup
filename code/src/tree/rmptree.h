#ifndef _RMPT_H
#define _RMPT_H

///////////////////////////////////////////////////////////////////////////////////////////////
/// Copyright (c) 2016, Sun Yat-sen University
/// All rights reserved.
/// \file rmptree.h
/// \brief Definition of an IP lookup index based on multi-prefix tree.
///
/// The lookup index consists of three parts: a fast lookup table that contains route information for prefixes shorter than U bits,
/// a table that consists of 2^U entries and each entry stores a pointer to the root node of a multi-prefix tree, 
/// and a set of multi-prefix trees.
///
/// \author Yi Wu
/// \date 2016.11
///////////////////////////////////////////////////////////////////////////////////////////////


#include "../common/common.h"
#include "../common/utility.h"

#include "fasttable.h"

#include <queue>
#include <cmath>
#include <chrono>
#include <random>
#include <algorithm>

#define DEBUG_RMPT


/// \brief Secondary node in a multi-prefix tree.
/// 
/// Nodes in a multi-prefix tree are divided into two categories: primary node and secondary node.
/// A secondary node is a 5-tuple consisting of <prefix, length of prefix, nexthop, pointer to left child, pointer to right child>
///
/// \param W 32, or 128 for IPv4 or IPv6, respectively. 
template<int W>
struct SNode{

	typedef typename choose_ip_type<W>::ip_type ip_type;

	static const size_t size; ///< size of a secondary node

	ip_type prefix; ///< prefix
	
	uint8 length; ///< length of prefix

	uint32 nexthop; ///< corresponding nexthop

	SNode* lchild; ///< pointer to left child

	SNode* rchild; ///< pointer to right child

	int stageidx; ///< pipe stage number, not required, only for test

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
/// \param MP number of prefixes in a primary node, which is equal to  2 * K + 1. Do not change it.
/// \param MC number of child pointers in a primary node, which is equal to pow(2, K). Do not change it.
template<int W, int K, size_t MP = 2 * K + 1, size_t MC = static_cast<size_t>(pow(2, K))>
struct PNode{

	typedef typename choose_ip_type<W>::ip_type ip_type;	
	
	typedef PNode<W, K> pnode_type;

	typedef SNode<W> snode_type; 

	static const size_t size; ///< size of a primary node

	uint8 t; ///< number of prefixes currently stored in the primary node, at most 2 * K + 1

	int stageidx; ///< pipe stage number

	/// \brief Prefix entry.
	///
	/// A primary node consists of multiple prefix esntries, each entry records a prefix, the length of the prefix and its nexthop.
	/// A primary node also contains multiple child pointers.
	struct PrefixEntry{
		
		ip_type prefix;
			
		uint8 length;

		uint32 nexthop;	

		PrefixEntry() : prefix(0), length(0), nexthop(0) {}
	};
	
	PrefixEntry prefixEntries[MP]; ///< store at most MP prefix entries

	pnode_type* childEntries[MC]; ///< store at most MC childs

	snode_type* sRoot; ///< pointer to the root of auxiliary prefix tree.

	PNode() : t(0), stageidx(0), sRoot(nullptr) {

		for (size_t i = 0; i < MC; ++i) {

			childEntries[i] = nullptr;
		}
	}

	~PNode() {
		
	}
};

template<int W, int K, size_t MP, size_t MC>
const size_t PNode<W, K, MP, MC>::size = sizeof(uint8) + sizeof(PrefixEntry) * MP + sizeof(pnode_type*) * MC + sizeof(snode_type*); // exclude stageidx


/// \brief Build and update the index.
///
/// Prefixes shorter than U bits are stored in a fast lookup table.
/// Prefixes not shorter than U bits are stored in the PT forest.
/// \param W 32 or 128 for IPv4 or IPv6, respectively. 
/// \param K Stride of MPT.
/// \param MP Max number of prefixes tored in a primary node (2 * K + 1).
/// \param MC Max number of child pointers stored in a primary node (pow(2, K).
/// \param U Threshold for classifying shorter and longer prefixes.
/// \param V Number of prefix trees at large.
/// \param H1 Max level of a primary node.
/// \param H2 Max level of a secondary node.
/// \note The maximum height of MPT is H1 = (W - U + 1) / K. The maximum height of PT is H2 = H1 + K, where the root node of PT is pointed to by a primary node located at the highest level of MPT.
template<int W, int K, int U, size_t MP = 2 * K + 1, size_t MC = static_cast<size_t>(pow(2, K)), size_t V = static_cast<size_t>(pow(2, U)), int H1 = (W - U + 1) / K + (((W - U + 1) % K != 0) ? 1 : 0), int H2 = H1 + K>
class RMPTree {
private:

	typedef typename choose_ip_type<W>::ip_type ip_type;

	typedef PNode<W, K> pnode_type; ///< primary node type

	typedef SNode<W> snode_type; ///< secondary node type

	pnode_type* mRootTable[V]; ///< pointers to a forest of multi-prefix tree

	uint32 mLocalLevelPNodeNum[V][H1]; ///< number of pnodes at each level in all MPT

	uint32 mLocalLevelSNodeNum[V][H2]; ///< number of snodes at each level in all MPT

	uint32 mLocalPNodeNum[V]; ///< number of pnodes in each MPT

	uint32 mLocalSNodeNum[V]; ///< number of snodes in each MPT
	
	uint32 mGlobalLevelPNodeNum[H1]; ///< number of pnodes at each level
	
	uint32 mGlobalLevelSNodeNum[H2]; ///< number of snodes at each level

	uint32 mTotalPNodeNum; ///< number of pnodes

	uint32 mTotalSNodeNum; ///< number of snodes 

	FastTable<W, U - 1> *ft; ///< pointer to the fast lookup table

public:
	
	/// \brief default ctor
	RMPTree() {

		initializeParameters();
	}
	
	/// \brief initialize parameters
	void initializeParameters() {

		for (size_t i = 0; i < V; ++i) {

			mRootTable[i] = nullptr;
		}

		for (size_t i = 0; i < V; ++i) {

			for (size_t j = 0; j < H1; ++j) {

				mLocalLevelPNodeNum[i][j] = 0;
			}
		}

		for (size_t i = 0; i < V; ++i) {

			for (size_t j = 0; j < H2; ++j) {

				mLocalLevelSNodeNum[i][j] = 0;
			}
		}

		for (size_t i = 0; i < V; ++i) {

			mLocalPNodeNum[i] = 0;
		}

		for (size_t i = 0; i < V; ++i) {

			mLocalSNodeNum[i] = 0;
		}

		for (size_t i = 0; i < H1; ++i) {

			mGlobalLevelPNodeNum[i] = 0;
		}	

		for (size_t i = 0; i < H2; ++i) {

			mGlobalLevelSNodeNum[i] = 0;
		}
	
		mTotalPNodeNum = 0;

		mTotalSNodeNum = 0;

		ft = new FastTable<W, U - 1>();

		return;
	}

	/// \brief disable copy-ctor
	RMPTree(const RMPTree& _mpt) = delete;

	/// \brief disable assignment op
	RMPTree& operator= (const RMPTree&) = delete;


	/// \brief dtor
	~RMPTree() {

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
		
	/// \brief Report the collected information
	void report() {

		mTotalPNodeNum = 0;

		mTotalSNodeNum = 0;
	
		for (size_t i = 0; i < V; ++i) {

			mTotalPNodeNum += mLocalPNodeNum[i];

			mTotalSNodeNum += mLocalSNodeNum[i]; 
		}

		std::cerr << "pnode num in total: " << mTotalPNodeNum << std::endl;

		std::cerr << "snode num in total: " << mTotalSNodeNum << std::endl;

		return;
	}

	/// \brief Insert a prefix into the index. 
	void ins(const ip_type& _prefix, const uint8& _length, const uint32& _nexthop) {

		if (_length < U) { // insert into the fast table

			ft->ins(_prefix, _length, _nexthop);
		}
		else { // insert into the MPT forest

			ins(_prefix, _length, _nexthop, mRootTable[utility::getBitsValue(_prefix, 0, U - 1)], 0, utility::getBitsValue(_prefix, 0, U - 1));
		}
		
		return;
	} 


	/// \brief Insert a prefix into the MPT forest.
	void ins(const ip_type& _prefix, const uint8& _length, const uint32& _nexthop, pnode_type*& _pnode, const int _level, const uint32 _treeIdx) {

		if (nullptr == _pnode) { // node is empty, create the node

			_pnode = new pnode_type();
	
			++mLocalPNodeNum[_treeIdx]; // primary nodes in current MPT

			++mLocalLevelPNodeNum[_treeIdx][_level]; // primary nodes at current level of current MPT
		}

		if (_length < U + (_level + 1) * K) { // [U, U + (_level + 1) * K - 1], in this level

			// current prefix must be inserted into the auxiliary prefix tree
			ins(_prefix, _length, _nexthop, _pnode->sRoot, 0, _level, _treeIdx); // sLevel in current auxiliary tree starts from 0
		}
		else { // insert the prefix into current primary node or a node in a higher level

			if (_pnode->t < MP) { // current primary node is not full, insert prefix into current primary node

				insertPrefixInPNode(_pnode, _prefix,_length, _nexthop);
			}
			else {
					
				// if the shortest prefix in current primary node is also shorter than the prefix to be inserted
				if (_pnode->prefixEntries[MP - 1].length < _length) { 

					// cache the shortest prefix in current primary node
					ip_type prefix = _pnode->prefixEntries[MP - 1].prefix;

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

			mLocalSNodeNum[_treeIdx]++;	

			mLocalLevelSNodeNum[_treeIdx][_pLevel + 1 + _sLevel]++; // be careful, required to plus 1 

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
					ip_type prefix = _snode->prefix;

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
	uint32 search(const ip_type& _ip, std::vector<int>& _trace) {

		// try to find a match in the fast lookup table
		uint32 nexthop1 = 0;

		nexthop1 = ft->search(_ip);

		// try to find a match in the forest of the multi-prefix trees
		uint32 nexthop2 = 0;

		nexthop2 = 0;

		pnode_type* pnode = mRootTable[utility::getBitsValue(_ip, 0, U - 1)];	

		int pLevel = 0;

		int sBestLength; // record best match in auxiliary prefix trees	

		while (nullptr != pnode) {

			_trace.push_back(pnode->stageidx);

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

					_trace.push_back(snode->stageidx);

					if (utility::getBitsValue(_ip, 0, snode->length - 1) == 
						utility::getBitsValue(snode->prefix, 0, snode->length - 1)) {

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


	/// \brief generate lookup trace for simulation
	void generateTrace (const std::string& _reqFile, const std::string& _traceFile){

		std::ifstream reqFin(_reqFile, std::ios_base::binary);

		std::string line;

		ip_type prefix;

		std::ofstream traFin(_traceFile, std::ios_base::binary);	
		
		size_t searchNum = 0;

		double avgSearchDepth = 0;

		while (getline(reqFin, line)) {

			std::vector<int> trace;

			std::stringstream ss(line);

			ss >> prefix;
			
			// generate trace while performing the lookup request
			search(prefix, trace);

			// collect search depth
			++searchNum;

			avgSearchDepth += trace.size();
		 
			// output trace to file	
			traFin << static_cast<size_t>(trace.size());

			traFin << " ";
	
			// record stage list
			for (int i = 0; i < trace.size(); ++i) {
			
				traFin << static_cast<size_t>(trace[i]);		

				traFin << " ";
			}
	
			//
			traFin << "\n";
		}
	
		avgSearchDepth /= searchNum; 

		std::cerr << "average search depth: " << avgSearchDepth << std::endl;		

		return;	
	}

	/// \brief Delete a prefix from the index.
	///
	/// A short prefix (< U bits) is deleted from the fast lookup table if exists.
	/// A long prefix (>=U bits) is deleted from the forest of multi-prefix tree.
	void del(const ip_type& _prefix, const uint8& _length) {

		if (_length < U) { // delete a short prefix in the fast lookup table

			ft->del(_prefix, _length);	
		}
		else { // delete a long prefix in the MPT forest

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

		if (_length < U + (_level + 1) * K) { // in the auxiliary PT

			// delete
			del(_prefix, _length, _pnode->sRoot, 0, _level, _treeIdx);				

			// adjust the prefix tree, if necessary
			if (nullptr == _pnode->sRoot && 0 == _pnode->t) { // empty external primary node
		
				delete _pnode;

				_pnode = nullptr;

				--mLocalPNodeNum[_treeIdx];

				--mLocalLevelPNodeNum[_treeIdx][_level];
			}
		}
		else {

			// try to find the prefix in current primary node
			int _pos = findPrefixInPNode(_pnode, _prefix, _length);

			if (_pos != _pnode->t + 1) { // found

				// delete
				deletePrefixInPNode(_pnode, _pos);

				// if current primary node is an internal node, then find the longest prefix in them and insert it into current primary node
				bool hasChild = false;

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
					del(long_prefix, long_length, _pnode->childEntries[childIdx], _level + 1, _treeIdx);
				}
				else { // external node

					// after delete, we must check if current pnode is empty (no prefix in the primary node and auxiliary tree)
					if (0 == _pnode->t && nullptr == _pnode->sRoot) {

						delete _pnode;
			
						_pnode = nullptr;

						--mLocalPNodeNum[_treeIdx];

						--mLocalLevelPNodeNum[_treeIdx][_level];
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

			if (_length == _pnode->prefixEntries[i].length && _pnode->prefixEntries[i].prefix == _prefix) {

				return i;
			}
		}
		
		return _pnode->t + 1;
	}


	/// \brief delete a prefix in an auxiliary prefix tree
	void del(const ip_type& _prefix, const uint8& _length, snode_type*& _snode, const int _sLevel, const int _pLevel, const uint32 _treeIdx){

		if (nullptr == _snode) return;

		if (_length == _snode->length && _snode->prefix == _prefix) { // find the prefix

			if (nullptr == _snode->lchild && nullptr == _snode->rchild) { // external node, directly delete it

				delete _snode;

				_snode = nullptr;

				--mLocalSNodeNum[_treeIdx];

				--mLocalLevelSNodeNum[_treeIdx][_pLevel + 1 + _sLevel]; // plus 1 is required

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

				int child_sLevel = _sLevel + 1; // level of child node

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

					++child_sLevel;
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

				--mLocalSNodeNum[_treeIdx];

				--mLocalLevelSNodeNum[_treeIdx][_pLevel + 1 + child_sLevel];

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

	/// \brief print a primary node 	
	void printPNode(const pnode_type* _pnode) const {

		for (size_t i = 0; i < MP; ++i) {

			std::cerr << "prefix: " << _pnode->prefixEntries[i].prefix << " lengh: " << (uint32)_pnode->prefixEntries[i].length << std::endl;
		}

		std::cerr << std::endl;
	}	


	/// \brief Scatter 
	///
	/// The structure of an MPT is different from other prefix trees.
	/// There's no efficient mapping method to distribute nodes in a MPT
	/// into a linear/cirular pipeline without no-ops
	/// Here, we only consider random pipeline.
	void scatterToPipeline(int _pipestyle, int _stagenum = H2) { // H2 > H1

		switch(_pipestyle) {

	//	case 0: lin(_stagenum); break;

		case 1: ran(_stagenum); break;

	//	case 2: cir(_stagenum); break;
		}

		return;
	}

	/// \brief Scatter nodes in a linear pipe line.
	/// \note abandon
	void lin(int _stagenum) {

		size_t* memUseInStage = new size_t[_stagenum];

		size_t* testGlobalPNodeNum = new size_t[_stagenum];

		size_t* testGlobalSNodeNum = new size_t[_stagenum];

		for (int i = 0; i < _stagenum; ++i) {

			memUseInStage[i] = 0;

			testGlobalPNodeNum[i] = 0;

			testGlobalSNodeNum[i] = 0;
		}
		
		// start numbering
		for (size_t i = 0; i < V; ++i) {

			if (nullptr != mRootTable[i]) { // current MPT is not empty

				mRootTable[i]->stageidx = 0; // put pRoot in the initial stage

				memUseInStage[0] += PNode<W, K>::size;
		
				testGlobalPNodeNum[0]++;	

				std::queue<pnode_type*> pqueue;

				pqueue.push(mRootTable[i]);

				while (!pqueue.empty()) {

					auto pfront = pqueue.front();

					if (nullptr != pfront->sRoot) { // auxiliary PT of current primary node is not empty

						pfront->sRoot->stageidx = (pfront->stageidx + 1) % _stagenum; // put sRoot into the next stage of pRoot

						memUseInStage[pfront->sRoot->stageidx] += SNode<W>::size;

						testGlobalSNodeNum[pfront->sRoot->stageidx]++;

						std::queue<snode_type*> squeue;

						squeue.push(pfront->sRoot);

						while (!squeue.empty()) {

							auto sfront = squeue.front();

							if (nullptr != sfront->lchild) {

								sfront->lchild->stageidx = (sfront->stageidx + 1) % _stagenum; // sequentially put nodes

								memUseInStage[sfront->lchild->stageidx] += SNode<W>::size;

								testGlobalSNodeNum[sfront->lchild->stageidx]++;

								squeue.push(sfront->lchild);
							}

							if (nullptr != sfront->rchild) {

								sfront->rchild->stageidx = (sfront->stageidx + 1) % _stagenum;

								memUseInStage[sfront->rchild->stageidx] += SNode<W>::size;

								testGlobalSNodeNum[sfront->rchild->stageidx]++;

								squeue.push(sfront->rchild);
							}

							squeue.pop();
						}
					}

					// put child nodes of current primary node
					for (size_t j = 0; j < MC; ++j) {

						if (nullptr != pfront->childEntries[j]) {

							pfront->childEntries[j]->stageidx = (pfront->stageidx + 1) % _stagenum; // sequentially put nodes

							memUseInStage[pfront->childEntries[j]->stageidx] += PNode<W, K>::size;

							testGlobalPNodeNum[pfront->childEntries[j]->stageidx]++;

							pqueue.push(pfront->childEntries[j]);
						}
					}

					pqueue.pop();
				}
			}
		}

		// output information
		size_t pnodeNumInAllStages = 0;

		size_t snodeNumInAllStages = 0;

		std::cerr << "mem use in each stage: \n";

		for (size_t i = 0; i < _stagenum; ++i) {

			std::cerr << "stage " << i << ": " << memUseInStage[i] << std::endl;;
		}

		std::cerr << "\nGlobal pnode num in each stage: \n";

		for (size_t i = 0; i < _stagenum; ++i) {

			pnodeNumInAllStages += testGlobalPNodeNum[i];

			std::cerr << "stage " << i << ": " << testGlobalPNodeNum[i] << std::endl;
		}
	
		std::cerr << "\nGlobal snode num in each stage: \n";

		for (size_t i = 0; i < _stagenum; ++i) {

			snodeNumInAllStages += testGlobalSNodeNum[i];

			std::cerr << "stage " << i << ": " << testGlobalSNodeNum[i] << std::endl;
		}

		std::cerr << "pnode number in all stages: " << pnodeNumInAllStages << std::endl;
		std::cerr << "snode number in all stages: " << snodeNumInAllStages << std::endl;

		delete[] memUseInStage;

		delete[] testGlobalPNodeNum;

		delete[] testGlobalSNodeNum;
	}


	/// \brief Scatter nodes in a random pipe line.
	///
	/// Use different generator to produce random number for distributing primary nodes and 
	/// secondary nodes.
	void ran(int _stagenum) {

		size_t* memUseInStage = new size_t[_stagenum];

		size_t* testGlobalPNodeNum = new size_t[_stagenum];

		size_t* testGlobalSNodeNum = new size_t[_stagenum];

		for (int i = 0; i < _stagenum; ++i) {

			memUseInStage[i] = 0;

			testGlobalPNodeNum[i] = 0;	

			testGlobalSNodeNum[i] = 0;
		}

		// random generator for pnode
		unsigned seed_pnode = std::chrono::system_clock::now().time_since_epoch().count();

		std::default_random_engine generator_pnode(seed_pnode);

		std::uniform_int_distribution<int> distribution_pnode(0, _stagenum - 1);

		auto roll_pnode = std::bind(distribution_pnode, generator_pnode);

		// random generator for snode
		unsigned seed_snode = std::chrono::system_clock::now().time_since_epoch().count() + 1000000;

		std::default_random_engine generator_snode(seed_snode);

		std::uniform_int_distribution<int> distribution_snode(0, _stagenum - 1);

		auto roll_snode = std::bind(distribution_snode, generator_snode);

		// start numbering nodes
		for (size_t i = 0; i < V; ++i) {

			if (nullptr != mRootTable[i]) { // pRoot is not null

				mRootTable[i]->stageidx = roll_pnode(); // roll

				memUseInStage[mRootTable[i]->stageidx] += PNode<W, K>::size;
				
				testGlobalPNodeNum[mRootTable[i]->stageidx]++;	

				std::queue<pnode_type*> pqueue;

				pqueue.push(mRootTable[i]);
			
				while (!pqueue.empty()) {

					auto pfront = pqueue.front();

					// auxiliary tree
					if (nullptr != pfront->sRoot) {

						pfront->sRoot->stageidx = roll_snode(); // roll  

						memUseInStage[pfront->sRoot->stageidx] += SNode<W>::size;

						testGlobalSNodeNum[pfront->sRoot->stageidx]++;

						std::queue<snode_type*> squeue;
	
						squeue.push(pfront->sRoot);

						while (!squeue.empty()) {

							auto sfront = squeue.front();
							
							if (nullptr != sfront->lchild) {

								sfront->lchild->stageidx = roll_snode(); // roll

								memUseInStage[sfront->lchild->stageidx] += SNode<W>::size;

								testGlobalSNodeNum[sfront->lchild->stageidx]++;

								squeue.push(sfront->lchild);
							}

							if (nullptr != sfront->rchild) {

								sfront->rchild->stageidx = roll_snode(); // roll

								memUseInStage[sfront->rchild->stageidx] += SNode<W>::size;

								testGlobalSNodeNum[sfront->rchild->stageidx]++;

								squeue.push(sfront->rchild);
							}

							squeue.pop();
						}
					}
					
					// child nodes
					for (size_t j = 0; j < MC; ++j) {

						if(nullptr != pfront->childEntries[j]) {

							pfront->childEntries[j]->stageidx = roll_pnode(); // roll

							memUseInStage[pfront->childEntries[j]->stageidx] += PNode<W, K>::size;

							testGlobalPNodeNum[pfront->childEntries[j]->stageidx]++;

							pqueue.push(pfront->childEntries[j]);
						}
					}

					pqueue.pop();
				}	
			}
		}	

		// output information
		size_t pnodeNumInAllStages = 0;

		size_t snodeNumInAllStages = 0;

		std::cerr << "mem use in each stage: \n";

		for (size_t i = 0; i < _stagenum; ++i) {

			std::cerr << "stage " << i << ": " << memUseInStage[i] << std::endl;;
		}

		std::cerr << "\nGlobal pnode num in each stage: \n";

		for (size_t i = 0; i < _stagenum; ++i) {

			pnodeNumInAllStages += testGlobalPNodeNum[i];

			std::cerr << "stage " << i << ": " << testGlobalPNodeNum[i] << std::endl;
		}
	
		std::cerr << "\nGlobal snode num in each stage: \n";

		for (size_t i = 0; i < _stagenum; ++i) {

			snodeNumInAllStages += testGlobalSNodeNum[i];

			std::cerr << "stage " << i << ": " << testGlobalSNodeNum[i] << std::endl;
		}

		std::cerr << "pnode number in all stages: " << pnodeNumInAllStages << std::endl;
		std::cerr << "snode number in all stages: " << snodeNumInAllStages << std::endl;

		delete[] memUseInStage;

		delete[] testGlobalPNodeNum;

		delete[] testGlobalSNodeNum;

		return;
	}

	
	/// \brief element to be sorted
	/// 
	/// This strucutre is used along with cir() function to map nodes of 
	// an MPT into a circular pipeline.
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


	/// \brief Scatter in a circular pipeline
	/// \note abandon
	void cir(int _stagenum) {

		size_t* memUseInStage = new size_t[_stagenum];

		size_t* testGlobalPNodeNum = new size_t[_stagenum];

		size_t* testGlobalSNodeNum = new size_t[_stagenum];

		for (int i = 0; i < _stagenum; ++i) {

			memUseInStage[i] = 0;

			testGlobalPNodeNum[i] = 0;	

			testGlobalSNodeNum[i] = 0;
		}

		// step 1: sort binary tries by their size in no-decreasing order
		// size of an MPT is calculated by accumulating the memory comsumption for storing all primary & secondary nodes
		std::vector<SortElem> vec;

		for (size_t i = 0; i < V; ++i) {

			if (nullptr != mRootTable[i]) {

				size_t treeSize = mLocalPNodeNum[i] * PNode<W, K>::size + mLocalSNodeNum[i] * SNode<W>::size;

				vec.push_back(SortElem(treeSize, i));
			}
		}

		std::sort(std::begin(vec), std::end(vec));

		// step 2: scatter nodes 
		size_t* colored = new size_t[_stagenum];

		for (int i = 0; i < _stagenum; ++i) colored[i] = 0;

		size_t* trycolor = new size_t[_stagenum];

		for (int i = 0; i < _stagenum; ++i)  trycolor[i] = 0;

		int bestStartIdx = 0; // the most proper stage that accommodate the root node

		for (int i = vec.size() - 1; i >= 0; --i) {

			size_t treeIdx = vec[i].treeIdx;

			double bestVar = std::numeric_limits<double>::max();

			for (size_t j = 0; j < _stagenum; ++j) {

				// reset
				for (size_t k = 0; k < _stagenum; ++k) {

					trycolor[k] = colored[k];
				}

				// add primary nodes
				for (size_t k = 0; k < H1; ++k) {

					trycolor[(j + k) % _stagenum] += mLocalLevelPNodeNum[treeIdx][k] * PNode<W, K>::size;
				}

				// add auxiliary nodes
				for (size_t k = 0; k < H2; ++k) {

					trycolor[(j + k) % _stagenum] += mLocalLevelSNodeNum[treeIdx][k] * SNode<W>::size;
				}

				// compute variance
				double sum = std::accumulate(trycolor, trycolor + _stagenum, 0.0);

				double mean = sum / _stagenum;

				double accum = 0.0;

				for (int k = 0; k < _stagenum; ++k) {

					accum += (trycolor[k] - mean) * (trycolor[k] - mean);
				}
				
				double var = accum / _stagenum;

				if (var < bestVar) {
					
					bestVar = var;

					bestStartIdx = j;
				}
			}	

			// add primary nodes
			for (int j = 0; j < H1; ++j) {

				trycolor[(bestStartIdx + j) % _stagenum] += mLocalLevelPNodeNum[treeIdx][j] * PNode<W, K>::size;
			}

			// add secondary nodes
			for (int j = 0; j < H2; ++j) {

				trycolor[(bestStartIdx + j) % _stagenum] += mLocalLevelSNodeNum[treeIdx][j] * SNode<W>::size;
			}

			// color nodes in current tree
			mRootTable[treeIdx]->stageidx = bestStartIdx;

			memUseInStage[mRootTable[treeIdx]->stageidx] += PNode<W, K>::size;

			testGlobalPNodeNum[mRootTable[treeIdx]->stageidx]++;

			std::queue<pnode_type*> pqueue;

			pqueue.push(mRootTable[treeIdx]);

			while (!pqueue.empty()) {

				auto pfront = pqueue.front();
			
				if (nullptr != pfront->sRoot) {

					pfront->sRoot->stageidx = (pfront->stageidx + 1) % _stagenum; // stage of sRoot is next to that of pRoot

					memUseInStage[pfront->sRoot->stageidx] += SNode<W>::size;

					testGlobalSNodeNum[pfront->sRoot->stageidx]++;
	
					std::queue<snode_type*> squeue;

					squeue.push(pfront->sRoot);

					while (!squeue.empty()) {

						auto sfront = squeue.front();

						if (nullptr != sfront->lchild) {

							sfront->lchild->stageidx = (sfront->stageidx + 1) % _stagenum; // in sequence

							memUseInStage[sfront->lchild->stageidx] += SNode<W>::size;

							testGlobalSNodeNum[sfront->lchild->stageidx]++;

							squeue.push(sfront->lchild);
						}

						if (nullptr != sfront->rchild) {

							sfront->rchild->stageidx = (sfront->stageidx + 1) % _stagenum; // in sequence

							memUseInStage[sfront->rchild->stageidx] += SNode<W>::size;

							testGlobalSNodeNum[sfront->rchild->stageidx]++;

							squeue.push(sfront->rchild);
						}

						squeue.pop();
					}
				}	

				// color childs
				for (size_t j = 0; j < MC; ++j) {

					if (nullptr != pfront->childEntries[j]) {

						pfront->childEntries[j]->stageidx = (pfront->stageidx + 1) % _stagenum;

						memUseInStage[pfront->childEntries[j]->stageidx] += PNode<W, K>::size;

						testGlobalPNodeNum[pfront->childEntries[j]->stageidx]++;

						pqueue.push(pfront->childEntries[j]);
					}
				}	

				pqueue.pop();
			}
		}

		// output information
		size_t pnodeNumInAllStages = 0;

		size_t snodeNumInAllStages = 0;


		std::cerr << "mem use in each stage: \n";

		for (size_t i = 0; i < _stagenum; ++i) {

			std::cerr << "stage " << i << ": " << memUseInStage[i] << std::endl;;
		}

		std::cerr << "\nGlobal pnode num in each stage: \n";

		for (size_t i = 0; i < _stagenum; ++i) {

			pnodeNumInAllStages += testGlobalPNodeNum[i];

			std::cerr << "stage " << i << ": " << testGlobalPNodeNum[i] << std::endl;
		}
	
		std::cerr << "\nGlobal snode num in each stage: \n";

		for (size_t i = 0; i < _stagenum; ++i) {

			snodeNumInAllStages += testGlobalSNodeNum[i];

			std::cerr << "stage " << i << ": " << testGlobalSNodeNum[i] << std::endl;
		}

		std::cerr << "pnode number in all stages: " << pnodeNumInAllStages << std::endl;

		std::cerr << "snode number in all stages: " << snodeNumInAllStages << std::endl;

		delete[] memUseInStage;

		delete[] testGlobalPNodeNum;

		delete[] testGlobalSNodeNum;

		delete[] colored;

		delete[] trycolor;

		return;
	}

	/// \brief Update the index 
	///
	/// After executing build(), we ee
	void update(const std::string & _fn, int _stagenum = W - U + 1) {

		size_t withdrawnum = 0;

		size_t announcenum = 0;

		std::ifstream fin(_fn, std::ios_base::binary);

		std::string line;

		ip_type prefix;

		uint8 length;

		uint32 nexthop;

		bool isAnnounce;

		// random generator for snode
		unsigned seed_p = std::chrono::system_clock::now().time_since_epoch().count();

		std::default_random_engine generator_p(seed_p);

		std::uniform_int_distribution<int> distribution_p(0, _stagenum - 1);	

		// random generator for snode
		unsigned seed_s = std::chrono::system_clock::now().time_since_epoch().count() + 10000000;

		std::default_random_engine generator_s(seed_s);

		std::uniform_int_distribution<int> distribution_s(0, _stagenum - 1);


		// retrieve
		while (getline(fin, line)) {

			// retrieve prefix, length and withdraw/announce
			utility::retrieveInfo(line, prefix, length, isAnnounce);

			if (false == isAnnounce) {
	
				++withdrawnum;

				del(prefix, length);
			}
			else {

				++announcenum;

				nexthop = length;

				ins(prefix, length, nexthop, generator_p, distribution_p, generator_s, distribution_s);
			}
		}		

		reportNodeNumInStage(_stagenum);

		std::cerr << "withdraw num: " << withdrawnum << " announce num: " << announcenum << std::endl;

		return;
	}


	/// \brief for update, insert into index 
	void ins(const ip_type& _prefix, const uint8& _length, const uint32& _nexthop, std::default_random_engine& _generator_p, std::uniform_int_distribution<int>& _distribution_p, std::default_random_engine& _generator_s, std::uniform_int_distribution<int>& _distribution_s) {

		if (_length < U) { // insert into the fast table

			ft->ins(_prefix, _length, _nexthop);
		}
		else { // insert into the MPT forest

			ins(_prefix, _length, _nexthop, mRootTable[utility::getBitsValue(_prefix, 0, U - 1)], 0, utility::getBitsValue(_prefix, 0, U - 1), _generator_p, _distribution_p, _generator_s, _distribution_s);
		}
		
		return;
	} 

	/// \brief for update, insert into MPT forest
	void ins(const ip_type& _prefix, const uint8& _length, const uint32& _nexthop, pnode_type*& _pnode, const int _level, const uint32 _treeIdx, std::default_random_engine& _generator_p, std::uniform_int_distribution<int>& _distribution_p, std::default_random_engine& _generator_s, std::uniform_int_distribution<int>& _distribution_s) {

		if (nullptr == _pnode) { // node is empty, create the node

			_pnode = new pnode_type();

			_pnode->stageidx = _distribution_p(_generator_p); // randomly allocated
	
			++mLocalPNodeNum[_treeIdx]; // primary nodes in current MPT

			++mLocalLevelPNodeNum[_treeIdx][_level]; // primary nodes at current level of current MPT
		}

		if (_length < U + (_level + 1) * K) { // [U, U + (_level + 1) * K - 1], in this level

			// current prefix must be inserted into the auxiliary prefix tree
			ins(_prefix, _length, _nexthop, _pnode->sRoot, 0, _level, _treeIdx, _generator_s, _distribution_s);
		}
		else { // insert the prefix into current primary node or a node in a higher level

			if (_pnode->t < MP) { // current primary node is not full, insert prefix into current primary node

				insertPrefixInPNode(_pnode, _prefix,_length, _nexthop);
			}
			else {
					
				// if the shortest prefix in current primary node is also shorter than the prefix to be inserted
				if (_pnode->prefixEntries[MP - 1].length < _length) { 

					// cache the shortest prefix in current primary node
					ip_type prefix = _pnode->prefixEntries[MP - 1].prefix;

					uint8 length = _pnode->prefixEntries[MP - 1].length;

					uint32 nexthop = _pnode->prefixEntries[MP - 1].nexthop;

					// delete shortest prefix in current primary node
					deletePrefixInPNode(_pnode, MP - 1);
					
					// insert new comer
					insertPrefixInPNode(_pnode, _prefix, _length, _nexthop);

					// recursively insert the prefix deleted from the primary node into a higher level
					ins(prefix, length, nexthop, _pnode->childEntries[utility::getBitsValue(prefix, U + _level * K, U + (_level + 1) * K - 1)], _level + 1, _treeIdx, _generator_p, _distribution_p, _generator_s, _distribution_s);
						
				}
				else {
				
					// insert prefix into a node in a higher level

					ins(_prefix, _length, _nexthop, _pnode->childEntries[utility::getBitsValue(_prefix, U + _level * K, U + (_level + 1) * K - 1)], _level + 1, _treeIdx, _generator_p, _distribution_p, _generator_s, _distribution_s);	
				}
			}
		}

		return;
	}	

	/// \brief for update, insert into the auxiliary tree.
	void ins(const ip_type& _prefix, const uint8& _length, const uint32& _nexthop, snode_type*& _snode, const int _sLevel, const int _pLevel, const uint32 _treeIdx, std::default_random_engine& _generator_s, std::uniform_int_distribution<int>& _distribution_s) {

		if (nullptr == _snode) { // empty

			// create a new secondary node
			_snode = new snode_type();

			_snode->stageidx = _distribution_s(_generator_s); // randomly allocated

			mLocalSNodeNum[_treeIdx]++;	

			mLocalLevelSNodeNum[_treeIdx][_pLevel + 1 + _sLevel]++; // be careful, required to plus 1 

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
					ip_type prefix = _snode->prefix;

					uint8 length = _snode->length;

					uint32 nexthop = _snode->nexthop;

					// replaced by _prefix
					_snode->prefix = _prefix;

					_snode->length = _length;

					_snode->nexthop = _nexthop;

					// recursively insert the replaced prefix
					if (0 == utility::getBitValue(prefix, U + _pLevel * K + _sLevel)) {

						ins(prefix, length, nexthop, _snode->lchild, _sLevel + 1, _pLevel, _treeIdx, _generator_s, _distribution_s);
					}
					else {

						ins(prefix, length, nexthop, _snode->rchild, _sLevel + 1, _pLevel, _treeIdx, _generator_s, _distribution_s);
					}	
				}
				else { // prefix in current node must be equal to the one to be inserted

					// do nothing
				}
			}
			else { // insert into a node in a higher level

				if (0 == utility::getBitValue(_prefix, U + _pLevel * K + _sLevel)) {

					ins(_prefix, _length, _nexthop, _snode->lchild, _sLevel + 1, _pLevel, _treeIdx, _generator_s, _distribution_s); 
				}
				else {

					ins(_prefix, _length, _nexthop, _snode->rchild, _sLevel + 1, _pLevel, _treeIdx, _generator_s, _distribution_s);
				}
			}
		}

		return;
	}

	/// \brief report number of nodes in each stage
	void reportNodeNumInStage(int _stagenum) {

		size_t* pnodeNumInStage = new size_t[_stagenum];

		size_t* snodeNumInStage = new size_t[_stagenum];

		for (int i = 0; i < _stagenum; ++i) {

			pnodeNumInStage[i] = 0;

			snodeNumInStage[i] = 0;
		}

		for (size_t i = 0; i < V; ++i) {

			if (nullptr == mRootTable[i]) {

				// do nothing
			}
			else {

				std::queue<pnode_type*> pqueue;

				std::queue<snode_type*> squeue;

				pqueue.push(mRootTable[i]);

				while(!pqueue.empty()) {

					auto pfront = pqueue.front();

					pnodeNumInStage[pfront->stageidx]++;

					// traverse auxiliary tree	
					if (nullptr != pqueue.front()->sRoot) {
				
						std::queue<snode_type*> squeue;

						squeue.push(pqueue.front()->sRoot);

						while (!squeue.empty()) {

							auto sfront = squeue.front();

							snodeNumInStage[sfront->stageidx]++;

							if (nullptr != sfront->lchild) squeue.push(sfront->lchild);

							if (nullptr != sfront->rchild) squeue.push(sfront->rchild);
							
							squeue.pop();
						}
					}
	
					// traverse childs
					for (size_t j = 0; j < MC; ++j) {

						if (nullptr != pqueue.front()->childEntries[j]) {

							pqueue.push(pqueue.front()->childEntries[j]);
						}

					}

					pqueue.pop();
				}
			}
		}


		size_t snodeNumInAllStages = 0;

		size_t pnodeNumInAllStages = 0;


		for (int i = 0; i < _stagenum; ++i) {

			snodeNumInAllStages += snodeNumInStage[i];

			pnodeNumInAllStages += pnodeNumInStage[i];

			std::cerr << "snode in stage " << i << ": " << snodeNumInStage[i] << std::endl;

			std::cerr << "pnode in stage " << i << ": " << pnodeNumInStage[i] << std::endl;
		}

		std::cerr << "snode in all stages: " << snodeNumInAllStages << std::endl;

		std::cerr << "pnode in all stages: " << pnodeNumInAllStages << std::endl;

		delete[] snodeNumInStage;

		delete[] pnodeNumInStage;
	
		return;
	}

};


#endif // RMPT_H
