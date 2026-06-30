#include "RMTestHarness.h"

#define API_VERSION_NUMBER_MAJOR 1
#define API_VERSION_NUMBER_MINOR 0
#define API_VERSION_NUMBER_PATCH 0

namespace WPEFramework {
namespace Plugin {

namespace {
    static Metadata<RMTestHarness> metadata(
        API_VERSION_NUMBER_MAJOR,
        API_VERSION_NUMBER_MINOR,
        API_VERSION_NUMBER_PATCH,
        {},
        {},
        {}
    );
}

SERVICE_REGISTRATION(RMTestHarness, API_VERSION_NUMBER_MAJOR, API_VERSION_NUMBER_MINOR, API_VERSION_NUMBER_PATCH);

RMTestHarness::RMTestHarness()
    : PluginHost::JSONRPC()
    , mService(nullptr)
    , mConnectionId(0)
    , mRMTestHarness(nullptr) {
}

RMTestHarness::~RMTestHarness() {
}

const string RMTestHarness::Initialize(PluginHost::IShell* service) {
    string message;

    ASSERT(mService == nullptr);
    ASSERT(mRMTestHarness == nullptr);

    mConnectionId = 0;
    mService = service;

    mRMTestHarness = mService->Root<Exchange::IRMTestHarness>(mConnectionId, 5000, _T("RMTestHarnessImplementation"));
    if (mRMTestHarness == nullptr) {
        message = _T("RMTestHarness implementation could not be instantiated.");
        mService = nullptr;
        return message;
    }

    Exchange::JRMTestHarness::Register(*this, mRMTestHarness);

    return string();
}

void RMTestHarness::Deinitialize(PluginHost::IShell* /* service */) {
    if (mRMTestHarness != nullptr) {
        Exchange::JRMTestHarness::Unregister(*this);

        mRMTestHarness->Release();
        mRMTestHarness = nullptr;
    }

    mService = nullptr;
}

string RMTestHarness::Information() const {
    return string("{\"service\": \"org.rdk.RMTestHarness\"}");
}

} // namespace Plugin
} // namespace WPEFramework