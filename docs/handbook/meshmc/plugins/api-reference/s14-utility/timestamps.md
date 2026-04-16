# Timestamps — `get_timestamp()` {#timestamps}

> **Section:** S14 (Utility) · **Header:** `PluginAPI.h` · **Trampoline:** `PluginManager::api_get_timestamp` · **Backend:** Qt `QDateTime::currentSecsSinceEpoch()`

---

## `get_timestamp()` — Get the current UNIX epoch time {#get_timestamp}

### Synopsis

```c
int64_t get_timestamp(void* mh);
```

Returns the current wall-clock time as the number of **whole seconds**
elapsed since the UNIX epoch (1970-01-01 00:00:00 UTC).

### Parameters

| # | Name | Type | Description |
|---|------|------|-------------|
| 1 | `mh` | `void*` | Module handle. Pass `ctx->module_handle`. |

> The `mh` parameter is **not used** by the current implementation —
> the function ignores it entirely. However, you must still pass a
> valid module handle for forward compatibility. Future versions may
> use it for per-module rate limiting or audit logging.

### Return Value

| Condition | Value |
|-----------|-------|
| Always | A positive `int64_t` representing seconds since 1970-01-01T00:00:00Z. |

This function **cannot fail**. The `int64_t` type guarantees no overflow
until the year 292,277,026,596 CE.

### Time Unit: Seconds, Not Milliseconds

The returned value is in **whole seconds** (i.e. UNIX epoch seconds,
also known as `time_t` on most platforms). It is **not** milliseconds
and it is **not** an ISO 8601 string.

| What it IS | Seconds since UNIX epoch (UTC) |
|------------|-------------------------------|
| What it is NOT | Milliseconds, microseconds, or nanoseconds |
| What it is NOT | An ISO 8601 formatted string |
| What it is NOT | Local time (it is always UTC-based) |

If you need millisecond precision, this function does not provide it.
Use platform-specific APIs (`clock_gettime`, `gettimeofday`,
`QueryPerformanceCounter`) directly — but be aware that doing so is
outside the MMCO API contract and may affect portability.

### Implementation

```cpp
int64_t PluginManager::api_get_timestamp(void* mh)
{
    (void)mh;
    return QDateTime::currentSecsSinceEpoch();
}
```

The implementation is a one-liner:
1. The `mh` parameter is explicitly cast to `void` (unused).
2. `QDateTime::currentSecsSinceEpoch()` is called. This is a static
   Qt function that returns the number of seconds since the UNIX epoch
   as a `qint64` (which is `int64_t` on all supported platforms).

There is no `tempString` involvement, no allocation, no string
conversion, and no module-state access.

### String Ownership

Not applicable. `get_timestamp()` returns `int64_t` directly. There is
no pointer, no buffer, and no ownership concern. The return value is a
plain integer copied onto the stack.

This makes `get_timestamp()` the safest function in the utility section
to call at any point — it has no side effects on any shared state
(including `tempString`).

### Clock Source and Accuracy

`QDateTime::currentSecsSinceEpoch()` delegates to the operating
system's real-time clock:

| Platform | Underlying syscall | Typical resolution |
|----------|-------------------|-------------------|
| Linux | `clock_gettime(CLOCK_REALTIME, ...)` | 1 ns (returned truncated to seconds) |
| macOS | `gettimeofday()` | 1 µs (returned truncated to seconds) |
| Windows | `GetSystemTimeAsFileTime()` | ~15.6 ms (returned truncated to seconds) |

The returned value is always truncated to whole seconds (floor). The
clock is a **wall clock** — it can jump forward or backward if the
system time is adjusted (via NTP, manual change, or daylight saving
transitions). It is **not** a monotonic clock.

> **Do not use `get_timestamp()` for measuring elapsed time or
> benchmarking.** Use it only for wall-clock timestamps (log entries,
> file dates, expiry checks measured in minutes/hours/days).

### Timezone

The epoch value is inherently UTC. If you need to display local time,
you must perform the timezone conversion yourself. The MMCO API does
not provide timezone utilities.

---

## Use Cases

### Timestamped Log Entries

```c
static void log_with_time(MMCOContext* ctx, const char* message)
{
    void* mh = ctx->module_handle;
    int64_t now = ctx->get_timestamp(mh);

    char buf[512];
    snprintf(buf, sizeof(buf), "[%lld] %s", (long long)now, message);
    ctx->log_info(mh, buf);
}
```

### Cache Expiry

```c
#define CACHE_TTL_SECONDS (3600)  /* 1 hour */

static int is_cache_valid(MMCOContext* ctx, int64_t cache_created_at)
{
    int64_t now = ctx->get_timestamp(ctx->module_handle);
    return (now - cache_created_at) < CACHE_TTL_SECONDS;
}
```

### Timestamped Backup Filenames

Combining `get_timestamp()` with the filesystem API (S10) to create
uniquely-named backup files:

