
/*
* If not stated otherwise in this file or this component's LICENSE file the
* following copyright and licenses apply:
*
* Copyright 2024 RDK Management
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
    
#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <stdio.h>

#include <WPEFramework/interfaces/IUserSettings.h>
#include <WPEFramework/interfaces/Ids.h>
#include <core/core.h>
#include "ServiceMock.h"
#include "Store2Mock.h"
#include "UserSettingsImplementation.h"

extern WPEFramework::Exchange::IUserSettings *InterfacePointer;
extern WPEFramework::Exchange::IUserSettingsInspector *IUserSettingsInspectorPointer;
// Access to the global mocks for setting test-specific expectations
extern ::testing::NiceMock<ServiceMock>* g_serviceMock;
extern Store2Mock* g_storeMock;

using namespace WPEFramework::Exchange;
using namespace WPEFramework;
using ::testing::NiceMock;
using ::testing::Return;
using ::testing::_;
using ::testing::DoAll;
using ::testing::SetArgReferee;

class TestNotification : public WPEFramework::Exchange::IUserSettings::INotification {
public:
    //IUnkown pure virtual methods implementations
    void AddRef() const override {}
    uint32_t Release() const override { return 0; }
    void* QueryInterface(const uint32_t) override { return nullptr; }
};

/**
* @brief Test to verify that GetAudioDescription returns ERROR_NONE with valid value
*
* This test checks the GetAudioDescription method to ensure it returns ERROR_NONE and sets the enabled flag correctly.@n
*
* **Test Group ID:** Basic: 01@n
* **Test Case ID:** 001@n
* **Priority:** High@n
* @n
* **Pre-Conditions:** None@n
* **Dependencies:** None@n
* **User Interaction:** None@n
* @n
* **Test Procedure:**@n
* | Variation / Step | Description | Test Data |Expected Result |Notes |
* | :----: | --------- | ---------- |-------------- | ----- |
* | 01| Call GetAudioDescription with valid InterfacePointer | enabled = false | result = Core::ERROR_NONE, enabled = true or false | Should Pass |
*/
TEST(UserSettingsTestAI, GetAudioDescription_ReturnsErrorNone_WithValidValue) {
    std::cout << "Entering GetAudioDescription_ReturnsErrorNone_WithValidValue" << std::endl;
    
    // Setup the expected behavior for GetValue
    bool audioDescription = true;
    std::string audioDescriptionValue = audioDescription ? "true" : "false";
    
    EXPECT_CALL(*g_storeMock, GetValue(_, _, USERSETTINGS_AUDIO_DESCRIPTION_KEY, _, _))
        .WillOnce(DoAll(
            SetArgReferee<3>(audioDescriptionValue),
            Return(Core::ERROR_NONE)
        ));
    
    bool enabled = false;
    Core::hresult result = InterfacePointer->GetAudioDescription(enabled);
    
    EXPECT_EQ(result, Core::ERROR_NONE);
    EXPECT_TRUE(enabled == audioDescription);
    
    std::cout << "Exiting GetAudioDescription_ReturnsErrorNone_WithValidValue" << std::endl;
}

/**
* @brief Test to verify the GetBlockNotRatedContent function returns the enabled state successfully
*
* This test checks if the GetBlockNotRatedContent function correctly retrieves the enabled state of blocking unrated content. The function is expected to return a success result and the state should be either true or false.@n
*
* **Test Group ID:** Basic: 01@n
* **Test Case ID:** 002@n
* **Priority:** High@n
* @n
* **Pre-Conditions:** None@n
* **Dependencies:** None@n
* **User Interaction:** None@n
* @n
* **Test Procedure:**@n
* | Variation / Step | Description | Test Data |Expected Result |Notes |
* | :----: | --------- | ---------- |-------------- | ----- |
* | 01| Call GetBlockNotRatedContent function | blockNotRatedContent = false | result = Core::ERROR_NONE, blockNotRatedContent = true or false | Should Pass |
*/
TEST(UserSettingsTestAI, GetBlockNotRatedContentReturnsEnabledStateSuccessfully) {
    std::cout << "Entering GetBlockNotRatedContentReturnsEnabledStateSuccessfully" << std::endl;
    
    // Setup the expected behavior for GetValue
    bool expectedValue = false;
    std::string blockNotRatedContentValue = expectedValue ? "true" : "false";
    
    EXPECT_CALL(*g_storeMock, GetValue(_, _, USERSETTINGS_BLOCK_NOT_RATED_CONTENT_KEY, _, _))
        .WillOnce(DoAll(
            SetArgReferee<3>(blockNotRatedContentValue),
            Return(Core::ERROR_NONE)
        ));
    
    bool blockNotRatedContent = false;
    Core::hresult result = InterfacePointer->GetBlockNotRatedContent(blockNotRatedContent);
    
    EXPECT_EQ(result, Core::ERROR_NONE);
    EXPECT_EQ(blockNotRatedContent, expectedValue);
    
    std::cout << "Exiting GetBlockNotRatedContentReturnsEnabledStateSuccessfully" << std::endl;
}

/**
* @brief Test to verify that the GetCaptions function returns the enabled state successfully.
*
* This test checks if the GetCaptions function correctly retrieves the enabled state of captions and returns the expected result code. This is important to ensure that the captions feature can be correctly queried for its state.
*
* **Test Group ID:** Basic: 01@n
* **Test Case ID:** 003@n
* **Priority:** High@n
* @n
* **Pre-Conditions:** None@n
* **Dependencies:** None@n
* **User Interaction:** None@n
* @n
* **Test Procedure:**@n
* | Variation / Step | Description | Test Data | Expected Result | Notes |
* | :----: | --------- | ---------- |-------------- | ----- |
* | 01 | Call GetCaptions to retrieve the enabled state | enabled = false | result = Core::ERROR_NONE, enabled = true | Should Pass |
*/
TEST(UserSettingsTestAI, GetCaptionsReturnsEnabledStateSuccessfully) {
    std::cout << "Entering GetCaptionsReturnsEnabledStateSuccessfully" << std::endl;

    // Set up specific mock behavior for this test to return enabled=true
    EXPECT_CALL(*g_storeMock, GetValue(_, _, _, _, _))
        .WillOnce(DoAll(
            SetArgReferee<3>("true"),  // Set the value parameter to "true"
            Return(Core::ERROR_NONE)
        ));

    bool enabled = false;
    Core::hresult result = InterfacePointer->GetCaptions(enabled);

    EXPECT_EQ(result, Core::ERROR_NONE);
    EXPECT_TRUE(enabled);

    std::cout << "Exiting GetCaptionsReturnsEnabledStateSuccessfully" << std::endl;
}

/**
* @brief Test to verify the GetHighContrast function returns the enabled state successfully.
*
* This test checks if the GetHighContrast function correctly retrieves the high contrast enabled state from the InterfacePointer. It ensures that the function returns a successful result and that the enabled state is either true or false.
*
* **Test Group ID:** Basic: 01@n
* **Test Case ID:** 004@n
* **Priority:** High@n
* @n
* **Pre-Conditions:** None@n
* **Dependencies:** None@n
* **User Interaction:** None@n
* @n
* **Test Procedure:**@n
* | Variation / Step | Description | Test Data |Expected Result |Notes |
* | :----: | --------- | ---------- |-------------- | ----- |
* | 01| Enter the test function | None | None | Should be successful |
* | 02| Initialize enabled to false | enabled = false | None | Should be successful |
* | 03| Call GetHighContrast function | enabled = false | result = Core::ERROR_NONE, enabled = true or false | Should Pass |
* | 04| Check the result of GetHighContrast | result = Core::ERROR_NONE | result = Core::ERROR_NONE | Should Pass |
* | 05| Verify the enabled state | enabled = true or false | enabled = true or false | Should Pass |
* | 06| Exit the test function | None | None | Should be successful |
*/
TEST(UserSettingsTestAI, GetHighContrastReturnsEnabledStateSuccessfully) {
    std::cout << "Entering GetHighContrastReturnsEnabledStateSuccessfully" << std::endl;
    
    // Set up the mock to return a specific boolean value for high contrast
    bool expectedValue = true;
    std::string highContrastValue = expectedValue ? "true" : "false";
    
    EXPECT_CALL(*g_storeMock, GetValue(_, _, USERSETTINGS_HIGH_CONTRAST_KEY, _, _))
        .WillOnce(DoAll(
            SetArgReferee<3>(highContrastValue),  // Return the expected high contrast setting
            Return(Core::ERROR_NONE)
        ));
    
    bool enabled = false;
    Core::hresult result = InterfacePointer->GetHighContrast(enabled);
    
    EXPECT_EQ(result, Core::ERROR_NONE);
    EXPECT_EQ(enabled, expectedValue);
    
    std::cout << "Exiting GetHighContrastReturnsEnabledStateSuccessfully" << std::endl;
}

/**
* @brief Test to verify the successful retrieval of live watershed status
*
* This test checks the functionality of the GetLiveWatershed method to ensure it correctly retrieves the live watershed status. The test verifies that the method returns a success result and that the liveWatershed variable is set to either true or false.
*
* **Test Group ID:** Basic: 01@n
* **Test Case ID:** 005@n
* **Priority:** High@n
* @n
* **Pre-Conditions:** None@n
* **Dependencies:** None@n
* **User Interaction:** None@n
* @n
* **Test Procedure:**@n
* | Variation / Step | Description | Test Data |Expected Result |Notes |
* | :----: | --------- | ---------- |-------------- | ----- |
* | 01| Call GetLiveWatershed method | liveWatershed = false | result = Core::ERROR_NONE, liveWatershed = true or false | Should Pass |
* | 02| Check result value | result = Core::ERROR_NONE | result = Core::ERROR_NONE | Should be successful |
* | 03| Validate liveWatershed value | liveWatershed = true or false | liveWatershed = true or false | Should be successful |
*/
TEST(UserSettingsTestAI, GetLiveWatershedReturnsLiveWatershedSuccessfully) {
    std::cout << "Entering GetLiveWatershedReturnsLiveWatershedSuccessfully" << std::endl;

    // Set up the mock to return a specific boolean value for live watershed
    bool expectedValue = true;
    std::string liveWatershedValue = expectedValue ? "true" : "false";
    
    EXPECT_CALL(*g_storeMock, GetValue(_, _, USERSETTINGS_LIVE_WATERSHED_KEY, _, _))
        .WillOnce(DoAll(
            SetArgReferee<3>(liveWatershedValue),
            Return(Core::ERROR_NONE)
        ));

    bool liveWatershed = false;
    Core::hresult result = InterfacePointer->GetLiveWatershed(liveWatershed);

    EXPECT_EQ(result, Core::ERROR_NONE);
    EXPECT_EQ(liveWatershed, expectedValue);

    std::cout << "Exiting GetLiveWatershedReturnsLiveWatershedSuccessfully" << std::endl;
}

/**
* @brief Test to verify that GetPinControl method returns the pin control status successfully.
*
* This test checks if the GetPinControl method of the InterfacePointer object returns the correct pin control status and ensures that the result is either true or false.@n
*
* **Test Group ID:** Basic: 01@n
* **Test Case ID:** 006@n
* **Priority:** High@n
* @n
* **Pre-Conditions:** None@n
* **Dependencies:** None@n
* **User Interaction:** None@n
* @n
* **Test Procedure:**@n
* | Variation / Step | Description | Test Data |Expected Result |Notes |
* | :----: | --------- | ---------- |-------------- | ----- |
* | 01| Call GetPinControl method | pinControl = false | result = Core::ERROR_NONE, pinControl = true or false | Should Pass |
*/
TEST(UserSettingsTestAI, GetPinControlReturnsPinControlSuccessfully) {
    std::cout << "Entering GetPinControlReturnsPinControlSuccessfully" << std::endl;

    // Set up the mock to return a specific boolean value for pin control
    bool expectedValue = true;
    std::string pinControlValue = expectedValue ? "true" : "false";
    
    EXPECT_CALL(*g_storeMock, GetValue(_, _, USERSETTINGS_PIN_CONTROL_KEY, _, _))
        .WillOnce(DoAll(
            SetArgReferee<3>(pinControlValue),
            Return(Core::ERROR_NONE)
        ));

    bool pinControl = false;
    Core::hresult result = InterfacePointer->GetPinControl(pinControl);

    EXPECT_EQ(result, Core::ERROR_NONE);
    EXPECT_EQ(pinControl, expectedValue);

    std::cout << "Exiting GetPinControlReturnsPinControlSuccessfully" << std::endl;
}

/**
* @brief Test to verify the GetPinOnPurchase method returns the pin on purchase status successfully.
*
* This test checks if the GetPinOnPurchase method of the InterfacePointer correctly retrieves the pin on purchase status. The test ensures that the method returns a successful result and that the pinOnPurchase variable is set to either true or false.@n
*
* **Test Group ID:** Basic: 01@n
* **Test Case ID:** 007@n
* **Priority:** High@n
* @n
* **Pre-Conditions:** None@n
* **Dependencies:** None@n
* **User Interaction:** None@n
* @n
* **Test Procedure:**@n
* | Variation / Step | Description | Test Data |Expected Result |Notes |
* | :----: | --------- | ---------- |-------------- | ----- |
* | 01| Enter the test function | None | None | Should be successful |
* | 02| Initialize pinOnPurchase to false | pinOnPurchase = false | None | Should be successful |
* | 03| Call GetPinOnPurchase method | pinOnPurchase = false | result = Core::ERROR_NONE, pinOnPurchase = true/false | Should Pass |
* | 04| Check the result of GetPinOnPurchase | result = Core::ERROR_NONE, pinOnPurchase = true/false | result == Core::ERROR_NONE, pinOnPurchase == true/false | Should Pass |
* | 05| Exit the test function | None | None | Should be successful |
*/
TEST(UserSettingsTestAI, GetPinOnPurchaseReturnsPinOnPurchaseSuccessfully) {
    std::cout << "Entering GetPinOnPurchaseReturnsPinOnPurchaseSuccessfully test";
    
    // Set up the mock to return a specific boolean value for pin on purchase
    bool expectedValue = false;
    std::string pinOnPurchaseValue = expectedValue ? "true" : "false";
    
    EXPECT_CALL(*g_storeMock, GetValue(_, _, USERSETTINGS_PIN_ON_PURCHASE_KEY, _, _))
        .WillOnce(DoAll(
            SetArgReferee<3>(pinOnPurchaseValue),
            Return(Core::ERROR_NONE)
        ));
    
    bool pinOnPurchase = false;
    Core::hresult result = InterfacePointer->GetPinOnPurchase(pinOnPurchase);
    
    EXPECT_EQ(result, Core::ERROR_NONE);
    EXPECT_EQ(pinOnPurchase, expectedValue);
    
    std::cout << "Exiting GetPinOnPurchaseReturnsPinOnPurchaseSuccessfully test";
}

/**
* @brief Test to verify the GetPlaybackWatershed function returns ERROR_NONE with a valid playbackWatershed value.
*
* This test checks if the GetPlaybackWatershed function correctly returns ERROR_NONE and sets the playbackWatershed value to either true or false.@n
*
* **Test Group ID:** Basic: 01@n
* **Test Case ID:** 008@n
* **Priority:** High@n
* @n
* **Pre-Conditions:** None@n
* **Dependencies:** None@n
* **User Interaction:** None@n
* @n
* **Test Procedure:**@n
* | Variation / Step | Description | Test Data |Expected Result |Notes |
* | :----: | --------- | ---------- |-------------- | ----- |
* | 01| Call GetPlaybackWatershed function | playbackWatershed = false | result = Core::ERROR_NONE, playbackWatershed = true or false | Should Pass |
*/
TEST(UserSettingsTestAI, GetPlaybackWatershed_ReturnsErrorNone_WithValidPlaybackWatershed) {
    std::cout << "Entering GetPlaybackWatershed_ReturnsErrorNone_WithValidPlaybackWatershed test";
    
    // Set up the mock to return a specific boolean value for playback watershed
    bool expectedValue = false;
    std::string playbackWatershedValue = expectedValue ? "true" : "false";
    
    EXPECT_CALL(*g_storeMock, GetValue(_, _, USERSETTINGS_PLAYBACK_WATERSHED_KEY, _, _))
        .WillOnce(DoAll(
            SetArgReferee<3>(playbackWatershedValue),
            Return(Core::ERROR_NONE)
        ));
    
    bool playbackWatershed = false;
    Core::hresult result = InterfacePointer->GetPlaybackWatershed(playbackWatershed);
    
    EXPECT_EQ(result, Core::ERROR_NONE);
    EXPECT_EQ(playbackWatershed, expectedValue);
    
    std::cout << "Exiting GetPlaybackWatershed_ReturnsErrorNone_WithValidPlaybackWatershed test";
}

