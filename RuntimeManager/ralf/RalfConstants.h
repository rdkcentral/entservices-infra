#pragma once
#include <string>
#include <utility>

namespace ralf
{
    const std::string RALF_OCI_BASE_SPEC_FILE = "/usr/share/ralf/oci-base-spec.json";
    const std::string RALF_GRAPHICS_LAYER_PATH = "/usr/share/gpu-layer/";
    const std::string RALF_GRAPHICS_LAYER_ROOTFS = RALF_GRAPHICS_LAYER_PATH + "rootfs";
    const std::string RALF_GRAPHICS_LAYER_CONFIG = RALF_GRAPHICS_LAYER_PATH + "config.json";
    const std::string RALF_OVERLAYFS_TYPE = "overlay";
    const std::string RALF_APP_ROOTFS_DIR = "/tmp/ralf/";

    typedef std::pair<std::string, std::string> RalfPkgInfoPair; // <mountPoint, packageMetadataJsonPath>
} // namespace ralf