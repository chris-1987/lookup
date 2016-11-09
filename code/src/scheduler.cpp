#include "scheduler.h"

int main(int argc, char** argv){

	
	std::string str = std::string("lala");

	SearchScheduler<1, 1, 5>* ss = new SearchScheduler<1, 1, 5>();

	ss->print();

	ss->schedule(str);

	UpdateScheduler<1, 1, 5>* us = new UpdateScheduler<1, 1, 5>();

	us->print();

	us->schedule(str);
	
	return 0;

}
