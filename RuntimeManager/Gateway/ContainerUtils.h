//
//  ContainerUtils.h
//  AppManager Gateway
//
//  Copyright © 2022 Sky UK. All rights reserved.
//

#ifndef CONTAINERUTILS_H
#define CONTAINERUTILS_H

#include <functional>
#include "tracing/Logging.h"
#include "UtilsLogging.h"
#include "tracing/Logging.h"

namespace WPEFramework
{
namespace Plugin
{

class ContainerUtils
{

public:
    enum Namespace {
        NetworkNamespace = 0x01,
        MountNamespace = 0x02,
        IpcNamespace = 0x04,
        PidNamespace = 0x08,
        UserNamespace = 0x10,
        UtsNamespace = 0x20
    };
/*
    static inline bool nsEnter(const QString &containerId, Namespace type,
                               const std::function<void()> &func)
    {
        return nsEnterImpl(containerId, type, func);
    }

    template< class Function, class... Args >
    static inline bool nsEnter(const QString &containerId, Namespace type,
                               Function&& f, Args&&... args)
    {
        return nsEnterImpl(containerId, type, std::bind(std::forward<Function>(f),
                                                        std::forward<Args>(args)...));
    }

    static pid_t execInNamespace(const QString &containerId,
                                 Namespaces namespaces, int processFd,
                                 const QStringList &arguments = { },
                                 const QStringList &envvars = { });
*/
    static uint32_t getContainerIpAddress(const std::string &containerId);

    /*
    static QList<QPair<pid_t, QString>> getContainerProcesses(const QString &containerId);

    static bool isContainer(const QString &containerId);

    static QByteArray readCGroupFile(const QString &containerId,
                                     const QLatin1String &cgroupName,
                                     const QLatin1String &fileName);

    static QMap<QString, QByteArray> readCGroupFiles(const QString &containerId,
                                                     const QLatin1String &cgroupName,
                                                     const QList<QLatin1String> &fileNames);

    static QDir openCGroupDirectory(const QString &containerId,
                                    const QLatin1String &cgroupName);

    static bool writeCGroupFile(const QString &containerId,
                                const QLatin1String &cgroupName,
                                const QLatin1String &fileName,
                                const QByteArray &data);
*/
private:
    static bool nsEnterImpl(const std::string &containerId, std::string type,
                            const std::function<void()> &func);
/*
    static pid_t execInNamespaceWithPid(pid_t containerPid,
                                        Namespaces namespaces,
                                        int processFd,
                                        const QStringList &arguments,
                                        const QStringList &envvars);
*/
};

}
}
#endif // CONTAINERUTILS_H
