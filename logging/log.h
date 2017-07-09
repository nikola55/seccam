#ifndef LOG_H
#define LOG_H

#include "logging.h"
#include "std_err_writer.h"

namespace yavca { namespace common {

extern logging* _logger_;

class log_include_counter {
public:
    log_include_counter() {
        if(count_ == 0) {
            count_ = new int(0);
            _logger_ = new logging;
            _logger_->init(new std_err_writer);
        }
        ++*count_;
    }
    ~log_include_counter() {
        assert(0 != count_);
        if(0 == --*count_) {
            delete count_;
            delete _logger_;
        }
    }

private:
    static int* count_;
};

namespace {
log_include_counter _counter_;
}

} }

// TODO : make thread safe
#define LOG(level) yavca::common::_logger_->begin_log(level, __FILE__, __PRETTY_FUNCTION__)

namespace common {
typedef yavca::common::logging log;
}

#endif // LOG_H
