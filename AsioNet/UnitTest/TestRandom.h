
#include <unordered_map>
#include <thread>
#include <mutex>
#include <string>
#include <iostream>
#include <chrono>
#include <condition_variable>

#include "UnitTestInterface.h"
#include "parallel_core/SafeRandom.hpp"

class TestRandom :public UnitTestInterface
{
public:
    static constexpr size_t capacity = 1024 * 1000;
    static constexpr size_t thread_num = 4;
public:
    virtual void test_memory() override
    {
        uint64_t x = 0;
        for (size_t i = 0; i < capacity; ++i)
        {
            x = SAFE_RAND;
        }
    }

    virtual void test_logic() override
    {
        std::unordered_map<uint64_t, size_t> container;
        for (size_t n = 0; n < capacity; ++n)
        {
            uint64_t cur_rand = SAFE_RAND % (capacity);
            if (HAS_KEY(container, cur_rand))
            {
                container[cur_rand] += 1;
            }
            else
            {
                container.insert(std::make_pair(cur_rand, 1));
            }
        }

        size_t count_arr[11] = { 0,0,0,0,0,0,0,0,0,0,0 };
        for (size_t i = 0; i < capacity; i++)
        {
            if (HAS_KEY(container, i))
            {
                size_t count = i / (capacity / 10);
                count_arr[count] += container[i];
            }
        }

        for (size_t i = 0; i < 10; ++i)
        {
            print_bar(count_arr[i] / (capacity / 10 / 10));
        }
    }

    virtual void test_time() override
    {
        uint64_t x = 0;

        auto timer = std::chrono::high_resolution_clock();
        auto start_t = timer.now();
        for (size_t i = 0; i < capacity; ++i)
        {
            x = SAFE_RAND;
        }
        auto end_t = timer.now();

        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_t - start_t);

        std::lock_guard<std::recursive_mutex> lck(_mut);
        std::cout << "time cost during running 1,024,000 random: " << duration.count() << "ms" << std::endl;
    }

    virtual void test_threadsafe() override
    {
        // 在每个线程是否分布均匀
        {
            std::unique_lock<std::recursive_mutex> lck(_mut);
            std::cout << "each thread" << std::endl;
            lck.unlock();

            std::thread ths[thread_num];

            for (size_t i = 0; i < thread_num; ++i)
            {
                ths[i] = std::thread([this]() {
                    test_logic();
                });
            }

            for (size_t i = 0; i < thread_num; ++i)
            {
                ths[i].join();
            }
        }

        // 所有线程加起来是否均匀分布
        test_all_thread_sum();
    }

    void test_all_thread_sum()
    {
        std::unique_lock<std::recursive_mutex> lck(_mut);
        std::cout << "total thread" << std::endl;
        lck.unlock();

        std::unordered_map<uint64_t, size_t> container_total[thread_num];
        std::condition_variable cv;
        unsigned int done_signals = 0u;

        std::thread ths[5];
        for (size_t i = 0; i < thread_num; ++i)
        {
            ths[i] = std::thread([&container_total, &done_signals, i, &cv]() {
                auto& container = container_total[i];
                for (size_t n = 0; n < capacity; ++n)
                {
                    uint64_t cur_rand = SAFE_RAND % (capacity);
                    if (HAS_KEY(container, cur_rand))
                    {
                        container[cur_rand] += 1;
                    }
                    else
                    {
                        container.insert(std::make_pair(cur_rand, 1));
                    }
                }

                done_signals |= (0x1 << i);
                
                cv.notify_all();
            });
        }

        ths[4] = std::thread([&done_signals, &cv, &container_total, this]() {
            std::unique_lock<std::mutex> lck(_cv_mut);
            while (done_signals != 15u)
            {
                cv.wait(lck);
            }

            size_t count_arr[11] = { 0,0,0,0,0,0,0,0,0,0,0 };
            
            for (int n = 0; n < thread_num; ++n)
            {
                auto& container = container_total[n];
                for (size_t i = 0; i < capacity; i++)
                {
                    if (HAS_KEY(container, i))
                    {
                        size_t count = i / (capacity / 10);
                        count_arr[count] += container[i];
                    }
                }
            }

            for (size_t i = 0; i < 10; ++i)
            {
                print_bar(count_arr[i] / thread_num / (capacity / 10 / 10));
            }
        });

        for (int i = 0; i < 5; ++i)
        {
            ths[i].join();
        }
    }

    void print_bar(size_t height)
    {
        const char* bar_element = "■";
        size_t len = strlen(bar_element);
        char buffer[1024];
        for (size_t i = 0; i < height; ++i)
        {
            std::memcpy(&(buffer[i * len]), bar_element, len);
        }

        buffer[height * len] = '\0';

        std::lock_guard<std::recursive_mutex> lock(_mut);
        std::cout << buffer << std::endl;
    }

private:
    std::recursive_mutex _mut;

    std::mutex _cv_mut;
};