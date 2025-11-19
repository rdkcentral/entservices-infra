# Coverity Major Issues - Fixes Applied

## Summary
This branch contains fixes for critical and high-priority Coverity issues identified in the entservices-infra codebase.

## Issues Fixed

### 1. Resource Leaks - File and Directory Handles
**Priority: CRITICAL**

#### DIR* Leaks Fixed:
- **StorageManager/RequestHandler.cpp**
  - `populateAppInfoCacheFromStoragePath()`: Added early return with proper closedir on error path
  - `clearAll()`: Restructured if-else to ensure closedir is always called on all paths
  
**Impact**: Prevents file descriptor exhaustion and system resource leaks

### 2. Double File Open
**Priority: CRITICAL**

#### Fixed in RuntimeManager/DobbySpecGenerator.cpp:
- Removed redundant `open()` call on already-opened ifstream
- Changed `good()` check to `is_open()` for proper stream state verification
- Eliminated potential file descriptor leak

**Impact**: Prevents undefined behavior and resource waste

### 3. Memory Leaks - Smart Pointer Adoption
**Priority: HIGH**

#### Fixed in RuntimeManager/DobbySpecGenerator:
- Converted raw pointer `AIConfiguration*` to `std::unique_ptr<AIConfiguration>`
- Removed manual `delete` from destructor
- Added null check before `initialize()` call
- Used `std::make_unique` for safer allocation

**Impact**: 
- Prevents memory leaks
- Improves exception safety
- Provides automatic resource management

## Remaining Issues (Not Fixed in This PR)

### Still To Address:
1. **Buffer Overflow Prevention**: Review all sprintf/strcpy usage (most use safe variants)
2. **NULL Pointer Checks**: Add defensive null checks in logging paths
3. **TOCTOU Vulnerabilities**: File existence checks followed by operations
4. **Thread Safety**: Some singleton patterns need std::call_once
5. **Integer Overflow**: Type narrowing conversions need range validation

## Testing Recommendations

Before merging, verify:
1. All existing unit tests pass
2. No new memory leaks with valgrind
3. Run static analysis (Coverity/cppcheck)
4. Integration tests for affected components:
   - StorageManager operations
   - RuntimeManager container creation
   - File system operations

## Files Modified

1. `RuntimeManager/DobbySpecGenerator.h` - Smart pointer declaration
2. `RuntimeManager/DobbySpecGenerator.cpp` - Smart pointer implementation, file handle fix
3. `StorageManager/RequestHandler.cpp` - DIR* leak fixes

## Coverity Issue Categories Addressed

- **RESOURCE_LEAK**: 3 instances fixed
- **USE_AFTER_FREE**: Prevented by smart pointer adoption
- **FORWARD_NULL**: Improved by adding null checks

