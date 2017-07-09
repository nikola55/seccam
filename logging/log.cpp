#include "log.h"

namespace yavca { namespace common {
logging* _logger_;
} }

int* yavca::common::log_include_counter::count_ = 0;
