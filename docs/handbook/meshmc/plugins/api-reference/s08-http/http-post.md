# `http_post()`

> Perform an asynchronous HTTP POST request with a body.

[← Back to Section 08 overview](README.md)

---

## `http_post()`

### Signature

```c
int (*http_post)(void* mh, const char* url,
                 const void* body, size_t body_size,
                 const char* content_type,
                 MMCOHttpCallback callback, void* user_data);
```

### Description

Queues an asynchronous HTTP POST request to the specified URL with
arbitrary body content. Like `http_get()`, the function returns
**immediately** after queuing the request. The HTTP response is delivered
later through the callback on the main thread.

The POST function extends `http_get()` with three additional parameters:

1. **`body`** — a pointer to the raw bytes to send as the request body.
2. **`body_size`** — the number of bytes to send.
3. **`content_type`** — the MIME type of the body (e.g., `"application/json"`).

This function is used for:

- Sending JSON payloads to REST APIs.
- Submitting form data.
- Uploading crash reports or telemetry.
- Posting authentication credentials to obtain tokens.
- Any HTTP interaction that requires a request body.

### Parameters

| Parameter | Type | Description |
|-----------|------|-------------|
| `mh` | `void*` | Module handle. Pass `ctx->module_handle`. |
| `url` | `const char*` | The URL to POST to. Must begin with `http://` or `https://`. Must be a valid, null-terminated UTF-8 C string. |
| `body` | `const void*` | Pointer to the request body bytes. May be `NULL` if `body_size` is `0`. |
| `body_size` | `size_t` | Number of bytes to read from `body`. If `0`, the POST is sent with an empty body. |
| `content_type` | `const char*` | MIME type string for the `Content-Type` header. If `NULL`, no `Content-Type` header is set. |
| `callback` | `MMCOHttpCallback` | Function pointer invoked when the response arrives. Must not be `NULL`. |
| `user_data` | `void*` | Opaque pointer passed through to the callback. May be `NULL`. |

### Parameter details

#### `body` and `body_size`

The body content is **copied** into a `QByteArray` before the function
returns. This means:

- The plugin does **not** need to keep the `body` buffer alive after
  `http_post()` returns.
- The body is fully captured even if the plugin stack unwinds before the
  request is sent over the network.
- Very large bodies are fine from a correctness standpoint, but will
  consume memory for the duration of the request.

```cpp
// From PluginManager.cpp — body is copied here:
QByteArray postData(static_cast<const char*>(body),
                    static_cast<int>(body_sz));
```

#### `content_type`

The `content_type` parameter sets the `Content-Type` HTTP header on the
request. If `NULL`, the header is omitted entirely. Common values:

| Content Type | Use Case |
|-------------|----------|
| `"application/json"` | JSON API calls |
| `"application/x-www-form-urlencoded"` | Form submissions |
| `"text/plain"` | Plain text payloads |
| `"application/octet-stream"` | Raw binary data |
| `"multipart/form-data"` | Not recommended — boundary generation is not handled |
| `NULL` | Omit the header (server decides how to interpret body) |

The string is copied into a `QString` immediately, so the plugin does not
need to keep it alive.

```cpp
// From PluginManager.cpp — content_type applied conditionally:
if (ct)
    request.setHeader(QNetworkRequest::ContentTypeHeader,
                      QString::fromUtf8(ct));
```

### Callback type

```c
typedef void (*MMCOHttpCallback)(void* user_data,
                                 int status_code,
                                 const void* response_body,
                                 size_t response_size);
```

