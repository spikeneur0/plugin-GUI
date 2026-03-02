# Open Ephys GUI - Full Code Audit Report

**Date:** 2026-03-01
**Version:** v1.0.2 (commit c91afeb)
**Auditor:** Claude Code

---

## Executive Summary

The Open Ephys GUI is a well-structured C++ application built on JUCE for real-time electrophysiology data acquisition. The codebase is mature and functional, but has several areas that could benefit from modernization and hardening. The most critical concerns involve **thread safety in the recording pipeline**, **suppressed compiler warnings**, and **the global-state AccessClass pattern**.

---

## 1. ARCHITECTURE & DESIGN

### 1.1 AccessClass is a Global Service Locator (Anti-Pattern)
**File:** `Source/AccessClass.h:42-113`, `Source/AccessClass.cpp:33-47`

The `AccessClass` namespace uses file-scoped raw pointers (`ui`, `ev`, `pg`, `cp`, etc.) as a global service locator. Any code in the application can call `AccessClass::getProcessorGraph()` to get a raw pointer to a core object.

**Problems:**
- No thread safety on these global pointers
- Lifetime management is implicit — if any object is destroyed out of order, dangling pointers result
- Tight coupling: plugins and UI code freely reach into core objects
- The `ExternalProcessorAccessor` friend class (`AccessClass.h:106-111`) reaches into private members of `GenericProcessor`, breaking encapsulation

**Suggestion:** Replace with a proper dependency injection pattern or at minimum add assertions/null checks on every getter.

### 1.2 Excessive `friend` Declarations on GenericProcessor
**File:** `Source/Processors/GenericProcessor/GenericProcessor.h:115-121`

```cpp
friend AccessClass::ExternalProcessorAccessor;
friend class RecordEngine;
friend class MessageCenter;
friend class ProcessorGraph;
friend class GenericEditor;
friend class Splitter;
friend class Merger;
```

Seven friend declarations effectively make the entire `private` section accessible to most of the codebase. This defeats the purpose of encapsulation.

**Suggestion:** Expose needed methods via protected/public interfaces rather than friends. Consider a "ProcessorInternals" API for trusted components.

### 1.3 `using namespace Plugin` in Header
**File:** `Source/Processors/GenericProcessor/GenericProcessor.h:98`

`using namespace Plugin;` in a header file pollutes the namespace for all translation units that include this header.

**Suggestion:** Remove and use fully qualified `Plugin::` names.

---

## 2. MEMORY MANAGEMENT

### 2.1 Deprecated `ScopedPointer` Usage
Multiple files still use JUCE's deprecated `ScopedPointer` instead of `std::unique_ptr`:

- `Source/Processors/Events/Event.h:42,277,339,439,527` — Event type aliases
- `Source/Processors/AudioNode/AudioEditor.h:164-170` — UI components
- `Source/Processors/MessageCenter/MessageCenter.h:111,126` — Editor and channel
- `Source/Processors/RecordNode/RecordNode.h:230` — `EventMonitor`
- `Source/Processors/PlaceholderProcessor/PlaceholderProcessorEditor.h:43-45`

**Suggestion:** Migrate all `ScopedPointer` to `std::unique_ptr` for standards compliance.

### 2.2 Raw `new`/`delete` in MainWindow
**File:** `Source/MainWindow.cpp:67,420-462`

```cpp
consoleViewer = new ConsoleViewer();  // line 67 — raw new, passed to setContentOwned
XmlElement* xml = new XmlElement("MAINWINDOW");  // line 420
...
delete xml;  // line 462
```

The `saveWindowBounds()` method manually allocates and deletes XML elements with `new`/`delete`. If an exception is thrown between allocation and `delete`, memory leaks.

**Suggestion:** Use `std::unique_ptr<XmlElement>` consistently (as is done in `saveProcessorGraph`).

### 2.3 Potential Null Dereference in `compareConfigFiles`
**File:** `Source/MainWindow.cpp:571-575`

```cpp
auto lcAudio = lcXml->getChildByName("AUDIO");
auto rcAudio = rcXml->getChildByName("AUDIO");
if (!lcAudio->isEquivalentTo(rcAudio, false))  // lcAudio could be nullptr!
```

If either config file lacks an `<AUDIO>` element, `getChildByName` returns `nullptr` and the next line crashes.

**Suggestion:** Add null checks before dereferencing.

---

## 3. THREAD SAFETY

### 3.1 DataQueue Lacks Synchronization Between Channels
**File:** `Source/Processors/RecordNode/DataQueue.cpp:234-288`

The `DataQueue` uses per-channel `AbstractFifo` objects but the `startRead()` method iterates across all channels without any mutex. If the audio thread writes to channel N while the record thread reads channels 0..N-1, the data could be inconsistent across channels for the same time block.

