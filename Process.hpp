#ifndef KITE_PROCESS_HPP_SKMD
#define KITE_PROCESS_HPP_SKMD

#include "IO.hpp"
#include "EventLoop.hpp"
#include <stdio.h>

namespace Kite {
    class Process : public Kite::Evented, public Kite::IO {
    public:
        Process(std::weak_ptr<EventLoop> ev);

        void popen(const std::string &cmd);
        void close();
        void closeRead();
        void closeWrite();

        virtual int read(char *buf, int len);
        virtual int write(const char *buf, int len);

        static std::string shell(const std::string &cmd, int timeout = 5000);

    protected:
        virtual void onSignal(int signal, int fd) override;
        virtual void onActivated(int fd, int e) override final;
        virtual void onReadActivated() {};
        virtual void onClosing() {}

    private:
        int d_pipein[2];
        int d_pipeout[2];
        int d_pid;
    };
};


#endif


