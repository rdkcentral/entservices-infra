/*
* Copyright 2024 RDK Management
* Licensed under the Apache License, Version 2.0
*/

#include "AppcLifecycleManager.h"

#define APPC_LIFECYCLEMANAGER_API_VERSION_MAJOR 1
#define APPC_LIFECYCLEMANAGER_API_VERSION_MINOR 0
#define APPC_LIFECYCLEMANAGER_API_VERSION_PATCH 0

namespace WPEFramework
{
    namespace {
    static Plugin::Metadata<Plugin::AppcLifecycleManager> metadata(
        // Version (Major, Minor, Patch)
        APPC_LIFECYCLEMANAGER_API_VERSION_MAJOR, APPC_LIFECYCLEMANAGER_API_VERSION_MINOR, APPC_LIFECYCLEMANAGER_API_VERSION_PATCH,
         // Preconditions
        {},
        // Terminations
        {},
        // Controls
        {}
    );
    }

namespace Plugin 
{

SERVICE_REGISTRATION(AppcLifecycleManager, APPC_LIFECYCLEMANAGER_API_VERSION_MAJOR, APPC_LIFECYCLEMANAGER_API_VERSION_MINOR, APPC_LIFECYCLEMANAGER_API_VERSION_PATCH);

AppcLifecycleManager* AppcLifecycleManager::_instance = nullptr;

AppcLifecycleManager::AppcLifecycleManager() :
    _currentService(nullptr), 
    _lifecycleManager(nullptr),
    _connectionId(0) 
{
    SYSLOG(Logging::Startup, (string(_T("AppcLifecycleManager Constructor"))));
    if (_instance == nullptr) {
        _instance = this;
    }
}

AppcLifecycleManager::~AppcLifecycleManager()
{
    SYSLOG(Logging::Shutdown, (string(_T("AppcLifecycleManager Destructor"))));
    _instance = nullptr;
}

const string AppcLifecycleManager::Initialize(PluginHost::IShell* service)
{
    string message="";
    ASSERT(nullptr != service);
    ASSERT(nullptr == _currentService);
    ASSERT(nullptr == _lifecycleManager);
    ASSERT(0 == _connectionId);

    SYSLOG(Logging::Startup, (string(_T("AppcLifecycleManager::Initialize called."))));

    _currentService = service;
    if (_currentService) {
        _currentService->AddRef();

        // Acquire ILifecycleManager interface
        _lifecycleManager = _currentService->QueryInterfaceByCallsign<Exchange::ILifecycleManager>("org.rdk.LifecycleManager");
        if (_lifecycleManager) {
            _lifecycleManager->AddRef();
            // Register JSON-RPC bridge
            Exchange::JLifecycleManager::Register(*this, _lifecycleManager);
        } else {
            SYSLOG(Logging::Startup, (string(_T("AppcLifecycleManager: Successfully acquired ILifecycleManager interface"))));
            message = "AppcLifecycleManager: Failed to acquire ILifecycleManager interface";
        }
    } else {
        SYSLOG(Logging::Startup, (string(_T("AppcLifecycleManager: IShell service is null"))));
        message = "AppcLifecycleManager: IShell service is null";
    }

    if (!message.empty()) {
        Deinitialize(service);
    }
    return message;
}

void AppcLifecycleManager::Deinitialize(PluginHost::IShell* service)
{
    ASSERT(_currentService == service);
    SYSLOG(Logging::Shutdown, (string(_T("AppcLifecycleManager::Deinitialize"))));
    // Unregister JSON-RPC bridge
    Exchange::JLifecycleManager::Unregister(*this);

    // Release ILifecycleManager interface
    if (_lifecycleManager) {
        _lifecycleManager->Release();
        _lifecycleManager = nullptr;
    }
    // Release IShell service
    if (_currentService) {
        _currentService->Release();
        _currentService = nullptr;
    }
    _connectionId = 0;
}

string AppcLifecycleManager::Information() const
{
    SYSLOG(Logging::Startup, (string(_T("AppcLifecycleManager::Information called."))));
    return string();
}

void AppcLifecycleManager::Deactivated(RPC::IRemoteConnection* connection)
{
    SYSLOG(Logging::Startup, (string(_T("AppcLifecycleManager::Deactivated called."))));

    if (connection->Id() == _connectionId) {
        ASSERT(_currentService != nullptr);
        Core::IWorkerPool::Instance().Submit(PluginHost::IShell::Job::Create(_currentService, PluginHost::IShell::DEACTIVATED, PluginHost::IShell::FAILURE));
    }
}

void AppcLifecycleManager::get_settargetappstate(const JsonObject& params, JsonObject& response)
{
    SYSLOG(Logging::Startup, (string(_T("AppcLifecycleManager::get_settargetappstate called."))));

    if (!_lifecycleManager) {
        SYSLOG(Logging::Error, (string(_T("ILifecycleManager not available"))));
        response["success"] = false;
        response["message"] = "ILifecycleManager not available";
        return;
    }

    WPEFramework::JsonData::LifecycleManager::SetTargetAppStateParamsData inputParams;
    std::string jsonString;
    params.ToString(jsonString);
    inputParams.FromString(jsonString);

    const string appInstanceId = inputParams.AppInstanceId.Value();
    const auto targetState = inputParams.TargetLifecycleState.Value();
    const string launchIntent = inputParams.LaunchIntent.Value();

     LOGINFO("SetTargetAppState called with AppInstanceId: %s, TargetLifecycleState: %d, LaunchIntent: %s",
            appInstanceId.c_str(), targetState, launchIntent.c_str());
   
    auto result = _lifecycleManager->SetTargetAppState(appInstanceId, targetState, launchIntent);

    LOGINFO("SetTargetAppState returned: %d", result);

    response["success"] = (result == Core::ERROR_NONE);
    response["result"] = result;
}

} // namespace Plugin
} // namespace WPEFramework
