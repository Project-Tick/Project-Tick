# Application Info — `get_app_version()`, `get_app_name()` {#app-info}

> **Section:** S14 (Utility) · **Header:** `PluginAPI.h` · **Trampolines:** `PluginManager::api_get_app_version`, `PluginManager::api_get_app_name` · **Backend:** `BuildConfig.VERSION_STR`, `BuildConfig.MESHMC_NAME`

These two functions expose the application's identity at runtime. They
are the canonical way for plugins to discover the host application's
name and version without hard-coding anything or depending on external
environment variables.

---

## `get_app_version()` — Get the application version string {#get_app_version}

### Synopsis

```c
const char* get_app_version(void* mh);
```

Returns the version string of the running MeshMC build.

### Parameters

| # | Name | Type | Description |
|---|------|------|-------------|
| 1 | `mh` | `void*` | Module handle. Pass `ctx->module_handle`. |

### Return Value

| Condition | Value |
|-----------|-------|
| Always | Non-null pointer to a null-terminated UTF-8 string. |

This function **cannot fail**. It always returns a valid C string.

### Version String Format

The returned string is `BuildConfig.VERSION_STR`, which is set at
**build time** from the CMake variable `@MeshMC_VERSION_STRING@`. The
format depends on the build:

#### Tagged releases (stable builds)

For tagged releases, the version follows a **three-component
`MAJOR.MINOR.HOTFIX`** scheme:

```text
1.2.0
1.2.1
2.0.0
```

This matches the output of `Config::printableVersionString()` when a
valid git tag is present.

#### Development builds (untagged)

When there is no git tag (or the tag ends with `-NOTFOUND`), the version
string may include a short commit hash suffix:

```text
1.2.0-r3a7b2c
```

The suffix format is `-r` followed by the first 6 characters of the git
commit hash.

#### The `VERSION_STR` vs `FULL_VERSION_STR` distinction

The plugin API exposes `VERSION_STR`, which is the human-readable
version set by CMake. Internally, MeshMC also has a `FULL_VERSION_STR`
(format `MAJOR.MINOR.BUILD`) used by the updater. Plugins do not have
direct access to `FULL_VERSION_STR`; if you need the individual
components for numeric comparison, you must parse `VERSION_STR` yourself.

### Implementation

```cpp
const char* PluginManager::api_get_app_version(void* mh)
{
    auto* r = rt(mh);
    r->tempString = BuildConfig.VERSION_STR.toStdString();
    return r->tempString.c_str();
}
```

The implementation:
1. Resolves the `ModuleRuntime` from the opaque `module_handle`.
2. Converts the Qt `QString` (`BuildConfig.VERSION_STR`) to a
   `std::string` and stores it in `ModuleRuntime::tempString`.
3. Returns the `c_str()` pointer into that buffer.

The conversion happens on every call (it is not cached), so the cost is
one `QString::toStdString()` allocation per invocation. This is
negligible in practice.

### String Ownership

The returned pointer points into `ModuleRuntime::tempString`. It is
**invalidated** by the next API call on the same module that writes to
`tempString`. This includes `get_app_name()`, `instance_get_name()`,
`setting_get()`, `mod_get_filename()`, and many others.

| Property | Value |
|----------|-------|
| Backing storage | `ModuleRuntime::tempString` (`std::string`) |
| Valid until | Next `tempString`-writing API call on this module |
| Caller must free? | No. Do **not** call `free()` on this pointer. |
| Thread safety | Single-threaded only (all MMCO API calls). |

**Always copy the string if you need it beyond the next API call:**

```c
const char* raw = ctx->get_app_version(ctx->module_handle);
char version[64];
snprintf(version, sizeof(version), "%s", raw);
/* 'version' is now safe to use after further API calls */
```

### When Does the Version Change?

The version string is compiled into the binary. It is constant for the
lifetime of the process. It will only change when:

- The user updates MeshMC to a new version (new binary).
- A developer rebuilds from a different git state.

Plugins can safely call `get_app_version()` once in `mmco_init()`, copy
the result, and use it for the remainder of their lifetime without
calling it again.

### Parsing the Version

For plugins that need to compare versions numerically, here is a parsing
pattern:

```c
#include <stdlib.h>
#include <string.h>

typedef struct {
    int major;
    int minor;
    int hotfix;
    char suffix[32]; /* e.g. "-r3a7b2c" or empty */
} ParsedVersion;

static int parse_version(const char* str, ParsedVersion* out)
{
    if (!str || !out)
        return -1;

    memset(out, 0, sizeof(*out));

    /* Parse MAJOR.MINOR.HOTFIX */
    const char* p = str;
    out->major = (int)strtol(p, (char**)&p, 10);
    if (*p != '.')
        return -1;
    p++;
    out->minor = (int)strtol(p, (char**)&p, 10);
    if (*p != '.')
        return -1;
    p++;
    out->hotfix = (int)strtol(p, (char**)&p, 10);

    /* Capture any suffix (e.g. "-r3a7b2c") */
    if (*p != '\0') {
        strncpy(out->suffix, p, sizeof(out->suffix) - 1);
    }

    return 0;
}

/**
 * Returns: negative if a < b, 0 if equal, positive if a > b.
 * Only compares MAJOR.MINOR.HOTFIX; ignores suffixes.
 */
static int compare_versions(const ParsedVersion* a, const ParsedVersion* b)
{
    if (a->major != b->major) return a->major - b->major;
    if (a->minor != b->minor) return a->minor - b->minor;
    return a->hotfix - b->hotfix;
}
```

