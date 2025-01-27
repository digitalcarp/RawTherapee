/*
 *  This file is part of RawTherapee.
 *
 *  Copyright (c) 2025 Daniel Gao <daniel.gao.work@gmail.com>
 *
 *  RawTherapee is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  RawTherapee is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with RawTherapee.  If not, see <https://www.gnu.org/licenses/>.
 */

#pragma once

#include <condition_variable>
#include <functional>
#include <mutex>
#include <queue>
#include <thread>
#include <vector>

class ThreadPool {
public:
    using Task = std::function<void()>;

    ThreadPool(size_t num_threads) : m_stop(false)
    {
        m_threads.reserve(num_threads);
        for (size_t i = 0; i < num_threads; i++) {
            m_threads.emplace_back([&]() { threadLoop(); });
        }
    }

    ~ThreadPool()
    {
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_stop = true;
        }
        m_cvar.notify_all();

        for (auto& t : m_threads) {
            t.join();
        }
    }

    void enqueue(Task&& task)
    {
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_tasks.emplace(std::move(task));
        }
        m_cvar.notify_one();
    }

private:
    void threadLoop()
    {
        while (true) {
            Task task;
            {
                std::unique_lock<std::mutex> lock(m_mutex);
                m_cvar.wait(lock, [&] {
                    return !m_tasks.empty() || m_stop;
                });

                if (m_stop && m_tasks.empty()) return;

                task = std::move(m_tasks.front());
                m_tasks.pop();
            }

            task();
        }
    }

    std::vector<std::thread> m_threads;
    std::queue<Task> m_tasks;
    std::mutex m_mutex;
    std::condition_variable m_cvar;
    bool m_stop;
};
