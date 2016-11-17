#ifndef _SCHEDULER_H
#define _SCHEDULER_H


//////////////////////////////////////////////
/// Copyright (c) 2016, Sun Yat-sen University
/// All rights reserved.
///
/// @file scheduler.h
/// @brief definition of scheduler classes
/// 
/// Define xxx
/// @author Yi WuvD
//////////////////////////////////////////////


#include "common.h"
#include "utility.h"

/// Scheduler, abstract class
///
/// SChedule search/update requests
template<int Pattern, int Distribution, int Pipenum>
class Scheduler{

private:

	Scheduler(const Scheduler&) = delete;

	Scheduler& operator= (const Scheduler&) = delete;
	
protected:

	Scheduler() {}

	virtual ~Scheduler() {}

	/// schedule once
	virtual void schedule (std::string & _fn) = 0;

	/// print info
	virtual void print() = 0;
};


/// Schedule search requests
///
/// Schedule requests following one of three patterns(FCFS, PIM & iSLIP) and one of two distributions (Bernoulli & Poisson)

template<int Pattern, int Distribution, int Pipenum>
class SearchScheduler : public Scheduler<Pattern, Distribution, Pipenum> {

private:

	SearchScheduler(const SearchScheduler&) = delete;

	SearchScheduler& operator= (const SearchScheduler&) = delete;

public:

	/// ctor
	SearchScheduler() : Scheduler<Pattern, Distribution, Pipenum>() {}


	/// schedule once
	void schedule(std::string & _fn) {

		utility::printMsg(std::string("filename ").append(_fn) ,0);
	}


	/// print info
	void print() {
		
		utility::printMsg("searchScheduler", 0);
		
	}

	/// dtor
	virtual ~SearchScheduler() {}
};


/// Schedule update requests
///
/// Schedule requests following one of three patterns(FCFS, PIM & iSLIP) and one of two distributions (Bernoulli & Poisson)

template<int Pattern, int Distribution, int Pipenum>
class UpdateScheduler : public Scheduler<Pattern, Distribution, Pipenum> {

private:

	UpdateScheduler(const UpdateScheduler&) = delete;

	UpdateScheduler& operator= (const UpdateScheduler&) = delete;

public:

	/// ctor
	UpdateScheduler() : Scheduler<Pattern, Distribution, Pipenum>() {}

	/// schedule once
	void schedule(std::string & _fn) {

		utility::printMsg(std::string("filename ").append(_fn) ,0);
	}
	
	/// print info
	void print() {

		utility::printMsg("updateScheduler", 0);

	}

	/// dtor
	virtual ~UpdateScheduler() {}
};

#endif 




