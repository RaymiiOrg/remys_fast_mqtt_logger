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

#pragma once

#include <thread>
#include <atomic>
#include <condition_variable>
#include <functional>
#include <mutex>
#include <vector>
#include <queue>

class ThreadPool {

public:
    explicit ThreadPool(size_t thread_count);
    ~ThreadPool();

    void enqueue(std::function<void()> task);

private:
    std::vector<std::thread> _workers {};
    std::queue<std::function<void()>> _task_queue;
    std::mutex _queue_mutex;
    std::condition_variable _cv;
    std::atomic<bool> _stop;

    void worker_thread();
};

