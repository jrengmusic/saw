#pragma once
#include <JuceHeader.h>

/** @brief Runs indexed work and dynamically generated work across a bounded set of threads.
 *
 *  Work is claimed through atomic counters, so the scheduling path holds no lock.
 */
struct Jobs
{
    /** @brief Runs function once for each index in `[0, count)` and joins all workers.
     *
     *  Workers take the indexes from one atomic counter. The calling thread works too. The
     *  worker count is `min(count, juce::SystemStats::getNumCpus())`. One item, or one worker,
     *  runs on the calling thread alone.
     *
     *  A thread that cannot start is not an error. The work then runs on the threads that did start.
     *
     *  When a worker throws, that worker stops. After all workers join, the failure of the
     *  lowest-numbered failed worker is thrown again in the calling thread.
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
            const int workerCount { std::min (count, juce::SystemStats::getNumCpus()) };

            if (workerCount <= 1)
            {
                for (int index { 0 }; index < count; ++index)
                    function (index);
            }
            else
            {
                std::atomic<int> nextIndex { 0 };
                jam::Array<std::exception_ptr> failures;
                failures.resize (workerCount);

                auto worker { [&nextIndex, &function, &failures, count] (int workerIndex)
                {
                    try
                    {
                        for (int index { nextIndex.fetch_add (1, std::memory_order_relaxed) };
                             index < count;
                             index = nextIndex.fetch_add (1, std::memory_order_relaxed))
                        {
                            function (index);
                        }
                    }
                    catch (...)
                    {
                        failures.at (workerIndex) = std::current_exception();
                    }
                } };

                jam::Array<std::thread> workers;
                workers.ensureStorageAllocated (workerCount - 1);

                for (int index { 0 }; index < workerCount - 1; ++index)
                {
                    try
                    {
                        workers.add (std::thread { worker, index + 1 });
                    }
                    catch (const std::system_error&)
                    {
                        break;
                    }
                }

                worker (0);

                for (auto& thread : workers)
                    thread.join();

                rethrowFailure (failures);
            }
        }
    }

    /** @brief Lock-free list of work items, read in the order they were added.
     *
     *  The slots sit in segments that double in size and never move. A slot address therefore
     *  stays valid while other threads add items, and no resize needs a lock.
     *
     *  @tparam Item The type of one work item.
     */
    template <typename Item>
    class Items
    {
    public:
        /** @brief Creates an empty list with its first segment. */
        Items()
        {
            addSegment (0);
        }

        /** @brief Adds one item to the end of the list.
         *
         *  The call claims a slot with an atomic add. It publishes the item with a release store,
         *  so a reader that acquires the flag sees the whole item.
         *
         *  @param item The item to add. It is moved into the slot.
         */
        void add (Item item) noexcept
        {
            numPending.fetch_add (1, std::memory_order_relaxed);
            const auto [segment, offset] { getPosition (numAdded.fetch_add (1, std::memory_order_relaxed)) };

            if (offset == 0)
                addSegment (segment + 1);

            Slot* slots { segments.at (segment).load (std::memory_order_acquire) };

            while (slots == nullptr)
            {
                std::this_thread::yield();
                slots = segments.at (segment).load (std::memory_order_acquire);
            }

            slots[offset].item = std::move (item);
            slots[offset].isReady.store (true, std::memory_order_release);
        }

        /** @brief Claims the next slot and waits for its item.
         *
         *  The wait yields the thread until the item is published. The call returns nullopt when
         *  no item is pending, so a worker can end.
         *
         *  @return The claimed item, or nullopt when no item is pending.
         */
        std::optional<Item> removeNext()
        {
            const auto [segment, offset] { getPosition (numClaimed.fetch_add (1, std::memory_order_relaxed)) };

            while (numPending.load (std::memory_order_acquire) > 0)
            {
                Slot* slots { segments.at (segment).load (std::memory_order_acquire) };

                if (slots != nullptr and slots[offset].isReady.load (std::memory_order_acquire))
                    return std::move (slots[offset].item);

                std::this_thread::yield();
            }

            return std::nullopt;
        }

        /** @brief Marks one claimed item as finished.
         *
         *  The pending count drops by one. When it reaches zero, removeNext returns nullopt.
         */
        void setDone() noexcept
        {
            numPending.fetch_sub (1, std::memory_order_acq_rel);
        }

        /** @brief Reports whether added items are not yet claimed.
         *
         *  @return True when more items were added than were claimed.
         */
        bool hasBacklog() const noexcept
        {
            return numAdded.load (std::memory_order_relaxed) > numClaimed.load (std::memory_order_relaxed);
        }

    private:
        /** @brief One position of the list: the published flag and the item.
         *
         *  The flag is set with a release store after the item is written, so a reader that loads the
         *  flag with acquire sees the whole item.
         */
        struct Slot
        {
            /** @brief Set with a release store when item is published. */
            std::atomic<bool> isReady { false };

            /** @brief The work item held by the slot. */
            Item item {};
        };

        /** @brief Number of slots in the first segment. Each later segment has twice the slots of the one before. */
        static constexpr size_t firstSegmentSize { 64 };

