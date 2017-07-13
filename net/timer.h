#ifndef TIMER_H
#define TIMER_H

#include <cstdint>

struct event_base;
struct event;

namespace net {

class timer {
public:
    typedef void (*on_expire)(void*);
public:
    timer(event_base* const ev_base,
          std::uint32_t timeout_sec,
          on_expire on_expire_cb,
          void* arg
          );
    ~timer();
private:
    timer(const timer&) = delete;
    void operator=(const timer&) = delete;
private:
    static void on_ev_expire(int, short, void* arg);
    void handle_on_ev_expire();
private:
    event* timer_;
    on_expire on_expire_cb_;
    void* arg_;
};

}

#endif // TIMER_H
