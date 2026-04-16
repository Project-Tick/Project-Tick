# Section 07 — Accounts & Java API

> **Header:** `PluginAPI.h` (function pointer declarations) · **SDK:** `mmco_sdk.h` (struct definition) · **Implementation:** `PluginManager.cpp` (static trampolines)

---

## Overview

Section 07 of the MMCO plugin API combines two closely related **read-only**
subsystems: account enumeration and Java installation discovery. Both are
essential for plugins that need to understand the runtime environment
before launching or configuring Minecraft instances.

The Account Management functions (internally labelled "S7" in the source)
allow a plugin to list every Minecraft account the user has logged into,
inspect its profile name, profile ID (UUID), authentication type, online
state, and determine which account is the active default.

The Java Management functions (internally "S8") expose MeshMC's discovered
Java installations — their version string, CPU architecture, filesystem
path, and whether the launcher considers them recommended.

Both subsystems are **strictly read-only**. Plugins can inspect accounts
and Java installs but cannot add, remove, or modify them. This is a
deliberate security boundary: plugins can read profile names and UUIDs to
display user information or customize behavior, but they **never** have
access to authentication tokens, refresh tokens, or credentials.

```text
┌──────────────────────────────────────────────────────────────────────────────────────┐
│  S07 — ACCOUNT MANAGEMENT (read-only)                                                │
│  1.  account_count()                 → number of registered accounts                 │
│  2.  account_get_profile_name(idx)   → Minecraft username / gamertag                 │
│  3.  account_get_profile_id(idx)     → undashed UUID (Mojang format)                 │
│  4.  account_get_type(idx)           → authentication type string ("msa")            │
│  5.  account_get_state(idx)          → state enum (0–6): Unchecked…Gone              │
│  6.  account_is_active(idx)          → whether this account is currently in use      │
│  7.  account_get_default_index()     → index of the default account (or -1)          │
│                                                                                      │
│  S08 — JAVA MANAGEMENT (read-only)                                                   │
│  8.  java_count()                    → number of discovered Java installs             │
│  9.  java_get_version(idx)           → version string (e.g. "21.0.3")                │
│ 10.  java_get_arch(idx)              → architecture string ("amd64", "aarch64", …)   │
│ 11.  java_get_path(idx)              → absolute path to java binary                  │
│ 12.  java_is_recommended(idx)        → whether MeshMC recommends this install        │
│ 13.  instance_get_java_version(id)   → Java version required by an instance          │
└──────────────────────────────────────────────────────────────────────────────────────┘
```

All thirteen function pointers live in `MMCOContext` and are assigned in
`PluginManager::initContext()`. They are available from the moment
`mmco_init()` is called with a valid module handle.

---

## Sub-Pages

| File | Contents |
|------|----------|
| [accounts.md](accounts.md) | Full reference for all 7 account management functions |
| [java.md](java.md) | Full reference for all 6 Java management functions |

---

## Context Struct Declarations

From `PluginAPI.h`, the relevant `MMCOContext` fields:

```c
/* S7 — Account Management (read-only) */
int         (*account_count)            (void* mh);
const char* (*account_get_profile_name) (void* mh, int index);
const char* (*account_get_profile_id)   (void* mh, int index);
const char* (*account_get_type)         (void* mh, int index);
int         (*account_get_state)        (void* mh, int index);
int         (*account_is_active)        (void* mh, int index);
int         (*account_get_default_index)(void* mh);

/* S8 — Java Management (read-only) */
int         (*java_count)               (void* mh);
const char* (*java_get_version)         (void* mh, int index);
const char* (*java_get_arch)            (void* mh, int index);
const char* (*java_get_path)            (void* mh, int index);
int         (*java_is_recommended)      (void* mh, int index);
const char* (*instance_get_java_version)(void* mh, const char* id);
```

The same layout appears in `mmco_sdk.h` for out-of-tree SDK consumers.

---

## The Account Model

### Authentication Backend

MeshMC currently supports a single authentication type:

| Type String | Enum Value | Description |
|-------------|------------|-------------|
| `"msa"` | `AccountType::MSA` | Microsoft Account authentication via Xbox Live / Mojang services |

Internally this is defined in `AccountData.h`:

