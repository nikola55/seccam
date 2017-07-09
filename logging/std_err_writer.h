#ifndef STD_ERR_WRITER_H
#define STD_ERR_WRITER_H

#include "writer.h"
#include <iostream>

namespace yavca { namespace common {

class std_err_writer : public writer {
public:
    std_err_writer();
    ~std_err_writer();
private:
    std_err_writer(const std_err_writer&);
    std_err_writer& operator =(const std_err_writer&);
public:
    bool write_message(const char *msg) {
        std::cerr << msg << std::endl;
        return true;
    }
};


} }

#endif // STD_ERR_WRITER_H
