#ifndef _UTILITY_H
#define _UTILITY_H

#include "common.h"

NAMESPACE_UTILITY_BEG

/// \brief print message
///
/// \param _msg message to print
/// \param code indicate warning level
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


/// \brief convert a string (integer value) to an unsigned integer for ipv6
///
/// \param _str input string
/// \param _beg start position
/// \param _end end position
/// \param _uint result stored in _uint
template<typename T>
void strToUInt(const std::string& _str, const size_t& _beg, const size_t& _end, T& _uint) {

	_uint = 0;

	for (size_t i = _beg; i < _end; ++i) {

		_uint = _uint * 10 + (_str[i] - '0'); 
	}

	return;
}

/// \brief get the bit-value for ipv4 
///
/// \param _uint ipv4 address
/// \param _pos offset
uint32 getBitValue(const uint32& _uint, const size_t& _pos) {
		
	static const uint32 odd = 1;

	return (_uint << _pos) & (odd << 31);
} 

/// \brief get the bit-value for ipv6
/// 
/// \param _uint ipv6 address
/// \param _pos offset
uint128 getBitValue(const uint128& _uint, const size_t& _pos) {
	// not allowe to declare 128-bit unsigned integer constant, thus we must directly compute 

	static const uint128 odd = 1;

	return (_uint << _pos) & (odd << 127);
} 


/// get the value of bits for ipv4
///
/// \param _uint inpv4 address
/// \param _stride number of bits to be retrieved
/// \param _pos start position
uint32 getBitsValue(const uint32& _uint, const uint32 _begBit, const uint32 _endBit) {

	uint32 mask = 0;

	for (uint32 i = _begBit; i <= _endBit; ++i) {

		mask = (mask << 1) + 1;
	}

	return (_uint >> (31 - _endBit)) & mask;
}


/// \brief retrieve IPv4 prefix, length and nexthop
///
/// \param _line input sttring line, containing prefix and length
/// \param _prefix store the prefix retrieved from _line
/// \param _length sotre the prefix length retrieved from _length
void retrieveInfo(const std::string& _line, ipv4_type& _prefix, uint8& _length) {

	// prefix, in xx.xx.xx.xx format
	size_t pos1 = _line.find_first_of(" ");

	std::string prefix = _line.substr(0, pos1);

	// length
	size_t pos2 = _line.find_first_of(" ", pos1 + 1);

	std::string length = _line.substr(pos1 + 1, pos2 - pos1 - 1);

	// convert prefix to _prefix by integrating the 4 seguments into a whole integer
	ipv4_type sum1;
	
	size_t beg, end;

	_prefix = 0, beg = 0;

	for (int i = 0; i < 4; ++i) {

		if (0 == i) {

			end = prefix.find_first_of(".", beg);
		}
		else if (3 == i) {

			end = prefix.size();
		}
		else {
			
			end = prefix.find_first_of(".", beg + 1);
		}

		strToUInt<ipv4_type>(prefix, beg, end, sum1);
	
		_prefix = (_prefix << 8) + sum1;
	
		beg = end + 1;	
	}

	// convert length to _length
	beg = 0, end = length.size();

	strToUInt<uint8>(length, beg, end, _length);
		
	return;
}

/// \brief retrieve IPv6 prefix, length and nexthop
/// \param _line
/// \param _prefix
/// \param _length
void retrieveInfo(const std::string& _line, ipv6_type& _prefix, uint8& _length) {

	return;
}


NAMESPACE_UTILITY_END

#endif
