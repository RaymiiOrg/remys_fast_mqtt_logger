/*
* Copyright (c) 2025 Remy van Elst
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Afferro General Public License as published by
 * the Free Software Foundation, version 3.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 *
 */

#include "ThreadPool.h"


ThreadPool::ThreadPool(size_t thread_count) :
  _stop(false)
{
    for (size_t i = 0; i < thread_count; ++i)
    {
        _workers.emplace_back(&ThreadPool::worker_thread, this);
    }
}

ThreadPool::~ThreadPool()
{
    _stop = true;
    _cv.notify_all();
    for (auto &thread: _workers)
    {
        if (thread.joinable())
            {
            thread.join();
        }
    }
}

void ThreadPool::enqueue(std::function<void()> task)
{
    {
        std::lock_guard lock(_queue_mutex);
        _task_queue.push(std::move(task));
    }
    _cv.notify_one();
}

void ThreadPool::worker_thread()
{
    while (!_stop) {
        std::function<void()> task;
        {
            std::unique_lock lock(_queue_mutex);
            _cv.wait(lock, [this] {
                return !_task_queue.empty() || _stop;
            });
            if (_stop)
                return;

            task = std::move(_task_queue.front());
            _task_queue.pop();
        }

        task();
    }
}