#include "threadpool.hpp"

namespace index_stream {

    // ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ Add tasks to task queue and pop when implementation complete ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ 
    ThreadPool::ThreadPool(size_t num_threads) : stop(false){
        for (size_t i = 0; i < num_threads; i++) {
            this->workers.emplace_back( [this] {
                for(;;) {
                    std::function<void()> task;
                    {
                        std::unique_lock<std::mutex> lock(this->queue_mutex);
                        this->condition.wait(lock, [this]() { return this->stop || !this->tasks.empty(); });
                        if (this->stop || this->tasks.empty())  return;
                        task = std::move(this->tasks.front());
                        this->tasks.pop();
                    }
                    task();
                }
            });
        }
    }

    // ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ stop and join all threads ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ 
    ThreadPool::~ThreadPool() {
        {
            std::unique_lock<std::mutex>lock(queue_mutex);
            stop = true;
        }
        condition.notify_all();
        
        for (std::thread& worker : workers)
            worker.join();
    }

}