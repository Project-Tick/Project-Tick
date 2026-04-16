# Section 06 — Worlds API

> **Header:** `PluginAPI.h` (function pointer declarations) · **SDK:** `mmco_sdk.h` (struct definition) · **Implementation:** `PluginManager.cpp` (static trampolines)

---

## Overview

The Worlds API provides plugins with complete access to the **Minecraft
save data** stored inside an instance. Every Minecraft instance managed by
MeshMC keeps its world saves in the standard `.minecraft/saves/` directory
layout. This section exposes functions to enumerate those worlds, read
their metadata (name, seed, game mode, last-played timestamp), and perform
mutating operations (rename, delete, install, refresh).

Unlike the Mods API (Section 05), which is parameterized by a `type` string
to address different mod categories, the Worlds API operates on a single
flat list — there is only one kind of "world" per instance.

```text
┌─────────────────────────────────────────────────────────────────────────────────┐
│  QUERY                                                                         │
│  1.  world_count(inst)              → number of discovered worlds              │
│  2.  world_get_name(inst, index)    → human-readable world name (level.dat)    │
│  3.  world_get_folder(inst, index)  → directory name inside saves/             │
│  4.  world_get_seed(inst, index)    → 64-bit world seed                        │
│  5.  world_get_game_type(inst, idx) → survival / creative / adventure / …      │
│  6.  world_get_last_played(inst, i) → epoch-millis timestamp                   │
│                                                                                │
│  MUTATE                                                                        │
│  7.  world_delete(inst, index)      → permanently remove world folder          │
│  8.  world_rename(inst, index, n)   → change the world's display name          │
│  9.  world_install(inst, filepath)  → import a world archive / folder          │
│ 10.  world_refresh(inst)            → rescan the saves/ directory              │
└─────────────────────────────────────────────────────────────────────────────────┘
```

All ten function pointers live in `MMCOContext` and are declared in
`PluginAPI.h`:

```c
/* S06 — World Management  (from MMCOContext in PluginAPI.h) */

/* Query */
int         (*world_count)         (void* mh, const char* instance_id);
const char* (*world_get_name)      (void* mh, const char* instance_id,
                                    int index);
const char* (*world_get_folder)    (void* mh, const char* instance_id,
                                    int index);
int64_t     (*world_get_seed)      (void* mh, const char* instance_id,
                                    int index);
int         (*world_get_game_type) (void* mh, const char* instance_id,
                                    int index);
int64_t     (*world_get_last_played)(void* mh, const char* instance_id,
                                     int index);

/* Mutate */
int (*world_delete)  (void* mh, const char* instance_id, int index);
int (*world_rename)  (void* mh, const char* instance_id, int index,
                      const char* new_name);
int (*world_install) (void* mh, const char* instance_id,
                      const char* filepath);
int (*world_refresh) (void* mh, const char* instance_id);
```

---

## How Minecraft Worlds Are Stored

To use this API effectively you need to understand the on-disk layout that
MeshMC (and vanilla Minecraft) use for save data. This section explains
the directory structure, the key metadata file `level.dat`, and how
MeshMC's `WorldList` model discovers and indexes worlds.

### Directory layout

Every Minecraft instance has a **game directory** (returned by
`instance_get_game_dir()` from [S03](../s03-instances/)). Inside that
directory, the standard Minecraft structure is:

```text
.minecraft/                    ← instance game directory
├── saves/                     ← worlds root — this is what the API enumerates
│   ├── New World/             ← one world = one folder
│   │   ├── level.dat          ← NBT-encoded metadata (name, seed, game-mode)
│   │   ├── level.dat_old      ← backup copy of level.dat
│   │   ├── region/            ← Anvil region files (.mca)
│   │   │   ├── r.0.0.mca
│   │   │   └── ...
│   │   ├── DIM-1/             ← Nether dimension
│   │   │   └── region/
│   │   ├── DIM1/              ← End dimension
│   │   │   └── region/
│   │   ├── data/              ← map.dat, raids, etc.
│   │   ├── datapacks/         ← custom data packs
│   │   ├── playerdata/        ← per-player NBT files
│   │   └── icon.png           ← world icon (displayed in MC UI)
│   ├── SMP Server Backup/
│   │   └── ...
│   └── Creative Flat/
│       └── ...
├── resourcepacks/
├── shaderpacks/
├── mods/                      ← (if Forge / Fabric)
├── options.txt
└── ...
```

