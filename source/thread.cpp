#include "thread.hpp"

#include <pthread.h>

#include <atomic>
#include <memory>
#include <mutex>
#include <vector>

#include "log.hpp"

namespace {
struct ThreadRecord {
    pthread_t handle{};
    std::atomic<bool> complete{false};
};

struct ThreadStart {
    std::function<void()> fn;
    ThreadRecord* record;
};

std::mutex g_mutex;
std::atomic<bool> g_stopping{false};
std::vector<std::unique_ptr<ThreadRecord>> g_threads;

void joinThreads(std::vector<std::unique_ptr<ThreadRecord>>& threads) {
    for (auto& thread : threads) pthread_join(thread->handle, nullptr);
}
}

bool startThread(std::function<void()> fn, size_t stackSize) {
    std::vector<std::unique_ptr<ThreadRecord>> completed;
    {
        std::lock_guard<std::mutex> lock(g_mutex);
        for (auto it = g_threads.begin(); it != g_threads.end();) {
            if ((*it)->complete.load(std::memory_order_acquire)) {
                completed.push_back(std::move(*it));
                it = g_threads.erase(it);
            } else {
                ++it;
            }
        }
    }
    joinThreads(completed);

    auto* start = new ThreadStart{std::move(fn), nullptr};
    pthread_attr_t attr;
    pthread_attr_init(&attr);
    pthread_attr_setstacksize(&attr, stackSize);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);

    int rc = 0;
    {
        std::lock_guard<std::mutex> lock(g_mutex);
        if (g_stopping.load()) {
            pthread_attr_destroy(&attr);
            delete start;
            return false;
        }

        auto record = std::make_unique<ThreadRecord>();
        start->record = record.get();
        g_threads.push_back(std::move(record));
        rc = pthread_create(&g_threads.back()->handle, &attr,
            [](void* p) -> void* {
                std::unique_ptr<ThreadStart> start(static_cast<ThreadStart*>(p));
                start->fn();
                start->record->complete.store(true, std::memory_order_release);
                return nullptr;
            },
            start);
        if (rc != 0) g_threads.pop_back();
    }
    pthread_attr_destroy(&attr);

    if (rc != 0) {
        delete start;
        logMsg("pthread_create failed: %d", rc);
        return false;
    }
    return true;
}

bool threadStopRequested() { return g_stopping.load(); }

void requestThreadStop() { g_stopping.store(true); }

void waitForBackgroundThreads() {
    std::vector<std::unique_ptr<ThreadRecord>> threads;
    {
        std::lock_guard<std::mutex> lock(g_mutex);
        g_stopping.store(true);
        threads.swap(g_threads);
    }
    joinThreads(threads);
}