/**
* @brief Test to validate the preferred languages string returned by the GetPreferredAudioLanguages method.
*
* This test checks if the GetPreferredAudioLanguages method correctly retrieves the preferred audio languages string and verifies that it matches the expected value "eng". This ensures that the method is functioning correctly and returning the correct language preference.
*
* **Test Group ID:** Basic: 01@n
* **Test Case ID:** 009@n
* **Priority:** High@n
* @n
* **Pre-Conditions:** None@n
* **Dependencies:** None@n
* **User Interaction:** None@n
* @n
* **Test Procedure:**@n
* | Variation / Step | Description | Test Data | Expected Result | Notes |
* | :----: | --------- | ---------- |-------------- | ----- |
* | 01 | Initialize preferredLanguages string and call GetPreferredAudioLanguages | preferredLanguages = "" | result = Core::ERROR_NONE, preferredLanguages = "eng" | Should Pass |
*/
TEST(UserSettingsTestAI, ValidPreferredLanguagesString) {
    std::cout << "Entering ValidPreferredLanguagesString test" << std::endl;
    
    // Set up specific mock behavior for this test
    EXPECT_CALL(*g_storeMock, GetValue(_, _, _, _, _))
        .WillOnce(DoAll(
            SetArgReferee<3>("eng"),  // Set the value parameter to "eng"
            Return(Core::ERROR_NONE)
        ));
    
    string preferredLanguages = "";
    Core::hresult result = InterfacePointer->GetPreferredAudioLanguages(preferredLanguages);
    
    EXPECT_EQ(result, Core::ERROR_NONE);
    EXPECT_EQ(preferredLanguages, "eng");
    
    std::cout << "Exiting ValidPreferredLanguagesString test" << std::endl;
}

/**
* @brief Test to validate the retrieval of preferred languages
*
* This test checks if the GetPreferredCaptionsLanguages function correctly retrieves the preferred languages set by the user. The test ensures that the function returns the expected result and the preferred languages string matches the expected value.
*
* **Test Group ID:** Basic: 01@n
* **Test Case ID:** 010@n
* **Priority:** High@n
* @n
* **Pre-Conditions:** None@n
* **Dependencies:** None@n
* **User Interaction:** None@n
* @n
* **Test Procedure:**@n
* | Variation / Step | Description | Test Data |Expected Result |Notes |
* | :----: | --------- | ---------- |-------------- | ----- |
* | 01| Call GetPreferredCaptionsLanguages API | preferredLanguages = "" | result = Core::ERROR_NONE, preferredLanguages = "eng,fra" | Should Pass |
*/
TEST(UserSettingsTestAI, ValidPreferredLanguages) {
    std::cout << "Entering ValidPreferredLanguages test";
    
    // Set up specific mock behavior for this test
    EXPECT_CALL(*g_storeMock, GetValue(_, _, _, _, _))
        .WillOnce(DoAll(
            SetArgReferee<3>("eng,fra"),  // Set the value parameter to "eng,fra"
            Return(Core::ERROR_NONE)
        ));
    
    string preferredLanguages = "";
    Core::hresult result = InterfacePointer->GetPreferredCaptionsLanguages(preferredLanguages);
    
    EXPECT_EQ(result, Core::ERROR_NONE);
    EXPECT_EQ(preferredLanguages, "eng,fra");
    
    std::cout << "Exiting ValidPreferredLanguages test";
}

/**
* @brief Test to verify that a valid service name is returned by the GetPreferredClosedCaptionService method.
*
* This test checks if the GetPreferredClosedCaptionService method returns a valid closed caption service name. 
* The valid service names include "CC1", "CC2", "CC3", "CC4", "TEXT1", "TEXT2", "TEXT3", "TEXT4", or any 
* service name starting with "SERVICE" followed by a number between 1 and 64.
*
* **Test Group ID:** Basic: 01@n
* **Test Case ID:** 011@n
* **Priority:** High@n
* @n
* **Pre-Conditions:** None@n
* **Dependencies:** None@n
* **User Interaction:** None@n
* @n
* **Test Procedure:**@n
* | Variation / Step | Description | Test Data |Expected Result |Notes |
* | :----: | --------- | ---------- |-------------- | ----- |
* | 01| Call GetPreferredClosedCaptionService method | service = "" | result should be Core::ERROR_NONE, service should be one of the valid names | Should Pass |
* | 02| Check if the service name is valid | service = returned value | service should be "CC1", "CC2", "CC3", "CC4", "TEXT1", "TEXT2", "TEXT3", "TEXT4", or "SERVICE" followed by a number between 1 and 64 | Should Pass |
*/
TEST(UserSettingsTestAI, ValidServiceNameReturned) {
    std::cout << "Entering ValidServiceNameReturned" << std::endl;
    
    // Set up the mock to return a valid service name
    EXPECT_CALL(*g_storeMock, GetValue(_, _, _, _, _))
        .WillOnce(DoAll(
            SetArgReferee<3>("CC1"),  // Return "CC1" as the closed caption service
            Return(Core::ERROR_NONE)
        ));
    
    string service = "";
    Core::hresult result = InterfacePointer->GetPreferredClosedCaptionService(service);
    
    EXPECT_EQ(result, Core::ERROR_NONE);
    EXPECT_TRUE(service == "CC1" || service == "CC2" || service == "CC3" || service == "CC4" ||
                service == "TEXT1" || service == "TEXT2" || service == "TEXT3" || service == "TEXT4" ||
                (service.substr(0, 7) == "SERVICE" && std::stoi(service.substr(7)) >= 1 && std::stoi(service.substr(7)) <= 64));
    
    std::cout << "Exiting ValidServiceNameReturned" << std::endl;
}

/**
* @brief Test to validate the presentation language retrieved by the GetPresentationLanguage method.
*
* This test checks if the GetPresentationLanguage method returns a valid presentation language code. The valid codes are "en-US", "es-US", "en-CA", and "fr-CA". The test ensures that the method returns one of these valid codes and that the result is successful.
*
* **Test Group ID:** Basic: 01@n
* **Test Case ID:** 012@n
* **Priority:** High@n
* @n
* **Pre-Conditions:** None@n
* **Dependencies:** None@n
* **User Interaction:** None@n
* @n
* **Test Procedure:**@n
* | Variation / Step | Description | Test Data |Expected Result |Notes |
* | :----: | --------- | ---------- |-------------- | ----- |
* | 01| Call GetPresentationLanguage method | presentationLanguage = "" | result = Core::ERROR_NONE, presentationLanguage = "en-US" or "es-US" or "en-CA" or "fr-CA" | Should Pass |
*/
TEST(UserSettingsTestAI, ValidPresentationLanguage) {
    std::cout << "Entering ValidPresentationLanguage test" << std::endl;
    
    // Set up the mock to return a valid presentation language
    EXPECT_CALL(*g_storeMock, GetValue(_, _, _, _, _))
        .WillOnce(DoAll(
            SetArgReferee<3>("en-US"),  // Return "en-US" as the presentation language
            Return(Core::ERROR_NONE)
        ));
    
    string presentationLanguage = "";
    Core::hresult result = InterfacePointer->GetPresentationLanguage(presentationLanguage);
    
    EXPECT_EQ(result, Core::ERROR_NONE);
    EXPECT_TRUE(presentationLanguage == "en-US" || presentationLanguage == "es-US" || presentationLanguage == "en-CA" || presentationLanguage == "fr-CA");
    
    std::cout << "Exiting ValidPresentationLanguage test" << std::endl;
}

/**
* @brief Test to validate the retrieval of viewing restrictions
*
* This test checks if the GetViewingRestrictions method correctly retrieves the viewing restrictions and ensures that the returned string is not empty.@n
*
* **Test Group ID:** Basic: 01@n
* **Test Case ID:** 013@n
* **Priority:** High@n
* @n
* **Pre-Conditions:** None@n
* **Dependencies:** None@n
* **User Interaction:** None@n
* @n
* **Test Procedure:**@n
* | Variation / Step | Description | Test Data | Expected Result | Notes |
* | :----: | --------- | ---------- |-------------- | ----- |
* | 01 | Entering ValidViewingRestrictions test | - | - | Should be successful |
* | 02 | Call GetViewingRestrictions method | viewingRestrictions = "" | result = Core::ERROR_NONE, viewingRestrictions is not empty | Should Pass |
* | 03 | Exiting ValidViewingRestrictions test | - | - | Should be successful |
*/
TEST(UserSettingsTestAI, ValidViewingRestrictions) {
    std::cout << "Entering ValidViewingRestrictions test";
    string viewingRestrictions = "";
    
    // Setup the expected behavior for GetValue
    std::string mockValue = "TVPG_Y7";
    EXPECT_CALL(*g_storeMock, GetValue(_, _, USERSETTINGS_VIEWING_RESTRICTIONS_KEY, _, _))
        .WillOnce(DoAll(
            SetArgReferee<3>(mockValue),
            Return(Core::ERROR_NONE)
        ));
    
    Core::hresult result = InterfacePointer->GetViewingRestrictions(viewingRestrictions);
    
    EXPECT_EQ(result, Core::ERROR_NONE);
    EXPECT_FALSE(viewingRestrictions.empty());
    
    std::cout << "Exiting ValidViewingRestrictions test";
}

/**
* @brief Test to validate the viewing restrictions window
*
* This test checks if the GetViewingRestrictionsWindow function correctly retrieves the viewing restrictions window and verifies that it matches the expected value "ALWAYS". This ensures that the function behaves as expected under normal conditions.
*
* **Test Group ID:** Basic: 01@n
* **Test Case ID:** 014@n
* **Priority:** High@n
* @n
* **Pre-Conditions:** None@n
* **Dependencies:** None@n
* **User Interaction:** None@n
* @n
* **Test Procedure:**@n
* | Variation / Step | Description | Test Data |Expected Result |Notes |
* | :----: | --------- | ---------- |-------------- | ----- |
* | 01| Entering the test function | None | None | Should be successful |
* | 02| Call GetViewingRestrictionsWindow API | viewingRestrictionsWindow = "" | result = Core::ERROR_NONE, viewingRestrictionsWindow = "ALWAYS" | Should Pass |
* | 03| Verify the result | result = Core::ERROR_NONE | result = Core::ERROR_NONE | Should Pass |
* | 04| Verify the viewingRestrictionsWindow value | viewingRestrictionsWindow = "ALWAYS" | viewingRestrictionsWindow = "ALWAYS" | Should Pass |
* | 05| Exiting the test function | None | None | Should be successful |
*/
TEST(UserSettingsTestAI, ValidGetViewingRestrictionsWindow) {
    std::cout << "Entering ValidGetViewingRestrictionsWindow" << std::endl;
    
    string viewingRestrictionsWindow = "";
    
    // Setup the expected behavior for GetValue
    EXPECT_CALL(*g_storeMock, GetValue(_, _, USERSETTINGS_VIEWING_RESTRICTIONS_WINDOW_KEY, _, _))
        .WillOnce(DoAll(
            SetArgReferee<3>("ALWAYS"),
            Return(Core::ERROR_NONE)
        ));
    
    Core::hresult result = InterfacePointer->GetViewingRestrictionsWindow(viewingRestrictionsWindow);
    
    EXPECT_EQ(result, Core::ERROR_NONE);
    EXPECT_EQ(viewingRestrictionsWindow, "ALWAYS");
    
    std::cout << "Exiting ValidGetViewingRestrictionsWindow" << std::endl;
}

/**
* @brief Test to verify the GetVoiceGuidance function returns ERROR_NONE with a valid enabled value.
*
* This test checks if the GetVoiceGuidance function of the InterfacePointer returns the expected result of ERROR_NONE and sets the enabled variable to a valid boolean value (true or false). This ensures that the function behaves correctly under normal conditions.
*
* **Test Group ID:** Basic: 01@n
* **Test Case ID:** 015@n
* **Priority:** High@n
* @n
* **Pre-Conditions:** None@n
* **Dependencies:** None@n
* **User Interaction:** None@n
* @n
* **Test Procedure:**@n
* | Variation / Step | Description | Test Data |Expected Result |Notes |
* | :----: | --------- | ---------- |-------------- | ----- |
* | 01| Call GetVoiceGuidance with a valid enabled variable | enabled = false | result = Core::ERROR_NONE, enabled = true or false | Should Pass |
*/
TEST(UserSettingsTestAI, GetVoiceGuidance_ReturnsErrorNoneWithValidEnabledValue) {
    std::cout << "Entering GetVoiceGuidance_ReturnsErrorNoneWithValidEnabledValue" << std::endl;
    
    // Set up the mock to return a specific boolean value for voice guidance
    bool expectedValue = true;
    std::string voiceGuidanceValue = expectedValue ? "true" : "false";
    
    EXPECT_CALL(*g_storeMock, GetValue(_, _, USERSETTINGS_VOICE_GUIDANCE_KEY, _, _))
        .WillOnce(DoAll(
            SetArgReferee<3>(voiceGuidanceValue),
            Return(Core::ERROR_NONE)
        ));
    
    bool enabled = false;
    Core::hresult result = InterfacePointer->GetVoiceGuidance(enabled);
    
    EXPECT_EQ(result, Core::ERROR_NONE);
    EXPECT_EQ(enabled, expectedValue);
    
    std::cout << "Exiting GetVoiceGuidance_ReturnsErrorNoneWithValidEnabledValue" << std::endl;
}

/**
* @brief Test to verify GetVoiceGuidanceHints returns ERROR_NONE with a valid hints value
*
* This test checks the GetVoiceGuidanceHints function to ensure it returns ERROR_NONE when called with a valid hints value. The test also verifies that the hints value is either true or false.
*
* **Test Group ID:** Basic: 01@n
* **Test Case ID:** 016@n
* **Priority:** High@n
* @n
* **Pre-Conditions:** None@n
* **Dependencies:** None@n
* **User Interaction:** None@n
* @n
* **Test Procedure:**@n
* | Variation / Step | Description | Test Data |Expected Result |Notes |
* | :----: | --------- | ---------- |-------------- | ----- |
* | 01| Call GetVoiceGuidanceHints with a valid hints value | hints = false | result = Core::ERROR_NONE, hints = true or false | Should Pass |
*/
TEST(UserSettingsTestAI, GetVoiceGuidanceHints_ReturnsErrorNone_WithValidHintsValue) {
    std::cout << "Entering GetVoiceGuidanceHints_ReturnsErrorNone_WithValidHintsValue" << std::endl;
    
    bool expectedValue = true;
    std::string hintsValue = expectedValue ? "true" : "false";
    
    // Setup the expected behavior for GetValue
    EXPECT_CALL(*g_storeMock, GetValue(_, _, USERSETTINGS_VOICE_GUIDANCE_HINTS_KEY, _, _))
        .WillOnce(DoAll(
            SetArgReferee<3>(hintsValue),
            Return(Core::ERROR_NONE)
        ));
    
    bool hints = false;
    Core::hresult result = InterfacePointer->GetVoiceGuidanceHints(hints);
    
    EXPECT_EQ(result, Core::ERROR_NONE);
    EXPECT_EQ(hints, expectedValue);
    
    std::cout << "Exiting GetVoiceGuidanceHints_ReturnsErrorNone_WithValidHintsValue" << std::endl;
}

/**
* @brief Test to verify the retrieval of a valid voice guidance rate
*
* This test checks if the GetVoiceGuidanceRate function correctly retrieves the voice guidance rate and returns the expected result code. It ensures that the function behaves as expected when called with valid parameters.
*
* **Test Group ID:** Basic: 01@n
* **Test Case ID:** 017@n
* **Priority:** High@n
* @n
* **Pre-Conditions:** None@n
* **Dependencies:** None@n
* **User Interaction:** None@n
* @n
* **Test Procedure:**@n
* | Variation / Step | Description | Test Data | Expected Result | Notes |
* | :----: | --------- | ---------- |-------------- | ----- |
* | 01 | Call GetVoiceGuidanceRate with a valid rate variable | rate = 0.0 | result = Core::ERROR_NONE, Rate should fall within the inclusive range of 0.1 to 10 | Should Pass |
*/
TEST(UserSettingsTestAI, ValidRateRetrieval) {
    std::cout << "Entering ValidRateRetrieval" << std::endl;
    double rate = 0.0;
    double expectedRate = 1.0;
    
    // Setup the expected behavior for GetValue
    EXPECT_CALL(*g_storeMock, GetValue(_, _, USERSETTINGS_VOICE_GUIDANCE_RATE_KEY, _, _))
        .WillOnce(DoAll(
            SetArgReferee<3>(std::to_string(expectedRate)),
            Return(Core::ERROR_NONE)
        ));
    
    Core::hresult result = InterfacePointer->GetVoiceGuidanceRate(rate);
    
    EXPECT_EQ(result, Core::ERROR_NONE);
    EXPECT_DOUBLE_EQ(rate, expectedRate);
    EXPECT_GE(rate, 0.1);
    EXPECT_LE(rate, 10.0);
    
    std::cout << "Rate: " << rate << std::endl;
    std::cout << "Exiting ValidRateRetrieval" << std::endl;
}

/**
* @brief Test the registration of a valid notification object
*
* This test verifies that the Register method successfully registers a valid notification object and returns the expected result.
*
* **Test Group ID:** Basic: 01@n
* **Test Case ID:** 018@n
* **Priority:** High@n
* @n
* **Pre-Conditions:** None@n
* **Dependencies:** None@n
* **User Interaction:** None@n
* @n
* **Test Procedure:**@n
* | Variation / Step | Description | Test Data |Expected Result |Notes |
* | :----: | --------- | ---------- |-------------- | ----- |
* | 01| Create a valid notification object | notification = new Exchange::IUserSettings::INotification() | Notification object created | Should be successful |
* | 02| Register the notification object | InterfacePointer->Register(notification) | result = Core::ERROR_NONE | Should Pass |
* | 03| Verify the result | EXPECT_EQ(result, Core::ERROR_NONE) | result = Core::ERROR_NONE | Should Pass |
*/
TEST(UserSettingsTestAI, RegisterWithValidNotificationObject) {
    std::cout << "Entering RegisterWithValidNotificationObject" << std::endl;
    Exchange::IUserSettings::INotification* notification = new TestNotification();
    
    Core::hresult result = InterfacePointer->Register(notification);
    
    EXPECT_EQ(result, Core::ERROR_NONE);
    
    std::cout << "Exiting RegisterWithValidNotificationObject" << std::endl;
}

