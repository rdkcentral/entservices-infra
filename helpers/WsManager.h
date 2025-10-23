/*
 * If not stated otherwise in this file or this component's LICENSE file the
 * following copyright and licenses apply:
 *
 * Copyright 2020 Metrological
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#pragma once

#include <mutex>
#include <memory>
#include <plugins/plugins.h>
#include "UtilsLogging.h"
#include "WebSocketLink.h"


// TODO: Remove once IsNullValue() in core/JSON.h is fixed
// and replace StreamJSONOneShotType with StreamJSONType
#include "StreamJSONOneShot.h"

#define DEFAULT_SOCKET_ADDRESS "127.0.0.1"
using namespace WPEFramework;

class WebSocketConnectionManager
{
public:
    ~WebSocketConnectionManager() {
        if (mChannel) {
            delete mChannel;
            mChannel = nullptr;
        }
    }
    class Config : public Core::JSON::Container
    {
    public:
        Config(const Config &) = delete;
        Config &operator=(const Config &) = delete;
        Config(Config &&) = delete;
        Config &operator=(Config &&) = delete;

        Config(const std::string socketAddress = DEFAULT_SOCKET_ADDRESS)
            : Core::JSON::Container(), Connector(socketAddress)
        {
            Add(_T("connector"), &Connector);
        }
        ~Config() override = default;

    public:
        Core::JSON::String Connector;
    };

    // Forward declarations
    public:
    class WebSocketChannel;

    // WebSocket JSON Object Factory
    class JSONObjectFactory : public Core::FactoryType<Core::JSON::IElement, char *>
    {
    public:
        JSONObjectFactory():Core::FactoryType<Core::JSON::IElement, char *>(), _jsonRPCFactory(5) {}
        JSONObjectFactory(const JSONObjectFactory &) = delete;
        JSONObjectFactory &operator=(const JSONObjectFactory &) = delete;
        ~JSONObjectFactory() = default;

    public:
        static JSONObjectFactory &Instance(){
            static JSONObjectFactory _singleton;
            return (_singleton);
        }
        Core::ProxyType<Core::JSON::IElement> Element(const string &identifier VARIABLE_IS_NOT_USED) {
            Core::ProxyType<Web::JSONBodyType<Core::JSONRPC::Message>> message = _jsonRPCFactory.Element();
            return Core::ProxyType<Core::JSON::IElement>(message);
        }

    private:
        Core::ProxyPoolType<Web::JSONBodyType<Core::JSONRPC::Message>> _jsonRPCFactory;
    };

    // WebSocket Server implementation
    public:
    class WebSocketServer
        : public Core::StreamJSONOneShotType<Web::WebSocket::HWebSocketServerType<Core::SocketStream>, JSONObjectFactory &, Core::JSON::IElement>
    {
    public:
        WebSocketServer() = delete;
        WebSocketServer &operator=(const WebSocketServer &) = delete;
        typedef Core::StreamJSONOneShotType<Web::WebSocket::HWebSocketServerType<Core::SocketStream>, JSONObjectFactory &, Core::JSON::IElement> BaseClass;                

    public:
        WebSocketServer(const SOCKET &connector, const Core::NodeId &remoteNode, Core::SocketServerType<WebSocketServer> *parent) :
         WebSocketServer::BaseClass(
                  5,
                  WebSocketConnectionManager::JSONObjectFactory::Instance(),
                  false, false, false,
                  connector,
                  remoteNode.AnyInterface(),
                  8096, 8096),
        _id(0),
        _parent(static_cast<WebSocketConnectionManager::WebSocketChannel &>(*parent)),
        _queue(10){
            LOGINFO("Connector value: %d", static_cast<int>(connector));
            LOGINFO("Remote host: %s", remoteNode.HostAddress().c_str());
        }
        ~WebSocketServer() {
            _qLock.Lock();
            _queue.Clear();
            _qLock.Unlock();
            LOGINFO("WebSocketServer destructed for connectionId: %d", _id);

        }

    public:
        void Received(Core::ProxyType<Core::JSON::IElement> &jsonObject){
            
            uint32_t connectionId = _id;

            if (jsonObject.IsValid() == false)
            {
                LOGERR("WebSocketServer: Invalid JSON object received");
            }
            else
            {
                Core::ProxyType<Core::JSONRPC::Message> message = Core::ProxyType<Core::JSONRPC::Message>(jsonObject);
                // Call internal ProcessMessage
                WebSocketConnectionManager::WebSocketServer::ProcessMessage(message, connectionId);
            }
        }
        void Send(Core::ProxyType<Core::JSON::IElement> &jsonObject) {
            if (jsonObject.IsValid() == false)
            {
                LOGERR("WebSocketServer: Invalid JSON object to send");
            }
            else
            {
                WebSocketConnectionManager::WebSocketServer::ToMessage(jsonObject);
            }
        };
        void StateChange(){
            if (this->IsOpen())
            {
                LOGINFO("Open - OK");
                const std::string &query = Link().Query();
                if (_parent.Interface()._authHandler != nullptr) {
                    bool authResult = _parent.Interface()._authHandler(_id, query);
                    if (!authResult) {
                        LOGERR("Authentication failed for query: %s", query.c_str());
                        this->Close(0);
                        return;
                    }
                    LOGINFO("Authentication succeeded");
                } else {
                    LOGWARN("No authentication handler set, proceeding without authentication");
                }
            }
            else if(this->IsSuspended())
            {
                LOGINFO("Closed - %s", this->IsSuspended() ? _T("SUSPENDED") : _T("OK"));
                if (_parent.Interface()._disconnectHandler != nullptr) {
                    _parent.Interface()._disconnectHandler(Id());
                }
            }
        }

        bool IsIdle() const {
            return (true);
        }

        void SendJSONRPCResponse(const std::string &result, int requestId, uint32_t connectionId) {
            Core::ProxyType<Core::JSONRPC::Message> response = Core::ProxyType<Core::JSONRPC::Message>::Create();
                    response->JSONRPC = Core::JSONRPC::Message::DefaultVersion;
                    response->Id = requestId;
                    response->Result = result;

                    LOGDBG("[SendJSONRPCResponse] Sending response for requestId=%d, connectionId=%d", requestId, connectionId);
                    LOGDBG("[SendJSONRPCResponse] Response: %s", result.c_str());

                    // Send the response back to the WebSocket client
                    this->Submit(Core::ProxyType<Core::JSON::IElement>(response));
        }

    private:
        friend class Core::SocketServerType<WebSocketServer>;

        uint32_t Id() {
            return (_id);
        }

        void Id(const uint32_t id){
            LOGINFO("Assigning connectionId: %d", id);
            _id = id;

            // Process any pending messages for this connection
            _qLock.Lock();
            while (_queue.Count() > 0) {
                auto message = _queue[0];
                if (message.IsValid()) {
                    LOGDBG("Processing pending message for connectionId: %d", _id);
                    WebSocketConnectionManager::WebSocketServer::ProcessMessage(message, _id);
                }
                _queue.Remove(0, message);
            }
            _qLock.Unlock();
        }

        void ToMessage(const Core::ProxyType<Core::JSON::IElement> &jsonObject) {
            string jsonMessage;
            jsonObject->ToString(jsonMessage);
            LOGDBG("WebSocket Bytes: %d", static_cast<uint32_t>(jsonMessage.size()));
            LOGDBG("WebSocket Sent: %s", jsonMessage.c_str());
        }
        void ProcessMessage(Core::ProxyType<Core::JSONRPC::Message> &message, uint32_t connectionId) {
            // Check for message->Id.IsSet()
                    if(!message->Id.IsSet()) {
                        // Log an Error for this usecase
                        LOGERR("Message MUST contain an id field");
                        return;
                    }

                    int requestId = message->Id.Value();

                    if (_id==0) {
                        LOGDBG("Connection ID Not set adding request to Pending queue");
                        AddToPending(message);
                        return;
                    }

                    // Extract method name from designator
                    if(!message->Designator.IsSet()) {
                        SendJSONRPCResponse(R"({"error": "Message MUST contain a method field"})", requestId, connectionId);
                        return;
                    }

                    std::string methodName = message->Designator.Value();
                    
                    LOGDBG("[ProcessMessage] Method: %s, RequestId: %d, ConnectionId: %d",
                            methodName.c_str(), requestId, connectionId);

                    // SYNCHRONOUS PROCESSING
                    try
                    {
                        auto& manager = _parent.Interface();
                        if (manager._messageHandler) {
                            std::string params = "{}"; // Default empty params
                            if (message->Parameters.IsSet() && !message->Parameters.Value().empty())
                            {
                               params = message->Parameters.Value();
                            }
                            manager._messageHandler(methodName, params, requestId, _id);
                        } else {
                            SendJSONRPCResponse(R"({"error": "Message handler not set"})", requestId, connectionId);
                        }
                    }
                    catch (const std::exception &e)
                    {
                        LOGERR("[ProcessMessage] Exception during synchronous processing: %s", e.what());
                        std::string errorResponse = R"({"error": "Processing exception", "details": ")" + std::string(e.what()) + R"("})";
                        WebSocketConnectionManager::WebSocketServer::SendJSONRPCResponse(errorResponse, requestId, connectionId);
                    }
                    catch (...)
                    {
                        LOGERR("[ProcessMessage] Unknown exception during synchronous processing");
                        WebSocketConnectionManager::WebSocketServer::SendJSONRPCResponse(R"({"error": "Unknown processing exception"})", requestId, connectionId);
                    }
        }

        void FromMessage(Core::JSON::IElement *jsonObject, const Core::ProxyType<Core::JSONRPC::Message> &message){
            jsonObject->FromString(message->Parameters.Value());
        }

        // New Method add message to the _queue
        void AddToPending(Core::ProxyType<Core::JSONRPC::Message>& element)
        {
            _qLock.Lock();
            if (_queue.Count() == 10 ) {
                LOGERR("Queue full for %d processing error for first entry", _id);
                // Remove the first entry
                auto firstElement = _queue[0];
                _queue.Remove(0, firstElement);
                std::string errorResponse = R"({"error": "Message MUST contain a method field"})";
                WebSocketConnectionManager::WebSocketServer::SendJSONRPCResponse(errorResponse, firstElement->Id.Value(), _id);
            }
            //
            _queue.Add(element);
            LOGINFO("Message queued for connectionId: %d, queue size: %d", _id, static_cast<int>(_queue.Count()));

             _qLock.Unlock();
        }

    private:
        uint32_t _id;
        WebSocketChannel &_parent;
        Core::CriticalSection _qLock;
        Core::ProxyList<Core::JSONRPC::Message> _queue;
    };

    // WebSocket Channel management
    class WebSocketChannel : public Core::SocketServerType<WebSocketServer>
    {
    public:
        WebSocketChannel() = delete;
        WebSocketChannel(const WebSocketChannel &copy) = delete;
        WebSocketChannel &operator=(const WebSocketChannel &) = delete;

    public:
        WebSocketChannel(const WPEFramework::Core::NodeId &remoteNode, WebSocketConnectionManager &parent):
            Core::SocketServerType<WebSocketServer>(remoteNode),
            _parent(parent) {
            Core::SocketServerType<WebSocketConnectionManager::WebSocketServer>::Open(Core::infinite);
        }
        ~WebSocketChannel() {
            Core::SocketServerType<WebSocketServer>::Close(1000);
        }
        WebSocketConnectionManager &Interface() {
            return _parent;
        }

    private:
        WebSocketConnectionManager &_parent;
    };

public:
        // Message handler callback type
    using MessageHandler = std::function<void(const std::string& method, const std::string& params, const uint32_t requestId, const uint32_t connectionId)>;
    using AuthHandler = std::function<bool(const uint32_t connectionId, const std::string& token)>;
    using DisconnectHandler = std::function<void(const uint32_t connectionId)>;
    // New method to add a message Handler into ProcessMessage method
    void SetMessageHandler(const MessageHandler& handler) { _messageHandler = handler; }

    void SetAuthHandler(const AuthHandler& handler) { _authHandler = handler; }

    void SetDisconnectHandler(DisconnectHandler handler) { _disconnectHandler = handler; }

    // Create a new method which can send message to a given connection id using the connection registry
    // Use the SendJSONRPCResponse in Websocket Server to send the message
    bool SendMessageToConnection(const uint32_t connectionId, const std::string &result, const int requestId)
    {
        Core::ProxyType<Core::JSONRPC::Message> response = Core::ProxyType<Core::JSONRPC::Message>::Create();
        response->JSONRPC = Core::JSONRPC::Message::DefaultVersion;
        response->Id = requestId;

        Core::JSONRPC::Message::Info info;
        if (info.FromString(result) && info.Code.IsSet() && info.Text.IsSet()) {
            response->Error = info;
        } else {
            response->Result = result;
        }



        LOGINFO("[SendJSONRPCResponse] Sending response for requestId=%d, connectionId=%d response=%s", requestId, connectionId, result.c_str());

        // Send the response back to the WebSocket client
        mChannel->Submit(connectionId, Core::ProxyType<Core::JSON::IElement>(response));
        
        return true;
    }

    bool DispatchNotificationToConnection(const uint32_t connectionId, const std::string &designator, const std::string &payload)
    {
        Core::ProxyType<Core::JSONRPC::Message> event = Core::ProxyType<Core::JSONRPC::Message>::Create();
        event->JSONRPC = Core::JSONRPC::Message::DefaultVersion;
        event->Designator = designator;
        event->Parameters = payload;
        LOGINFO("Emit Event for method=%s, connectionId=%d params=%s", designator.c_str(), connectionId, payload.c_str());
        mChannel->Submit(connectionId, Core::ProxyType<Core::JSON::IElement>(event));
        return true;
    }

    bool SendRequestToConnection(const uint32_t connectionId, const std::string &designator, const uint32_t requestId, const std::string &params)
    {
        Core::ProxyType<Core::JSONRPC::Message> request = Core::ProxyType<Core::JSONRPC::Message>::Create();
        request->JSONRPC = Core::JSONRPC::Message::DefaultVersion;
        request->Id = requestId;
        request->Designator = designator;
        request->Parameters = params;

        LOGINFO("Send Request for method=%s, connectionId=%d params=%s", designator.c_str(), connectionId, params.c_str());
        mChannel->Submit(connectionId, Core::ProxyType<Core::JSON::IElement>(request));
        return true;
    }

    // New Method to start websocket channel using NodeId
    bool Start(const Core::NodeId &remoteNode)
    {
        try
        {
            mChannel = new WebSocketChannel(remoteNode, *this);
            if (mChannel == nullptr)
            {
                LOGERR("Failed to create WebSocket channel");
                return false;
            }

            LOGINFO("WebSocket channel started successfully on %s %d", remoteNode.HostAddress().c_str(), remoteNode.PortNumber());
            return true;
        }
        catch (const std::exception &e)
        {
            LOGERR("Exception while starting WebSocket channel: %s", e.what());
            return false;
        }
        catch (...)
        {
            LOGERR("Unknown exception while starting WebSocket channel");
            return false;
        }
    }

    // Close connection for a given connection id
    void Close(const uint32_t connectionId) {
        const auto& client = mChannel->Client(connectionId);
        client->Close(0);
    }

private:
    
    MessageHandler _messageHandler;
    AuthHandler _authHandler;
    DisconnectHandler _disconnectHandler;
    WebSocketChannel *mChannel = nullptr;
};

   