#pragma once

#include "Module.h"
#include "UtilsLogging.h"

#include <essos-resmgr.h>
#include <interfaces/IRMTestHarness.h>

#include <map>
#include <mutex>
#include <string>
#include <vector>

namespace WPEFramework {
namespace Plugin {

class RMTestHarnessImplementation : public Exchange::IRMTestHarness {
public:
	    struct EventRecord {
        std::string event;
        int type;
        int id;
        uint64_t timestampMs;
    };

    RMTestHarnessImplementation();
    ~RMTestHarnessImplementation();

    Core::hresult InitClient(bool& success, std::string& clientId) override;
    Core::hresult DestroyClient(const std::string& clientId, bool& success) override;

    Core::hresult RequestResource(const std::string& clientId, int type, int usage, int priority,
                                  bool& success, int& assignedId) override;

    Core::hresult ReleaseResource(const std::string& clientId, int type, int assignedId, bool& success) override;

    Core::hresult CleanupAllClients(bool& success) override;
    Core::hresult GetPolicyPriorityTie(const std::string& clientId, bool& success, bool& requesterWins);
    Core::hresult GetAVState(const std::string& clientId, bool& success, int& state);
    Core::hresult ResourceGetCount(const std::string& clientId, int type, bool& success, int& count);
    Core::hresult ResourceGetOwner(const std::string& clientId, int type, int id, bool& success, int& client, int& priority);
    Core::hresult ResourceGetCaps(const std::string& clientId, int type, int id, bool& success, int& caps);
    Core::hresult ResourceSetState(const std::string& clientId, int type, int id, int state, bool& success);
    Core::hresult ResourceGetState(const std::string& clientId, int type, int id, bool& success, int& state);
    Core::hresult RequestSetPriority(const std::string& clientId, int type, int requestId, int priority, bool& success);
    Core::hresult RequestSetUsage(const std::string& clientId, int type, int requestId, int usage, bool& success);
    Core::hresult RequestCancel(const std::string& clientId, int type, int requestId, bool& success);
    Core::hresult DumpState(const std::string& clientId, bool& success);
    Core::hresult AddToBlackList(const std::string& clientId, const std::string& appId, bool& success);
    Core::hresult RemoveFromBlackList(const std::string& clientId, const std::string& appId, bool& success);
    Core::hresult GetBlackListState(const std::string& clientId, bool& success, bool& blackListEnabled);

    BEGIN_INTERFACE_MAP(RMTestHarnessImplementation)
        INTERFACE_ENTRY(Exchange::IRMTestHarness)
    END_INTERFACE_MAP

private:

    struct ClientContext {
        std::string clientId;
        EssRMgr* rm;
        EssRMgrRequest lastRequest;
        int lastAssignedId;
        int lastAssignedCaps;
        int lastType;
        std::vector<EventRecord> events;

        ClientContext()
            : rm(nullptr)
            , lastAssignedId(-1)
            , lastAssignedCaps(0)
            , lastType(-1) {
            memset(&lastRequest, 0, sizeof(lastRequest));
        }
    };

private:
    RMTestHarnessImplementation(const RMTestHarnessImplementation&) = delete;
    RMTestHarnessImplementation& operator=(const RMTestHarnessImplementation&) = delete;
    static uint64_t NowMs();

    static std::string EventName(int event);

    static void Notify(EssRMgr* rm, int event, int type, int id, void* userData);

    void CleanupAll();
    ClientContext* FindClient(const std::string& clientId);

    std::mutex mLock;
    std::map<std::string, ClientContext> mClients;
    uint32_t mNextClientId;
};

} // namespace Plugin
} // namespace WPEFramework
