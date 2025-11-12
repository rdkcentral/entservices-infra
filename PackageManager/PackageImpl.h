#pragma once

#include "Module.h"
#include "UtilsLogging.h"

#include <ralf/VerificationBundle.h>

#include <IPackageImpl.h>

namespace packagemanager {

class PackageImpl : public IPackageImpl {
    public:
    PackageImpl();
    ~PackageImpl();

    Result Initialize(const std::string &configStr, ConfigMetadataArray &aConfigMetadata) override;

    Result Install(const std::string &packageId, const std::string &version, const NameValues &additionalMetadata, const std::string &fileLocator, ConfigMetaData &configMetadata) override;
    Result Uninstall(const std::string &packageId) override;

    Result Lock(const std::string &packageId, const std::string &version, std::string &unpackedPath, ConfigMetaData &configMetadata, NameValues &additionalLocks) override;
    Result Unlock(const std::string &packageId, const std::string &version) override;
    Result GetFileMetadata(const std::string &fileLocator, std::string &packageId, std::string &version, ConfigMetaData &configMetadata) override;

    private:
    //Result GetPackageMetaData(PackagePtr package, PackageMetadataPtr metaData, ConfigMetaData& configMetadata);
    //std::shared_ptr<packagemanager::IPackageManagerConfig> pmConfig;
    LIBRALF_NS::VerificationBundle vb;

    const string certPath = "/etc/sky/certs/production.crt";
    const string packageDir = "/media/apps/sky/packages/";
    const string packageFile = "package.wgt";
    const string extractionDir = "/var/run/sky/extracted/";


};

}
