#ifndef _UINT_H
#define _UINT_H

using int8 = char;

using uint8 = unsigned char;

using int16 = short;

using uint16 = unsigned short;

using int32 = int;

using uint32 = unsigned int;

using int64 = long long int;

using uint64 = unsigned long long int;

using int128 = __int128;

using uint128 = unsigned __int128;


//
template<int W>
struct choose_ip_type{};

template<>
struct choose_ip_type<32> {

	typedef uint32 ip_type;
};

template<>
struct choose_ip_type<128> {

	typedef uint128 ip_type;
};


typedef typename choose_ip_type<32>::ip_type ipv4_type;

typedef typename choose_ip_type<128>::ip_type ipv6_type;



#endif
