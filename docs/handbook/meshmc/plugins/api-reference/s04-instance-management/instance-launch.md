# Instance Launch API

> **Sub-page of [S04 — Instance Management](README.md)**
> Covers: `instance_launch()`, `instance_can_launch()`

---

## instance_launch()

Launch a Minecraft process for the specified instance.

### Signature

```c
int (*instance_launch)(void* mh, const char* id, int online);
```

### Parameters

| Parameter | Type | Description |
|-----------|------|-------------|
| `mh` | `void*` | Module handle — the opaque pointer from `MMCOContext.module_handle`. Identifies the calling plugin. |
| `id` | `const char*` | Instance ID string. Must be a valid ID previously obtained from `instance_get_id()` (S03). Must not be `NULL`. |
| `online` | `int` | Authentication mode. Non-zero (`1`) for online play (full Microsoft account authentication). Zero (`0`) for offline mode (no server authentication, player name prompt). |

### Return Value

| Value | Meaning |
|-------|---------|
| `0` | The launch sequence was successfully initiated. This does **not** mean Minecraft is running yet — launch is asynchronous. |
| `-1` | Launch failed. Possible causes: invalid instance ID, `NULL` parameters, instance cannot be launched (`canLaunch()` returned false), application state prevents launch (e.g., update in progress). |

### Thread Safety

**Main thread only.** This function creates UI objects (`LaunchController`,
dialogs) and connects Qt signals. Calling from a background thread will
corrupt Qt's internal state and likely crash the application.

### Pointer Ownership

No pointers are returned. The `id` string is read and copied internally —
no reference to the caller's string is retained after return.

---

### How Launch Works Internally

Understanding the internal flow helps plugin authors predict behaviour,
especially around timing, UI dialogs, and hook dispatch.

#### Step 1: Resolve the Instance

```cpp
// PluginManager.cpp — api_instance_launch()
int PluginManager::api_instance_launch(void* mh, const char* id, int online)
{
    auto* r = rt(mh);
    auto* app = r->manager->m_app;
    if (!app || !app->instances() || !id)
        return -1;
    auto inst = app->instances()->getInstanceById(QString::fromUtf8(id));
    if (!inst)
        return -1;
    return app->launch(inst, online != 0) ? 0 : -1;
}
```

The trampoline converts the C string to `QString`, looks up the instance in
the `InstanceList`, and delegates to `Application::launch()`.

#### Step 2: Application::launch()

```cpp
// Application.cpp (simplified)
bool Application::launch(InstancePtr instance, bool online, ...)
{
    if (m_updateRunning)
        return false;                         // (a) updates block launches

    if (instance->canLaunch()) {
        auto& extras = m_instanceExtras[instance->id()];
        auto& controller = extras.controller;
        controller.reset(new LaunchController());
        controller->setInstance(instance);
        controller->setOnline(online);
        // ... set parent widget ...
        controller->start();                  // (b) async launch begins
        return true;
    }

    if (instance->isRunning()) {
        showInstanceWindow(instance, "console"); // (c) already running
        return true;
    }

    if (instance->canEdit()) {
        showInstanceWindow(instance);            // (d) broken but editable
        return true;
    }

    return false;
}
```

Key branches:

| Branch | Condition | Plugin sees |
|--------|-----------|-------------|
| **(a)** | `m_updateRunning` is true | `instance_launch()` returns `-1` |
| **(b)** | `canLaunch()` is true | `instance_launch()` returns `0`, launch proceeds asynchronously |
| **(c)** | `isRunning()` is true | `instance_launch()` returns `0`, console window opens (no new process) |
| **(d)** | `canEdit()` is true (broken version, etc.) | `instance_launch()` returns `0`, instance window opens |
| none | All checks fail | `instance_launch()` returns `-1` |

> **Important:** Branches (c) and (d) return success (`0`) from the plugin's
> perspective even though no new process starts. The API call "succeeded" in
> the sense that it did something useful.

#### Step 3: LaunchController Flow

The `LaunchController` is a `Task` subclass. Its `executeTask()` method
runs asynchronously on the main thread event loop:

1. **JVM argument validation** via `JavaCommon::checkJVMArgs()`.
2. **Account selection** via `decideAccount()`:
   - If a default account exists → use it.
   - If no accounts exist → prompt user to add one (dialog).
   - If no default → show `ProfileSelectDialog`.
