# Environment Variables — `launch_set_env()` {#set-env}

> **Section:** S15 (Launch Modifiers) · **Header:** `PluginAPI.h` · **Trampoline:** `PluginManager::api_launch_set_env` · **Backend:** `QMap<QString, QString>`, `qputenv()`, `qunsetenv()`

---

## `launch_set_env()` — Inject an environment variable into the JVM process {#launch_set_env}

### Synopsis

```c
int launch_set_env(void* mh, const char* key, const char* value);
```

Sets an environment variable that will be present in the child JVM
process's environment when the instance launches. The variable is
injected via `qputenv()` immediately after the `INSTANCE_PRE_LAUNCH`
hook dispatch completes and is automatically removed via
`qunsetenv()` after the instance exits.

**This function may only be called inside an `INSTANCE_PRE_LAUNCH`
hook callback (hook ID `0x0200`).** Calling it at any other time
results in undefined behavior.

### Parameters

| # | Name | Type | Description |
|---|------|------|-------------|
| 1 | `mh` | `void*` | Module handle. Pass `ctx->module_handle`. |
| 2 | `key` | `const char*` | Environment variable name. Must be non-null. UTF-8 encoded, null-terminated. |
| 3 | `value` | `const char*` | Environment variable value. Must be non-null. UTF-8 encoded, null-terminated. |

#### Parameter Constraints

- **`key`**: Must not be `nullptr`. There is no restriction on the
  key format beyond what the operating system supports. On POSIX
  systems, keys are case-sensitive and conventionally use uppercase
  with underscores (e.g. `MY_PLUGIN_VAR`). On Windows, keys are
  case-insensitive.
- **`value`**: Must not be `nullptr`. May be an empty string (`""`),
  which sets the variable to an empty value (distinct from unsetting
  it). There is no length limit enforced by the API.
- **`key` and `value`**: The API copies both strings internally
  (via `QString::fromUtf8()`). The caller retains ownership and may
  free or modify the strings immediately after the call returns.

### Return Value

| Condition | Value |
|-----------|-------|
| Success | `0` |
| `key` is `nullptr` | `-1` |
| `value` is `nullptr` | `-1` |

### Implementation

```cpp
int PluginManager::api_launch_set_env(void* mh, const char* key,
                                      const char* value)
{
    auto* r = rt(mh);
    if (!key || !value)
        return -1;
    r->manager->m_pendingLaunchEnv.insert(QString::fromUtf8(key),
                                          QString::fromUtf8(value));
    return 0;
}
```

The implementation:
1. Resolves the `ModuleRuntime*` from the opaque module handle.
2. Null-checks both `key` and `value`. Returns `-1` if either is null.
3. Converts both to `QString` using `fromUtf8()`.
4. Inserts the key-value pair into the shared
   `PluginManager::m_pendingLaunchEnv` map (a `QMap<QString, QString>`).
5. Returns `0` on success.

### Storage Behavior

The `m_pendingLaunchEnv` map is a **shared, global** map — it is not
per-module. This has important implications:

| Behavior | Description |
|----------|-------------|
| **Duplicate keys** | If the same key is set by multiple plugins (or multiple times by the same plugin), the last value wins. `QMap::insert()` replaces existing values for the same key. |
| **Cleared before each launch** | `clearPendingLaunchMods()` is called before each `INSTANCE_PRE_LAUNCH` dispatch. Variables from a previous launch never leak into the next one. |
| **Consumed after dispatch** | After the hook dispatch, `takePendingLaunchEnv()` moves the map out (swap semantics). The `PluginManager` no longer holds a reference. |

### How the Variable Is Applied

After the `INSTANCE_PRE_LAUNCH` hook dispatch completes,
`LaunchController` applies the pending environment variables:

```cpp
auto pendingEnv = APPLICATION->pluginManager()->takePendingLaunchEnv();
for (auto it = pendingEnv.constBegin(); it != pendingEnv.constEnd();
     ++it) {
    qputenv(it.key().toUtf8().constData(), it.value().toUtf8());
}
```

This calls `qputenv()` for each key-value pair. `qputenv()` is Qt's
cross-platform wrapper around `setenv()` (POSIX) or
`SetEnvironmentVariable()` (Windows).

### How the Variable Is Cleaned Up

After the instance process exits (the `Task::finished` signal fires),
the environment variables are removed:

```cpp
connect(m_launcher.get(), &Task::finished, this,
        [pendingEnv, ...]() {
            for (auto it = pendingEnv.constBegin();
                 it != pendingEnv.constEnd(); ++it) {
                qunsetenv(it.key().toUtf8().constData());
            }
        });
```

This ensures that plugin-injected environment variables do not persist
in the launcher process after the instance exits.

### String Ownership

This function does **not** use `ModuleRuntime::tempString`. It copies
both `key` and `value` into `QString` objects stored in
`m_pendingLaunchEnv`. The caller retains full ownership of the input
strings.

| Property | Value |
|----------|-------|
| Backing storage | `PluginManager::m_pendingLaunchEnv` (shared `QMap`) |
| Input strings copied? | Yes — via `QString::fromUtf8()` |
| `tempString` affected? | No |
| Caller must free inputs? | Caller retains ownership, frees as needed. |

---

## Use Cases

### Setting JVM Arguments

```c
static int on_pre_launch(void* mh, int hook_id, void* payload)
{
    (void)hook_id;
    (void)payload;

    /* Set maximum heap size to 4 GB */
    g_ctx->launch_set_env(mh, "_JAVA_OPTIONS", "-Xmx4G -Xms2G");

    return 0;
}
```

