All notable changes to this RDK Service will be documented in this file.

    Each RDK Service has a CHANGELOG file that contains all changes done so far. When version is updated, add a entry in the CHANGELOG.md at the top with user friendly information on what was changed with the new version. Please don't mention JIRA tickets in CHANGELOG.

    Please Add entry in the CHANGELOG for each version change and indicate the type of change with these labels:
        Added for new features.
        Changed for changes in existing functionality.
        Deprecated for soon-to-be removed features.
        Removed for now removed features.
        Fixed for any bug fixes.
        Security in case of vulnerabilities.

    Changes in CHANGELOG should be updated when commits are added to the main or release branches. There should be one CHANGELOG entry per JIRA Ticket. This is not enforced on sprint branches since there could be multiple changes for the same JIRA ticket during development.

    For more details, refer to versioning section under Main README.

## [1.0.4] - 2025-06-12
- Backend as an external dynamic library

## [1.0.3] - 2025-02-07
### Fixed
- Fix hang on call to zdump
    
## [1.0.2] - 2024-12-17
- Support optional event mapping json configuration

