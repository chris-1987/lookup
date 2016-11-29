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

#include <string>
#include <sstream>
#include <queue>

static const double LAMBDA = 0.7; // packet arriving probability

static const int BURSTSIZE = 1; // number of packets arriving at the same time

static const int QUEUESIZE = 128; // size of queue


/// \brief Schedule lookup requests in a circular pipeline
///
/// In a circular pipeline, a search request may start searching from any pipe stage (dependent on the location of the target subtrie) 
/// and wrap around the pipeline to finsh the task.
///
/// \param W at most W search steps for a request
/// \param K number of pipe stages
/// \param S size of request queue per stage
///
/// \note K >= W
template<int W, int K, int S = QUEUESIZE>
class CirSched{

private:
	
	size_t mSlotNum; ///< number of time slots in total

	size_t mRequestNum; ///< number of requests

	size_t mBusySlotNumStage[K]; ///< number of busy time slots for each pipe stage

	size_t mBusySlotNumAvg; ///< average number of busy time slots over all pipe stages

	bool mIsUsed[K]; ///< a pipe stage is occupied during the time slot

	int mMaxQueueLength; ///< maximum length of the request queue

	int mAvgQueueLength; ///< average length of the request queue

public:

	/// \brief structure of request
	struct Request{

		int stagelist[K];

		int stepnum;

		int curstep;

		Request() : curstep(0) {}

		bool isFinished() {

			return stepnum == curstep;
		}

		void toNext() {

			curstep++;
		}
	};

	/// \brief structure of pip
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
	};

	ReqQue mReqQues[K]; ///< queues of requests, one queue per stage

	CirSched() : mSlotNum(0), mRequestNum(0), mBusySlotNumAvg(0), mMaxQueueLength(0), mAvgQueueLength(0) {

		for (int i = 0; i < K; ++i) {

			mBusySlotNumStage[i] = 0;
		}

		for (int i = 0; i < K; ++i) {

			mIsUsed[i] = false;
		}
	}

	/// \brief execute 
	void execute() {

		for (int i = 0; i < K; ++i) {

			mIsUsed[i] = false;
		}

		//
		
for (int i = 0; i < K; ++i) {

			if (mStages[i].isEmpty() && !mReqQue[i].isEmpty()) {

				mStages[i].req = mReqQue.mData[0];
			}

			if (!mStages[i].isEmpty()) {

				mStages[i].req->toNext();
			}
		}

		/// wrap around
		Request *preReq, *curReq;

		preReq = mStages[K - 1].req;

		for (int i = 0; i < K; ++i) {
			
			curReq = mStages[i].req;

			mStages[i].req = preReq;

			preReq = curReq;	
		}

		// dispatch completed requests
		for (int i = 0; i < K; ++i) {

			if (nullptr != mStages[i].req && mStages[i].req->isFinished()) {

				delete mStages[i].req;
			
				mStages[i].req = nullptr;
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

		// step 2: scheduling
		// packet arrivals submit to bernoulli distribution
		unsigned seed = std::chrono::system_clock::now().time_since_epoch().count();

		std::default_random_engine generator(seed);

		std::bernoulli_distribution distribution(LAMBDA);

		auto genreq = std::bind(distribution, generator);

		// start simulation
		std::ifstream fin2(_traceFile, std::ios_base:;binary);

		std::string line2;

		mSlotNum = 0;

		while (linenum > 0 || !mReqQue.isEmpty()) {

			mSlotNum++;

			if (genreq()) {

				for (int i = 0; i < BURSTSIZE; ++i) {

					if (linenum > 0) {

						getline(fin2, line2);

						std::stringstream ss(line2);

						Request* newReq = new Request();

						ss >> newReq->stenum;

						for (int j = 0; j < newReq->stepnum; ++j) {

							ss >> newReq->stagelist[j];
						}


						if (0 == newReq->stepnum) {

							delete newReq;
						}
						else {

							int startStage = newReq->stagelist[0];

							mReqQue[startStage].append(newReq);

							if (mReqQue[startStage].mData.size() > mMaxQueueLength[startStage]) {

								mMaxQueueLength[startStage] = mReqQue[startStage].mData.size();
							}
						}

						--linenum;
					}
				}
			}

			for (int i = 0; i < K; ++i) {

				mAvgQueueLength[i] += mReqQue[i].mData.size();
			}

			dispatch();
		}

		searchReport();
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

		for (int i = 0; i < K; ++i) {

			std::cerr << i << "'s queue\tavg (per slot): " << mAvgQueueLength[i] << " max: " << mMaxQueueLength[i] << std::endl;
		}

		return;
	}

	
};





#endif