```cpp
enum class AccountType { MSA };
```

All accounts in MeshMC use the Microsoft Authentication flow. The
`typeString()` method on `MinecraftAccount` always returns `"msa"` in
the current codebase. Future authentication backends (e.g. for offline /
demo modes) would introduce additional type strings; plugins should not
hard-code assumptions about this value.

### Account State Machine

Each account has a state that reflects its current authentication status.
The `account_get_state()` function returns an integer corresponding to the
`AccountState` enum defined in `AccountData.h`:

```cpp
enum class AccountState {
    Unchecked,  // 0 — Never validated since launch
    Offline,    // 1 — Explicitly offline (no network)
    Working,    // 2 — Authentication in progress
    Online,     // 3 — Successfully authenticated
    Errored,    // 4 — Authentication failed with an error
    Expired,    // 5 — Token has expired, needs refresh
    Gone        // 6 — Account no longer exists upstream
};
```

```text
State Diagram:

    ┌──────────┐      validate       ┌──────────┐
    │ Unchecked├─────────────────────►│ Working  │
    │   (0)    │                      │   (2)    │
    └──────────┘                      └────┬─────┘
                                           │
                          ┌────────────────┼────────────────┐
                          ▼                ▼                ▼
                    ┌──────────┐    ┌──────────┐    ┌──────────┐
                    │  Online  │    │ Errored  │    │ Expired  │
                    │   (3)    │    │   (4)    │    │   (5)    │
                    └──────────┘    └──────────┘    └────┬─────┘
                                                         │
                                          refresh        │
                                    ┌────────────────────┘
                                    ▼
                              ┌──────────┐
                              │  Working │
                              │   (2)    │
                              └──────────┘

                    ┌──────────┐
                    │  Offline │  ← user chose offline mode
                    │   (1)    │
                    └──────────┘

                    ┌──────────┐
                    │   Gone   │  ← account deleted upstream
                    │   (6)    │
                    └──────────┘
```

### Profile Data

Each Minecraft account has an associated **profile**:

- **Profile Name** — The Minecraft in-game name (gamertag). This is the
  name that appears in multiplayer, on the title screen, etc. Returned
  by `account_get_profile_name()`. If no profile has been set up,
  MeshMC returns a placeholder string like `"No profile (Xbox gamertag)"`.

- **Profile ID** — The Minecraft UUID assigned by Mojang. This is returned
  by `account_get_profile_id()` as an **undashed hex string** (32
  characters, e.g. `"a1b2c3d4e5f6a7b8c9d0e1f2a3b4c5d6"`). This is the
  standard Mojang API format — no dashes. If your plugin needs the
  dashed format (`a1b2c3d4-e5f6-a7b8-c9d0-e1f2a3b4c5d6`), you must
  insert the dashes yourself at positions 8-4-4-4-12.

### Security Boundary

Plugins operate in a **read-only sandbox** with respect to accounts:

| Accessible via API | NOT accessible via API |
|--------------------|-----------------------|
| Profile name (gamertag) | MSA access token |
| Profile ID (UUID) | MSA refresh token |
| Account type string | Xbox Live tokens |
| Account state (int) | Mojang services token |
| Active flag | Yggdrasil token |
| Default account index | Account credentials |
|  | Skin/cape binary data |
|  | Entitlement details |

This boundary exists because authentication tokens would allow
impersonation. The MMCO API deliberately exposes only the information
needed for display, logging, and per-user configuration — never anything
that could compromise the account.

---

## Java Discovery Mechanism

### How MeshMC Finds Java Installations

MeshMC maintains a `JavaInstallList` (derived from `BaseVersionList`)
that is populated by scanning the system for Java installations. The
discovery process runs at startup and can be re-triggered by the user.

The scanner looks at:

