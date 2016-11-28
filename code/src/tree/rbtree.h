#ifndef _RBTREE_H
#define _RBTREE_H

////////////////////////////////////////////////////////////
/// Copyright (c) 2016, Sun Yat-sen University,
/// All rights reserved
/// \file rbtree.h
/// \brief Definition of an IP lookup index based on binary tree.
///
/// The lookup index consists of three parts: a fast lookup table that contains route information for prefixes shorter than U bits,
/// a table that consists of 2^U entries and each entry stores a pointer to the root of a binary tree, 
/// and a set of binary tree.

/// \author Yi Wu
/// \date 2016.11
///////////////////////////////////////////////////////////


#include "../common/common.h"
#include "../common/utility.h"

#include "fasttable.h"

#include <queue>
#include <deque>
#include <stack>
#include <cmath>
#include <random>
#include <algorithm>
#include <numeric>
#include <limits>
#include <sstream>


/// \brief Nodes in a binary tree.
/// 
/// Two pointers that indicate the child nodes and a nexthop information field.
///
/// \param W 32 or 128 for IPv4 or IPv6 address, respectively.
template<int W>
struct BNode {

	typedef typename choose_ip_type<W>::ip_type ip_type;

	BNode* lchild; ///< pointer to left child

	BNode* rchild; ///< pointer to right child
		
	uint32 nexthop; ///< next hop information

	int stageidx; ///< stage index, indicate the pipestage in which the node is located
	
	/// \brief ctor
	BNode() : lchild(nullptr), rchild(nullptr), nexthop(0), stageidx(0) {}
};


/// \brief Build and update the index.
///
/// Prefixes shorter than U bits are stored in a fast lookup table.
/// Prefixes not shorter than U bits are stored in the set of binary trees.
///
/// \param W 32 for IPV4 and 128 for IPV6.
/// \param U A threshold for classifying shorter and longer prefixes ( < U and >=U).
/// \param V Number of binary trees at large.
template<int W, int U, size_t V = static_cast<size_t>(pow(2, U))>
class RBTree {
private:
	
	typedef typename choose_ip_type<W>::ip_type ip_type;

	typedef BNode<W> node_type;

	node_type* mRootTable[V]; ///< each entry points to a binary tree

	uint32 mNodeNum[V]; ///< number of nodes in each binary tree

	uint32 mLevelNodeNum[V][W - U + 1]; ///< number of nodes in each level of a binary tree

	uint32 mTotalNodeNum; ///< number of nodes in total

	FastTable<W, U - 1> *ft; ///< pointer to fast table, a fast table is used to store shorter prefixes and provide search, insert and delete interfaces

public:

	/// \brief default ctor
	RBTree() {

		initializeParameters();
	}

	/// \brief initialize parameters
	void initializeParameters() {

		mTotalNodeNum = 0;

		ft = nullptr;
	
		for (int i = 0; i < V; ++i) {

			mRootTable[i] = nullptr;
		}

		for (int i = 0; i < V; ++i) {

			mNodeNum[i] = 0;
		}
	
		for (int i = 0; i < V; ++i) {

			for (int j = 0; j < W - U + 1; ++j) {

				mLevelNodeNum[i][j] = 0;
			}
		}

		ft = new FastTable<W, U - 1>();
	}

	/// \brief disable copy-ctor
	RBTree(const RBTree& _rbt) = delete;

	/// \brief disable assignment op
	RBTree& operator= (const RBTree&) = delete;

	/// \brief dtor
	///
	/// destory index
	///
	~RBTree() {

		for (size_t i = 0; i < V; ++i) {

			if (nullptr != mRootTable[i]) {

				destroy(i);

				mRootTable[i] = nullptr;
			}
		}

		delete ft;
	
		ft = nullptr;
	}

	/// \brief clear
	void clear() {

		for (size_t i = 0; i < V; ++i) {

			if (nullptr != mRootTable[i]) {

				destroy(i);

			}
		}
	}
	/// \brief Build the index.
	void build(const std::string & _fn) {

		// clear old index if there exists any
		clear();

		// 
		initializeParameters();
	
		// insert prefixes one by one into index
		std::ifstream fin(_fn, std::ios_base::binary);

		std::string line; 

		ip_type prefix;

		uint8 length;

		uint32 nexthop;

		while(getline(fin, line)) {

			// retrieve prefix and length
			utility::retrieveInfo(line, prefix, length);

			nexthop = length;

			if (0 == length) { // insert */0
		
				// do nothing
			}
			else { // insert into index
			
				ins(prefix, length, nexthop);
			}
		}

		report();

		// traverse();

		return;
	}
	