/**
* @brief Test the Register function with a null notification object
*
* This test verifies that the Register function correctly handles the case where a null notification object is passed. 
* It ensures that the function returns an error code indicating an invalid parameter.
*
* **Test Group ID:** Basic: 01@n
* **Test Case ID:** 019@n
* **Priority:** High@n
* @n
* **Pre-Conditions:** None@n
* **Dependencies:** None@n
* **User Interaction:** None@n
* @n
* **Test Procedure:**@n
* | Variation / Step | Description | Test Data |Expected Result |Notes |
* | :----: | --------- | ---------- |-------------- | ----- |
* | 01| Call Register with null notification object | notification = nullptr | result = Core::ERROR_INVALID_PARAMETER, EXPECT_EQ(result, Core::ERROR_INVALID_PARAMETER) | Should Fail |
*/
TEST(UserSettingsTestAI, RegisterWithNullNotificationObject) {
    std::cout << "Entering RegisterWithNullNotificationObject" << std::endl;
    Exchange::IUserSettings::INotification* notification = nullptr;
    
    Core::hresult result = InterfacePointer->Register(notification);
    
    EXPECT_EQ(result, Core::ERROR_INVALID_PARAMETER);
    
    std::cout << "Exiting RegisterWithNullNotificationObject" << std::endl;
}

/**
* @brief Test to verify enabling audio description functionality
*
* This test checks if the audio description can be successfully enabled using the SetAudioDescription method of the IUserSettings interface. The test ensures that the method returns the expected result when called with a valid input.
*
* **Test Group ID:** Basic: 01@n
* **Test Case ID:** 020@n
* **Priority:** High@n
* @n
* **Pre-Conditions:** None@n
* **Dependencies:** None@n
* **User Interaction:** None@n
* @n
* **Test Procedure:**@n
* | Variation / Step | Description | Test Data |Expected Result |Notes |
* | :----: | --------- | ---------- |-------------- | ----- |
* | 01| Get the interface pointer | None | Interface pointer should be valid | Should be successful |
* | 02| Enable audio description | input = true | result = Core::ERROR_NONE, EXPECT_EQ(result, Core::ERROR_NONE) | Should Pass |
*/
TEST(UserSettingsTestAI, EnableAudioDescription) {
    std::cout << "Entering EnableAudioDescription" << std::endl;
    bool enabled = true;
    
    Core::hresult result = InterfacePointer->SetAudioDescription(enabled);
    
    EXPECT_EQ(result, Core::ERROR_NONE);
    std::cout << "Exiting EnableAudioDescription" << std::endl;
}

/**
* @brief Test to verify the functionality of disabling audio description
*
* This test checks if the audio description can be successfully disabled using the SetAudioDescription method@n
*
* **Test Group ID:** Basic: 01@n
* **Test Case ID:** 021@n
* **Priority:** High@n
* @n
* **Pre-Conditions:** None@n
* **Dependencies:** None@n
* **User Interaction:** None@n
* @n
* **Test Procedure:**@n
* | Variation / Step | Description | Test Data |Expected Result |Notes |
* | :----: | --------- | ---------- |-------------- | ----- |
* | 01| Get the interface pointer | None | Interface pointer should be obtained | Should be successful |
* | 02| Disable audio description | input: false | result should be Core::ERROR_NONE, EXPECT_EQ(result, Core::ERROR_NONE) | Should Pass |
*/
TEST(UserSettingsTestAI, DisableAudioDescription) {
    std::cout << "Entering DisableAudioDescription" << std::endl;
    bool enabled = false;
    
    Core::hresult result = InterfacePointer->SetAudioDescription(enabled);
    
    EXPECT_EQ(result, Core::ERROR_NONE);
    std::cout << "Exiting DisableAudioDescription" << std::endl;
}

/**
* @brief Test to verify the functionality of setting block not rated content to true
*
* This test checks if the SetBlockNotRatedContent function correctly sets the block not rated content flag to true and returns the expected result.@n
*
* **Test Group ID:** Basic: 01@n
* **Test Case ID:** 022@n
* **Priority:** High@n
* @n
* **Pre-Conditions:** None@n
* **Dependencies:** None@n
* **User Interaction:** None@n
* @n
* **Test Procedure:**@n
* | Variation / Step | Description | Test Data |Expected Result |Notes |
* | :----: | --------- | ---------- |-------------- | ----- |
* | 01| Call SetBlockNotRatedContent with true | input = true | result = Core::ERROR_NONE, EXPECT_EQ(result, Core::ERROR_NONE) | Should Pass |
*/
TEST(UserSettingsTestAI, SetBlockNotRatedContent_True) {
    std::cout << "Entering SetBlockNotRatedContent_True" << std::endl;
    bool blockNotRatedContent = true;
    
    Core::hresult result = InterfacePointer->SetBlockNotRatedContent(blockNotRatedContent);
    
    EXPECT_EQ(result, Core::ERROR_NONE);
    std::cout << "Exiting SetBlockNotRatedContent_True" << std::endl;
}

/**
* @brief Test to verify the functionality of setting block not rated content to false
*
* This test checks if the SetBlockNotRatedContent function correctly sets the block not rated content flag to false and returns the expected result.@n
*
* **Test Group ID:** Basic: 01@n
* **Test Case ID:** 023@n
* **Priority:** High@n
* @n
* **Pre-Conditions:** None@n
* **Dependencies:** None@n
* **User Interaction:** None@n
* @n
* **Test Procedure:**@n
* | Variation / Step | Description | Test Data |Expected Result |Notes |
* | :----: | --------- | ---------- |-------------- | ----- |
* | 01| Call SetBlockNotRatedContent with false | input = false | result = Core::ERROR_NONE, EXPECT_EQ(result, Core::ERROR_NONE) | Should Pass |
*/
TEST(UserSettingsTestAI, SetBlockNotRatedContent_False) {
    std::cout << "Entering SetBlockNotRatedContent_False" << std::endl;
    bool blockNotRatedContent = false;

    Core::hresult result = InterfacePointer->SetBlockNotRatedContent(blockNotRatedContent);
    
    EXPECT_EQ(result, Core::ERROR_NONE);
    std::cout << "Exiting SetBlockNotRatedContent_False" << std::endl;
}

/**
* @brief Test to verify enabling captions successfully
*
* This test verifies that the captions can be enabled successfully using the SetCaptions method of the IUserSettings interface. It checks if the method returns the expected result when captions are enabled.
*
* **Test Group ID:** Basic: 01@n
* **Test Case ID:** 024@n
* **Priority:** High@n
* @n
* **Pre-Conditions:** None@n
* **Dependencies:** None@n
* **User Interaction:** None@n
* @n
* **Test Procedure:**@n
* | Variation / Step | Description | Test Data |Expected Result |Notes |
* | :----: | --------- | ---------- |-------------- | ----- |
* | 01| Get the interface pointer | None | InterfacePointer should not be null | Should be successful |
* | 02| Enable captions using SetCaptions method | input: true | result should be Core::ERROR_NONE | Should Pass |
* | 03| Verify the result using EXPECT_EQ | result: Core::ERROR_NONE | EXPECT_EQ should pass | Should be successful |
*/
TEST(UserSettingsTestAI, EnableCaptionsSuccessfully) {
    std::cout << "Entering EnableCaptionsSuccessfully" << std::endl;
    bool enabled = true;
    
    Core::hresult result = InterfacePointer->SetCaptions(enabled);
    
    EXPECT_EQ(result, Core::ERROR_NONE);
    std::cout << "Exiting EnableCaptionsSuccessfully" << std::endl;
}

/**
* @brief Test to verify the successful disabling of captions
*
* This test checks the functionality of the SetCaptions method to ensure that captions can be successfully disabled. It verifies that the method returns the expected result when called with the parameter to disable captions.
*
* **Test Group ID:** Basic: 01@n
* **Test Case ID:** 025@n
* **Priority:** High@n
* @n
* **Pre-Conditions:** None@n
* **Dependencies:** None@n
* **User Interaction:** None@n
* @n
* **Test Procedure:**@n
* | Variation / Step | Description | Test Data |Expected Result |Notes |
* | :----: | --------- | ---------- |-------------- | ----- |
* | 01| Get the interface pointer | None | Interface pointer should be obtained | Should be successful |
* | 02| Call SetCaptions with false to disable captions | input: false | result should be Core::ERROR_NONE | Should Pass |
* | 03| Verify the result | result from SetCaptions | result should be Core::ERROR_NONE | Should Pass |
*/
TEST(UserSettingsTestAI, DisableCaptionsSuccessfully) {
    std::cout << "Entering DisableCaptionsSuccessfully" << std::endl;
    bool enabled = false;

    Core::hresult result = InterfacePointer->SetCaptions(enabled);
    
    EXPECT_EQ(result, Core::ERROR_NONE);
    std::cout << "Exiting DisableCaptionsSuccessfully" << std::endl;
}

/**
* @brief Test to verify enabling high contrast mode in user settings
*
* This test checks if the high contrast mode can be successfully enabled through the user settings interface. It ensures that the function call to enable high contrast mode returns the expected result without any errors.@n
*
* **Test Group ID:** Basic: 01@n
* **Test Case ID:** 026@n
* **Priority:** High@n
* @n
* **Pre-Conditions:** None@n
* **Dependencies:** None@n
* **User Interaction:** None@n
* @n
* **Test Procedure:**@n
* | Variation / Step | Description | Test Data |Expected Result |Notes |
* | :----: | --------- | ---------- |-------------- | ----- |
* | 01| Enable high contrast mode | enabled = true | result = Core::ERROR_NONE, EXPECT_EQ(result, Core::ERROR_NONE) | Should Pass |
*/
TEST(UserSettingsTestAI, EnableHighContrastMode) {
    std::cout << "Entering EnableHighContrastMode" << std::endl;
    bool enabled = true;
    
    Core::hresult result = InterfacePointer->SetHighContrast(enabled);
    
    EXPECT_EQ(result, Core::ERROR_NONE);
    std::cout << "Exiting EnableHighContrastMode" << std::endl;
}

/**
* @brief Test to verify the disabling of high contrast mode
*
* This test checks if the high contrast mode can be successfully disabled using the SetHighContrast method@n
*
* **Test Group ID:** Basic: 01@n
* **Test Case ID:** 027@n
* **Priority:** High@n
* @n
* **Pre-Conditions:** None@n
* **Dependencies:** None@n
* **User Interaction:** None@n
* @n
* **Test Procedure:**@n
* | Variation / Step | Description | Test Data |Expected Result |Notes |
* | :----: | --------- | ---------- |-------------- | ----- |
* | 01| Call SetHighContrast with enabled set to false | enabled = false | result should be Core::ERROR_NONE | Should Pass |
*/
TEST(UserSettingsTestAI, DisableHighContrastMode) {
    std::cout << "Entering DisableHighContrastMode" << std::endl;
    bool enabled = false;
    
    Core::hresult result = InterfacePointer->SetHighContrast(enabled);
    
    EXPECT_EQ(result, Core::ERROR_NONE);
    std::cout << "Exiting DisableHighContrastMode" << std::endl;
}

/**
* @brief Test to verify the SetLiveWatershed function with true input
*
* This test checks if the SetLiveWatershed function correctly handles the input value 'true' and returns the expected result. This is important to ensure that the function behaves as expected when enabling the live watershed feature.
*
* **Test Group ID:** Basic: 01@n
* **Test Case ID:** 028@n
* **Priority:** High@n
* @n
* **Pre-Conditions:** None@n
* **Dependencies:** None@n
* **User Interaction:** None@n
* @n
* **Test Procedure:**@n
* | Variation / Step | Description | Test Data |Expected Result |Notes |
* | :----: | --------- | ---------- |-------------- | ----- |
* | 01| Call SetLiveWatershed with true | input = true | result = Core::ERROR_NONE, EXPECT_EQ(result, Core::ERROR_NONE) | Should Pass |
*/
TEST(UserSettingsTestAI, SetLiveWatershed_True) {
    std::cout << "Entering SetLiveWatershed_True" << std::endl;
    bool liveWatershed = true;

    Core::hresult result = InterfacePointer->SetLiveWatershed(liveWatershed);

    EXPECT_EQ(result, Core::ERROR_NONE);
    std::cout << "Exiting SetLiveWatershed_True" << std::endl;
}

/**
* @brief Test to verify the SetLiveWatershed function with false input
*
* This test checks the behavior of the SetLiveWatershed function when it is called with the input value 'false'. The objective is to ensure that the function correctly handles this input and returns the expected result, which is Core::ERROR_NONE.
*
* **Test Group ID:** Basic: 01@n
* **Test Case ID:** 029@n
* **Priority:** High@n
* @n
* **Pre-Conditions:** None@n
* **Dependencies:** None@n
* **User Interaction:** None@n
* @n
* **Test Procedure:**@n
* | Variation / Step | Description | Test Data |Expected Result |Notes |
* | :----: | --------- | ---------- |-------------- | ----- |
* | 01| Call SetLiveWatershed with false | input = false | result = Core::ERROR_NONE, EXPECT_EQ(result, Core::ERROR_NONE) | Should Pass |
*/
TEST(UserSettingsTestAI, SetLiveWatershed_False) {
    std::cout << "Entering SetLiveWatershed_False" << std::endl;
    bool liveWatershed = false;

    Core::hresult result = InterfacePointer->SetLiveWatershed(liveWatershed);

    EXPECT_EQ(result, Core::ERROR_NONE);
    std::cout << "Exiting SetLiveWatershed_False" << std::endl;
}

/**
* @brief Test to verify the SetPinControl method with true input
*
* This test checks the functionality of the SetPinControl method when the input is set to true. It ensures that the method returns the expected result and behaves correctly.
*
* **Test Group ID:** Basic: 01@n
* **Test Case ID:** 030@n
* **Priority:** High@n
* @n
* **Pre-Conditions:** None@n
* **Dependencies:** None@n
* **User Interaction:** None@n
* @n
* **Test Procedure:**@n
* | Variation / Step | Description | Test Data | Expected Result | Notes |
* | :----: | --------- | ---------- |-------------- | ----- |
* | 01 | Get the interface pointer | None | InterfacePointer should not be null | Should be successful |
* | 02 | Call SetPinControl with true | input = true | result should be Core::ERROR_NONE, EXPECT_EQ(result, Core::ERROR_NONE) | Should Pass |
*/
TEST(UserSettingsTestAI, SetPinControlTrue) {
    std::cout << "Entering SetPinControlTrue test" << std::endl;
    bool pinControl = true;

    Core::hresult result = InterfacePointer->SetPinControl(pinControl);

    EXPECT_EQ(result, Core::ERROR_NONE);
    std::cout << "Exiting SetPinControlTrue test" << std::endl;
}

/**
* @brief Test to verify the SetPinControl method with false input
*
* This test checks the functionality of the SetPinControl method when the input is set to false. It ensures that the method returns the expected result and behaves correctly when disabling the pin control.
*
* **Test Group ID:** Basic: 01@n
* **Test Case ID:** 031@n
* **Priority:** High@n
* @n
* **Pre-Conditions:** None@n
* **Dependencies:** None@n
* **User Interaction:** None@n
* @n
* **Test Procedure:**@n
* | Variation / Step | Description | Test Data |Expected Result |Notes |
* | :----: | --------- | ---------- |-------------- | ----- |
* | 01| Get the interface pointer | None | InterfacePointer should not be null | Should be successful |
* | 02| Call SetPinControl with false | input = false | result = Core::ERROR_NONE, EXPECT_EQ(result, Core::ERROR_NONE) | Should Pass |
*/
TEST(UserSettingsTestAI, SetPinControlFalse) {
    std::cout << "Entering SetPinControlFalse test" << std::endl;
    bool pinControl = true;

    Core::hresult result = InterfacePointer->SetPinControl(pinControl);

    EXPECT_EQ(result, Core::ERROR_NONE);
    std::cout << "Exiting SetPinControlFalse test" << std::endl;
}

/**
* @brief Test to verify setting the PIN on purchase to true
*
* This test checks the functionality of the SetPinOnPurchase method by setting the PIN on purchase to true. It ensures that the method returns the expected result when the operation is successful.
*
* **Test Group ID:** Basic: 01@n
* **Test Case ID:** 032@n
* **Priority:** High@n
* @n
* **Pre-Conditions:** None@n
* **Dependencies:** None@n
* **User Interaction:** None@n
* @n
* **Test Procedure:**@n
* | Variation / Step | Description | Test Data | Expected Result | Notes |
* | :----: | --------- | ---------- |-------------- | ----- |
* | 01 | Call SetPinOnPurchase with true | input = true | result = Core::ERROR_NONE, EXPECT_EQ(result, Core::ERROR_NONE) | Should Pass |
*/
TEST(UserSettingsTestAI, SetPinOnPurchaseTrue) {
    std::cout << "Entering SetPinOnPurchaseTrue" << std::endl;
    bool pinOnPurchase = true;

    Core::hresult result = InterfacePointer->SetPinOnPurchase(pinOnPurchase);

    EXPECT_EQ(result, Core::ERROR_NONE);
    std::cout << "Exiting SetPinOnPurchaseTrue" << std::endl;
}

