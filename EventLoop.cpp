#include "EventLoop.hpp"
#include "Timer.hpp"
#include <poll.h>
#include <unistd.h>
#include <fcntl.h>
#include <time.h>
#include <iostream>
#include <climits>

using namespace Kite;

Evented::Evented(std::weak_ptr<EventLoop> ev)
   : p_Ev(ev)
{
}

Evented::~Evented()
{
    auto ev = p_Ev.lock();
    auto it = ev->p_evs.begin();
    while (it != ev->p_evs.end()) {
        if (it->second.first == this)
            ev->p_evs.erase(it++);
        else
            it++;
    }
}

void Evented::evAdd(int fd, int events)
{
    auto ev = p_Ev.lock();
    ev->p_evs.insert(std::make_pair(fd, std::make_pair(this, events)));
}

void Evented::evRemove(int fd)
{
    auto ev = p_Ev.lock();
    auto it = ev->p_evs.begin();
    while (it != ev->p_evs.end()) {
        if (it->second.first == this && it->first == fd)
            ev->p_evs.erase(it++);
        else
            it++;
    }
}


EventLoop::EventLoop()
{
    // wake up pipe to interrupt poll()
    pipe(p_intp);
    fcntl(p_intp[0], F_SETFL, O_NONBLOCK);
}

EventLoop::~EventLoop()
{
    close (p_intp[0]);
    close (p_intp[1]);
}

int EventLoop::exec()
{
    p_running = true;
    while (p_running) {

        //copy the timer list because it might be modified by a callback while iterating
        std::unordered_set<Timer *> timers = p_timers;
        for (auto it = timers.begin(); it != timers.end(); it++) {
            if ((*it)->expires() == 0) {
                (*it)->doExpire();
            }
        }

        // calculate timeout. again the callbacks might have modified the list :/
        int timeout = INT_MAX;
        for (auto it = p_timers.begin(); it != p_timers.end(); it++) {
            auto tt = (*it)->expires();
            if (tt < timeout)
                timeout = tt;
        }


        int    pollnum = p_evs.size() + 1;
        struct pollfd fds[pollnum];

        fds[0].fd = p_intp[0];
        fds[0].events = POLLIN;

        int i = 1;

        //copy list of ev fds because it might be modified inside activated()
        auto evs = p_evs;
        auto it  = evs.begin();

        while (it != evs.end()) {
            fds[i].fd = it->first;
            fds[i].events = 0;
            if (it->second.second & Kite::Evented::Read) {
                fds[i].events |= POLLIN;
            }
            if (it->second.second & Kite::Evented::Write) {
                fds[i].events |= POLLOUT;
            }
            i++;
            it++;
        }


        int ret = poll(fds, pollnum, timeout);



        i = 1;
        it = evs.begin();
        while (it != evs.end()) {
            if (fds[i].revents) {
                int kiteevents = 0;
                if (fds[i].revents & POLLIN) {
                    kiteevents |= Kite::Evented::Read;
                }
                if (fds[i].revents & POLLOUT) {
                    kiteevents |= Kite::Evented::Write;
                }

                //check the actual list rather then the copy for the Evented pointer
                //because the Evented might be deleted already
                auto evented = p_evs[it->first].first;
                if (evented) {
                    evented->onActivated(fds[i].fd, kiteevents);
                }
            }
            i++;
            it++;
        }

    }
    return p_exitCode;
}

void EventLoop::exit(int e)
{
    Timer::later(shared_from_this(), [this, e] () {
            p_exitCode = e;
            p_running = false;
            return false;
    }, 1, "exit");
    write(p_intp[1], "\n", 1);
}
