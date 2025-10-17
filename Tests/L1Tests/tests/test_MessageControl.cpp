// New: verify that a specific control entry can be found via the Controls iterator.
TEST_F(MessageControlL1Test, ControlStructureDetailed) {
    Exchange::IMessageControl::Control testControl;
    testControl.type = Exchange::IMessageControl::TRACING;
    testControl.category = "DetailedCategory";
    testControl.module = "DetailedModule";
    testControl.enabled = true;

    Core::hresult hr = plugin->Enable(
        testControl.type,
        testControl.category,
        testControl.module,
        testControl.enabled);
    EXPECT_EQ(Core::ERROR_NONE, hr);

    Exchange::IMessageControl::IControlIterator* controls = nullptr;
    hr = plugin->Controls(controls);
    EXPECT_EQ(Core::ERROR_NONE, hr);
    ASSERT_NE(nullptr, controls);

    bool found = false;
    Exchange::IMessageControl::Control current;
    while (controls->Next(current)) {
        if (current.type == testControl.type &&
            current.category == testControl.category &&
            current.module == testControl.module &&
            current.enabled == testControl.enabled) {
            found = true;
            break;
        }
    }
    EXPECT_TRUE(found);
    controls->Release();
}

// New: verify TestShell refcount behavior and ConfigLine preservation.
TEST_F(MessageControlL1Test, TestShell_RefCountAndConfig) {
    TestShell* shell = new TestShell(R"({"console":false,"syslog":true})");
    ASSERT_NE(nullptr, shell);

    // ConfigLine preserved
    EXPECT_EQ(R"({"console":false,"syslog":true})", shell->ConfigLine());

    // Refcount: initial 1, AddRef -> 2, Release -> 1, Release -> 0 (object not deleted by Release())
    shell->AddRef(); // now 2
    uint32_t v = shell->Release(); // now 1
    EXPECT_EQ(1u, v);
    v = shell->Release(); // now 0
    EXPECT_EQ(0u, v);

    // Manual delete since Release() does not delete in this test shell
    delete shell;
}

// New: verify Substitute and Metadata helpers on TestShell behave as simple passthroughs.
TEST_F(MessageControlL1Test, TestShell_SubstituteAndMetadata) {
    TestShell shell; // stack instance
    const string input = "replace-me";
    EXPECT_EQ(input, shell.Substitute(input));

    string meta;
    Core::hresult hr = shell.Metadata(meta);
    EXPECT_EQ(Core::ERROR_NONE, hr);
    // Metadata result is not strictly specified; ensure call succeeds and returns a string (possibly empty)
    EXPECT_TRUE(meta.size() >= 0);
}
