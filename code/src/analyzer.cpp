#include "analyzer.h"

int main(int argc, char** argv){

	Analyzer analyzer(argv[1], argv[2]);	

	analyzer.generate();

	return 0;

}
