#pragma once

#include <cstddef>
#include <functional>

// Background work is tracked so services stay alive until workers finish.
bool startThread(std::function<void()> fn, size_t stackSize = 1024 * 1024);
bool threadStopRequested();
void requestThreadStop();
void waitForBackgroundThreads();
