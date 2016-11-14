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
/// A node contains _entrynum entries, each of which consists of a prefix/child pointer, a length field and a nexthop field (typically a pointer).
/// 
/// \param W 32 or 128 for IPv4 or IPv6, respectively.
template<int W>
struct FNode{

	typedef typename choose_ip_type<W>::ip_type ip_type;  // if W = 32 or 128, then ip_type is uint32 or uint128

	/// \brief payload, including prefix, a pointer to child and next hop information
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
/// A node contains _entrynum entries, each of which consists of a prefix/child pointer, a length field and a nexthop field (typically a pointer).
/// 
/// \param W 32 or 128 for IPv4 or IPv6, respectively.
template<int W>
struct FNode2{

	typedef typename choose_ip_type<W>::ip_type ip_type;  // if W = 32 or 128, then ip_type is uint32 or uint128

	/// \brief payload
	///
	/// Each entry contains either the length of the corresponding prefix or the pointer to a subtree.
	/// We check nexthop to determine whether store the child pointer or the length of the prefix. 
	struct Entry {

		union{
			FNode2<W>* child; ///< after leaf-pushing, child pointer and prefix share a common space
	
			uint8 length; ///< length of original prefix
		};

		uint32 nexthop; ///< next hop information, typically a pointer to route information
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

	int mapper[K]; ///< map expansion level to levels

	int entrynum[K + 1]; ///< number of entries per node in expansion level, entrynum[K] is not used

	int stride[K]; ///< stride of each level

	int imapper[W + 1]; ///< map level to expansion level
	
	uint32 nodenum; ///< number of nodes in total

	uint32 levelnodenum[K]; ///< number of nodes per level

private:

	FSTree (const FSTree& _factory) = delete;
	
	FSTree& operator= (const FSTree& _factory) = delete;

public:

	/// \brief ctor
	FSTree () : fst_root(nullptr) {}

	/// \brief produce a fixed-stride tree (without leaf-pushing)
	void build (const std::string& _fn) {

		// build an auxiliary binary tree 
		btree_type* bt = btree_type::getInstance();

		bt->build(_fn);

		// compute expansion levels using dynamic programming
		doPrefixExpansion(bt);

		// create the root node
		fst_root = new fnode_type(entrynum[0]);
	
		++nodenum;

		++levelnodenum[0];

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

			if (0 == length) { // */0, do nothing

			}
			else { // insert the prefix
	
//				ins(prefix,length,nexthop, fst_root, 0);
			}
			
		}

		std::cerr << "created node num: " << nodenum << std::endl;

		for (int i = 0; i < K; ++i) {

			std::cerr << "the " << i << "'s level--node num: " << levelnodenum[i] << std::endl;
		}


		// rebuild the fixed-stride tree by leaf-pushing
//		rebuild();

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

		// find the optimal scheme and store the result in mapper
		mapper[K - 1] = W;

		for (int i = K - 2; i >= 0; --i) {

			mapper[i] = rArr[mapper[i + 1]][i + 1];
		}
	
		// compute stride for each expansion level
		stride[0] = mapper[0];

		for (int i = 1; i < K; ++i) {

			stride[i] = mapper[i] - mapper[i - 1]; 
		}

		// compute entrynum per node in each level
		entrynum[0] = static_cast<size_t>(pow(2, stride[0]));

		for (int i = 1; i < K; ++i) {

			entrynum[i] = _bt->getLevelNodeNum(mapper[i - 1] + 1) * static_cast<size_t>(pow(2, stride[i]));
		}

		entrynum[K] = 0; // used as a boundary sentinel

		// generate the inverse mapper
		for (int i = 0, j = 0; i < K; ++i) {

			for (; j <= mapper[i]; ++j) {

				imapper[j] = i;
			}
		}


#ifdef DEBUG_FST
	
		// output result
		std::cerr << "mapper: ";

		for (int i = 0; i < K; ++i) {

			std::cerr << mapper[i] << " ";
		}

		std::cerr << "\nstride: ";
	
		for (int i = 0; i < K; ++i) {

			std::cerr << stride[i] << " ";
		}

		std::cerr << "\nentrynum: ";

		for (int i = 0; i < K; ++i) {

			std::cerr << entrynum[i] << " ";
		}

		std::cerr << "\nimapper: ";

		for (int i = 0; i < W + 1; ++i) {

			std::cerr << imapper[i] << " ";
		}
		
		std::cerr << "\ntotal memory footprint in unit of entries: " << pArr[W][K - 1] << std::endl;	
		
		std::cerr << "maximum per-stage memory requirement in unit of entries: " << qArr[W][K - 1] << std::endl; 
		

#endif

		return;
	}


	/// \brief insert a prefix into the fixed-stride tree
	///
	/// Expand prefix into a set of fixed-length ones and insert them one by one into the fixed-stride tree.
	/// A prefix may embrace another one in the same node, be careful.
	/// \note This function is used only when building the fixed-stride tree while leaf-pushing is not done yet.
//	void ins (const ip_type & _prefix, const uint8 _length, const uint32& _nexthop, fnode_type* _node, const int _level) {
//
//		fnode_type* node = nullptr;
//			
//		if (_length < mapper[_level]) { // insert prefix into current node
//
//			node = _node;							
//
//			// expand prefix and insert the expanded ones into the node	
//			//size_t prefixSegment = utility::getBits(_prefix);
//
//			size_t entryBegOffset = prefixSegment << (mapper[_level] - _length);
//
//			size_t entryEndOffset = entryBegOffset + static_cast<int>(pow(2, mapper[_level] - length));
//
//			for (size_t i = entryBegOffset; i < entryEndOffset; ++i) {
//
//				if (node->entries[i].length < _length) {
//
//					node->entries[i].length = _length;
//	
//					node->entries[i].prefix = _prefix;
//
//					node->entries[i].nexthop = _nexthop;	
//				}
//			}
//		}	
//		else { // recursively insert prefix into a node in the next level
//
//			//size_t childOffset = utility::getBits(_prefix);
//			if (nullptr == _node->entries[childOffset]) {
//					
//				node = new fnode_type(entrynum[_level + 1]);					
//			}
//
//			ins (_prefix, _length, _nexthop, _node, _level + 1);
//		}
//	
//		return;	
//	}

