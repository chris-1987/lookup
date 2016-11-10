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

		uint8 length; // prefix length

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
	typedef BNode<W> bnode_type;

	bnode_type* bt_root; // ptr to root node of auxiliary binary tree, this is used to 

	void* fst_root; // ptr to root node of fixed-stride tree

	struct Expansion{

		size_t nodenum; // num of nodes in the expansion level

		int stride; // stride of the expansion level

		size_t entrynum; // number of entries per node in the expansion level
	};
	
	std::vector<Expansion> expander;

	std::map<int, int> levelmapper; // level in btree mapped to expansion level in fstree

private:

	FSTree (const FSTree& _factory) = delete;
	
	FSTree& operator= (const FSTree& _factory) = delete;

public:

	/// ctor
	FSTree() : bt_root(nullptr), fst_root(nullptr) {}

	/// produce a fixed-stride tree
	void build4(const std::string & _fn) {

		// build a binary tree 
		BTree<W>* bt = BTree<W>::getInstance();

		bt_root = bt->build4(_fn);

		// compute level stride using dynamic programming
		computeStride(bt);

		// create the root node
		fst_root = new FNode<W>(static_cast<size_t>(expander[0].entrynum));

		// traverse binary tree in bridth-first order to create the fixed-stride tree 
		std::queue<bnode_type*> bt_queue;


		bt_queue.push(bt_root);

		while (!bt_queue.empty()) {

			bt_queue.front();
		}
			
	}

	

private:

	/// compute stride for expansion level using dynamic programming technique
	void computeStride(bnode_type* _btRoot) {

		
	}

};


#endif // _FST_H