1. **System PATH** — Searches for `java` / `javaw` executables in `$PATH`
2. **Well-known directories** — Platform-specific locations:
   - Linux: `/usr/lib/jvm/`, `/usr/java/`, `~/.sdkman/candidates/java/`
   - macOS: `/Library/Java/JavaVirtualMachines/`,
     `/usr/local/opt/openjdk*/`
   - Windows: `C:\Program Files\Java\`, `C:\Program Files\Eclipse Adoptium\`,
     registry entries under `HKLM\SOFTWARE\JavaSoft`
3. **MeshMC-managed installs** — Java runtimes downloaded through the
   launcher's built-in Java downloader (stored in the MeshMC data
   directory)

For each discovered Java binary, MeshMC spawns the `JavaChecker` process
which runs `java -XshowSettings:all` and parses the output to extract:

- Version string (e.g. `"21.0.3"`, `"1.8.0_402"`, `"17.0.11"`)
- Architecture (e.g. `"amd64"`, `"aarch64"`, `"x86"`)
- Vendor string (not exposed to plugins)

### The `JavaInstall` Structure

Each discovered Java installation is represented by a `JavaInstall` object
(defined in `JavaInstall.h`):

```cpp
struct JavaInstall : public BaseVersion {
    JavaVersion id;          // Parsed version (major.minor.security)
    QString     arch;        // Architecture string ("amd64", "aarch64", …)
    QString     path;        // Absolute path to the java binary
    bool        recommended; // Whether MeshMC recommends this install
};
```

### The `JavaVersion` Class

The version string is parsed into a structured `JavaVersion` object
(from `JavaVersion.h`):

```cpp
class JavaVersion {
    int     m_major;       // e.g. 21 for "21.0.3"
    int     m_minor;       // e.g. 0
    int     m_security;    // e.g. 3
    bool    m_parseable;   // false if version string couldn't be parsed
    QString m_prerelease;  // prerelease suffix, if any
    QString m_string;      // original string representation
};
```

The `toString()` method returns the original version string as discovered.
This is what `java_get_version()` returns to plugins.

### Architecture Strings

The `arch` field is whatever the JVM's `os.arch` system property reports.
Common values:

| `java_get_arch()` return | Platform | Description |
|--------------------------|----------|-------------|
| `"amd64"` | Linux / Windows x86-64 | Standard 64-bit x86 |
| `"x86_64"` | macOS x86-64 | Same arch, macOS naming |
| `"aarch64"` | Linux / Windows ARM64 | 64-bit ARM |
| `"arm64"` | macOS Apple Silicon | Same as aarch64 |
| `"x86"` | Windows 32-bit | Legacy 32-bit x86 |

> **Note:** The architecture string comes directly from the Java runtime
> and may vary between JVM vendors. Plugins should handle aliases: both
> `"amd64"` and `"x86_64"` mean 64-bit x86; both `"aarch64"` and
> `"arm64"` mean 64-bit ARM.

### Recommended Flag

MeshMC marks certain Java installations as "recommended" based on:

1. The architecture matches the host system
2. The version is appropriate for modern Minecraft (Java 17+ for 1.17+,
   Java 21+ for 1.20.5+)
3. It is not a known-problematic build

The `java_is_recommended()` function exposes this flag.

### Java List Loading

The Java list is loaded asynchronously. The `java_count()` function
checks `javalist()->isLoaded()` and returns `0` if the discovery scan
has not completed yet. Plugins that need to enumerate Java installations
should ideally do so in a hook that runs after initialization (such as
`MMCO_HOOK_LAUNCH_PRE`) rather than in `mmco_init()`.

---

## String Ownership

All `const char*` returns from both the Account and Java APIs use the
per-module `tempString` mechanism described in [Section 01](../s01-lifecycle/).
Key rules:

1. **Pointer validity**: The returned string remains valid only until the
   **next** call to any API function that returns `const char*` from the
   same module handle.
2. **Copy immediately**: If you need the string longer, `strdup()` or copy
   it into your own buffer before calling another API function.
3. **Do not free**: The pointer is owned by the runtime. Calling `free()`
   on it is undefined behavior.
4. **NULL on error**: All string-returning functions return `NULL` when the
   requested data is not available (invalid index, missing profile, etc.).

```c
/* WRONG — second call invalidates the first pointer */
const char* name = ctx->account_get_profile_name(mh, 0);
const char* uuid = ctx->account_get_profile_id(mh, 0);
printf("%s %s\n", name, uuid);  /* name is now dangling! */

/* CORRECT — copy before next call */
const char* tmp = ctx->account_get_profile_name(mh, 0);
char* name = tmp ? strdup(tmp) : NULL;

