#include "timer.h"

#include <cstdlib>

#include <event2/event.h>

using net::timer;

timer::timer(event_base* const ev_base,
             std::uint32_t timeout_sec,
             net::timer::on_expire on_expire_cb,
             void *arg)
    : timer_(NULL)
    , on_expire_cb_(on_expire_cb)
    , arg_(arg) {

    timer_ = evtimer_new(ev_base, &timer::on_ev_expire, this);

    timeval timeout;
    timeout.tv_sec = timeout_sec;
    timeout.tv_usec = 0;
    evtimer_add(timer_, &timeout);

}

timer::~timer() {
    event_free(timer_);
}

void timer::on_ev_expire(int, short, void *arg) {
    timer* t = static_cast<timer*>(arg);
    t->handle_on_ev_expire();
    delete t;
}

void timer::handle_on_ev_expire() {
    on_expire_cb_(arg_);
}
