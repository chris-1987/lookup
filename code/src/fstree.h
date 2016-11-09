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
#include <queue>
#include <deque>
#include <stack>
#include <cmath>

/// non-leaf node in a fixed-stride tree
///
/// contains 2^S entry, each of which consists of a prefix/child pointer.
template<int ENTRYNUM, bool LEAF>
struct FNode{

	struct data {

		union {
			uint32 prefix;

			void* child; //must be reinterpret_casted to proper type
		}

		uint32 nexthop;
	}	

	data[ENTRYNUM];

	FNode() {

		for (size_t i = 0; i < ENTRYNUM; ++i) {
			
			if (LEAF) {

				data[i].prefix = 0;
			}
			else {

				data[i].child = nullptr;	
			}	
			
			data[i].nexthop = 0;
		}	
	}
};


/// simple factory for building fixed-stride tree, singleton
///
/// create a fixed-stride tree, provided number of expansion levels and objective function
/// note  : CPE aims at minimize the total memory footprint while MINMAX aims at minimize the per-block memory footprint. Both CPE and MINMAX create a binary tree to faciliate the dynamic programmming process for determining strides in all the expansion levels.
/// build : first create an auxiliary binary tree, then determine $k$ length-ranges for minimizing the memory requirememt according to a pre-defined objective 
///         function
/// update: support two kinds of update, namely withdraw/announce a prefix and alter the nexthop information.

template<int K, int MODE>
class FSTree{

private:

	BNode* bt_root; // ptr to root node of auxiliary binary tree

	uint32 levelnodenum[K];

	int levelstride[K];

	void* fst_root; // ptr to root node of fixed-stride tree

private:

	FSTree (const FSTreeFactory& _factory) = delete;
	
	FSTree& operator= (const FSTreeFactory& _factory) = delete;

public:

	/// ctor
	FSTree() {

		for (int i = 0; i < K; ++i) levelnodenum[i] = 0;

		for (int i = 0; i < K; ++i) levelstride[i] = 0;
	}

	/// produce a fixed-stride tree
	void build4(const std::string & _bgptable) {

		// build a binary tree 
		BTree* bt = BTree::getInstance(_bgptable);

		bt_root = bt->build4(_bgptable);

		// compute level stride using dynamic programming
		computeLevelStride(bt);

		// build the fixed-stride tree by traversing the node 
		if (levelstride[0] == 0) {
		
			

		}
		fst_root = new FNode<node>t
	}

	

private:

	
	void computeExpansionLevel(std::string _bgptable);

};

template<int L, typename ObjFunc>
class FSTree{

private:

	FSTree 
	static BTree* bt; 

	BNode* root; /// ptr to root

	uint32 nodenum;

private:

	/// default ctor
	BTree() : root(nullptr), nodenum(0) {}

	BTree(const BTree& _bt) = delete;

	BTree& operator= (const BTree&) = delete;

	/// destroyer
	~BTree() {

		if (nullptr != root) {

			destroy();
		}	
	}

public: 

	/// create an instance of BTree if not yet instantiated; otherwise, return the instance.
	static BTree* getInstance();

	/// build BTree for IPv4 BGP table
	void build4(const std::string & _fn) {

		// destroy the old tree if there exists
		if (nullptr != root) {

			destroy();
		}

		// create a new root node
		root = new BNode();
		
		// insert prefixes into btree one by one
		std::ifstream fin(_fn, std::ios_base::binary);

		std::string line; 

		uint32 prefix;
	}					
};



#endif // _FST_H
