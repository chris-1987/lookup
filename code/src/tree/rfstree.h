#ifndef _RFST_H
#define _RFST_H

////////////////////////////////////////////////////////////////////////////////
/// Copyright (c) 2016, Sun Yat-sen University,
/// All right reserved.
/// \file rfstree.h
/// \brief Definition of a lookup index based on fixed-stride tree
/// 
/// The lookup index consists of three parts: a fast lookup table that contains route information for prefixes shorter than U bits,
/// a table that consists of 2^U entries and each entry stores a pointer to the root of a fixed-stride tree,
/// and a set of fixed-stride trees.
///
/// \author Yi Wu
/// \date 2016.11
////////////////////////////////////////////////////////////////////////////////

#include "../common/common.h"
#include "../common/utility.h"
#include "rbtree.h"
#include <queue>
#include <deque>
#include <stack>
#include <cmath>
#include <map>
#include <chrono>

#define DEBUG_RFST

/// \brief Node in a non-leaf-pushed fixed-stride tree
///
/// Each node contains a set of entries and each entry records a prefix,
/// its child pointer, the length of prefix and the nexthop information.
///
/// \param W 32 or 128 for IPv4 or IPv6, respectively.
template<int W>
struct FNode{

	typedef typename choose_ip_type<W>::ip_type ip_type;

	/// \brief entry content 
	struct Entry{

		ip_type prefix;

		FNode<W>* child;

		uint8 length;

		uint32 nexthop;

		Entry() : prefix(0), child(nullptr), length(0), nexthop(0) {}
	};

	Entry* entries; ///< a pointer to entry array

	/// \brief ctor
	FNode(const size_t _entrynum) {

		entries = new Entry[_entrynum];

		for (size_t i = 0; i < _entrynum; ++i) {

			entries[i].prefix = 0;

			entries[i].child = nullptr;

			entries[i].length = 0;

			entries[i].nexthop = 0;
		}	
	} 

	/// \brief dtor
	~FNode() {

		delete[] entries;

		entries = nullptr;
	}
};

/// \brief A node in a leaf-pushed fixed-stride tree.
///
/// Each node contains a set of entries and each entry stores a child pointer
/// or a length/nexthop pair.
///
/// \param W 32 or 128 for IPv4 or IPv6, respectively.
template<int W>
struct FNode2{

	typedef typename choose_ip_type<W>::ip_type ip_type;

	/// \brief payload
	///
	/// A field reused to store a child pointer or a (length, nexthop) pair.
	///
	struct Entry{

		union {
		
			FNode2<W>* child;

			struct{

				uint8 length;

				uint32 nexthop;
			};
		};

		bool isLeaf; ///< this field only needs 1 bit when being implemented
		
		Entry() : child(nullptr), isLeaf(false) {}

	};

	int stageidx;

	Entry* entries;

	/// \brief ctor
	FNode2(const size_t _entrynum) {

		entries = new Entry[_entrynum];

		stageidx = 0;
	}

	~FNode2(){

		delete[] entries;

		entries = nullptr;
	}
};


/// \brief Create an index based on leaf-pushed fixed-stride tree.
///
/// The index consists of three parts: a fast lookup table, a root table and a forest of leaf-pushed fixed-stride trees.
/// To build each leaf-pushed fixed-stride tree, we first build the corresponding
/// non-leaf-pushed fixed-stride tree and then reconstruct the tree by push
/// prefixes in internal nodes into leaf nodes.
/// \param W 32 or 128 for IPv4 or IPv6
/// \param K number of expansion levels
/// \param M 0 for CPE, 1 for MINMAX and 2 for different levels with a same stride
/// \param U A threshold for classifying short and long prefixes.
/// \param V The number of fixed-stride trees
template<int W, int K, int M, int U, size_t V = static_cast<size_t>(pow(2, U))>
class RFSTree{

private:

	typedef RBTree<W, U> rbtree_type; 
	
	typedef BNode<W> bnode_type;

	typedef FNode<W> fnode_type;

	typedef FNode2<W> fnode2_type;

	typedef typename choose_ip_type<W>::ip_type ip_type;

private:

	fnode_type* mRootTable[V]; ///< non-leaf-pushed

	fnode2_type* mRootTable2[V]; ///< leaf-pushed

	int mBegLevel[K]; ///< start level of expansion level

	int mEndLevel[K]; ///< end level of expansion level

	int mStride[K]; ///< stride of expansion level

	size_t mNodeEntryNum[K + 1]; ///< number of entries in a node at different level
	
	size_t mExpansionLevel[W - U + 1]; ///< correspoding expansion level for each level in rbt

	size_t mGlobalLevelNodeNum[K]; ///< number of nodes in the forest at each level

	size_t mLocalLevelNodeNum[V][K]; ///< number of nodes in each tree at each level

	size_t mGlobalLevelEntryNum[K]; ///< number of entries in the forest at each level

	size_t mLocalLevelEntryNum[V][K]; ///< number of entries in each tree at each level

	size_t mLocalNodeNum[V]; ////< number of nodes in each tree

	size_t mLocalEntryNum[V]; ///< number of entries in each tree

