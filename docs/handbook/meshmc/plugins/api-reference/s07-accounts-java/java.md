# Java API — Full Function Reference

> All 6 Java management function pointers documented in detail.
> For an overview of the Java discovery mechanism, architecture strings,
> and key concepts, see [README.md](README.md).

---

## Table of Contents

- [Query Functions](#query-functions)
  - [`java_count()`](#java_count)
  - [`java_get_version()`](#java_get_version)
  - [`java_get_arch()`](#java_get_arch)
  - [`java_get_path()`](#java_get_path)
  - [`java_is_recommended()`](#java_is_recommended)
  - [`instance_get_java_version()`](#instance_get_java_version)
- [Practical Examples](#practical-examples)
  - [Java Installation Lister](#java-installation-lister)
  - [Java Compatibility Checker](#java-compatibility-checker)
  - [Minimum Java Version Enforcer](#minimum-java-version-enforcer)
  - [Architecture-Aware Java Selector](#architecture-aware-java-selector)
  - [Instance Java Audit Report](#instance-java-audit-report)

---

# Query Functions

All six Java functions are **read-only**. They inspect the `JavaInstallList`
maintained by MeshMC's application object. The list is populated by the
Java discovery system that runs at startup (see [README.md](README.md) for
details on how Java installations are discovered).

Every function takes `void* mh` (the module handle) as its first
parameter. Functions that address a specific Java installation take an
`int index` which must be in the range `[0, java_count() - 1]`. The
exception is `instance_get_java_version()`, which takes an instance ID
instead of a Java list index.

### Important: Java List Loading

The Java installation list is loaded **asynchronously** after MeshMC
starts. If `java_count()` is called before the discovery scan has
completed, it returns `0` even if Java installations exist on the system.

To ensure the list is available:

- Call these functions from **hook callbacks** (e.g. `meshmc.launch.pre`)
  rather than directly in `mmco_init()`.
- If you must query in `mmco_init()`, check the return of `java_count()`
  and handle `0` gracefully — it might just mean "not loaded yet".

The underlying check is `javalist()->isLoaded()`:

```cpp
if (!app || !app->javalist() || !app->javalist()->isLoaded())
    return 0;  // or nullptr
```

---

## `java_count()`

Returns the number of Java installations that MeshMC has discovered on
the system.

### Signature

```c
int (*java_count)(void* mh);
```

### Parameters

| Parameter | Type | Description |
|-----------|------|-------------|
| `mh` | `void*` | Module handle passed to `mmco_init()`. Identifies the calling plugin. Must not be `NULL`. |

### Return Value

| Condition | Return |
|-----------|--------|
| Success | Number of discovered Java installs (≥ 0). |
| Java list not yet loaded | `0` |
| Application pointer `NULL` | `0` |
| Java list `NULL` | `0` |
| `mh` is `NULL` | Undefined behavior (crash likely) |

### Thread Safety

Must be called from the **main (GUI) thread** only.

### Implementation Detail

```cpp
int PluginManager::api_java_count(void* mh)
{
    auto* r = rt(mh);
    auto* app = r->manager->m_app;
    if (!app || !app->javalist() || !app->javalist()->isLoaded())
        return 0;
    return app->javalist()->count();
}
```

The function checks `isLoaded()` before accessing the list. This prevents
returning stale or incomplete results if the Java scanner is still running.
`count()` returns the size of the internal version list, which is an O(1)
operation.

### What Gets Counted

Each unique Java binary path found by the scanner becomes one entry. If
the same physical installation is discovered through multiple scan paths
(e.g. both `/usr/bin/java` and `/usr/lib/jvm/java-21/bin/java` resolve
to the same binary), the deduplication depends on the scanner
implementation — in practice, each distinct path is a separate entry.

### Example

```c
int n = ctx->java_count(mh);
if (n == 0) {
    ctx->log(mh, 1, "No Java installations found (list may still be loading).");
} else {
    char msg[128];
    snprintf(msg, sizeof(msg), "Found %d Java installation(s).", n);
    ctx->log(mh, 0, msg);
}
```

---

## `java_get_version()`

Returns the version string of the Java installation at the given index.

### Signature

```c
const char* (*java_get_version)(void* mh, int index);
```

### Parameters

| Parameter | Type | Description |
|-----------|------|-------------|
| `mh` | `void*` | Module handle. Must not be `NULL`. |
| `index` | `int` | Zero-based index into the Java install list. Must be in range `[0, java_count() - 1]`. |

### Return Value

| Condition | Return |
|-----------|--------|
| Success | Null-terminated UTF-8 string containing the Java version (e.g. `"21.0.3"`, `"17.0.11"`, `"1.8.0_402"`). |
| Index out of range | `NULL` |
| Java list not loaded | `NULL` |
| Cast to `JavaInstall` fails | `NULL` |

### String Ownership

The returned pointer uses the per-module `tempString` buffer. It is valid
only until the **next** call to any `const char*`-returning API function
from the same module handle. Copy immediately:

```c
const char* tmp = ctx->java_get_version(mh, i);
char version[64];
snprintf(version, sizeof(version), "%s", tmp ? tmp : "");
```

### Version String Format

The version string is the output of `JavaVersion::toString()`, which
preserves the exact string that `java -XshowSettings:all` reported. The
format depends on the Java version:

| Java Era | Example String | Major | Minor | Security |
|----------|---------------|-------|-------|----------|
| Java 8 (legacy) | `"1.8.0_402"` | 8 | 0 | 402 |
| Java 9+ (modern) | `"17.0.11"` | 17 | 0 | 11 |
| Java 21 (LTS) | `"21.0.3"` | 21 | 0 | 3 |
| Early access | `"24-ea"` | 24 | 0 | 0 |

#### Parsing the Major Version

For most plugin use cases, you only need the **major** version number to
determine compatibility. A simple extraction:

```c
/**
 * Extract the major Java version number from a version string.
 * Handles both "1.8.x" (returns 8) and "17.x.y" (returns 17) formats.
 */
static int java_major_version(const char* ver) {
    if (!ver)
        return -1;

    /* Legacy format: "1.8.0_xxx" → major is the part after "1." */
    if (ver[0] == '1' && ver[1] == '.') {
        return atoi(ver + 2);
    }

    /* Modern format: "17.0.11" → major is the leading integer */
    return atoi(ver);
}
```

### Implementation Detail

```cpp
const char* PluginManager::api_java_get_version(void* mh, int idx)
{
    auto* r = rt(mh);
    auto* app = r->manager->m_app;
    if (!app || !app->javalist() || !app->javalist()->isLoaded())
        return nullptr;
    if (idx < 0 || idx >= app->javalist()->count())
        return nullptr;
    auto ver = std::dynamic_pointer_cast<JavaInstall>(
        app->javalist()->at(idx));
    if (!ver)
        return nullptr;
    r->tempString = ver->id.toString().toStdString();
    return r->tempString.c_str();
}
```

The `id` field on `JavaInstall` is a `JavaVersion` object. Its
`toString()` method returns the original version string as parsed from
the Java runtime output.

### Example

```c
int n = ctx->java_count(mh);
for (int i = 0; i < n; i++) {
    const char* tmp = ctx->java_get_version(mh, i);
    if (tmp) {
        int major = java_major_version(tmp);
        printf("Java [%d]: version %s (major: %d)\n", i, tmp, major);
    }
}
```

---

## `java_get_arch()`

Returns the CPU architecture string of the Java installation at the given
index.

### Signature

```c
const char* (*java_get_arch)(void* mh, int index);
```

### Parameters

| Parameter | Type | Description |
|-----------|------|-------------|
| `mh` | `void*` | Module handle. Must not be `NULL`. |
| `index` | `int` | Zero-based index into the Java install list. Must be in range `[0, java_count() - 1]`. |

### Return Value

| Condition | Return |
|-----------|--------|
| Success | Null-terminated string identifying the Java runtime's CPU architecture. |
| Index out of range | `NULL` |
| Java list not loaded | `NULL` |
| Cast to `JavaInstall` fails | `NULL` |

### Architecture String Values

The architecture string comes directly from the Java runtime's `os.arch`
system property. It is **not normalized** by MeshMC — the exact string
from the JVM is returned.

| Returned String | Platform | CPU Architecture |
|-----------------|----------|------------------|
| `"amd64"` | Linux, Windows | x86-64 (64-bit Intel/AMD) |
| `"x86_64"` | macOS | x86-64 (64-bit Intel) — same as `amd64` |
| `"aarch64"` | Linux, Windows | ARM 64-bit |
| `"arm64"` | macOS (Apple Silicon) | ARM 64-bit — same as `aarch64` |
| `"x86"` | Windows (32-bit JVM) | x86 32-bit (legacy) |
| `"arm"` | Linux (32-bit ARM) | ARM 32-bit (e.g. Raspberry Pi with 32-bit OS) |

#### Handling Aliases

Since different platforms use different names for the same architecture,
plugins should normalize when comparing:

```c
/**
 * Normalize a Java arch string to a canonical form.
 * Returns a static string — do not free.
 */
static const char* normalize_arch(const char* arch) {
    if (!arch) return "unknown";
    if (strcmp(arch, "amd64") == 0 || strcmp(arch, "x86_64") == 0)
        return "x86_64";
    if (strcmp(arch, "aarch64") == 0 || strcmp(arch, "arm64") == 0)
        return "aarch64";
    if (strcmp(arch, "x86") == 0 || strcmp(arch, "i386") == 0 ||
        strcmp(arch, "i686") == 0)
        return "x86";
    return arch;  /* return as-is for unknown architectures */
}
```

### String Ownership

Same `tempString` rules as all other string-returning functions. Copy
immediately before calling another API function.

### Implementation Detail

```cpp
const char* PluginManager::api_java_get_arch(void* mh, int idx)
{
    auto* r = rt(mh);
    auto* app = r->manager->m_app;
    if (!app || !app->javalist() || !app->javalist()->isLoaded())
        return nullptr;
    if (idx < 0 || idx >= app->javalist()->count())
        return nullptr;
    auto ver = std::dynamic_pointer_cast<JavaInstall>(
        app->javalist()->at(idx));
    if (!ver)
        return nullptr;
    r->tempString = ver->arch.toStdString();
    return r->tempString.c_str();
}
```

The `arch` field is a `QString` stored directly on the `JavaInstall`
struct, populated by the `JavaChecker` when it probes the Java binary.

### Example

```c
const char* tmp = ctx->java_get_arch(mh, i);
if (tmp) {
    const char* norm = normalize_arch(tmp);
    printf("Java [%d] architecture: %s (normalized: %s)\n", i, tmp, norm);
}
```

---

## `java_get_path()`

Returns the absolute filesystem path to the Java binary for the
installation at the given index.

### Signature

```c
const char* (*java_get_path)(void* mh, int index);
```

### Parameters

| Parameter | Type | Description |
|-----------|------|-------------|
| `mh` | `void*` | Module handle. Must not be `NULL`. |
| `index` | `int` | Zero-based index into the Java install list. Must be in range `[0, java_count() - 1]`. |

### Return Value

| Condition | Return |
|-----------|--------|
| Success | Null-terminated string containing the absolute path to the `java` (or `javaw.exe` on Windows) binary. |
| Index out of range | `NULL` |
| Java list not loaded | `NULL` |
| Cast to `JavaInstall` fails | `NULL` |

### Path Format

The path is an **absolute** native filesystem path:

| Platform | Example Path |
|----------|-------------|
| Linux | `"/usr/lib/jvm/java-21-openjdk-amd64/bin/java"` |
| Linux (MeshMC-managed) | `"/home/user/.local/share/MeshMC/java/java-21/bin/java"` |
| macOS | `"/Library/Java/JavaVirtualMachines/jdk-21.jdk/Contents/Home/bin/java"` |
| Windows | `"C:\\Program Files\\Eclipse Adoptium\\jdk-21.0.3\\bin\\javaw.exe"` |

The path points to the actual executable, not the `JAVA_HOME` directory.
On Linux/macOS this is typically `bin/java`; on Windows it may be
`bin/java.exe` or `bin/javaw.exe`.

### String Ownership

Standard `tempString` rules. Copy before your next API call:

```c
const char* tmp = ctx->java_get_path(mh, i);
char path[1024];
snprintf(path, sizeof(path), "%s", tmp ? tmp : "");
```

### Implementation Detail

```cpp
const char* PluginManager::api_java_get_path(void* mh, int idx)
{
    auto* r = rt(mh);
    auto* app = r->manager->m_app;
    if (!app || !app->javalist() || !app->javalist()->isLoaded())
        return nullptr;
    if (idx < 0 || idx >= app->javalist()->count())
        return nullptr;
    auto ver = std::dynamic_pointer_cast<JavaInstall>(
        app->javalist()->at(idx));
    if (!ver)
        return nullptr;
    r->tempString = ver->path.toStdString();
    return r->tempString.c_str();
}
```

The `path` field is a `QString` on the `JavaInstall` struct, set to the
absolute path that was discovered by the filesystem scanner or downloaded
by the Java downloader.

### Usage Notes

- The path may contain spaces (e.g. `C:\Program Files\...`). Always
  quote it when constructing shell commands.
- The binary at this path may have been removed since discovery. Use
  `fs_exists_abs()` to verify it still exists before relying on it.
- On macOS, the path goes through the `Contents/Home/` layer. This is
  the standard JDK layout on macOS.

### Example

```c
const char* tmp = ctx->java_get_path(mh, i);
if (tmp) {
    /* Check if the binary still exists */
    if (ctx->fs_exists_abs(mh, tmp)) {
        printf("Java binary found: %s\n", tmp);
    } else {
        printf("Java binary MISSING: %s\n", tmp);
    }
}
```

---

## `java_is_recommended()`

Returns whether MeshMC considers the Java installation at the given index
to be "recommended" for use.

### Signature

```c
int (*java_is_recommended)(void* mh, int index);
```

### Parameters

| Parameter | Type | Description |
|-----------|------|-------------|
| `mh` | `void*` | Module handle. Must not be `NULL`. |
| `index` | `int` | Zero-based index into the Java install list. Must be in range `[0, java_count() - 1]`. |

### Return Value

| Condition | Return |
|-----------|--------|
| Java install is recommended | `1` |
| Java install is not recommended | `0` |
| Index out of range | `0` |
| Java list not loaded | `0` |
| Cast to `JavaInstall` fails | `0` |

### What "Recommended" Means

MeshMC flags a Java installation as recommended when it meets all of the
following criteria:

1. **Architecture match**: The Java runtime's CPU architecture matches the
   host system (e.g. an `amd64` JVM on an `amd64` system). A 32-bit JVM
   on a 64-bit system is not recommended.

2. **Version appropriateness**: The Java version is suitable for modern
   Minecraft:
   - Java 17+ for Minecraft 1.17 through 1.20.4
   - Java 21+ for Minecraft 1.20.5+
   - Java 8 for Minecraft versions before 1.17

3. **Not a known-problematic build**: Certain JVM builds have known
   issues with Minecraft (e.g. some early-access builds or specific
   vendor distributions with graphical glitches).

The `recommended` flag is set once during the Java discovery scan and
does not change until the next scan. It is stored as a `bool` field on
the `JavaInstall` struct.

### Implementation Detail

```cpp
int PluginManager::api_java_is_recommended(void* mh, int idx)
{
    auto* r = rt(mh);
    auto* app = r->manager->m_app;
    if (!app || !app->javalist() || !app->javalist()->isLoaded())
        return 0;
    if (idx < 0 || idx >= app->javalist()->count())
        return 0;
    auto ver = std::dynamic_pointer_cast<JavaInstall>(
        app->javalist()->at(idx));
    return (ver && ver->recommended) ? 1 : 0;
}
```

### Example

```c
int n = ctx->java_count(mh);
int rec_count = 0;
for (int i = 0; i < n; i++) {
    if (ctx->java_is_recommended(mh, i)) {
        const char* tmp = ctx->java_get_version(mh, i);
        char ver[64];
        snprintf(ver, sizeof(ver), "%s", tmp ? tmp : "?");

        tmp = ctx->java_get_arch(mh, i);
        char arch[32];
        snprintf(arch, sizeof(arch), "%s", tmp ? tmp : "?");

        printf("  Recommended: Java %s (%s)\n", ver, arch);
        rec_count++;
    }
}
if (rec_count == 0) {
    printf("  No recommended Java installations found.\n");
}
```

---

## `instance_get_java_version()`

Returns the Java version that a specific instance is configured to use
(or requires). This is different from the Java *installation* query
functions — it tells you what Java version the **instance** expects,
not what's installed on the system.

### Signature

```c
const char* (*instance_get_java_version)(void* mh, const char* id);
```

### Parameters

| Parameter | Type | Description |
|-----------|------|-------------|
| `mh` | `void*` | Module handle. Must not be `NULL`. |
| `id` | `const char*` | Null-terminated instance ID string (as returned by `instance_get_id()` from [S03](../s03-instances/)). Must identify a valid Minecraft instance. |

### Return Value

| Condition | Return |
|-----------|--------|
| Success | Null-terminated string containing the Java version required by the instance (e.g. `"17"`, `"21.0.3"`, `"1.8.0"`). |
| Instance not found | `NULL` |
| Instance is not a MinecraftInstance | `NULL` |
| `id` is `NULL` | `NULL` |

### How the Java Version Is Determined

The instance's Java version comes from `MinecraftInstance::getJavaVersion()`,
which returns a `JavaVersion` object. This is determined by:

1. **Instance settings override**: If the user has explicitly configured a
   Java version for this instance in the instance settings, that is used.
2. **Component pack requirement**: The instance's component system (loader
   metadata) specifies a minimum Java version. For example:
   - Minecraft 1.16.5 requires Java 8 (`"1.8.0"`)
   - Minecraft 1.17+ requires Java 17 (`"17"`)
   - Minecraft 1.20.5+ requires Java 21 (`"21"`)

The returned string is `JavaVersion::toString()`.

### String Ownership

Standard `tempString` rules. Copy before your next API call.

### Implementation Detail

```cpp
const char* PluginManager::api_instance_get_java_version(void* mh,
                                                          const char* id)
{
    auto* r = rt(mh);
    auto* mc = resolveMC(r, id);
    if (!mc)
        return nullptr;
    r->tempString = mc->getJavaVersion().toString().toStdString();
    return r->tempString.c_str();
}
```

The `resolveMC()` helper looks up the instance by ID in the instance
list and downcasts it to `MinecraftInstance*`. If the instance doesn't
exist or isn't a Minecraft instance, `NULL` is returned.

### Relationship to Java Install Functions

This function answers *"what Java does this instance need?"* while
`java_get_version()` answers *"what Java is installed on this system?"*.
A typical plugin workflow is:

1. Get the required Java version from `instance_get_java_version()`
2. Iterate Java installs with `java_count()` / `java_get_version()`
3. Find an installation whose major version is ≥ the required major
4. Optionally prefer recommended installs (`java_is_recommended()`)

### Example

```c
void check_instance_java(MMCOContext* ctx, void* mh, const char* inst_id) {
    const char* tmp = ctx->instance_get_java_version(mh, inst_id);
    if (!tmp) {
        printf("Could not determine Java requirement.\n");
        return;
    }

    char required[64];
    snprintf(required, sizeof(required), "%s", tmp);
    int req_major = java_major_version(required);

    printf("Instance requires Java %s (major: %d)\n", required, req_major);

    int best = -1;
    int n = ctx->java_count(mh);
    for (int i = 0; i < n; i++) {
        tmp = ctx->java_get_version(mh, i);
        if (!tmp) continue;
        int major = java_major_version(tmp);
        if (major >= req_major) {
            if (best < 0 || ctx->java_is_recommended(mh, i)) {
                best = i;
            }
        }
    }

    if (best >= 0) {
        tmp = ctx->java_get_path(mh, best);
        printf("Best match: %s\n", tmp ? tmp : "(path unavailable)");
    } else {
        printf("No compatible Java found!\n");
    }
}
```

---

# Practical Examples

## Java Installation Lister

A complete plugin that lists all discovered Java installations in a tree
widget, showing version, architecture, path, and recommended status:

```c
#include <mmco_sdk.h>
#include <stdio.h>
#include <string.h>

static MMCOContext* ctx;
static void* mh;
static void* tree;

static void refresh_list(void* ud) {
    (void)ud;
    ctx->ui_tree_clear(mh, tree);

    int n = ctx->java_count(mh);
    for (int i = 0; i < n; i++) {
        const char* tmp;
        char ver[64], arch[32], path[1024], rec[16];

        tmp = ctx->java_get_version(mh, i);
        snprintf(ver, sizeof(ver), "%s", tmp ? tmp : "?");

        tmp = ctx->java_get_arch(mh, i);
        snprintf(arch, sizeof(arch), "%s", tmp ? tmp : "?");

        tmp = ctx->java_get_path(mh, i);
        snprintf(path, sizeof(path), "%s", tmp ? tmp : "?");

        snprintf(rec, sizeof(rec), "%s",
                 ctx->java_is_recommended(mh, i) ? "Yes" : "No");

        const char* row[] = { ver, arch, path, rec };
        ctx->ui_tree_add_row(mh, tree, row, 4);
    }
}

MMCO_EXPORT int mmco_init(void* handle, MMCOContext* context) {
    mh = handle;
    ctx = context;

    const char* cols[] = { "Version", "Arch", "Path", "Recommended" };
    void* page = ctx->ui_page_create(mh, "java_list",
                                     "Java Installations", "java");
    void* layout = ctx->ui_layout_create(mh, page, 0);  /* vertical */
    tree = ctx->ui_tree_create(mh, page, cols, 4, NULL, NULL);
    void* btn = ctx->ui_button_create(mh, page, "Refresh", NULL,
                                      refresh_list, NULL);
    ctx->ui_layout_add_widget(mh, layout, tree);
    ctx->ui_layout_add_widget(mh, layout, btn);
    ctx->ui_page_set_layout(mh, page, layout);

    refresh_list(NULL);
    return 0;
}

MMCO_EXPORT void mmco_unload(void* handle) {
    (void)handle;
}
```

---

## Java Compatibility Checker

A pre-launch hook that verifies the instance has a compatible Java
installation available, and warns the user if not:

```c
#include <mmco_sdk.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

static MMCOContext* ctx;
static void* mh;

static int java_major_version(const char* ver) {
    if (!ver) return -1;
    if (ver[0] == '1' && ver[1] == '.')
        return atoi(ver + 2);
    return atoi(ver);
}

static int on_hook(void* ud, const char* hook, const void* payload) {
    (void)ud;
    if (strcmp(hook, "meshmc.launch.pre") != 0)
        return 0;

    /* payload is the instance ID for launch hooks */
    const char* inst_id = (const char*)payload;
    if (!inst_id)
        return 0;

    /* What Java does this instance need? */
    const char* tmp = ctx->instance_get_java_version(mh, inst_id);
    if (!tmp)
        return 0;  /* can't determine — let launcher handle it */

    char required[64];
    snprintf(required, sizeof(required), "%s", tmp);
    int req_major = java_major_version(required);
    if (req_major <= 0)
        return 0;

    /* Search for a compatible installation */
    int n = ctx->java_count(mh);
    int found_any = 0;
    int found_recommended = 0;

    for (int i = 0; i < n; i++) {
        tmp = ctx->java_get_version(mh, i);
        if (!tmp) continue;
        int major = java_major_version(tmp);
        if (major >= req_major) {
            found_any = 1;
            if (ctx->java_is_recommended(mh, i))
                found_recommended = 1;
        }
    }

    if (!found_any) {
        char msg[512];
        snprintf(msg, sizeof(msg),
            "This instance requires Java %d or newer, but no compatible "
            "Java installation was found on your system.\n\n"
            "Please install Java %d and restart MeshMC.",
            req_major, req_major);
        ctx->ui_show_message(mh, 2, "Java Not Found", msg);
        /* Return non-zero to abort the launch */
        return -1;
    }

    if (!found_recommended) {
        char msg[512];
        snprintf(msg, sizeof(msg),
            "A Java %d+ installation was found, but it is not marked as "
            "recommended. Performance or compatibility issues may occur.",
            req_major);
        ctx->ui_show_message(mh, 1, "Java Warning", msg);
        /* Return 0 to allow launch to proceed */
    }

    return 0;
}

MMCO_EXPORT int mmco_init(void* handle, MMCOContext* context) {
    mh = handle;
    ctx = context;
    ctx->hook_register(mh, "meshmc.launch.pre", on_hook, NULL);
    return 0;
}

MMCO_EXPORT void mmco_unload(void* handle) {
    (void)handle;
}
```

---

## Minimum Java Version Enforcer

A plugin that checks all instances and reports which ones have outdated
Java requirements compared to a plugin-configured minimum:

```c
#include <mmco_sdk.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static MMCOContext* ctx;
static void* mh;

#define MIN_JAVA_MAJOR 17  /* Minimum acceptable Java version */

static int java_major_version(const char* ver) {
    if (!ver) return -1;
    if (ver[0] == '1' && ver[1] == '.')
        return atoi(ver + 2);
    return atoi(ver);
}

static void audit_instances(void* ud) {
    (void)ud;

    int n = ctx->instance_count(mh);
    int issues = 0;

    for (int i = 0; i < n; i++) {
        const char* tmp = ctx->instance_get_id(mh, i);
        if (!tmp) continue;
        char id[128];
        snprintf(id, sizeof(id), "%s", tmp);

        tmp = ctx->instance_get_name(mh, i);
        char name[256];
        snprintf(name, sizeof(name), "%s", tmp ? tmp : "(unnamed)");

        tmp = ctx->instance_get_java_version(mh, id);
        if (!tmp) continue;

        int major = java_major_version(tmp);
        if (major > 0 && major < MIN_JAVA_MAJOR) {
            char msg[512];
            snprintf(msg, sizeof(msg),
                "Instance '%s' uses Java %d (minimum: %d)",
                name, major, MIN_JAVA_MAJOR);
            ctx->log(mh, 1, msg);
            issues++;
        }
    }

    if (issues == 0) {
        ctx->log(mh, 0, "All instances meet minimum Java requirements.");
    } else {
        char summary[256];
        snprintf(summary, sizeof(summary),
            "%d instance(s) below minimum Java %d.", issues, MIN_JAVA_MAJOR);
        ctx->ui_show_message(mh, 1, "Java Audit", summary);
    }
}

MMCO_EXPORT int mmco_init(void* handle, MMCOContext* context) {
    mh = handle;
    ctx = context;
    /* Run audit after a brief delay or on user request */
    return 0;
}
```

---

## Architecture-Aware Java Selector

Finding the best Java install that matches a target architecture, useful
for plugins that manage cross-architecture instances:

```c
#include <mmco_sdk.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

static MMCOContext* ctx;
static void* mh;

static const char* normalize_arch(const char* arch) {
    if (!arch) return "unknown";
    if (strcmp(arch, "amd64") == 0 || strcmp(arch, "x86_64") == 0)
        return "x86_64";
    if (strcmp(arch, "aarch64") == 0 || strcmp(arch, "arm64") == 0)
        return "aarch64";
    if (strcmp(arch, "x86") == 0 || strcmp(arch, "i386") == 0 ||
        strcmp(arch, "i686") == 0)
        return "x86";
    return arch;
}

static int java_major_version(const char* ver) {
    if (!ver) return -1;
    if (ver[0] == '1' && ver[1] == '.')
        return atoi(ver + 2);
    return atoi(ver);
}

/**
 * Find the best Java installation matching the given architecture
 * and minimum major version. Returns the index, or -1 if not found.
 */
static int find_best_java(const char* target_arch, int min_major) {
    const char* norm_target = normalize_arch(target_arch);
    int best = -1;
    int best_major = -1;

    int n = ctx->java_count(mh);
    for (int i = 0; i < n; i++) {
        /* Check architecture */
        const char* tmp = ctx->java_get_arch(mh, i);
        if (!tmp) continue;
        char arch[32];
        snprintf(arch, sizeof(arch), "%s", tmp);
        if (strcmp(normalize_arch(arch), norm_target) != 0)
            continue;

        /* Check version */
        tmp = ctx->java_get_version(mh, i);
        if (!tmp) continue;
        int major = java_major_version(tmp);
        if (major < min_major)
            continue;

        /* Prefer recommended, then highest version */
        int rec = ctx->java_is_recommended(mh, i);
        if (best < 0 || rec || major > best_major) {
            best = i;
            best_major = major;
            if (rec) break;  /* recommended is always best */
        }
    }

    return best;
}

/* Usage: */
void example_usage(void) {
    int idx = find_best_java("amd64", 21);
    if (idx >= 0) {
        const char* tmp = ctx->java_get_path(mh, idx);
        printf("Best x86_64 Java 21+: %s\n", tmp ? tmp : "?");
    } else {
        printf("No compatible Java found for x86_64 with Java 21+\n");
    }
}
```

---

## Instance Java Audit Report

A comprehensive example combining both account and Java APIs to produce a
full system report:

```c
#include <mmco_sdk.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static MMCOContext* ctx;
static void* mh;

static int java_major_version(const char* ver) {
    if (!ver) return -1;
    if (ver[0] == '1' && ver[1] == '.')
        return atoi(ver + 2);
    return atoi(ver);
}

static void generate_report(void* ud) {
    (void)ud;
    char line[1024];

    /* --- Account Section --- */
    int acct_n = ctx->account_count(mh);
    int def = ctx->account_get_default_index(mh);

    snprintf(line, sizeof(line), "=== System Report ===");
    ctx->log(mh, 0, line);

    snprintf(line, sizeof(line), "Accounts: %d (default: %d)", acct_n, def);
    ctx->log(mh, 0, line);

    for (int i = 0; i < acct_n; i++) {
        const char* tmp = ctx->account_get_profile_name(mh, i);
        char name[256];
        snprintf(name, sizeof(name), "%s", tmp ? tmp : "?");

        int state = ctx->account_get_state(mh, i);

        snprintf(line, sizeof(line), "  Account[%d]: %s (state=%d%s)",
                 i, name, state, (i == def) ? ", DEFAULT" : "");
        ctx->log(mh, 0, line);
    }

    /* --- Java Section --- */
    int java_n = ctx->java_count(mh);
    snprintf(line, sizeof(line), "Java Installations: %d", java_n);
    ctx->log(mh, 0, line);

    for (int i = 0; i < java_n; i++) {
        const char* tmp = ctx->java_get_version(mh, i);
        char ver[64];
        snprintf(ver, sizeof(ver), "%s", tmp ? tmp : "?");

        tmp = ctx->java_get_arch(mh, i);
        char arch[32];
        snprintf(arch, sizeof(arch), "%s", tmp ? tmp : "?");

        tmp = ctx->java_get_path(mh, i);
        char path[512];
        snprintf(path, sizeof(path), "%s", tmp ? tmp : "?");

        int rec = ctx->java_is_recommended(mh, i);

        snprintf(line, sizeof(line),
                 "  Java[%d]: %s (%s) %s\n    Path: %s",
                 i, ver, arch, rec ? "[REC]" : "", path);
        ctx->log(mh, 0, line);
    }

    /* --- Instance Java Requirements --- */
    int inst_n = ctx->instance_count(mh);
    snprintf(line, sizeof(line), "Instance Java Requirements:");
    ctx->log(mh, 0, line);

    for (int i = 0; i < inst_n; i++) {
        const char* tmp = ctx->instance_get_id(mh, i);
        if (!tmp) continue;
        char id[128];
        snprintf(id, sizeof(id), "%s", tmp);

        tmp = ctx->instance_get_name(mh, i);
        char name[256];
        snprintf(name, sizeof(name), "%s", tmp ? tmp : "(unnamed)");

        tmp = ctx->instance_get_java_version(mh, id);
        char req[64];
        snprintf(req, sizeof(req), "%s", tmp ? tmp : "(unknown)");

        int req_major = java_major_version(req);

        /* Check if we have a compatible Java */
        int compatible = 0;
        for (int j = 0; j < java_n; j++) {
            tmp = ctx->java_get_version(mh, j);
            if (tmp && java_major_version(tmp) >= req_major) {
                compatible = 1;
                break;
            }
        }

        snprintf(line, sizeof(line), "  %s: requires Java %s %s",
                 name, req, compatible ? "[OK]" : "[MISSING]");
        ctx->log(mh, 0, line);
    }

    ctx->log(mh, 0, "=== End Report ===");
}

MMCO_EXPORT int mmco_init(void* handle, MMCOContext* context) {
    mh = handle;
    ctx = context;
    /* Could register a menu item or button to trigger the report */
    return 0;
}

MMCO_EXPORT void mmco_unload(void* handle) {
    (void)handle;
}
```

---

## Error Handling Summary

All Java functions follow a consistent error pattern:

| Error Condition | `java_count` | `java_get_version` | `java_get_arch` | `java_get_path` | `java_is_recommended` | `instance_get_java_version` |
|----------------|-------------|--------------------|-----------------|-----------------|-----------------------|-----------------------------|
| `mh` is `NULL` | UB | UB | UB | UB | UB | UB |
| App is `NULL` | `0` | `NULL` | `NULL` | `NULL` | `0` | `NULL` |
| Java list `NULL` | `0` | `NULL` | `NULL` | `NULL` | `0` | N/A |
| Java list not loaded | `0` | `NULL` | `NULL` | `NULL` | `0` | N/A |
| Index out of range | N/A | `NULL` | `NULL` | `NULL` | `0` | N/A |
| `dynamic_pointer_cast` fails | N/A | `NULL` | `NULL` | `NULL` | `0` | N/A |
| Instance not found | N/A | N/A | N/A | N/A | N/A | `NULL` |
| `id` is `NULL` | N/A | N/A | N/A | N/A | N/A | `NULL` |

"UB" = Undefined Behavior. The module handle should never be `NULL`.

---

## Minecraft Java Version Requirements

For reference, here are the Java version requirements for different
Minecraft releases, which determine what `instance_get_java_version()`
returns:

| Minecraft Version | Required Java | Major Version |
|-------------------|---------------|---------------|
| 1.0 – 1.16.5 | Java 8 | 8 |
| 1.17 – 1.17.1 | Java 16 | 16 |
| 1.18 – 1.20.4 | Java 17 | 17 |
| 1.20.5+ | Java 21 | 21 |

These requirements are enforced by the Minecraft launcher metadata and
are reflected in the instance's component pack.

---

## Related Sections

| Section | Relationship |
|---------|-------------|
| [S01 — Lifecycle](../s01-lifecycle/) | `tempString` ownership, `mmco_init()` entry point |
| [S03 — Instances](../s03-instances/) | `instance_get_id()` for use with `instance_get_java_version()` |
| [S07 — Accounts](accounts.md) | Account functions — the other half of this section |
| [S10 — Filesystem](../s10-filesystem/) | `fs_exists_abs()` to verify Java paths |
| [S12 — UI Dialogs](../s12-ui-dialogs/) | Showing Java warnings in dialogs |
