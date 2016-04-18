#include "vEdThread.h"

namespace vEdCard
{
Thread::Thread()
    :work_(false), stop_(true)
{}
Thread::~Thread()
{
    stop();
    return 
}
void Thread::run()
{
    while(true)
    {
        {
            std::unique_lock<std::mutex> lck(mtx_);
            while(!stop_ && !work_) cv_.wait(lck);
            if(stop_)
                return ;
        }
        task_();
        work_ = false ;
    }
}
void Thread::join()
{
    pthread_->join();
}
void Thread::start()
{
    pthread_.reset(new std::thread(std::bind(&Thread::run, this)));
}

void Thread::stop()
{
    {
        std::unique_lock<std::mutex> lck(mtx_);
        if(!stop_)
        {
            stop_ = true;
            cv_.notify_all();
        }
    }
    if(!pthread_)
        pthread_->join();

    return ;

}
}