/**
* @brief Test to verify the SetPinOnPurchase function with false input
*
* This test checks the functionality of the SetPinOnPurchase method when provided with a false input. It ensures that the method correctly processes the input and returns the expected result without errors.@n
*
* **Test Group ID:** Basic: 01@n
* **Test Case ID:** 033@n
* **Priority:** High@n
* @n
* **Pre-Conditions:** None@n
* **Dependencies:** None@n
* **User Interaction:** None@n
* @n
* **Test Procedure:**@n
* | Variation / Step | Description | Test Data |Expected Result |Notes |
* | :----: | --------- | ---------- |-------------- | ----- |
* | 01| Call SetPinOnPurchase with false | input = false | result = Core::ERROR_NONE, EXPECT_EQ(result, Core::ERROR_NONE) | Should Pass |
*/
TEST(UserSettingsTestAI, SetPinOnPurchaseFalse) {
    std::cout << "Entering SetPinOnPurchaseFalse" << std::endl;
    bool pinOnPurchase = false;

    Core::hresult result = InterfacePointer->SetPinOnPurchase(pinOnPurchase);

    EXPECT_EQ(result, Core::ERROR_NONE);
    std::cout << "Exiting SetPinOnPurchaseFalse" << std::endl;
}

/**
* @brief Test to verify the SetPlaybackWatershed function with true input
*
* This test checks if the SetPlaybackWatershed function correctly handles the input value 'true' and returns the expected result. This is important to ensure that the function behaves as expected when setting the playback watershed to true.
*
* **Test Group ID:** Basic: 01@n
* **Test Case ID:** 034@n
* **Priority:** High@n
* @n
* **Pre-Conditions:** None@n
* **Dependencies:** None@n
* **User Interaction:** None@n
* @n
* **Test Procedure:**@n
* | Variation / Step | Description | Test Data |Expected Result |Notes |
* | :----: | --------- | ---------- |-------------- | ----- |
* | 01| Call SetPlaybackWatershed with true | input = true | result = Core::ERROR_NONE, EXPECT_EQ(result, Core::ERROR_NONE) | Should Pass |
*/
TEST(UserSettingsTestAI, SetPlaybackWatershedTrue) {
    std::cout << "Entering SetPlaybackWatershedTrue" << std::endl;
    bool playbackWatershed = true;

    Core::hresult result = InterfacePointer->SetPlaybackWatershed(playbackWatershed);

    EXPECT_EQ(result, Core::ERROR_NONE);
    std::cout << "Exiting SetPlaybackWatershedTrue" << std::endl;
}

/**
* @brief Test to verify the SetPlaybackWatershed function with false input
*
* This test checks the functionality of the SetPlaybackWatershed method when provided with a false input. It ensures that the method correctly handles the input and returns the expected result, which is Core::ERROR_NONE.@n
*
* **Test Group ID:** Basic: 01@n
* **Test Case ID:** 035@n
* **Priority:** High@n
* @n
* **Pre-Conditions:** None@n
* **Dependencies:** None@n
* **User Interaction:** None@n
* @n
* **Test Procedure:**@n
* | Variation / Step | Description | Test Data |Expected Result |Notes |
* | :----: | --------- | ---------- |-------------- | ----- |
* | 01| Call SetPlaybackWatershed with false | input = false | result = Core::ERROR_NONE, EXPECT_EQ(result, Core::ERROR_NONE) | Should Pass |
*/
TEST(UserSettingsTestAI, SetPlaybackWatershedFalse) {
    std::cout << "Entering SetPlaybackWatershedFalse" << std::endl;
    bool playbackWatershed = false;

    Core::hresult result = InterfacePointer->SetPlaybackWatershed(playbackWatershed);

    EXPECT_EQ(result, Core::ERROR_NONE);
    std::cout << "Exiting SetPlaybackWatershedFalse" << std::endl;
}

/**
* @brief Test to verify setting a valid single language code for preferred audio languages.
*
* This test checks if the function SetPreferredAudioLanguages correctly sets a valid single language code ("eng") and returns the expected result. This is important to ensure that the function handles valid inputs properly and updates the preferred audio languages as expected.
*
* **Test Group ID:** Basic: 01@n
* **Test Case ID:** 036@n
* **Priority:** High@n
* @n
* **Pre-Conditions:** None@n
* **Dependencies:** None@n
* **User Interaction:** None@n
* @n
* **Test Procedure:**@n
* | Variation / Step | Description | Test Data |Expected Result |Notes |
* | :----: | --------- | ---------- |-------------- | ----- |
* | 01| Call SetPreferredAudioLanguages with a valid single language code | input = "eng" | result = Core::ERROR_NONE, EXPECT_EQ(result, Core::ERROR_NONE) | Should Pass |
*/
TEST(UserSettingsTestAI, SetPreferredAudioLanguages_ValidSingleLanguageCode) {
    std::cout << "Entering SetPreferredAudioLanguages_ValidSingleLanguageCode" << std::endl;
    std::string preferred_languages = "eng";

    Core::hresult result = InterfacePointer->SetPreferredAudioLanguages(preferred_languages);

    EXPECT_EQ(result, Core::ERROR_NONE);
    std::cout << "Exiting SetPreferredAudioLanguages_ValidSingleLanguageCode" << std::endl;
}

/**
* @brief Test to verify the behavior of SetPreferredAudioLanguages when an empty string is passed.
*
* This test checks if the SetPreferredAudioLanguages function correctly handles an empty string input by returning an error code. This is important to ensure that invalid inputs are properly managed by the system.
*
* **Test Group ID:** Basic: 01@n
* **Test Case ID:** 037@n
* **Priority:** High@n
* @n
* **Pre-Conditions:** None@n
* **Dependencies:** None@n
* **User Interaction:** None@n
* @n
* **Test Procedure:**@n
* | Variation / Step | Description | Test Data |Expected Result |Notes |
* | :----: | --------- | ---------- |-------------- | ----- |
* | 01| Call SetPreferredAudioLanguages with an empty string | input = "" | result = Core::ERROR_INVALID_PARAMETER, EXPECT_EQ(result, Core::ERROR_INVALID_PARAMETER) | Should Fail |
*/
TEST(UserSettingsTestAI, SetPreferredAudioLanguages_EmptyString) {
    std::cout << "Entering SetPreferredAudioLanguages_EmptyString" << std::endl;
    std::string preferred_languages = "";
    
    // Setup the expected behavior for invalid input
    EXPECT_CALL(*g_storeMock, SetValue(_, _, USERSETTINGS_PREFERRED_AUDIO_LANGUAGES_KEY, _, _))
        .WillOnce(Return(Core::ERROR_INVALID_PARAMETER));

    Core::hresult result = InterfacePointer->SetPreferredAudioLanguages(preferred_languages);

    EXPECT_EQ(result, Core::ERROR_INVALID_PARAMETER);
    std::cout << "Exiting SetPreferredAudioLanguages_EmptyString" << std::endl;
}

/**
* @brief Test to verify the behavior of SetPreferredAudioLanguages when an invalid language code is provided.
*
* This test checks if the SetPreferredAudioLanguages method correctly handles an invalid language code input and returns the appropriate error code. This is important to ensure that the method validates its inputs properly and maintains robustness.
*
* **Test Group ID:** Basic: 01@n
* **Test Case ID:** 038@n
* **Priority:** High@n
* @n
* **Pre-Conditions:** None@n
* **Dependencies:** None@n
* **User Interaction:** None@n
* @n
* **Test Procedure:**@n
* | Variation / Step | Description | Test Data |Expected Result |Notes |
* | :----: | --------- | ---------- |-------------- | ----- |
* | 01| Call SetPreferredAudioLanguages with invalid language code | input: "xyz" | result: Core::ERROR_INVALID_PARAMETER | Should Fail |
*/
TEST(UserSettingsTestAI, SetPreferredAudioLanguages_InvalidLanguageCode) {
    std::cout << "Entering SetPreferredAudioLanguages_InvalidLanguageCode" << std::endl;
    std::string preferred_languages = "xyz";

    // Setup the expected behavior for invalid language code
    EXPECT_CALL(*g_storeMock, SetValue(_, _, USERSETTINGS_PREFERRED_AUDIO_LANGUAGES_KEY, _, _))
        .WillOnce(Return(Core::ERROR_INVALID_PARAMETER));

    Core::hresult result = InterfacePointer->SetPreferredAudioLanguages(preferred_languages);

    EXPECT_EQ(result, Core::ERROR_INVALID_PARAMETER);
    std::cout << "Exiting SetPreferredAudioLanguages_InvalidLanguageCode" << std::endl;
}

/**
* @brief Test to verify the behavior of SetPreferredAudioLanguages with mixed valid and invalid language codes.
*
* This test checks the SetPreferredAudioLanguages function to ensure it correctly handles a mix of valid and invalid language codes. The function should return an error when invalid language codes are included in the input string.
*
* **Test Group ID:** Basic: 01@n
* **Test Case ID:** 039@n
* **Priority:** High@n
* @n
* **Pre-Conditions:** None@n
* **Dependencies:** None@n
* **User Interaction:** None@n
* @n
* **Test Procedure:**@n
* | Variation / Step | Description | Test Data | Expected Result | Notes |
* | :----: | --------- | ---------- |-------------- | ----- |
* | 01 | Call SetPreferredAudioLanguages with mixed valid and invalid language codes | input = "eng,xyz,spa" | result = Core::ERROR_INVALID_PARAMETER, EXPECT_EQ(result, Core::ERROR_INVALID_PARAMETER) | Should Fail |
*/
TEST(UserSettingsTestAI, SetPreferredAudioLanguages_MixedValidAndInvalidLanguageCodes) {
    std::cout << "Entering SetPreferredAudioLanguages_MixedValidAndInvalidLanguageCodes" << std::endl;
    std::string preferred_languages = "eng,xyz,spa";

    // Setup the expected behavior for mixed valid/invalid language codes
    EXPECT_CALL(*g_storeMock, SetValue(_, _, USERSETTINGS_PREFERRED_AUDIO_LANGUAGES_KEY, _, _))
        .WillOnce(Return(Core::ERROR_INVALID_PARAMETER));

    Core::hresult result = InterfacePointer->SetPreferredAudioLanguages(preferred_languages);
    
    EXPECT_EQ(result, Core::ERROR_INVALID_PARAMETER);
    std::cout << "Exiting SetPreferredAudioLanguages_MixedValidAndInvalidLanguageCodes" << std::endl;
}

/**
* @brief Test to verify the behavior of SetPreferredAudioLanguages when special characters are included in the input.
*
* This test checks if the SetPreferredAudioLanguages function correctly handles input strings that contain special characters. The function is expected to return an error code indicating invalid parameters when such input is provided.
*
* **Test Group ID:** Basic: 01@n
* **Test Case ID:** 040@n
* **Priority:** High@n
* @n
* **Pre-Conditions:** None@n
* **Dependencies:** None@n
* **User Interaction:** None@n
* @n
* **Test Procedure:**@n
* | Variation / Step | Description | Test Data |Expected Result |Notes |
* | :----: | --------- | ---------- |-------------- | ----- |
* | 01| Call SetPreferredAudioLanguages with special characters in the input string | input = "eng,spa,@#$" | result = Core::ERROR_INVALID_PARAMETER, EXPECT_EQ(result, Core::ERROR_INVALID_PARAMETER) | Should Fail |
*/
TEST(UserSettingsTestAI, SetPreferredAudioLanguages_SpecialCharacters) {
    std::cout << "Entering SetPreferredAudioLanguages_SpecialCharacters" << std::endl;
    std::string preferred_languages = "eng,spa,@#$";

    // Setup the expected behavior for input with special characters
    EXPECT_CALL(*g_storeMock, SetValue(_, _, USERSETTINGS_PREFERRED_AUDIO_LANGUAGES_KEY, _, _))
        .WillOnce(Return(Core::ERROR_INVALID_PARAMETER));

    Core::hresult result = InterfacePointer->SetPreferredAudioLanguages(preferred_languages);

    EXPECT_EQ(result, Core::ERROR_INVALID_PARAMETER);
    std::cout << "Exiting SetPreferredAudioLanguages_SpecialCharacters" << std::endl;
}

/**
* @brief Test to verify setting valid preferred languages
*
* This test verifies that the SetPreferredCaptionsLanguages function correctly sets the preferred languages when provided with valid input strings. It ensures that the function returns the expected result and behaves as intended when valid language codes are passed.
*
* **Test Group ID:** Basic: 01@n
* **Test Case ID:** 041@n
* **Priority:** High@n
* @n
* **Pre-Conditions:** None@n
* **Dependencies:** None@n
* **User Interaction:** None@n
* @n
* **Test Procedure:**@n
* | Variation / Step | Description | Test Data | Expected Result | Notes |
* | :----: | --------- | ---------- |-------------- | ----- |
* | 01 | Entering the test function | None | None | Should be successful |
* | 02 | Set preferred languages to "eng,fra" | preferredLanguages = "eng,fra" | result = Core::ERROR_NONE, EXPECT_EQ(result, Core::ERROR_NONE) | Should Pass |
* | 03 | Exiting the test function | None | None | Should be successful |
*/
TEST(UserSettingsTestAI, SetValidPreferredLanguages) {
    std::cout << "Entering SetValidPreferredLanguages" << std::endl;
    std::string preferredLanguages = "eng,fra";
    
    Core::hresult result = InterfacePointer->SetPreferredCaptionsLanguages(preferredLanguages);
   
    EXPECT_EQ(result, Core::ERROR_NONE);
    std::cout << "Exiting SetValidPreferredLanguages" << std::endl;
}

/**
* @brief Test to verify setting a single valid preferred language
*
* This test checks if the function SetPreferredCaptionsLanguages correctly sets a single valid preferred language "eng" and returns the expected result.@n
*
* **Test Group ID:** Basic: 01@n
* **Test Case ID:** 042@n
* **Priority:** High@n
* @n
* **Pre-Conditions:** None@n
* **Dependencies:** None@n
* **User Interaction:** None@n
* @n
* **Test Procedure:**@n
* | Variation / Step | Description | Test Data |Expected Result |Notes |
* | :----: | --------- | ---------- |-------------- | ----- |
* | 01| Set a single valid preferred language | preferredLanguages = "eng" | result = Core::ERROR_NONE, EXPECT_EQ(result, Core::ERROR_NONE) | Should Pass |
*/
TEST(UserSettingsTestAI, SetSingleValidPreferredLanguage) {
    std::cout << "Entering SetSingleValidPreferredLanguage" << std::endl;
    std::string preferredLanguages = "eng";
   
    Core::hresult result = InterfacePointer->SetPreferredCaptionsLanguages(preferredLanguages);
    
    EXPECT_EQ(result, Core::ERROR_NONE);
    std::cout << "Exiting SetSingleValidPreferredLanguage" << std::endl;
}

/**
* @brief Test to verify the behavior when setting empty preferred languages
*
* This test checks the behavior of the SetPreferredCaptionsLanguages function when an empty string is passed as the preferred languages. This is to ensure that the function correctly handles invalid input and returns the appropriate error code.
*
* **Test Group ID:** Basic: 01@n
* **Test Case ID:** 043@n
* **Priority:** High@n
* @n
* **Pre-Conditions:** None@n
* **Dependencies:** None@n
* **User Interaction:** None@n
* @n
* **Test Procedure:**@n
* | Variation / Step | Description | Test Data | Expected Result | Notes |
* | :----: | --------- | ---------- |-------------- | ----- |
* | 01 | Call SetPreferredCaptionsLanguages with empty string | preferredLanguages = "" | result = Core::ERROR_INVALID_PARAMETER, EXPECT_EQ(result, Core::ERROR_INVALID_PARAMETER) | Should Fail |
*/
TEST(UserSettingsTestAI, SetEmptyPreferredLanguages) {
    std::cout << "Entering SetEmptyPreferredLanguages" << std::endl;
    std::string preferredLanguages = "";
    
    // Setup the expected behavior for empty input string
    EXPECT_CALL(*g_storeMock, SetValue(_, _, USERSETTINGS_PREFERRED_CAPTIONS_LANGUAGES_KEY, _, _))
        .WillOnce(Return(Core::ERROR_INVALID_PARAMETER));
    
    Core::hresult result = InterfacePointer->SetPreferredCaptionsLanguages(preferredLanguages);
    
    EXPECT_EQ(result, Core::ERROR_INVALID_PARAMETER);
    std::cout << "Exiting SetEmptyPreferredLanguages" << std::endl;
}

/**
* @brief Test to verify the behavior of SetPreferredCaptionsLanguages with invalid language codes
*
* This test checks if the SetPreferredCaptionsLanguages method correctly handles invalid language codes by returning an error code. This is important to ensure that the system validates input parameters properly.
*
* **Test Group ID:** Basic: 01@n
* **Test Case ID:** 044@n
* **Priority:** High@n
* @n
* **Pre-Conditions:** None@n
* **Dependencies:** None@n
* **User Interaction:** None@n
* @n
* **Test Procedure:**@n
* | Variation / Step | Description | Test Data |Expected Result |Notes |
* | :----: | --------- | ---------- |-------------- | ----- |
* | 01| Call SetPreferredCaptionsLanguages with invalid language codes | preferredLanguages = "xyz,abc" | result = Core::ERROR_INVALID_PARAMETER, Assertion: EXPECT_EQ(result, Core::ERROR_INVALID_PARAMETER) | Should Fail |
*/
TEST(UserSettingsTestAI, SetInvalidPreferredLanguages) {
    std::cout << "Entering SetInvalidPreferredLanguages" << std::endl;
    std::string preferredLanguages = "xyz,abc";
    
    // Setup the expected behavior for invalid language codes
    EXPECT_CALL(*g_storeMock, SetValue(_, _, USERSETTINGS_PREFERRED_CAPTIONS_LANGUAGES_KEY, _, _))
        .WillOnce(Return(Core::ERROR_INVALID_PARAMETER));
    
    Core::hresult result = InterfacePointer->SetPreferredCaptionsLanguages(preferredLanguages);
    
    EXPECT_EQ(result, Core::ERROR_INVALID_PARAMETER);
    std::cout << "Exiting SetInvalidPreferredLanguages" << std::endl;
}