	size_t mTotalNodeNum; ///< number of nodes at all levels in all trees

	size_t mTotalEntryNum;  ///< number of entries at all levels in all trees

	size_t mMaxGlobalLevelEntryNum; ///< maximum number of entries in a level (nodes in a same level of all trees are summed up)

	FastTable<W, U - 1> *ft; ///< pointer to fast table
	
private:

	RFSTree(const RFSTree& _rfst) = delete;

	RFSTree operator= (const RFSTree& _rfst) = delete;

public:

	/// \brief ctor
	RFSTree() {

		initializeParameters();
	}


	/// \brief initialize parameters
	void initializeParameters() {
	
		for (size_t i = 0; i < V; ++i) {

			mRootTable[i] = nullptr;

			mRootTable2[i] = nullptr;
		}

		for (int i = 0; i < K; ++i) {

			mBegLevel[i] = 0;
		
			mEndLevel[i] = 0;

			mStride[i] = 0;
		}
		
		for (int i = 0; i < K + 1; ++i) {

			mNodeEntryNum[i] = 0;
		}
	
		for (int i = 0; i < W - U + 1; ++i) {

			mExpansionLevel[i] = 0;
		}
		
		for (size_t i = 0; i < K; ++i) {

			mGlobalLevelNodeNum[i] = 0;

			mGlobalLevelEntryNum[i] = 0;

			for (size_t j = 0; j < V; ++j) {

				mLocalLevelNodeNum[j][i] = 0;

				mLocalLevelEntryNum[j][i] = 0;
			}
		}		

		for (size_t i = 0; i < V; ++i) {

			mLocalNodeNum[i] = 0;

			mLocalEntryNum[i] = 0;
		}

		ft = new FastTable<W, U - 1>();
	}

	/// \brief dtor
	///
	/// destroy index
	~RFSTree() {

		clear();
	}

	/// \brief clear the index
	void clear() {

		for (size_t i = 0; i < V; ++i) {

			if (nullptr != mRootTable[i]) {

				destroy(i); // both non-leaf-pushed and leaf-pushed are destroyed
			}
		}

		delete ft;

		ft = nullptr;
	}


	/// \brief destroy a tree in the forest
	void destroy(const size_t _idx) {

		{
			if (nullptr != mRootTable[_idx]) {

				std::queue<std::pair<fnode_type*, int> > queue;

				queue.push(std::pair<fnode_type*, int>(mRootTable[_idx], 0));

				while (!queue.empty()) {

					auto front = queue.front();

					for (size_t i = 0; i < mNodeEntryNum[front.second]; ++i) {

						if (nullptr != front.first->entries[i].child) {

							queue.push(std::pair<fnode_type*, int>(front.first->entries[i].child, front.second + 1));
						}
					}

					delete front.first;

					queue.pop();
				}

				mRootTable[_idx] = nullptr;
			}
		}

		{

			if (nullptr != mRootTable2[_idx]) {

				std::queue<std::pair<fnode2_type*, int> > queue2;

				queue2.push(std::pair<fnode2_type*, int>(mRootTable2[_idx], 0));

				while (!queue2.empty()) {

					auto front = queue2.front();

					for (size_t i = 0; i < mNodeEntryNum[front.second]; ++i) {

						if (false == front.first->entries[i].isLeaf) {

							queue2.push(std::pair<fnode2_type*, int>(front.first->entries[i].child, front.second + 1));
						}
					}

					delete front.first;

					queue2.pop();
				}

				mRootTable2[_idx] = nullptr;
			}
		}

		return;
	}

	/// \brief produce a non-leaf-pushed fixed-stride tree	
	void build(const std::string& _fn) {

		// if an index exists, clear.
		clear();

		// initialize	
		initializeParameters();

		// build the auxiliary binary tree
		rbtree_type* rbt = new rbtree_type();

		rbt->build(_fn);
		
		// compute expansion levels using dynamic programming
		doPrefixExpansion(rbt);

		// insert prefixes in BGP table one by one
		std::ifstream fin(_fn, std::ios_base::binary);

		std::string line;

		ip_type prefix;

		uint8 length;

		uint32 nexthop;

		while (getline(fin, line)) {

			// retrieve prefix and length
			utility::retrieveInfo(line, prefix, length);

			nexthop = length;

			if (0 == length) {

				// do nothing
			}
			else {

				ins(prefix, length, nexthop);
			}
		}

		// rebuild the fixed-stride tree by leaf-pushing the prefixes
		rebuild();
	
		return;
	}

	/// \brief generate prefix expansion scheme according to node distribution in rbt.
	///	
	/// Three strategies are considered (CPE, MINMAX and even)
	void doPrefixExpansion(rbtree_type* _rbt) {

		switch(M) {

		case 0: 

		case 1: doPrefixExpansionDP(_rbt); break;
		
		case 2: doPrefixExpansionEven(); break;

		}
	}
		