### Example: Version Check at Init

```c
#include "mmco_sdk.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

MMCO_DEFINE_MODULE("VersionChecker", "1.0.0", "Example Author",
                   "Demonstrates version gating", "GPL-3.0-or-later");

#define MIN_MAJOR 1
#define MIN_MINOR 2
#define MIN_HOTFIX 0

static MMCOContext* g_ctx;

extern "C" int mmco_init(MMCOContext* ctx)
{
    g_ctx = ctx;
    void* mh = ctx->module_handle;

    const char* ver_raw = ctx->get_app_version(mh);

    /* Copy before the next API call clobbers tempString */
    char ver[64];
    snprintf(ver, sizeof(ver), "%s", ver_raw);

    /* Parse major.minor.hotfix */
    int maj = 0, min = 0, hot = 0;
    if (sscanf(ver, "%d.%d.%d", &maj, &min, &hot) < 3) {
        ctx->log_error(mh, "Failed to parse MeshMC version string");
        return 0; /* continue loading, just warn */
    }

    /* Check minimum version */
    if (maj < MIN_MAJOR ||
        (maj == MIN_MAJOR && min < MIN_MINOR) ||
        (maj == MIN_MAJOR && min == MIN_MINOR && hot < MIN_HOTFIX)) {
        char buf[128];
        snprintf(buf, sizeof(buf),
                 "Requires MeshMC >= %d.%d.%d, found %s",
                 MIN_MAJOR, MIN_MINOR, MIN_HOTFIX, ver);
        ctx->log_error(mh, buf);
        return -1; /* refuse to load */
    }

    char buf[128];
    snprintf(buf, sizeof(buf), "Version check passed: %s", ver);
    ctx->log_info(mh, buf);

    return 0;
}

extern "C" void mmco_unload(MMCOContext* ctx)
{
    (void)ctx;
}
```

### Example: Including Version in Exported Data

```c
static void write_export_header(MMCOContext* ctx, char* buf, size_t sz)
{
    void* mh = ctx->module_handle;

    const char* ver = ctx->get_app_version(mh);
    char version[64];
    snprintf(version, sizeof(version), "%s", ver);

    const char* name = ctx->get_app_name(mh);
    char appname[64];
    snprintf(appname, sizeof(appname), "%s", name);

    int64_t ts = ctx->get_timestamp(mh);

    snprintf(buf, sz,
             "# Exported by %s %s at epoch %lld\n",
             appname, version, (long long)ts);
}
```

---

## `get_app_name()` — Get the application display name {#get_app_name}

### Synopsis

```c
const char* get_app_name(void* mh);
```

Returns the human-readable display name of the host application.

### Parameters

| # | Name | Type | Description |
|---|------|------|-------------|
| 1 | `mh` | `void*` | Module handle. Pass `ctx->module_handle`. |

### Return Value

| Condition | Value |
|-----------|-------|
| Always | Non-null pointer to a null-terminated UTF-8 string. |

This function **cannot fail**. It always returns a valid C string.

### Name Value

The returned string is `BuildConfig.MESHMC_NAME`, set at build time from
the CMake variable `@MeshMC_Name@`. For official builds this is:

```text
MeshMC
```

The name is a single word with no spaces or special characters in the
default configuration. Custom forks may change it at build time.

### Implementation

```cpp
const char* PluginManager::api_get_app_name(void* mh)
{
    auto* r = rt(mh);
    r->tempString = BuildConfig.MESHMC_NAME.toStdString();
    return r->tempString.c_str();
}
```

Identical in structure to `api_get_app_version()`:
1. Resolves `ModuleRuntime*` from the module handle.
2. Converts `BuildConfig.MESHMC_NAME` (`QString`) to `std::string`,
   storing it in `tempString`.
3. Returns `tempString.c_str()`.

### String Ownership

**Identical to `get_app_version()`.** The pointer is backed by
`ModuleRuntime::tempString` and is invalidated by the next
`tempString`-writing API call.

| Property | Value |
|----------|-------|
| Backing storage | `ModuleRuntime::tempString` (`std::string`) |
| Valid until | Next `tempString`-writing API call on this module |
| Caller must free? | No. |
| Thread safety | Single-threaded only. |

