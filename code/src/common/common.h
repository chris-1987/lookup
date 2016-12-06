#ifndef _COMMON_H
#define _COMMON_H

#include "namespace.h"
#include "types.h"

#include <iostream>
#include <fstream>
#include <string>
#include <memory>
#include <limits>
#include <cassert>

static const double LAMBDA = 0.3; // packet arriving probability

static const int BURSTSIZE = 1; // number of packets arriving at the same time

static const int QUEUESIZE = 128; // size of queue

#define _PRINT_MSG_ENABLE //option for enabling printMsg 


#endif
