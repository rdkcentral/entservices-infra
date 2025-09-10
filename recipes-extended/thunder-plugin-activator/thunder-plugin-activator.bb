DESCRIPTION = "ThunderPluginActivator: command-line tool to activate Thunder plugins"
HOMEPAGE = "https://github.com/rdkcentral/ThunderPluginActivator"
LICENSE = "Apache-2.0"
LIC_FILES_CHKSUM = "file://LICENSE;md5=86d3f3a95c324c9479bd8986968f4327"

PACKAGE_ARCH = "${MIDDLEWARE_ARCH}"
DEPENDS = "cmake-native wpeframework-tools-native wpeframework"

PV = "1.1.0"
PR = "r0"

SRC_URI = "git://github.com/rdkcentral/ThunderPluginActivator;protocol=https;branch=main;name=thunderpluginactivator"

SRCREV = "b7ae1f44f3b6cc23a1bb63465c73b06dc0119407"

S = "${WORKDIR}/git"

inherit cmake pkgconfig

FILES:${PN} += "${bindir}/PluginActivator"
