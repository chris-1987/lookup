#ifndef _UTILITY_H
#define _UTILITY_H

#include "common.h"

NAMESPACE_UTILITY_BEG

/// print message
void printMsg(const std::string& _msg, const int code = 0) {

#ifdef _PRINT_MSG_ENABLE
	std::string errMsg;

	std::cerr << "errno: " + std::to_string(code) << " ";

	switch (code) {

		case 0: std::cerr << "normal---" << _msg << std::endl; break;

		case 1: std::cerr << "warning---" << _msg << std::endl; break;

		case 2: std::cerr << "fatal---" << _msg << std::endl; break;

		default: std::cerr << "not found specificaion\n";
	}

#endif

	return;
}


/// convert a string (integer value) to an integer
void strToUInt(const std::string& _str, const size_t& _beg, const size_t& _end,  uint32& _uint) {

	_uint = 0;

	for (size_t i = _beg; i < _end; ++i) {

		_uint = _uint * 10 + (_str[i] - '0'); 
	}

	return;
}

/// get the bit-value of _uint[_pos]
uint32 getBit(const uint32& _uint, const size_t& _pos) {

	 return _uint & OFFSET[_pos]; //if !=0, then 1; otherwise 0.
} 

/// retrieve IPv4 prefix, length and nexthop
void retrieveInfo4(const std::string& _line, uint32& _prefix, uint8& _length) {

	// prefix, in xx.xx.xx.xx format
	size_t pos1 = _line.find_first_of(" ");

	std::string prefix = _line.substr(0, pos1);

	// length
	size_t pos2 = _line.find_first_of(" ", pos1 + 1);

	std::string length = _line.substr(pos1 + 1, pos2 - pos1 - 1);

	// convert prefix to _prefix by integrating the 4 seguments into a whole integer
	uint32 sum;
	
	size_t beg, end;

	_prefix = 0, beg = 0;

	for (int i = 0; i < 4; ++i) {

		if (0 == i) end = prefix.find_first_of(".", beg);
		else if (3 == i) end = prefix.size();
		else end = prefix.find_first_of(".", beg + 1);

		strToUInt(prefix, beg, end, sum);
	
		_prefix = (_prefix << 8) + sum;
	
		beg = end + 1;	
	}

	// convert length to _length
	_length = 0, beg = 0, end = length.size();

	strToUInt(length, beg, end, sum);
		
	_length = sum;	

	return;
}



NAMESPACE_UTILITY_END

#endif
