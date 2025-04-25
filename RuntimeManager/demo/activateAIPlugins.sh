#!/bin/sh
#This script activates all AI Manager plugins before running the application in sequential way

curl -H "Authorization: Bearer `WPEFrameworkSecurityUtility | cut -d '"' -f 4`" --header "Content-Type: application/json" --request POST --data '{"jsonrpc":"2.0","id":"3","method": "Controller.1.activate", "params":{"callsign":"org.rdk.OCIContainer"}}' http://127.0.0.1:9998/jsonrpc
curl -H "Authorization: Bearer `WPEFrameworkSecurityUtility | cut -d '"' -f 4`" --header "Content-Type: application/json" --request POST --data '{"jsonrpc":"2.0","id":"3","method": "Controller.1.activate", "params":{"callsign":"org.rdk.RDKWindowManager"}}' http://127.0.0.1:9998/jsonrpc
curl -H "Authorization: Bearer `WPEFrameworkSecurityUtility | cut -d '"' -f 4`" --header "Content-Type: application/json" --request POST --data '{"jsonrpc":"2.0","id":"3","method": "Controller.1.activate", "params":{"callsign":"org.rdk.RuntimeManager"}}' http://127.0.0.1:9998/jsonrpc
curl -H "Authorization: Bearer `WPEFrameworkSecurityUtility | cut -d '"' -f 4`" --header "Content-Type: application/json" --request POST --data '{"jsonrpc":"2.0","id":"3","method": "Controller.1.activate", "params":{"callsign":"org.rdk.LifecycleManager"}}' http://127.0.0.1:9998/jsonrpc
curl -H "Authorization: Bearer `WPEFrameworkSecurityUtility | cut -d '"' -f 4`" --header "Content-Type: application/json" --request POST --data '{"jsonrpc":"2.0","id":"3","method": "Controller.1.activate", "params":{"callsign":"org.rdk.AppManager"}}' http://127.0.0.1:9998/jsonrpc
