#pragma once
#include <JuceHeader.h>

/** @brief Executes indexed work items across a bounded hardware thread pool.
 */
struct Jobs
{
    /** @brief Runs @p function once for each index in `[0, count)` and joins all workers.
     *
     *  Uses atomic work-stealing across `min(count, hardware_concurrency)` threads when `count > 1`.
     *
     *  @tparam Function Callable accepting a single `int` item index.
     *  @param count     Number of work items; must be non-negative.
     *  @param function  Callable invoked concurrently from worker threads; caller synchronizes shared state.
     */
    template <typename Function>
    static void run (int count, Function&& function)
    {
        jassert (count >= 0);

        if (count == 1)
        {
            function (0);
        }
        else if (count > 1)
        {
            const auto hardwareThreads { static_cast<int> (std::thread::hardware_concurrency()) };
            const int workerCount { std::min (count, hardwareThreads > 0 ? hardwareThreads : 1) };

            if (workerCount <= 1)
            {
                for (int index { 0 }; index < count; ++index)
                    function (index);
            }
            else
            {
                std::atomic<int> nextIndex { 0 };

                auto worker = [&nextIndex, &function, count]
                {
                    for (int index { nextIndex.fetch_add (1, std::memory_order_relaxed) };
                         index < count;
                         index = nextIndex.fetch_add (1, std::memory_order_relaxed))
                    {
                        function (index);
                    }
                };

                std::vector<std::thread> workers;
                workers.reserve (static_cast<size_t> (workerCount - 1));

                for (int index { 0 }; index < workerCount - 1; ++index)
                    workers.emplace_back (worker);

                worker();

                for (auto& thread : workers)
                    thread.join();
            }
        }
    }
};