The `m_readInProgress` flag (`DataQueue.h:113`) is a plain `bool`, not `std::atomic<bool>`, yet it's read/written from both the audio and record threads.

**Suggestion:** Make `m_readInProgress` atomic. Consider whether cross-channel consistency guarantees are needed.

### 3.2 RecordNode Shared State Without Locks
**File:** `Source/Processors/RecordNode/RecordNode.h:214-229`

Multiple fields like `recordEvents`, `recordSpikes`, `recordContinuousChannels`, `fifoUsage`, and `isRecording` are accessed from both the audio thread (via `process()`) and the UI thread, with no synchronization primitives protecting them. Only `setFirstBlock` is `std::atomic`.

**Suggestion:** Mark boolean flags as `std::atomic<bool>` and protect map structures with mutexes or use lock-free alternatives.

### 3.3 DataQueue Overflow Silently Drops Data
**File:** `Source/Processors/RecordNode/DataQueue.cpp:188-191`

```cpp
if ((size1 + size2) < nSamples)
{
    LOGE(__FUNCTION__, " Recording Data Queue Overflow...");
}
// continues writing partial data anyway
```

When the queue overflows, it logs an error but proceeds to write only `size1 + size2` samples while calling `finishedWrite(size1 + size2)`. The remaining samples are silently dropped. For a scientific recording application, this is a **data integrity risk**.

**Suggestion:** At minimum, increment a counter that the UI can display. Consider pausing acquisition or warning the user more prominently.

---

## 4. BUILD SYSTEM & COMPILER SETTINGS

### 4.1 All Warnings Suppressed (`/W0`)
**File:** `CMakeLists.txt:109,191`

```cmake
target_compile_options(open-ephys PRIVATE /sdl- /nologo /MP /W0 /bigobj)
```

`/W0` disables ALL compiler warnings. `/sdl-` disables Security Development Lifecycle checks. This means the compiler will not warn about:
- Uninitialized variables
- Signed/unsigned mismatches
- Buffer overflows (SDL checks disabled)
- Unused variables
- Implicit conversions

**This is the single most impactful change to make.**

**Suggestion:** Enable at least `/W3` (or `/W4`) and `/sdl`. Fix the resulting warnings incrementally. Add `-Wall -Wextra` for GCC/Clang builds.

### 4.2 Hardcoded Port Number
**File:** `Source/Utils/OpenEphysHttpServer.h:45`

```cpp
#define PORT 37497
```

The HTTP server port is a compile-time constant. If another instance or application uses this port, the server silently fails.

**Suggestion:** Make configurable via settings or environment variable, with fallback.

### 4.3 No Address Sanitizer / UBSan in Debug Builds
The CMakeLists.txt does not enable any sanitizers for debug builds (ASan, UBSan, TSan). For a real-time application with complex threading, these are essential development tools.

**Suggestion:** Add optional CMake flags for `-fsanitize=address,undefined` on GCC/Clang and `/fsanitize=address` on MSVC.

---

## 5. SECURITY

### 5.1 HTTP Server Has No Authentication
**File:** `Source/Utils/OpenEphysHttpServer.h`

The HTTP server listens on port 37497 with **no authentication**. Any process on the machine (or network, if the firewall allows) can:
- Start/stop acquisition
- Start/stop recording
- Modify processor parameters
- Load/save signal chains

**Suggestion:** Add at minimum a token-based auth, or bind to localhost only (127.0.0.1).

### 5.2 Auto-Updater Downloads Over HTTPS But No Signature Verification
**File:** `Source/AutoUpdater.h`