	/// \brief destroy a binary tree
	void destroy(size_t _idx) {

		if (nullptr == mRootTable[_idx]) {
		
			return;
		}

		node_type* node = mRootTable[_idx];

		std::queue<node_type*> queue;
		
		queue.push(node);

		while(!queue.empty()) {

			auto front = queue.front();

			if (nullptr != front->lchild) queue.push(front->lchild);

			if (nullptr != front->rchild) queue.push(front->rchild);

			delete front;

			front = nullptr;	

			queue.pop();
		}

		return;
	}			
	
	/// \brief Report statistical inforamtion collected at the time point.
	void report() {

		mTotalNodeNum = 0;

		for (int i = 0; i < V; ++i) {

		//	std::cerr << "\n------node num in the " << i << "'s tree: " << mNodeNum[i] << std::endl;

		//	for (int j = 0; j < W - U + 1; ++j) {

		//		std::cerr << "level " << j << ": " << mLevelNodeNum[i][j]<< "\t";
		//	}
			
		//	std::cin.get();

			mTotalNodeNum += mNodeNum[i];	
		}

		std::cerr << "node num in total: " << mTotalNodeNum << std::endl;
	}



	/// \brief insert into index	
	void ins(const ip_type& _prefix, const uint8& _length, const uint32& _nexthop) {

		if (_length < U){ // insert into a fast lookup table (use prefix + length as index entry) 
	
			ft->ins(_prefix, _length, _nexthop);	
					
		}
		else { // insert into a binary tree

			ins(_prefix, _length, _nexthop, mRootTable[utility::getBitsValue(_prefix, 0, U - 1)], U, utility::getBitsValue(_prefix, 0, U - 1)); 
		}

		return;
	}	
		

	/// \brief insert into a binary tree
	void ins(const ip_type& _prefix, const uint8& _length, const uint32& _nexthop, node_type*& _node, const int _level, const size_t _treeIdx) {

		if (nullptr == _node) {

			_node = new node_type();

			++mNodeNum[_treeIdx];

			++mLevelNodeNum[_treeIdx][_level - U];
		}

		if (_length == _level) { // insert into current node
			
			_node->nexthop = _nexthop;
		}
		else {

			if (0 == utility::getBitValue(_prefix, _level)) {
				
				ins(_prefix, _length, _nexthop, _node->lchild, _level + 1, _treeIdx);
			}
			else {

				ins(_prefix, _length, _nexthop, _node->rchild, _level + 1, _treeIdx);
			}
		}

		return;
	}


	/// \brief traverse all binary trees in bridth-first order
	void traverse() {

		uint32 nodeNum = 0;

		for (int i = 0; i < V; ++i) {

			if (nullptr == mRootTable[i]) {
				
				// do nothing
			}
			else {
		
				std::queue<node_type*> queue;

				queue.push(mRootTable[i]);
			
				while(!queue.empty()) {

					nodeNum++;

					if (nullptr != queue.front()->lchild) queue.push(queue.front()->lchild);

					if (nullptr != queue.front()->rchild) queue.push(queue.front()->rchild);
					
					printNode(queue.front());

					queue.pop();
				}
			}
		}

		return;
	}

	void printNode(node_type* _node) {

		std::cerr << "nexthop: " << _node->nexthop << std::endl;

		return;
	}
	