tmp = ctx->account_get_profile_id(mh, 0);
char* uuid = tmp ? strdup(tmp) : NULL;

printf("%s %s\n", name ? name : "(null)", uuid ? uuid : "(null)");
free(name);
free(uuid);
```

---

## Thread Safety

All functions in Section 07 **must be called from the main (GUI) thread**.
This is the same thread that calls `mmco_init()` and hook callbacks. The
underlying data models (`AccountList`, `JavaInstallList`) are QObject-based
and are not thread-safe.

---

## Quick Example: Account Summary

A compact example that demonstrates the account subsystem:

```c
#include <mmco_sdk.h>
#include <stdio.h>
#include <string.h>

static MMCOContext* ctx;
static void* mh;

static const char* state_name(int s) {
    switch (s) {
        case 0: return "Unchecked";
        case 1: return "Offline";
        case 2: return "Working";
        case 3: return "Online";
        case 4: return "Errored";
        case 5: return "Expired";
        case 6: return "Gone";
        default: return "Unknown";
    }
}

void print_account_summary(void) {
    int n = ctx->account_count(mh);
    int def = ctx->account_get_default_index(mh);

    printf("=== MeshMC Accounts (%d) ===\n", n);
    for (int i = 0; i < n; i++) {
        const char* tmp = ctx->account_get_profile_name(mh, i);
        char name[256];
        snprintf(name, sizeof(name), "%s", tmp ? tmp : "(no profile)");

        tmp = ctx->account_get_profile_id(mh, i);
        char uuid[64];
        snprintf(uuid, sizeof(uuid), "%s", tmp ? tmp : "(none)");

        int state = ctx->account_get_state(mh, i);
        int active = ctx->account_is_active(mh, i);

        printf("  [%d] %s%s%s\n", i, name,
               (i == def) ? " (DEFAULT)" : "",
               active ? " *ACTIVE*" : "");
        printf("      UUID:  %s\n", uuid);
        printf("      State: %s (%d)\n", state_name(state), state);
    }
}
```

---

## Quick Example: Java Compatibility Checker

```c
#include <mmco_sdk.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static MMCOContext* ctx;
static void* mh;

void check_java_compatibility(const char* instance_id) {
    /* Get the Java version the instance needs */
    const char* tmp = ctx->instance_get_java_version(mh, instance_id);
    if (!tmp) {
        printf("Could not determine required Java version.\n");
        return;
    }
    char required[64];
    snprintf(required, sizeof(required), "%s", tmp);

    int major_required = atoi(required);  /* "17.0.1" → 17 */
    printf("Instance requires Java %s (major: %d)\n",
           required, major_required);

    /* Find compatible Java installs */
    int n = ctx->java_count(mh);
    int found = 0;
    for (int i = 0; i < n; i++) {
        tmp = ctx->java_get_version(mh, i);
        if (!tmp) continue;

        int major = atoi(tmp);
        if (major >= major_required) {
            char ver[64];
            snprintf(ver, sizeof(ver), "%s", tmp);

            tmp = ctx->java_get_arch(mh, i);
            char arch[32];
            snprintf(arch, sizeof(arch), "%s", tmp ? tmp : "?");

            tmp = ctx->java_get_path(mh, i);
            char path[512];
            snprintf(path, sizeof(path), "%s", tmp ? tmp : "?");

            int rec = ctx->java_is_recommended(mh, i);
            printf("  [%d] Java %s (%s) %s\n", i, ver, arch,
                   rec ? "[RECOMMENDED]" : "");
            printf("       %s\n", path);
            found++;
        }
    }

    if (!found) {
        printf("No compatible Java installation found!\n");
    }
}
```

---

## Related Sections

| Section | Relationship |
|---------|-------------|
| [S01 — Lifecycle](../s01-lifecycle/) | `tempString` ownership model, `mmco_init()` |
| [S03 — Instances](../s03-instances/) | `instance_get_id()` used with `instance_get_java_version()` |
| [S04 — Instance Management](../s04-instance-management/) | Launch operations that depend on accounts and Java |
| [S12 — UI Dialogs](../s12-ui-dialogs/) | Displaying account/Java info in dialog UI |
