#ifndef LOGGING_H
#define LOGGING_H

#include <sstream>
#include <cassert>

#include "writer.h"

namespace yavca { namespace common {

class logging {
public:
    enum level {
        emerg = 0,
        alert,
        crit,
        err,
        warning,
        notice,
        info,
        debug
    };

    static const char* str_levels[debug+1];

public:
    logging();
    ~logging();
private:
    logging(const logging&);
    logging& operator=(const logging&);
public:

    logging& begin_log(level l, const char*, const char* func) {
        ss_ << str_levels[l] << " [" << func << "] ";
        return *this;
    }

    logging& operator<<(const char*);
    logging& operator<<(const std::string&);
    logging& operator<<(char);
    logging& operator<<(signed char);
    logging& operator<<(unsigned char);
    logging& operator<<(unsigned short int);
    logging& operator<<(signed short int);
    logging& operator<<(unsigned int);
    logging& operator<<(signed int);
    logging& operator<<(unsigned long int);
    logging& operator<<(signed long int);
    logging& operator<<(bool);
public:

    class end_type {
    private:
        friend class logging;
        end_type() {}
        ~end_type() {}
        end_type(const end_type&);
        void operator =(const end_type&);
    };

    static const end_type end;

    void operator<<(const end_type&) {
        write_message(ss_);
        ss_.str("");
    }
public:
    void init(writer* w) {
        assert(0 == writer_);
        writer_ = w;
    }

private:
    void write_message(const std::ostringstream&);
private:
    std::ostringstream ss_;
    writer* writer_;
};

#define PROXY_SS(type) \
    inline logging& logging::operator<<(type v) { \
        assert(0 != writer_); \
        ss_ << v; \
        return *this; \
    }

PROXY_SS(const char*);
PROXY_SS(const std::string&);
PROXY_SS(char);
PROXY_SS(unsigned char);
PROXY_SS(signed char);
PROXY_SS(unsigned short int);
PROXY_SS(signed short int);
PROXY_SS(unsigned int);
PROXY_SS(signed int);
PROXY_SS(unsigned long int);
PROXY_SS(signed long int);
PROXY_SS(bool);

inline void logging::write_message(const std::ostringstream &ss) {
    writer_->write_message(ss.str().c_str());
}

} }

#endif // LOGGING_H