	/// \brief delete a prefix from the fixed-stride tree
	///
	/// Update in a fixed-string tree (after leaf pushing) is expensive. This is much more costly when performing IPv6 lookups. 
	/// We do not investigate update performance of fixed-stride trees. This t
	void del() = delete;

	
	/// \brief Rebuid a fixed-stride tree by using leaf-pushing to reduce memory requirement. 
	///
	/// After leaf pushing, each entry in a node contains either a pointer to its subtree or a prefix, along with a pointer to nexthop inforamtion.
//	void rebuild(){
//
//		if (nullptr == fst_root) {
//			
//			fst2_root = nullptr;
//
//			return;
//		}
//
//		// step 1: traverse nodes in the non-leaf-pushed fixed-stride tree, push prefixes in parent nodes into child nodes
//		std::queue<std::tuple<fnode_type*, int, fnode_type::entry*>> queue; // a tuple of 3 items <node_ptr, level, parent_entry_ptr>
//
//		queue.push(std::tuple<fst_root, 0, nullptr>);
//
//		while (!queue.empty()) {
//
//			auto front = queue.front();
//
//			// process current node
//			if (0 == front.second) { // root node
//
//				// do nothing
//			}
//			else {
//
//				for (size_t i = 0; i < entrynum[front.second]; ++i) {
//
//					if (front.first->entry[i].length < front.third->length) {
//
//						front.first->entry[i].prefix = front.third->prefix;
//
//						front.first->entry[i].nexthop = front.third->nexthop;
//
//						front.first->entry[i].length = front.third->length;
//					}
//				}	
//			}
//
//			// push child nodes 
//			for (size_t i = 0; i < entrynum[front.second + 1]; ++i) {
//
//				if (nullptr != front.first->entry[i].child) {
//
//					queue.push(std::tuple(front.first->entry[i].child, front.second + 1, &(front.first->entry[i]));
//				}
//			}
//
//			queue.pop();				
//		} 
//
//
//		std::queue2<std::tuple<fnode_type*, int, fnode2_type*> queue; 
//
//		// create a mirror node for the root node
//		fnode2_root = new FNode2(entrynum[0]);
//	
//		queue2.push(std::tuple<fst_root, 0, fst2_root>);
//
//		while (!queue.empty()) {
//
//			auto front = queue.front();
//
//			// update mirror nodes
//			for (size_t i = 0; i < entrynum[front.second]; ++i) {
//
//				if (nullptr != fnode_root->entries[i].child) { // has a child, update child
//		
//					fnode2_root->entries[i].child = fnode_root->entries[i].child;
//				} 
//				else { // has a child, update length
//	
//					fnode2_root->entries[i].length = fnode_root->entries[i].length;
//				}
//
//				fnode2_root->entries[i].nexthop = fnode_root->entries[i].nexthop; // update nexthop
//			}
//		
//			// create mirror nodes
//			for (size_t i = 0; i < entrynum[front.second]; ++i) {
//
//				if (nullptr != front.first->entries[i].child) {
//
//					FNode2* node2 = new FNode2(entrynum[front.second + 1]);
//		
//					queue2.push(std::tuple<front.first->entries[i].child, front.second, node2>);
//				}		
//			}
//		}		
//	}

	/// \brief destroy the fixed-stride tree (without leaf-pushing)
	///
	/// traverse the fixed-stride tree  
	void destroy() {

		// destroy the fixed-stride tree with the root node fst_root (with leaf-pushing)
		if (nullptr != fst_root) {

			return;
		}		

		std::queue<std::pair<fnode_type*, int>> queue;	

		queue.push(std::pair<fst_root, 0>);

					
//		queue.push(std::pair<fst_root, 0>);
//
//		while (!queue.empty()) {
//			
//			for (size_t i = 0; i < entrynum[queue.front().first]; ++i) {
//			
//				if (nullptr != queue.front()->entry[i]) {
//
//					queue.push(std::pair<queue.front().first->entry[i], queue.front().second + 1>); 
//					delete queue.front();
//
//					queue.pop();
//				}
//			} 
//		}
//		
//		// destory the fixed-stride tree with the root node fst2_root (without leaf-pushing)
//		if (nullptr != fst2_root) {
//
//			return;
//		}	
//
//		std::queue<std::pair<fnode2_type*, int>> queue2;
//
//		queue2.push(std::pair<fst2_root, 0>);
//
//		while (!queue.empty()) {
//
//
//			for (size_t i = 0; i < entrynum[queue2.front().first]; ++i) {
//
//
//				if (nullptr != queue2.front()->entry[i]) {
//
//
//					queue2.push(std::pair<queue2.front().first->entry[i], queue2.front().second + 1>);
//
//					delete queue2.front();
//
//
//					queue2.pop();
//				}
//			}
//		}
//	}

	~FSTree() {

		destroy();
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
