#include "RMTestHarnessImplementation.h"

#include <chrono>
#include <cstring>

namespace WPEFramework {
namespace Plugin {

SERVICE_REGISTRATION(RMTestHarnessImplementation, 1, 0);

RMTestHarnessImplementation::RMTestHarnessImplementation() {
    mNextClientId = 1;
    LOGINFO("Constructing RMTestHarnessImplementation: %p", this);
}

RMTestHarnessImplementation::~RMTestHarnessImplementation() {
    CleanupAll();
    LOGINFO("Destructing RMTestHarnessImplementation: %p", this);
}

/**
 * @brief Get current time in milliseconds.
 * @return Current timestamp in milliseconds.
 */
uint64_t RMTestHarnessImplementation::NowMs() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
}

/**
 * @brief Convert event enum to string name.
 * @param event [in] Event enumeration value (EssRMgrEvent_granted, EssRMgrEvent_revoked).
 * @return String representation of the event.
 */
std::string RMTestHarnessImplementation::EventName(int event) {
    switch (event) {
        case EssRMgrEvent_granted:
            return "granted";
        case EssRMgrEvent_revoked:
            return "revoked";
        default:
            return "unknown";
    }
}

/**
 * @brief Callback function invoked by resource manager on events.
 * @param rm [in] Resource manager context.
 * @param event [in] Event type (EssRMgrEvent_granted or EssRMgrEvent_revoked).
 * @param type [in] Resource type.
 * @param id [in] Resource id.
 * @param userData [in] User data pointer (ClientContext*).
 */
void RMTestHarnessImplementation::Notify(EssRMgr* rm, int event, int type, int id, void* userData) {
    ClientContext* ctx = static_cast<ClientContext*>(userData);
    if (ctx == nullptr) {
        return;
    }

    EventRecord e;
    e.event = EventName(event);
    e.type = type;
    e.id = id;
    e.timestampMs = NowMs();
    ctx->events.push_back(e);

    if (event == EssRMgrEvent_granted) {
        ctx->lastAssignedId = id;
    } else if (event == EssRMgrEvent_revoked) {
        EssRMgrReleaseResource(rm, type, id);

        EventRecord released;
        released.event = "released";
        released.type = type;
        released.id = id;
        released.timestampMs = NowMs();
        ctx->events.push_back(released);

        ctx->lastAssignedId = -1;
    }
}

/**
 * @brief Initialize a new client context.
 * @param success [out] Operation result.
 * @param clientId [out] Auto-assigned logical client identifier.
 * @return Core::hresult indicating success or failure.
 */
Core::hresult RMTestHarnessImplementation::InitClient(bool& success, std::string& clientId) {
    success = false;

    std::lock_guard<std::mutex> guard(mLock);

    EssRMgr* rm = EssRMgrCreate();
    if (rm == nullptr) {
        return Core::ERROR_GENERAL;
    }

    const std::string newClientId = std::string("client") + std::to_string(mNextClientId++);

    ClientContext ctx;
    ctx.clientId = newClientId;
    ctx.rm = rm;
    mClients[newClientId] = ctx;

    clientId = newClientId;
    success = true;

    return Core::ERROR_NONE;
}

/**
 * @brief Destroy and cleanup a client context.
 * @param clientId [in] Logical client identifier.
 * @param success [out] Operation result.
 * @return Core::hresult indicating success or failure.
 */
Core::hresult RMTestHarnessImplementation::DestroyClient(const std::string& clientId, bool& success) {
    success = false;

    std::lock_guard<std::mutex> guard(mLock);

    auto it = mClients.find(clientId);
    if (it == mClients.end()) {
        return Core::ERROR_NONE;
    }

    if (it->second.rm != nullptr) {
        if (it->second.lastAssignedId >= 0 && it->second.lastType >= 0) {
            EssRMgrReleaseResource(it->second.rm, it->second.lastType, it->second.lastAssignedId);
        }
        EssRMgrDestroy(it->second.rm);
        it->second.rm = nullptr;
    }

    mClients.erase(it);
    success = true;
    return Core::ERROR_NONE;
}

/**
 * @brief Request a resource for a client.
 * @param clientId [in] Logical client identifier.
 * @param type [in] Resource type.
 * @param usage [in] Usage bitmask.
 * @param priority [in] Arbitration priority.
 * @param success [out] Operation result.
 * @param assignedId [out] Assigned resource id (-1 if failed).
 * @return Core::hresult indicating success or failure.
 */
Core::hresult RMTestHarnessImplementation::RequestResource(const std::string& clientId, int type, int usage, int priority,
                                                           bool& success, int& assignedId) {
    success = false;
    assignedId = -1;

    std::lock_guard<std::mutex> guard(mLock);
    ClientContext* ctx = FindClient(clientId);
    if (ctx == nullptr || ctx->rm == nullptr) {
        return Core::ERROR_NONE;
    }

    memset(&ctx->lastRequest, 0, sizeof(EssRMgrRequest));
    ctx->lastRequest.type = type;
    ctx->lastRequest.usage = usage;
    ctx->lastRequest.priority = priority;
    ctx->lastRequest.asyncEnable = true;
    ctx->lastRequest.notifyCB = &RMTestHarnessImplementation::Notify;
    ctx->lastRequest.notifyUserData = ctx;
    ctx->lastRequest.requestId = -1;
    ctx->lastRequest.assignedId = -1;
    ctx->lastRequest.assignedCaps = 0;

    success = EssRMgrRequestResource(ctx->rm, type, &ctx->lastRequest);
    ctx->lastType = type;
    ctx->lastAssignedId = ctx->lastRequest.assignedId;
    ctx->lastAssignedCaps = ctx->lastRequest.assignedCaps;

    assignedId = ctx->lastRequest.assignedId;
    return Core::ERROR_NONE;
}

/**
 * @brief Release a resource assigned to a client.
 * @param clientId [in] Logical client identifier.
 * @param type [in] Resource type.
 * @param assignedId [in] Assigned resource id.
 * @param success [out] Operation result.
 * @return Core::hresult indicating success or failure.
 */
Core::hresult RMTestHarnessImplementation::ReleaseResource(const std::string& clientId, int type, int assignedId, bool& success) {
    success = false;

    std::lock_guard<std::mutex> guard(mLock);
    ClientContext* ctx = FindClient(clientId);
    if (ctx == nullptr || ctx->rm == nullptr) {
        return Core::ERROR_NONE;
    }

    EssRMgrReleaseResource(ctx->rm, type, assignedId);
    success = true;

    EventRecord e;
    e.event = "released";
    e.type = type;
    e.id = assignedId;
    e.timestampMs = NowMs();
    ctx->events.push_back(e);
    ctx->lastAssignedId = -1;

    return Core::ERROR_NONE;
}

/**
 * @brief Cleanup and destroy all client contexts.
 * @param success [out] Operation result.
 * @return Core::hresult (always Core::ERROR_NONE).
 */
Core::hresult RMTestHarnessImplementation::CleanupAllClients(bool& success) {
    CleanupAll();
    success = true;
    return Core::ERROR_NONE;
}

Core::hresult RMTestHarnessImplementation::GetPolicyPriorityTie(const std::string& clientId, bool& success, bool& requesterWins) {
    success = false;
    requesterWins = true;

    std::lock_guard<std::mutex> guard(mLock);
    ClientContext* ctx = FindClient(clientId);
    if (ctx == nullptr || ctx->rm == nullptr) {
        return Core::ERROR_NONE;
    }

    requesterWins = EssRMgrGetPolicyPriorityTie(ctx->rm);
    success = true;
    return Core::ERROR_NONE;
}

Core::hresult RMTestHarnessImplementation::GetAVState(const std::string& clientId, bool& success, int& state) {
    success = false;
    state = 0;

    std::lock_guard<std::mutex> guard(mLock);
    ClientContext* ctx = FindClient(clientId);
    if (ctx == nullptr || ctx->rm == nullptr) {
        return Core::ERROR_NONE;
    }

    success = EssRMgrGetAVState(ctx->rm, &state);
    return Core::ERROR_NONE;
}

Core::hresult RMTestHarnessImplementation::ResourceGetCount(const std::string& clientId, int type, bool& success, int& count) {
    success = false;
    count = 0;

    std::lock_guard<std::mutex> guard(mLock);
    ClientContext* ctx = FindClient(clientId);
    if (ctx == nullptr || ctx->rm == nullptr) {
        return Core::ERROR_NONE;
    }

    count = EssRMgrResourceGetCount(ctx->rm, type);
    success = true;
    return Core::ERROR_NONE;
}

Core::hresult RMTestHarnessImplementation::ResourceGetOwner(const std::string& clientId, int type, int id, bool& success, int& client, int& priority) {
    success = false;
    client = 0;
    priority = 0;

    std::lock_guard<std::mutex> guard(mLock);
    ClientContext* ctx = FindClient(clientId);
    if (ctx == nullptr || ctx->rm == nullptr) {
        return Core::ERROR_NONE;
    }

    success = EssRMgrResourceGetOwner(ctx->rm, type, id, &client, &priority);
    return Core::ERROR_NONE;
}

Core::hresult RMTestHarnessImplementation::ResourceGetCaps(const std::string& clientId, int type, int id, bool& success, int& caps) {
    success = false;
    caps = 0;

    std::lock_guard<std::mutex> guard(mLock);
    ClientContext* ctx = FindClient(clientId);
    if (ctx == nullptr || ctx->rm == nullptr) {
        return Core::ERROR_NONE;
    }

    EssRMgrCaps outCaps;
    memset(&outCaps, 0, sizeof(EssRMgrCaps));
    success = EssRMgrResourceGetCaps(ctx->rm, type, id, &outCaps);
    if (success) {
        caps = outCaps.capabilities;
    }
    return Core::ERROR_NONE;
}

Core::hresult RMTestHarnessImplementation::ResourceSetState(const std::string& clientId, int type, int id, int state, bool& success) {
    success = false;

    std::lock_guard<std::mutex> guard(mLock);
    ClientContext* ctx = FindClient(clientId);
    if (ctx == nullptr || ctx->rm == nullptr) {
        return Core::ERROR_NONE;
    }

    success = EssRMgrResourceSetState(ctx->rm, type, id, state);
    return Core::ERROR_NONE;
}

Core::hresult RMTestHarnessImplementation::ResourceGetState(const std::string& clientId, int type, int id, bool& success, int& state) {
    success = false;
    state = 0;

    std::lock_guard<std::mutex> guard(mLock);
    ClientContext* ctx = FindClient(clientId);
    if (ctx == nullptr || ctx->rm == nullptr) {
        return Core::ERROR_NONE;
    }

    success = EssRMgrResourceGetState(ctx->rm, type, id, &state);
    return Core::ERROR_NONE;
}

Core::hresult RMTestHarnessImplementation::RequestSetPriority(const std::string& clientId, int type, int requestId, int priority, bool& success) {
    success = false;

    std::lock_guard<std::mutex> guard(mLock);
    ClientContext* ctx = FindClient(clientId);
    if (ctx == nullptr || ctx->rm == nullptr) {
        return Core::ERROR_NONE;
    }

    success = EssRMgrRequestSetPriority(ctx->rm, type, requestId, priority);
    return Core::ERROR_NONE;
}

Core::hresult RMTestHarnessImplementation::RequestSetUsage(const std::string& clientId, int type, int requestId, int usage, bool& success) {
    success = false;

    std::lock_guard<std::mutex> guard(mLock);
    ClientContext* ctx = FindClient(clientId);
    if (ctx == nullptr || ctx->rm == nullptr) {
        return Core::ERROR_NONE;
    }

    EssRMgrUsage usageData;
    memset(&usageData, 0, sizeof(EssRMgrUsage));
    usageData.usage = usage;
    usageData.info = ctx->lastRequest.info;

    success = EssRMgrRequestSetUsage(ctx->rm, type, requestId, &usageData);
    return Core::ERROR_NONE;
}

Core::hresult RMTestHarnessImplementation::RequestCancel(const std::string& clientId, int type, int requestId, bool& success) {
    success = false;

    std::lock_guard<std::mutex> guard(mLock);
    ClientContext* ctx = FindClient(clientId);
    if (ctx == nullptr || ctx->rm == nullptr) {
        return Core::ERROR_NONE;
    }

    EssRMgrRequestCancel(ctx->rm, type, requestId);
    success = true;
    return Core::ERROR_NONE;
}

Core::hresult RMTestHarnessImplementation::DumpState(const std::string& clientId, bool& success) {
    success = false;

    std::lock_guard<std::mutex> guard(mLock);
    ClientContext* ctx = FindClient(clientId);
    if (ctx == nullptr || ctx->rm == nullptr) {
        return Core::ERROR_NONE;
    }

    EssRMgrDumpState(ctx->rm);
    success = true;
    return Core::ERROR_NONE;
}

Core::hresult RMTestHarnessImplementation::AddToBlackList(const std::string& clientId, const std::string& appId, bool& success) {
    success = false;

    std::lock_guard<std::mutex> guard(mLock);
    ClientContext* ctx = FindClient(clientId);
    if (ctx == nullptr || ctx->rm == nullptr) {
        return Core::ERROR_NONE;
    }

    success = EssRMgrAddToBlackList(ctx->rm, appId.c_str());
    return Core::ERROR_NONE;
}

Core::hresult RMTestHarnessImplementation::RemoveFromBlackList(const std::string& clientId, const std::string& appId, bool& success) {
    success = false;

    std::lock_guard<std::mutex> guard(mLock);
    ClientContext* ctx = FindClient(clientId);
    if (ctx == nullptr || ctx->rm == nullptr) {
        return Core::ERROR_NONE;
    }

    success = EssRMgrRemoveFromBlackList(ctx->rm, appId.c_str());
    return Core::ERROR_NONE;
}

Core::hresult RMTestHarnessImplementation::GetBlackListState(const std::string& clientId, bool& success, bool& blackListEnabled) {
    success = false;
    blackListEnabled = false;

    std::lock_guard<std::mutex> guard(mLock);
    ClientContext* ctx = FindClient(clientId);
    if (ctx == nullptr || ctx->rm == nullptr) {
        return Core::ERROR_NONE;
    }

    blackListEnabled = EssRMgrGetBlackListState(ctx->rm);
    success = true;
    return Core::ERROR_NONE;
}

/**
 * @brief Find a client context by identifier.
 * @param clientId [in] Logical client identifier.
 * @return Pointer to ClientContext if found, nullptr otherwise.
 */
RMTestHarnessImplementation::ClientContext* RMTestHarnessImplementation::FindClient(const std::string& clientId) {
    auto it = mClients.find(clientId);
    if (it == mClients.end()) {
        return nullptr;
    }
    return &it->second;
}

/**
 * @brief Cleanup and destroy all client contexts (thread-safe).
 * Releases all assigned resources and destroys all resource manager contexts.
 */
void RMTestHarnessImplementation::CleanupAll() {
    std::lock_guard<std::mutex> guard(mLock);

    for (auto& entry : mClients) {
        ClientContext& ctx = entry.second;
        if (ctx.rm != nullptr) {
            if (ctx.lastAssignedId >= 0 && ctx.lastType >= 0) {
                EssRMgrReleaseResource(ctx.rm, ctx.lastType, ctx.lastAssignedId);
            }
            EssRMgrDestroy(ctx.rm);
            ctx.rm = nullptr;
        }
    }

    mClients.clear();
}

} // namespace Plugin
} // namespace WPEFramework

