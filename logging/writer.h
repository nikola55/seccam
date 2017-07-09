#ifndef WRITER_H
#define WRITER_H

namespace yavca { namespace common {

class writer {
public:
    writer();
    virtual ~writer();
private:
    writer(const writer&);
    writer& operator=(const writer&);
public:
    virtual bool write_message(const char* msg) = 0;
};

} }

#endif // WRITER_H
