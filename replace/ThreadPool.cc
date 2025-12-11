#include "ThreadPool.h"

namespace webserver::http
{

ThreadPool::~ThreadPool() {
    {
        std::unique_lock<std::mutex> lock(_queue_mutex);
        _stop = true;
    }
    _cond.notify_all();
    for(std::thread &worker : _workers) {
        if(worker.joinable()) worker.join();
    }
}

}