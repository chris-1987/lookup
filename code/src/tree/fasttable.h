#ifndef _FASTTABLE_H
#define _FASTTABLE_H


////////////////////////////////////////////////////////////
/// Copyright (c) 2016, Sun Yat-sen University,
/// All rights reserved
/// \file fasttable.h
/// \brief definition of a fast table for searching short prefixes
/// 
/// Build a fast table for prefixes not longer than U bits.
/// For each prefixes shorter than U bits, we expand them to U bits by padding 0. 
/// All the prefixes with a same expanded form are gathered together in the table. 
/// To insert/delete a prefix into/from the table, we expand the prefix to U bits by padding 0 and use <expandedPrefix> as an index to find the set of 
/// prefixes that have a same expanded form and use its length to check whether or not it exists.
///
/// \author Yi Wu
/// \date 2016.11
///////////////////////////////////////////////////////////

#include "../common/common.h"
#include "../common/utility.h"

#include <cmath>


/// \brief A fast lookup table for storing prefixes shorter than U bits.
/// 
/// \param U A threshold value, assumed to be a multiple of 2.
template<int W, int U, size_t V = static_cast<size_t>(pow(2, U))>
struct FastTable{

private:

	typedef typename choose_ip_type<W>::ip_type ip_type;

public:
	/// \brief Entry in the fast lookup table
	struct Entry{

		bool mask[U]; ///< mask value

		uint32 nexthop[U]; ///< nexthop value

		/// \brief default ctor
		Entry(){

			for (int i = 0; i < U; ++i) {

				mask[i] = false;

				nexthop[i] = 0;
			}
		}
	};

	
	Entry mEntries[V]; ///< V entries

	/// \brief insert a prefix
	void ins(const ip_type& _prefix, const uint8& _length, const uint32& _nexthop) {

		size_t idx = utility::getBitsValue(_prefix, 0, U - 1);

		mEntries[idx].mask[_length - 1] = true;

		mEntries[idx].nexthop[_length - 1] = _nexthop;

		return;
	}

	/// \brief delete a prefix
	void del(const ip_type& _prefix, const uint8& _length) {
	
		size_t idx = utility::getBitsValue(_prefix, 0, U - 1);

		mEntries[idx].mask[_length - 1] = false;

		mEntries[idx].nexthop[_length - 1] = 0;

		return;
	}

	/// \brief search a prefix
	uint32 search(const ip_type& _prefix) {

		size_t idx = utility::getBitsValue(_prefix, 0, U - 1);

		for (int i = U - 1; i >= 0; --i) { // the longer the better

			if (true == mEntries[idx].mask[i]) {

				return mEntries[idx].nexthop[i];
			}
		}

		return 0;
	}	
};

#endif