	/// \brief search for _ip
	uint32 search(const ip_type& _ip, std::vector<int>& _trace) {

		// try to find a match in the fast lookup table
		uint32 nexthop1 = 0;

		nexthop1 = ft->search(_ip);

		// try to find a match in the binary trees
		uint32 nexthop2 = 0;

		node_type* node = mRootTable[utility::getBitsValue(_ip, 0, U - 1)]; 

		int level = U;

		while (nullptr != node) {

			_trace.push_back(node->stageidx); // stageidx

			if (node->nexthop != 0) { // contains a valid prefix

				nexthop2 = node->nexthop;
			}
		
			// branch 
			if (0 == utility::getBitValue(_ip, level)) {

				node = node->lchild;
			}		
			else {

				node = node->rchild;
			}

			++level;
		}				

		
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

		while (getline(reqFin, line)) {

			std::vector<int> trace;

			std::stringstream ss(line);

			ss >> prefix;
			
			// generate trace while performing the lookup request
			search(prefix, trace);

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
	
		return;	
	}



	/// \brief delete a prefix in the index
	void del(const ip_type& _prefix, const uint8& _length) {

		if (_length < U) { // process in an alternatively way

			// delete from the fast lookup table
			ft->del(_prefix, _length);
		}
		else { // delete from a binary tree

			del(_prefix, _length, mRootTable[utility::getBitsValue(_prefix, 0, U - 1)], utility::getBitsValue(_prefix, 0, U - 1));
		}

		return;	
	}	

	/// \brief delete a prefix in a binary tree
	///
	/// If _prefix is located in a leaf node, then directly delete the node; otherwise,
	/// clear the nexthop field.
	/// \note After delete a leaf node, its parent may become a leaf node as well. 
	/// If the parent has no prefix, we must recursively delete the parent node as well. 
	void del(const ip_type& _prefix, const uint8& _length, node_type*& _root, const size_t _treeIdx){
	
		std::stack<node_type*> stack;

		node_type* node = _root;

		int level = U; // start from level U

		while (level < _length && nullptr != node) {
	
			stack.push(node);
			
			if (0 == utility::getBitValue(_prefix, level)) {

				node = node->lchild;
			}			
			else {
	
				node = node->rchild;
			}

			++level;
		}

		if (level == _length && nullptr != node) { // find the node

			node->nexthop = 0;

			if (nullptr == node->lchild && nullptr == node->rchild) { // if leaf node, then delete the node

				delete node;

				node = nullptr;

				--mNodeNum[_treeIdx];

				--mLevelNodeNum[_treeIdx][level - U];
					
				// modify parent
				while (!stack.empty()) {

					// modify pointer
					auto top = stack.top();

					if (0 == utility::getBitValue(_prefix, level - 1)) {

						top->lchild = nullptr;
					}			
					else {
						
						top->rchild = nullptr;
					}

					// parent becomes a leaf node, delete it
					if (nullptr == top->lchild && nullptr == top->rchild && top->nexthop == 0) {
			
						delete top;

						top = nullptr;

						stack.pop();

						--mNodeNum[_treeIdx];
	
						--mLevelNodeNum[_treeIdx][level - U - 1];
					}
					else {

						break;
					}

					--level;
				}

				if (stack.empty()) { // stack is empty, then the root is also deleted

					_root = nullptr;
				}
			}
		}
		
		return;
	}

	/// \brief Scatter nodes in binary trees according to a pipeline
	///
	/// Three types of pipelines are considered: linear, cirular and random
	/// 
	void scatterToPipeline(int _pipestyle, int _stagenum = W - U + 1){

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
	void lin(int _stagenum) {
		
		size_t* nodeNumInStage = new size_t[_stagenum];

		for (int i = 0; i < _stagenum; ++i) {

			nodeNumInStage[i] = 0;
		}

		// start numbering from 0 to _stagenum - 1, nodes in a same level are located in the same level
		for (size_t i = 0; i < V; ++i) {

			if (nullptr != mRootTable[i]) {

				std::queue<node_type*> queue;

				mRootTable[i]->stageidx = 0;

				nodeNumInStage[0]++;

				queue.push(mRootTable[i]);

				while (!queue.empty()) {
					
					auto front = queue.front(); 

					if (nullptr != front->lchild) {

						front->lchild->stageidx = front->stageidx + 1; // plus 1

						nodeNumInStage[front->lchild->stageidx]++;
						
						queue.push(front->lchild);
					}								

					if (nullptr != front->rchild) {

						front->rchild->stageidx = front->stageidx + 1; // plus 1

						nodeNumInStage[front->rchild->stageidx]++;

						queue.push(front->rchild);
					}

					queue.pop();
				}
			}
		}

		size_t nodeNumInAllStages = 0;

		for (size_t i = 0; i < _stagenum; ++i) {

			nodeNumInAllStages += nodeNumInStage[i];

			std::cerr << "nodes in stage " << i << ": " << nodeNumInStage[i] << std::endl;
		}

		std::cerr << "nodes in all stages: " << nodeNumInAllStages << std::endl;

		delete[] nodeNumInStage;
	}


	/// \brief Scatter in a random pipeline.
	///
	/// Map nodes into a random pipeline. A random generator is applied to determine the pipe stage to where a node is allocated.
	///
	void ran(int _stagenum) {

		size_t* nodeNumInStage = new size_t[_stagenum];

		for (int i = 0; i < _stagenum; ++i) {

			nodeNumInStage[i] = 0;
		}

		unsigned seed = std::chrono::system_clock::now().time_since_epoch().count();

		std::default_random_engine generator(seed);

		std::uniform_int_distribution<int> distribution(0, _stagenum - 1);

		auto roll = std::bind(distribution, generator);

		for (size_t i = 0; i < V; ++i) {

			if (nullptr != mRootTable[i]) {

				std::queue<node_type*> queue;

				mRootTable[i]->stageidx = roll();

				nodeNumInStage[mRootTable[i]->stageidx]++;

				queue.push(mRootTable[i]);

				while (!queue.empty()) {
					
					auto front = queue.front(); 

					if (nullptr != front->lchild) {

						front->lchild->stageidx = roll(); // roll

						nodeNumInStage[front->lchild->stageidx]++;

						queue.push(front->lchild);
					}								

					if (nullptr != front->rchild) {

						front->rchild->stageidx = roll(); // roll

						nodeNumInStage[front->rchild->stageidx]++;

						queue.push(front->rchild);
					}

					queue.pop();
				}
			}
		}
			
		size_t nodeNumInAllStages = 0;

		for (size_t i = 0; i < _stagenum; ++i) {

			nodeNumInAllStages += nodeNumInStage[i];

			std::cerr << "nodes in stage " << i << ": " << nodeNumInStage[i] << std::endl;
		}

		std::cerr << "nodes in all stages: " << nodeNumInAllStages << std::endl;

		delete[] nodeNumInStage;

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
	void cir(int _stagenum) {

		// step 1: sort binary tries by their size in non-decreasing order
		std::vector<SortElem> vec;

		for (size_t i = 0; i < V; ++i) {

			if (nullptr != mRootTable[i]) {

				vec.push_back(SortElem(mNodeNum[i], i));
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
	
				for (size_t k = 0; k < W - U + 1; ++k) { // put nodes one level per stage

					trycolor[(j + k) % _stagenum] += mLevelNodeNum[treeIdx][k]; // wrap around	
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

			for (int j = 0; j < W - U + 1; ++j) {
		
				colored[(bestStartIdx + j) % _stagenum] += mLevelNodeNum[treeIdx][j]; // wrap around	
			}

			// color nodes in current binary tree
			node_type* root = mRootTable[treeIdx];

			std::queue<node_type*> queue;

			root->stageidx = bestStartIdx;

			queue.push(root);

			while (!queue.empty()) {

				auto front = queue.front();

				if (nullptr != front->lchild) {

					front->lchild->stageidx = (front->stageidx + 1) % _stagenum; // wrap around

					queue.push(front->lchild);
				}

				if (nullptr != front->rchild) {

					front->rchild->stageidx = (front->stageidx + 1) % _stagenum; // wrap around

					queue.push(front->rchild);
				}

				queue.pop();
			}
		} 

		size_t nodeNumInAllStages = 0;

		for (int i = 0; i < _stagenum; ++i) {
			
			nodeNumInAllStages += colored[i];

			std::cerr << "nodes in stage " << i << ": " << colored[i] << std::endl;
		}

		std::cerr << "nodes in all stages: " << nodeNumInAllStages << std::endl;

		delete[] colored;

		delete[] trycolor;
	}


	/// \brief Compute and return number of nodes at a specific level in all the binary trees.
	size_t getLevelNodeNum(const int _level) {

		size_t sum = 0;

		for (size_t i = 0; i < V; ++i) {
		
			sum += mLevelNodeNum[i][_level];
		}
		
		return sum;
	}
};

#endif // _BTree_H
