# Accounts API — Full Function Reference

> All 7 account management function pointers documented in detail.
> For an overview of the account model, security boundary, and key
> concepts, see [README.md](README.md).

---

## Table of Contents

- [Query Functions](#query-functions)
  - [`account_count()`](#account_count)
  - [`account_get_profile_name()`](#account_get_profile_name)
  - [`account_get_profile_id()`](#account_get_profile_id)
  - [`account_get_type()`](#account_get_type)
  - [`account_get_state()`](#account_get_state)
  - [`account_is_active()`](#account_is_active)
  - [`account_get_default_index()`](#account_get_default_index)
- [Practical Examples](#practical-examples)
  - [Account Lister Plugin](#account-lister-plugin)
  - [Profile Display Widget](#profile-display-widget)
  - [Default Account Guard](#default-account-guard)
  - [UUID Format Conversion Helper](#uuid-format-conversion-helper)

---

# Query Functions

All seven account functions are **read-only**. They inspect the in-memory
`AccountList` model managed by MeshMC's `Application` object. They do not
make network requests, modify account state, or access authentication
tokens.

Every function takes `void* mh` (the module handle) as its first
parameter. Functions that address a specific account take an `int index`
which must be in the range `[0, account_count() - 1]`.

---

## `account_count()`

Returns the total number of Minecraft accounts registered in MeshMC.

### Signature

```c
int (*account_count)(void* mh);
```

### Parameters

| Parameter | Type | Description |
|-----------|------|-------------|
| `mh` | `void*` | Module handle passed to `mmco_init()`. Identifies the calling plugin. Must not be `NULL`. |

### Return Value

| Condition | Return |
|-----------|--------|
| Success | Number of accounts (≥ 0). A value of `0` means no accounts are logged in. |
| `Application` pointer is `NULL` | `0` |
| Account list is `NULL` | `0` |
| `mh` is `NULL` | Undefined behavior (crash likely) |

### Thread Safety

Must be called from the **main (GUI) thread** only.

### Implementation Detail

```cpp
int PluginManager::api_account_count(void* mh)
{
    auto* r = rt(mh);
    auto* app = r->manager->m_app;
    if (!app || !app->accounts())
        return 0;
    return app->accounts()->count();
}
```

The function delegates to `AccountList::count()`, which returns the size
of the internal `QList<MinecraftAccountPtr>`. This is an O(1) operation.

### Usage Notes

- The count can change during the lifetime of the plugin if the user
  adds or removes accounts through the MeshMC UI. However, account list
  modifications only happen on the main thread, so the count is stable
  within a single hook callback or API call sequence on the main thread.
- A return of `0` means the user has not logged in at all. Plugins that
  require an active account should handle this gracefully.

### Example

```c
int n = ctx->account_count(mh);
if (n == 0) {
    ctx->log(mh, 1, "No accounts found — please log in first.");
    return;
}
```

---

## `account_get_profile_name()`

Returns the Minecraft profile name (in-game username / gamertag) for the
account at the given index.

### Signature

```c
const char* (*account_get_profile_name)(void* mh, int index);
```

### Parameters

| Parameter | Type | Description |
|-----------|------|-------------|
| `mh` | `void*` | Module handle. Must not be `NULL`. |
| `index` | `int` | Zero-based index into the account list. Must be in range `[0, account_count() - 1]`. |

### Return Value

| Condition | Return |
|-----------|--------|
| Success | Null-terminated UTF-8 string containing the profile name (e.g. `"Steve"`, `"xX_Gamer_Xx"`). |
| Index out of range | `NULL` |
| Account has no profile | A placeholder string like `"No profile (Xbox gamertag)"` |
| Account list unavailable | `NULL` |
| `mh` is `NULL` | Undefined behavior |

### String Ownership

The returned pointer is stored in the per-module `tempString` buffer. It
is valid only until the **next** call to any `const char*`-returning API
function from the same module handle. Copy the string immediately if you
need it to persist:

```c
const char* tmp = ctx->account_get_profile_name(mh, i);
char* name = tmp ? strdup(tmp) : NULL;
```

### Implementation Detail

```cpp
const char* PluginManager::api_account_get_profile_name(void* mh, int idx)
{
    auto* r = rt(mh);
    auto* app = r->manager->m_app;
    if (!app || !app->accounts())
        return nullptr;
    auto acc = app->accounts()->at(idx);
    if (!acc)
        return nullptr;
    r->tempString = acc->profileName().toStdString();
    return r->tempString.c_str();
}
```

Internally, `profileName()` reads from `AccountData::minecraftProfile.name`.
If the profile name is empty (i.e. the MSA account has not been linked to
a Minecraft profile), MeshMC returns a translated placeholder string of
the form `"No profile (gamertag)"` where the gamertag is the Xbox Live
display name.

### What the Profile Name Represents

The profile name is the **Minecraft in-game name** — the name visible in:
- Multiplayer player lists
- Chat messages
- The title screen
- Server whitelists

This is *not* the Xbox Live gamertag (though for many users they are the
same). The profile name is specifically the name registered with Mojang's
profile service.

### Example

```c
for (int i = 0; i < ctx->account_count(mh); i++) {
    const char* tmp = ctx->account_get_profile_name(mh, i);
    char name[256];
    snprintf(name, sizeof(name), "%s", tmp ? tmp : "(unknown)");
    ctx->log(mh, 0, name);
}
```

---

## `account_get_profile_id()`

Returns the Minecraft profile UUID for the account at the given index.

### Signature

```c
const char* (*account_get_profile_id)(void* mh, int index);
```

### Parameters

| Parameter | Type | Description |
|-----------|------|-------------|
| `mh` | `void*` | Module handle. Must not be `NULL`. |
| `index` | `int` | Zero-based index into the account list. Must be in range `[0, account_count() - 1]`. |

### Return Value

| Condition | Return |
|-----------|--------|
| Success | Null-terminated string containing the UUID in **undashed hexadecimal** format (32 characters, e.g. `"a1b2c3d4e5f6a7b8c9d0e1f2a3b4c5d6"`). |
| Index out of range | `NULL` |
| Account has no profile | `NULL` (empty profile ID maps to empty string, returned as `NULL` since the string has zero length — see notes) |
| Account list unavailable | `NULL` |

### UUID Format

The UUID is returned in **Mojang API format** — a 32-character lowercase
hexadecimal string with **no dashes**:

```text
Mojang format:  a1b2c3d4e5f6a7b8c9d0e1f2a3b4c5d6
Standard UUID:  a1b2c3d4-e5f6-a7b8-c9d0-e1f2a3b4c5d6
```

If you need the standard dashed format (RFC 4122), you must insert dashes
yourself. Here is a utility function:

```c
/**
 * Convert an undashed 32-char Mojang UUID to standard dashed format.
 * Returns 0 on success, -1 if input is not exactly 32 hex chars.
 * Output buffer must be at least 37 bytes (36 chars + null terminator).
 */
static int uuid_to_dashed(const char* in, char* out, size_t out_sz) {
    if (!in || !out || out_sz < 37)
        return -1;
    size_t len = strlen(in);
    if (len != 32)
        return -1;

    /* Validate hex characters */
    for (size_t i = 0; i < 32; i++) {
        char c = in[i];
        if (!((c >= '0' && c <= '9') ||
              (c >= 'a' && c <= 'f') ||
              (c >= 'A' && c <= 'F')))
            return -1;
    }

    /* 8-4-4-4-12 */
    snprintf(out, out_sz, "%.8s-%.4s-%.4s-%.4s-%.12s",
             in, in + 8, in + 12, in + 16, in + 20);
    return 0;
}
```

### String Ownership

Same as all `const char*` returns — valid until the next string-returning
API call from the same module handle. Copy immediately.

### Implementation Detail

```cpp
const char* PluginManager::api_account_get_profile_id(void* mh, int idx)
{
    auto* r = rt(mh);
    auto* app = r->manager->m_app;
    if (!app || !app->accounts())
        return nullptr;
    auto acc = app->accounts()->at(idx);
    if (!acc)
        return nullptr;
    r->tempString = acc->profileId().toStdString();
    return r->tempString.c_str();
}
```

The profile ID originates from `AccountData::minecraftProfile.id`, which
is the UUID string returned by the Mojang profile API during authentication.
It is stored as received — an undashed lowercase hex string.

### When Profile ID Is Empty

If a Microsoft Account has been linked to Xbox Live but does **not** own
Minecraft (i.e. `ownsMinecraft == false` in `MinecraftEntitlement`), the
profile will have an empty ID. In this case, `profileId()` returns an
empty `QString`, which converts to an empty `std::string`, and the function
returns a pointer to an empty string `""` (not `NULL`).

Plugins should check for both `NULL` and empty string:

```c
const char* uuid = ctx->account_get_profile_id(mh, i);
if (!uuid || uuid[0] == '\0') {
    /* Account has no Minecraft profile */
}
```

### Example

```c
const char* tmp = ctx->account_get_profile_id(mh, 0);
char uuid_raw[64];
snprintf(uuid_raw, sizeof(uuid_raw), "%s", tmp ? tmp : "");

char uuid_dashed[37];
if (uuid_to_dashed(uuid_raw, uuid_dashed, sizeof(uuid_dashed)) == 0) {
    printf("Player UUID: %s\n", uuid_dashed);
} else {
    printf("Invalid or missing UUID: '%s'\n", uuid_raw);
}
```

---

## `account_get_type()`

Returns the authentication type string for the account at the given index.

### Signature

```c
const char* (*account_get_type)(void* mh, int index);
```

### Parameters

| Parameter | Type | Description |
|-----------|------|-------------|
| `mh` | `void*` | Module handle. Must not be `NULL`. |
| `index` | `int` | Zero-based index into the account list. |

### Return Value

| Condition | Return |
|-----------|--------|
| Success | Null-terminated string identifying the authentication type. Currently always `"msa"`. |
| Index out of range | `NULL` |
| Account list unavailable | `NULL` |

### Known Type Strings

| String | Meaning |
|--------|---------|
| `"msa"` | Microsoft Account (Xbox Live → Mojang services). This is the only type currently supported. |

### Implementation Detail

```cpp
const char* PluginManager::api_account_get_type(void* mh, int idx)
{
    auto* r = rt(mh);
    auto* app = r->manager->m_app;
    if (!app || !app->accounts())
        return nullptr;
    auto acc = app->accounts()->at(idx);
    if (!acc)
        return nullptr;
    r->tempString = acc->typeString().toStdString();
    return r->tempString.c_str();
}
```

The `typeString()` method on `MinecraftAccount` is hard-coded to return
`"msa"` in the current codebase:

```cpp
QString typeString() const
{
    return "msa";
}
```

### Future-Proofing

Although `"msa"` is currently the only value, plugins should not assume
this will always be the case. Possible future values might include:

- `"offline"` — An offline / demo account
- `"ely"` — Third-party authentication (e.g. Ely.by)
- `"local"` — Local-only testing profile

Compare using `strcmp()` rather than checking string length or first
character.

### Example

```c
const char* tmp = ctx->account_get_type(mh, i);
if (tmp && strcmp(tmp, "msa") == 0) {
    printf("Account %d uses Microsoft authentication\n", i);
}
```

---

## `account_get_state()`

Returns the current authentication state of the account at the given
index, as an integer corresponding to the `AccountState` enum.

### Signature

```c
int (*account_get_state)(void* mh, int index);
```

### Parameters

| Parameter | Type | Description |
|-----------|------|-------------|
| `mh` | `void*` | Module handle. Must not be `NULL`. |
| `index` | `int` | Zero-based index into the account list. |

### Return Value

| Value | Enum Name | Meaning |
|-------|-----------|---------|
| `0` | `Unchecked` | Account has not been validated since MeshMC launched. Token status is unknown. |
| `1` | `Offline` | User has explicitly chosen offline mode, or network is unavailable. |
| `2` | `Working` | An authentication or refresh operation is currently in progress. |
| `3` | `Online` | Account is fully authenticated and ready to use for launching. |
| `4` | `Errored` | The last authentication attempt failed with an error. |
| `5` | `Expired` | The access token has expired and needs to be refreshed. |
| `6` | `Gone` | The account no longer exists on the Microsoft/Mojang side (deleted or banned). |
| `-1` | *(error)* | The index is invalid, or the account list is unavailable. |

### Implementation Detail

```cpp
int PluginManager::api_account_get_state(void* mh, int idx)
{
    auto* r = rt(mh);
    auto* app = r->manager->m_app;
    if (!app || !app->accounts())
        return -1;
    auto acc = app->accounts()->at(idx);
    if (!acc)
        return -1;
    return static_cast<int>(acc->accountState());
}
```

The state is read from `AccountData::accountState`, which is updated by
the authentication task system. The state can change asynchronously (e.g.
when a background token refresh completes), but reads on the main thread
are always consistent.

### State Transitions

```text
Unchecked (0) ──► Working (2) ──► Online (3)
                       │
                       ├──► Errored (4)
                       │
                       └──► Expired (5) ──► Working (2) [refresh]
                                                │
                                                └──► Online (3) or Errored (4)

Offline (1) — set by user or network failure
Gone (6)    — account deleted upstream, terminal state
```

### Practical Guidance

- **Before launching**: Check that the default account is in state `3`
  (`Online`). If it's `0` (`Unchecked`) or `5` (`Expired`), the launcher
  will automatically attempt a refresh, but you may want to warn the user.
- **State `2` (`Working`)**: Don't try to launch while authentication is
  in progress. Wait or inform the user.
- **State `6` (`Gone`)**: This account is permanently invalid. The user
  needs to remove it and log in again.

### Example

```c
static const char* state_names[] = {
    "Unchecked", "Offline", "Working", "Online",
    "Errored", "Expired", "Gone"
};

int state = ctx->account_get_state(mh, i);
if (state >= 0 && state <= 6) {
    printf("Account state: %s\n", state_names[state]);
} else {
    printf("Account state: unknown (%d)\n", state);
}

/* Only consider accounts that are online */
if (state == 3) {
    printf("Account is ready for launch.\n");
}
```

---

## `account_is_active()`

Returns whether the account at the given index is currently "active" —
i.e., it is being used for an operation (typically a running game session
or an in-progress authentication task).

### Signature

```c
int (*account_is_active)(void* mh, int index);
```

### Parameters

| Parameter | Type | Description |
|-----------|------|-------------|
| `mh` | `void*` | Module handle. Must not be `NULL`. |
| `index` | `int` | Zero-based index into the account list. |

### Return Value

| Condition | Return |
|-----------|--------|
| Account is active | `1` |
| Account is not active | `0` |
| Index out of range | `0` |
| Account list unavailable | `0` |

### What "Active" Means

The `isActive()` method on `MinecraftAccount` is part of the `Usable`
mixin class. An account becomes "active" when something acquires a use
reference to it — typically:

1. **A game session is running** with that account
2. **An authentication task** (login, refresh) is in progress

When the game exits or the auth task completes, the use count drops back
to zero and `isActive()` returns `false`.

This is different from the "default" account (see `account_get_default_index()`).
The default account is the one selected in the UI for new launches. The
active flag means an account is currently *in use*.

### Implementation Detail

```cpp
int PluginManager::api_account_is_active(void* mh, int idx)
{
    auto* r = rt(mh);
    auto* app = r->manager->m_app;
    if (!app || !app->accounts())
        return 0;
    auto acc = app->accounts()->at(idx);
    return (acc && acc->isActive()) ? 1 : 0;
}
```

### Example

```c
int n = ctx->account_count(mh);
for (int i = 0; i < n; i++) {
    if (ctx->account_is_active(mh, i)) {
        const char* tmp = ctx->account_get_profile_name(mh, i);
        char name[256];
        snprintf(name, sizeof(name), "%s", tmp ? tmp : "(unknown)");
        printf("Account '%s' is currently in use.\n", name);
    }
}
```

---

## `account_get_default_index()`

Returns the index of the default account — the account that MeshMC will
use for new game launches when no specific account is selected.

### Signature

```c
int (*account_get_default_index)(void* mh);
```

### Parameters

| Parameter | Type | Description |
|-----------|------|-------------|
| `mh` | `void*` | Module handle. Must not be `NULL`. |

### Return Value

| Condition | Return |
|-----------|--------|
| Success | Zero-based index of the default account. |
| No default set | `-1` |
| No accounts exist | `-1` |
| Account list unavailable | `-1` |

### Default vs Active

| Concept | Meaning | API |
|---------|---------|-----|
| **Default** | The account selected in MeshMC's profile dropdown. Used for new launches. | `account_get_default_index()` |
| **Active** | An account currently in use (running game, auth in progress). | `account_is_active()` |

A default account may or may not be active. An active account may or may
not be the default (e.g. the user could launch with a non-default account
via the instance settings).

### Implementation Detail

```cpp
int PluginManager::api_account_get_default_index(void* mh)
{
    auto* r = rt(mh);
    auto* app = r->manager->m_app;
    if (!app || !app->accounts())
        return -1;
    auto def = app->accounts()->defaultAccount();
    if (!def)
        return -1;
    for (int i = 0; i < app->accounts()->count(); ++i) {
        if (app->accounts()->at(i) == def)
            return i;
    }
    return -1;
}
```

The implementation iterates the account list comparing shared pointers.
This is an O(n) scan where n is the number of accounts (typically very
small — most users have 1–3 accounts).

### Example

```c
int def_idx = ctx->account_get_default_index(mh);
if (def_idx < 0) {
    printf("No default account set.\n");
} else {
    const char* tmp = ctx->account_get_profile_name(mh, def_idx);
    printf("Default account: %s\n", tmp ? tmp : "(unknown)");
}
```

---

# Practical Examples

## Account Lister Plugin

A complete plugin that lists all accounts in a UI tree widget on a custom
page. Demonstrates iterating accounts, copying strings safely, and
building a table view.

```c
#include <mmco_sdk.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static MMCOContext* ctx;
static void* mh;
static void* tree_widget;

static const char* state_to_str(int s) {
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

static void refresh_account_list(void* ud) {
    (void)ud;
    ctx->ui_tree_clear(mh, tree_widget);

    int n = ctx->account_count(mh);
    int def = ctx->account_get_default_index(mh);

    for (int i = 0; i < n; i++) {
        /* Copy each field before the next API call */
        const char* tmp;
        char name[256], uuid[64], type[32], state[32], flags[32];

        tmp = ctx->account_get_profile_name(mh, i);
        snprintf(name, sizeof(name), "%s", tmp ? tmp : "(no profile)");

        tmp = ctx->account_get_profile_id(mh, i);
        snprintf(uuid, sizeof(uuid), "%s", tmp ? tmp : "(none)");

        tmp = ctx->account_get_type(mh, i);
        snprintf(type, sizeof(type), "%s", tmp ? tmp : "?");

        int st = ctx->account_get_state(mh, i);
        snprintf(state, sizeof(state), "%s", state_to_str(st));

        int active = ctx->account_is_active(mh, i);
        snprintf(flags, sizeof(flags), "%s%s",
                 (i == def) ? "Default " : "",
                 active ? "Active" : "");

        const char* row[] = { name, uuid, type, state, flags };
        ctx->ui_tree_add_row(mh, tree_widget, row, 5);
    }
}

static int on_hook(void* ud, const char* hook, const void* payload) {
    (void)ud;
    (void)payload;

    if (strcmp(hook, "meshmc.instancewindow.pages") == 0) {
        /* We could add our page here — simplified for brevity */
    }
    return 0;
}

MMCO_EXPORT int mmco_init(void* handle, MMCOContext* context) {
    mh = handle;
    ctx = context;

    const char* cols[] = { "Name", "UUID", "Type", "State", "Flags" };
    void* page = ctx->ui_page_create(mh, "account_list",
                                     "Account List", "accounts");
    void* layout = ctx->ui_layout_create(mh, page, 0);
    tree_widget = ctx->ui_tree_create(mh, page, cols, 5, NULL, NULL);
    void* btn = ctx->ui_button_create(mh, page, "Refresh", NULL,
                                      refresh_account_list, NULL);
    ctx->ui_layout_add_widget(mh, layout, tree_widget);
    ctx->ui_layout_add_widget(mh, layout, btn);
    ctx->ui_page_set_layout(mh, page, layout);

    refresh_account_list(NULL);
    return 0;
}

MMCO_EXPORT void mmco_unload(void* handle) {
    (void)handle;
}
```

---

## Profile Display Widget

A simpler example that creates a label showing the default account's
profile information:

```c
#include <mmco_sdk.h>
#include <stdio.h>
#include <string.h>

static MMCOContext* ctx;
static void* mh;
static void* label;

static void update_label(void) {
    int def = ctx->account_get_default_index(mh);
    char buf[512];

    if (def < 0) {
        snprintf(buf, sizeof(buf), "No account selected");
    } else {
        const char* tmp = ctx->account_get_profile_name(mh, def);
        char name[256];
        snprintf(name, sizeof(name), "%s", tmp ? tmp : "(none)");

        tmp = ctx->account_get_profile_id(mh, def);
        char uuid[64];
        snprintf(uuid, sizeof(uuid), "%s", tmp ? tmp : "(none)");

        int state = ctx->account_get_state(mh, def);
        const char* status = (state == 3) ? "Online" :
                             (state == 2) ? "Authenticating..." :
                             (state == 5) ? "Token expired" :
                             "Not ready";

        snprintf(buf, sizeof(buf),
                 "Player: %s\nUUID: %s\nStatus: %s", name, uuid, status);
    }

    ctx->ui_label_set_text(mh, label, buf);
}

MMCO_EXPORT int mmco_init(void* handle, MMCOContext* context) {
    mh = handle;
    ctx = context;

    label = ctx->ui_label_create(mh, NULL, "Loading...");
    update_label();
    return 0;
}
```

---

## Default Account Guard

A hook-based example that checks the default account's state before
allowing a launch, and shows a warning if the account is not ready:

```c
#include <mmco_sdk.h>
#include <string.h>

static MMCOContext* ctx;
static void* mh;

static int on_hook(void* ud, const char* hook, const void* payload) {
    (void)ud;

    if (strcmp(hook, "meshmc.launch.pre") == 0) {
        int def = ctx->account_get_default_index(mh);
        if (def < 0) {
            ctx->ui_show_message(mh, 1, "Launch Warning",
                "No default account is set. Please select an account "
                "before launching.");
            /* Return 0 to allow launch to continue anyway —
               the launcher will prompt for an account.
               Return non-zero to abort the launch. */
            return 0;
        }

        int state = ctx->account_get_state(mh, def);
        if (state == 4 /* Errored */ || state == 6 /* Gone */) {
            const char* tmp = ctx->account_get_profile_name(mh, def);
            char name[256];
            snprintf(name, sizeof(name), "%s", tmp ? tmp : "(unknown)");

            char msg[512];
            snprintf(msg, sizeof(msg),
                "Account '%s' is in an error state (code %d). "
                "You may need to re-authenticate.", name, state);
            ctx->ui_show_message(mh, 1, "Account Problem", msg);
        }
    }
    return 0;
}

MMCO_EXPORT int mmco_init(void* handle, MMCOContext* context) {
    mh = handle;
    ctx = context;
    ctx->hook_register(mh, "meshmc.launch.pre", on_hook, NULL);
    return 0;
}
```

---

## UUID Format Conversion Helper

A complete utility showing both undashed-to-dashed and dashed-to-undashed
conversion, useful for interfacing with external APIs (Mojang API returns
undashed; most other services expect dashed):

```c
#include <string.h>
#include <stdio.h>
#include <ctype.h>

/**
 * Convert 32-char undashed UUID to 36-char dashed UUID.
 * Returns 0 on success, -1 on invalid input.
 */
static int uuid_undashed_to_dashed(const char* in, char* out, size_t out_sz) {
    if (!in || !out || out_sz < 37)
        return -1;
    if (strlen(in) != 32)
        return -1;
    for (int i = 0; i < 32; i++) {
        if (!isxdigit((unsigned char)in[i]))
            return -1;
    }
    /* Format: 8-4-4-4-12 */
    snprintf(out, out_sz, "%.8s-%.4s-%.4s-%.4s-%.12s",
             in, in + 8, in + 12, in + 16, in + 20);
    return 0;
}

/**
 * Convert 36-char dashed UUID to 32-char undashed UUID.
 * Returns 0 on success, -1 on invalid input.
 */
static int uuid_dashed_to_undashed(const char* in, char* out, size_t out_sz) {
    if (!in || !out || out_sz < 33)
        return -1;
    if (strlen(in) != 36)
        return -1;
    /* Validate dash positions */
    if (in[8] != '-' || in[13] != '-' || in[18] != '-' || in[23] != '-')
        return -1;

    int j = 0;
    for (int i = 0; i < 36; i++) {
        if (i == 8 || i == 13 || i == 18 || i == 23)
            continue;
        if (!isxdigit((unsigned char)in[i]))
            return -1;
        out[j++] = in[i];
    }
    out[j] = '\0';
    return 0;
}

/* Usage with account API: */
void show_account_uuid(MMCOContext* ctx, void* mh, int idx) {
    const char* tmp = ctx->account_get_profile_id(mh, idx);
    if (!tmp || tmp[0] == '\0') {
        printf("No UUID available\n");
        return;
    }

    char undashed[33];
    snprintf(undashed, sizeof(undashed), "%s", tmp);

    char dashed[37];
    if (uuid_undashed_to_dashed(undashed, dashed, sizeof(dashed)) == 0) {
        printf("UUID (dashed):   %s\n", dashed);
        printf("UUID (undashed): %s\n", undashed);
    } else {
        printf("UUID (raw): %s\n", undashed);
    }
}
```

---

## Error Handling Summary

All account functions follow a consistent error pattern:

| Error Condition | `account_count` | `account_get_profile_name` | `account_get_profile_id` | `account_get_type` | `account_get_state` | `account_is_active` | `account_get_default_index` |
|----------------|-----------------|---------------------------|--------------------------|--------------------|--------------------|--------------------|-----------------------------|
| `mh` is `NULL` | UB | UB | UB | UB | UB | UB | UB |
| App is `NULL` | `0` | `NULL` | `NULL` | `NULL` | `-1` | `0` | `-1` |
| Account list `NULL` | `0` | `NULL` | `NULL` | `NULL` | `-1` | `0` | `-1` |
| Index out of range | N/A | `NULL` | `NULL` | `NULL` | `-1` | `0` | N/A |
| No default set | N/A | N/A | N/A | N/A | N/A | N/A | `-1` |

"UB" = Undefined Behavior. The module handle should never be `NULL` — it
is provided by the runtime and should be passed through unchanged.

---

## Related Sections

| Section | Relationship |
|---------|-------------|
| [S01 — Lifecycle](../s01-lifecycle/) | `tempString` ownership, `mmco_init()` entry point |
| [S04 — Instance Management](../s04-instance-management/) | Account used to launch instances |
| [S08 — Java](java.md) | Java installs — the other half of this section |
| [S12 — UI Dialogs](../s12-ui-dialogs/) | Showing account info in dialog windows |
| [S13 — UI Page Builder](../s13-ui-page-builder/) | Building account management pages |
