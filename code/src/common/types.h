#ifndef _UINT_H
#define _UINT_H

#include <limits>
#include <iostream>


using int8 = char;

using uint8 = unsigned char;

using int16 = short;

using uint16 = unsigned short;

using int32 = int;

using uint32 = unsigned int;

using int64 = long long int;

using uint64 = unsigned long long int;


/// \brief 128-bit unsigned integer
///
/// Comprised of two 64-bit unsigned integer
///
struct MyUint128{

private:

	uint64 low; ///< 64-bit segment on the right

	uint64 high; //< 64-bit segument on the left (includes the LSB)


public:
	
	/// \brief default ctor
	MyUint128() : low(0), high(0) {}

	/// \brief copy ctor
	MyUint128(const MyUint128& _a) : low(_a.low), high(_a.high) {}

	/// \brief from two uint64
	MyUint128(const uint64& _low, const uint64& _high) : low(_low), high(_high) {}

	/// \brief from int32
	MyUint128(const int32& _a) : low(_a), high(0) {}

	/// \brief from uint64
	MyUint128(const uint64& _a) : low(_a), high(0) {}

	/// \brief set value of the 16-bit segment, _idx in [0, 7]
	void setSegment(uint64 _seg, int _idx) {

		switch(_idx) {

		case 0:
			high = _seg << 48; 
			
			break; 
		case 1:
			high = high + (_seg << 32); 

			break;
		case 2:
			high = high + (_seg << 16); 

			break;
		case 3:
			high = high + _seg; 

			break;
		case 4:
			low = _seg << 48;

			 break;
		case 5:
			low = low + (_seg << 32);

			 break;
		case 6:
			low = low + (_seg << 16);

			 break;
		case 7:
			low = low + _seg;

			 break;

		default:

			std::cerr << "set segment wrong\n"; std::cin.get();
		}

		return;
	}

	/// \brief equality checking
	bool operator == (const MyUint128& _a) {

		return (low == _a.low && high == _a.high);
	}

	/// \brief less-than checking 
	bool operator < (const MyUint128& _a) {

		return (high < _a.high) || (high == _a.high && low < _a.low);
	}

	/// \brief less-than-or-equal checking
	bool operator <= (const MyUint128& _a) const {

		return (high < _a.high) || (high == _a.high && low <= _a.low);
	}

	/// \brief greater-than checking
	bool operator > (const MyUint128& _a) const {

		return (high > _a.high) || (high == _a.high && low > _a.low);
	}

	/// \brief greater-than-or-equal checking
	bool operator >= (const MyUint128& _a) const {

		return (high > _a.high) || (high == _a.high && low >= _a.low);
	}

	/// \brief outputtable
	friend std::ostream& operator << (std::ostream& _os, const MyUint128& _a) {

		// output, whitespace is required
		return _os << _a.high << " " << _a.low;
	}

	/// \brief inputtable
	friend std::istream& operator >> (std::istream& _os, MyUint128& _a) {

		return _os >> _a.high >> _a.low;

	}

	/// \brief get bit value
	///
	/// retrieve a bit at the specified position, where pos in [0, 127]
	uint32 getBitValue(int32 _pos) const {

		static const uint64 odd = static_cast<uint64>(1) << 63;

		uint32 res = 0;

		if (_pos < 64) { // retrieve bit value from high part

			res = (((high << _pos) & odd) == 0) ? 0 : 1;
		}
		else { // retrieve bit value from low part

			_pos -= 64;

			res = (((low << _pos) & odd) == 0) ? 0 : 1;
		}

		return res;
	}


	/// \brief get bits value 
	///
	/// retrieve a range of bits and compute the decimal value, where _begBit and _endBit are in [0, 127] and _begBit <= _endBit
	/// \note _endBit - _begBit < 32 (assumption)
	uint32 getBitsValue(uint32 _begBit, uint32 _endBit) const {

		uint32 res = 0;

		if (_endBit < 64) { // [_begBit, _endBit] in high part

			uint64 mask = 0;

			for (uint32 i = _begBit; i <= _endBit; ++i) {

				mask = (mask << 1) + 1;
			}

			res = static_cast<uint32>((high >> (63 - _endBit)) & mask);
		}
		else if (_begBit >= 64) { // [_begBit, _endBit] in low part

			_begBit -= 64;

			_endBit -= 64;

			uint64 mask = 0;

			for (uint32 i = _begBit; i <= _endBit; ++i) {

				mask = (mask << 1) + 1;
			}

			res = static_cast<uint32>((low >> (63 - _endBit)) & mask);
		}
		else { // [_begBit, 63] in high part, [64, _endBit] in end part

			uint64 mask = 0;

			// retrieve part from high part, [_begBit, 63] in high part
			for (uint32 i = _begBit; i <= 63; ++i) {

				mask = (mask << 1) + 1;
			}

			res = static_cast<uint32>(high & mask);

			// retrieve part from low part, [64, _endBit] in low part
			_endBit -= 64;

			res = (res << (_endBit + 1)) +  static_cast<uint32>(low >> (63 - _endBit));
		}

		return res;
	}

};

//
template<int W>
struct choose_ip_type{};

template<>
struct choose_ip_type<32> {

	typedef uint32 ip_type;
};

template<>
struct choose_ip_type<128> {

	typedef MyUint128 ip_type;
};


typedef typename choose_ip_type<32>::ip_type ipv4_type;

typedef typename choose_ip_type<128>::ip_type ipv6_type;



#endif
