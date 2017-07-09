#include "logging.h"

yavca::common::logging::logging()
    : writer_(0) {

}

yavca::common::logging::~logging() {
    delete writer_;
}

const char* yavca::common::logging::str_levels[debug+1] = {
    "emerg",
    "alert",
    "crit",
    "err",
    "warning",
    "notice",
    "info",
    "debug"
};

const yavca::common::logging::end_type yavca::common::logging::end;