3. **Authentication** via `login()`:
   - `AccountState::Online` → proceed to launch (or prompt for offline name).
   - `AccountState::Offline` → force offline mode.
   - `AccountState::Errored` / `AccountState::Unchecked` → refresh token.
   - `AccountState::Expired` → error dialog, launch fails.
   - `AccountState::Gone` → error dialog, launch fails.
4. **Instance launch** via `launchInstance()`:
   - Reload instance settings.
   - Create `LaunchTask` (the actual process manager).
   - **Dispatch `MMCO_HOOK_INSTANCE_PRE_LAUNCH`** to all plugins.
   - Prepend console text (version info, server resolution, online mode).
   - Call `LaunchTask::start()` which forks the Java process.

#### Step 4: Process Exit

When Minecraft exits:

- **Clean exit** → `LaunchController::onSucceeded()` fires
  `MMCO_HOOK_INSTANCE_POST_LAUNCH`.
- **Crash** → `LaunchController::onFailed()` fires, `m_crashed` flag is set
  on the instance. *POST_LAUNCH does not fire.*

---

### The `online` Parameter in Detail

The `online` parameter controls whether MeshMC attempts full
Microsoft-account authentication before launching.

| Value | Mode | Account Behaviour | Server Access |
|-------|------|-------------------|---------------|
| `1` (non-zero) | **Online** | Full auth flow. Token refresh if needed. Account must own Minecraft (or demo). | Full server access with authenticated profile. |
| `0` | **Offline** | Prompts user for a player name. No token exchange. | LAN only. Public servers reject unauthenticated players. |

**What happens with no accounts?**

If `online != 0` and no Microsoft accounts are configured, `decideAccount()`
shows a dialog asking the user to add one. If the user declines, the launch
fails internally (the controller emits `failed`), but `instance_launch()`
has already returned `0` because the controller was successfully created.

**Demo mode:**

If `online != 0` and the selected account does **not** own Minecraft, the
user sees a "Play demo?" dialog. If they accept, Minecraft launches in
demo mode. If they decline, launch is aborted.

---

### Hook Interaction: PRE_LAUNCH

After authentication succeeds and just before the Java process is forked,
`LaunchController::launchInstance()` dispatches the pre-launch hook:

```cpp
// LaunchController.cpp — inside launchInstance()
if (APPLICATION->pluginManager()) {
    MMCOInstanceInfo hookInfo{};
    hookInfo.instance_id    = idUtf8.constData();
    hookInfo.instance_name  = nameUtf8.constData();
    hookInfo.instance_path  = pathUtf8.constData();
    hookInfo.minecraft_version = nullptr;
    APPLICATION->pluginManager()->dispatchHook(
        MMCO_HOOK_INSTANCE_PRE_LAUNCH, &hookInfo);
}
```

The `dispatchHook()` implementation:

```cpp
bool PluginManager::dispatchHook(uint32_t hook_id, void* payload)
{
    auto range = m_hooks.equal_range(hook_id);
    for (auto it = range.first; it != range.second; ++it) {
        const auto& reg = it.value();
        int rc = reg.callback(reg.module_handle, hook_id,
                              payload, reg.user_data);
        if (rc != 0)
            return true;   // cancelled
    }
    return false;
}
```

**Can a plugin cancel the launch via PRE_LAUNCH?**

The dispatch function supports cancellation — if a callback returns non-zero,
`dispatchHook()` returns `true`. However, the call site in
`LaunchController::launchInstance()` **does not check the return value**. It
calls `dispatchHook()` and immediately continues to `m_launcher->start()`.

> **Conclusion:** In the current ABI, returning non-zero from a
> `MMCO_HOOK_INSTANCE_PRE_LAUNCH` callback does **not** cancel the launch.
> The hook is notification-only.

Plugin authors should:
- Return `0` from PRE_LAUNCH callbacks for forward compatibility.
- Use PRE_LAUNCH for logging, analytics, or pre-launch setup work.
- Do **not** rely on cancellation semantics — they may be added in a
  future ABI version.

### Hook Interaction: POST_LAUNCH

`MMCO_HOOK_INSTANCE_POST_LAUNCH` is dispatched in
`LaunchController::onSucceeded()`, which fires when the Minecraft process
exits with a zero exit code:

```cpp
void LaunchController::onSucceeded()
{
    if (APPLICATION->pluginManager() && m_instance) {
        MMCOInstanceInfo hookInfo{};
        hookInfo.instance_id    = idUtf8.constData();
        hookInfo.instance_name  = nameUtf8.constData();
        hookInfo.instance_path  = pathUtf8.constData();
        hookInfo.minecraft_version = nullptr;
        APPLICATION->pluginManager()->dispatchHook(
            MMCO_HOOK_INSTANCE_POST_LAUNCH, &hookInfo);
    }
    emitSucceeded();
}
```

**POST_LAUNCH does not fire on crash or abort.** If you need to react to
all exit conditions, combine POST_LAUNCH with periodic `instance_is_running()`
and `instance_has_crashed()` polling.

---

### Error Conditions

| Condition | Return | Notes |
|-----------|--------|-------|
| `id` is `NULL` | `-1` | Caught in trampoline before any work. |
| `id` does not match any instance | `-1` | `getInstanceById()` returns null. |
| Application pointer is null | `-1` | Should never happen after init. |
| Instance list not loaded | `-1` | Can happen during very early init. |
| Application update is running | `-1` | `m_updateRunning` checked first. |
| Instance is already running | `0` | Console window opened instead. No new process. |
| Instance has broken version | `0` | Instance window opened instead. No new process. |
| Account authentication fails | `0` | Launch was initiated but controller will emit `failed` asynchronously. The API call returns `0` because the controller was created. |
| No accounts configured, user cancels dialog | `0` | Same as above. |

> **Key insight:** A return value of `0` does *not* guarantee Minecraft will
> start. It means the launch sequence was successfully initiated. Many
> failure modes are asynchronous. Use `instance_is_running()` or the
> POST_LAUNCH hook to confirm the process started.

---

### Implementation Notes (for MeshMC contributors)

The trampoline `PluginManager::api_instance_launch()` is declared as a
`static` member function in `PluginManager.h`:

```cpp
static int api_instance_launch(void* mh, const char* id, int online);
```

It is assigned to `ctx.instance_launch` in `buildContext()`:

```cpp
ctx.instance_launch = api_instance_launch;
```

The trampoline uses `rt(mh)` to recover the `ModuleRuntime*`, then accesses
`r->manager->m_app` to reach the `Application` singleton. The design
intentionally goes through `Application::launch()` rather than creating a
`LaunchController` directly, ensuring all normal UI flow (window management,
running-instance counting) is preserved.

---

## instance_can_launch()

Check whether an instance is currently in a state that allows launching.

### Signature

```c
int (*instance_can_launch)(void* mh, const char* id);
```

### Parameters

| Parameter | Type | Description |
|-----------|------|-------------|
| `mh` | `void*` | Module handle. |
| `id` | `const char*` | Instance ID string. Must not be `NULL`. |

### Return Value

| Value | Meaning |
|-------|---------|
| `1` | The instance can be launched. A call to `instance_launch()` will start the launch sequence. |
| `0` | The instance cannot be launched right now, **or** the instance ID is invalid / `NULL`. |

### Thread Safety

**Main thread only.**

### What Prevents an Instance from Launching?

`BaseInstance::canLaunch()` returns `false` when:

| Condition | Description |
|-----------|-------------|
| Already running | `m_isRunning` is true. Another launch is in progress. |
| Broken version | The instance's version manifest is corrupt or missing. |
| Status not `Present` | The instance is being imported, deleted, or is in another transient state. |

The exact logic is in the `MinecraftInstance::canLaunch()` override, which
additionally checks that the component list (pack profile) is valid.

### Implementation

```cpp
int PluginManager::api_instance_can_launch(void* mh, const char* id)
{
    auto* r = rt(mh);
    auto* inst = resolveInstance(r, id);
    return inst ? (inst->canLaunch() ? 1 : 0) : 0;
}
```

The `resolveInstance()` helper:

```cpp
static BaseInstance* resolveInstance(PluginManager::ModuleRuntime* r,
                                     const char* id)
{
    auto* app = r->manager->m_app;
    if (!app || !app->instances() || !id)
        return nullptr;
    auto inst = app->instances()->getInstanceById(QString::fromUtf8(id));
    return inst.get();
}
```

---

### Recommended Pattern: Guard Before Launch

Always call `instance_can_launch()` before `instance_launch()` to give the
user meaningful feedback:

```cpp
static void launch_instance(MMCOContext* ctx, void* mh, const char* id)
{
    if (!ctx->instance_can_launch(mh, id)) {
        if (ctx->instance_is_running(mh, id)) {
            ctx->log_warn(mh, "Instance is already running.");
        } else {
            ctx->log_error(mh, "Instance cannot be launched "
                               "(broken version or invalid state).");
        }
        return;
    }

    if (ctx->instance_launch(mh, id, 1) != 0) {
        ctx->log_error(mh, "instance_launch() failed unexpectedly.");
    }
}
```

---

## Complete Example: Auto-Launch Plugin

This plugin automatically launches a specific instance (by name) in online
mode when MeshMC finishes initializing. It also demonstrates cleanup.

```cpp
/* auto_launch.cpp — MMCO plugin that auto-launches an instance */
#include <mmco_sdk.h>
#include <cstring>
#include <cstdio>

static MMCOContext* g_ctx = nullptr;
static void*        g_mh  = nullptr;

/* Name of the instance to auto-launch (could also come from settings) */
static const char* TARGET_NAME = "Survival 1.21";

static char g_target_id[256] = {};

/* Find instance ID by display name */
static bool find_instance_by_name(const char* name)
{
    int n = g_ctx->instance_count(g_mh);
    for (int i = 0; i < n; i++) {
        const char* id = g_ctx->instance_get_id(g_mh, i);
        if (!id) continue;

        /* Copy the ID before the next API call invalidates it */
        char id_buf[256];
        strncpy(id_buf, id, sizeof(id_buf) - 1);
        id_buf[sizeof(id_buf) - 1] = '\0';

        const char* inst_name = g_ctx->instance_get_name(g_mh, id_buf);
        if (inst_name && strcmp(inst_name, name) == 0) {
            strncpy(g_target_id, id_buf, sizeof(g_target_id) - 1);
            return true;
        }
    }
    return false;
}

/* Hook callback: app is ready, try to launch */
static int on_app_initialized(void* module_handle, uint32_t hook_id,
                               void* payload, void* user_data)
{
    (void)module_handle; (void)hook_id; (void)payload; (void)user_data;

    if (!find_instance_by_name(TARGET_NAME)) {
        g_ctx->log_warn(g_mh, "Auto-launch: target instance not found.");
        return 0;
    }

    if (!g_ctx->instance_can_launch(g_mh, g_target_id)) {
        char msg[512];
        snprintf(msg, sizeof(msg),
                 "Auto-launch: '%s' is not in a launchable state.", TARGET_NAME);
        g_ctx->log_warn(g_mh, msg);
        return 0;
    }

    if (g_ctx->instance_launch(g_mh, g_target_id, 1) == 0) {
        g_ctx->log_info(g_mh, "Auto-launch: launch sequence started.");
    } else {
        g_ctx->log_error(g_mh, "Auto-launch: instance_launch() failed.");
    }

    return 0;
}

/* Hook callback: log when the instance exits */
static int on_post_launch(void* module_handle, uint32_t hook_id,
                           void* payload, void* user_data)
{
    (void)module_handle; (void)hook_id; (void)user_data;
    MMCOInstanceInfo* info = (MMCOInstanceInfo*)payload;
    if (info && info->instance_id &&
        strcmp(info->instance_id, g_target_id) == 0) {
        g_ctx->log_info(g_mh, "Auto-launch: instance exited successfully.");
    }
    return 0;
}

extern "C" int mmco_init(MMCOContext* ctx)
{
    g_ctx = ctx;
    g_mh  = ctx->module_handle;

    ctx->hook_register(g_mh, MMCO_HOOK_APP_INITIALIZED,
                       on_app_initialized, nullptr);
    ctx->hook_register(g_mh, MMCO_HOOK_INSTANCE_POST_LAUNCH,
                       on_post_launch, nullptr);

    g_ctx->log_info(g_mh, "AutoLaunch plugin loaded.");
    return 0;
}

extern "C" void mmco_unload(MMCOContext* ctx)
{
    ctx->hook_unregister(ctx->module_handle, MMCO_HOOK_APP_INITIALIZED,
                         on_app_initialized);
    ctx->hook_unregister(ctx->module_handle, MMCO_HOOK_INSTANCE_POST_LAUNCH,
                         on_post_launch);
}
```

### Key Patterns Demonstrated