The `LatestVersionCheckerAndUpdater` checks for new versions and downloads installers. While it likely uses HTTPS (via JUCE's URL class), there is no code-signing or checksum verification of downloaded binaries.

**Suggestion:** Verify downloaded files against a known hash or GPG signature before prompting the user to install.

### 5.3 Plugin Loading Executes Arbitrary DLLs
**File:** `Source/Processors/PluginManager/PluginManager.h`

Plugins are loaded from fixed directories as native DLLs. There is no signature verification, no sandboxing, and no permission model. A malicious DLL in the plugins directory gets full process privileges.

**Suggestion:** Document the security model. Consider code-signing checks on plugin DLLs on Windows/macOS.

---

## 6. RECORDING PIPELINE

### 6.1 DataQueue `fillSampleNumbers` Has a Logic Bug
**File:** `Source/Processors/RecordNode/DataQueue.cpp:140-148`

```cpp
for (int i = 0; i < size; i += m_blockSize)
{
    if ((blockStartPos + i) < (index + size))
    {
        latestSampleNumber = startSampleNumber + (i * m_blockSize);  // BUG: double multiplication
        m_sampleNumbers[channel]->at(blockIdx) = latestSampleNumber;
    }
}
```

The variable `i` already increments by `m_blockSize` each iteration, so `i * m_blockSize` effectively squares the block size offset. The sample number jumps will grow quadratically rather than linearly.

**Suggestion:** This should likely be `startSampleNumber + i` (since `i` already counts in block-sized steps) or the loop increment should be `i++` with `i * m_blockSize`.

### 6.2 Large Hardcoded Buffer Constants
**File:** `Source/Processors/RecordNode/RecordNode.h:44-52`

```cpp
#define WRITE_BLOCK_LENGTH 1024
#define DATA_BUFFER_NBLOCKS 300
#define EVENT_BUFFER_NEVENTS 200000
#define SPIKE_BUFFER_NSPIKES 200000
#define MAX_BUFFER_SIZE 40960
#define CHANNELS_PER_THREAD 384
```

These are compile-time constants. At 384 channels with 300 blocks of 1024 samples, the data buffer alone is ~450 MB of float data. For systems with fewer channels, this wastes memory; for higher channel counts, it may be insufficient.

**Suggestion:** Make these configurable or dynamically sized based on actual channel count.

---

## 7. PLUGIN API

### 7.1 Public Fields on GenericProcessor
**File:** `Source/Processors/GenericProcessor/GenericProcessor.h:154-157,458-467`

```cpp
GenericProcessor* sourceNode;   // public raw pointer
GenericProcessor* destNode;     // public raw pointer
int nextAvailableChannel;       // public
int saveOrder;                  // public
int loadOrder;                  // public
int currentChannel;             // public
```

Public mutable fields on a core base class invite misuse by plugins. Any plugin can modify `sourceNode`/`destNode` and corrupt the signal chain.

**Suggestion:** Make these private with accessor methods. The `sourceNode`/`destNode` pattern should use the existing getter/setter methods exclusively.

### 7.2 Static Mutable State in GenericProcessor
**File:** `Source/Processors/GenericProcessor/GenericProcessor.h:590,602`

```cpp
static void registerUndoableAction(int nodeId, ProcessorAction* action)
    { undoableActions[nodeId].push_back(action); }
static std::map<int, std::vector<ProcessorAction*>> undoableActions;
```

A static map of raw pointers shared across all processor instances. No thread safety, no ownership semantics. If a processor is deleted while actions reference it, dangling pointers result.

**Suggestion:** Move undo management to `ProcessorGraph` and use smart pointers.

---

## 8. UI & CODE QUALITY

### 8.1 C-style Casts Throughout
**File:** `Source/MainWindow.cpp:123,260,282,439,515,520`

```cpp
UIComponent* ui = (UIComponent*) documentWindow->getContentComponent();
```

C-style casts are used everywhere instead of `static_cast<>` or `dynamic_cast<>`. These bypass type checking and can mask errors.

**Suggestion:** Replace with `static_cast<>` (or `dynamic_cast<>` where the type might not match).

### 8.2 `moreThanOneInstanceAllowed()` Returns True
**File:** `Source/Main.cpp:193`

Multiple GUI instances can run simultaneously, competing for the same config files, recording directories, hardware, and HTTP port. This could cause data corruption.

**Suggestion:** Return `false` or add instance coordination (lock file, named mutex).

---

## 9. PRIORITY RANKINGS

| Priority | Issue | Impact |
|----------|-------|--------|
| **CRITICAL** | `/W0` suppresses all warnings + `/sdl-` | Hides bugs, buffer overflows, UB |
| **CRITICAL** | DataQueue `fillSampleNumbers` logic bug | Incorrect sample numbers in recordings |
| **HIGH** | DataQueue overflow silently drops samples | Data loss during recording |
| **HIGH** | HTTP server has no authentication | Remote control of acquisition |
| **HIGH** | `m_readInProgress` not atomic | Race condition in recording |
| **MEDIUM** | RecordNode shared state without locks | Potential data races |
| **MEDIUM** | AccessClass global raw pointers | Dangling pointer risk |
| **MEDIUM** | ScopedPointer deprecation | Technical debt |
| **MEDIUM** | Null dereference in compareConfigFiles | Crash on malformed config |
| **LOW** | Public mutable fields on GenericProcessor | API misuse by plugins |
| **LOW** | C-style casts | Code quality |
| **LOW** | Raw new/delete in saveWindowBounds | Memory leak risk |
| **LOW** | using namespace in header | Namespace pollution |

---

## 10. RECOMMENDED FIRST STEPS

1. **Enable compiler warnings** (`/W3` minimum, fix incrementally)
2. **Fix the `fillSampleNumbers` sample-number bug** — this directly affects recorded data
3. **Make `m_readInProgress` atomic** and audit other shared state in RecordNode
4. **Bind HTTP server to localhost** and add token auth
5. **Add null checks** in `compareConfigFiles` and other XML parsing code
6. **Migrate ScopedPointer to std::unique_ptr** across the codebase
