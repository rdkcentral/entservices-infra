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
    // unshare the specific namespace from the thread
    if (unshare(nsType) != 0)
    {
        //qErrnoWarning(errno, "failed to unshare");
        *success = false;
        return;
    }

    // switch into the new namespace
    if (setns(newNsFd, nsType) != 0)
    {
        //qErrnoWarning(errno, "failed to switch into new namespace");
        *success = false;
        return;
    }

    // execute the caller's function
    func();

    *success = true;
}

// -----------------------------------------------------------------------------
/*!
    \static
    \internal

    Utility function to run some code in a specific namespace given a file
    descriptor of the namespace.

    This function uses the setns syscall and therefore it must spawn a thread
    to run the callback in.  However this function blocks until the thread
    completes, so although it is multi-threaded it's API is blocking, i.e.
    effectively single threaded.

    The \a nsType argument should be one of the following values:
        CLONE_NEWIPC  - run in a IPC namespace
        CLONE_NEWNET  - run in a network namespace
        CLONE_NEWNS   - run in a mount namespace

 */
/*
static bool nsEnterWithFd(int namespaceFd, const std::function<void()> &func)
{
    bool success = false;

    // spawn the thread to run the callback in
    std::thread thread = std::thread(std::bind(&nsThread, namespaceFd, 0, &success, func));

    // block until the thread completes
    thread.join();

    return success;
}
*/

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
    char nsName[8];
    char nsPath[32];

    strcpy(nsName, "net");

    bool success;

    // get the namespace of the containered app
    sprintf(nsPath, "/proc/%d/ns/%s", pid, nsName);
    int newNsFd = open(nsPath, O_RDONLY | O_CLOEXEC);
    if (newNsFd < 0)
    {
//        qErrnoWarning(errno, "failed to open container namespace @ '%s'", nsPath);
        success = false;
    }
    else
    {
        // spawn the thread to run the callback in
        std::thread thread = std::thread(std::bind(&nsThread, newNsFd, nsType, &success, func));

        // block until the thread completes
        thread.join();
    }

    // close the namespaces
    if ((newNsFd >= 0) && (close(newNsFd) != 0))
    {
//        qErrnoWarning(errno, "failed to close namespace");
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
    // the container id is used as the name for the cgroups, so we can use to
    // get a list of pids within a cgroup, which effectively from our PoV is
    // list of pids within a container

    const std::string cgroupPath = "/sys/fs/cgroup/memory/" + containerId + "/cgroup.procs";
    int procsFd = open(cgroupPath.c_str(), O_RDONLY | O_CLOEXEC);
    if (procsFd < 0)
    {
        if (errno == ENOENT)
	{
            //qInfo("no cgroup file @ '%s'", qPrintable(cgroupPath));
	}
        else{
            //qErrnoWarning(errno, "failed to open cgroup file @ '%s'", qPrintable(cgroupPath));
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
        //qWarning("cgroup procs file is empty - no pids in container?");
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
    // find a pid in the container
    pid_t containerPid = findContainerPid(containerId);
    if (containerPid <= 0)
    {
        //qWarning("no container found with id '%s'", qPrintable(containerId));
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
            //qErrnoWarning(errno, "failed to create socket");
            return;
        }

        // attempt to get the interface details
        if (ioctl(sock, SIOCGIFADDR, &ifr) < 0)
        {
            //qErrnoWarning(errno, "failed to get interface ip address");
            close(sock);
            return;
        }

        if (close(sock) < 0)
        {
            //qErrnoWarning(errno, "failed to close ifcase socket");
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
    else
    {
        return 0;
    }
}

// -----------------------------------------------------------------------------
/*!
    Returns the list of processes within the container and their pids.  The
    names of the processes is taken from their comm values which means it is
    truncated to 15 characters.

 */
/*
QList<QPair<pid_t, QString>> ContainerUtils::getContainerProcesses(const QString &containerId)
{
    QList<QPair<pid_t, QString>> processes;

    // the container id is used as the name for the cgroups, so we can use to
    // get a list of pids within a cgroup, which effectively from our PoV is
    // list of pids within a container

    const QString cgroupPath = QLatin1String("/sys/fs/cgroup/memory/")
                               + containerId
                               + QLatin1String("/cgroup.procs");

    QFile cgroupProcs(cgroupPath);
    if (!cgroupProcs.open(QFile::ReadOnly))
    {
        qWarning("failed to open cgroup file @ '%s' - is container running",
                 qPrintable(cgroupPath));
        return processes;
    }

    QString line;
    QTextStream stream(&cgroupProcs);
    while (stream.readLineInto(&line))
    {
        // convert the line to an int
        bool ok = false;
        int pid = line.toInt(&ok);
        if (!ok || (pid < 2))
        {
            qWarning("failed to convert pid line '%s' to int", qPrintable(line));
            continue;
        }

        // try and read the comm string
        const QString procPath = QString::asprintf("/proc/%d/comm", pid);
        QFile commFile(procPath);
        if (!commFile.open(QFile::ReadOnly))
        {
            qWarning("failed to open file @ '%s'", qPrintable(procPath));
        }
        else
        {
            QByteArray comm = commFile.readAll().trimmed();
            if (comm.isEmpty())
            {
                qWarning("empty comm file @ '%s'", qPrintable(procPath));
            }
            else
            {
                processes.append(qMakePair(pid, comm));
                qDebug("found process '%s:%d' in container", comm.constData(), pid);
            }
        }
    }

    return processes;
}
*/

// -----------------------------------------------------------------------------
/*!
    \static
    \internal

    Attempts to fork / exec a process within one or more namespaces of the
    given container.

 */
/*
pid_t ContainerUtils::execInNamespaceWithPid(pid_t containerPid,
                                             Namespaces namespaces,
                                             int processFd,
                                             const QStringList &arguments,
                                             const QStringList &envvars)
{
    // fork off a process
    pid_t childPid = fork();
    if (childPid < 0)
    {
        qErrnoWarning(errno, "fork failed");
        return -1;
    }

    if (childPid == 0)
    {
        // within child, so close all fds we no longer need
        for (int fd = 3; fd < 256; fd++)
        {
            if (fd != processFd)
                close(fd);
        }

        // set the namespace(s) requested
        const struct NsDetails
        {
            Namespace ns;
            int nsType;
            const char *name;
        } nsDetails[] = {
            { UserNamespace,    CLONE_NEWUSER,  "user"  },
            { IpcNamespace,     CLONE_NEWIPC,   "ipc"   },
            { UtsNamespace,     CLONE_NEWUTS,   "uts"   },
            { NetworkNamespace, CLONE_NEWNET,   "net"   },
            { MountNamespace,   CLONE_NEWNS,    "mnt"   },
        };

        for (const NsDetails &details : nsDetails)
        {
            if (namespaces & details.ns)
            {
                char nsPath[64];
                sprintf(nsPath, "/proc/%d/ns/%s", containerPid, details.name);

                int ns = open(nsPath, O_RDONLY);
                if (ns < 0)
                {
                    fprintf(stderr, "failed to user ns @ '%s' - %d %s\n",
                            nsPath, errno, strerror(errno));
                }
                else
                {
                    if (setns(ns, details.nsType) != 0)
                    {
                        fprintf(stderr, "failed to set '%s' ns - %d %s\n",
                                details.name, errno, strerror(errno));
                    }
                    else
                    {
                        // fprintf(stderr, "set '%s' ns\n", nsPath);
                    }

                    close(ns);
                }
            }
        }

        // if setting a user namespace then always reset the uid / gid and groups
        if (namespaces & UserNamespace)
        {
            setgroups(0, nullptr);
            setgid(0);
            setuid(0);
        }

        // convert args and env vars to c arrays
        char **argv = (char**) calloc(sizeof(char*), arguments.length() + 1);
        for (int i = 0; i < arguments.length(); i++)
            argv[i] = strdup(qPrintable(arguments[i]));

        char **envp = (char**) calloc(sizeof(char*), envvars.length() + 1);
        for (int i = 0; i < envvars.length(); i++)
            envp[i] = strdup(qPrintable(envvars[i]));

        // exec the process
        syscall(SYS_execveat, processFd, "", argv, envp, int(AT_EMPTY_PATH));

        fprintf(stderr, "execveat call failed - %d %s\n", errno, strerror(errno));
        exit(EXIT_FAILURE);
    }

    qDebug("forked container process given pid %d", childPid);

    return childPid;

}
*/
// -----------------------------------------------------------------------------
/*!
    Attempts to fork / exec a process within one or more namespaces of the
    given container.

 */
/*
pid_t ContainerUtils::execInNamespace(const QString &containerId,
                                      Namespaces namespaces,
                                      int processFd,
                                      const QStringList &arguments,
                                      const QStringList &envvars)
{
    // find a pid in the container
    pid_t containerPid = findContainerPid(containerId);
    if (containerPid <= 0)
    {
        qWarning("no container found with id '%s'", qPrintable(containerId));
        return false;
    }

    qDebug("pid of container '%s' is %d", qPrintable(containerId), containerPid);

    // if wanting to exec with a different pid namespace then first spawn a
    // thread to run in the new pid namespace and them fork from that
    if (namespaces & PidNamespace)
    {
        pid_t result = -1;
        nsEnterWithPid(containerPid, CLONE_NEWPID,[&]()
                       {
                           result = execInNamespaceWithPid(containerPid, namespaces,
                                                           processFd, arguments, envvars);
                       });

        return result;
    }
    else
    {
        // not running in a pid namespace so can just fork / exec
        return execInNamespaceWithPid(containerPid, namespaces, processFd,
                                      arguments, envvars);
    }
}
*/

// -----------------------------------------------------------------------------
/*!
    \static

    Simple test to make sure that a container with the given ID is running.

 */
/*
bool ContainerUtils::isContainer(const QString &containerId)
{
    const QString cgroupPath = QLatin1String("/sys/fs/cgroup/memory/")
                               + containerId
                               + QLatin1String("/cgroup.procs");

    return (access(qPrintable(cgroupPath), F_OK) == 0);
}
*/

// -----------------------------------------------------------------------------
/*!
    \static

    Attempts to read the contents of a cgroup file for the container with given
    \a containerId.

 */
/*
QByteArray ContainerUtils::readCGroupFile(const QString &containerId,
                                          const QLatin1String &cgroupName,
                                          const QLatin1String &fileName)
{
    // construct the absolute path
    const QString path = QString("/sys/fs/cgroup/%1/%2/%1.%3")
        .arg(cgroupName).arg(containerId).arg(fileName);

    // attempt to open the cgroup file
    QFile file(path);
    if (!file.open(QFile::ReadOnly))
    {
        qWarning("failed to open cgroup file @ '%s' - %s",
                 qPrintable(path), qPrintable(file.errorString()));
        return QByteArray();
    }

    // read the entire file
    return file.readAll();
}
*/

// -----------------------------------------------------------------------------
/*!
    \static

    Attempts to read the contents of a bunch of cgroup files for a given
    container.

 */
/*
QMap<QString, QByteArray> ContainerUtils::readCGroupFiles(const QString &containerId,
                                                          const QLatin1String &cgroupName,
                                                          const QList<QLatin1String> &fileNames)
{
    // construct the absolute path to the cgroup
    char cgroupPath[256];
    snprintf(cgroupPath, sizeof(cgroupPath), "/sys/fs/cgroup/%s/%s",
             cgroupName.data(), qPrintable(containerId));

    int cgroupDirFd = open(cgroupPath, O_CLOEXEC | O_DIRECTORY);
    if (cgroupDirFd < 0)
    {
        qErrnoWarning(errno, "failed to open cgroup @ '%s'", cgroupPath);
        return { };
    }

    //
    QMap<QString, QByteArray> results;

    // for now it's a safe assumption that cgroup files are less than 4k in size
    char buffer[4096];

    // try and open and read all the files
    for (const QLatin1String &fileName : fileNames)
    {
        // prefix the file name with the cgroup name
        char cgroupFileName[256];
        snprintf(cgroupFileName, sizeof(cgroupFileName), "%s.%s",
                 cgroupName.data(), fileName.data());

        // try and open the file
        int fd = openat(cgroupDirFd, cgroupFileName, O_CLOEXEC | O_RDONLY);
        if (fd < 0)
        {
            qErrnoWarning(errno, "failed to open cgroup file @ '%s/%s'",
                          cgroupPath, cgroupFileName);
        }
        else
        {
            ssize_t rd = TEMP_FAILURE_RETRY(read(fd, buffer, sizeof(buffer)));
            if (rd < 0)
            {
                qErrnoWarning(errno, "failed to read cgroup file @ '%s/%s'",
                              cgroupPath, cgroupFileName);
            }
            else if (rd > 0)
            {
                buffer[rd - 1] = '\0';
                results.insert(fileName, QByteArray(buffer, rd));

                qDebug("read '%s' from cgroup file @ '%s/%s'",
                       buffer, cgroupPath, cgroupFileName);
            }

            close(fd);
        }
    }

    if (close(cgroupDirFd) != 0)
        qErrnoWarning(errno, "failed to close cgroup directory");

    return results;
}
*/

// -----------------------------------------------------------------------------
/*!
    \static

    Writes the given cgroup file

 */
/*
bool ContainerUtils::writeCGroupFile(const QString &containerId,
                                     const QLatin1String &cgroupName,
                                     const QLatin1String &fileName,
                                     const QByteArray &data)
{
    // construct the absolute path
    const QString path = QString("/sys/fs/cgroup/%1/%2/%1.%3")
        .arg(cgroupName).arg(containerId).arg(fileName);

    // attempt to open the cgroup file
    QFile file(path);
    if (!file.open(QFile::WriteOnly))
    {
        qWarning("failed to open cgroup file @ '%s' - %s",
                 qPrintable(path), qPrintable(file.errorString()));
        return false;
    }

    // write the file
    return (file.write(data) > 0);
}
*/

// -----------------------------------------------------------------------------
/*!
    \static

    Returns the directory object for a given container's cgroup directory.

 */
/*
QDir ContainerUtils::openCGroupDirectory(const QString &containerId,
                                         const QLatin1String &cgroupName)
{
    // construct the absolute path
    const QString path = QString("/sys/fs/cgroup/%1/%2")
        .arg(cgroupName).arg(containerId);

    // return a directory wrapping it
    return QDir(path);
}
*/
