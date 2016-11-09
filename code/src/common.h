#ifndef _COMMON_H
#define _COMMON_H

#include "namespace.h"
#include "types.h"

#include <iostream>
#include <fstream>
#include <string>
#include <memory>
#include <limits>



#define _PRINT_MSG_ENABLE //option for enabling printMsg 

static const uint32 OFFSET[] = {
	0x80000000, 0x40000000, 0x20000000, 0x10000000, 
	0x08000000, 0x04000000, 0x02000000, 0x01000000, 
	0x00800000, 0x00400000, 0x00200000, 0x00100000, 
	0x00080000, 0x00040000, 0x00020000, 0x00010000,
	0x00008000, 0x00004000, 0x00002000, 0x00001000,
	0x00000800, 0x00000400, 0x00000200, 0x00000100,
	0x00000080, 0x00000040, 0x00000020, 0x00000010,
	0x00000008, 0x00000004, 0x00000002, 0x00000001
};

#endif