The critical observations:

1. **One folder = one world.** The folder name is *not* the display name.
   The human-readable name is stored inside `level.dat` (`Data.LevelName`).
2. **`level.dat` is authoritative.** MeshMC reads this file (NBT-encoded,
   gzip-compressed) to extract the world name, seed, game type, and
   last-played timestamp.
3. **Folder name vs. display name.** `world_get_name()` returns the
   display name from `level.dat`. `world_get_folder()` returns the
   directory name on disk. These two values can differ.

### The `saves/` directory path

The path to the saves directory can be obtained in two ways:

| Method | Returns |
|--------|---------|
| `instance_get_worlds_dir(mh, id)` | Absolute path to the `saves/` folder |
| `instance_get_game_dir(mh, id)` + `"/saves"` | Manual construction |

The `instance_get_worlds_dir()` function (documented in
[S03 — Instance Properties](../s03-instances/instance-properties.md)) is
the preferred method; it accounts for edge cases where the game directory
might be overridden.

### World sizing semantics

The Worlds API does **not** include a dedicated `world_get_size()` function
pointer. If your plugin needs to calculate the size of a world (e.g., for
a backup / disk-usage reporter), you must combine two APIs:

1. **`world_get_folder()`** — get the folder name
2. **`instance_get_worlds_dir()`** — get the parent `saves/` path
3. **`fs_file_size()` / `fs_list_dir()`** from [S10 — Filesystem](../s10-filesystem/) — walk
   the directory tree and sum file sizes

The reason there is no built-in size function is that computing the
recursive size of a world directory can be expensive (hundreds of region
files, potentially gigabytes of data), and a blocking call would stall
the UI thread. Plugins that need this should perform the calculation
asynchronously or accept the cost explicitly.

**Example — computing world size manually:**

```c
struct SizeAccum {
    int64_t total;
};

static void size_cb(void* ud, const char* name, int is_dir) {
    (void)name; (void)is_dir;
    /* This callback receives each entry; we would need full paths
       to stat each file. See the full example in world-query.md. */
}

int64_t compute_world_size(MMCOContext* ctx, void* mh,
                           const char* inst_id, int world_idx)
{
    const char* saves = ctx->instance_get_worlds_dir(mh, inst_id);
    const char* folder = ctx->world_get_folder(mh, inst_id, world_idx);
    if (!saves || !folder) return -1;

    char path[4096];
    snprintf(path, sizeof(path), "%s/%s", saves, folder);

    /* Use fs_list_dir recursively, or shell out, or use
       platform helpers — see world-query.md for full code. */
    return -1; /* placeholder */
}
```

