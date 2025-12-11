/*  brief: 该线程池是业务线程池，如果要基于该组件搭建应用服务器，则需要用到业务线程池做到动静分离
 *         而如果只需要做网关服务器，则不用改业务线程池，因为如果业务处理很慢，则会阻塞其它就绪事
 *         件的处理 
 */

#pragma once

#include <vector>
#include <queue>
#include <memory>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <future>
#include <functional>
#include <stdexcept>

namespace webserver::http
{

class ThreadPool
{
public:
    /* brief: 构造函数，启动指定数量的工作线程 */
    explicit ThreadPool(size_t threads_num) : _stop(false) {
        for(size_t i = 0; i < threads_num; ++i) {
            _workers.emplace_back([this] { while(true) {
                std::function<void()> task;
                {
                    std::unique_lock<std::mutex> lock(this->_queue_mutex);
                    this->_cond.wait(lock, [this]{ return this->_stop || !this->_tasks.empty(); });

                    if(this->_stop && this->_tasks.empty()) return;

                    task = std::move(this->_tasks.front());
                    this->_tasks.pop();
                }
                task();
            }});
        }
    }
    /* brief: 析构函数 */
    ~ThreadPool();
    /* brief: 提交任务 */
    template<class F, class... Args>
    void Enqueue(F&& f, Args&&... args) {
        auto task = std::bind(std::forward<F>(f), std::forward<Args>(args)...);
        {
            std::unique_lock<std::mutex> lock(_queue_mutex);
            if(_stop) throw std::runtime_error("向停止了的任务队列提交任务");
            _tasks.emplace(task);
        }
        _cond.notify_one();
    }
private:
    std::vector<std::thread> _workers; //业务线程
    std::queue<std::function<void()>> _tasks; //生产者消费者模型的任务队列
    std::mutex _queue_mutex;
    std::condition_variable _cond;
    bool _stop;
};

}