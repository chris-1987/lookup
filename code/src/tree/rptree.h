#ifndef _RPTREE_H
#define _RPTREE_H

///////////////////////////////////////////////////////////////////////////////////////////////
/// Copyright (c) 2016, Sun Yat-sen University
/// All rights reserved
/// \file rptree.h
/// \brief Definition of an IP lookup index based on prefix tree.
///
/// The lookup index consists of three parts: a fast lookup table that contains route information for prefixes shorter than U bits,
/// a table that consists of 2^U entries and each entry stores a pointer to the root of a prefix tree,
/// and a set of binary trees.
///
/// \author Yi Wu
/// \date 2016.11
///////////////////////////////////////////////////////////////////////////////////////////////

#include "../common/common.h"
#include "../common/utility.h"

#include "fasttable.h"

#include <queue>
#include <cmath>
#include <random>
#include <algorithm>

/// \brief Nodes in a prefix tree.
///
/// Each node in the tree contains two pointers, a prefix field and a nexthop field. 
/// Prefies stored in level-$k$ is no shorter than $k$.
template<int W>
struct PNode{

	typedef typename choose_ip_type<W>::ip_type ip_type;

	PNode* lchild; ///< left child

	PNode* rchild; ///< right child

	ip_type prefix; ///< prefix

	uint8 length; ///< length of prefix

	uint32 nexthop; ///< next hop	

	int stageidx; ///< location in pipe stage

	/// \brief ctor
	PNode() : lchild(nullptr), rchild(nullptr), prefix(0), length(0), nexthop(0) {}

};

/// \brief Build and update the index.
/// 
/// Prefixes shorter than U bits are stored in a fast lookup table.
/// Prefixes not shorter than U bits are stored in the PT forest.
/// 
/// \param W 32 or 128 for IPv4 or IPv6, respectively.
/// \param U A threshold for classifying shorter and longer prefixes (< U and >= U)
/// \param V Number of prefix trees at large.
template<int W, int U, size_t V = static_cast<size_t>(pow(2, U))>
class RPTree{
private:

	typedef typename choose_ip_type<W>::ip_type ip_type;

	typedef PNode<W> node_type;

	node_type* mRootTable[V]; ///< pointers to a forest of prefix tree

	uint32 mNodeNum[V]; ///< number of nodes in each prefix tree

	uint32 mLevelNodeNum[V][W - U + 1]; ///< number of nodes in each level of a prefix tree

	uint32 mTotalNodeNum; ///< number of nodes in total 

	FastTable<W, U - 1> ft;

public:

	/// \brief default ctor
	RPTree() {

		initializeParameters();
	}

	/// \brief initialize parameters
	void initializeParameters() {

		mTotalNodeNum = 0;

		for (size_t i = 0; i < V; ++i) {

			mRootTable[i] = nullptr;
		}

		for (size_t i = 0; i < V; ++i) {

			mNodeNum[i] = 0;
		}

		for (size_t i = 0; i < V; ++i) {

			for (int j = 0; j < W - U + 1; ++j) {

				mLevelNodeNum[i][j] = 0;
			}
		}

		ft = FastTable<W, U - 1>();	
	}
	
	/// \brief copy ctor, disabled
	RPTree(const RPTree& _pt) = delete;

	/// \brief assignment op, disabled
	RPTree& operator= (const RPTree&) = delete;

	/// \brief dtor
	~RPTree() {

		clear();
	}

	/// \brief clear the index
	void clear() {

		for(size_t i = 0; i < V; ++i) {

			if (nullptr != mRootTable[i]) {

				destroy(i);

				mRootTable[i] = nullptr;
			}
		}
	}


