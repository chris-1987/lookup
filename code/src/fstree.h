#ifndef _FST_H
#define _FST_H

////////////////////////////////////////////////////////////
/// Copyright (c) 2016, Sun Yat-sen University,
/// All rights reserved
/// @file fstree.h
/// @brief definition of fixed-stride trees
/// build, insert and delete prefixes in a fixed-stride tree
/// @author Yi Wu
///////////////////////////////////////////////////////////

#include "common.h"
#include "utility.h"
#include "btree.h"
#include <queue>
#include <deque>
#include <stack>
#include <cmath>
#include <map>

#define DEBUG_FST

/// \brief A node in a fixed-stride tree before leaf-pushing.
///
/// Each node contains _entrynum entries, where each entry records prefix, child pointer, length field and nexthop.
/// 
/// \param W 32 or 128 for IPv4 or IPv6, respectively.
template<int W>
struct FNode{

	typedef typename choose_ip_type<W>::ip_type ip_type;  // if W = 32 or 128, then ip_type is uint32 or uint128

	/// \brief payload
	///
	/// 4 fields, which are prefix, pointer to child, length and next hop information
	///
	struct Entry {

		ip_type prefix; ///< prefix in integer value

		FNode<W>* child; ///< pointer to the subtree

		uint8 length; ///< length of the prefix

		uint32 nexthop; ///< next hop information
	};

	Entry* entries; ///< _entrynum entries of data

	/// ctor
	FNode (const size_t _entrynum) {

		entries = new Entry[_entrynum];

		for (size_t i = 0; i < _entrynum; ++i) {
			
			entries[i].prefix = 0;

			entries[i].child = nullptr;

			entries[i].length = 0;
			
			entries[i].nexthop = 0;
		}	
	}

	/// dtor
	~FNode () {

		delete[] entries;

		entries = nullptr;
	}
};


/// \brief A node in a fixed-stride tree after leaf-pushing.
///
/// Each node contains _entrynum entries, where each entry only contains one field shared by child pointer and nexthop+length.
/// 
/// \param W 32 or 128 for IPv4 or IPv6, respectively.
template<int W>
struct FNode2{

	typedef typename choose_ip_type<W>::ip_type ip_type;  // if W = 32 or 128, then ip_type is uint32 or uint128

	/// \brief payload
	///
	/// 1 field shared by child pointer and length + nexthop
	///
	struct Entry {

		union {
			FNode2<W>* child;
	
			struct {
				
				uint8 length; // 8 bits, adjustable
	
				uint32 nexthop; // 8 bits, adjustable
			};
		};	

		bool isLeaf; // this field can be combined into union when implemented 
	};

	Entry* entries; ///< _entrynum entries of data

	/// ctor
	FNode2 (const size_t _entrynum) {

		entries = new Entry[_entrynum];

		for (size_t i = 0; i < _entrynum; ++i) {
			
			entries[i].child = nullptr; // nullptr is represented by 0 in machine code

			entries[i].length = 0;
			
			entries[i].nexthop = 0;
		}	
	}

	/// dtor
	~FNode2 () {

		delete[] entries;

		entries = nullptr;
	}
};

/// \brief Create a fixed-stride tree. Singleton class.
///
/// To build a fixed-stride tree, we first create an auxiliary binary tree and then determine expansion levels by using CPE or MINMAX.
/// we expand each prefix into a set of fixed-length ones and insert them into the fixed-stride tree one by one. 
/// Afterward, we rebuild the fixed-stride tree by using the leaf-pushing technique for a smaller memory requirement. 
/// \note Two strategies are used to perform prefix expansion. The first approach CPE, namely compressed prefix expansion aims at minimize the total memory footprint, while the second approach MINMAX aims at minimize the per-block memory footprint but tries to keep the total memory footprint as small as possible. Update in a fixe-stride tree is expensive due to the use of prefix expansion and leaf-pushing. This becomes even worse for the next generation IPv6 route lookup techniques. Therefore, we only investigate the performance of IP lookups for a fixed-stride tree and do not provide interfaces to insert/delete prefixes in fixed-stride tree after leaf-pushing.
/// \param W 32 or 128 for IPv4 or IPv6
/// \param K number of expansion levels
/// \param M 0 or 1 for CPE or MINMAX
template<int W, int K, int M>
class FSTree{

private:

	typedef BTree<W> btree_type; ///< auxiliary binary tree

	typedef BNode<W> bnode_type; ///< node type in the auxiliary binary tree

	typedef FNode<W> fnode_type; ///< node type before leaf-pushing

	typedef FNode2<W> fnode2_type; ///< node type after leaf-pushing

	typedef typename choose_ip_type<W>::ip_type ip_type;  ///< integer type adopted by ipv4/ipv6

private:

	static FSTree<W, K, M>* fst; ///< pointer to fst

	fnode_type* fst_root; ///< pointer to the root node of the fixed-stride tree before leaf-pushing

	fnode2_type* fst2_root; ///< pointer to the root node of the fixed-stride tree after leaf-pushing

	// dynamic programming parameters
	int mBegLevel[K]; ///< start level of expansion level

	int mEndLevel[K]; ///< end level of expansion level
	
	int mStride[K]; ///< stride of expansion level

	uint32 mNodeEntryNum[K + 1]; ///< number of entries per node in expansion level

	int mExpansionLevel[W + 1]; ///< expansion level of level in binary tree

	// collected information of fst
	uint32 mNodeNum; ///< number of nodes in total

	uint32 mLevelNodeNum[K]; ///< number of nodes in each level

	uint32 mEntryNum; ///< number of entries in total

	uint32 mLevelEntryNum[K]; ///< number of entries in each level

	uint32 mMaxLevelEntryNum; ///< maximum number of entries among all the levels 
private:

	FSTree (const FSTree& _factory) = delete;
	
	FSTree& operator= (const FSTree& _factory) = delete;

public:

	/// \brief ctor
	FSTree () : fst_root(nullptr), fst2_root(nullptr), mNodeNum(0), mEntryNum(0) {

		for (int i = 0; i < K; ++i) {

			mLevelNodeNum[i] = 0;
		}

		for (int i = 0; i < K; ++i) {

			mLevelEntryNum[i] = 0;
		}

		for (int i = 0; i < K; ++i) {

			mBegLevel[i] = 0;
		}

		for (int i = 0; i < K; ++i) {

			mEndLevel[i] = 0;
		}

		for (int i = 0; i < K; ++i) {

			mStride[i] = 0;
		}

		for (int i = 0; i < K + 1; ++i) {

			mNodeEntryNum[i] = 0;
		}

		for (int i = 0; i < W + 1; ++i) {

			mExpansionLevel[i] = 0;
		}

		mMaxLevelEntryNum = 0;
	}

	/// \brief produce a fixed-stride tree (without leaf-pushing)
	void build(const std::string& _fn) {

		if(nullptr != fst_root) {

			destroy();
		}

		// build an auxiliary binary tree 
		btree_type* bt = btree_type::getInstance();

		bt->build(_fn);

		// compute expansion levels using dynamic programming
		doPrefixExpansion(bt);

		// create the root node
		fst_root = new fnode_type(mNodeEntryNum[0]);
	
		//read the BGP table and insert all the prefixes into the fixed-stride tree
		std::ifstream fin(_fn, std::ios_base::binary);					

		std::string line;

		ip_type prefix;

		uint8 length;

		uint32 nexthop;

		while (getline(fin, line)) {

			// retrieve prefix and length
			utility::retrieveInfo(line, prefix, length);

			nexthop = length; // represent the nexthop information by length (for test only)

			if (0 == length) { // attempt to insert */0, do nothing
			
			}
			else { // insert the prefix
				
				//std::cerr << "prefix: " << prefix << " length: " << static_cast<uint32>(length) << " nexthop: " << nexthop << std::endl;

				ins(prefix, length, nexthop, fst_root, 0);	
			}
			
		}

		traverse_fst();

		// rebuild the fixed-stride tree by leaf-pushing
		rebuild();

		return;
	}

	static FSTree<W, K, M>* getInstance();

private:

	/// \brief generate prefix expansion scheme according to node distribution in binary tree
	/// 
	/// Two strategies with different objectives are available for prefix expansion. The template argument M is used to specify the strategy.
	/// CPE aims at minimize the total memory footprint in all the SRAM, while MinMax aims at minimize per-block memory footprint 
	/// and tries to keep the total memory footprint as small as possible.
	/// \note CPE is used in default.
	void doPrefixExpansion(btree_type* _bt) {

		size_t pArr[W + 1][K]; // record memory requirement over all pipe stages

		size_t qArr[W + 1][K]; // record per-stage memory requirement

		size_t rArr[W + 1][K]; // record the level selected to be expanded in the previous iteration

		//initialize pArr, qArr and rArr
		for (int i = 0; i < W + 1; ++i) {

			for (int j = 0; j < K; ++j) {

				pArr[i][j] = std::numeric_limits<size_t>::max();

				qArr[i][j] = std::numeric_limits<size_t>::max();

				rArr[i][j] = std::numeric_limits<size_t>::max();
			}
		}
		
		// pArr[i][0] = qArr[i][0] = 2^i, rArr[i][0] = W + 1. That is, use expansion level 0 to represent levels 0, 1, ..., i
		for (int i = 1; i < W + 1; ++i) {

			pArr[i][0] = static_cast<size_t>(pow(2, i)); 	
	
			qArr[i][0] = pArr[i][0];

			rArr[i][0] = W + 1; // no previous iteration 
		}
		
		// pArr[i][i] = pArr[i - 1][i - 1] + levelnodenum[i], qArr[i][i] = max{qArr[i - 1][i - 1], levelnodenum[i]}
		pArr[0][0] = _bt->getLevelNodeNum(0);
		
		qArr[0][0] = pArr[0][0];

		rArr[0][0] = W + 1; // no previous iteration

		for (int i = 1; i < K; ++i) {

			pArr[i][i] = pArr[i - 1][i - 1] + _bt->getLevelNodeNum(i);

			qArr[i][i] = std::max(qArr[i - 1][i - 1], static_cast<size_t>(_bt->getLevelNodeNum(i)));

			rArr[i][i] = i - 1;
		}
	
		// calculate the remaining subproblems by dynamic programming	
		for (int j = 1; j < K; ++j) {
	
			for (int i = j + 1; i < W + 1; ++i) {
				
				for (int k = j - 1; k < i; ++k) {
			
					size_t tmp_p = pArr[k][j - 1] + (_bt->getLevelNodeNum(k + 1) * static_cast<size_t>(pow(2, i - k)));
					
					size_t tmp_q = std::max(qArr[k][j - 1], (_bt->getLevelNodeNum(k + 1) * static_cast<size_t>(pow(2, i - k))));

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
		mEndLevel[K - 1] = W;

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


#ifdef DEBUG_FST
	
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

		for (int i = 0; i < W + 1; ++i) {

			std::cerr << mExpansionLevel[i] << " ";
		}
		
		std::cerr << "\nCalculated by DP, total memory footprint in unit of entries: " << pArr[W][K - 1] << std::endl;	
		
		std::cerr << "Calculated by DP, maximum per-stage memory requirement in unit of entries: " << qArr[W][K - 1] << std::endl; 
		
#endif

		return;
	}



	/// \brief insert a prefix into the fixed-stride tree
	///
	/// Expand prefix to generate a set of fixed-length ones, insert them one by one into the fixed-stride tree.
	/// 
	/// \note This function is used only when building the fixed-stride tree without leaf-pushing. 
	void ins(const ip_type& _prefix, const uint8 _length, const uint32& _nexthop, fnode_type* _node, const int _expansionLevel) {


//		std::cerr << "target expansion level: " << mExpansionLevel[_length] << " current expansion level: " << _expansionLevel << std::endl;
	
		if (mExpansionLevel[_length] > _expansionLevel) { // current node is not the location of the prefix

			uint32 begBit = mBegLevel[_expansionLevel] - 1;

			uint32 endBit = mEndLevel[_expansionLevel] - 1;
			
			uint32 entryIndex = utility::getBitsValue(_prefix, begBit, endBit);

//			std::cerr << "begBit: " << begBit << " endBit: " << endBit << std::endl;

//			std::cerr << "entryIndex: " << entryIndex << std::endl;

			fnode_type* childNode = nullptr;


			if (nullptr == _node->entries[entryIndex].child) { // no child node, create it

				childNode = new fnode_type(mNodeEntryNum[_expansionLevel + 1]);

				_node->entries[entryIndex].child = childNode;
			}			
			else {

				childNode = _node->entries[entryIndex].child;

			}
			
			ins(_prefix, _length, _nexthop, childNode, _expansionLevel + 1); // recursively insert the prefix
		}
		else { // expand prefix and insert them into current node

			uint32 begBit = mBegLevel[_expansionLevel] - 1;

			uint32 endBit = _length - 1;

			uint32 begEntryIndex = utility::getBitsValue(_prefix, begBit, endBit);

//			std::cerr << "bitsvalue: " << begEntryIndex << std::endl;

			begEntryIndex = begEntryIndex << (mEndLevel[_expansionLevel] - _length);			

			uint32 endEntryIndex = begEntryIndex + static_cast<uint32>(pow(2, mEndLevel[_expansionLevel] - _length));

//			std::cerr << "begBit: " << begBit << " endBit: " << endBit << std::endl;

//			std::cerr << "begEntryIndex: " << begEntryIndex << " endEntryIndex: " << endEntryIndex << std::endl;
					
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




	/// \brief delete a prefix from the fixed-stride tree
	///
	/// Update in a fixed-string tree (after leaf pushing) is expensive. This is much more costly when performing IPv6 lookups. 
	/// We do not investigate update performance of fixed-stride trees. This t
	void del() = delete;

	
	/// \brief To reduce memory requirement, rebuid a fixed-stride tree using leaf-pushing. 
	///
	/// After leaf-pushing prefixes in non-leaf entries, each entry in a node contains either a pointer to its subtree or a prefix, but not both.
	///
	void rebuild() {

		if (nullptr != fst2_root) {

			destroy();
		}
	

		// step1: traverse nodes in the fixed-stride tree with the root node fst_root, push prefixes from lower levels to higher levels
		{	
			std::queue<std::tuple<fnode_type*, int, typename fnode_type::Entry*> > queue; // <node_ptr, expansionlevel, parent_entry_ptr>
		
			queue.push(std::tuple<fnode_type*, int, typename fnode_type::Entry*>(fst_root, 0, nullptr));
	
			while (!queue.empty()) {
		
				auto front = queue.front();
	
				// update current node
				if (0 == std::get<1>(front)) { // root node
	
					// no need to modify root node, do nothing
				}
				else {
	
					for (size_t i = 0; i < mNodeEntryNum[std::get<1>(front)]; ++i) {
	
						if (std::get<0>(front)->entries[i].length < std::get<2>(front)->length) {
	
							std::get<0>(front)->entries[i].prefix = std::get<2>(front)->prefix;
				
							std::get<0>(front)->entries[i].nexthop = std::get<2>(front)->nexthop;
	
							std::get<0>(front)->entries[i].length = std::get<2>(front)->length;
						}
					}				
				}
	
				// recursively push child nodes
				for (size_t i = 0; i < mNodeEntryNum[std::get<1>(front)]; ++i) {
	
					if (nullptr != std::get<0>(front)->entries[i].child) {
	
						queue.push(std::tuple<fnode_type*, int, typename fnode_type::Entry*>(std::get<0>(front)->entries[i].child, std::get<1>(front) + 1, &(std::get<0>(front)->entries[i])));
					}
				}
				
				queue.pop();
			}	

			traverse_fst();
		}

		// step 2: traverse nodes in the fixed-stride tree with the root node fst_root, create a mirror node for each visited node to rebuild the tree
		// The newly creatd fixed-stride tree is rooted at fst2_root and consists of multiple fnode2_type nodes.
		{		
			std::queue<std::tuple<fnode_type*, int, fnode2_type*> > queue;		

			// create a mirror node for fst_root
			fst2_root = new fnode2_type(mNodeEntryNum[0]);

			mLevelNodeNum[0] = 1;

			mNodeNum = 1;

			queue.push(std::tuple<fnode_type*, int, fnode2_type*>(fst_root, 0, fst2_root));	

			while (!queue.empty()) {

				auto front = queue.front();

				// update mirror node
				for (size_t i = 0; i < mNodeEntryNum[std::get<1>(front)]; ++i) {

					if (nullptr == std::get<0>(front)->entries[i].child) { // leaf node
	
						std::get<2>(front)->entries[i].isLeaf = true;

						std::get<2>(front)->entries[i].length = std::get<0>(front)->entries[i].length;

						std::get<2>(front)->entries[i].nexthop = std::get<0>(front)->entries[i].nexthop;
					} 
					else { // non-leaf entry, need to record the child pointer and create a mirror for the child node
	
						std::get<2>(front)->entries[i].isLeaf = false;

						std::get<2>(front)->entries[i].child = new fnode2_type(mNodeEntryNum[std::get<1>(front) + 1]);

						mLevelNodeNum[std::get<1>(front) + 1] += 1;

						mNodeNum += 1;

						queue.push(std::tuple<fnode_type*, int, fnode2_type*>(std::get<0>(front)->entries[i].child, std::get<1>(front) + 1, std::get<2>(front)->entries[i].child));
					}
	
				}		

				queue.pop();
			}	

			traverse_fst2();
		}

		// compute actual storage requirement
		for (int i = 0; i < K; ++i) {

			mLevelEntryNum[i] = mLevelNodeNum[i] * mNodeEntryNum[i];

			if (mLevelEntryNum[i] > mMaxLevelEntryNum) {

				mMaxLevelEntryNum = mLevelEntryNum[i];
			}

			mEntryNum += mLevelEntryNum[i];
		}

		std::cerr << "After rebuild, the tree information: \n";

		std::cerr << "total node num: " << mNodeNum << std::endl;

		std::cerr << "level node num: \n";

		for (int i = 0; i < K; ++i) {

			std::cerr << "level " << i << ": " << mLevelNodeNum[i] << std::endl;
		}

		std::cerr << "total entry num: " << mEntryNum << std::endl;

		for (int i = 0; i < K; ++i) {

			std::cerr << "level " << i << ": " << mLevelEntryNum[i] << std::endl;
		}		

		std::cerr << "mMaxLevelEntryNum: " << mMaxLevelEntryNum << std::endl;

		return;
	}

public:
	/// \brief search 
	uint32 search(const ip_type& _ip) {

		uint32 nexthop = 0;

		int expansionLevel = 0;

		fnode2_type* node = fst2_root;

		// find a leaf entry contains LPM
		while (true) {

			uint32 begBit = mBegLevel[expansionLevel] - 1;

			uint32 endBit = mEndLevel[expansionLevel] - 1;

			uint32 entryIndex = utility::getBitsValue(_ip, begBit, endBit);

			if (true == node->entries[entryIndex].isLeaf) {

				nexthop = node->entries[entryIndex].nexthop;

				break;
			}
			else {

				node = node->entries[entryIndex].child;
			}

			++expansionLevel;
		}

		return nexthop;		
	}


	/// \brief destroy the fixed-stride tree (without leaf-pushing)
	///
	/// traverse the fixed-stride tree  
	void destroy() {

		{

			// destroy the fixed-stride tree starting with the root node fst_root (without leaf-pushing)
			if (nullptr != fst_root) {
	
				return;
			}		
	
			std::queue<std::pair<fnode_type*, int> > queue; // <fnode_type*, level>	
	
			queue.push(std::pair<fnode_type*, int>(fst_root, 0)); // push root node
	
			while (!queue.empty()) {
	
				auto front = queue.front();
		
				for (size_t i = 0; i < mNodeEntryNum[front.second]; ++i) {
	
					if (nullptr != front.first->entries[i].child) {
	
						queue.push(std::pair<fnode_type*, int>(front.first->entries[i].child, front.second + 1));
	
						delete front.first;
	
						queue.pop();
					}		
				}
			}				
		}

		{
			// destroy the fixed-stride tree starting with the root node fst2_root (with leaf-pushing) 
			if (nullptr != fst2_root) {

				return;
			}

			std::queue<std::pair<fnode2_type*, int> > queue2; // <fnode2_type*, level>
		
			queue2.push(std::pair<fnode2_type*, int>(fst2_root, 0)); // push root node
	
			while (!queue2.empty()) {
	
				auto front = queue2.front();
	
				for (size_t i = 0; i < mNodeEntryNum[front.second]; ++i) {

					if (nullptr != front.first->entries[i].child) {

						queue2.push(std::pair<fnode2_type*, int>(front.first->entries[i].child, front.second + 1));

						delete front.first;
					}
				}

				queue2.pop();
			}
		}
	
	}

	~FSTree() {

		destroy();
	}


	/// \brief traverse the fixed-stride tree rooted at fst_root 
	void traverse_fst(){

		static size_t totalNodeNum = 0;

		static size_t levelNodeNum[K] = {0};

		if (0 != totalNodeNum) {

			totalNodeNum = 0;
			
			for (int i = 0; i < K; ++i) {

				levelNodeNum[i] = 0;
			}
		}		

		std::queue<std::pair<fnode_type*, int> > queue;

		queue.push(std::pair<fnode_type*, int>(fst_root, 0));

		totalNodeNum++;

		levelNodeNum[0]++;

		while (!queue.empty()) {

			auto front = queue.front();

	//		printFSTNode(front.first, front.second);

			for (size_t i = 0; i < mNodeEntryNum[front.second]; ++i) {

				if (nullptr != front.first->entries[i].child) {

					queue.push(std::pair<fnode_type*, int>(front.first->entries[i].child, front.second + 1));

					totalNodeNum++;

					levelNodeNum[front.second + 1]++;
				}
			}

			queue.pop();
		}


		std::cerr << "fst:\n";

		std::cerr << "total node num: " << totalNodeNum << std::endl;

		std::cerr << "level node num: \n";

		for (int i = 0; i < K; ++i) {

			std::cerr << "level " << i << ": " << levelNodeNum[i] << std::endl;
		}

		return;
	}

	/// \brief print fnode_type
	void printFSTNode(fnode_type* _node, int _level){

		for (size_t i = 0; i < mNodeEntryNum[_level]; ++i) {
			
			std::cerr << "entry no: " << i;

			std::cerr << " prefix: " << _node->entries[i].prefix;

			std::cerr << " length: " << static_cast<uint32>(_node->entries[i].length);

			std::cerr << " nexthop: " << _node->entries[i].nexthop;

			std::cerr << " child: " << _node->entries[i].child << std::endl;
		}
	}


	/// \brief traverse the fixed-stride tree rooted at fst2_root 
	void traverse_fst2(){

		static size_t totalNodeNum = 0;

		static size_t levelNodeNum[K] = {0};

		if (0 != totalNodeNum) {

			totalNodeNum = 0;
			
			for (int i = 0; i < K; ++i) {

				levelNodeNum[i] = 0;
			}
		}		

		std::queue<std::pair<fnode2_type*, int> > queue;

		queue.push(std::pair<fnode2_type*, int>(fst2_root, 0));

		totalNodeNum++;

		levelNodeNum[0]++;

		while (!queue.empty()) {

			auto front = queue.front();

	//		printFST2Node(front.first, front.second);

			for (size_t i = 0; i < mNodeEntryNum[front.second]; ++i) {

				if (false == front.first->entries[i].isLeaf) { // non-leaf entry

					queue.push(std::pair<fnode2_type*, int>(front.first->entries[i].child, front.second + 1));

					totalNodeNum++;

					levelNodeNum[front.second + 1]++;
				}
			}

			queue.pop();
		}


		std::cerr << "fst2 info: \n";

		std::cerr << "total node num: " << totalNodeNum << std::endl;

		std::cerr << "level node num: \n";

		for (int i = 0; i < K; ++i) {

			std::cerr << "level " << i << ": " << levelNodeNum[i] << std::endl;
		}

		return;
	}


	/// \brief print fnode2_type
	void printFST2Node(fnode2_type* _node, int _level){

		for (size_t i = 0; i < mNodeEntryNum[_level]; ++i) {
			
			std::cerr << "entry no: " << i;

			if (false == _node->entries[i].isLeaf) { // non-leaf node

				std::cerr << " length: invalid ";
			
				std::cerr << " nexthop: invalid ";

				std::cerr << " child: " << _node->entries[i].child << std::endl;
			}
			else { // leaf node

				std::cerr << " length: " << _node->entries[i].length;
			
				std::cerr << " nexthop: " << _node->entries[i].nexthop;

				std::cerr << " child: invalid" << std::endl;				
			}
		}
	}


};

template<int W, int K, int M>
FSTree<W, K, M>* FSTree<W, K, M>::fst = nullptr;

template<int W, int K, int M>
FSTree<W, K, M>* FSTree<W, K, M>::getInstance() {

	if (nullptr == fst) {

		fst = new FSTree();
	}

	return fst;
}

#endif // _FST_H
