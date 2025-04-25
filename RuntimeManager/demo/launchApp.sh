#!/bin/sh
#This script calls appmanager to launch an application. This further interacts with lifecycle manager, runtime manager to launch app

#Below hack is needed to avoid getting permission issue when running apps inside container
chmod 777 /dev/aml_*
chmod 777 /dev/shm/AudioServiceShmem
chmod 777 /dev/ion

curl -H "Authorization: Bearer `WPEFrameworkSecurityUtility | cut -d '"' -f 4`" --header "Content-Type: application/json" --request POST --data '{"jsonrpc":"2.0","id":"3","method": "org.rdk.AppManager.1.launchApp", "params":{"appId":"YouTube"}}' http://127.0.0.1:9998/jsonrpc
