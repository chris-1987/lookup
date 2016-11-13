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


/// non-leaf node in a fixed-stride tree
///
/// contains 2^S entry, each of which consists of a prefix/child pointer.
/// @param W 32 or 128 for IPv4 or IPv6

template<int W>
struct FNode{

	typedef typename choose_ip_type<W>::ip_type ip_type;  // if W = 32 or 128, then ip_type is uint32 or uint128

	/// data field, including prefix, a pointer to child and next hop information

	struct data {

		ip_type prefix; // in the final fst-tree, prefix and child pointer share a common space

		void* child; // in the final fst-tree, child pointer and prefix share a common space

		uint8 length; // length of original prefix

		uint32 nexthop; // next hop information, typically a pointer to route information
	};

	data* dataArr; // the array size is determined by the stride

	FNode (const size_t _entrynum) {

		dataArr = new data[_entrynum];

		for (size_t i = 0; i < _entrynum; ++i) {
			
			dataArr[i].prefix = 0;

			dataArr[i].child = nullptr;

			dataArr[i].length = 0;
			
			dataArr[i].nexthop = 0;
		}	
	}

	~FNode () {

		delete [] dataArr;

		dataArr = nullptr;
	}
};


/// simple factory for building fixed-stride tree, singleton
///
/// create a fixed-stride tree, provided number of expansion levels and objective function
/// note  : CPE aims at minimize the total memory footprint while MINMAX aims at minimize the per-block memory footprint. Both CPE and MINMAX create a binary tree to faciliate the dynamic programmming process for determining strides in all the expansion levels.
/// build : first create an auxiliary binary tree, then determine $k$ length-ranges for minimizing the memory requirememt according to a pre-defined objective 
///         function
/// update: support two kinds of update, namely withdraw/announce a prefix and alter the nexthop information.
/// @param W 32 or 128 for IPv4 or IPv6
/// @param K number of stride
/// @param M strategy for level expansion, 0 or 1 for CPE or MINMAX

template<int W, int K, int M>
class FSTree{

private:

	static FSTree<W, K, M> * fst;

	typedef BTree<W> btree_type;

	typedef BNode<W> bnode_type;

	struct Expansion{

		size_t nodenum; // num of nodes in the expansion level

		int stride; // stride of the expansion level

		size_t entrynum; // number of entries per node in the expansion level
	};

private:

	void* fst_root;	

	int mapper[K]; // map expansion level to levels

	int entrynum[K]; // number of entries per node in expansion level

	int imapper[W + 1]; // map level to expansion level


private:

	FSTree (const FSTree& _factory) = delete;
	
	FSTree& operator= (const FSTree& _factory) = delete;

public:

	/// ctor
	FSTree () : fst_root(nullptr) {}

	/// produce a fixed-stride tree
	void build (const std::string& _fn) {

		// build a binary tree 
		btree_type* bt = btree_type::getInstance();

		bt->build(_fn);

		// compute level stride using dynamic programming
		doPrefixExpansion(bt);

		// create the root node of which the stride is 2^mapper[0]
		fst_root = new FNode<W>(static_cast<size_t>(mapper[0]));
	
		for (size_t i = 0; i < 	
			
		//fst_root = new FNodeW>(static_cast<size_t>(leveexpander[0].entrynum));		
			
	}

	static FSTree<W, K, M>* getInstance();

private:

	/// perform prefix expansion according to node distribution in binary tree
	void doPrefixExpansion(btree_type* _bt) {

		switch(M) {

		case 1: 
			doPrefixExpansionCPE(_bt); break; // compressed prefix expansion

		case 2: 
			doPrefixExpansionMM(_bt); break; // min-max

		default:
			doPrefixExpansionCPE(_bt); 
		}
	}

	/// perform level expansion following compressed prefix expansion
	void doPrefixExpansionCPE(btree_type* _bt) {

		size_t tArr[W + 1][K]; // tArr[i][j] is the minimum memory requirement for expanding i + 1 levels to j + 1 levels 

		size_t mArr[W + 1][K]; // mArr[i][j] = r, such that tArr[r][j - 1] + node(r) * 2^(i - r) is minimum for j <= r < i

		//initialize tArr[][]
		for (int i = 0; i < W + 1; ++i) {

			for (int j = 0; j < K; ++j) {

				tArr[i][j] = std::numeric_limits<size_t>::max();

				mArr[i][j] = std::numeric_limits<size_t>::max();
			}
		}
		
		// tArr[i][0] = 2^i, expand all nodes in level [0, i] to expansion level 0, this level only contains one node
		for (int i = 1; i < W + 1; ++i) {

			tArr[i][0] = static_cast<size_t>(pow(2, i)) * 1; // one node only, 	

			mArr[i][0] = W + 1; // previous expansion level includes no levels 
		}
		
		// tArr[i][i] = tArr[i - 1][i - 1] + levelnodenum[i]. 
		tArr[0][0] = _bt->getLevelNodeNum(0); // must be 0 
		
		mArr[0][0] = W + 1; // previous expansion level includes no levels

		for (int i = 1; i < K; ++i) {

			tArr[i][i] = tArr[i - 1][i - 1] + _bt->getLevelNodeNum(i);

			mArr[i][i] = i - 1; // the highest level of the previous expansion level is i - 1
		}
	
		// calculate the remaining subproblems by DP 	
		for (int j = 1; j < K; ++j) {

			for (int i = j + 1; i < W + 1; ++i) {
				
				for (int k = j - 1; k < i; ++k) {
				
					size_t tmp = tArr[k][j - 1] + (_bt->getLevelNodeNum(k + 1) * static_cast<size_t>(pow(2, i - k)));

					if (tArr[i][j] > tmp) {

						tArr[i][j] = tmp;
				
						mArr[i][j] = k;
					}
				}
			}
		}
	
		// find the optimal by scanning mArr, store the result in mapper
		mapper[K - 1] = W;

		for (int i = K - 2; i >= 0; --i) {

			mapper[i] = mArr[mapper[i + 1]][i + 1];
		}
	
		// generate the inverse mapper
		for (int i = 0, j = 0; i < K; ++i) {

			for (; j <= mapper[i]; ++j) {

				imapper[j] = i;
			}
		}

	
		// output the optimal
		std::cerr << "mapper: " << std::endl;

		for (int i = 0; i < K; ++i) {

			std::cerr << mapper[i] << " ";
		}

		std::cerr << "imapper: " << std::endl;

		for (int i = 0; i < W + 1; ++i) {

			std::cerr << imapper[i] << " ";
		}
		
		std::cerr << "\nmax: " << tArr[W][K - 1] << std::endl;		
	}


	/// perform level expansion following Min-Max
	void doPrefixExpansionMM(btree_type* _bt) {

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