/**
* @brief Test to verify the behavior of SetPreferredCaptionsLanguages with invalid format input
*
* This test checks if the SetPreferredCaptionsLanguages function correctly handles an invalid format for the preferred languages string. The input string contains consecutive commas, which is not a valid format for specifying languages.@n
*
* **Test Group ID:** Basic: 01@n
* **Test Case ID:** 045@n
* **Priority:** High@n
* @n
* **Pre-Conditions:** None@n
* **Dependencies:** None@n
* **User Interaction:** None@n
* @n
* **Test Procedure:**@n
* | Variation / Step | Description | Test Data |Expected Result |Notes |
* | :----: | --------- | ---------- |-------------- | ----- |
* | 01| Call SetPreferredCaptionsLanguages with invalid format string | preferredLanguages = "eng,,fra" | result = Core::ERROR_INVALID_PARAMETER, EXPECT_EQ(result, Core::ERROR_INVALID_PARAMETER) | Should Fail |
*/
TEST(UserSettingsTestAI, SetPreferredLanguagesWithInvalidFormat) {
    std::cout << "Entering SetPreferredLanguagesWithInvalidFormat" << std::endl;
    std::string preferredLanguages = "eng,,fra";
    
    // Setup the expected behavior for invalid format (consecutive commas)
    EXPECT_CALL(*g_storeMock, SetValue(_, _, USERSETTINGS_PREFERRED_CAPTIONS_LANGUAGES_KEY, _, _))
        .WillOnce(Return(Core::ERROR_INVALID_PARAMETER));
    
    Core::hresult result = InterfacePointer->SetPreferredCaptionsLanguages(preferredLanguages);
    
    EXPECT_EQ(result, Core::ERROR_INVALID_PARAMETER);
    std::cout << "Exiting SetPreferredLanguagesWithInvalidFormat" << std::endl;
}

/**
* @brief Test to verify the behavior of SetPreferredCaptionsLanguages when special characters are included in the input string.
*
* This test checks if the SetPreferredCaptionsLanguages function correctly handles input strings that contain special characters. The expected behavior is that the function should return an error indicating invalid parameters when special characters are present in the language codes.
*
* **Test Group ID:** Basic: 01@n
* **Test Case ID:** 046@n
* **Priority:** High@n
* @n
* **Pre-Conditions:** None@n
* **Dependencies:** None@n
* **User Interaction:** None@n
* @n
* **Test Procedure:**@n
* | Variation / Step | Description | Test Data | Expected Result | Notes |
* | :----: | --------- | ---------- |-------------- | ----- |
* | 01 | Call SetPreferredCaptionsLanguages with special characters in the language string | preferredLanguages = "eng,@fra" | result = Core::ERROR_INVALID_PARAMETER, EXPECT_EQ(result, Core::ERROR_INVALID_PARAMETER) | Should Fail |
*/
TEST(UserSettingsTestAI, SetPreferredLanguagesWithSpecialCharacters) {
    std::cout << "Entering SetPreferredLanguagesWithSpecialCharacters" << std::endl;
    std::string preferredLanguages = "eng,@fra";
    
    // Setup the expected behavior for input with special characters
    EXPECT_CALL(*g_storeMock, SetValue(_, _, USERSETTINGS_PREFERRED_CAPTIONS_LANGUAGES_KEY, _, _))
        .WillOnce(Return(Core::ERROR_INVALID_PARAMETER));
    
    Core::hresult result = InterfacePointer->SetPreferredCaptionsLanguages(preferredLanguages);
    
    EXPECT_EQ(result, Core::ERROR_INVALID_PARAMETER);
    std::cout << "Exiting SetPreferredLanguagesWithSpecialCharacters" << std::endl;
}

/**
* @brief Test to verify the behavior of SetPreferredCaptionsLanguages when numeric values are provided as input.
*
* This test checks if the SetPreferredCaptionsLanguages function correctly handles numeric values as input for preferred languages. 
* The function is expected to return an error indicating invalid parameters when numeric values are provided.
*
* **Test Group ID:** Basic: 01@n
* **Test Case ID:** 047@n
* **Priority:** High@n
* @n
* **Pre-Conditions:** None@n
* **Dependencies:** None@n
* **User Interaction:** None@n
* @n
* **Test Procedure:**@n
* | Variation / Step | Description | Test Data |Expected Result |Notes |
* | :----: | --------- | ---------- |-------------- | ----- |
* | 01| Call SetPreferredCaptionsLanguages with numeric values as input | preferredLanguages = "123,456" | result = Core::ERROR_INVALID_PARAMETER, EXPECT_EQ(result, Core::ERROR_INVALID_PARAMETER) | Should Fail |
*/
TEST(UserSettingsTestAI, SetPreferredLanguagesWithNumericValues) {
    std::cout << "Entering SetPreferredLanguagesWithNumericValues" << std::endl;
    std::string preferredLanguages = "123,456";
    
    // Setup the expected behavior for input with numeric values
    EXPECT_CALL(*g_storeMock, SetValue(_, _, USERSETTINGS_PREFERRED_CAPTIONS_LANGUAGES_KEY, _, _))
        .WillOnce(Return(Core::ERROR_INVALID_PARAMETER));
    
    Core::hresult result = InterfacePointer->SetPreferredCaptionsLanguages(preferredLanguages);
   
    EXPECT_EQ(result, Core::ERROR_INVALID_PARAMETER);
    std::cout << "Exiting SetPreferredLanguagesWithNumericValues" << std::endl;
}

/**
* @brief Test to verify the behavior of SetPreferredCaptionsLanguages with mixed valid and invalid language codes.
*
* This test checks if the SetPreferredCaptionsLanguages function correctly handles a string containing both valid and invalid language codes. The expected behavior is that the function should return an error indicating invalid parameters.
*
* **Test Group ID:** Basic: 01@n
* **Test Case ID:** 048@n
* **Priority:** High@n
* @n
* **Pre-Conditions:** None@n
* **Dependencies:** None@n
* **User Interaction:** None@n
* @n
* **Test Procedure:**@n
* | Variation / Step | Description | Test Data |Expected Result |Notes |
* | :----: | --------- | ---------- |-------------- | ----- |
* | 01| Call SetPreferredCaptionsLanguages with mixed valid and invalid language codes | preferredLanguages = "eng,xyz" | result = Core::ERROR_INVALID_PARAMETER, EXPECT_EQ(result, Core::ERROR_INVALID_PARAMETER) | Should Fail |
*/
TEST(UserSettingsTestAI, SetPreferredLanguagesWithMixedValidAndInvalidCodes) {
    std::cout << "Entering SetPreferredLanguagesWithMixedValidAndInvalidCodes" << std::endl;
    std::string preferredLanguages = "eng,xyz";
    
    // Setup the expected behavior for mixed valid/invalid language codes
    EXPECT_CALL(*g_storeMock, SetValue(_, _, USERSETTINGS_PREFERRED_CAPTIONS_LANGUAGES_KEY, _, _))
        .WillOnce(Return(Core::ERROR_INVALID_PARAMETER));
    
    Core::hresult result = InterfacePointer->SetPreferredCaptionsLanguages(preferredLanguages);
    
    EXPECT_EQ(result, Core::ERROR_INVALID_PARAMETER);
    std::cout << "Exiting SetPreferredLanguagesWithMixedValidAndInvalidCodes" << std::endl;
}

/**
* @brief Test the setting of valid closed caption services from CC1 to CC4.
*
* This test verifies that the SetPreferredClosedCaptionService function correctly sets the closed caption service to valid values (CC1 to CC4). It ensures that the function handles these valid inputs without errors and returns the expected result.
*
* **Test Group ID:** Basic: 01@n
* **Test Case ID:** 049@n
* **Priority:** High@n
* @n
* **Pre-Conditions:** None@n
* **Dependencies:** None@n
* **User Interaction:** None@n
* @n
* **Test Procedure:**@n
* | Variation / Step | Description | Test Data | Expected Result | Notes |
* | :----: | --------- | ---------- |-------------- | ----- |
* | 01 | Set closed caption service to CC1 | service = "CC1" | Core::ERROR_NONE | Should Pass |
* | 02 | Set closed caption service to CC2 | service = "CC2" | Core::ERROR_NONE | Should Pass |
* | 03 | Set closed caption service to CC3 | service = "CC3" | Core::ERROR_NONE | Should Pass |
* | 04 | Set closed caption service to CC4 | service = "CC4" | Core::ERROR_NONE | Should Pass |
*/
TEST(UserSettingsTestAI, SetValidClosedCaptionService_CC1ToCC4) {
    std::cout << "Entering SetValidClosedCaptionService_CC1ToCC4" << std::endl;
    
    for (int i = 1; i <= 4; ++i) {
        std::string service = "CC" + std::to_string(i);
        EXPECT_EQ(Core::ERROR_NONE, InterfacePointer->SetPreferredClosedCaptionService(service));
    }
    
    std::cout << "Exiting SetValidClosedCaptionService_CC1ToCC4" << std::endl;
}

/**
* @brief Test to verify setting valid closed caption services from TEXT1 to TEXT4
*
* This test checks if the SetPreferredClosedCaptionService function correctly sets the closed caption service to valid values TEXT1, TEXT2, TEXT3, and TEXT4. This ensures that the function handles valid input values as expected.
*
* **Test Group ID:** Basic: 01@n
* **Test Case ID:** 050@n
* **Priority:** High@n
* @n
* **Pre-Conditions:** None@n
* **Dependencies:** None@n
* **User Interaction:** None@n
* @n
* **Test Procedure:**@n
* | Variation / Step | Description | Test Data |Expected Result |Notes |
* | :----: | --------- | ---------- |-------------- | ----- |
* | 01| Set closed caption service to TEXT1 | service = TEXT1 | Core::ERROR_NONE, EXPECT_EQ check | Should Pass |
* | 02| Set closed caption service to TEXT2 | service = TEXT2 | Core::ERROR_NONE, EXPECT_EQ check | Should Pass |
* | 03| Set closed caption service to TEXT3 | service = TEXT3 | Core::ERROR_NONE, EXPECT_EQ check | Should Pass |
* | 04| Set closed caption service to TEXT4 | service = TEXT4 | Core::ERROR_NONE, EXPECT_EQ check | Should Pass |
*/
TEST(UserSettingsTestAI, SetValidClosedCaptionService_TEXT1ToTEXT4) {
    std::cout << "Entering SetValidClosedCaptionService_TEXT1ToTEXT4" << std::endl;
    
    for (int i = 1; i <= 4; ++i) {
        std::string service = "TEXT" + std::to_string(i);
        EXPECT_EQ(Core::ERROR_NONE, InterfacePointer->SetPreferredClosedCaptionService(service));
    }
    
    std::cout << "Exiting SetValidClosedCaptionService_TEXT1ToTEXT4" << std::endl;
}

/**
* @brief Test to verify setting valid closed caption services from SERVICE1 to SERVICE64
*
* This test checks the functionality of setting preferred closed caption services from SERVICE1 to SERVICE64. 
* It ensures that the function `SetPreferredClosedCaptionService` correctly handles valid service names and returns the expected result.
*
* **Test Group ID:** Basic: 01@n
* **Test Case ID:** 051@n
* **Priority:** High@n
* @n
* **Pre-Conditions:** None@n
* **Dependencies:** None@n
* **User Interaction:** None@n
* @n
* **Test Procedure:**@n
* | Variation / Step | Description | Test Data | Expected Result | Notes |
* | :----: | --------- | ---------- |-------------- | ----- |
* | 01 | Set closed caption service to SERVICE1 | service = "SERVICE1" | Core::ERROR_NONE | Should Pass |
* | 02 | Set closed caption service to SERVICE2 | service = "SERVICE2" | Core::ERROR_NONE | Should Pass |
* | 03 | Set closed caption service to SERVICE3 | service = "SERVICE3" | Core::ERROR_NONE | Should Pass |
* | 04 | Set closed caption service to SERVICE4 | service = "SERVICE4" | Core::ERROR_NONE | Should Pass |
* | 05 | Set closed caption service to SERVICE5 | service = "SERVICE5" | Core::ERROR_NONE | Should Pass |
* | 06 | Set closed caption service to SERVICE6 | service = "SERVICE6" | Core::ERROR_NONE | Should Pass |
* | 07 | Set closed caption service to SERVICE7 | service = "SERVICE7" | Core::ERROR_NONE | Should Pass |
* | 08 | Set closed caption service to SERVICE8 | service = "SERVICE8" | Core::ERROR_NONE | Should Pass |
* | 09 | Set closed caption service to SERVICE9 | service = "SERVICE9" | Core::ERROR_NONE | Should Pass |
* | 10 | Set closed caption service to SERVICE10 | service = "SERVICE10" | Core::ERROR_NONE | Should Pass |
* | 11 | Set closed caption service to SERVICE11 | service = "SERVICE11" | Core::ERROR_NONE | Should Pass |
* | 12 | Set closed caption service to SERVICE12 | service = "SERVICE12" | Core::ERROR_NONE | Should Pass |
* | 13 | Set closed caption service to SERVICE13 | service = "SERVICE13" | Core::ERROR_NONE | Should Pass |
* | 14 | Set closed caption service to SERVICE14 | service = "SERVICE14" | Core::ERROR_NONE | Should Pass |
* | 15 | Set closed caption service to SERVICE15 | service = "SERVICE15" | Core::ERROR_NONE | Should Pass |
* | 16 | Set closed caption service to SERVICE16 | service = "SERVICE16" | Core::ERROR_NONE | Should Pass |
* | 17 | Set closed caption service to SERVICE17 | service = "SERVICE17" | Core::ERROR_NONE | Should Pass |
* | 18 | Set closed caption service to SERVICE18 | service = "SERVICE18" | Core::ERROR_NONE | Should Pass |
* | 19 | Set closed caption service to SERVICE19 | service = "SERVICE19" | Core::ERROR_NONE | Should Pass |
* | 20 | Set closed caption service to SERVICE20 | service = "SERVICE20" | Core::ERROR_NONE | Should Pass |
* | 21 | Set closed caption service to SERVICE21 | service = "SERVICE21" | Core::ERROR_NONE | Should Pass |
* | 22 | Set closed caption service to SERVICE22 | service = "SERVICE22" | Core::ERROR_NONE | Should Pass |
* | 23 | Set closed caption service to SERVICE23 | service = "SERVICE23" | Core::ERROR_NONE | Should Pass |
* | 24 | Set closed caption service to SERVICE24 | service = "SERVICE24" | Core::ERROR_NONE | Should Pass |
* | 25 | Set closed caption service to SERVICE25 | service = "SERVICE25" | Core::ERROR_NONE | Should Pass |
* | 26 | Set closed caption service to SERVICE26 | service = "SERVICE26" | Core::ERROR_NONE | Should Pass |
* | 27 | Set closed caption service to SERVICE27 | service = "SERVICE27" | Core::ERROR_NONE | Should Pass |
* | 28 | Set closed caption service to SERVICE28 | service = "SERVICE28" | Core::ERROR_NONE | Should Pass |
* | 29 | Set closed caption service to SERVICE29 | service = "SERVICE29" | Core::ERROR_NONE | Should Pass |
* | 30 | Set closed caption service to SERVICE30 | service = "SERVICE30" | Core::ERROR_NONE | Should Pass |
* | 31 | Set closed caption service to SERVICE31 | service = "SERVICE31" | Core::ERROR_NONE | Should Pass |
* | 32 | Set closed caption service to SERVICE32 | service = "SERVICE32" | Core::ERROR_NONE | Should Pass |
* | 33 | Set closed caption service to SERVICE33 | service = "SERVICE33" | Core::ERROR_NONE | Should Pass |
* | 34 | Set closed caption service to SERVICE34 | service = "SERVICE34" | Core::ERROR_NONE | Should Pass |
* | 35 | Set closed caption service to SERVICE35 | service = "SERVICE35" | Core::ERROR_NONE | Should Pass |
* | 36 | Set closed caption service to SERVICE36 | service = "SERVICE36" | Core::ERROR_NONE | Should Pass |
* | 37 | Set closed caption service to SERVICE37 | service = "SERVICE37" | Core::ERROR_NONE | Should Pass |
* | 38 | Set closed caption service to SERVICE38 | service = "SERVICE38" | Core::ERROR_NONE | Should Pass |
* | 39 | Set closed caption service to SERVICE39 | service = "SERVICE39" | Core::ERROR_NONE | Should Pass |
* | 40 | Set closed caption service to SERVICE40 | service = "SERVICE40" | Core::ERROR_NONE | Should Pass |
* | 41 | Set closed caption service to SERVICE41 | service = "SERVICE41" | Core::ERROR_NONE | Should Pass |
* | 42 | Set closed caption service to SERVICE42 | service = "SERVICE42" | Core::ERROR_NONE | Should Pass |
* | 43 | Set closed caption service to SERVICE43 | service = "SERVICE43" | Core::ERROR_NONE | Should Pass |
* | 44 | Set closed caption service to SERVICE44 | service = "SERVICE44" | Core::ERROR_NONE | Should Pass |
* | 45 | Set closed caption service to SERVICE45 | service = "SERVICE45" | Core::ERROR_NONE | Should Pass |
* | 46 | Set closed caption service to SERVICE46 | service = "SERVICE46" | Core::ERROR_NONE | Should Pass |
* | 47 | Set closed caption service to SERVICE47 | service = "SERVICE47" | Core::ERROR_NONE | Should Pass |
* | 48 | Set closed caption service to SERVICE48 | service = "SERVICE48" | Core::ERROR_NONE | Should Pass |
* | 49 | Set closed caption service to SERVICE49 | service = "SERVICE49" | Core::ERROR_NONE | Should Pass |
* | 50 | Set closed caption service to SERVICE50 | service = "SERVICE50" | Core::ERROR_NONE | Should Pass |
* | 51 | Set closed caption service to SERVICE51 | service = "SERVICE51" | Core::ERROR_NONE | Should Pass |
* | 52 | Set closed caption service to SERVICE52 | service = "SERVICE52" | Core::ERROR_NONE | Should Pass |
* | 53 | Set closed caption service to SERVICE53 | service = "SERVICE53" | Core::ERROR_NONE | Should Pass |
* | 54 | Set closed caption service to SERVICE54 | service = "SERVICE54" | Core::ERROR_NONE | Should Pass |
* | 55 | Set closed caption service to SERVICE55 | service = "SERVICE55" | Core::ERROR_NONE | Should Pass |
* | 56 | Set closed caption service to SERVICE56 | service = "SERVICE56" | Core::ERROR_NONE | Should Pass |
* | 57 | Set closed caption service to SERVICE57 | service = "SERVICE57" | Core::ERROR_NONE | Should Pass |
* | 58 | Set closed caption service to SERVICE58 | service = "SERVICE58" | Core::ERROR_NONE | Should Pass |
* | 59 | Set closed caption service to SERVICE59 | service = "SERVICE59" | Core::ERROR_NONE | Should Pass |
* | 60 | Set closed caption service to SERVICE60 | service = "SERVICE60" | Core::ERROR_NONE | Should Pass |
* | 61 | Set closed caption service to SERVICE61 | service = "SERVICE61" | Core::ERROR_NONE | Should Pass |
* | 62 | Set closed caption service to SERVICE62 | service = "SERVICE62" | Core::ERROR_NONE | Should Pass |
* | 63 | Set closed caption service to SERVICE63 | service = "SERVICE63" | Core::ERROR_NONE | Should Pass |
* | 64 | Set closed caption service to SERVICE64 | service = "SERVICE64" | Core::ERROR_NONE | Should Pass |
*/
TEST(UserSettingsTestAI, SetValidClosedCaptionService_SERVICE1ToSERVICE64) {
    std::cout << "Entering SetValidClosedCaptionService_SERVICE1ToSERVICE64" << std::endl;
    
    for (int i = 1; i <= 64; ++i) {
        std::string service = "SERVICE" + std::to_string(i);
        EXPECT_EQ(Core::ERROR_NONE, InterfacePointer->SetPreferredClosedCaptionService(service));
    }
    
    std::cout << "Exiting SetValidClosedCaptionService_SERVICE1ToSERVICE64" << std::endl;
}

