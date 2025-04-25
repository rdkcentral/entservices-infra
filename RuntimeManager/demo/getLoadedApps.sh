#!/bin/sh
#This script call appmanager to get the list of loaded application along with lifecycle state

curl -H "Authorization: Bearer `WPEFrameworkSecurityUtility | cut -d '"' -f 4`" --header "Content-Type: application/json" --request POST --data '{"jsonrpc":"2.0","id":"3","method": "org.rdk.AppManager.1.getLoadedApps", "params":{}}' http://127.0.0.1:9998/jsonrpc
