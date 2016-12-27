#ifndef CIRSCHED_H
#define CIRSCHED_H

////////////////////////////////////////////////////////////
/// Copyright (c) 2016, Sun Yat-sen University,
/// All rights reserved
/// \file cirsched.h
/// \brief Definition of the scheduler for a circular pipeline.
///
/// Schedule lookup tasks in a circular pipeline.
/// Please refer to "CAMP: fast and efficient
/// IP lookup architecture" fore more details.
///
/// \author Yi Wu
/// \date 2016.11
///////////////////////////////////////////////////////////

#include "../common/common.h"

#include <string>
#include <sstream>
#include <queue>

/// \brief Schedule lookup requests in a circular pipeline
///
/// In a circular pipeline, a search request may start searching from any pipe stage (dependent on the location of the target subtrie) 
/// and wrap around the pipeline to finsh the task.
///
/// \param W at most W search steps for a request
/// \param K number of pipe stages
/// \param S size of request queue per stage
///
/// \note K >= W. That is, the number of pipe stages is no fewer than the height of the tree (the number of search steps)
template<int W, int K, int S = QUEUESIZE>
class CirSched{

private:
	
	size_t mSlotNum; ///< number of time slots in total

	size_t mRequestNum; ///< number of requests

	size_t mBusySlotNumStage[K]; ///< number of busy time slots for each pipe stage

	size_t mBusySlotNumAvg; ///< average number of busy time slots over all pipe stages

	int mMaxQueueLength[K]; ///< maximum lengths of the request queues

	double mAvgQueueLength[K]; ///< average lengths of the request queues

public:

	/// \brief structure of request
	struct Request{

		int stagelist[W];

		int stepnum;

		int curstep;

		Request() : curstep(0) {}
	};

	/// \brief structure of request queue 
	struct ReqQue{

		std::vector<Request*> mData; ///< payload

		/// \brief check if queue is empty
		bool isEmpty() {

			return 0 == mData.size();
		}

		/// \brief check if queue is overflow
		bool isOverflow() {

			return S < mData.size();
		}

		/// \brief append a request to the end of the queue
		void append(Request* _req) {

			mData.push_back(_req);

			return;
		}

		/// \brief remove the head of request (which is assigned to the corresponding pipe stage)
		void removeHead() {

			mData.erase(mData.begin());

			return;
		}

		Request* getHead() {

			return mData[0];
		}
	};

	ReqQue mReqQue[K]; ///< queues of requests, one queue per stage

	/// \brief structure of stage
	struct Stage{

		Request* mReq;

		Stage() {

			mReq = nullptr;
		}

		/// \brief check if no request occupies the stage
		bool isEmpty() {

			return mReq == nullptr;
		}

		/// \brief check if request is finished
		bool isFinished() {

			return mReq->stepnum == mReq->curstep;
		}

		/// \brief one step further
		void toNext() {

			mReq->curstep++;

			return;
		}

		/// \brief dispatch the completed request
		void dispatch() {

			delete mReq;

			mReq = nullptr;

			return;
		}

		/// \brief get request handler
		Request* getReq() {

			return mReq;
		}

		/// \brief set request
		void setReq(Request* _req) {

			mReq = _req;

			return;
		}
	};

	Stage mStage[K];

	CirSched() : mSlotNum(0), mRequestNum(0), mBusySlotNumAvg(0) {

		for (int i = 0; i < K; ++i) {

			mBusySlotNumStage[i] = 0;
		}

		for (int i = 0; i < K; ++i) {

			mMaxQueueLength[i] = 0;
		}

		for (int i = 0; i < K; ++i) {

			mAvgQueueLength[i] = 0;
		}
	}


	bool isAllQueueEmpty() {

		for (int i = 0; i < K; ++i) {

			if (!mReqQue[i].isEmpty()) {

				return false;
			}
		}

		return true;
	}

	/// \brief execute 
	void execute() {
	
		// execute a search step
		for (int i = 0; i < K; ++i) {

			if (mStage[i].isEmpty()) { // current stage has no request in execution

				if (!mReqQue[i].isEmpty()) { // try to forward the head request in the corresponding queue to the stage

					mStage[i].setReq(mReqQue[i].getHead());
	
					mReqQue[i].removeHead(); // remove the request from the queue
				}
			}

			if (!mStage[i].isEmpty()) { // current stage has a request in execution

				mBusySlotNumStage[i]++;
	
				// after performing a search step, go to next
				mStage[i].toNext();
			}
		}		

		return;
	}

