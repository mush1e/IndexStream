#include "threadpool.hpp"

namespace index_stream {

    ThreadPool::ThreadPool(size_t num_threads) : stop(false), pause(false), active_tasks(0) {
        for (size_t i = 0; i < num_threads; i++) {
            this->workers.emplace_back( [this] {
                for(;;) {
                    std::function<void()> task;
                    {
                        std::unique_lock<std::mutex> lock(this->queue_mutex);
                        this->condition.wait(lock, [this]() { 
                            return this->stop || (!this->pause && !this->tasks.empty()); 
                        });
                        if (this->stop && this->tasks.empty()) 
                            return;
                        if (this->pause) 
                            continue;
                        task = std::move(this->tasks.front());
                        this->tasks.pop();
                        active_tasks++;  
                    }
                    task();
                    {  
                        std::unique_lock<std::mutex> lock(this->queue_mutex);
                        active_tasks--;
                        if (active_tasks == 0 && tasks.empty()) {
                            all_tasks_done_condition.notify_one();  
                        }
                    }
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

    // ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ Pause the task queue ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ 
    auto ThreadPool::pause_task_queue() -> void {
        std::unique_lock<std::mutex> lock(queue_mutex);
        pause = true;  
    }

    // ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ Resume the task queue ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ 
    auto ThreadPool::resume_task_queue() -> void {
        {
            std::unique_lock<std::mutex> lock(queue_mutex);
            pause = false;  
        }
        condition.notify_all();
    }

    // ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ Await pending tasks ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ 
    auto ThreadPool::await_pending_tasks() -> bool {
        std::unique_lock<std::mutex> lock(queue_mutex);
        all_tasks_done_condition.wait(lock, [this]() {
            return active_tasks == 0;
        });
        return true;
    }

}