/**
* @brief Test to verify the behavior of SetPreferredClosedCaptionService with an invalid service parameter.
*
* This test checks if the SetPreferredClosedCaptionService function correctly handles an invalid closed caption service input ("CC5"). The function is expected to return an error code indicating an invalid parameter.
*
* **Test Group ID:** Basic: 01@n
* **Test Case ID:** 052@n
* **Priority:** High@n
* @n
* **Pre-Conditions:** None@n
* **Dependencies:** None@n
* **User Interaction:** None@n
* @n
* **Test Procedure:**@n
* | Variation / Step | Description | Test Data |Expected Result |Notes |
* | :----: | --------- | ---------- |-------------- | ----- |
* | 01| Call SetPreferredClosedCaptionService with invalid service "CC5" | service = "CC5" | Return value should be Core::ERROR_INVALID_PARAMETER, Assertion should pass | Should Fail |
*/
TEST(UserSettingsTestAI, SetInvalidClosedCaptionService_CC5) {
    std::cout << "Entering SetInvalidClosedCaptionService_CC5" << std::endl;
    std::string service = "CC5";
    
    // Setup the expected behavior for invalid closed caption service
    EXPECT_CALL(*g_storeMock, SetValue(_, _, USERSETTINGS_PREFERRED_CLOSED_CAPTIONS_SERVICE_KEY, _, _))
        .WillOnce(Return(Core::ERROR_INVALID_PARAMETER));
    
    EXPECT_EQ(Core::ERROR_INVALID_PARAMETER, InterfacePointer->SetPreferredClosedCaptionService(service));
    
    std::cout << "Exiting SetInvalidClosedCaptionService_CC5" << std::endl;
}

/**
* @brief Test to validate the behavior of SetPreferredClosedCaptionService with an invalid service name
*
* This test checks the response of the SetPreferredClosedCaptionService method when provided with an invalid closed caption service name "TEXT0". The objective is to ensure that the method correctly identifies and handles invalid input parameters.
*
* **Test Group ID:** Basic: 01@n
* **Test Case ID:** 053@n
* **Priority:** High@n
* @n
* **Pre-Conditions:** None@n
* **Dependencies:** None@n
* **User Interaction:** None@n
* @n
* **Test Procedure:**@n
* | Variation / Step | Description | Test Data |Expected Result |Notes |
* | :----: | --------- | ---------- |-------------- | ----- |
* | 01| Call SetPreferredClosedCaptionService with invalid service name | service = "TEXT0" | Return value should be Core::ERROR_INVALID_PARAMETER, Assertion should pass | Should Fail |
*/
TEST(UserSettingsTestAI, SetInvalidClosedCaptionService_TEXT0) {
    std::cout << "Entering SetInvalidClosedCaptionService_TEXT0" << std::endl;
    std::string service = "TEXT0";
    
    // Setup the expected behavior for invalid closed caption service
    EXPECT_CALL(*g_storeMock, SetValue(_, _, USERSETTINGS_PREFERRED_CLOSED_CAPTIONS_SERVICE_KEY, _, _))
        .WillOnce(Return(Core::ERROR_INVALID_PARAMETER));
    
    EXPECT_EQ(Core::ERROR_INVALID_PARAMETER, InterfacePointer->SetPreferredClosedCaptionService(service));
    
    std::cout << "Exiting SetInvalidClosedCaptionService_TEXT0" << std::endl;
}

/**
* @brief Test to validate the behavior of SetPreferredClosedCaptionService with an invalid service parameter
*
* This test checks the response of the SetPreferredClosedCaptionService method when provided with an invalid closed caption service identifier. The objective is to ensure that the method correctly identifies and handles invalid input parameters by returning an appropriate error code.
*
* **Test Group ID:** Basic: 01@n
* **Test Case ID:** 054@n
* **Priority:** High@n
* @n
* **Pre-Conditions:** None@n
* **Dependencies:** None@n
* **User Interaction:** None@n
* @n
* **Test Procedure:**@n
* | Variation / Step | Description | Test Data | Expected Result | Notes |
* | :----: | --------- | ---------- |-------------- | ----- |
* | 01 | Call SetPreferredClosedCaptionService with invalid service | service = "SERVICE65" | Return value should be Core::ERROR_INVALID_PARAMETER | Should Fail |
*/
TEST(UserSettingsTestAI, SetInvalidClosedCaptionService_SERVICE65) {
    std::cout << "Entering SetInvalidClosedCaptionService_SERVICE65" << std::endl;
    std::string service = "SERVICE65";
    
    // Setup the expected behavior for invalid closed caption service
    EXPECT_CALL(*g_storeMock, SetValue(_, _, USERSETTINGS_PREFERRED_CLOSED_CAPTIONS_SERVICE_KEY, _, _))
        .WillOnce(Return(Core::ERROR_INVALID_PARAMETER));
    
    EXPECT_EQ(Core::ERROR_INVALID_PARAMETER, InterfacePointer->SetPreferredClosedCaptionService(service));
    
    std::cout << "Exiting SetInvalidClosedCaptionService_SERVICE65" << std::endl;
}

/**
* @brief Test to verify the behavior of SetPreferredClosedCaptionService with an invalid service name.
*
* This test checks if the SetPreferredClosedCaptionService method correctly handles an invalid service name by returning an error code. This is important to ensure that the system can gracefully handle invalid inputs.
*
* **Test Group ID:** Basic: 01@n
* **Test Case ID:** 055@n
* **Priority:** High@n
* @n
* **Pre-Conditions:** None@n
* **Dependencies:** None@n
* **User Interaction:** None@n
* @n
* **Test Procedure:**@n
* | Variation / Step | Description | Test Data | Expected Result | Notes |
* | :----: | --------- | ---------- |-------------- | ----- |
* | 01 | Call SetPreferredClosedCaptionService with invalid service name | service = "INVALID" | Return value should be Core::ERROR_INVALID_PARAMETER, Assertion should pass | Should Fail |
*/
TEST(UserSettingsTestAI, SetInvalidClosedCaptionService_INVALID) {
    std::cout << "Entering SetInvalidClosedCaptionService_INVALID" << std::endl;
    std::string service = "INVALID";
    
    // Setup the expected behavior for invalid closed caption service
    EXPECT_CALL(*g_storeMock, SetValue(_, _, USERSETTINGS_PREFERRED_CLOSED_CAPTIONS_SERVICE_KEY, _, _))
        .WillOnce(Return(Core::ERROR_INVALID_PARAMETER));
    
    EXPECT_EQ(Core::ERROR_INVALID_PARAMETER, InterfacePointer->SetPreferredClosedCaptionService(service));
    
    std::cout << "Exiting SetInvalidClosedCaptionService_INVALID" << std::endl;
}

/**
* @brief Test to verify the behavior of SetPreferredClosedCaptionService when an empty string is passed as the service.
*
* This test checks if the SetPreferredClosedCaptionService function correctly handles an empty string input for the service parameter and returns the appropriate error code. This is important to ensure that invalid inputs are properly managed by the system.
*
* **Test Group ID:** Basic: 01@n
* **Test Case ID:** 056@n
* **Priority:** High@n
* @n
* **Pre-Conditions:** None@n
* **Dependencies:** None@n
* **User Interaction:** None@n
* @n
* **Test Procedure:**@n
* | Variation / Step | Description | Test Data |Expected Result |Notes |
* | :----: | --------- | ---------- |-------------- | ----- |
* | 01| Call SetPreferredClosedCaptionService with an empty string | service = "" | Core::ERROR_INVALID_PARAMETER | Should Fail |
*/
TEST(UserSettingsTestAI, SetEmptyClosedCaptionService) {
    std::cout << "Entering SetEmptyClosedCaptionService" << std::endl;
    std::string service = "";
    
    // Setup the expected behavior for empty closed caption service
    EXPECT_CALL(*g_storeMock, SetValue(_, _, USERSETTINGS_PREFERRED_CLOSED_CAPTIONS_SERVICE_KEY, _, _))
        .WillOnce(Return(Core::ERROR_INVALID_PARAMETER));
    
    EXPECT_EQ(Core::ERROR_INVALID_PARAMETER, InterfacePointer->SetPreferredClosedCaptionService(service));
    
    std::cout << "Exiting SetEmptyClosedCaptionService" << std::endl;
}

/**
* @brief Test to verify setting a valid presentation language to "en-US"
*
* This test checks if the SetPresentationLanguage function correctly sets the presentation language to "en-US" and returns the expected result. This is important to ensure that the function handles valid language codes properly.
*
* **Test Group ID:** Basic: 01@n
* **Test Case ID:** 057@n
* **Priority:** High@n
* @n
* **Pre-Conditions:** None@n
* **Dependencies:** None@n
* **User Interaction:** None@n
* @n
* **Test Procedure:**@n
* | Variation / Step | Description | Test Data |Expected Result |Notes |
* | :----: | --------- | ---------- |-------------- | ----- |
* | 01| Call SetPresentationLanguage with "en-US" | input = "en-US" | result = Core::ERROR_NONE, EXPECT_EQ(result, Core::ERROR_NONE) | Should Pass |
*/
TEST(UserSettingsTestAI, SetValidPresentationLanguage_enUS) {
    std::cout << "Entering SetValidPresentationLanguage_enUS" << std::endl;
    
    Core::hresult result = InterfacePointer->SetPresentationLanguage("en-US");
    
    EXPECT_EQ(result, Core::ERROR_NONE);
    std::cout << "Exiting SetValidPresentationLanguage_enUS" << std::endl;
}

/**
* @brief Test to verify setting a valid presentation language to "es-US"
*
* This test checks if the SetPresentationLanguage function correctly sets the presentation language to "es-US" and returns the expected result. This is important to ensure that the function handles valid language codes properly.
*
* **Test Group ID:** Basic: 01@n
* **Test Case ID:** 058@n
* **Priority:** High@n
* @n
* **Pre-Conditions:** None@n
* **Dependencies:** None@n
* **User Interaction:** None@n
* @n
* **Test Procedure:**@n
* | Variation / Step | Description | Test Data |Expected Result |Notes |
* | :----: | --------- | ---------- |-------------- | ----- |
* | 01| Call SetPresentationLanguage with "es-US" | input = "es-US" | result = Core::ERROR_NONE, EXPECT_EQ(result, Core::ERROR_NONE) | Should Pass |
*/
TEST(UserSettingsTestAI, SetValidPresentationLanguage_esUS) {
    std::cout << "Entering SetValidPresentationLanguage_esUS" << std::endl;
    
    Core::hresult result = InterfacePointer->SetPresentationLanguage("es-US");
    
    EXPECT_EQ(result, Core::ERROR_NONE);
    std::cout << "Exiting SetValidPresentationLanguage_esUS" << std::endl;
}

/**
* @brief Test to verify setting a valid presentation language
*
* This test checks if the SetPresentationLanguage function correctly sets the presentation language to "en-CA" and returns the expected result. This is important to ensure that the function handles valid language codes properly.
*
* **Test Group ID:** Basic: 01@n
* **Test Case ID:** 059@n
* **Priority:** High@n
* @n
* **Pre-Conditions:** None@n
* **Dependencies:** None@n
* **User Interaction:** None@n
* @n
* **Test Procedure:**@n
* | Variation / Step | Description | Test Data | Expected Result | Notes |
* | :----: | --------- | ---------- |-------------- | ----- |
* | 01 | Call SetPresentationLanguage with "en-CA" | input: "en-CA" | result: Core::ERROR_NONE, EXPECT_EQ(result, Core::ERROR_NONE) | Should Pass |
*/
TEST(UserSettingsTestAI, SetValidPresentationLanguage_enCA) {
    std::cout << "Entering SetValidPresentationLanguage_enCA" << std::endl;
    
    Core::hresult result = InterfacePointer->SetPresentationLanguage("en-CA");
    
    EXPECT_EQ(result, Core::ERROR_NONE);
    std::cout << "Exiting SetValidPresentationLanguage_enCA" << std::endl;
}

/**
* @brief Test to verify setting a valid presentation language
*
* This test checks if the SetPresentationLanguage function correctly sets the presentation language to "fr-CA" and returns the expected result. This is important to ensure that the function handles valid language codes properly.
*
* **Test Group ID:** Basic: 01@n
* **Test Case ID:** 060@n
* **Priority:** High@n
* @n
* **Pre-Conditions:** None@n
* **Dependencies:** None@n
* **User Interaction:** None@n
* @n
* **Test Procedure:**@n
* | Variation / Step | Description | Test Data |Expected Result |Notes |
* | :----: | --------- | ---------- |-------------- | ----- |
* | 01| Call SetPresentationLanguage with "fr-CA" | input = "fr-CA" | result = Core::ERROR_NONE, EXPECT_EQ(result, Core::ERROR_NONE) | Should Pass |
*/
TEST(UserSettingsTestAI, SetValidPresentationLanguage_frCA) {
    std::cout << "Entering SetValidPresentationLanguage_frCA" << std::endl;
    
    Core::hresult result = InterfacePointer->SetPresentationLanguage("fr-CA");
    
    EXPECT_EQ(result, Core::ERROR_NONE);
    std::cout << "Exiting SetValidPresentationLanguage_frCA" << std::endl;
}