> **Warning:** `_JAVA_OPTIONS` is picked up by all JVM invocations
> in the process tree. Prefer instance-specific JVM argument settings
> when available.

### Custom Plugin Detection Variable

A common pattern is for a plugin to signal its presence to a
companion Minecraft mod running inside the JVM:

```c
static int on_pre_launch(void* mh, int hook_id, void* payload)
{
    (void)hook_id;
    MMCOInstanceInfo* info = (MMCOInstanceInfo*)payload;

    /* Let the companion mod know this plugin is active */
    g_ctx->launch_set_env(mh, "MESHMC_PLUGIN_MYMOD", "1");

    /* Pass the instance ID for mod-side correlation */
    g_ctx->launch_set_env(mh, "MESHMC_INSTANCE_ID",
                          info->instance_id);

    return 0;
}
```

### Debug Flags for Specific Instances

Conditionally enable debugging based on instance name:

```c
#include <string.h>

static int on_pre_launch(void* mh, int hook_id, void* payload)
{
    (void)hook_id;
    MMCOInstanceInfo* info = (MMCOInstanceInfo*)payload;

    /* Enable debug logging only for the "Dev" instance */
    if (strstr(info->instance_name, "Dev") != NULL) {
        g_ctx->launch_set_env(mh, "MESHMC_DEBUG", "1");
        g_ctx->launch_set_env(mh, "MESHMC_DEBUG_LEVEL", "TRACE");
        g_ctx->log_info(mh, "Debug mode enabled for Dev instance");
    }

    return 0;
}
```

### Reading Settings to Determine Environment Variables

Combining S2 (Settings) with S15 to let users configure environment
variables through the plugin's settings:

```c
static int on_pre_launch(void* mh, int hook_id, void* payload)
{
    (void)hook_id;
    (void)payload;

    /* Read user-configured env var from plugin settings */
    const char* custom_var = g_ctx->setting_get(mh, "custom_env_key");
    if (custom_var && custom_var[0] != '\0') {
        char key[128];
        snprintf(key, sizeof(key), "%s", custom_var);

        const char* custom_val = g_ctx->setting_get(mh, "custom_env_value");
        if (custom_val) {
            g_ctx->launch_set_env(mh, key, custom_val);

            char buf[256];
            snprintf(buf, sizeof(buf), "Set env: %s=%s", key, custom_val);
            g_ctx->log_info(mh, buf);
        }
    }

    return 0;
}
```

### Setting Multiple Environment Variables

```c
typedef struct {
    const char* key;
    const char* value;
} EnvVar;

static const EnvVar g_profiler_vars[] = {
    { "JAVA_TOOL_OPTIONS", "-agentpath:/opt/profiler/libprofiler.so" },
    { "PROFILER_OUTPUT",   "/tmp/meshmc-profile.jfr" },
    { "PROFILER_EVENTS",   "cpu,alloc,lock" },
    { NULL, NULL }
};

static int on_pre_launch(void* mh, int hook_id, void* payload)
{
    (void)hook_id;
    (void)payload;

    for (const EnvVar* ev = g_profiler_vars; ev->key; ++ev) {
        int rc = g_ctx->launch_set_env(mh, ev->key, ev->value);
        if (rc != 0) {
            char buf[128];
            snprintf(buf, sizeof(buf), "Failed to set env: %s", ev->key);
            g_ctx->log_error(mh, buf);
        }
    }

    return 0;
}
```

---

## Error Conditions

| Condition | Return value | Behavior |
|-----------|-------------|----------|
| `key` is `nullptr` | `-1` | No side effects. |
| `value` is `nullptr` | `-1` | No side effects. |
| `key` is empty string `""` | `0` | Sets an env var with an empty name (OS-dependent behavior, not recommended). |
| `value` is empty string `""` | `0` | Sets the env var to an empty value (valid). |
| Same key set multiple times | `0` | Last value wins. |
| Called outside `PRE_LAUNCH` hook | `0` | Undefined behavior — modifications may be discarded or applied to the wrong launch. |

---

## Process-Wide Side Effects

> **Important:** `qputenv()` modifies the environment of the
> **launcher process itself**, not just the child JVM.

This means:

1. **Other threads** in MeshMC can see the injected variables via
   `qgetenv()` or `getenv()` while the instance is running.
2. **Concurrent launches** may see each other's environment
   variables if two instances are launched simultaneously.
3. The variables are **removed after the instance exits**, but
   during the instance lifetime they are globally visible.

This is a fundamental limitation of the POSIX `setenv()` / Windows
`SetEnvironmentVariable()` APIs. The practical risk is low for most
plugins, but be aware of it if you set variables that could conflict
with other launcher subsystems.

---

## Cross-References

| Section | Relationship |
|---------|-------------|
| **S1 (Lifecycle)** | `hook_register()` is used to register the `INSTANCE_PRE_LAUNCH` callback where `launch_set_env()` is called. |
| **S2 (Settings)** | `setting_get()` can read user-configured env var names/values to pass to `launch_set_env()`. |
| **S3 (Instances)** | The `MMCOInstanceInfo*` payload provides `instance_id` for conditional env var logic. |
| **S15 (Launch Modifiers)** | `launch_prepend_wrapper()` is the companion function for modifying the launch command line. |
| **S16 (Application Settings)** | `app_setting_get()` can read global settings to conditionally apply env vars. |