	/// \brief Build the index.
	void build(const std::string& _fn) {

		// clear old index if there exists any
		clear();

		// initialize 
		initializeParameters();

		// insert prefixes into ptree one by one
		std::ifstream fin(_fn, std::ios_base::binary);

		std::string line;

		ip_type prefix;

		uint8 length;

		uint32 nexthop;

		while(getline(fin, line)) {

			// retrieve prefix and length
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

		//traverse();	

		return;
	}


	void destroy(size_t _idx) {

		if (nullptr == mRootTable[_idx]) {

			return;
		}

		node_type* node = mRootTable[_idx];

		std::queue<node_type*> queue;

		queue.push(node);

		while (!queue.empty()) {
		
			auto front = queue.front();

			if (nullptr != queue.front()->lchild) queue.push(queue.front()->lchild);

			if (nullptr != queue.front()->rchild) queue.push(queue.front()->rchild);

			delete front;

			front = nullptr;

			queue.pop();
		}

		return;
	}

	/// \brief Report the collected information.
	void report() {

		mTotalNodeNum = 0;

		for (size_t i = 0; i < V; ++i) {

			mTotalNodeNum += mNodeNum[i];
		}

		std::cerr << "node num in total: " << mTotalNodeNum << std::endl;
	}


	/// \brief Insert a prefix into the index.
	void ins(const ip_type& _prefix, const uint8& _length, const uint32& _nexthop) {

		if (_length < U) { // insert into the fast table

			ft.ins(_prefix, _length, _nexthop);
		}
		else { // insert into the PT forest

			ins(_prefix, _length, _nexthop, mRootTable[utility::getBitsValue(_prefix, 0, U - 1)], U, utility::getBitsValue(_prefix, 0, U - 1));
		}

		return;
	}

	/// \brief Insert a prefix into the PT forest.
	void ins(const ip_type& _prefix, const uint8& _length, const uint32& _nexthop, node_type*& _node, const int _level, const size_t _treeIdx) {

		if (nullptr == _node) { // create a new node and insert the prefix into the node

			// create a node
			_node = new node_type();

			++mNodeNum[_treeIdx];

			++mLevelNodeNum[_treeIdx][_level - U];

			// insert the prefix
			_node->prefix = _prefix;

			_node->length = _length;
		
			_node->nexthop = _nexthop;
			
			return;
		}	
		else { // current node is not null, insert prefix into current node if required, otherwise, insert the prefix into a higher level
			
			if (_length == _level) { // _prefix must be inserted into current node
				
				if (_node->length > _level) { // check if the prefix is inserted repeatedly
	
					// cache the prefix in _node 
					ip_type prefix = _node->prefix;
		
					uint8 length = _node->length;
		
					uint32 nexthop = _node->nexthop;
		
					// insert _prefix into _node
					_node->prefix = _prefix;
	
					_node->length = _length;
		
					_node->nexthop = _nexthop;
	
					// recursively insert the cached prefix into higher levels
					if (0 == utility::getBitValue(prefix, _level)) { 
		
						ins(prefix, length, nexthop, _node->lchild, _level + 1, _treeIdx);
					}
					else {
		
						ins(prefix, length, nexthop, _node->rchild, _level + 1, _treeIdx);
					}
				}
				else { // the prefix in current node must be the same as the one to be inserted

					// do nothing
				}
			}	
			else { // recursively insert _prefix into higher levels
	
				if (0 == utility::getBitValue(_prefix, _level)) {
	
					ins(_prefix, _length, _nexthop, _node->lchild, _level + 1, _treeIdx);
				}
				else {

					ins(_prefix, _length, _nexthop, _node->rchild, _level + 1, _treeIdx);
				}
			}
		}

		return;
	}

	void traverse() {

		uint32 nodeNum = 0; // for test

		for (size_t i = 0; i < V; ++i) {

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

				//	printNode(queue.front());

					queue.pop();
				}
			}
		}

		std::cerr << "traversed node num: " << nodeNum << std::endl;