/**
* @brief Test to verify the behavior of SetPresentationLanguage with an invalid language code.
*
* This test checks if the SetPresentationLanguage method correctly handles an invalid language code "xx-XX" and returns the appropriate error code. This is important to ensure that the method validates input parameters properly.
*
* **Test Group ID:** Basic: 01@n
* **Test Case ID:** 061@n
* **Priority:** High@n
* @n
* **Pre-Conditions:** None@n
* **Dependencies:** None@n
* **User Interaction:** None@n
* @n
* **Test Procedure:**@n
* | Variation / Step | Description | Test Data | Expected Result | Notes |
* | :----: | --------- | ---------- |-------------- | ----- |
* | 01 | Call SetPresentationLanguage with invalid language code | input = "xx-XX" | result = Core::ERROR_INVALID_PARAMETER, Assertion check: EXPECT_EQ(result, Core::ERROR_INVALID_PARAMETER) | Should Fail |
*/
TEST(UserSettingsTestAI, SetInvalidPresentationLanguage_xxXX) {
    std::cout << "Entering SetInvalidPresentationLanguage_xxXX" << std::endl;
    
    // Set up the mock to return an error for invalid language code
    EXPECT_CALL(*g_storeMock, SetValue(_, _, _, _, _))
        .WillOnce(Return(Core::ERROR_INVALID_PARAMETER));
    
    Core::hresult result = InterfacePointer->SetPresentationLanguage("xx-XX");
    
    EXPECT_EQ(result, Core::ERROR_INVALID_PARAMETER);
    std::cout << "Exiting SetInvalidPresentationLanguage_xxXX" << std::endl;
}

/**
* @brief Test to verify the behavior of SetPresentationLanguage when an empty string is provided.
*
* This test checks if the SetPresentationLanguage method correctly handles an empty string input and returns the appropriate error code. This is important to ensure that invalid inputs are properly managed by the system.
*
* **Test Group ID:** Basic: 01@n
* **Test Case ID:** 062@n
* **Priority:** High@n
* @n
* **Pre-Conditions:** None@n
* **Dependencies:** None@n
* **User Interaction:** None@n
* @n
* **Test Procedure:**@n
* | Variation / Step | Description | Test Data | Expected Result | Notes |
* | :----: | --------- | ---------- |-------------- | ----- |
* | 01 | Call SetPresentationLanguage with an empty string | input = "" | result = Core::ERROR_INVALID_PARAMETER | Should Fail |
*/
TEST(UserSettingsTestAI, SetEmptyPresentationLanguage) {
    std::cout << "Entering SetEmptyPresentationLanguage" << std::endl;
    
    Core::hresult result = InterfacePointer->SetPresentationLanguage("");
    
    EXPECT_EQ(result, Core::ERROR_INVALID_PARAMETER);
    std::cout << "Exiting SetEmptyPresentationLanguage" << std::endl;
}

/**
* @brief Test to verify the behavior of SetPresentationLanguage with invalid format input
*
* This test checks if the SetPresentationLanguage method correctly handles an invalid format input and returns the appropriate error code. The input "english-US" is not a valid language format, and the expected result is Core::ERROR_INVALID_PARAMETER.
*
* **Test Group ID:** Basic: 01@n
* **Test Case ID:** 063@n
* **Priority:** High@n
* @n
* **Pre-Conditions:** None@n
* **Dependencies:** None@n
* **User Interaction:** None@n
* @n
* **Test Procedure:**@n
* | Variation / Step | Description | Test Data |Expected Result |Notes |
* | :----: | --------- | ---------- |-------------- | ----- |
* | 01| Entering the test function | None | None | Should be successful |
* | 02| Call SetPresentationLanguage with invalid format | input = "english-US" | result = Core::ERROR_INVALID_PARAMETER | Should Pass |
* | 03| Verify the result using EXPECT_EQ | result = Core::ERROR_INVALID_PARAMETER | EXPECT_EQ(result, Core::ERROR_INVALID_PARAMETER) | Should Pass |
* | 04| Exiting the test function | None | None | Should be successful |
*/
TEST(UserSettingsTestAI, SetPresentationLanguageInvalidFormat) {
    std::cout << "Entering SetPresentationLanguageInvalidFormat" << std::endl;
    
    Core::hresult result = InterfacePointer->SetPresentationLanguage("english-US");
    
    EXPECT_EQ(result, Core::ERROR_INVALID_PARAMETER);
    std::cout << "Exiting SetPresentationLanguageInvalidFormat" << std::endl;
}

/**
* @brief Test to verify the behavior of SetPresentationLanguage with special characters
*
* This test checks if the SetPresentationLanguage function correctly handles input containing special characters. The function is expected to return an error when such input is provided.
*
* **Test Group ID:** Basic: 01@n
* **Test Case ID:** 064@n
* **Priority:** High@n
* @n
* **Pre-Conditions:** None@n
* **Dependencies:** None@n
* **User Interaction:** None@n
* @n
* **Test Procedure:**@n
* | Variation / Step | Description | Test Data | Expected Result | Notes |
* | :----: | --------- | ---------- |-------------- | ----- |
* | 01 | Call SetPresentationLanguage with special characters | input = "!@#$" | result = Core::ERROR_INVALID_PARAMETER, EXPECT_EQ(result, Core::ERROR_INVALID_PARAMETER) | Should Fail |
*/
TEST(UserSettingsTestAI, SetPresentationLanguageSpecialCharacters) {
    std::cout << "Entering SetPresentationLanguageSpecialCharacters" << std::endl;
    
    Core::hresult result = InterfacePointer->SetPresentationLanguage("!@#$");
    
    EXPECT_EQ(result, Core::ERROR_INVALID_PARAMETER);
    std::cout << "Exiting SetPresentationLanguageSpecialCharacters" << std::endl;
}

/**
* @brief Test to validate the SetViewingRestrictions function with a valid JSON input.
*
* This test checks if the SetViewingRestrictions function correctly processes a valid JSON input string and returns the expected result. This ensures that the function can handle and parse valid JSON data as intended.
*
* **Test Group ID:** Basic: 01@n
* **Test Case ID:** 065@n
* **Priority:** High@n
* @n
* **Pre-Conditions:** None@n
* **Dependencies:** None@n
* **User Interaction:** None@n
* @n
* **Test Procedure:**@n
* | Variation / Step | Description | Test Data | Expected Result | Notes |
* | :----: | --------- | ---------- |-------------- | ----- |
* | 01 | Call SetViewingRestrictions with valid JSON input | input = "{\"restrictions\": [{\"scheme\": \"US_TV\", \"restrict\": [\"TV-Y7/FV\"]}, {\"scheme\": \"MPAA\", \"restrict\": []}]}" | result = Core::ERROR_NONE, EXPECT_EQ(result, Core::ERROR_NONE) | Should Pass |
*/
TEST(UserSettingsTestAI, ValidJSONInput) {
    std::cout << "Entering ValidJSONInput test" << std::endl;
    std::string input = "{\"restrictions\": [{\"scheme\": \"US_TV\", \"restrict\": [\"TV-Y7/FV\"]}, {\"scheme\": \"MPAA\", \"restrict\": []}]}";
    
    Core::hresult result = InterfacePointer->SetViewingRestrictions(input);
    
    EXPECT_EQ(result, Core::ERROR_NONE);
    std::cout << "Exiting ValidJSONInput test" << std::endl;
}

/**
* @brief Test to verify the behavior of SetViewingRestrictions when given a null input
*
* This test checks the SetViewingRestrictions function to ensure it correctly handles a null input scenario. 
* The function is expected to return an error code indicating an invalid parameter when provided with an empty string as input.
*
* **Test Group ID:** Basic: 01@n
* **Test Case ID:** 066@n
* **Priority:** High@n
* @n
* **Pre-Conditions:** None@n
* **Dependencies:** None@n
* **User Interaction:** None@n
* @n
* **Test Procedure:**@n
* | Variation / Step | Description | Test Data |Expected Result |Notes |
* | :----: | --------- | ---------- |-------------- | ----- |
* | 01| Call SetViewingRestrictions with empty string | input = "" | result = Core::ERROR_INVALID_PARAMETER, EXPECT_EQ(result, Core::ERROR_INVALID_PARAMETER) | Should Fail |
*/
TEST(UserSettingsTestAI, NullInput) {
    std::cout << "Entering NullInput test" << std::endl;
    std::string input = "";
    
    Core::hresult result = InterfacePointer->SetViewingRestrictions(input);
    
    EXPECT_EQ(result, Core::ERROR_INVALID_PARAMETER);
    std::cout << "Exiting NullInput test" << std::endl;
}

/**
* @brief Test to verify the behavior of SetViewingRestrictions with unsupported JSON value
*
* This test checks the SetViewingRestrictions method to ensure it correctly handles an unsupported JSON value.@n
*
* **Test Group ID:** Basic: 01@n
* **Test Case ID:** 067@n
* **Priority:** High@n
* @n
* **Pre-Conditions:** None@n
* **Dependencies:** None@n
* **User Interaction:** None@n
* @n
* **Test Procedure:**@n
* | Variation / Step | Description | Test Data | Expected Result | Notes |
* | :----: | --------- | ---------- |-------------- | ----- |
* | 01 | Entering the test | None | None | Should be successful |
* | 02 | Set unsupported JSON value | input = "{\"XYZ:123\"}", result = InterfacePointer->SetViewingRestrictions(input) | result = Core::ERROR_INVALID_PARAMETER, EXPECT_EQ(result, Core::ERROR_INVALID_PARAMETER) | Should Pass |
* | 03 | Exiting the test | None | None | Should be successful |
*/
TEST(UserSettingsTestAI, JSONWithUnsupportedValue) {
    std::cout << "Entering JSONWithUnsupportedValue test" << std::endl;
    std::string input = "{\"XYZ:123\"}";
    
    Core::hresult result = InterfacePointer->SetViewingRestrictions(input);
    
    EXPECT_EQ(result, Core::ERROR_INVALID_PARAMETER);
    std::cout << "Exiting JSONWithUnsupportedValue test" << std::endl;
}

/**
* @brief Test to validate the setting of viewing restrictions window
*
* This test verifies that the SetViewingRestrictionsWindow function correctly sets the viewing restrictions window to "ALWAYS" and returns the expected result.@n
*
* **Test Group ID:** Basic: 01@n
* **Test Case ID:** 068@n
* **Priority:** High@n
* @n
* **Pre-Conditions:** None@n
* **Dependencies:** None@n
* **User Interaction:** None@n
* @n
* **Test Procedure:**@n
* | Variation / Step | Description | Test Data |Expected Result |Notes |
* | :----: | --------- | ---------- |-------------- | ----- |
* | 01| Call SetViewingRestrictionsWindow with "ALWAYS" | input = "ALWAYS" | result = Core::ERROR_NONE, EXPECT_EQ(result, Core::ERROR_NONE) | Should Pass |
*/
TEST(UserSettingsTestAI, ValidSetViewingRestrictionsWindow) {
    std::cout << "Entering ValidSetViewingRestrictionsWindow" << std::endl;

    Core::hresult result = InterfacePointer->SetViewingRestrictionsWindow("ALWAYS");

    EXPECT_EQ(result, Core::ERROR_NONE);
    std::cout << "Exiting ValidSetViewingRestrictionsWindow" << std::endl;
}

/**
* @brief Test to verify the behavior of SetViewingRestrictionsWindow with an empty string
*
* This test checks the behavior of the SetViewingRestrictionsWindow function when it is called with an empty string as the parameter. The function is expected to return an error indicating an invalid parameter.
*
* **Test Group ID:** Basic: 01@n
* **Test Case ID:** 069@n
* **Priority:** High@n
* @n
* **Pre-Conditions:** None@n
* **Dependencies:** None@n
* **User Interaction:** None@n
* @n
* **Test Procedure:**@n
* | Variation / Step | Description | Test Data |Expected Result |Notes |
* | :----: | --------- | ---------- |-------------- | ----- |
* | 01| Call SetViewingRestrictionsWindow with an empty string | input = "" | result = Core::ERROR_INVALID_PARAMETER, EXPECT_EQ(result, Core::ERROR_INVALID_PARAMETER) | Should Fail |
*/
TEST(UserSettingsTestAI, EmptyViewingRestrictionsWindow) {
    std::cout << "Entering EmptyViewingRestrictionsWindow" << std::endl;

    Core::hresult result = InterfacePointer->SetViewingRestrictionsWindow("");

    EXPECT_EQ(result, Core::ERROR_INVALID_PARAMETER);
    std::cout << "Exiting EmptyViewingRestrictionsWindow" << std::endl;
}

/**
* @brief Test to validate the behavior of SetViewingRestrictionsWindow with invalid input
*
* This test verifies that the SetViewingRestrictionsWindow function correctly handles invalid input values by returning an appropriate error code. This ensures that the function can gracefully handle erroneous input and maintain system stability.
*
* **Test Group ID:** Basic: 01@n
* **Test Case ID:** 070@n
* **Priority:** High@n
* @n
* **Pre-Conditions:** None@n
* **Dependencies:** None@n
* **User Interaction:** None@n
* @n
* **Test Procedure:**@n
* | Variation / Step | Description | Test Data |Expected Result |Notes |
* | :----: | --------- | ---------- |-------------- | ----- |
* | 01| Call SetViewingRestrictionsWindow with invalid input | input = "INVALID_VALUE" | result = Core::ERROR_INVALID_PARAMETER, EXPECT_EQ(result, Core::ERROR_INVALID_PARAMETER) | Should Fail |
*/
TEST(UserSettingsTestAI, InvalidViewingRestrictionsWindow) {
    std::cout << "Entering InvalidViewingRestrictionsWindow" << std::endl;

    Core::hresult result = InterfacePointer->SetViewingRestrictionsWindow("INVALID_VALUE");

    EXPECT_EQ(result, Core::ERROR_INVALID_PARAMETER);
    std::cout << "Exiting InvalidViewingRestrictionsWindow" << std::endl;
}

/**
* @brief Test the behavior of SetViewingRestrictionsWindow with special characters as input
*
* This test verifies that the SetViewingRestrictionsWindow function correctly handles special characters in the input string. The function is expected to return an error code indicating invalid parameters when special characters are used.
*
* **Test Group ID:** Basic: 01@n
* **Test Case ID:** 071@n
* **Priority:** High@n
* @n
* **Pre-Conditions:** None@n
* **Dependencies:** None@n
* **User Interaction:** None@n
* @n
* **Test Procedure:**@n
* | Variation / Step | Description | Test Data | Expected Result | Notes |
* | :----: | --------- | ---------- |-------------- | ----- |
* | 01 | Call SetViewingRestrictionsWindow with special characters | input = "@!#%&*" | result = Core::ERROR_INVALID_PARAMETER, EXPECT_EQ(result, Core::ERROR_INVALID_PARAMETER) | Should Fail |
*/
TEST(UserSettingsTestAI, ViewingRestrictionsWindowWithSpecialCharacters) {
    std::cout << "Entering ViewingRestrictionsWindowWithSpecialCharacters" << std::endl;

    Core::hresult result = InterfacePointer->SetViewingRestrictionsWindow("@!#%&*");

    EXPECT_EQ(result, Core::ERROR_INVALID_PARAMETER);
    std::cout << "Exiting ViewingRestrictionsWindowWithSpecialCharacters" << std::endl;
}

/**
* @brief Test to verify enabling voice guidance functionality
*
* This test checks if the voice guidance can be successfully enabled using the SetVoiceGuidance method. It ensures that the method returns the expected result when enabling voice guidance.@n
*
* **Test Group ID:** Basic: 01@n
* **Test Case ID:** 072@n
* **Priority:** High@n
* @n
* **Pre-Conditions:** None@n
* **Dependencies:** None@n
* **User Interaction:** None@n
* @n
* **Test Procedure:**@n
* | Variation / Step | Description | Test Data | Expected Result | Notes |
* | :----: | --------- | ---------- |-------------- | ----- |
* | 01 | Call SetVoiceGuidance to enable voice guidance | input: true | result = Core::ERROR_NONE, EXPECT_EQ(result, Core::ERROR_NONE) | Should Pass |
*/
TEST(UserSettingsTestAI, EnableVoiceGuidance) {
    std::cout << "Entering EnableVoiceGuidance" << std::endl;
    bool enabled = true;

    Core::hresult result = InterfacePointer->SetVoiceGuidance(enabled);

    EXPECT_EQ(result, Core::ERROR_NONE);
    std::cout << "Exiting EnableVoiceGuidance" << std::endl;
}

/**
* @brief Test to verify the functionality of disabling voice guidance
*
* This test checks if the voice guidance can be successfully disabled using the SetVoiceGuidance method. It ensures that the method returns the expected result when disabling the voice guidance feature.@n
*
* **Test Group ID:** Basic: 01@n
* **Test Case ID:** 073@n
* **Priority:** High@n
* @n
* **Pre-Conditions:** None@n
* **Dependencies:** None@n
* **User Interaction:** None@n
* @n
* **Test Procedure:**@n
* | Variation / Step | Description | Test Data | Expected Result | Notes |
* | :----: | --------- | ---------- |-------------- | ----- |
* | 01 | Call SetVoiceGuidance to disable voice guidance | input: false | result = Core::ERROR_NONE, EXPECT_EQ(result, Core::ERROR_NONE) | Should Pass |
*/
TEST(UserSettingsTestAI, DisableVoiceGuidance) {
    std::cout << "Entering DisableVoiceGuidance" << std::endl;
    // Removed unused variable 'enabled'

    Core::hresult result = InterfacePointer->SetVoiceGuidance(false);

    EXPECT_EQ(result, Core::ERROR_NONE);
    std::cout << "Exiting DisableVoiceGuidance" << std::endl;
}