	/// \brief evenly split levels in rbt into K partitions
	///
	/// \note Prefixes smaller than U bits are stored in fast table. Thus, we consider level U to be level 0 when performing prefix expansion.
	void doPrefixExpansionEven() {

		// compute stride
		for (int i = 0; i < K; ++i) {

			mStride[i] = (W - U) / K;
		}

		mStride[K - 1] += (W - U) % K; // add-on

		// compute corresponding endLevel in rbt
		mEndLevel[K - 1] = W - U;

		for (int i = K - 2; i >= 0; --i) {

			mEndLevel[i] = mEndLevel[i + 1] - mStride[i + 1];
		}

		// compute corresponding begLevel in rbt
		mBegLevel[0] = 1; // note that we number level U + 1 as level 1 herein

		for (int i = 1; i < K; ++i) {

			mBegLevel[i] = mEndLevel[i - 1] + 1;
		}

		// compute number of entries/childs per node in each level
		for (int i = 0; i < K; ++i) {

			mNodeEntryNum[i] = static_cast<size_t>(pow(2, mStride[i]));
		}

		mNodeEntryNum[K] = 0; // boundary sentinel

		// compute expansion level
		for (int i = 0, j = 0; i < K; ++i) {

			for (; j <= mEndLevel[i]; ++j) {

				mExpansionLevel[j] = i;
			}
		}

#ifdef DEBUG_RFST
	
		// output result
		std::cerr << "begin level:\n";

		for (int i = 0; i < K; ++i) {

			std::cerr << mBegLevel[i] << " ";
		}

		std::cerr << "\nstride:\n";
	
		for (int i = 0; i < K; ++i) {

			std::cerr << mStride[i] << " ";
		}

		std::cerr << "\nend level:\n";
	
		for (int i = 0; i < K; ++i) {

			std::cerr << mEndLevel[i] << " ";
		}

		std::cerr << "\nentrynum:\n";

		for (int i = 0; i < K; ++i) {

			std::cerr << mNodeEntryNum[i] << " ";
		}

		std::cerr << "\nexpansion level:\n";

		for (int i = 0; i < W - U + 1; ++i) {

			std::cerr << mExpansionLevel[i] << " ";
		}
				
#endif

		return;
	}