| Pattern | Lines | Why |
|---------|-------|-----|
| Copy ID before next API call | `strncpy(id_buf, ...)` | `tempString` invalidation rule |
| Guard with `instance_can_launch()` | `if (!g_ctx->instance_can_launch(...))` | Prevents confusing error returns |
| Use `MMCO_HOOK_APP_INITIALIZED` for deferred work | `on_app_initialized` | Instances aren't loaded during `mmco_init()` |
| Clean hook unregistration in `mmco_unload()` | Both hooks unregistered | Prevents dangling callbacks |
| Return `0` from all hooks | `return 0` | Forward-compatible, doesn't block dispatch chain |

---

## Complete Example: Scheduled Launcher

A more advanced example that launches an instance after a configurable
delay, using the plugin's file storage to persist the target:

```cpp
/* scheduled_launch.cpp — launches an instance after a delay */
#include <mmco_sdk.h>
#include <cstring>
#include <cstdio>
#include <cstdlib>

static MMCOContext* g_ctx = nullptr;
static void*        g_mh  = nullptr;
static char         g_id[256] = {};
static int64_t      g_launch_after = 0;  /* epoch seconds */

static int on_app_initialized(void* module_handle, uint32_t hook_id,
                               void* payload, void* user_data)
{
    (void)module_handle; (void)hook_id; (void)payload; (void)user_data;

    /* Read saved config from plugin data dir */
    char buf[512] = {};
    int64_t bytes = g_ctx->fs_read(g_mh, "schedule.txt", buf, sizeof(buf) - 1);
    if (bytes <= 0) {
        g_ctx->log_info(g_mh, "No scheduled launch configured.");
        return 0;
    }

    /* Parse "instance_id timestamp" */
    char* space = strchr(buf, ' ');
    if (!space) return 0;
    *space = '\0';
    strncpy(g_id, buf, sizeof(g_id) - 1);
    g_launch_after = strtoll(space + 1, nullptr, 10);

    /* Check if it's time */
    int64_t now = g_ctx->instance_get_last_launch(g_mh, g_id);
    /* Reuse a timestamp approach — in real code, use S14 get_timestamp() */
    if (now == 0) {
        /* Never launched — go ahead */
    }

    if (!g_ctx->instance_can_launch(g_mh, g_id)) {
        g_ctx->log_warn(g_mh, "Scheduled instance not launchable.");
        return 0;
    }

    g_ctx->instance_launch(g_mh, g_id, 1);
    g_ctx->log_info(g_mh, "Scheduled launch initiated.");

    /* Clear the schedule */
    g_ctx->fs_write(g_mh, "schedule.txt", "", 0);

    return 0;
}

extern "C" int mmco_init(MMCOContext* ctx)
{
    g_ctx = ctx;
    g_mh  = ctx->module_handle;
    ctx->hook_register(g_mh, MMCO_HOOK_APP_INITIALIZED,
                       on_app_initialized, nullptr);
    return 0;
}

extern "C" void mmco_unload(MMCOContext* ctx)
{
    ctx->hook_unregister(ctx->module_handle, MMCO_HOOK_APP_INITIALIZED,
                         on_app_initialized);
}
```

---

## FAQ

### Q: Can I launch multiple instances simultaneously?

**Yes.** MeshMC supports running multiple instances at once. Each
`instance_launch()` call for a different instance ID will create a separate
`LaunchController`. However, launching the same instance while it is already
running will open the console window instead.

### Q: What happens if I call `instance_launch()` during `mmco_init()`?

The instance list may not be fully loaded yet. It is safer to defer launch
to a `MMCO_HOOK_APP_INITIALIZED` callback, which fires after all
subsystems (accounts, instances, settings) are ready.

### Q: Does `instance_launch()` block until Minecraft exits?

**No.** It returns as soon as the launch controller is created. The actual
process runs asynchronously. Use `instance_is_running()` or the POST_LAUNCH
hook to track process state.

### Q: Can I pass a custom account to `instance_launch()`?

Not through the plugin API. The C API only exposes the `online` flag.
Account selection follows MeshMC's standard flow (default account or user
dialog). If you need specific account control, use `instance_launch()` with
`online=1` and ensure the desired account is set as default beforehand.

### Q: What does `instance_launch(mh, id, 0)` do with accounts?

In offline mode (`online=0`), the selected account's token is not verified.
MeshMC prompts the user for a player name (defaulting to the account
profile name). No Microsoft authentication occurs.

---

*Part of [S04 — Instance Management](README.md) · [MMCO Plugin API Reference](../README.md)*
