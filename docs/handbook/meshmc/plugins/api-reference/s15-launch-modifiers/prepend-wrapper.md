# Wrapper Commands — `launch_prepend_wrapper()` {#prepend-wrapper}

> **Section:** S15 (Launch Modifiers) · **Header:** `PluginAPI.h` · **Trampoline:** `PluginManager::api_launch_prepend_wrapper` · **Backend:** `QString`, transient `LaunchTask` wrapper state

---

## `launch_prepend_wrapper()` — Prepend a wrapper command to the launch sequence {#launch_prepend_wrapper}

### Synopsis

```c
int launch_prepend_wrapper(void* mh, const char* wrapper_cmd);
```

Prepends a wrapper command to the Minecraft instance's launch command
line. The wrapper is placed **before** any existing user-configured
wrapper command and **before** the Java executable. The prepend only
applies to the current launch task; the instance settings are not
modified.

**This function may only be called inside an `INSTANCE_PRE_LAUNCH`
hook callback (hook ID `0x0200`).** Calling it at any other time
results in undefined behavior.

### Parameters

| # | Name | Type | Description |
|---|------|------|-------------|
| 1 | `mh` | `void*` | Module handle. Pass `ctx->module_handle`. |
| 2 | `wrapper_cmd` | `const char*` | The wrapper command to prepend. Must be non-null and non-empty. UTF-8 encoded, null-terminated. |

#### Parameter Constraints

- **`wrapper_cmd`**: Must not be `nullptr` and must not be an empty
  string (`""`). The API does **not** validate that the command is a
  valid executable or exists on `PATH`. The command string may
  include arguments (e.g. `"strace -f -e trace=network"`).
- The API copies the string internally (via `QString::fromUtf8()`).
  The caller retains ownership and may free or modify the string
  immediately after the call returns.

### Return Value

| Condition | Value |
|-----------|-------|
| Success | `0` |
| `wrapper_cmd` is `nullptr` | `-1` |
| `wrapper_cmd` is empty string `""` | `-1` |

### Implementation

```cpp
int PluginManager::api_launch_prepend_wrapper(void* mh,
                                              const char* wrapper_cmd)
{
    auto* r = rt(mh);
    if (!wrapper_cmd || wrapper_cmd[0] == '\0')
        return -1;
    QString cmd = QString::fromUtf8(wrapper_cmd);
    if (r->manager->m_pendingLaunchWrapper.isEmpty()) {
        r->manager->m_pendingLaunchWrapper = cmd;
    } else {
        r->manager->m_pendingLaunchWrapper =
            cmd + " " + r->manager->m_pendingLaunchWrapper;
    }
    return 0;
}
```

The implementation:
1. Resolves the `ModuleRuntime*` from the opaque module handle.
2. Checks that `wrapper_cmd` is non-null and non-empty. Returns `-1`
   on either condition.
3. Converts the command to `QString` using `fromUtf8()`.
4. If no pending wrapper exists yet, sets it directly.
5. If a pending wrapper already exists (from a previous call or
   another plugin), **prepends** the new command with a space
   separator. This means the new command wraps around the existing
   wrapper chain.
6. Returns `0` on success.

### Chaining Behavior

When multiple `launch_prepend_wrapper()` calls are made (either by
the same plugin or by different plugins), the wrappers are chained.
Each new wrapper is prepended **before** the existing chain:

```text
Call 1:  launch_prepend_wrapper(mh, "strace -f")
         → m_pendingLaunchWrapper = "strace -f"

Call 2:  launch_prepend_wrapper(mh, "timeout 3600")
         → m_pendingLaunchWrapper = "timeout 3600 strace -f"

Call 3:  launch_prepend_wrapper(mh, "nice -n 10")
         → m_pendingLaunchWrapper = "nice -n 10 timeout 3600 strace -f"
```

The final launch command line becomes:

```text
nice -n 10 timeout 3600 strace -f [user-configured-wrapper] java [args...]
```

### How the Wrapper Is Applied

After the `INSTANCE_PRE_LAUNCH` hook dispatch completes,
`LaunchController` merges the pending wrapper with the instance's
configured wrapper and stores the effective chain on the current
`LaunchTask`:

```cpp
QString pendingWrapper =
    APPLICATION->pluginManager()->takePendingLaunchWrapper();
if (!pendingWrapper.isEmpty()) {
    auto wrapperCommand = m_instance->getWrapperCommand().trimmed();
    if (wrapperCommand.isEmpty()) {
        m_launcher->setWrapperCommand(pendingWrapper);
    } else {
        m_launcher->setWrapperCommand(
            pendingWrapper + " " + wrapperCommand);
    }
}
```

The logic:
1. Takes the pending wrapper string from the `PluginManager` (swap
   semantics — the manager's copy is cleared).
2. Reads the instance's configured `WrapperCommand` through the normal
   settings API.
3. If the instance has no existing wrapper, stores the pending wrapper
   directly on the current `LaunchTask`.
4. If the instance already has a wrapper, concatenates the pending
   wrapper **before** the existing one with a space separator and stores
   the result on the current `LaunchTask`.

The launch steps then read `LaunchTask::wrapperCommand()` and apply that
effective chain when spawning Java.

### Why No Restore Step Is Needed

The effective wrapper now lives only on the `LaunchTask`. When the task
finishes, that transient state is discarded with the task object. The
user's configured `WrapperCommand` setting is never rewritten, so there
is nothing to restore after the process exits.

### String Ownership

This function does **not** use `ModuleRuntime::tempString`. It copies
the input string into a `QString` stored in
`m_pendingLaunchWrapper`. The caller retains full ownership of the
input string.

| Property | Value |
|----------|-------|
| Backing storage | `PluginManager::m_pendingLaunchWrapper` (shared `QString`) |
| Input string copied? | Yes — via `QString::fromUtf8()` |
| `tempString` affected? | No |
| Caller must free input? | Caller retains ownership. |

---

## Use Cases

### Debugging with `strace`

Trace all system calls made by the Minecraft process:

```c
static int on_pre_launch(void* mh, int hook_id, void* payload)
{
    (void)hook_id;
    (void)payload;

    int rc = g_ctx->launch_prepend_wrapper(mh,
        "strace -f -o /tmp/minecraft-trace.log");
    if (rc != 0) {
        g_ctx->log_error(mh, "Failed to prepend strace wrapper");
    }

    return 0;
}
```

### Profiling with `perf`

Record CPU performance data for later analysis:

```c
static int on_pre_launch(void* mh, int hook_id, void* payload)
{
    (void)hook_id;
    (void)payload;

    g_ctx->launch_prepend_wrapper(mh,
        "perf record -g -o /tmp/minecraft-perf.data");

    return 0;
}
```

### Process Priority with `nice`

Lower the Minecraft process priority to reduce system impact:

```c
static int on_pre_launch(void* mh, int hook_id, void* payload)
{
    (void)hook_id;
    (void)payload;

    g_ctx->launch_prepend_wrapper(mh, "nice -n 5");

    return 0;
}
```

### Timeout Protection

Automatically kill the instance if it runs longer than 2 hours:

```c
static int on_pre_launch(void* mh, int hook_id, void* payload)
{
    (void)hook_id;
    (void)payload;

    /* 7200 seconds = 2 hours */
    g_ctx->launch_prepend_wrapper(mh, "timeout 7200");

    return 0;
}
```

### User-Configurable Wrapper via Settings

Let users configure the wrapper command through plugin settings:

```c
static int on_pre_launch(void* mh, int hook_id, void* payload)
{
    (void)hook_id;
    (void)payload;

    const char* wrapper = g_ctx->setting_get(mh, "wrapper_command");
    if (wrapper && wrapper[0] != '\0') {
        /* Copy before next API call clobbers tempString */
        char cmd[512];
        snprintf(cmd, sizeof(cmd), "%s", wrapper);

        int rc = g_ctx->launch_prepend_wrapper(mh, cmd);
        if (rc != 0) {
            g_ctx->log_error(mh, "Failed to apply custom wrapper");
        } else {
            char buf[640];
            snprintf(buf, sizeof(buf), "Applied wrapper: %s", cmd);
            g_ctx->log_info(mh, buf);
        }
    }

    return 0;
}
```

### Combining Wrapper and Environment Variables

A complete profiling plugin that sets both env vars and a wrapper:

```c
static int on_pre_launch(void* mh, int hook_id, void* payload)
{
    (void)hook_id;
    MMCOInstanceInfo* info = (MMCOInstanceInfo*)payload;

    /* Check if profiling is enabled for this instance */
    int profiling = g_ctx->setting_get_bool(mh, "profiling_enabled");
    if (!profiling)
        return 0;

    /* Set Java profiler agent */
    g_ctx->launch_set_env(mh,
        "JAVA_TOOL_OPTIONS",
        "-agentpath:/opt/async-profiler/lib/libasyncProfiler.so=start,"
        "event=cpu,file=/tmp/profile-%p.jfr");

    /* Set output directory */
    g_ctx->launch_set_env(mh, "PROFILER_OUTPUT_DIR", "/tmp/profiles");

    /* Wrap with nice to avoid starving other processes */
    g_ctx->launch_prepend_wrapper(mh, "nice -n 10");

    char buf[256];
    snprintf(buf, sizeof(buf),
             "Profiling enabled for instance: %s",
             info->instance_name);
    g_ctx->log_info(mh, buf);

    return 0;
}
```

### Conditional Wrapper Based on Instance Type

```c
#include <string.h>

static int on_pre_launch(void* mh, int hook_id, void* payload)
{
    (void)hook_id;
    MMCOInstanceInfo* info = (MMCOInstanceInfo*)payload;

    /* Only wrap modded instances for debugging */
    const char* type = g_ctx->instance_get_type(mh, info->instance_id);
    if (!type)
        return 0;

    char inst_type[64];
    snprintf(inst_type, sizeof(inst_type), "%s", type);

    if (strcmp(inst_type, "Fabric") == 0 ||
        strcmp(inst_type, "Forge") == 0) {
        g_ctx->launch_prepend_wrapper(mh,
            "strace -f -e trace=open,read,write "
            "-o /tmp/modded-trace.log");
        g_ctx->log_info(mh, "Trace wrapper applied for modded instance");
    }

    return 0;
}
```

---

## Error Conditions

| Condition | Return value | Behavior |
|-----------|-------------|----------|
| `wrapper_cmd` is `nullptr` | `-1` | No side effects. |
| `wrapper_cmd` is empty string `""` | `-1` | No side effects. |
| Command does not exist on PATH | `0` | The API does not validate; the OS will report an error when the launch fails. |
| Called outside `PRE_LAUNCH` hook | `0` | Undefined behavior — wrapper may be discarded or applied incorrectly. |
| Multiple calls in same hook | `0` | Commands are chained (prepended). |

---

## Comparison: `launch_set_env()` vs `launch_prepend_wrapper()`

| Aspect | `launch_set_env()` | `launch_prepend_wrapper()` |
|--------|-------------------|--------------------------|
| **Purpose** | Inject environment variables | Modify the launch command line |
| **Scope** | Process-wide (via `qputenv`) | Instance-specific (`WrapperCommand` setting) |
| **Multiple calls** | Last value per key wins | Commands are chained (prepended) |
| **Empty value** | Allowed (`""` sets empty var) | Not allowed (returns `-1`) |
| **Validation** | Null check only | Null and empty check |
| **Cleanup** | `qunsetenv()` after exit | `WrapperCommand` restored after exit |
| **Visibility** | All threads see the env var | Only affects the instance launch |
| **Uses tempString** | No | No |

---

## Platform Considerations

### Linux / macOS

Wrapper commands are executed via the shell. You can use full paths
(`/usr/bin/strace`) or rely on `PATH` resolution. Shell features like
pipes and redirections depend on how MeshMC constructs the final
command line — consult the instance launch implementation for details.

### Windows

Wrapper commands must be valid Windows executables or batch files.
POSIX-specific tools like `strace`, `nice`, and `timeout` are not
available natively. Use Windows equivalents or WSL-aware wrappers.

---

## Cross-References

| Section | Relationship |
|---------|-------------|
| **S1 (Lifecycle)** | `hook_register()` is used to register the `INSTANCE_PRE_LAUNCH` callback. |
| **S2 (Settings)** | `setting_get()` can read user-configured wrapper commands. |
| **S3 (Instances)** | `instance_get_type()` can be used to conditionally apply wrappers based on instance type. |
| **S4 (Instance Management)** | `instance_launch()` triggers the `INSTANCE_PRE_LAUNCH` hook. |
| **S15 (Launch Modifiers)** | `launch_set_env()` is the companion function for environment injection. |