```c
#include "mmco_sdk.h"
#include <stdio.h>
#include <string.h>

static int create_timestamped_backup(MMCOContext* ctx,
                                     const char* instance_id)
{
    void* mh = ctx->module_handle;

    /* Get the instance game root */
    const char* game_root = ctx->instance_get_game_root(mh, instance_id);
    if (!game_root)
        return -1;

    /* Copy before the next API call clobbers tempString */
    char src_dir[512];
    snprintf(src_dir, sizeof(src_dir), "%s", game_root);

    /* Build a timestamped filename */
    int64_t now = ctx->get_timestamp(mh);
    const char* data_dir = ctx->fs_plugin_data_dir(mh);
    /* data_dir is NOT in tempString — it's in ModuleRuntime::dataDir,
       so it remains valid */

    char zip_path[512];
    snprintf(zip_path, sizeof(zip_path),
             "%s/backup-%lld.zip", data_dir, (long long)now);

    /* Check if a backup with this exact timestamp already exists */
    if (ctx->fs_exists_abs(mh, zip_path)) {
        ctx->log_warn(mh, "Backup already exists for this second, skipping");
        return 1;
    }

    /* Compress the game directory into the backup */
    int rc = ctx->zip_compress_dir(mh, zip_path, src_dir);
    if (rc != 0) {
        ctx->log_error(mh, "Failed to create backup archive");
        return -1;
    }

    char msg[256];
    snprintf(msg, sizeof(msg), "Backup created: %s", zip_path);
    ctx->log_info(mh, msg);

    return 0;
}
```

### Cooldown / Rate Limiting

```c
static int64_t g_last_action_time = 0;
#define COOLDOWN_SECONDS 30

static int can_perform_action(MMCOContext* ctx)
{
    int64_t now = ctx->get_timestamp(ctx->module_handle);
    if (now - g_last_action_time < COOLDOWN_SECONDS) {
        char buf[128];
        snprintf(buf, sizeof(buf),
                 "Please wait %lld more seconds",
                 (long long)(COOLDOWN_SECONDS - (now - g_last_action_time)));
        ctx->log_warn(ctx->module_handle, buf);
        return 0;
    }
    g_last_action_time = now;
    return 1;
}
```

### Persisting a Timestamp to Disk

Store and retrieve a timestamp via the sandboxed filesystem API (S10):

```c
static const char* TS_FILE = "last_run.dat";

static void save_run_timestamp(MMCOContext* ctx)
{
    void* mh = ctx->module_handle;
    int64_t now = ctx->get_timestamp(mh);
    char buf[32];
    int len = snprintf(buf, sizeof(buf), "%lld", (long long)now);
    ctx->fs_write(mh, TS_FILE, buf, len);
}

static int64_t load_run_timestamp(MMCOContext* ctx)
{
    char buf[32] = {0};
    int64_t bytes = ctx->fs_read(ctx->module_handle, TS_FILE,
                                 buf, sizeof(buf) - 1);
    if (bytes <= 0)
        return 0;
    return (int64_t)strtoll(buf, NULL, 10);
}
```

---

## Formatting Epoch Seconds as Human-Readable Strings

The MMCO API does not include a date-formatting function. Use the C
standard library:

```c
#include <time.h>

static void format_timestamp_utc(int64_t epoch, char* buf, size_t sz)
{
    time_t t = (time_t)epoch;
    struct tm utc;
#ifdef _WIN32
    gmtime_s(&utc, &t);
#else
    gmtime_r(&t, &utc);
#endif
    strftime(buf, sz, "%Y-%m-%d %H:%M:%S UTC", &utc);
}
/* Result e.g. "2026-04-15 12:30:00 UTC" */
```

For local time, replace `gmtime_r` with `localtime_r` (POSIX) or
`localtime_s` (Windows).

---

## Comparison with `time()`

`get_timestamp()` returns the same value as `time(NULL)` on all
supported platforms. The advantage of the MMCO function is portability
(Qt's abstraction), consistency (all plugins use the same source), and
forward compatibility (if MeshMC adds virtual time for testing, plugins
using `get_timestamp()` will automatically benefit). Calling
`time(NULL)` directly is permitted but not recommended.

---

## Error Conditions

`get_timestamp()` cannot fail. There are no error return values.

| Condition | Return value |
|-----------|-------------|
| Normal operation | Positive `int64_t` (seconds since epoch) |
| System clock set to pre-epoch date | 0 or negative (extremely unlikely) |

---

## Cross-References

| Section | Relationship |
|---------|-------------|
| **S10 (Filesystem)** | Combine timestamps with `fs_write()` for timestamped data, or use in backup-file naming with `fs_plugin_data_dir()`. |
| **S3 (Instances)** | `instance_get_last_launch()` and `instance_get_last_play_time()` return timestamps in the same epoch-seconds format. Compare them with `get_timestamp()` to compute "time since last played". |
| **S6 (Worlds)** | `world_get_last_played()` returns epoch seconds per world — same unit as `get_timestamp()`. |
| **S8 (HTTP)** | Use timestamps for HTTP cache control (If-Modified-Since headers, cache expiry). |
