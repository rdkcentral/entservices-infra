//
//  ContainerUtils.cpp
//  AppManager Gateway
//
//  Copyright © 2022 Sky UK. All rights reserved.
//

#include "ContainerUtils.h"

#include <cstring>
#include <thread>
#include <ext/stdio_filebuf.h>

#include <grp.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/syscall.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <net/if.h>
#include <netinet/in.h>


namespace WPEFramework
{
namespace Plugin
{

#if !defined(SYS_execveat)
#  if defined(__NR_execveat)
#    define SYS_execveat  __NR_execveat
#  elif defined(__arm__)
#    define SYS_execveat  387
#  endif
#endif

# define CLONE_NEWNET   0x40000000      /* New network namespace.  */

// -----------------------------------------------------------------------------
/*!
    \static
    \internal

    Thread helper function that implements the setns syscall

    This must be executed as a thread as it calls setns which switches
    namespaces and you don't really want to do this in the main thread.

 */
static void nsThread(int newNsFd, int nsType, bool *success,
                     std::function<void()> &func)
{
    LOGINFO("nsThread started");
    // unshare the specific namespace from the thread
    if (unshare(nsType) != 0)
    {
        LOGERR("failed to unshare");
        *success = false;
        return;
    }

    // switch into the new namespace
    if (setns(newNsFd, nsType) != 0)
    {
        LOGERR("failed to switch into new namespace");
        *success = false;
        return;

    }

    // execute the caller's function
    func();

    *success = true;
    LOGINFO("nsThread End");
}

// -----------------------------------------------------------------------------
/*!
    \static
    \internal

    Utility function to run some code in a specific namespace of the container.

    This function uses the setns syscall and therefore it must spawn a thread
    to run the callback in.  However this function blocks until the thread
    completes, so although it is multi-threaded it's API is blocking, i.e.
    effectively single threaded.

    The \a nsType argument should be one of the following values:
        CLONE_NEWIPC  - run in a IPC namespace
        CLONE_NEWNET  - run in a network namespace
        CLONE_NEWNS   - run in a mount namespace

 */
static bool nsEnterWithPid(pid_t pid, int nsType,
                           const std::function<void()> &func)
{
    LOGINFO("Entering nsEnterWithPid with pid=%d, nsType=%d", pid, nsType);
    char nsName[8];
    char nsPath[32];
    strcpy(nsName, "net");

    bool success;

    // get the namespace of the containered app
    sprintf(nsPath, "/proc/%d/ns/%s", pid, nsName);
    int newNsFd = open(nsPath, O_RDONLY | O_CLOEXEC);
    if (newNsFd < 0)
    {
        LOGERR("failed to open container namespace @ '%s'", nsPath);
        success = false;
    }
    else
    {
	LOGINFO("thread started");
        // spawn the thread to run the callback in
        std::thread thread = std::thread(std::bind(&nsThread, newNsFd, nsType, &success, func));

        // block until the thread completes
        thread.join();
	LOGINFO("thread end");
    }

    // close the namespaces
    if ((newNsFd >= 0) && (close(newNsFd) != 0))
    {
        LOGERR("failed to close namespace");
    }

    return success;
}

// -----------------------------------------------------------------------------
/*!
    Finds a pid of a process that is running inside the container with given
    \a containerId.  If no container with \a containerId is running then
    -1 is returned.

    \note This makes lot of assumptions on how Dobby creates containers, so is
    not portable, and prone to breaking if Dobby changes how it works
    internally.

 */
static pid_t findContainerPid(const std::string &containerId)
{
	
     LOGINFO("Container IP: %s", containerId.c_str());
    const std::string cgroupPath = "/sys/fs/cgroup/memory/" + containerId + "/cgroup.procs";
    int procsFd = open(cgroupPath.c_str(), O_RDONLY | O_CLOEXEC);
    if (procsFd < 0)
    {
        if (errno == ENOENT)
	{
            LOGINFO("no cgroup file @ %s", cgroupPath.c_str());
	}
        else{
            LOGERR("failed to open cgroup file @ %s", cgroupPath.c_str());
	}

        return -1;
    }

    // wrap the fd in a c++ file buf, it will close the fd on destruction
    __gnu_cxx::stdio_filebuf<char> fileBuf(procsFd, std::ios::in);
    std::istream fileStream(&fileBuf);

    // we only need one pid, so just read the first line
    std::string line;
    if (!std::getline(fileStream, line))
    {
        LOGERR("cgroup procs file is empty - no pids in container?");
        return -1;
    }

    // convert to value and return
    long pid = strtol(line.c_str(), nullptr, 10);
    if (pid >= INT32_MAX)
        return -1;
    else
        return pid;
}

// -----------------------------------------------------------------------------
/*!
    Runs the given function in the context of a namespace of the container with
    \a containerId.


    \note This makes lot of assumptions on how Dobby creates containers, so is
    not portable, and prone to breaking if Dobby changes how it works
    internally.

 */
bool ContainerUtils::nsEnterImpl(const std::string &containerId, std::string type,
                                 const std::function<void()> &func)
{
    pid_t containerPid = findContainerPid(containerId);
    if (containerPid <= 0)
    {
        LOGERR("no container found with id '%s'", containerId.c_str());
        return false;
    }

    return nsEnterWithPid(containerPid, CLONE_NEWNET, func);
}

// -----------------------------------------------------------------------------
/*!
    Gets the IPv4 address of the container, which is the IP address assign to
    the veth0 inside the container.

    This uses the nsEnter function to enter the network namespace of the
    container and then uses ioctls on the `eth0` interface to get it's assigned
    IP address.

    \note This makes lot of assumptions on how Dobby creates containers, so is
    not portable, and prone to breaking if Dobby changes how it works
    internally.

 */
uint32_t ContainerUtils::getContainerIpAddress(const std::string &containerId)
{
    LOGINFO("Container ID: %s", containerId.c_str());

    uint32_t ipv4Addr = 0;

    // lambda to run on a thread in the context of the container network
    // namespace
    auto getIpAddress = [&]() {

        // the interface within the container is always called 'eth0'
        ifreq ifr;
        memset(&ifr, 0x00, sizeof(ifr));
        strcpy(ifr.ifr_name, "eth0");

        // create a general socket for the ioctl
        int sock = socket(AF_INET, SOCK_STREAM | SOCK_CLOEXEC, 0);
        if (sock < 0)
        {
            LOGERR("failed to create socket");
            return;
        }

        // attempt to get the interface details
        if (ioctl(sock, SIOCGIFADDR, &ifr) < 0)
        {
            LOGERR("failed to get interface ip address");
            close(sock);
            return;
        }

        if (close(sock) < 0)
        {
            LOGERR("failed to close ifcase socket");
        }

        // finally, get and store the ip address
        sockaddr_in* ifaceAddr = reinterpret_cast<sockaddr_in*>(&ifr.ifr_addr);
        ipv4Addr = ntohl(ifaceAddr->sin_addr.s_addr);
    };

    // run the lambda in the network ns of the container
    if (nsEnterImpl(containerId, "net", getIpAddress) && (ipv4Addr != 0))
    {
        return ipv4Addr;
    }
    return 0;
}
}
}
