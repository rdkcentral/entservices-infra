/*
* Copyright 2024 RDK Management
* Licensed under the Apache License, Version 2.0
*/

#include "AppcLifecycleManager.h"
#include <interfaces/ILifecycleManager.h>
#include <core/JSON.h>
#include <string>
#include <interfaces/json/JsonData_LifecycleManager.h>
#include <interfaces/json/JLifecycleManager.h>

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
    mCurrentService(nullptr), 
    mconnectionId(0), 
    _lifecycleManager(nullptr)
{
    if (_instance == nullptr) {
        _instance = this;
    }
}

AppcLifecycleManager::~AppcLifecycleManager()
{
    _instance = nullptr;
}

const string AppcLifecycleManager::Initialize(PluginHost::IShell* service)
{
    string message="";
    ASSERT(nullptr != service);
    ASSERT(nullptr == mCurrentService);
    ASSERT(nullptr == _lifecycleManager);
    ASSERT(0 == mconnectionId);

    mCurrentService = service;
    if (mCurrentService) {
        mCurrentService->AddRef();

        // Acquire ILifecycleManager interface
        _lifecycleManager = mCurrentService->QueryInterfaceByCallsign<Exchange::ILifecycleManager>("org.rdk.LifecycleManager");
        if (_lifecycleManager) {
            _lifecycleManager->AddRef();
            // Register JSON-RPC bridge
            Exchange::JLifecycleManager::Register(*this, _lifecycleManager);
        } else {
            message = "AppcLifecycleManager: Failed to acquire ILifecycleManager interface";
        }
    } else {
        message = "AppcLifecycleManager: IShell service is null";
    }

    if (!message.empty()) {
        Deinitialize(service);
    }
    return message;
}

void AppcLifecycleManager::Deinitialize(PluginHost::IShell* service)
{
    ASSERT(mCurrentService == service);

    // Unregister JSON-RPC bridge
    Exchange::JLifecycleManager::Unregister(*this);

    // Release ILifecycleManager interface
    if (_lifecycleManager) {
        _lifecycleManager->Release();
        _lifecycleManager = nullptr;
    }
    // Release IShell service
    if (mCurrentService) {
        mCurrentService->Release();
        mCurrentService = nullptr;
    }
    mconnectionId = 0;
}

string AppcLifecycleManager::Information() const
{
    return string();
}

void AppcLifecycleManager::Deactivated(RPC::IRemoteConnection* connection)
{
    if (connection->Id() == mconnectionId) {
        ASSERT(mCurrentService != nullptr);
        Core::IWorkerPool::Instance().Submit(PluginHost::IShell::Job::Create(mCurrentService, PluginHost::IShell::DEACTIVATED, PluginHost::IShell::FAILURE));
    }
}

void AppcLifecycleManager::get_settargetappstate(const JsonObject& params, JsonObject& response)
{
    if (!_lifecycleManager) {
        response["success"] = false;
        response["message"] = "ILifecycleManager not available";
        return;
    }

    WPEFramework::JsonData::LifecycleManager::SetTargetAppStateParamsData inputParams;
    inputParams.FromString(params.ToString());

    const string appInstanceId = inputParams.AppInstanceId.Value();
    const auto targetState = inputParams.TargetLifecycleState.Value();
    const string launchIntent = inputParams.LaunchIntent.Value();

    auto result = _lifecycleManager->SetTargetAppState(appInstanceId, targetState, launchIntent);
    response["success"] = (result == Core::ERROR_NONE);
    response["result"] = result;
}

} // namespace Plugin
} // namespace WPEFramework
