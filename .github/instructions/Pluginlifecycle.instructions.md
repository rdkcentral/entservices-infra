---
description: Guidelines for C++ files and header files that share the same name as their parent folder.
applyTo: "**/**.cpp,**/**.h"
---


## Mandatory Lifecycle Methods

  Every plugin must implement:

    - Initialize(IShell* service) → Called when the plugin is activated.
    
    - Deinitialize(IShell* service) → Called when the plugin is deactivated.
   
  ## Initialization
      
      ### Requirement
          
          - Initialize() must handle all setup logic; constructors should remain minimal.
          - It must validate inputs and acquire necessary references.
          
      ### Example
         const string HdcpProfile::Initialize(PluginHost::IShell* service) {
            .....
            if (_hdcpProfile != nullptr) {
                  ...
                  Exchange::IConfiguration* configure = _hdcpProfile->QueryInterface<Exchange::IConfiguration>();
                  ...
            }
            ....
          }
          
          - SHOULD register your listener object twice:

              Framework Service (_service): Use _service->Register(listener) to receive general plugin state change notifications (like ACTIVATED/DEACTIVATED).
                  
                        Example: _service->Register(&_hdcpProfileNotification);
                  
              Target Plugin Interface (_hdcpProfile): Use _hdcpProfile->Register(listener) to receive the plugin's specific custom events (e.g., onProfileChanged).
                  
                        Example: _hdcpProfile->Register(&_hdcpProfileNotification);
                   
          - It must return a non-empty string on failure with a clear error message.
    
              
             Example:
    
                   const string HdcpProfile::Initialize(PluginHost::IShell* service) {
                       ...
                       message = _T("HdcpProfile could not be configured");
                       ...
                       message = _T("HdcpProfile implementation did not provide a configuration interface");
                       ...
                       message = _T("HdcpProfile plugin could not be initialised");
                       ...
                       
                   }
                   
          - Threads or async tasks should be started here if needed, with proper tracking.
    
               Example:
    
                  Core::hresult NativeJSImplementation::Initialize(string waylandDisplay)
                  {   
                      std::cout << "initialize called on nativejs implementation " << std::endl;
                      mRenderThread = std::thread([=](std::string waylandDisplay) {
                          mNativeJSRenderer = std::make_shared<NativeJSRenderer>(waylandDisplay);
                          mNativeJSRenderer->run();	
                          std::cout << "After launch application execution ... " << std::endl;
                        	mNativeJSRenderer.reset();
                          }, waylandDisplay);
                      return (Core::ERROR_NONE);
                  }

          - Before executing Initialize, ensure all private member variables are in a reset state (either initialized by the constructor or cleared by a prior Deinitialize). Validate this by asserting their default values.

              Example:
              
                    const string HdcpProfile::Initialize(PluginHost::IShell *service)
                    {
                      ASSERT(_server == nullptr);
                      ASSERT(_impl == nullptr);
                      ASSERT(_connectionId == 0);
                    }

          - If a plugin A needs to communicate with plugin B(via com-rpc or json-rpc) , then it should call AddRef() on the IShellService instance passed as input to increment its reference count. IShellService instance should not be reference counted(using AddRef()) when it is not going to communicate with any other plugins.

              Example:

                   const string HdcpProfile::Initialize(PluginHost::IShell *service)
                    {
                      ...
                       _service->AddRef();
                       //Accessing other Plugins via COM-RPC or JSON-RPC in upcoming methods.
                       ...
                    }

          - Only one Initialize() method must exist — avoid overloads or split logic.
    
  ### Deinitialize and Cleanup
    
      ### Requirement
    
          - Deinitialize() must clean up all resources acquired during Initialize().
          - It must release resources in reverse order of initialization.
          - Every pointer or instance must be checked for nullptr before cleanup.
              Example:
               void HdcpProfile::Deinitialize(PluginHost::IShell* service) {
                  ...
                  if (_service != nullptr) {
                           _service->Release();
                           _service = nullptr;
                       }
                  ...
                }   
                
          - All acquired interfaces must be explicitly Released().

              Example:

                 void HdcpProfile::Deinitialize(PluginHost::IShell* service) {
                   ...
                     if (_hdcpProfile != nullptr) {
                        ....
                        // Release interface
                        RPC::IRemoteConnection* connection = service->RemoteConnection(_connectionId);
                        connection->Terminate();
                        connection->Release();
                       ....
                    }
                   ...
                }   
          - Notification handlers should be unregistered before releasing interfaces.
              
              Example:
               void HdcpProfile::Deinitialize(PluginHost::IShell* service) {
                  ...
                  // Unregister notifications first
                    if (_service != nullptr) {
                        _service->Unregister(&_hdcpProfileNotification);
                    }
                  ...
                }   
                
          - Remote connections must be terminated after releasing plugin references.
    
              Example:
               void HdcpProfile::Deinitialize(PluginHost::IShell* service) {
                   ...
                   if (_hdcpProfile != nullptr) {
                      
                        ....
                        if (nullptr != connection)
                        {
                            // Lets trigger the cleanup sequence for
                            // out-of-process code. Which will guard
                            // that unwilling processes, get shot if
                            // not stopped friendly :-)
                            connection->Terminate();
                            connection->Release();
                        }
                       ....
                  }
              }   
                
          - Threads must be joined or safely terminated.
    
               Example:
                 Core::hresult NativeJSImplementation::Deinitialize()
                {
                   LOGINFO("deinitializing NativeJS process");
                   if (mNativeJSRenderer)
                   {
                       mNativeJSRenderer->terminate();
                       if (mRenderThread.joinable())
                       {
                           mRenderThread.join();
                       }
                   }
        	         return (Core::ERROR_NONE);
                }
          - Internal state (e.g., _connectionId, _service) and private members should be reset to their default state.
    
               Example:
               void HdcpProfile::Deinitialize(PluginHost::IShell* service) {
                  ...
                   if (connection != nullptr) {
                            connection->Terminate();
                            connection->Release();
                        }
                  ...
                     if (_service != nullptr) {
                        _service->Release();
                        _service = nullptr;
                    }
                }   
          
          - If addref() is called on IShellService instance in Initialize() , then it should call Release() on the IShellService instance to decrement its reference count.
          
              Example:

                void HdcpProfile::Deinitialize(PluginHost::IShell* service) {
                   ...
                   if (_service != nullptr)
                   {
                      _service->Release();
                      _service = nullptr;
                   }
                   ...
                }
                
          - All cleanup steps should be logged for traceability.
    
               Example:
    
               void HdcpProfile::Deinitialize(PluginHost::IShell* service) {
                   ...
                   SYSLOG(Logging::Shutdown, (_T("HdcpProfile de-initialised")));
                   ...
                }


  ### Deactivated

      Each plugin should implement the deactivated method. In Deactivated, it should be checked if remote connectionId matches yours plugin's connectionId.If it matches , then deactivate the plugin.

        ### Example
      
            void XCast::Deactivated(RPC::IRemoteConnection *connection)
            {
                  if (connection->Id() == _connectionId)
                  {
                      ASSERT(nullptr != _service);
                      Core::IWorkerPool::Instance().Submit(PluginHost::IShell::Job::Create(_service, PluginHost::IShell::DEACTIVATED, PluginHost::IShell::FAILURE));
                  }
            }