	/// \brief Two strategies for splitting nodes.
	///
	/// CPE aims at minimize the total memory footprint in all the pipe stages,
	/// while MINMAX aims at minimize per-block memory footprint.
	void doPrefixExpansionDP(rbtree_type* _rbt) {

		size_t pArr[W - U + 1][K]; // memory footprint in total

		size_t qArr[W - U + 1][K]; // per-stage memory requirement

		size_t rArr[W - U + 1][K]; // level selected to be expanded in the previous iteration

		//initialize pArr, qArr and rArr
		for (int i = 0; i < W - U + 1; ++i) {

			for (int j = 0; j < K; ++j) {

				pArr[i][j] = std::numeric_limits<size_t>::max();

				qArr[i][j] = std::numeric_limits<size_t>::max();

				rArr[i][j] = std::numeric_limits<size_t>::max();
			}
		}
	
		// pArr[i][0] 	
		// pArr[i][0] = qArr[i][0] = 2^i, rArr[i][0] = W + 1. That is, use expansion level 0 to represent levels 0, 1, ..., i
		for (int i = 1; i < W  - U + 1; ++i) {

			pArr[i][0] = _rbt->getLevelNodeNum(0) * static_cast<size_t>(pow(2, i));
	
			qArr[i][0] = pArr[i][0];

			rArr[i][0] = W - U + 1; // no previous iteration 
		}
		
		// pArr[i][i] = pArr[i - 1][i - 1] + levelnodenum[i], qArr[i][i] = max{qArr[i - 1][i - 1], levelnodenum[i]}
		pArr[0][0] = _rbt->getLevelNodeNum(0);
		
		qArr[0][0] = pArr[0][0];

		rArr[0][0] = W - U + 1; // no previous iteration

		for (int i = 1; i < K; ++i) {

			pArr[i][i] = pArr[i - 1][i - 1] + _rbt->getLevelNodeNum(i);

			qArr[i][i] = std::max(qArr[i - 1][i - 1], _rbt->getLevelNodeNum(i));

			rArr[i][i] = i - 1;
		}
	
		// calculate the remaining subproblems by dynamic programming	
		for (int j = 1; j < K; ++j) {
	
			for (int i = j + 1; i < W - U + 1; ++i) {
				
				for (int k = j - 1; k < i; ++k) {
			
					size_t tmp_p = pArr[k][j - 1] + (_rbt->getLevelNodeNum(k + 1) * static_cast<size_t>(pow(2, i - k)));
					
					size_t tmp_q = std::max(qArr[k][j - 1], (_rbt->getLevelNodeNum(k + 1) * static_cast<size_t>(pow(2, i - k))));

					if (0 == M) {
						// for j > i and j - 1 <= k < i, we have the following two formulas:
						// pArr[i][j] = min{pArr[k][j - 1] + 2^(i - k - 1) * levelnodenum[k + 1]}
						// qArr[i][j] = max{pArr[k][j - 1], 2^(i - k - 1) * levelnodenum[k + 1]}

						// if pArr[i][j] > tmp_p, then find a scheme that has smaller total memory requirement
						if (pArr[i][j] > tmp_p) {
	
							pArr[i][j] = tmp_p;
					
							qArr[i][j] = tmp_q;
				
							rArr[i][j] = k;
						}
					}
					else if (1 == M) {
						// for j > i and j - 1 <= k < i, we have the following two formulas:
						// qArr[i][j] = min{max{qArr[k][j - 1], 2^(i - k - 1) * levelnodenum[k + 1]}}
						// pArr[i][j] = min{pArr[k][j - 1] + 2^(i - k - 1) * levelnodenum[k + 1]}
	
						// if qArr[i][j] > tmp_q, then find a scheme that has smaller per-stage memory requirement
						// otherwise, if qArr[i][j] = tmp_q && pArr[i][j] > tmp_p, then find a scheme that has equal per-stage 
						// requirement but smaller total memory requirement
						if ((qArr[i][j] > tmp_q) || (qArr[i][j] == tmp_q && pArr[i][j] > tmp_p)) {
	
							pArr[i][j] = tmp_p;

							qArr[i][j] = tmp_q;
	
							rArr[i][j] = k;
						}
					}
				}
			}
		}

		// generate the optimal scheme
		// compute end level for expansion level
		mEndLevel[K - 1] = W - U;

		for (int i = K - 2; i >= 0; --i) {

			mEndLevel[i] = rArr[mEndLevel[i + 1]][i + 1];
		}
	
		// comptute stride for expansion level
		mStride[0] = mEndLevel[0];

		for (int i = 1; i < K; ++i) {

			mStride[i] = mEndLevel[i] - mEndLevel[i - 1]; 
		}

		// compute begin level for expansion level
		mBegLevel[0] = 1;

		for (int i = 1; i < K; ++i) {

			mBegLevel[i] = mEndLevel[i - 1] + 1;
		}

		// compute entrynum/childnum per node in each level
		for (int i = 0; i < K; ++i) {

			mNodeEntryNum[i] = static_cast<size_t>(pow(2, mStride[i]));
		}

		mNodeEntryNum[K] = 0; // used as a boundary sentinel

		// compute expansion level for corresponding level in binary tree
		for (int i = 0, j = 0; i < K; ++i) {

			for (; j <= mEndLevel[i]; ++j) {

				mExpansionLevel[j] = i;
			}
		}


#ifdef DEBUG_RFST
	
		// output result
		std::cerr << "begin level:\n";

		for (int i = 0; i < K; ++i) {

			std::cerr << mBegLevel[i] << " ";
		}

		std::cerr << "\nstride:\n";
	
		for (int i = 0; i < K; ++i) {

			std::cerr << mStride[i] << " ";
		}

		std::cerr << "\nend level:\n";
	
		for (int i = 0; i < K; ++i) {

			std::cerr << mEndLevel[i] << " ";
		}

		std::cerr << "\nentrynum:\n";

		for (int i = 0; i < K; ++i) {

			std::cerr << mNodeEntryNum[i] << " ";
		}

		std::cerr << "\nexpansion level:\n";

		for (int i = 0; i < W - U + 1; ++i) {

			std::cerr << mExpansionLevel[i] << " ";
		}
		
		std::cerr << "\nCalculated by DP, total memory footprint in unit of entries: " << pArr[W - U][K - 1] << std::endl;	
		
		std::cerr << "Calculated by DP, maximum per-stage memory requirement in unit of entries: " << qArr[W - U][K - 1] << std::endl; 
		
#endif

		return;

	}

		

	/// \brief report collected information
	///
	/// \note The information is collected after performing leaf-pushing
	void report() {

		// output number of nodes in total	
		mTotalNodeNum = 0;

		for (int i = 0; i < K; ++i) {

			mTotalNodeNum += mGlobalLevelNodeNum[i];
		}

		std::cerr << "total node num: " << mTotalNodeNum << std::endl;

		// compute number of entries in total
		mTotalEntryNum = 0;

		for (int i = 0; i < K; ++i) {

			mTotalEntryNum += mGlobalLevelEntryNum[i];
		}

		std::cerr << "total entry num: " << mTotalEntryNum << std::endl;

		// output number of nodes in each level
		std::cerr << "node in each level: " << std::endl;

		for (int i = 0; i < K; ++i) {

			std::cerr << "--level " << i << ": " << mGlobalLevelNodeNum[i] << std::endl;
		}

		// output number of entries in each level
		std::cerr << "entry in each level: " << std::endl;

		for (int i = 0; i < K; ++i) {

			std::cerr << "--level " << i << ": " << mGlobalLevelEntryNum[i] << std::endl;
		} 

		// output maximum number of entries in all the levels
		std::cerr << "\nmax entry num in a level is: " << mMaxGlobalLevelEntryNum << std::endl;


		// output number of nodes in each tree
		std::cerr << "node in each tree: " << std::endl;

		for (size_t i = 0; i < V; ++i) {

			std::cerr << "--tree " << i << ": " << mLocalNodeNum[i] << std::endl;
		}
		
		// output number of entries in each tree	
		std::cerr << "\nentry in each tree: " << std::endl;

		for (size_t i = 0; i < V; ++i) {

			std::cerr << "--tree " << i << ": " << mLocalEntryNum[i] << std::endl;
		}

		return;	
	}