### Relationship to Other Name Fields

`BuildConfig` defines several name-related fields:

| Field | Example value | Exposed to plugins? |
|-------|---------------|---------------------|
| `MESHMC_NAME` | `MeshMC` | **Yes** — via `get_app_name()` |
| `MESHMC_DISPLAYNAME` | `MeshMC` | No |
| `MESHMC_BINARY` | `meshmc` | No |
| `MESHMC_CONFIGFILE` | `meshmc.cfg` | No |

Plugins should use `get_app_name()` for all user-facing strings. Do not
assume the name is `"MeshMC"` — custom forks may brand differently.

### Example: Branded Dialogs

```c
static void show_about(MMCOContext* ctx)
{
    void* mh = ctx->module_handle;

    const char* name = ctx->get_app_name(mh);
    char title[128];
    snprintf(title, sizeof(title), "About MyPlugin — running on %s", name);
    /* name pointer is still valid here because snprintf doesn't call API */

    const char* ver = ctx->get_app_version(mh);
    char msg[256];
    snprintf(msg, sizeof(msg),
             "MyPlugin v1.0.0\n"
             "Host: %s %s\n"
             "Licensed under GPL-3.0-or-later",
             title + 30, /* reuse the name we already formatted */
             ver);
    /* WARNING: 'ver' clobbered title's underlying name pointer,
       but we already consumed it into title[] */

    ctx->ui_show_message(mh, 0, title, msg);
}
```

### Example: Logging with Application Identity

This pattern is from the official `hello_world.cpp` SDK example:

```c
/* From sdk/examples/hello_world.cpp */
const char* appName = g_ctx->get_app_name(g_ctx->module_handle);
const char* appVer  = g_ctx->get_app_version(g_ctx->module_handle);
snprintf(buf, sizeof(buf), "Running on %s %s", appName, appVer);
MMCO_LOG(g_ctx, buf);
```

> **Subtle correctness note:** In the snippet above, `appName` is
> invalidated when `get_app_version()` is called (both write to the same
> `tempString`). This code works **only** because `snprintf` reads both
> `appName` and `appVer` before any further API call. The pointers are
> still pointing at valid memory (the `std::string` buffer was
> reallocated, but in this case `get_app_version()` replaced the
> content). **Strictly speaking**, `appName` is stale after the second
> call. The safe pattern is to copy `appName` into a local buffer first.
> In practice, since `snprintf` consumes both pointers synchronously
> before any intervening API call, this works — but it is fragile and
> not recommended for production plugins.

---

## Combining `get_app_version()` and `get_app_name()` Safely

Because both functions write to the same `tempString` buffer, calling
one invalidates the pointer from the other. The correct pattern is:

```c
void* mh = ctx->module_handle;

/* 1. Get and copy the name */
const char* raw_name = ctx->get_app_name(mh);
char name[64];
snprintf(name, sizeof(name), "%s", raw_name);

/* 2. Get and copy the version */
const char* raw_ver = ctx->get_app_version(mh);
char ver[64];
snprintf(ver, sizeof(ver), "%s", raw_ver);

/* 3. Now both 'name' and 'ver' are safe local copies */
char header[256];
snprintf(header, sizeof(header), "%s v%s", name, ver);
```

---

## Relationship to the Plugin Data Directory

Plugins that need to find their own data directory should use
`fs_plugin_data_dir()` from **Section 10 (Filesystem)**, not attempt to
construct paths from `get_app_name()`. The data directory path includes
the application name as a path component but also includes
platform-specific base directories:

| Platform | Data directory pattern |
|----------|----------------------|
| Linux | `~/.local/share/meshmc/plugin-data/<module-id>` |
| Windows | `%LOCALAPPDATA%/meshmc/plugin-data/<module-id>` |
| macOS | `~/Library/Application Support/MeshMC/plugin-data/<module-id>` |

Use `fs_plugin_data_dir()` to get the exact path. See
[S10 — File I/O](../s10-filesystem/file-io.md#fs_plugin_data_dir) for
full documentation.

---

## Error Conditions

Neither `get_app_version()` nor `get_app_name()` can fail. There are no
error codes, no null-return conditions, and no edge cases. The backing
`BuildConfig` fields are always populated at application startup.

| Function | Can return `NULL`? | Can return empty string? |
|----------|-------------------|--------------------------|
| `get_app_version` | No | No (always has at least `"0.0.0"`) |
| `get_app_name` | No | No (always has at least the CMake project name) |

---

## ABI Stability Notes

These function pointers have been present since **ABI version 1** and
are guaranteed to remain at their current offsets in `MMCOContext`. The
`struct_size` / `abi_version` guards at the top of `MMCOContext` allow
the launcher to detect plugins compiled against older or newer headers.

If a future ABI version adds more utility functions, they will be
appended after `get_timestamp` and will not affect the offsets of
`get_app_version` or `get_app_name`.
