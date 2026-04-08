//
//  NetFilterLock.h
//  AppManager Gateway
//
//  Copyright © 2022 Sky UK. All rights reserved.
//

#ifndef NETFILTERLOCK_H
#define NETFILTERLOCK_H

#include <mutex>
#include <chrono>
#include "UtilsLogging.h"
#include "tracing/Logging.h"


class NetFilterLock
{
public:
    NetFilterLock();
    ~NetFilterLock();

    NetFilterLock(NetFilterLock&) = delete;
    NetFilterLock(NetFilterLock&&) = delete;

public:
    void lock();
    void unlock();
    bool try_lock();

    bool try_lock_until(const std::chrono::steady_clock::time_point &deadline);

    template <class _Rep, class _Period>
    inline bool try_lock_for(const std::chrono::duration<_Rep, _Period>& duration)
    {
        const std::chrono::steady_clock::time_point deadline =
            std::chrono::steady_clock::now() + duration;

        return try_lock_until(deadline);
    }

private:
    int mLockFd = -1;
};

#endif // NETFILTERLOCK_H