        /** @brief Number of segments. The bound fixes the array of segment pointers, so it never resizes. */
        static constexpr size_t maximumSegments { 32 };

        /** @brief Maps a flat index to its segment and its offset in that segment.
         *
         *  The segments double in size, so the segment is found by subtracting each segment size in turn.
         *
         *  @param index The flat index of the position.
         *
         *  @return The segment number and the offset of the position in that segment.
         */
        static std::pair<size_t, size_t> getPosition (size_t index) noexcept
        {
            size_t segment { 0 };
            size_t segmentSize { firstSegmentSize };

            while (index >= segmentSize)
            {
                index -= segmentSize;
                segmentSize *= 2;
                ++segment;
            }

            return { segment, index };
        }

        /** @brief Allocates the slots of one segment, and publishes the pointer with a release store.
         *
         *  @param segment The number of the segment. Its size is firstSegmentSize doubled segment times.
         */
        void addSegment (size_t segment)
        {
            storage.at (segment) = std::make_unique<Slot[]> (firstSegmentSize << segment);
            segments.at (segment).store (storage.at (segment).get(), std::memory_order_release);
        }

        /** @brief Owns the slots of each segment. A segment is allocated once and never moves. */
        std::array<std::unique_ptr<Slot[]>, maximumSegments> storage;

        /** @brief Published pointers to the segments, read by other threads with acquire loads. */
        std::array<std::atomic<Slot*>, maximumSegments> segments {};

        /** @brief Number of positions claimed by add, so that each add owns one position. */
        std::atomic<size_t> numAdded { 0 };

        /** @brief Number of positions claimed by removeNext, so that each removal owns one position. */
        std::atomic<size_t> numClaimed { 0 };

        /** @brief Items added and not yet marked done. removeNext ends when it reaches zero. */
        std::atomic<size_t> numPending { 0 };
    };

    /** @brief Runs work whose items add further items, with at most workerCount threads.
     *
     *  The first items start the work. Each call of function may add items through addItem. A
     *  worker starts a further worker when items are waiting and a worker index is free. Each
     *  workerIndex is used by one thread at a time, so a caller keeps one slot per worker and
     *  needs no lock.
     *
     *  The failures are held in one slot per worker. Each worker writes only its own slot, and
     *  the array never resizes, so no lock is needed. A failed item is caught. Its worker keeps
     *  the last failure in its slot and takes the next item. After all workers join, the failure of
     *  the lowest-numbered failed worker is thrown again in the calling thread.
     *
     *  @tparam Item     The type of one work item.
     *  @tparam Function Callable accepting `(int workerIndex, Item item, addItem)`, where addItem
     *                   takes an Item and adds it to the work.
     *  @param workerCount The maximum number of threads. Must be positive.
     *  @param firstItems The items that start the work.
     *  @param function   Callable invoked from worker threads for each item.
     */
    template <typename Item, typename Function>
    static void runRecursive (int workerCount, jam::Array<Item> firstItems, const Function& function)
    {
        jassert (workerCount > 0);
        Items<Item> items;

        for (auto& item : firstItems)
            items.add (std::move (item));

        std::atomic<int> numWorkers { 1 };
        jam::Array<std::exception_ptr> failures;
        failures.resize (workerCount);
        runWorker (items, numWorkers, failures, 0, function);
        rethrowFailure (failures);
    }

private:
    /** @brief Rethrows the failure of the lowest-numbered failed worker, when any worker failed. */
    static void rethrowFailure (const jam::Array<std::exception_ptr>& failures)
    {
        for (const auto& failure : failures)
        {
            if (failure != nullptr)
                std::rethrow_exception (failure);
        }
    }

    /** @brief Takes items until none is pending, and starts further workers while items are waiting. */
    template <typename Item, typename Function>
    static void runWorker (Items<Item>& items, std::atomic<int>& numWorkers, jam::Array<std::exception_ptr>& failures, int workerIndex, const Function& function)
    {
        jam::Array<std::thread> workerThreads;
        workerThreads.ensureStorageAllocated (failures.size());

        auto startWorker { [&items, &numWorkers, &workerThreads, &failures, &function]
        {
            if (items.hasBacklog() and numWorkers.load (std::memory_order_relaxed) < failures.size())
            {
                const int claimedIndex { numWorkers.fetch_add (1) };

                if (claimedIndex < failures.size())
                {
                    try
                    {
                        workerThreads.add (std::thread { [&items, &numWorkers, &failures, &function, claimedIndex]
                        {
                            runWorker (items, numWorkers, failures, claimedIndex, function);
                        } });
                    }
                    catch (const std::system_error&)
                    {
                    }
                }
            }
        } };

        auto addItem { [&items, &startWorker] (Item newItem)
        {
            items.add (std::move (newItem));
            startWorker();
        } };

        for (auto item { items.removeNext() }; item.has_value(); item = items.removeNext())
        {
            startWorker();

            try
            {
                function (workerIndex, *item, addItem);
            }
            catch (...)
            {
                failures.at (workerIndex) = std::current_exception();
            }

            items.setDone();
        }

        for (auto& thread : workerThreads)
            thread.join();
    }
};