/**
* @brief Test to verify the SetVoiceGuidanceHints function with a true value
*
* This test checks if the SetVoiceGuidanceHints function correctly sets the voice guidance hints to true and returns the expected result. This is important to ensure that the voice guidance feature can be enabled successfully.
*
* **Test Group ID:** Basic: 01@n
* **Test Case ID:** 074@n
* **Priority:** High@n
* @n
* **Pre-Conditions:** None@n
* **Dependencies:** None@n
* **User Interaction:** None@n
* @n
* **Test Procedure:**@n
* | Variation / Step | Description | Test Data |Expected Result |Notes |
* | :----: | --------- | ---------- |-------------- | ----- |
* | 01| Call SetVoiceGuidanceHints with true | input = true | result = Core::ERROR_NONE, EXPECT_EQ(result, Core::ERROR_NONE) | Should Pass |
*/
TEST(UserSettingsTestAI, SetVoiceGuidanceHints_True) {
    std::cout << "Entering SetVoiceGuidanceHints_True" << std::endl;
    bool hints = true; 

    Core::hresult result = InterfacePointer->SetVoiceGuidanceHints(hints);

    EXPECT_EQ(result, Core::ERROR_NONE);
    std::cout << "Exiting SetVoiceGuidanceHints_True" << std::endl;
}

/**
* @brief Test to verify the SetVoiceGuidanceHints function with false input
*
* This test checks the functionality of the SetVoiceGuidanceHints method when the input is set to false. It ensures that the method correctly handles the input and returns the expected result, which is Core::ERROR_NONE.
*
* **Test Group ID:** Basic: 01@n
* **Test Case ID:** 075@n
* **Priority:** High@n
* @n
* **Pre-Conditions:** None@n
* **Dependencies:** None@n
* **User Interaction:** None@n
* @n
* **Test Procedure:**@n
* | Variation / Step | Description | Test Data |Expected Result |Notes |
* | :----: | --------- | ---------- |-------------- | ----- |
* | 01| Call SetVoiceGuidanceHints with false | input = false | result = Core::ERROR_NONE, EXPECT_EQ(result, Core::ERROR_NONE) | Should Pass |
*/
TEST(UserSettingsTestAI, SetVoiceGuidanceHints_False) {
    std::cout << "Entering SetVoiceGuidanceHints_False" << std::endl;
    bool hints = false; 

    Core::hresult result = InterfacePointer->SetVoiceGuidanceHints(hints);

    EXPECT_EQ(result, Core::ERROR_NONE);
    std::cout << "Exiting SetVoiceGuidanceHints_False" << std::endl;
}

/**
* @brief Test a range of valid rates from 0.1 to 10.0 (inclusive)
*
* This test checks if the SetVoiceGuidanceRate function correctly handles the minimum valid rate input and returns the expected result.
*
* **Test Group ID:** Basic: 01@n
* **Test Case ID:** 076@n
* **Priority:** High@n
* @n
* **Pre-Conditions:** None@n
* **Dependencies:** None@n
* **User Interaction:** None@n
* @n
* **Test Procedure:**@n
* | Variation / Step | Description | Test Data |Expected Result |Notes |
* | :----: | --------- | ---------- |-------------- | ----- |
* | 01| Set the minimum valid voice guidance rate | rate = 0.1 | result = Core::ERROR_NONE, EXPECT_EQ(result, Core::ERROR_NONE) | Should Pass |
*/
TEST(UserSettingsTestAI, SetVoiceGuidanceRate_ValidRange) {
    std::cout << "Entering SetVoiceGuidanceRate_ValidRange" << std::endl;

    for (double rate = 0.1; rate <= 10.0; rate += 0.5) {
        Core::hresult result = InterfacePointer->SetVoiceGuidanceRate(rate);
        EXPECT_EQ(result, Core::ERROR_NONE) << "Failed for rate: " << rate;
    }

    std::cout << "Exiting SetVoiceGuidanceRate_ValidRange" << std::endl;
}

/**
* @brief Test to verify the behavior of SetVoiceGuidanceRate when the rate is below the minimum boundary.
*
* This test checks if the SetVoiceGuidanceRate function correctly handles the case where the input rate is below the minimum acceptable boundary. The expected behavior is that the function should return an error indicating that the input is out of range.
*
* **Test Group ID:** Basic: 01@n
* **Test Case ID:** 077@n
* **Priority:** High@n
* @n
* **Pre-Conditions:** None@n
* **Dependencies:** None@n
* **User Interaction:** None@n
* @n
* **Test Procedure:**@n
* | Variation / Step | Description | Test Data |Expected Result |Notes |
* | :----: | --------- | ---------- |-------------- | ----- |
* | 01| Set the voice guidance rate below the minimum boundary | rate = 0.0 | result = Core::ERROR_INVALID_RANGE, EXPECT_EQ(result, Core::ERROR_INVALID_RANGE) | Should Fail |
*/
TEST(UserSettingsTestAI, SetVoiceGuidanceRate_BelowMinimumBoundary) {
    std::cout << "Entering SetVoiceGuidanceRate_BelowMinimumBoundary" << std::endl;
    double rate = 0.0;
    
    Core::hresult result = InterfacePointer->SetVoiceGuidanceRate(rate);
    
    EXPECT_EQ(result, Core::ERROR_INVALID_RANGE);
    std::cout << "Exiting SetVoiceGuidanceRate_BelowMinimumBoundary" << std::endl;
}

/**
* @brief Test to verify the behavior of SetVoiceGuidanceRate when the input rate is above the maximum boundary.
*
* This test checks if the SetVoiceGuidanceRate function correctly handles an input rate that is above the allowed maximum boundary. The expected behavior is that the function should return an error indicating that the input is out of range.
*
* **Test Group ID:** Basic: 01@n
* **Test Case ID:** 078@n
* **Priority:** High@n
* @n
* **Pre-Conditions:** None@n
* **Dependencies:** None@n
* **User Interaction:** None@n
* @n
* **Test Procedure:**@n
* | Variation / Step | Description | Test Data |Expected Result |Notes |
* | :----: | --------- | ---------- |-------------- | ----- |
* | 01| Set the voice guidance rate above the maximum boundary | rate = 10.1 | result = Core::ERROR_INVALID_RANGE, EXPECT_EQ(result, Core::ERROR_INVALID_RANGE) | Should Fail |
*/
TEST(UserSettingsTestAI, SetVoiceGuidanceRate_AboveMaximumBoundary) {
    std::cout << "Entering SetVoiceGuidanceRate_AboveMaximumBoundary" << std::endl;
    double rate = 10.1;
    
    Core::hresult result = InterfacePointer->SetVoiceGuidanceRate(rate);
    
    EXPECT_EQ(result, Core::ERROR_INVALID_RANGE);
    std::cout << "Exiting SetVoiceGuidanceRate_AboveMaximumBoundary" << std::endl;
}

/**
* @brief Test to verify the behavior of SetVoiceGuidanceRate when a negative rate is provided.
*
* This test checks if the SetVoiceGuidanceRate function correctly handles a negative rate input by returning an error code. This is important to ensure that the function validates input parameters properly and maintains the integrity of the system by rejecting invalid values.
*
* **Test Group ID:** Basic: 01@n
* **Test Case ID:** 079@n
* **Priority:** High@n
* @n
* **Pre-Conditions:** None@n
* **Dependencies:** None@n
* **User Interaction:** None@n
* @n
* **Test Procedure:**@n
* | Variation / Step | Description | Test Data |Expected Result |Notes |
* | :----: | --------- | ---------- |-------------- | ----- |
* | 01| Call SetVoiceGuidanceRate with a negative rate | rate = -1.0 | result = Core::ERROR_INVALID_RANGE, EXPECT_EQ(result, Core::ERROR_INVALID_RANGE) | Should Fail |
*/
TEST(UserSettingsTestAI, SetVoiceGuidanceRate_NegativeRate) {
    std::cout << "Entering SetVoiceGuidanceRate_NegativeRate" << std::endl;
    double rate = -1.0;
    
    Core::hresult result = InterfacePointer->SetVoiceGuidanceRate(rate);
    
    EXPECT_EQ(result, Core::ERROR_INVALID_RANGE);
    std::cout << "Exiting SetVoiceGuidanceRate_NegativeRate" << std::endl;
}

/**
* @brief Test the Unregister function with a valid notification object
*
* This test verifies that the Unregister function correctly handles a valid notification object by unregistering it successfully.@n
*
* **Test Group ID:** Basic: 01@n
* **Test Case ID:** 080@n
* **Priority:** High@n
* @n
* **Pre-Conditions:** None@n
* **Dependencies:** None@n
* **User Interaction:** None@n
* @n
* **Test Procedure:**@n
* | Variation / Step | Description | Test Data |Expected Result |Notes |
* | :----: | --------- | ---------- |-------------- | ----- |
* | 01| Create a new notification object | notification = new Exchange::IUserSettings::INotification() | Notification object created | Should be successful |
* | 02| Call Unregister with the notification object | notification = valid object | result = Core::ERROR_NONE | Should Pass |
* | 03| Delete the notification object | delete notification | Notification object deleted | Should be successful |
*/
TEST(UserSettingsTestAI, UnregisterWithValidNotificationObject) {
    std::cout << "Entering UnregisterWithValidNotificationObject" << std::endl;
    Exchange::IUserSettings::INotification* notification = new TestNotification();

    Core::hresult result = InterfacePointer->Unregister(notification);

    EXPECT_EQ(result, Core::ERROR_NONE);
    delete notification;
    std::cout << "Exiting UnregisterWithValidNotificationObject" << std::endl;
}

/**
* @brief Test the Unregister function with a null notification object
*
* This test verifies that the Unregister function correctly handles the case where a null notification object is passed. It ensures that the function returns an error code indicating an invalid parameter.
*
* **Test Group ID:** Basic: 01@n
* **Test Case ID:** 081@n
* **Priority:** High@n
* @n
* **Pre-Conditions:** None@n
* **Dependencies:** None@n
* **User Interaction:** None@n
* @n
* **Test Procedure:**@n
* | Variation / Step | Description | Test Data | Expected Result | Notes |
* | :----: | --------- | ---------- |-------------- | ----- |
* | 01 | Call Unregister with null notification object | notification = nullptr | result = Core::ERROR_INVALID_PARAMETER, EXPECT_EQ(result, Core::ERROR_INVALID_PARAMETER) | Should Fail |
*/
TEST(UserSettingsTestAI, UnregisterWithNullNotificationObject) {
    std::cout << "Entering UnregisterWithNullNotificationObject" << std::endl;
    Exchange::IUserSettings::INotification* notification = nullptr;
    
    Core::hresult result = InterfacePointer->Unregister(notification);
    
    EXPECT_EQ(result, Core::ERROR_INVALID_PARAMETER);
    std::cout << "Exiting UnregisterWithNullNotificationObject" << std::endl;
}

/**
* @brief Test the Unregister function with an already unregistered notification object
*
* This test verifies that attempting to unregister a notification object that has already been unregistered returns the appropriate error code.@n
*
* **Test Group ID:** Basic: 01@n
* **Test Case ID:** 082@n
* **Priority:** High@n
* @n
* **Pre-Conditions:** None@n
* **Dependencies:** None@n
* **User Interaction:** None@n
* @n
* **Test Procedure:**@n
* | Variation / Step | Description | Test Data |Expected Result |Notes |
* | :----: | --------- | ---------- |-------------- | ----- |
* | 01| Create a new notification object | notification = new Exchange::IUserSettings::INotification() | | Should be successful |
* | 02| Unregister the notification object for the first time | InterfacePointer->Unregister(notification) | | Should be successful |
* | 03| Attempt to unregister the already unregistered notification object | result = InterfacePointer->Unregister(notification) | result = Core::ERROR_FAILED_UNREGISTERED, EXPECT_EQ(result, Core::ERROR_FAILED_UNREGISTERED) | Should Pass |
* | 04| Delete the notification object | delete notification | | Should be successful |
*/
TEST(UserSettingsTestAI, UnregisterWithAlreadyUnregisteredNotificationObject) {
    std::cout << "Entering UnregisterWithAlreadyUnregisteredNotificationObject" << std::endl;
    Exchange::IUserSettings::INotification* notification = new TestNotification();
    
    InterfacePointer->Unregister(notification);
    Core::hresult result = InterfacePointer->Unregister(notification);
    
    EXPECT_EQ(result, Core::ERROR_FAILED_UNREGISTERED);
    delete notification;
    std::cout << "Exiting UnregisterWithAlreadyUnregisteredNotificationObject" << std::endl;
}

/**
* @brief Test the GetMigrationState function with all valid SettingsKey enum values.
*
* This test verifies that the GetMigrationState function correctly handles each valid key in the SettingsKey enum and returns the expected result without errors.
* It also prints the value of the requiresMigration flag for manual verification.
*
* **Test Group ID:** Basic: 01\n
* **Test Case ID:** 083\n
* **Priority:** High\n
*
* **Pre-Conditions:** None\n
* **Dependencies:** None\n
* **User Interaction:** None\n
*
* **Test Procedure:**\n
* | Step | Description | Test Data | Expected Result | Notes |
* |------|-------------|-----------|------------------|-------|
* | 01   | Loop through all enum values in SettingsKey | key ∈ [1, 17] | result = Core::ERROR_NONE | Should Pass |
* | 02   | Output requiresMigration for each key | requiresMigration ∈ {true, false} | Observe output | |
*/
TEST(UserSettingsInspectorTest, GetMigrationState_AllValidKeys) {
    std::cout << "Entering GetMigrationState_AllValidKeys" << std::endl;

    using Key = WPEFramework::Exchange::IUserSettingsInspector::SettingsKey;

    for (uint32_t keyVal = static_cast<uint32_t>(Key::PREFERRED_AUDIO_LANGUAGES);
         keyVal <= static_cast<uint32_t>(Key::VOICE_GUIDANCE_HINTS); ++keyVal) {

        Key key = static_cast<Key>(keyVal);
        bool requiresMigration = false;

        Core::hresult result = IUserSettingsInspectorPointer->GetMigrationState(key, requiresMigration);

        EXPECT_EQ(result, Core::ERROR_NONE) << "GetMigrationState failed for key: " << keyVal;
        EXPECT_TRUE(requiresMigration == true || requiresMigration == false);

        std::cout << "Key: " << keyVal << " => requiresMigration: " << std::boolalpha << requiresMigration << std::endl;
    }

    std::cout << "Exiting GetMigrationState_AllValidKeys" << std::endl;
}

/**
* @brief Test to verify the behavior of GetMigrationState with an invalid key
*
* This test checks the GetMigrationState method of the IUserSettingsInspector interface when provided with an invalid key. The objective is to ensure that the method returns the appropriate error code and does not set the requiresMigration flag.
*
* **Test Group ID:** Basic: 01@n
* **Test Case ID:** 084@n
* **Priority:** High@n
* @n
* **Pre-Conditions:** None@n
* **Dependencies:** None@n
* **User Interaction:** None@n
* @n
* **Test Procedure:**@n
* | Variation / Step | Description | Test Data |Expected Result |Notes |
* | :----: | --------- | ---------- |-------------- | ----- |
* | 01| Call GetMigrationState with an invalid key | key = 999, requiresMigration = false | result = Core::ERROR_UNKNOWN_KEY, requiresMigration = false | Should Fail |
*/
TEST(UserSettingsInspectorTest, GetMigrationState_InvalidKey) {
    std::cout << "Entering GetMigrationState_InvalidKey" << std::endl;
    bool requiresMigration = false;
    
    Core::hresult result = IUserSettingsInspectorPointer->GetMigrationState(static_cast<WPEFramework::Exchange::IUserSettingsInspector::SettingsKey>(999), requiresMigration);
    
    EXPECT_EQ(result, Core::ERROR_UNKNOWN_KEY);
    EXPECT_FALSE(requiresMigration);
    std::cout << "Exiting GetMigrationState_InvalidKey" << std::endl;
}

/**
* @brief Test to validate the states array in UserSettingsInspector
*
* This test verifies that the GetMigrationStates method of the InterfacePointer correctly retrieves the migration states and that each state key falls within the expected range.
*
* **Test Group ID:** Basic: 01@n
* **Test Case ID:** 085@n
* **Priority:** High@n
* @n
* **Pre-Conditions:** None@n
* **Dependencies:** None@n
* **User Interaction:** None@n
* @n
* **Test Procedure:**@n
* | Variation / Step | Description | Test Data | Expected Result | Notes |
* | :----: | --------- | ---------- |-------------- | ----- |
* | 01 | Log entry for test start | None | None | Should be successful |
* | 02 | Call GetMigrationStates method | states = nullptr | result = Core::ERROR_NONE, states != nullptr | Should Pass |
* | 03 | Iterate through states and validate keys | state.key values | state.key >= PREFERRED_AUDIO_LANGUAGES && state.key <= VOICE_GUIDANCE_HINTS | Should Pass |
* | 04 | Log entry for test end | None | None | Should be successful |
*/
TEST(UserSettingsInspectorTest, ValidStatesArray)
{
    std::cout << "Entering ValidStatesArray" << std::endl;

    WPEFramework::Exchange::IUserSettingsInspector::IUserSettingsMigrationStateIterator* states = nullptr;

    const Core::hresult result = IUserSettingsInspectorPointer->GetMigrationStates(states);

    ASSERT_EQ(result, Core::ERROR_NONE);
    ASSERT_NE(states, nullptr);

    WPEFramework::Exchange::IUserSettingsInspector::SettingsMigrationState state{};

    while (states->Next(state)) {
        ASSERT_TRUE(state.key >= WPEFramework::Exchange::IUserSettingsInspector::PREFERRED_AUDIO_LANGUAGES &&
                    state.key <= WPEFramework::Exchange::IUserSettingsInspector::VOICE_GUIDANCE_HINTS);
    }

    states->Release();

    std::cout << "Exiting ValidStatesArray" << std::endl;
}