	/// \brief insert a prefix into index
	void ins(const ip_type& _prefix, const uint8& _length, const uint32& _nexthop) {

		if (_length < U) { // insert short prefixes into fast table

			ft->ins(_prefix, _length, _nexthop);
		}
		else { // insert long prefixes into forest

			ins(_prefix, _length, _nexthop, mRootTable[utility::getBitsValue(_prefix, 0, U - 1)], 0, utility::getBitsValue(_prefix, 0, U - 1));
		}
	}


	/// \brief insert a long prefix into the index
	void ins(const ip_type& _prefix, const uint8& _length, const uint32 _nexthop, fnode_type*& _node, const int _expansionLevel, const size_t _treeIdx){

		if (nullptr == _node) {

			_node = new fnode_type(mNodeEntryNum[_expansionLevel]);
		}

		if (mExpansionLevel[_length - U] > _expansionLevel) { // insert _prefix into a higher expansion level

			uint32 begBit = mBegLevel[_expansionLevel] + U - 1;

			uint32 endBit = mEndLevel[_expansionLevel] + U - 1;

			uint32 entryIndex = utility::getBitsValue(_prefix, begBit, endBit); // branch

			ins(_prefix, _length, _nexthop, _node->entries[entryIndex].child, _expansionLevel + 1, _treeIdx);
		}	
		else { // expand prefix and insert them into current node

			uint32 begBit = mBegLevel[_expansionLevel] + U - 1;

			uint32 endBit = _length - 1;

			uint32 begEntryIndex = utility::getBitsValue(_prefix, begBit, endBit);

			begEntryIndex = begEntryIndex << (mEndLevel[_expansionLevel] + U - _length);

			uint32 endEntryIndex = begEntryIndex + static_cast<uint32>(pow(2, mEndLevel[_expansionLevel] + U - _length));

			for (size_t i = begEntryIndex; i < endEntryIndex; ++i) {

				if (_node->entries[i].length < _length) {

					_node->entries[i].length = _length;

					_node->entries[i].prefix = _prefix;

					_node->entries[i].nexthop = _nexthop;
				}
			}
		}
		
		return;
	}

	/// \brieve delete a prefix from the index
	///
	/// not available
	void del() = delete;

	
	/// \brief rebuild the fixed-stride tree by leaf-pushing
	void rebuild() {

		// step 1: traverse nodes in the fixed-stride tree with the root node fst_root, push prefixes from lower levels to higher levels
		{	

			for (size_t i = 0; i < V; ++i) {

				if (nullptr != mRootTable[i]) {

					// <node_ptr, expansionlevel, parent_entry_ptr>
					std::queue<std::tuple<fnode_type*, int, typename fnode_type::Entry*> > queue; 
		
					queue.push(std::tuple<fnode_type*, int, typename fnode_type::Entry*>(mRootTable[i], 0, nullptr));
	
					while (!queue.empty()) {
		
						auto front = queue.front();
	
						// update current node
						if (0 == std::get<1>(front)) { // root node of a tree
	
							// no need to modify root node, do nothing
						}
						else {
			
							for (size_t j = 0; j < mNodeEntryNum[std::get<1>(front)]; ++j) {
	
								if (std::get<0>(front)->entries[j].length < std::get<2>(front)->length) {
	
									std::get<0>(front)->entries[j].prefix = std::get<2>(front)->prefix;
						
									std::get<0>(front)->entries[j].nexthop = std::get<2>(front)->nexthop;
	
									std::get<0>(front)->entries[j].length = std::get<2>(front)->length;
								}
							}				
						}
	
						// recursively push child nodes
						for (size_t j = 0; j < mNodeEntryNum[std::get<1>(front)]; ++j) {
	
							if (nullptr != std::get<0>(front)->entries[j].child) {
	
								queue.push(std::tuple<fnode_type*, int, typename fnode_type::Entry*>(std::get<0>(front)->entries[j].child, std::get<1>(front) + 1, &(std::get<0>(front)->entries[j])));
							}
						}

						queue.pop();
					}	
				}	
			}

			traverseFST();
		}

		// step 2: traverse nodes in non-leaf-pushed trees to create mirror leaf-pushed trees
		{		

			for (size_t i = 0; i < V; ++i) {

				if (nullptr != mRootTable[i]) { // if the origin tree is not empty, then copy the tree

					std::queue<std::tuple<fnode_type*, int, fnode2_type*> > queue;		
						
					mRootTable2[i] = new fnode2_type(mNodeEntryNum[0]); 
				
					++mGlobalLevelNodeNum[0];

					++mLocalLevelNodeNum[i][0];
				
					queue.push(std::tuple<fnode_type*, int, fnode2_type*>(mRootTable[i], 0, mRootTable2[i]));

					while (!queue.empty()) {

						auto front = queue.front();

						// update mirror node
						for (size_t j = 0; j < mNodeEntryNum[std::get<1>(front)]; ++j) {

							if (nullptr == std::get<0>(front)->entries[j].child) { // leaf node
	
								std::get<2>(front)->entries[j].isLeaf = true;

								std::get<2>(front)->entries[j].length = std::get<0>(front)->entries[j].length;

								std::get<2>(front)->entries[j].nexthop = std::get<0>(front)->entries[j].nexthop;
							} 
							else { // non-leaf entry, need to record the child pointer and create a mirror for the child node
			
								std::get<2>(front)->entries[j].isLeaf = false;
		
								std::get<2>(front)->entries[j].child = new fnode2_type(mNodeEntryNum[std::get<1>(front) + 1]);

								++mGlobalLevelNodeNum[std::get<1>(front) + 1];

								++mLocalLevelNodeNum[i][std::get<1>(front) + 1];

								queue.push(std::tuple<fnode_type*, int, fnode2_type*>(std::get<0>(front)->entries[j].child, std::get<1>(front) + 1, std::get<2>(front)->entries[j].child));
							}
	
						}		
	
						queue.pop();
					}		
				}
			}

			traverseFST2();
			
			// compute actual storage requirement
			mMaxGlobalLevelEntryNum = 0;

			for (int i = 0; i < K; ++i) {
				
				mGlobalLevelEntryNum[i] = mGlobalLevelNodeNum[i] * static_cast<size_t>(pow(2, mStride[i]));

				if (mGlobalLevelEntryNum[i] > mMaxGlobalLevelEntryNum) {

					mMaxGlobalLevelEntryNum = mGlobalLevelEntryNum[i];
				}

				for (size_t j = 0; j < V; ++j) {

					mLocalLevelEntryNum[j][i] = mLocalLevelNodeNum[j][i] * static_cast<size_t>(pow(2, mStride[i]));
				}
			}

			// compute number of nodes in total	
			mTotalNodeNum = 0;
	
			for (int i = 0; i < K; ++i) {
	
				mTotalNodeNum += mGlobalLevelNodeNum[i];
			}

			// compute number of entries in total
			mTotalEntryNum = 0;
	
			for (int i = 0; i < K; ++i) {
	
				mTotalEntryNum += mGlobalLevelEntryNum[i];
			}

			// compute number of nodes in each tree
			for (size_t i = 0; i < V; ++i) {

				mLocalNodeNum[i] = 0;

				for (int j = 0; j < K; ++j) {

					mLocalNodeNum[i] += mLocalLevelNodeNum[i][j];
				}
			}

			// compute number of entries in each tree
			for (size_t i = 0; i < V; ++i) {

				mLocalEntryNum[i] = 0;	

				for (int j = 0; j < K; ++j) {

					mLocalEntryNum[i] += mLocalLevelEntryNum[i][j];
				}
			}
		}

		return;
	}

