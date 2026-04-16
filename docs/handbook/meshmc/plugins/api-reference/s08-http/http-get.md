# `http_get()`

> Perform an asynchronous HTTP GET request.

[← Back to Section 08 overview](README.md)

---

## `http_get()`

### Signature

```c
int (*http_get)(void* mh, const char* url,
                MMCOHttpCallback callback, void* user_data);
```

### Description

Queues an asynchronous HTTP GET request to the specified URL. The function
validates the URL and callback pointer, constructs a `QNetworkRequest`,
sets the `User-Agent` header, and dispatches the request through
MeshMC's shared `QNetworkAccessManager`. The function returns **immediately**
— the actual HTTP response is delivered later through the callback.

The callback is invoked on the **main (GUI) thread** when the HTTP
transaction completes (either successfully or with an error). The callback
receives the HTTP status code, a pointer to the raw response body, and
the size of that body. See the [callback documentation](README.md#the-mmcohttpcallback-typedef)
for full details on the callback parameters and lifetime rules.

This function is the primary mechanism for plugins to **fetch data from
remote servers** — checking for updates, downloading configuration files,
querying JSON APIs, retrieving mod metadata, etc.

### Parameters

| Parameter | Type | Description |
|-----------|------|-------------|
| `mh` | `void*` | Module handle. Pass `ctx->module_handle`. |
| `url` | `const char*` | The URL to request. Must begin with `http://` or `https://`. Must be a valid, null-terminated UTF-8 C string. |
| `callback` | `MMCOHttpCallback` | Function pointer that will be called when the request completes. Must not be `NULL`. |
| `user_data` | `void*` | Opaque pointer passed through to the callback. May be `NULL`. |

### Callback type

```c
typedef void (*MMCOHttpCallback)(void* user_data,
                                 int status_code,
                                 const void* response_body,
                                 size_t response_size);
```

See [README.md — MMCOHttpCallback](README.md#the-mmcohttpcallback-typedef) for
the complete typedef documentation, lifetime rules, and thread model.

### Return value

| Condition | Value |
|-----------|-------|
| Request queued successfully | `0` |
| `url` is `NULL` | `−1` |
| `callback` is `NULL` | `−1` |
| URL scheme is not `http://` or `https://` | `−1` |
| Network manager unavailable | `−1` |

**Important:** A return value of `0` means the request was *queued*, not
that it will succeed. The callback will fire eventually even if the server
returns an error (4xx, 5xx) or a network-level failure occurs
(`status_code == 0` in the callback).

A return value of `−1` means the request was *rejected* and the callback
will **never** be invoked. This is critical for resource management: if
your `user_data` points to heap-allocated memory, you must free it
yourself when `http_get()` returns `−1`, because the callback that would
normally free it will never run.

### Thread safety

**Main thread only.** This function must be called from the main GUI
thread (which is where all MMCO plugin code runs). The callback is also
invoked on the main thread.

### URL validation

The URL is checked with a simple prefix test:

```cpp
QString qurl = QString::fromUtf8(url);
if (!qurl.startsWith("http://") && !qurl.startsWith("https://"))
    return -1;
```

This means:

- `https://api.example.com/v1/versions` — **accepted**
- `http://localhost:8080/data` — **accepted** (useful for development)
- `ftp://files.example.com/mod.jar` — **rejected**
- `file:///etc/passwd` — **rejected**
- `javascript:alert(1)` — **rejected**
- Empty string `""` — **rejected** (does not start with `http`)
- `NULL` — **rejected** (checked before URL parsing)

The function does **not** perform deeper URL validation (e.g., it does
not verify that the hostname is a valid FQDN or that the port number is
in range). Malformed URLs will be caught by Qt during the network
request, and the callback will receive `status_code == 0`.

### Headers set by MeshMC

| Header | Value | Source |
|--------|-------|--------|
| `User-Agent` | `BuildConfig.USER_AGENT` | Set unconditionally by the implementation. Cannot be overridden. |

No other headers are set. The `Accept` header is not set, so the server
receives whatever default Qt sends (typically `*/*`).

If you need to communicate an API key or authentication token, pass it as
a **URL query parameter**:

```c
ctx->http_get(mh,
    "https://api.example.com/v1/data?api_key=YOUR_KEY",
    on_response, NULL);
```

### Ownership & memory

- The `url` string is copied into a `QString` immediately. The plugin
  does not need to keep the string alive after `http_get()` returns.
- The `callback` function pointer is captured by the Qt connection
  lambda. It must remain valid until the callback fires (which it always
  will, as function pointers do not expire).
- The `user_data` pointer is captured by value in the lambda and passed
  through to the callback. MeshMC does not dereference or free it.

### Implementation

```cpp
// PluginManager.cpp
int PluginManager::api_http_get(void* mh, const char* url,
                                MMCOHttpCallback cb, void* ud)
{
    auto* r = rt(mh);
    auto* app = r->manager->m_app;

    if (!url || !cb)
        return -1;

    // Validate URL scheme (only http/https allowed)
    QString qurl = QString::fromUtf8(url);
    if (!qurl.startsWith("http://") && !qurl.startsWith("https://"))
        return -1;

    auto nam = app->network();
    if (!nam)
        return -1;

    QNetworkRequest request{QUrl(qurl)};
    request.setHeader(QNetworkRequest::UserAgentHeader,
                      BuildConfig.USER_AGENT);

    QNetworkReply* reply = nam->get(request);

    QObject::connect(reply, &QNetworkReply::finished,
                     [reply, cb, ud]() {
        int status = reply->attribute(
            QNetworkRequest::HttpStatusCodeAttribute).toInt();
        QByteArray body = reply->readAll();
        cb(ud, status, body.constData(),
           static_cast<size_t>(body.size()));
        reply->deleteLater();
    });

    return 0;
}
```

### Call chain

```text
Plugin:  ctx->http_get(mh, "https://api.example.com/data", my_cb, my_ud)
    │
    ▼
PluginManager::api_http_get(mh, url, cb, ud)
    │
    ├── rt(mh) → ModuleRuntime*
    ├── r->manager->m_app → Application*
    ├── NULL check: url, cb
    ├── Scheme check: starts with "http://" or "https://"
    ├── app->network() → QNetworkAccessManager*
    ├── Build QNetworkRequest with QUrl(qurl)
    ├── Set User-Agent header
    ├── nam->get(request) → QNetworkReply*
    ├── Connect QNetworkReply::finished → lambda
    │       │
    │       └── (later, on main thread)
    │           ├── status = reply→HttpStatusCodeAttribute
    │           ├── body = reply→readAll()
    │           ├── cb(ud, status, body.data, body.size)
    │           └── reply→deleteLater()
    │
    └── return 0
```

---

## Examples

### Example 1 — Simple version check

A plugin checks a remote JSON endpoint to see if a newer version is
available. This is the most common use case for `http_get()`.

```c
#include <mmco_sdk.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

static MMCOContext* ctx;
static void* mh;

/* Callback: receives the version check response */
static void on_version_check(void* ud, int status,
                              const void* body, size_t len)
{
    (void)ud;

    if (status == 0) {
        ctx->log(mh, 2, "Version check failed: network error");
        return;
    }
    if (status != 200) {
        char msg[128];
        snprintf(msg, sizeof(msg),
                 "Version check: HTTP %d", status);
        ctx->log(mh, 1, msg);
        return;
    }

    /* Copy body to a null-terminated string for safe parsing */
    char* json = (char*)malloc(len + 1);
    if (!json) return;
    memcpy(json, body, len);
    json[len] = '\0';

    /*
     * Minimal version extraction — in a real plugin you would
     * use a proper JSON parser. Here we look for "version":"X.Y.Z"
     */
    const char* key = "\"version\":\"";
    const char* start = strstr(json, key);
    if (start) {
        start += strlen(key);
        const char* end = strchr(start, '"');
        if (end) {
            size_t vlen = (size_t)(end - start);
            char ver[64];
            if (vlen < sizeof(ver)) {
                memcpy(ver, start, vlen);
                ver[vlen] = '\0';
                char log_msg[256];
                snprintf(log_msg, sizeof(log_msg),
                         "Latest version: %s", ver);
                ctx->log(mh, 0, log_msg);
            }
        }
    }

    free(json);
}

int mmco_init(MMCOContext* c)
{
    ctx = c;
    mh = c->module_handle;

    /* Fire-and-forget version check */
    int rc = ctx->http_get(mh,
        "https://api.example.com/myplugin/latest.json",
        on_version_check, NULL);

    if (rc != 0) {
        ctx->log(mh, 1, "Failed to start version check");
    }

    return 0;
}
```

### Example 2 — Download a file to disk

Download a resource and write it to the instance's mods directory using
the Filesystem API.

```c
typedef struct {
    char inst_id[128];
    char filename[256];
} DownloadContext;

static void on_download_complete(void* ud, int status,
                                  const void* body, size_t len)
{
    DownloadContext* dc = (DownloadContext*)ud;

    if (status != 200) {
        char msg[256];
        snprintf(msg, sizeof(msg),
                 "Download failed: HTTP %d for %s",
                 status, dc->filename);
        ctx->log(mh, 2, msg);
        free(dc);
        return;
    }

    /* Build destination path */
    const char* inst_dir = ctx->inst_get_dir(mh, dc->inst_id);
    if (!inst_dir) {
        ctx->log(mh, 2, "Cannot resolve instance directory");
        free(dc);
        return;
    }

    char path[1024];
    snprintf(path, sizeof(path), "%s/mods/%s", inst_dir, dc->filename);

    /* Write downloaded bytes to file */
    int rc = ctx->fs_write_file(mh, path, body, len);
    if (rc == 0) {
        char msg[512];
        snprintf(msg, sizeof(msg),
                 "Downloaded %s (%zu bytes)", dc->filename, len);
        ctx->log(mh, 0, msg);
    } else {
        ctx->log(mh, 2, "Failed to write downloaded file");
    }

    free(dc);
}

void download_mod(const char* inst_id, const char* url,
                  const char* filename)
{
    DownloadContext* dc = (DownloadContext*)calloc(1, sizeof(*dc));
    if (!dc) return;

    snprintf(dc->inst_id, sizeof(dc->inst_id), "%s", inst_id);
    snprintf(dc->filename, sizeof(dc->filename), "%s", filename);

    int rc = ctx->http_get(mh, url, on_download_complete, dc);
    if (rc != 0) {
        ctx->log(mh, 2, "Failed to queue download");
        free(dc);  /* Callback won't run — clean up here */
    }
}
```

### Example 3 — Chained requests (fetch index, then download)

A common pattern is to first fetch a JSON index, parse it, then issue a
second HTTP request to download the actual file.

```c
typedef struct {
    char inst_id[128];
} IndexContext;

static void on_mod_downloaded(void* ud, int status,
                               const void* body, size_t len)
{
    IndexContext* ic = (IndexContext*)ud;

    if (status == 200 && len > 0) {
        const char* dir = ctx->inst_get_dir(mh, ic->inst_id);
        if (dir) {
            char path[1024];
            snprintf(path, sizeof(path),
                     "%s/mods/downloaded-mod.jar", dir);
            ctx->fs_write_file(mh, path, body, len);
            ctx->log(mh, 0, "Mod downloaded and installed");
        }
    } else {
        ctx->log(mh, 2, "Mod download failed");
    }

    free(ic);
}

static void on_index_received(void* ud, int status,
                               const void* body, size_t len)
{
    IndexContext* ic = (IndexContext*)ud;

    if (status != 200 || len == 0) {
        ctx->log(mh, 2, "Failed to fetch mod index");
        free(ic);
        return;
    }

    /* Parse the URL from the index JSON (simplified) */
    char* json = (char*)malloc(len + 1);
    if (!json) { free(ic); return; }
    memcpy(json, body, len);
    json[len] = '\0';

    const char* key = "\"download_url\":\"";
    const char* start = strstr(json, key);
    if (!start) {
        ctx->log(mh, 1, "No download_url in index");
        free(json);
        free(ic);
        return;
    }

    start += strlen(key);
    const char* end = strchr(start, '"');
    if (!end) {
        free(json);
        free(ic);
        return;
    }

    size_t url_len = (size_t)(end - start);
    char* download_url = (char*)malloc(url_len + 1);
    if (!download_url) {
        free(json);
        free(ic);
        return;
    }
    memcpy(download_url, start, url_len);
    download_url[url_len] = '\0';

    free(json);

    /* Chain: issue a second GET to download the actual mod.
     * Re-use the same IndexContext — ownership transfers. */
    int rc = ctx->http_get(mh, download_url, on_mod_downloaded, ic);
    if (rc != 0) {
        ctx->log(mh, 2, "Failed to queue mod download");
        free(ic);
    }

    free(download_url);
}

void fetch_and_install_mod(const char* inst_id,
                           const char* index_url)
{
    IndexContext* ic = (IndexContext*)calloc(1, sizeof(*ic));
    if (!ic) return;
    snprintf(ic->inst_id, sizeof(ic->inst_id), "%s", inst_id);

    int rc = ctx->http_get(mh, index_url, on_index_received, ic);
    if (rc != 0) {
        ctx->log(mh, 2, "Failed to fetch index");
        free(ic);
    }
}
```

### Example 4 — Multiple concurrent requests

You can issue multiple `http_get()` calls without waiting for any of them
to complete. Each request is independent and each callback will fire
separately.

```c
typedef struct {
    int request_id;
} MultiContext;

static void on_multi_response(void* ud, int status,
                               const void* body, size_t len)
{
    MultiContext* mc = (MultiContext*)ud;
    char msg[256];
    snprintf(msg, sizeof(msg),
             "Request #%d completed: HTTP %d, %zu bytes",
             mc->request_id, status, len);
    ctx->log(mh, 0, msg);
    free(mc);
}

void check_multiple_servers(void)
{
    const char* urls[] = {
        "https://api.server1.example.com/status",
        "https://api.server2.example.com/status",
        "https://api.server3.example.com/status",
    };

    for (int i = 0; i < 3; i++) {
        MultiContext* mc = (MultiContext*)calloc(1, sizeof(*mc));
        if (!mc) continue;
        mc->request_id = i;

        int rc = ctx->http_get(mh, urls[i], on_multi_response, mc);
        if (rc != 0) {
            free(mc);
        }
    }

    /* All three requests are now in-flight concurrently.
     * Callbacks will fire in whatever order the servers respond. */
}
```

---

## Common mistakes

### Mistake 1 — Using `response_body` after the callback returns

```c
/* WRONG — body is freed after callback returns */
static const void* saved_body;
static size_t saved_len;

void my_callback(void* ud, int status, const void* body, size_t len)
{
    saved_body = body;   /* Dangling pointer! */
    saved_len = len;
}

void process_later(void)
{
    /* saved_body now points to freed memory */
    memcpy(buf, saved_body, saved_len);  /* CRASH */
}
```

**Fix:** Copy the data during the callback.

### Mistake 2 — Forgetting to free `user_data` on rejection

```c
/* WRONG — memory leak when http_get returns -1 */
MyData* data = calloc(1, sizeof(*data));
ctx->http_get(mh, some_url, my_callback, data);
/* If http_get returned -1, my_callback never fires,
   and data is leaked */
```

**Fix:**

```c
MyData* data = calloc(1, sizeof(*data));
int rc = ctx->http_get(mh, some_url, my_callback, data);
if (rc != 0) {
    free(data);  /* Clean up — callback won't run */
}
```

### Mistake 3 — Blocking the main thread waiting for a response

```c
/* WRONG — busy-waiting freezes the UI */
static volatile int done = 0;
void my_callback(void* ud, int s, const void* b, size_t l) {
    done = 1;
}

ctx->http_get(mh, url, my_callback, NULL);
while (!done) { /* spin */ }   /* DEADLOCK — callback runs on same thread */
```

The callback can **never fire** while you are blocking the main thread,
because the callback is dispatched on the main thread. This is a
deadlock.

**Fix:** Structure your code as continuation-passing: do all post-response
work inside the callback (or chain of callbacks).

### Mistake 4 — Assuming callbacks fire in order

If you issue multiple requests, the callbacks fire in the order that the
**servers respond**, not the order you issued the requests. Use your
`user_data` to track which request each callback corresponds to.

---

## Edge cases

### Empty response body

Some endpoints return `204 No Content` or `200 OK` with no body. The
callback will fire with `response_size == 0`. The `response_body` pointer
may or may not be `NULL` — always check `response_size` before reading.

### Very large responses

There is no built-in limit on response size. If a server sends a 500 MB
response, Qt will buffer the entire thing in memory before invoking the
callback. Be careful when hitting endpoints with unbounded response sizes.

### Redirects

Qt's `QNetworkAccessManager` follows HTTP redirects (301, 302, 307, 308)
automatically. The `status_code` in the callback is the status of the
**final** response after all redirects are followed. The plugin does not
see the intermediate redirect responses.

### HTTPS certificate errors

If the remote server has an invalid or expired TLS certificate, Qt will
reject the connection. The callback will fire with `status_code == 0`.

### URL encoding

The `url` parameter is passed directly to `QUrl`. If your URL contains
special characters (spaces, non-ASCII), you must percent-encode them
before passing the URL. `QUrl` does some normalisation but does not fully
encode arbitrary strings.

```c
/* WRONG */
ctx->http_get(mh,
    "https://api.example.com/search?q=hello world",
    cb, NULL);

/* CORRECT */
ctx->http_get(mh,
    "https://api.example.com/search?q=hello%20world",
    cb, NULL);
```
