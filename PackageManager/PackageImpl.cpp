#include <iostream>
#include <filesystem>

#include <ralf/Logging.h>
#include <ralf/Package.h>

#include "PackageImpl.h"

namespace packagemanager {
using namespace LIBRALF_NS;
//using LpResult = packagemanager::Result ;

PackageImpl::PackageImpl()
{
    LOGDBG();
    auto cert = Certificate::loadFromFile(certPath);
    if (cert.isError()) {
        LOGERR("Failed to load certificate %s error=%s", certPath.c_str(), cert.error().what());
    } else {
        LOGDBG("load certificate %s ", certPath.c_str());
        vb.addCertificate(cert.value());
    }
}

PackageImpl::~PackageImpl() {

}

Result PackageImpl:: PackageImpl::Initialize(const std::string &configStr, ConfigMetadataArray &aConfigMetadata)
{
    Result result = SUCCESS;

    std::error_code ec;
    for (auto const &dirEntry : std::filesystem::directory_iterator(packageDir, ec)) {
        if (ec.value() != 0)
            break;

        if (dirEntry.is_directory()) {
            auto file = dirEntry.path().string() + "/" + packageFile;
            if (std::filesystem::exists(file)) {
                auto package = Package::openWithoutVerification(file);
                if (!package.isError()) {
                    ConfigMetaData configMetadata;
                    auto data = package->metaData();
                    LOGDBG("Loading %18s %s", data->versionName().c_str(), data->id().c_str() );
                    // data->type() data->runtimeInfo();
                    ConfigMetadataKey key {data->id().c_str(), data->versionName()};
                    aConfigMetadata.insert({key, configMetadata});
                }
            }
        }
    }

    return result;
}


Result PackageImpl:: Install(const std::string &packageId, const std::string &version, const NameValues &additionalMetadata, const std::string &fileLocator, ConfigMetaData &configMetadata)
{
    Result result = SUCCESS;
    LOGDBG();

    setLogLevel(nullptr, LogPriority::Warning);

    LOGDBG();
    auto package = Package::openWithoutVerification(fileLocator);
    //auto package = Package::open(fileLocator, vb);
    if (!package.isError()) {
        //LOGDBG("format: %d". package->format());
        LOGDBG("%s installing", fileLocator.c_str());
        auto result = package->verify();

        auto data = package->metaData();
        LOGDBG("%s %s ver=%s type=%d", fileLocator.c_str(), data->id().c_str(), data->versionName().c_str(), (int)data->type());
    } else {
        LOGERR("Failed to install %s error=%s", fileLocator.c_str(), package.error().what());
    }

    LOGDBG();
    return result;
}


Result PackageImpl:: Uninstall(const std::string &packageId)
{
    Result result = SUCCESS;

    return result;
}


Result PackageImpl:: Lock(const std::string &packageId, const std::string &version, std::string &unpackedPath, ConfigMetaData &configMetadata, NameValues &additionalLocks)
{
    Result result = SUCCESS;
    string packageFile = packageDir + packageId + "/" + packageFile;
    auto package = Package::open(packageFile, vb);
    if (!package.isError()) {
        //LOGDBG("format: %d". package->format());
        LOGDBG("%s installing", packageFile.c_str());
        auto result = package->verify();

        string pkgExtDir = extractionDir + packageId;
        std::filesystem::create_directory(pkgExtDir);
        package->extractTo(pkgExtDir);

        auto data = package->metaData();
        LOGDBG("%s %s ver=%s type=%d", packageFile.c_str(), data->id().c_str(), data->versionName().c_str(), (int)data->type());
    } else {
        LOGERR("Failed to install %s error=%s", packageFile.c_str(), package.error().what());
    }

   return result;
}

Result PackageImpl:: Unlock(const std::string &packageId, const std::string &version)
{
    Result result = SUCCESS;

    return result;
}

Result PackageImpl:: GetFileMetadata(const std::string &fileLocator, std::string &packageId, std::string &version, ConfigMetaData &configMetadata)
{
    Result result = SUCCESS;

    return result;
}


std::shared_ptr<packagemanager::IPackageImpl> IPackageImpl::instance()
{
    std::shared_ptr<packagemanager::IPackageImpl> packageImpl =
        std::make_shared<packagemanager::PackageImpl>();

    return packageImpl;
}

}

    // Following result in crash
    // auto cert = Certificate();
    // vb.addCertificate(cert);
    // auto package = Package::open(fileLocator, vb);