	/// \brief a run of executing search requests
	void searchRun(const std::string& _traceFile) {

		// step 1: count number of traces
		std::ifstream fin(_traceFile, std::ios_base::binary);

		std::string line;

		size_t linenum = 0;

		while (getline(fin, line)) linenum++;

		fin.close();

		mRequestNum = linenum;

		std::cerr << "linenum: " << linenum << std::endl;

		// step 2: scheduling
		// packet arrivals submit to bernoulli distribution
		unsigned seed = std::chrono::system_clock::now().time_since_epoch().count();

		std::default_random_engine generator(seed);

		std::bernoulli_distribution distribution(LAMBDA);

		auto genreq = std::bind(distribution, generator);

		// start simulation
		std::ifstream fin2(_traceFile, std::ios_base::binary);

		std::string line2;

		mSlotNum = 0;
		
		while (linenum > 0 || !isAllQueueEmpty()) {

			mSlotNum++;

			if (genreq()) {

				for (int i = 0; i < BURSTSIZE; ++i) {

					if (linenum > 0) {

						getline(fin2, line2);

						std::stringstream ss(line2);

						Request* newReq = new Request();

						ss >> newReq->stepnum;

						for (int j = 0; j < newReq->stepnum; ++j) {

							ss >> newReq->stagelist[j];
						}

			//			std::cerr << "\nnewReq: " << newReq->stepnum << " ";
			//			for (int j = 0; j < newReq->stepnum; ++j) {
			//				std::cerr << newReq->stagelist[j]<< " ";
			//			}
			//			std::cerr << std::endl;

						if (0 == newReq->stepnum) { // no need to be further processed

							delete newReq;

							newReq = nullptr;
						}
						else { // queue the request in the stage at which the target root node is located

							int startStage = newReq->stagelist[0];

							mReqQue[startStage].append(newReq);

							// collect max queue length
							if (mReqQue[startStage].mData.size() > mMaxQueueLength[startStage]) {

								mMaxQueueLength[startStage] = mReqQue[startStage].mData.size();
							}
						}

						--linenum;
					}
				}
			}

			// collect average queue length	
			for (int i = 0; i < K; ++i) {

				mAvgQueueLength[i] += mReqQue[i].mData.size();
			}

			// execute a search request
			execute();

			// dispatch completed requests
			for (int i = 0; i < K; ++i) {

				if (!mStage[i].isEmpty() && mStage[i].isFinished()) {
	
					mStage[i].dispatch();
				}
			}
	
			// forward the request in each stage to the downstream stage
			Request* preReq = mStage[K - 1].getReq();
	
			Request* curReq;
	
			for (int i = 0; i < K; ++i) {
	
				curReq = mStage[i].getReq();
	
				mStage[i].setReq(preReq);
	
				preReq = curReq;			
			}		
		}
	
		searchReport();

		return;
	}

	/// \breif print search report
	void searchReport() {

		std::cerr << "lamda: " << LAMBDA << std::endl;

		std::cerr << "burst size: " << BURSTSIZE << std::endl;

		std::cerr << "queue size: " << QUEUESIZE << std::endl; 

		std::cerr << "request num: " << mRequestNum << std::endl;

		std::cerr << "slot num: " << mSlotNum << std::endl;

		mBusySlotNumAvg = 0;

		for (int i = 0; i < K; ++i) {

			std::cerr << "busy slot for stage " << i << ": " <<  mBusySlotNumStage[i] << std::endl;			

			mBusySlotNumAvg += mBusySlotNumStage[i];
		}

		mBusySlotNumAvg /= K;

		std::cerr << "busy slot num in average (per stage): " << mBusySlotNumAvg << std::endl;

		std::cerr << "usage ratio: (busy slot/ total slot): " << static_cast<double>(mBusySlotNumAvg) / mSlotNum << std::endl;

		std::cerr << "queue length for each queue: " << std::endl;

		uint32 total_max_queue_length = 0;

		for (int i = 0; i < K; ++i) {

			std::cerr << i << "'s queue--avg (per slot): " << mAvgQueueLength[i] / mSlotNum << " max: " << mMaxQueueLength[i] << std::endl;

			total_max_queue_length += mMaxQueueLength[i];
		}

		std::cerr << "total max queue length: " << total_max_queue_length << std::endl;

		return;
	}

	
};





#endif
