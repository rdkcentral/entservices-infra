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

AppcLifecycleManager::AppcLifecycleManager() :
    _currentService(nullptr), 
    _lifecycleManager(nullptr),
    _connectionId(0) 
{
    SYSLOG(Logging::Startup, (string(_T("AppcLifecycleManager Constructor"))));
}

AppcLifecycleManager::~AppcLifecycleManager()
{
    SYSLOG(Logging::Shutdown, (string(_T("AppcLifecycleManager Destructor"))));
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
            Exchange::JAppcLifecycleManager::Register(*this, this);
            SYSLOG(Logging::Startup, (string(_T("AppcLifecycleManager: Successfully acquired ILifecycleManager interface"))));
        } else {
            SYSLOG(Logging::Startup, (string(_T("AppcLifecycleManager: Failed to acquired ILifecycleManager interface"))));
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
    Exchange::JAppcLifecycleManager::Unregister(*this);

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


Core::hresult AppcLifecycleManager::SetTargetAppState(const string& appInstanceId, const Exchange::IAppcLifecycleManager::LifecycleState &targetState, const string& launchIntent)
{
    SYSLOG(Logging::Startup, (string(_T("AppcLifecycleManager::SetTargetAppState called."))));

     LOGINFO("SetTargetAppState called with AppInstanceId: %s, TargetState: %d, LaunchIntent: %s",
        appInstanceId.c_str(), targetState, launchIntent.c_str());

    if (!_lifecycleManager) {
        SYSLOG(Logging::Error, (string(_T("ILifecycleManager not available"))));
        return Core::ERROR_UNAVAILABLE;
    }


    uint32_t result = _lifecycleManager->SetTargetAppState(appInstanceId, static_cast<Exchange::ILifecycleManager::LifecycleState>(targetState), launchIntent);
    LOGINFO("SetTargetAppState returned: %d", result);

    return (result == Core::ERROR_NONE) ? Core::ERROR_NONE : result;
}


} // namespace Plugin
} // namespace WPEFramework
