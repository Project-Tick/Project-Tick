# Instance Hooks

> `MMCO_HOOK_INSTANCE_PRE_LAUNCH` · `MMCO_HOOK_INSTANCE_POST_LAUNCH` · `MMCO_HOOK_INSTANCE_CREATED` · `MMCO_HOOK_INSTANCE_REMOVED`

Instance hooks track the lifecycle of Minecraft instances — creation, deletion,
and the launch sequence.  `INSTANCE_PRE_LAUNCH` is the only cancellable hook in
this group, giving plugins veto power over launches.

All four hooks share the same payload type: `MMCOInstanceInfo*`.

---

## Shared payload — `MMCOInstanceInfo`

Every hook in this section delivers a pointer to an `MMCOInstanceInfo` struct.

### Declaration (from `PluginHooks.h`)

```cpp
struct MMCOInstanceInfo {
    const char* instance_id;        /* Unique instance identifier */
    const char* instance_name;      /* Human-readable instance name */
    const char* instance_path;      /* Absolute path to the instance directory */
    const char* minecraft_version;  /* Minecraft version string, e.g. "1.20.4" */
};
```

### Field reference

| Field | Type | Nullable | Description |
|---|---|---|---|
| `instance_id` | `const char*` | No | The unique, stable identifier for the instance.  This is the same value returned by `instance_get_id()` and used as the key for all instance API functions. |
| `instance_name` | `const char*` | No | The human-readable display name of the instance (e.g., `"My SMP Server 1.21"`). |
| `instance_path` | `const char*` | No | The absolute filesystem path to the instance's root directory. |
| `minecraft_version` | `const char*` | **Yes** | The resolved Minecraft version string.  May be `nullptr` if the version cannot be determined (corrupted instance, unknown modloader, or the instance was just created and hasn't been fully configured yet). |

### Memory model

| Property | Detail |
|---|---|
| **Allocator** | Launcher — stack-allocated in the calling function |
| **Storage** | Stack frame of the dispatch call site (`LaunchController`, `InstanceList`, etc.) |
| **String backing** | `QByteArray` locals in the same stack frame (`idUtf8`, `nameUtf8`, `pathUtf8`) |
| **Lifetime** | Valid **only** for the duration of the callback invocation |

The struct and all its `const char*` fields are ephemeral.  If you need any
value after the callback returns, **copy it immediately**:

```cpp
static int on_instance_event(void* mh, uint32_t hook_id,
                             void* payload, void* ud)
{
    auto* info = static_cast<MMCOInstanceInfo*>(payload);
    if (!info) return 0;

    /* SAFE: copy to std::string */
    std::string id = info->instance_id;
    std::string name = info->instance_name;

    /* UNSAFE: pointer becomes dangling after this callback returns */
    // const char* bad = info->instance_id;  // DO NOT store raw pointers!

    return 0;
}
```

### How the launcher populates the struct

For launch hooks, `LaunchController.cpp` builds the struct:

```cpp
QByteArray idUtf8 = m_instance->id().toUtf8();
QByteArray nameUtf8 = m_instance->name().toUtf8();
QByteArray pathUtf8 = m_instance->instanceRoot().toUtf8();
MMCOInstanceInfo hookInfo{};
hookInfo.instance_id = idUtf8.constData();
hookInfo.instance_name = nameUtf8.constData();
hookInfo.instance_path = pathUtf8.constData();
hookInfo.minecraft_version = nullptr;
```

Note that `minecraft_version` is set to `nullptr` in the current launcher code
for launch hooks.  It may be populated in future versions.  Always null-check
this field.

---

## `MMCO_HOOK_INSTANCE_PRE_LAUNCH`

### Summary

| Property | Value |
|---|---|
| **Constant** | `MMCO_HOOK_INSTANCE_PRE_LAUNCH` |
| **Hex value** | `0x0200` |
| **Payload** | `MMCOInstanceInfo*` |
| **Cancellable** | **Yes** |
| **Thread** | Main (GUI) thread |
| **Fires** | Before each instance launch attempt |

### When it fires

Dispatched in `LaunchController.cpp`, immediately before the Minecraft process
is started (`m_launcher->start()`).

**Exact dispatch location** — `LaunchController.cpp`:

```cpp
if (APPLICATION->pluginManager()) {
    QByteArray idUtf8 = m_instance->id().toUtf8();
    QByteArray nameUtf8 = m_instance->name().toUtf8();
    QByteArray pathUtf8 = m_instance->instanceRoot().toUtf8();
    MMCOInstanceInfo hookInfo{};
    hookInfo.instance_id = idUtf8.constData();
    hookInfo.instance_name = nameUtf8.constData();
    hookInfo.instance_path = pathUtf8.constData();
    hookInfo.minecraft_version = nullptr;
    APPLICATION->pluginManager()->dispatchHook(
        MMCO_HOOK_INSTANCE_PRE_LAUNCH, &hookInfo);
}

m_launcher->start();
```

At the point this hook fires:

- The `LaunchController` has resolved the instance, account, and Java path.
- The launch command line has been prepared.
- The Minecraft process has **not** started yet.
- The instance is **not** marked as running.

### Cancellation

This is **the only cancellable hook** in the instance lifecycle.  If any
callback returns non-zero:

- `dispatchHook()` returns `true`.
- The launcher can abort the launch (the instance process is never created).
- Subsequent callbacks in the chain are **not** called.

### Use cases

- **Launch gating** — prevent launches based on custom rules (time limits,
  parental controls, version restrictions).
- **Pre-launch validation** — check for required mods, configuration, or Java
  version before launching.
- **Backup creation** — trigger an automatic backup before each launch.
- **Logging** — record launch attempts for analytics.
- **Payload inspection** — read the instance ID and name for conditional logic.

### Example — Block legacy Minecraft versions

```cpp
struct LaunchGuardState {
    bool block_legacy;
    const char* minimum_version;
};

static LaunchGuardState g_guard = { true, "1.16.0" };

static int on_pre_launch(void* mh, uint32_t hook_id,
                         void* payload, void* ud)
{
    auto* info = static_cast<MMCOInstanceInfo*>(payload);
    auto* guard = static_cast<LaunchGuardState*>(ud);
    if (!info || !guard->block_legacy)
        return 0;

    if (info->minecraft_version &&
        strcmp(info->minecraft_version, guard->minimum_version) < 0)
    {
        g_ctx->log_warn(g_ctx->module_handle,
            (std::string("Blocked launch of ") + info->instance_name
             + " (MC " + info->minecraft_version + " < "
             + guard->minimum_version + ")").c_str());
        return 1;  /* CANCEL the launch */
    }

    return 0;  /* allow */
}

MMCO_EXPORT int mmco_init(MMCOContext* ctx)
{
    g_ctx = ctx;
    ctx->hook_register(ctx->module_handle,
                       MMCO_HOOK_INSTANCE_PRE_LAUNCH,
                       on_pre_launch, &g_guard);
    return 0;
}
```

### Example — Auto-backup before launch

```cpp
static int on_pre_launch(void* mh, uint32_t hook_id,
                         void* payload, void* ud)
{
    auto* info = static_cast<MMCOInstanceInfo*>(payload);
    if (!info || !info->instance_path)
        return 0;

    MMCO_LOG(g_ctx, (std::string("Creating pre-launch backup of ")
                     + info->instance_name).c_str());

    /* Call into backup logic (synchronous — runs on main thread) */
    bool ok = create_backup_snapshot(info->instance_path, info->instance_id);
    if (!ok) {
        MMCO_ERR(g_ctx, "Backup failed — allowing launch anyway.");
    }

    return 0;  /* always allow launch */
}
```

### Notes

- `minecraft_version` is currently `nullptr` in launch hooks.  If you need the
  version, query it via the instance API:
  `g_ctx->instance_get_path(g_ctx->module_handle, info->instance_id)` and
  parse `instance.cfg` in that directory.
- If you cancel the launch and want to inform the user, log a warning — the
  launcher does not show a dialog for plugin-cancelled launches.
- Multiple plugins can register for `PRE_LAUNCH`.  If any one cancels, the
  launch is fully aborted.  Plugins later in the chain are not consulted.

---

## `MMCO_HOOK_INSTANCE_POST_LAUNCH`

### Summary

| Property | Value |
|---|---|
| **Constant** | `MMCO_HOOK_INSTANCE_POST_LAUNCH` |
| **Hex value** | `0x0201` |
| **Payload** | `MMCOInstanceInfo*` |
| **Cancellable** | No |
| **Thread** | Main (GUI) thread |
| **Fires** | After a successful instance launch |

### When it fires

Dispatched in `LaunchController::onSucceeded()`, which is called when the
Minecraft process has been launched and the `LaunchTask` reports success.

**Exact dispatch location** — `LaunchController.cpp`:

```cpp
void LaunchController::onSucceeded()
{
    // Dispatch post-launch hook to plugins
    if (APPLICATION->pluginManager() && m_instance) {
        QByteArray idUtf8 = m_instance->id().toUtf8();
        QByteArray nameUtf8 = m_instance->name().toUtf8();
        QByteArray pathUtf8 = m_instance->instanceRoot().toUtf8();
        MMCOInstanceInfo hookInfo{};
        hookInfo.instance_id = idUtf8.constData();
        hookInfo.instance_name = nameUtf8.constData();
        hookInfo.instance_path = pathUtf8.constData();
        hookInfo.minecraft_version = nullptr;
        APPLICATION->pluginManager()->dispatchHook(
            MMCO_HOOK_INSTANCE_POST_LAUNCH, &hookInfo);
    }

    emitSucceeded();
}
```

At the point this hook fires:

- The Minecraft process **has started** (the JVM is running).
- The instance is marked as running.
- The launch was considered successful by the launcher.
- The launch console window (if enabled) is showing output.

### Return value

The launcher ignores the return value.  Callbacks should return `0`.

### Use cases

- **Launch counting / analytics** — increment a counter each time an instance
  launches.
- **Post-launch actions** — start monitoring the game log, enable overlay
  features, notify a Discord bot.
- **Timing** — record the launch timestamp for session duration tracking.
- **Notification** — inform the user that the game is now running.

### Example — Launch counter

```cpp
struct PluginState {
    int launch_count;
};

static PluginState g_state = { 0 };

static int on_post_launch(void* mh, uint32_t hook_id,
                          void* payload, void* ud)
{
    auto* state = static_cast<PluginState*>(ud);
    auto* info = static_cast<MMCOInstanceInfo*>(payload);
    if (!info) return 0;

    state->launch_count++;

    char buf[256];
    snprintf(buf, sizeof(buf),
             "Instance '%s' launched (total launches: %d)",
             info->instance_name, state->launch_count);
    g_ctx->log_info(g_ctx->module_handle, buf);

    return 0;
}

MMCO_EXPORT int mmco_init(MMCOContext* ctx)
{
    g_ctx = ctx;
    ctx->hook_register(ctx->module_handle,
                       MMCO_HOOK_INSTANCE_POST_LAUNCH,
                       on_post_launch, &g_state);
    return 0;
}
```

### Example — Record launch timestamp

```cpp
#include <ctime>
#include <map>
#include <string>

static std::map<std::string, time_t> g_launch_times;

static int on_post_launch(void* mh, uint32_t hook_id,
                          void* payload, void* ud)
{
    auto* info = static_cast<MMCOInstanceInfo*>(payload);
    if (!info) return 0;

    g_launch_times[info->instance_id] = time(nullptr);

    MMCO_LOG(g_ctx,
        (std::string("Started tracking session for: ")
         + info->instance_name).c_str());
    return 0;
}
```

### Notes

- `POST_LAUNCH` only fires for **successful** launches.  If the launch fails
  (bad Java path, missing libraries, etc.), this hook does **not** fire.
  The launcher calls `onFailed()` instead, which does not dispatch any hook.
- The `minecraft_version` field is `nullptr` in the current implementation.
- This hook fires on the main thread.  If you need to start long-running
  monitoring, spawn a worker thread and return immediately.

---

## `MMCO_HOOK_INSTANCE_CREATED`

### Summary

| Property | Value |
|---|---|
| **Constant** | `MMCO_HOOK_INSTANCE_CREATED` |
| **Hex value** | `0x0202` |
| **Payload** | `MMCOInstanceInfo*` |
| **Cancellable** | No |
| **Thread** | Main (GUI) thread |
| **Fires** | When a new instance is added to the instance list |

### When it fires

Dispatched when a new instance is created, either by the user (via the "Add
Instance" dialog) or programmatically.  The hook fires **after** the instance
has been added to the internal `InstanceList` and is fully queryable via the
instance API.

At the point this hook fires:

- The instance directory has been created on disk.
- The instance appears in `instance_count()` / `instance_get_id()`.
- Instance properties (`instance_get_name()`, `instance_get_path()`) are
  available.
- The instance may not have a Minecraft version set yet if creation is a
  multi-step process (e.g., "create then configure").

### Payload fields

The `MMCOInstanceInfo` struct is populated with:

| Field | Populated? | Notes |
|---|---|---|
| `instance_id` | Yes | The new instance's unique ID. |
| `instance_name` | Yes | The display name chosen by the user. |
| `instance_path` | Yes | Absolute path to the new instance directory. |
| `minecraft_version` | Possibly | May be `nullptr` if the version is not yet resolved. |

### Return value

The launcher ignores the return value.  Callbacks should return `0`.

### Use cases

- **Instance tracking** — maintain an internal map of known instances.
- **Default configuration** — apply plugin-specific defaults to new instances
  (e.g., enable auto-backup, set recommended JVM arguments).
- **Notification** — alert the user that a new instance was set up.
- **Companion file creation** — create plugin-specific files in the new
  instance's directory.

### Example — Apply default settings to new instances

```cpp
static int on_instance_created(void* mh, uint32_t hook_id,
                                void* payload, void* ud)
{
    auto* info = static_cast<MMCOInstanceInfo*>(payload);
    if (!info) return 0;

    MMCO_LOG(g_ctx,
        (std::string("New instance created: ") + info->instance_name).c_str());

    /* Set a plugin-specific default for this instance */
    std::string key = std::string("plugin.backup-system.")
                    + info->instance_id + ".enabled";
    g_ctx->setting_set(g_ctx->module_handle, key.c_str(), "true");

    return 0;
}

MMCO_EXPORT int mmco_init(MMCOContext* ctx)
{
    g_ctx = ctx;
    ctx->hook_register(ctx->module_handle,
                       MMCO_HOOK_INSTANCE_CREATED,
                       on_instance_created, nullptr);
    return 0;
}
```

### Example — Create a companion directory for new instances

```cpp
#include <sys/stat.h>

static int on_instance_created(void* mh, uint32_t hook_id,
                                void* payload, void* ud)
{
    auto* info = static_cast<MMCOInstanceInfo*>(payload);
    if (!info || !info->instance_path)
        return 0;

    /* Create a plugin-specific directory inside the instance */
    std::string plugin_dir = std::string(info->instance_path)
                           + "/backups";
    mkdir(plugin_dir.c_str(), 0755);

    MMCO_LOG(g_ctx,
        (std::string("Created backup dir for: ")
         + info->instance_name).c_str());
    return 0;
}
```

### Interaction with `instance_count()`

When called from inside an `INSTANCE_CREATED` callback, `instance_count()`
returns the count **including** the new instance.  The new instance is fully
registered by the time the hook fires.

### Notes

- This hook fires once per instance creation.  Renaming or modifying an
  instance does **not** re-fire this hook.
- If multiple instances are imported at once (batch import), the hook fires
  once per instance, serially.
- The instance's on-disk structure is valid at the time of the callback — you
  can safely read/write files in `instance_path`.

---

## `MMCO_HOOK_INSTANCE_REMOVED`

### Summary

| Property | Value |
|---|---|
| **Constant** | `MMCO_HOOK_INSTANCE_REMOVED` |
| **Hex value** | `0x0203` |
| **Payload** | `MMCOInstanceInfo*` |
| **Cancellable** | No |
| **Thread** | Main (GUI) thread |
| **Fires** | When an instance is deleted from the instance list |

### When it fires

Dispatched when an instance is removed, either by the user (via "Delete
Instance") or programmatically.  The hook fires **before** the instance's
on-disk directory is deleted (if trash/delete is requested), but **after** the
instance has been removed from the internal `InstanceList`.

At the point this hook fires:

- The instance has been removed from the `InstanceList` — it will **not** appear
  in `instance_count()` / `instance_get_id()` results.
- The instance directory **may still exist** on disk (deletion may be deferred
  or the user chose "remove from list only").
- The `MMCOInstanceInfo` struct contains the last known metadata of the instance.

### Payload fields

| Field | Populated? | Notes |
|---|---|---|
| `instance_id` | Yes | The ID of the removed instance. |
| `instance_name` | Yes | The display name of the removed instance. |
| `instance_path` | Yes | Absolute path — directory may or may not still exist on disk. |
| `minecraft_version` | Possibly | May be `nullptr`. |

### Return value

The launcher ignores the return value.  Callbacks should return `0`.

### Use cases

- **Cleanup** — remove plugin-specific data, settings, or companion files
  associated with the deleted instance.
- **Instance tracking** — update an internal map of known instances.
- **Confirmation logging** — record that an instance was deleted.
- **Backup preservation** — optionally preserve backups even when the instance
  is removed.

### Example — Clean up plugin settings for deleted instance

```cpp
static int on_instance_removed(void* mh, uint32_t hook_id,
                                void* payload, void* ud)
{
    auto* info = static_cast<MMCOInstanceInfo*>(payload);
    if (!info) return 0;

    MMCO_LOG(g_ctx,
        (std::string("Instance removed: ") + info->instance_name).c_str());

    /* Remove plugin-specific settings for this instance */
    std::string key = std::string("plugin.backup-system.")
                    + info->instance_id + ".enabled";
    /* Set to empty string to clear (no "delete key" API exists) */
    g_ctx->setting_set(g_ctx->module_handle, key.c_str(), "");

    return 0;
}

MMCO_EXPORT int mmco_init(MMCOContext* ctx)
{
    g_ctx = ctx;
    ctx->hook_register(ctx->module_handle,
                       MMCO_HOOK_INSTANCE_REMOVED,
                       on_instance_removed, nullptr);
    return 0;
}
```

### Example — Maintain an instance tracking cache

```cpp
#include <set>
#include <string>

static std::set<std::string> g_tracked_instances;

static int on_instance_created(void* mh, uint32_t hook_id,
                                void* payload, void* ud)
{
    auto* info = static_cast<MMCOInstanceInfo*>(payload);
    if (info && info->instance_id)
        g_tracked_instances.insert(info->instance_id);
    return 0;
}

static int on_instance_removed(void* mh, uint32_t hook_id,
                                void* payload, void* ud)
{
    auto* info = static_cast<MMCOInstanceInfo*>(payload);
    if (info && info->instance_id)
        g_tracked_instances.erase(info->instance_id);
    return 0;
}

MMCO_EXPORT int mmco_init(MMCOContext* ctx)
{
    g_ctx = ctx;

    /* Populate initial set from existing instances */
    int count = ctx->instance_count(ctx->module_handle);
    for (int i = 0; i < count; i++) {
        const char* id = ctx->instance_get_id(ctx->module_handle, i);
        if (id)
            g_tracked_instances.insert(id);
    }

    ctx->hook_register(ctx->module_handle,
                       MMCO_HOOK_INSTANCE_CREATED,
                       on_instance_created, nullptr);
    ctx->hook_register(ctx->module_handle,
                       MMCO_HOOK_INSTANCE_REMOVED,
                       on_instance_removed, nullptr);
    return 0;
}
```

### Interaction with `instance_count()`

When called from inside an `INSTANCE_REMOVED` callback, `instance_count()`
returns the count **excluding** the removed instance.  The removal has already
taken effect.

### Notes

- Calling `instance_get_name()` or `instance_get_path()` with the removed
  instance's ID will return `nullptr` — the instance is no longer in the list.
  Use the fields from the `MMCOInstanceInfo` payload instead.
- The instance directory at `instance_path` may still exist on disk.  Do not
  assume it has been deleted — the user may have chosen "remove from list only".
  Conversely, do not assume it exists — it may have been deleted already.
- If you store per-instance data in your own files/settings, this is the right
  hook to clean them up.  Leaving orphaned data is harmless but wasteful.

---

## Instance hook dispatch timeline

```
User clicks "Launch" on instance "My SMP"
  │
  ├─ LaunchController prepares launch
  │    ├─ Builds MMCOInstanceInfo { id="abc", name="My SMP",
  │    │                            path="/home/user/.meshmc/instances/abc",
  │    │                            minecraft_version=nullptr }
  │    ├─ dispatchHook(MMCO_HOOK_INSTANCE_PRE_LAUNCH, &hookInfo)  ◄── [1]
  │    │    ├─ Plugin A callback → returns 0 (allow)
  │    │    └─ Plugin B callback → returns 0 (allow)
  │    │
  │    │    (if any returned non-zero → launch aborted, no POST_LAUNCH)
  │    │
  │    └─ m_launcher->start()  →  JVM process created
  │
  │  ... Minecraft running ...
  │
  └─ LaunchController::onSucceeded()
       ├─ Builds MMCOInstanceInfo { id="abc", name="My SMP", ... }
       ├─ dispatchHook(MMCO_HOOK_INSTANCE_POST_LAUNCH, &hookInfo)  ◄── [2]
       │    ├─ Plugin A callback → returns 0
       │    └─ Plugin B callback → returns 0
       └─ emitSucceeded()


User creates a new instance "Fabric 1.21.5"
  │
  └─ InstanceList adds new entry
       └─ dispatchHook(MMCO_HOOK_INSTANCE_CREATED, &hookInfo)  ◄── [3]
            ├─ Plugin A → returns 0
            └─ Plugin B → returns 0


User deletes instance "Old World"
  │
  └─ InstanceList removes entry
       └─ dispatchHook(MMCO_HOOK_INSTANCE_REMOVED, &hookInfo)  ◄── [4]
            ├─ Plugin A → returns 0
            └─ Plugin B → returns 0
       └─ (optional) delete instance directory from disk
```

**Legend:**

- **[1] `INSTANCE_PRE_LAUNCH`** — cancellable, fires before JVM start
- **[2] `INSTANCE_POST_LAUNCH`** — notification, fires after successful start
- **[3] `INSTANCE_CREATED`** — notification, instance fully registered
- **[4] `INSTANCE_REMOVED`** — notification, instance already removed from list

---

## See also

- [Application hooks](app-hooks.md) — `APP_INITIALIZED`, `APP_SHUTDOWN`, `SETTINGS_CHANGED`
- [UI hooks](ui-hooks.md) — `UI_MAIN_READY`, `UI_INSTANCE_PAGES`, `UI_CONTEXT_MENU`
- [Hooks Reference overview](README.md) — dispatch model, registration API recap, common patterns
- [API Reference § Instances](../api-reference/s03-instances/) — `instance_count()`, `instance_get_id()`, `instance_get_name()`, `instance_get_path()`