A complete, working recursive size calculator is provided in
[world-query.md § Practical Example: World Size Reporter](world-query.md#practical-example-world-size-reporter).

---

## Function Summary

| Function | Sub-page | Purpose | Returns |
|----------|----------|---------|---------|
| [`world_count()`](world-query.md#world_count) | Query | Number of worlds in instance | `int` (≥ 0) |
| [`world_get_name()`](world-query.md#world_get_name) | Query | Human-readable world name | `const char*` (nullable) |
| [`world_get_folder()`](world-query.md#world_get_folder) | Query | On-disk directory name | `const char*` (nullable) |
| [`world_get_seed()`](world-query.md#world_get_seed) | Query | 64-bit world seed | `int64_t` |
| [`world_get_game_type()`](world-query.md#world_get_game_type) | Query | Game mode (0–3) | `int` |
| [`world_get_last_played()`](world-query.md#world_get_last_played) | Query | Last-played epoch millis | `int64_t` |
| [`world_delete()`](world-query.md#world_delete) | Mutate | Delete a world permanently | `int` (0 / −1) |
| [`world_rename()`](world-query.md#world_rename) | Mutate | Rename a world's display name | `int` (0 / −1) |
| [`world_install()`](world-query.md#world_install) | Mutate | Import a world file/folder | `int` (0 / −1) |
| [`world_refresh()`](world-query.md#world_refresh) | Mutate | Rescan the saves directory | `int` (0 / −1) |

---

## Key Concepts

### Instance ID and MinecraftInstance resolution

Every function in this section takes an `instance_id` parameter (C string).
This is the same ID returned by `instance_get_id()` from
[S03 — Instance Query](../s03-instances/). Internally the implementation
calls `resolveMC()`, which:

1. Looks up the `BaseInstance` by ID in the global instance list.
2. Attempts a `dynamic_cast` to `MinecraftInstance*`.
3. Returns `nullptr` if the instance is not a Minecraft instance.

If the cast fails — for example, if a future instance type (e.g., a
generic game launcher entry) is added to MeshMC — all world functions
will return their error sentinel: `0` for counts, `nullptr` for strings,
`-1` for mutating operations.

> **Rule of thumb:** Always check return values. A `world_count()` of `0`
> might mean "no worlds" or "not a Minecraft instance."

### The `WorldList` model

Internally each `MinecraftInstance` owns a `WorldList` object. This is a
Qt model that maintains a cached, sorted list of `World` entries. The cache
is populated by scanning the `saves/` directory on demand (typically when
the user opens the instance's world management page, or when a plugin
calls `world_refresh()`).

Key points:

- **Lazy loading.** The world list may not be populated until something
  requests it. `world_count()` triggers a load if needed.
- **Index stability.** Indices are valid only until the next mutating
  operation. After `world_delete()`, `world_install()`, or
  `world_refresh()`, all previously-obtained indices are **invalidated**.
  Always re-query `world_count()` after mutations.
- **Thread safety.** All world functions must be called from the **main
  (GUI) thread**. The `WorldList` model is a Qt object and is not
  thread-safe.

### String ownership and `tempString`

All functions that return `const char*` store the result in the
per-module `tempString` buffer inside `ModuleRuntime`. This means:

1. **The pointer is valid until the next API call** that returns a string.
   Copy the value immediately if you need it to persist.
2. **Do not free the pointer.** The host owns the memory.
3. **Calling another string-returning API overwrites the previous result.**

This is the same ownership model used throughout S03, S05, and all other
sections that return strings. See
[S01 — Lifecycle](../s01-lifecycle/) for the full explanation.

```c
/* WRONG — second call invalidates first pointer */
const char* name  = ctx->world_get_name(mh, inst, 0);
const char* name2 = ctx->world_get_name(mh, inst, 1);
/* name is now INVALID — it points to name2's content */

/* CORRECT — copy before next call */
const char* raw = ctx->world_get_name(mh, inst, 0);
char name_copy[256];
if (raw) snprintf(name_copy, sizeof(name_copy), "%s", raw);

raw = ctx->world_get_name(mh, inst, 1);
char name2_copy[256];
if (raw) snprintf(name2_copy, sizeof(name2_copy), "%s", raw);
```

### Game type values

`world_get_game_type()` returns an integer corresponding to Minecraft's
`GameType` enum stored in `level.dat`:

| Value | Game Mode | Constant |
|-------|-----------|----------|
| `0` | Survival | `GameType::Survival` |
| `1` | Creative | `GameType::Creative` |
| `2` | Adventure | `GameType::Adventure` |
| `3` | Spectator | `GameType::Spectator` |
| `-1` | *Error / not found* | — |

### Timestamps

`world_get_last_played()` returns the `LastPlayed` field from `level.dat`,
converted to **milliseconds since the Unix epoch** (January 1, 1970,
00:00:00 UTC). This is the same unit used by Java's
`System.currentTimeMillis()` and by Minecraft internally.

A return value of `0` indicates either an error (world not found) or a
world that has never been played (both cases are indistinguishable).

---

## Cross-References

| Section | Relevance |
|---------|-----------|
| [S03 — Instance Query](../s03-instances/) | `instance_get_worlds_dir()` returns the saves path; `instance_get_id()` provides the `instance_id` parameter |
| [S05 — Mods](../s05-mods/) | Structurally similar API (count/get-by-index pattern); mod types vs. world list |
| [S09 — Zip / Archive](../s09-zip/) | `zip_compress_dir()` to back up a world folder; `zip_extract()` to restore |
| [S10 — Filesystem](../s10-filesystem/) | `fs_file_size()`, `fs_list_dir()`, `fs_exists_abs()` for world size calculation and path verification |
| [S12 — UI Dialogs](../s12-ui-dialogs/) | `ui_file_open_dialog()` to let user pick a world archive; `ui_confirm_dialog()` before destructive operations |

---

## Quick-Start Example

A minimal plugin snippet that lists all worlds in the first instance:

```c
#include <mmco_sdk.h>
#include <stdio.h>
#include <string.h>

static MMCOContext* ctx;
static void* mh;

void list_all_worlds(void)
{
    int inst_count = ctx->instance_count(mh);
    if (inst_count <= 0) {
        ctx->log(mh, "No instances found.");
        return;
    }

    for (int i = 0; i < inst_count; i++) {
        const char* inst_id = ctx->instance_get_id(mh, i);
        if (!inst_id) continue;

        /* Copy because world_ calls overwrite tempString */
        char id_buf[256];
        snprintf(id_buf, sizeof(id_buf), "%s", inst_id);

        int wc = ctx->world_count(mh, id_buf);
        if (wc <= 0) continue;

        const char* inst_name = ctx->instance_get_name(mh, i);
        char iname[256] = "(unknown)";
        if (inst_name) snprintf(iname, sizeof(iname), "%s", inst_name);

        char msg[512];
        snprintf(msg, sizeof(msg), "Instance '%s' has %d world(s):", iname, wc);
        ctx->log(mh, msg);

        for (int w = 0; w < wc; w++) {
            const char* wname = ctx->world_get_name(mh, id_buf, w);
            char wn[256] = "(unnamed)";
            if (wname) snprintf(wn, sizeof(wn), "%s", wname);

            const char* wfolder = ctx->world_get_folder(mh, id_buf, w);
            char wf[256] = "?";
            if (wfolder) snprintf(wf, sizeof(wf), "%s", wfolder);

            int64_t seed = ctx->world_get_seed(mh, id_buf, w);
            int gtype    = ctx->world_get_game_type(mh, id_buf, w);

            char line[1024];
            snprintf(line, sizeof(line),
                     "  [%d] \"%s\" (folder: %s, seed: %lld, mode: %d)",
                     w, wn, wf, (long long)seed, gtype);
            ctx->log(mh, line);
        }
    }
}

int mmco_init(MMCOContext* c, void* handle)
{
    ctx = c;
    mh  = handle;
    list_all_worlds();
    return 0;
}
```

For extended examples — including a world size calculator, a world backup
trigger using S09 Zip, and a world-management UI page — see
[world-query.md](world-query.md).

---

## Implementation Notes (Contributors)

### Trampoline chain

Each API function follows the standard trampoline pattern:

```text
Plugin calls  ctx->world_count(mh, inst_id)
   → MMCOContext function pointer
   → PluginManager::api_world_count(mh, inst)   [static]
   → rt(mh)                                     [recover ModuleRuntime*]
   → resolveWorldList(r, inst)                  [helper]
      → resolveMC(r, inst)                      [look up MinecraftInstance*]
      → mc->worldList()                         [get shared_ptr<WorldList>]
   → wl->size()                                 [delegate to Qt model]
```

### `resolveWorldList()`

This is a file-local helper in `PluginManager.cpp`:

```cpp
static std::shared_ptr<WorldList>
resolveWorldList(PluginManager::ModuleRuntime* r, const char* inst_id)
{
    auto* mc = resolveMC(r, inst_id);
    if (!mc)
        return nullptr;
    return mc->worldList();
}
```

It returns `nullptr` if:
- `inst_id` is `nullptr` or empty
- No instance with that ID exists
- The instance is not a `MinecraftInstance`
- The instance has no `WorldList` (should not happen for MC instances)

### Wire-up in `initContext()`

```cpp
// S6 — World Management
ctx.world_count          = api_world_count;
ctx.world_get_name       = api_world_get_name;
ctx.world_get_folder     = api_world_get_folder;
ctx.world_get_seed       = api_world_get_seed;
ctx.world_get_game_type  = api_world_get_game_type;
ctx.world_get_last_played = api_world_get_last_played;
ctx.world_delete         = api_world_delete;
ctx.world_rename         = api_world_rename;
ctx.world_install        = api_world_install;
ctx.world_refresh        = api_world_refresh;
```

---

## Sub-Pages

| Page | Contents |
|------|----------|
| [world-query.md](world-query.md) | Full documentation for all 10 world functions: `world_count()` through `world_refresh()`, with parameter tables, return semantics, error conditions, and extended examples |