		return;
	}

	/// \brief print node information
	void printNode(node_type* _node) {

		std::cerr << "prefix: " << _node->prefix << " length: " << _node->length << "nexthop: " << _node->nexthop << std::endl;

		return;
	}


	/// \brief Search LPM for target IP address.
	///
	/// Record trace in the vector.
	///
	uint32 search(const ip_type& _ip, std::vector<int>& _trace) {

		// try to find a match in the fast lookup table
		uint32 nexthop1 = 0;

		nexthop1 = ft.search(_ip);

		// try to find a match in the forest of prefix trees
		uint32 nexthop2 = 0;

		node_type* node = mRootTable[utility::getBitsValue(_ip, 0, U - 1)];

		int level = U;

		int bestLength = 0;

		while (nullptr != node) {

			_trace.push_back(node->stageidx);

			if (utility::getBitsValue(_ip, 0, node->length - 1) == utility::getBitsValue(node->prefix, 0, node->length - 1)) {

				if (bestLength < node->length) {

					bestLength = node->length;

					nexthop2 = node->nexthop;
				}
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

		// return a match in the forest of prefix trees if any
		if (0 != nexthop2) return nexthop2;

		// return a match in the fast lookup table if any
		if (0 != nexthop1) return nexthop1;

		return 0;
	}

	/// \brief generate lookup trace for simulation
	void generateTrace (const std::string& _reqFile, const std::string& _traceFile, const uint32 _stageNum){

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

		std::cerr << "workload: " << LAMBDA * BURSTSIZE * avgSearchDepth / _stageNum << std::endl;

		std::cerr << "average search depth: " << avgSearchDepth << std::endl;		

		return;	
	}

	/// \brief Delete a prefix in the index.
	void del(const ip_type& _prefix, const uint8& _length) {

		if (_length < U) {
		
			ft.del(_prefix, _length);
		}
		else {

			del(_prefix, _length, mRootTable[utility::getBitsValue(_prefix, 0, U - 1)], U, utility::getBitsValue(_prefix, 0, U - 1));
		}

		return;
	}

	/// \brief Delete a prefix in the PT forest.
	void del(const ip_type& _prefix, const uint8& _length, node_type*& _node, const int _level, const size_t _treeIdx) {

		if (nullptr == _node) return; // find nothing	

		if (_node->prefix == _prefix && _length == _node->length) { // prefix is found
			
			if (nullptr == _node->lchild && nullptr == _node->rchild) { // a leaf node, delete it directly
				
				delete _node;

				_node = nullptr; // reset parent's child pointer

				--mNodeNum[_treeIdx];

				--mLevelNodeNum[_treeIdx][_level - U];

				return;
			}	
			else { // not a leaf node, substitute the content of current node with the leaf node
			
				node_type* pnode = _node; // parent node of leaf node

				node_type* cnode = nullptr; // child node 

				bool isLeftBranch = true; // true if branch to left

				// at least has a child
				if (nullptr != pnode->lchild) {

					cnode = pnode->lchild;

					isLeftBranch = true;
				}
				else {

					cnode = pnode->rchild;

					isLeftBranch = false;
				}
				
				int child_level = _level + 1;

				// find leaf descendant
				while (nullptr != cnode->lchild || nullptr != cnode->rchild) {

					if (nullptr != cnode->lchild) {

						pnode = cnode;

						cnode = cnode->lchild;

						isLeftBranch = true;
					}
					else {

						pnode = cnode;

						cnode = cnode->rchild;

						isLeftBranch = false;
					}

					child_level++;
				}	
					
	
				// replace content of _node by that of those in cnode
				_node->prefix = cnode->prefix;

				_node->length = cnode->length;

				_node->nexthop = cnode->nexthop;

				// reset child pointer of pnode
				if (isLeftBranch) {

					pnode->lchild = nullptr;
				}
				else {

					pnode->rchild = nullptr;
				}

				// delete cnode
				delete cnode;

				cnode = nullptr;

				--mNodeNum[_treeIdx];

				--mLevelNodeNum[_treeIdx][child_level];

				return;
			}
		}
		else { // attempt to find the prefix in higher levels

			if (0 == utility::getBitValue(_prefix, _level)) {

				del(_prefix, _length, _node->lchild, _level + 1, _treeIdx);
			} 
			else {

				del(_prefix, _length, _node->rchild, _level + 1, _treeIdx);
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

						front->lchild->stageidx = front->stageidx + 1; // increment 1

						nodeNumInStage[front->lchild->stageidx]++; 
						
						queue.push(front->lchild);
					}								

					if (nullptr != front->rchild) {

						front->rchild->stageidx = front->stageidx + 1; // increment 1

						nodeNumInStage[front->rchild->stageidx]++;

						queue.push(front->rchild);
					}

					queue.pop();
				}
			}
		}

		size_t nodeNumInAllStages = 0;

		// compute total number of nodes 
		for (size_t i = 0; i < _stagenum; ++i) {

			nodeNumInAllStages += nodeNumInStage[i];
		}

		std::cerr << "nodes in all stages: " << nodeNumInAllStages << std::endl;

		// collect normalized memory block utilization
		double min_ratio = std::numeric_limits<double>::max(), max_ratio = 0.0, mean_ratio = 0.0, cur_ratio;

		for (size_t i = 0; i < _stagenum; ++i) {
	
			cur_ratio = (double)nodeNumInStage[i] / nodeNumInAllStages;

			mean_ratio += cur_ratio;

			if (min_ratio > cur_ratio) min_ratio = cur_ratio;

			if (max_ratio < cur_ratio) max_ratio = cur_ratio;
			
			std::cerr << "nodes in stage " << i << ": " << nodeNumInStage[i] << " ratio: " << (double)cur_ratio << std::endl;
		}

		mean_ratio /= _stagenum;
	
		std::cerr << "min ratio: " << min_ratio << " max ratio: " << max_ratio << " mean ratio: " << mean_ratio << std::endl;	

		delete[] nodeNumInStage;

		return;
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
	
		// collect information	
		size_t nodeNumInAllStages = 0;

		// compute total number of nodes 
		for (size_t i = 0; i < _stagenum; ++i) {

			nodeNumInAllStages += nodeNumInStage[i];
		}

		std::cerr << "nodes in all stages: " << nodeNumInAllStages << std::endl;

		// collect normalized memory block utilization
		double min_ratio = std::numeric_limits<double>::max(), max_ratio = 0.0, mean_ratio = 0.0, cur_ratio;

		for (size_t i = 0; i < _stagenum; ++i) {
	
			cur_ratio = (double)nodeNumInStage[i] / nodeNumInAllStages;

			mean_ratio += cur_ratio;

			if (min_ratio > cur_ratio) min_ratio = cur_ratio;

			if (max_ratio < cur_ratio) max_ratio = cur_ratio;
			
			std::cerr << "nodes in stage " << i << ": " << nodeNumInStage[i] << " ratio: " << (double)cur_ratio << std::endl;
		}

		mean_ratio /= _stagenum;
	
		std::cerr << "min ratio: " << min_ratio << " max ratio: " << max_ratio << " mean ratio: " << mean_ratio << std::endl;	

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

		// collect information
		size_t nodeNumInAllStages = 0;

		// compute total number of nodes 
		for (size_t i = 0; i < _stagenum; ++i) {

			nodeNumInAllStages += colored[i];
		}

		std::cerr << "nodes in all stages: " << nodeNumInAllStages << std::endl;

		// collect normalized memory block utilization
		double min_ratio = std::numeric_limits<double>::max(), max_ratio = 0.0, mean_ratio = 0.0, cur_ratio;

		for (size_t i = 0; i < _stagenum; ++i) {
	
			cur_ratio = (double)colored[i] / nodeNumInAllStages;

			mean_ratio += cur_ratio;

			if (min_ratio > cur_ratio) min_ratio = cur_ratio;

			if (max_ratio < cur_ratio) max_ratio = cur_ratio;
			
			std::cerr << "nodes in stage " << i << ": " << colored[i] << " ratio: " << (double)cur_ratio << std::endl;
		}

		mean_ratio /= _stagenum;
	
		std::cerr << "min ratio: " << min_ratio << " max ratio: " << max_ratio << " mean ratio: " << mean_ratio << std::endl;	

		delete[] colored;

		delete[] trycolor;
	}


	/// \brief update
	void update(const std::string& _fn, int _pipestyle, int _stagenum = W - U + 1) {

		size_t withdrawnum = 0;

		size_t announcenum = 0;

		std::ifstream fin(_fn, std::ios_base::binary);

		std::string line;

		ip_type prefix;

		uint8 length;

		uint32 nexthop;

		bool isAnnounce;

		// randome number generator
		unsigned seed = std::chrono::system_clock::now().time_since_epoch().count();
		
		std::default_random_engine generator(seed);
		
		std::uniform_int_distribution<int> distribution(0, _stagenum - 1);

		while (getline(fin, line)) {

			// retrieve prefix and length
			utility::retrieveInfo(line, prefix, length, isAnnounce);

			if (false == isAnnounce) {

				++withdrawnum;

				del(prefix, length);
			}
			else {

				++announcenum;

				nexthop = length;

				ins(prefix, length, nexthop, _pipestyle, generator, distribution, _stagenum);
			}
		}

		reportNodeNumInStage(_stagenum);

		std::cerr << "withdraw num: " << withdrawnum << " announce num: " << announcenum << std::endl;

		return;
	}

	/// \brief for update, insert into index
	void ins(const ip_type& _prefix, const uint8& _length, const uint32& _nexthop, const int _pipestyle, std::default_random_engine& _generator, std::uniform_int_distribution<int>& _distribution, const int _stagenum) {

		if (_length < U) { // insert into the fast table

			ft.ins(_prefix, _length, _nexthop);
		}
		else { // insert into the PT forest

			ins(_prefix, _length, _nexthop, mRootTable[utility::getBitsValue(_prefix, 0, U - 1)], U, utility::getBitsValue(_prefix, 0, U - 1), true, _pipestyle, std::numeric_limits<int>::max(), _generator, _distribution, _stagenum);
		}

		return;

	}

	/// \brief for update, insert into a prefix tree
	void ins(const ip_type& _prefix, const uint8& _length, const uint32& _nexthop, node_type*& _node, const int _level, const size_t _treeIdx, const bool _isRoot, const int _pipestyle, const int _parentStageidx, std::default_random_engine& _generator, std::uniform_int_distribution<int>& _distribution, const int _stagenum) {

		if (nullptr == _node) { // create a new node and insert the prefix into the node

			_node = new node_type();

			if (_isRoot) {

				switch(_pipestyle) {

				case 0: // linear pipeline, root is located at the first stage
					_node->stageidx = 0; break;

				case 1:
				case 2: // circular and randome pipeline, root is randomly allocated into a stage

					_node->stageidx = _distribution(_generator); break;
				}
			}
			else { // not root node

				switch(_pipestyle) {

				case 1: // random
					_node->stageidx = _distribution(_generator); break;

				case 0:
				case 2:
					_node->stageidx = (_parentStageidx + 1) % _stagenum; break;
				}
			}

			++mNodeNum[_treeIdx];

			++mLevelNodeNum[_treeIdx][_level - U];

			// insert the prefix
			_node->prefix = _prefix;

			_node->length = _length;
		
			_node->nexthop = _nexthop;
			
			return;
		}	
		else { // current node is not null, insert prefix into current node if required, otherwise, insert the prefix into a higher level
			
			if (_length == _level) { // _prefix must be inserted into current node
				
				if (_node->length > _level) { // check if the prefix is inserted repeatedly
	
					// cache the prefix in _node 
					ip_type prefix = _node->prefix;
		
					uint8 length = _node->length;
		
					uint32 nexthop = _node->nexthop;
		
					// insert _prefix into _node
					_node->prefix = _prefix;
	
					_node->length = _length;
		
					_node->nexthop = _nexthop;
	
					// recursively insert the cached prefix into higher levels
					if (0 == utility::getBitValue(prefix, _level)) { 
		
						ins(prefix, length, nexthop, _node->lchild, _level + 1, _treeIdx, false, _pipestyle, _node->stageidx, _generator, _distribution, _stagenum);
					}
					else {
		
						ins(prefix, length, nexthop, _node->rchild, _level + 1, _treeIdx, false, _pipestyle, _node->stageidx, _generator, _distribution, _stagenum);
					}
				}
				else { // the prefix in current node must be the same as the one to be inserted

					// do nothing
				}
			}	
			else { // recursively insert _prefix into higher levels
	
				if (0 == utility::getBitValue(_prefix, _level)) {
	
					ins(_prefix, _length, _nexthop, _node->lchild, _level + 1, _treeIdx, false, _pipestyle, _node->stageidx, _generator, _distribution, _stagenum);
				}
				else {

					ins(_prefix, _length, _nexthop, _node->rchild, _level + 1, _treeIdx, false, _pipestyle, _node->stageidx, _generator, _distribution, _stagenum);
				}
			}
		}

		return;
	}

	/// \brief report number of nodes in each stage
	void reportNodeNumInStage(int _stagenum) {

		size_t* nodeNumInStage = new size_t[_stagenum];

		for (int i = 0; i < _stagenum; ++i) {

			nodeNumInStage[i] = 0;
		}

		for (size_t i = 0; i < V; ++i) {

			if (nullptr == mRootTable[i]) {

				// do nothing
			}
			else {

				std::queue<node_type*> queue;

				queue.push(mRootTable[i]);

				while(!queue.empty()) {

					nodeNumInStage[queue.front()->stageidx]++;

					if (nullptr != queue.front()->lchild) queue.push(queue.front()->lchild);

					if (nullptr != queue.front()->rchild) queue.push(queue.front()->rchild);

					queue.pop();
				}
			}
		}
		
		size_t nodeNumInAllStages = 0;

		for (int i = 0; i < _stagenum; ++i) {

			nodeNumInAllStages += nodeNumInStage[i];

			std::cerr << "nodes in stage " << i << ": " << nodeNumInStage[i] << std::endl;
		}
		
		std::cerr << "nodes in all stages: " << nodeNumInAllStages << std::endl;

		delete[] nodeNumInStage;

		return;
	}
};

#endif // _PT_H