The callback semantics are identical to `http_get()`. See
[README.md — MMCOHttpCallback](README.md#the-mmcohttpcallback-typedef) for
the complete typedef documentation, lifetime rules, and thread model.

### Return value

| Condition | Value |
|-----------|-------|
| Request queued successfully | `0` |
| `url` is `NULL` | `−1` |
| `callback` is `NULL` | `−1` |
| URL scheme is not `http://` or `https://` | `−1` |
| Network manager unavailable | `−1` |

**Note:** `body` being `NULL` with `body_size > 0` is **undefined
behaviour** — the `QByteArray` constructor will read from a null pointer.
Always ensure `body` is valid if `body_size` is non-zero.

A return value of `−1` means the callback will **never fire**. Clean up
any heap-allocated `user_data` immediately.

### Thread safety

**Main thread only.** Same model as `http_get()`.

### URL validation

Identical to `http_get()`:

```cpp
QString qurl = QString::fromUtf8(url);
if (!qurl.startsWith("http://") && !qurl.startsWith("https://"))
    return -1;
```

### Headers set by MeshMC

| Header | Value | Source | Overridable? |
|--------|-------|--------|--------------|
| `User-Agent` | `BuildConfig.USER_AGENT` | Set unconditionally | No |
| `Content-Type` | Value of `content_type` parameter | Set if `content_type` is non-NULL | Yes — via parameter |

### Ownership & memory

- **`url`** — copied into `QString`. Safe to free after call.
- **`body`** — copied into `QByteArray`. Safe to free after call.
- **`content_type`** — copied into `QString`. Safe to free after call.
- **`callback`** — captured by the lambda. Must remain valid (always true
  for function pointers in loaded plugins).
- **`user_data`** — captured by value. MeshMC does not dereference or
  free it.

### Implementation

```cpp
// PluginManager.cpp
int PluginManager::api_http_post(void* mh, const char* url,
                                 const void* body, size_t body_sz,
                                 const char* ct, MMCOHttpCallback cb,
                                 void* ud)
{
    auto* r = rt(mh);
    auto* app = r->manager->m_app;

    if (!url || !cb)
        return -1;

    QString qurl = QString::fromUtf8(url);
    if (!qurl.startsWith("http://") && !qurl.startsWith("https://"))
        return -1;

    auto nam = app->network();
    if (!nam)
        return -1;

    QNetworkRequest request{QUrl(qurl)};
    request.setHeader(QNetworkRequest::UserAgentHeader,
                      BuildConfig.USER_AGENT);
    if (ct)
        request.setHeader(QNetworkRequest::ContentTypeHeader,
                          QString::fromUtf8(ct));

    QByteArray postData(static_cast<const char*>(body),
                        static_cast<int>(body_sz));
    QNetworkReply* reply = nam->post(request, postData);

    QObject::connect(reply, &QNetworkReply::finished,
                     [reply, cb, ud]() {
        int status = reply->attribute(
            QNetworkRequest::HttpStatusCodeAttribute).toInt();
        QByteArray respBody = reply->readAll();
        cb(ud, status, respBody.constData(),
           static_cast<size_t>(respBody.size()));
        reply->deleteLater();
    });

    return 0;
}
```

### Call chain

```text
Plugin:  ctx->http_post(mh, url, json, json_len, "application/json", cb, ud)
    │
    ▼
PluginManager::api_http_post(mh, url, body, body_sz, ct, cb, ud)
    │
    ├── rt(mh) → ModuleRuntime*
    ├── r->manager->m_app → Application*
    ├── NULL check: url, cb
    ├── Scheme check: starts with "http://" or "https://"
    ├── app->network() → QNetworkAccessManager*
    ├── Build QNetworkRequest with QUrl(qurl)
    ├── Set User-Agent header
    ├── if (ct) → Set Content-Type header
    ├── Copy body into QByteArray postData
    ├── nam->post(request, postData) → QNetworkReply*
    ├── Connect QNetworkReply::finished → lambda
    │       │
    │       └── (later, on main thread)
    │           ├── status = reply→HttpStatusCodeAttribute
    │           ├── respBody = reply→readAll()
    │           ├── cb(ud, status, respBody.data, respBody.size)
    │           └── reply→deleteLater()
    │
    └── return 0
```

---

## Examples

### Example 1 — JSON API call

Post a JSON payload to a REST API and parse the response.

```c
#include <mmco_sdk.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

static MMCOContext* ctx;
static void* mh;

static void on_api_response(void* ud, int status,
                             const void* body, size_t len)
{
    (void)ud;

    if (status == 0) {
        ctx->log(mh, 2, "API request failed: network error");
        return;
    }

    char msg[256];
    snprintf(msg, sizeof(msg), "API response: HTTP %d, %zu bytes",
             status, len);
    ctx->log(mh, 0, msg);

    if (status == 200 && len > 0) {
        /* Copy and process the JSON response */
        char* json = (char*)malloc(len + 1);
        if (!json) return;
        memcpy(json, body, len);
        json[len] = '\0';

        /* Parse json... */
        ctx->log(mh, 0, json);

        free(json);
    }
}

void send_crash_report(const char* report_json)
{
    size_t len = strlen(report_json);

    int rc = ctx->http_post(mh,
        "https://api.example.com/v1/crash-reports",
        report_json, len,
        "application/json",
        on_api_response, NULL);

    if (rc != 0) {
        ctx->log(mh, 2, "Failed to queue crash report");
    }
}
```

### Example 2 — Form-encoded data

Submit data in `application/x-www-form-urlencoded` format.

```c
static void on_login_response(void* ud, int status,
                               const void* body, size_t len)
{
    if (status == 200) {
        /* Copy token from response */
        char* token = (char*)malloc(len + 1);
        if (!token) return;
        memcpy(token, body, len);
        token[len] = '\0';

        /* Store token in settings for later use */
        ctx->setting_set(mh, "auth_token", token);
        ctx->log(mh, 0, "Login successful");

        free(token);
    } else {
        char msg[128];
        snprintf(msg, sizeof(msg), "Login failed: HTTP %d", status);
        ctx->log(mh, 1, msg);
    }
}

void login_to_service(const char* username, const char* password)
{
    char body[1024];
    int body_len = snprintf(body, sizeof(body),
        "username=%s&password=%s", username, password);

    if (body_len < 0 || body_len >= (int)sizeof(body)) {
        ctx->log(mh, 2, "Login body too large");
        return;
    }

    int rc = ctx->http_post(mh,
        "https://auth.example.com/login",
        body, (size_t)body_len,
        "application/x-www-form-urlencoded",
        on_login_response, NULL);

    if (rc != 0) {
        ctx->log(mh, 2, "Failed to queue login request");
    }
}
```

> **Security warning:** The example above passes credentials in the POST
> body. Never log or persist passwords. In production, prefer OAuth2
> flows or API key authentication where possible.

### Example 3 — POST with structured user data

Track multiple operations using a heap-allocated context struct that
is freed in the callback.

```c
typedef enum {
    OP_SUBMIT_REPORT,
    OP_UPDATE_CONFIG,
    OP_REGISTER_PLUGIN,
} PostOperation;

typedef struct {
    PostOperation operation;
    char label[128];
    int retry_count;
} PostContext;

static void on_post_done(void* ud, int status,
                          const void* body, size_t len)
{
    PostContext* pc = (PostContext*)ud;

    char msg[512];
    snprintf(msg, sizeof(msg),
             "[%s] POST completed: HTTP %d (%zu bytes)",
             pc->label, status, len);
    ctx->log(mh, status == 200 ? 0 : 1, msg);

    if (status != 200 && pc->retry_count < 3) {
        /* Retry logic — re-queue the request.
         * NOTE: we would need to re-build the body here;
         * this is a simplified example. */
        pc->retry_count++;
        ctx->log(mh, 1, "Retrying...");
        /* In practice, you'd store the body and re-post */
    }

    free(pc);
}

void submit_report(const char* json, size_t json_len)
{
    PostContext* pc = (PostContext*)calloc(1, sizeof(*pc));
    if (!pc) return;

    pc->operation = OP_SUBMIT_REPORT;
    snprintf(pc->label, sizeof(pc->label), "CrashReport");
    pc->retry_count = 0;

    int rc = ctx->http_post(mh,
        "https://reports.example.com/submit",
        json, json_len,
        "application/json",
        on_post_done, pc);

    if (rc != 0) {
        ctx->log(mh, 2, "Failed to queue report submission");
        free(pc);
    }
}
```

### Example 4 — POST binary data

Upload raw binary content (e.g., a screenshot or crash dump).

```c
static void on_upload_done(void* ud, int status,
                            const void* body, size_t len)
{
    (void)ud;
    (void)body;
    (void)len;

    if (status == 200 || status == 201) {
        ctx->log(mh, 0, "Upload successful");
    } else {
        char msg[128];
        snprintf(msg, sizeof(msg), "Upload failed: HTTP %d", status);
        ctx->log(mh, 2, msg);
    }
}

void upload_crash_dump(const char* dump_path)
{
    /* Read the binary file */
    size_t file_size = 0;
    int64_t sz = ctx->fs_file_size(mh, dump_path);
    if (sz <= 0) {
        ctx->log(mh, 2, "Cannot read dump file");
        return;
    }
    file_size = (size_t)sz;

    void* data = malloc(file_size);
    if (!data) return;

    int rc = ctx->fs_read_file(mh, dump_path, data, file_size);
    if (rc != 0) {
        free(data);
        return;
    }

    rc = ctx->http_post(mh,
        "https://uploads.example.com/crash-dump",
        data, file_size,
        "application/octet-stream",
        on_upload_done, NULL);

    /* Body is copied into QByteArray — safe to free immediately */
    free(data);

    if (rc != 0) {
        ctx->log(mh, 2, "Failed to queue upload");
    }
}
```

### Example 5 — POST with NULL content type

If the server does not require a `Content-Type` header (or you want to
let the server auto-detect), pass `NULL`:

```c
void send_raw_bytes(const void* data, size_t len)
{
    int rc = ctx->http_post(mh,
        "https://api.example.com/ingest",
        data, len,
        NULL,  /* No Content-Type header */
        on_generic_response, NULL);

    if (rc != 0) {
        ctx->log(mh, 2, "Failed to queue raw POST");
    }
}
```

### Example 6 — Empty POST (no body)

Some APIs use POST with no body as a trigger (e.g., "start a build"):

```c
void trigger_remote_build(void)
{
    int rc = ctx->http_post(mh,
        "https://ci.example.com/api/build/trigger",
        NULL, 0,     /* Empty body */
        NULL,        /* No content type */
        on_build_triggered, NULL);

    if (rc != 0) {
        ctx->log(mh, 2, "Failed to trigger build");
    }
}
```

---

## Comparison with `http_get()`

| Aspect | `http_get()` | `http_post()` |
|--------|-------------|--------------|
| HTTP method | GET | POST |
| Request body | None | Optional (`body` + `body_size`) |
| Content-Type header | Not set | Set via `content_type` parameter |
| Parameter count | 4 | 7 |
| Callback type | `MMCOHttpCallback` | `MMCOHttpCallback` (same) |
| URL validation | `http://` or `https://` prefix | Same |
| Return values | `0` or `−1` | Same |
| Thread model | Async, callback on main thread | Same |
| Body memory | N/A | Copied into `QByteArray` — caller can free immediately |

---

## Common mistakes

### Mistake 1 — Forgetting to set `content_type` for JSON

```c
/* WRONG — server may reject or misinterpret the body */
ctx->http_post(mh, url, json, strlen(json),
    NULL,  /* Missing content type! */
    cb, NULL);
```

**Fix:**

```c
ctx->http_post(mh, url, json, strlen(json),
    "application/json",
    cb, NULL);
```

### Mistake 2 — Using `strlen()` on binary data

```c
/* WRONG — strlen stops at first 0x00 byte */
ctx->http_post(mh, url, binary_data, strlen(binary_data),
    "application/octet-stream", cb, NULL);
```

**Fix:** Track the binary data size separately.

### Mistake 3 — NULL body with non-zero size

```c
/* UNDEFINED BEHAVIOUR — QByteArray reads from NULL */
ctx->http_post(mh, url, NULL, 100, "text/plain", cb, NULL);
```

**Fix:** If body is `NULL`, `body_size` must be `0`.

### Mistake 4 — Assuming the body is not copied

```c
/* This is actually FINE — body is copied immediately */
{
    char buf[64];
    snprintf(buf, sizeof(buf), "{\"key\":\"value\"}");
    ctx->http_post(mh, url, buf, strlen(buf),
        "application/json", cb, NULL);
}
/* buf is out of scope here, but the copy is in-flight — no problem */
```

The body is copied into a `QByteArray` before `http_post()` returns,
so stack-allocated buffers are safe.

---

## Edge cases

### Empty POST vs. GET

An empty POST (`body_size == 0`) is semantically different from a GET.
Servers may treat them differently. Use `http_get()` when you do not
need to send a body.

### Very large POST bodies

There is no size limit imposed by MeshMC, but:

- The entire body is held in memory (in the `QByteArray`).
- The entire body is buffered before sending — there is no streaming.
- Remote servers may reject very large bodies (commonly > 10 MB).

### Response body for POST

POST responses follow the same rules as GET responses. The
`response_body` is valid only during the callback. Copy it if needed.

### Redirects for POST

Qt may or may not follow redirects for POST requests, depending on the
redirect type:

- `307 Temporary Redirect` and `308 Permanent Redirect` — POST is
  re-sent to the new URL (method preserved).
- `301` and `302` — the redirect may change the method to GET
  (per HTTP specification). Behaviour depends on Qt version.

### Content-Type for empty body

Setting `content_type` with `body_size == 0` is valid but unusual. The
server receives a `Content-Type` header with no body content. Some
servers may reject this.
