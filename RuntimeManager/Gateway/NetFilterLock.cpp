//
//  NetFilterLock.cpp
//  AppManager Gateway
//
//  Copyright © 2022 Sky UK. All rights reserved.
//

#include "NetFilterLock.h"
#include "NetFilter.h"

#include <thread>

#include <fcntl.h>
#include <unistd.h>
#include <sys/file.h>


#define XT_LOCK_NAME    "/run/xtables.lock"


NetFilterLock::NetFilterLock()
{
    mLockFd = open(XT_LOCK_NAME, O_CLOEXEC | O_CREAT, 0600);
  //  if (mLockFd < 0)
        //qErrnoWarning(errno, "failed to create / open '%s'", XT_LOCK_NAME);
}

NetFilterLock::~NetFilterLock()
{
//    if ((mLockFd >= 0) && (close(mLockFd) != 0))
       // qErrnoWarning(errno, "failed to close lock file");
}

void NetFilterLock::lock()
{
    if (mLockFd < 0)
    {
    //    qWarning("invalid xtables lock");
    }
    else if (TEMP_FAILURE_RETRY(flock(mLockFd, LOCK_EX)) < 0)
    {
      //  qErrnoWarning(errno, "failed to acquire xtables file lock");
    }
}

void NetFilterLock::unlock()
{
    if (mLockFd < 0)
    {
        //qWarning("invalid xtables lock");
    }
    else if (TEMP_FAILURE_RETRY(flock(mLockFd, LOCK_UN)) < 0)
    {
        //qErrnoWarning(errno, "failed to clear xtables file lock");
    }
}

bool NetFilterLock::try_lock()
{
    if (mLockFd < 0)
    {
//        qWarning("invalid xtables lock");
        return false;
    }

    return (TEMP_FAILURE_RETRY(flock(mLockFd, LOCK_EX | LOCK_NB)) == 0);
}

bool NetFilterLock::try_lock_until(const std::chrono::steady_clock::time_point &deadline)
{
    if (mLockFd < 0)
    {
  //      qWarning("invalid xtables lock");
        return false;
    }

    while (std::chrono::steady_clock::now() < deadline)
    {
        if (TEMP_FAILURE_RETRY(flock(mLockFd, LOCK_EX | LOCK_NB)) == 0)
            return true;

        //thread::msleep(20);
    }

    if (TEMP_FAILURE_RETRY(flock(mLockFd, LOCK_EX | LOCK_NB)) == 0)
        return true;

//    qWarning("timed out waiting to acquire the xtables file lock");
    return false;
}