	/// \brief search LPM for target IP address
	uint32 search(const ip_type& _ip, std::vector<int>& _trace) {

		// try to find a match in the fast lookup table
		uint32 nexthop1 = 0;

		nexthop1 = ft->search(_ip);

		// try to find a match in the forest
		uint32 nexthop2 = 0;

		uint32 begBit = 0;

		uint32 endBit = U - 1;

		uint32 entryIndex = utility::getBitsValue(_ip, begBit, endBit);

		int expansionLevel = 0;

		if (nullptr != mRootTable2[entryIndex]) {

	 		fnode2_type* node = mRootTable2[entryIndex];
			
			while(true) {

				_trace.push_back(node->stageidx); // stageidx
			
				begBit = mBegLevel[expansionLevel] + U - 1;

				endBit = mEndLevel[expansionLevel] + U - 1;
				
				entryIndex = utility::getBitsValue(_ip, begBit, endBit);
				
				if (true == node->entries[entryIndex].isLeaf) {

					nexthop2 = node->entries[entryIndex].nexthop;

					break;
				}
				else {

					node = node->entries[entryIndex].child;
				}

				++expansionLevel;
			}
		}


		// a match in the forest must be longer than a match in 
		// the fast lookup table. Therefore, it has a higher priority
		// if found, return the nexthop2
		if (0 != nexthop2) return nexthop2;

		if (0 != nexthop1) return nexthop1;

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

	/// \brief traverse non-leaf-pushed fixed-stride tree
	void traverseFST(){

		static size_t testTotalNodeNum = 0;

		static size_t testGlobalLevelNodeNum[K] = {0};

		for (size_t i = 0; i < V; ++i) {

			if (nullptr != mRootTable[i]) {

				std::queue<std::pair<fnode_type*, int> > queue;

				queue.push(std::pair<fnode_type*, int>(mRootTable[i], 0));
	
				while(!queue.empty()) {
			
					auto front = queue.front();

					++testTotalNodeNum;
			
					++testGlobalLevelNodeNum[front.second]; 
					
					for (size_t j = 0; j < mNodeEntryNum[front.second]; ++j) {
				
						if (nullptr != front.first->entries[j].child){

							queue.push(std::pair<fnode_type*, int>(front.first->entries[j].child, front.second + 1));
						}
					}

					queue.pop();
				}
			}
		}

		std::cerr << "Traverse after leaf-pushing:\n";

		std::cerr << "Traversed node num: " << testTotalNodeNum << std::endl;

//		std::cerr << "Traverse node num in each level: \n";

//		for (int i = 0; i < K; ++i) {
//
//			std::cerr << "level " << i << ": " << testGlobalLevelNodeNum[i] << std::endl; 
//		}

		return;
	}

	void traverseFST2() {

		static size_t testTotalNodeNum = 0;

		static size_t testGlobalLevelNodeNum[K] = {0};

		for (size_t i = 0; i < V; ++i) {

			if (nullptr != mRootTable2[i]) {

				std::queue<std::pair<fnode2_type*, int> > queue;

				queue.push(std::pair<fnode2_type*, int>(mRootTable2[i], 0));
	
				while(!queue.empty()) {
			
					auto front = queue.front();

					++testTotalNodeNum;
			
					++testGlobalLevelNodeNum[front.second]; 
					
					for (size_t j = 0; j < mNodeEntryNum[front.second]; ++j) {
				
						if (false == front.first->entries[j].isLeaf){

							queue.push(std::pair<fnode2_type*, int>(front.first->entries[j].child, front.second + 1));
						}
					}

					queue.pop();
				}
			}
		}

		std::cerr << "Traverse after rebuilding the tree\n";

		std::cerr << "Traversed node num: " << testTotalNodeNum << std::endl;

//		std::cerr << "Traverse node num in each level: \n";

//		for (int i = 0; i < K; ++i) {
//
//			std::cerr << "level " << i << ": " << testGlobalLevelNodeNum[i] << std::endl; 
//		}

		return;

	}

		
	/// \brief Scatter nodes in the forest to a pipeline
	///
	/// For CPE and MINMAX, we only consider linear pipeline
	/// For EVEN, we consider linear, circular and random pipelines 
	void scatterToPipeline(int _pipestyle, int _stagenum = W - U + 1) {
	
		switch(_pipestyle) {

		case 0: lin(_stagenum); break;

		case 1: ran(_stagenum); break;

		case 2: cir(_stagenum); break;

		}

		return;		
	}	

	
	/// \brief Scatter nodes in a linear pipeline.
	///
	/// Map nodes into a linear pipe line in a manner of one level per stage.
	///
	/// \note the number of stages is W - U + 1 for a linear pipe line.
	/// \note CPE, MINMAX and EVEN are permitted
	void lin(int _stagenum) {
		
		size_t* nodeNumInStage = new size_t[_stagenum];

		for (int i = 0; i < _stagenum; ++i) {

			nodeNumInStage[i] = 0;
		}

		// start numbering from 0 to _stagenum - 1, nodes in a same level are located in the same level
		for (size_t i = 0; i < V; ++i) {

			if (nullptr != mRootTable2[i]) {

				std::queue<std::pair<fnode2_type*, int> > queue;

				queue.push(std::pair<fnode2_type*, int>(mRootTable2[i], 0));
	
				while(!queue.empty()) {
			
					auto front = queue.front();

					front.first->stageidx = front.second;
					
					nodeNumInStage[front.second] += 1;
						
					for (size_t j = 0; j < mNodeEntryNum[front.second]; ++j) {

						if (false == front.first->entries[j].isLeaf) {

							queue.push(std::pair<fnode2_type*, int>(front.first->entries[j].child, front.second + 1));
						}
					}
	
					queue.pop();
				}
			}
		}

		size_t nodeNumInAllStages = 0;

		size_t entryNumInAllStages = 0;

		for (size_t i = 0; i < _stagenum; ++i) {

			nodeNumInAllStages += nodeNumInStage[i];

			entryNumInAllStages += nodeNumInStage[i] * mNodeEntryNum[i];

			std::cerr << "nodes in stage " << i << ": " << nodeNumInStage[i] << std::endl;
		}

		std::cerr << "nodes in all stages: " << nodeNumInAllStages << std::endl;

		std::cerr << "entries in all stages: " << entryNumInAllStages << std::endl;

		delete[] nodeNumInStage;
	}


	/// \brief Scatter in a random pipeline.
	///
	/// Map nodes into a random pipeline. A random generator is applied to determine the pipe stage to where a node is allocated.
	///
	void ran(int _stagenum) {
		
		// collect information about the number of nodes/entries in each stage
		size_t* nodeNumInStage = new size_t[_stagenum];

		size_t* entryNumInStage = new size_t[_stagenum];

		for (int i = 0; i < _stagenum; ++i) {

			nodeNumInStage[i] = 0;

			entryNumInStage[i] = 0;
		}
	
		unsigned seed = std::chrono::system_clock::now().time_since_epoch().count();

		std::default_random_engine generator = std::default_random_engine(seed);

		std::uniform_int_distribution<int> distribution = std::uniform_int_distribution<int>(0, _stagenum - 1);

		auto roll = std::bind(distribution, generator);

		
		// start numbering from 0 to _stagenum - 1, nodes in a same level are located in the same level
		for (size_t i = 0; i < V; ++i) {

			if (nullptr != mRootTable2[i]) {

				std::queue<std::pair<fnode2_type*, int> > queue;

				queue.push(std::pair<fnode2_type*, int>(mRootTable2[i], 0));
	
				while(!queue.empty()) {
			
					auto front = queue.front();

					int rand_stageidx = roll();

					front.first->stageidx = rand_stageidx;
							
					nodeNumInStage[rand_stageidx] += 1;

					entryNumInStage[rand_stageidx] += mNodeEntryNum[front.second];
						
					for (size_t j = 0; j < mNodeEntryNum[front.second]; ++j) {

						if (false == front.first->entries[j].isLeaf) { // has child, push the child

							 queue.push(std::pair<fnode2_type*, int>(front.first->entries[j].child, front.second + 1));
						}
					}
			
					queue.pop();
				}
			}
		}

		size_t nodeNumInAllStages = 0;

		size_t entryNumInAllStages = 0;

		for (size_t i = 0; i < _stagenum; ++i) {

			nodeNumInAllStages += nodeNumInStage[i];

			entryNumInAllStages += entryNumInStage[i];

//			std::cerr << "nodes in stage " << i << ": " << nodeNumInStage[i] << std::endl;
	
			std::cerr << "entries in stage " << i << ": " << entryNumInStage[i] << std::endl;
		}

//		std::cerr << "nodes in all stages: " << nodeNumInAllStages << std::endl;

		std::cerr << "entries in all stages: " << entryNumInAllStages << std::endl;

		delete[] nodeNumInStage;

		delete[] entryNumInStage;

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

	/// \brief Scatter in a circular pipe line.
	///
	/// Scatter nodes into a circular pipe line according to method proposed by Sailesh Karmar et al.
	/// we use variance to make heuristic.
	/// Because nodes in a fixed-stride tree is of different size in different levels, we perform the heuristic by coloring the entries
	/// instead of coloring the nodes.
	void cir(int _stagenum) {

		// step 1: sort binary tries by their size in non-decreasing order
		std::vector<SortElem> vec;

		for (size_t i = 0; i < V; ++i) {

			if (nullptr != mRootTable2[i]) {

				vec.push_back(SortElem(mLocalEntryNum[i], i)); // mLocalEntrynum records number of entries in a tree
			}		
		}

		std::sort(std::begin(vec), std::end(vec));		

		// step 2: scatter nodes
		size_t* colored = new size_t[_stagenum];

		for (int i = 0; i < _stagenum; ++i) colored[i]= 0;

		size_t* trycolor = new size_t[_stagenum];

		for (int i = 0; i < _stagenum; ++i) trycolor[i] = 0;

		int bestStartIdx = 0;

		for (int i = vec.size() - 1; i >= 0; --i) { // larger-size tree first
		
			size_t treeIdx = vec[i].treeIdx;

			double bestVar = std::numeric_limits<double>::max();

			for (size_t j = 0; j < _stagenum; ++j) { // try to put the root node at j-th pipe stage

				for (size_t k = 0; k < _stagenum; ++k) { // reset 

					trycolor[k] = colored[k];
				}
	
				for (size_t k = 0; k < K; ++k) { // put nodes (color all entries) one level per stage

					trycolor[(j + k) % _stagenum] += mLocalLevelEntryNum[treeIdx][k]; 		
				}

				// compute variance
				double sum = std::accumulate(trycolor, trycolor + _stagenum, 0.0);

				double mean = sum / _stagenum;

				double accum = 0.0;

				for (int k = 0; k < _stagenum; ++k) {

					accum += (trycolor[k] - mean) * (trycolor[k] - mean);
				}
				
				double var = accum / _stagenum;

//				std::cerr << "var: " << var << " bestVar: " << bestVar << std::endl;
//				std::cin.get();

				if (var < bestVar) {
					
					bestVar = var;

					bestStartIdx = j;
				}
			}	

			for (int j = 0; j < K; ++j) {
				
				colored[(bestStartIdx + j) % _stagenum] += mLocalLevelEntryNum[treeIdx][j];	
			}

			// color current tree
			std::queue<std::pair<fnode2_type*, int> > queue;

			mRootTable2[treeIdx]->stageidx = bestStartIdx;

			queue.push(std::pair<fnode2_type*, int>(mRootTable2[treeIdx], 0));

			while (!queue.empty()) {

				auto front = queue.front();
			
				for (size_t j = 0; j < mNodeEntryNum[front.second]; ++j) {
				
					if (false == front.first->entries[j].isLeaf) {

						front.first->entries[j].child->stageidx = (front.first->stageidx + 1) % _stagenum; // wrap around

						queue.push(std::pair<fnode2_type*, int>(front.first->entries[j].child, front.second + 1));
					}	
				}

				queue.pop();
			}
		} 

		size_t entryNumInAllStages = 0;

		for (int i = 0; i < _stagenum; ++i) {
			
			std::cerr << "entry in stage " << i << ": " << colored[i] << std::endl;

			entryNumInAllStages += colored[i];
		}

		std::cerr << "entries in all stages: " << entryNumInAllStages << std::endl;

		delete[] colored;

		delete[] trycolor;
	}
};


#endif // _RFST_H
