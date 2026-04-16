# Section 08 — HTTP API

> **Header:** `PluginAPI.h` (function pointer declarations) · **SDK:** `mmco_sdk.h` (struct definition) · **Implementation:** `PluginManager.cpp` (static trampolines)

---

## Overview

The HTTP API gives plugins the ability to perform **asynchronous HTTP
requests** — `GET` and `POST` — against remote servers. Every request is
non-blocking: the function returns immediately after queuing the request,
and the result is delivered through a **callback** that fires on the
**main (GUI) thread** once the network operation completes.

This is the *only* mechanism available to plugins for network I/O. Plugins
cannot open raw sockets, perform DNS lookups, or use any transport other
than HTTP/HTTPS. The restriction is deliberate: it lets MeshMC enforce
URL validation, TLS policy, and a consistent `User-Agent` header across
the entire application.

```text
┌──────────────────────────────────────────────────────────────────────┐
│  1. http_get(url, callback, user_data)                              │
│        → queue a GET request; receive response via callback          │
│                                                                      │
│  2. http_post(url, body, body_size, content_type, callback, ud)     │
│        → queue a POST request with a payload; receive via callback   │
└──────────────────────────────────────────────────────────────────────┘
```

Both function pointers live in `MMCOContext` and are declared in
`PluginAPI.h`:

```c
/* S08 — HTTP (from MMCOContext in PluginAPI.h) */

int (*http_get)(void* mh, const char* url,
                MMCOHttpCallback callback, void* user_data);

int (*http_post)(void* mh, const char* url,
                 const void* body, size_t body_size,
                 const char* content_type,
                 MMCOHttpCallback callback, void* user_data);
```

---

## The `MMCOHttpCallback` typedef

All HTTP responses — whether success or failure — are delivered through
a single callback type defined at the top of `PluginAPI.h`:

```c
typedef void (*MMCOHttpCallback)(void* user_data,
                                 int status_code,
                                 const void* response_body,
                                 size_t response_size);
```

### Callback parameters

| Parameter | Type | Description |
|-----------|------|-------------|
| `user_data` | `void*` | The opaque pointer that was passed to `http_get()` or `http_post()`. |
| `status_code` | `int` | The HTTP status code. `200` for success, `404` for not found, etc. On network-level failure (DNS error, timeout, TLS failure), this is `0` because Qt's `QNetworkReply` does not assign an HTTP status attribute when the request never reaches the server. |
| `response_body` | `const void*` | Pointer to the raw response bytes. May be `NULL` if `response_size` is `0`. |
| `response_size` | `size_t` | Number of bytes pointed to by `response_body`. |

### Lifetime of `response_body`

The `response_body` pointer is **only valid for the duration of the
callback invocation**. Once the callback returns, the underlying
`QByteArray` is destroyed and the memory is freed. If you need to retain
the data, you **must** copy it:

```c
void my_callback(void* ud, int status, const void* body, size_t len)
{
    /* WRONG — dangling pointer after callback returns */
    // global_ptr = body;

    /* CORRECT — copy into your own buffer */
    char* copy = (char*)malloc(len + 1);
    memcpy(copy, body, len);
    copy[len] = '\0';
    /* ... use copy ... */
}
```

### Thread model

The callback fires on the **main (GUI) thread**. This is because MeshMC
uses Qt's signal/slot mechanism internally: when `QNetworkReply::finished`
is emitted, the connected lambda runs in the thread that owns the
`QNetworkAccessManager`, which is the main thread.

Consequences:

- You **can** call other MMCO API functions (UI, filesystem, settings)
  directly from the callback.
- You **must not** perform long-running computation in the callback — it
  will freeze the UI.
- If you need to process a large download, copy the data and schedule
  the work for later (e.g., via a timer or a setting that is polled).

---

## Function summary

| Function | Sub-page | Purpose | Returns |
|----------|----------|---------|---------|
| [`http_get()`](http-get.md#http_get) | GET | Perform an asynchronous HTTP GET request | `int` (0 or −1) |
| [`http_post()`](http-post.md#http_post) | POST | Perform an asynchronous HTTP POST request with a body | `int` (0 or −1) |

---

## Asynchronous execution model

Understanding the request lifecycle is critical for correct plugin
behaviour. Here is the full sequence:

```text
Plugin code                    MeshMC (main thread)              Network
────────────                   ────────────────────              ───────
ctx->http_get(mh, url, cb, ud)
    │
    ├── Validate url, cb
    ├── Build QNetworkRequest
    ├── Set User-Agent header
    ├── nam->get(request); ──────────────────────────────────► TCP/TLS
    ├── Connect QNetworkReply::finished → lambda
    └── return 0  (success — request is now in-flight)
                                                                  │
    ... plugin continues executing ...                            │
                                                                  │
                                ◄─────────────────────────────────┘
                                QNetworkReply::finished emitted
                                    │
                                    └── lambda executes:
                                        status = reply→statusCode
                                        body   = reply→readAll()
                                        cb(ud, status, body.data, body.size)
                                        reply→deleteLater()
```

Key takeaway: **`http_get()` and `http_post()` return before the HTTP
response arrives.** The return value (`0` or `−1`) only tells you whether
the request was *queued* successfully — not whether the server will
respond or what the response will be.

---

## Security considerations

### URL scheme validation

Both `http_get()` and `http_post()` reject any URL that does not begin
with `http://` or `https://`. File URLs (`file://`), FTP URLs (`ftp://`),
and any other scheme cause the function to return `−1` immediately.

```c
/* From PluginManager.cpp */
QString qurl = QString::fromUtf8(url);
if (!qurl.startsWith("http://") && !qurl.startsWith("https://"))
    return -1;
```

### User-Agent header

MeshMC sets the `User-Agent` header to `BuildConfig.USER_AGENT` on every
outgoing request. Plugins **cannot override** this header. The header
identifies the request as coming from MeshMC and includes the launcher
version. This ensures that remote servers can identify and rate-limit
MeshMC traffic as a whole, preventing individual plugins from spoofing
their identity.

### No raw socket access

Plugins have no access to sockets, DNS resolution, or any transport-layer
API. All network I/O is funnelled through `QNetworkAccessManager`, which
means:

- TLS certificate validation is handled by Qt / the system TLS backend.
- System proxy settings are respected automatically.
- The operating system's certificate store is used for trust decisions.

### No custom headers

The current API does not expose a way to set arbitrary request headers.
The only headers that are set are:

| Header | Source | Overridable? |
|--------|--------|--------------|
| `User-Agent` | `BuildConfig.USER_AGENT` | No |
| `Content-Type` | `content_type` parameter (POST only) | Yes — via API parameter |

If your plugin needs to authenticate with a remote API (e.g., an API key),
you must pass credentials in the **URL query string** or the **POST body**.
Bearer tokens in headers are not currently supported.

### Timeout behaviour

The HTTP API does not impose an explicit timeout. The request uses Qt's
default network timeout, which depends on the platform and Qt version:

- Qt 6.7+: default is **30 seconds** for transfer timeout.
- Qt 6.6 and earlier: **no default timeout** (relies on OS TCP timeout).

If the remote server is unreachable, the callback may take a long time to
fire (or may never fire if the TCP connection hangs). Plugins that need
strict timeout behaviour should implement their own timeout logic using
`mmco_timer_start()` from the Utility API (Section 14).

---

## Error handling

### Return values from `http_get()` / `http_post()`

| Return | Meaning |
|--------|---------|
| `0` | Request queued successfully. The callback **will** be invoked at some point. |
| `−1` | Request rejected. The callback will **not** be invoked. |

Rejection reasons:

- `url` is `NULL`.
- `callback` is `NULL`.
- URL scheme is not `http://` or `https://`.
- `QNetworkAccessManager` is unavailable (should not happen under normal operation).

### Status codes in callback

| `status_code` | Meaning |
|----------------|---------|
| `200` | OK — normal success. |
| `301`, `302`, `307`, `308` | Redirect (Qt follows redirects automatically by default). |
| `400` | Bad Request. |
| `401` | Unauthorized. |
| `403` | Forbidden. |
| `404` | Not Found. |
| `429` | Too Many Requests (rate limited). |
| `500` | Internal Server Error. |
| `0` | Network-level failure. The HTTP transaction never completed — DNS failure, TLS error, connection refused, timeout, etc. |

When `status_code` is `0`, the `response_body` may still contain data
(Qt sometimes populates the body with an error description), but you
should treat it as an error case.

### Recommended error handling pattern

```c
void on_response(void* ud, int status, const void* body, size_t len)
{
    if (status == 0) {
        /* Network error — no HTTP response at all */
        ctx->log(mh, 2, "Network error: request failed");
        return;
    }
    if (status < 200 || status >= 300) {
        /* HTTP error */
        char msg[256];
        snprintf(msg, sizeof(msg), "HTTP %d error", status);
        ctx->log(mh, 1, msg);
        return;
    }
    /* Success — process body */
}
```

---

## Relationship to other sections

| Section | Relationship |
|---------|-------------|
| [S10 — Filesystem](../s10-filesystem/README.md) | Downloaded data can be written to disk with `fs_write_file()`. |
| [S09 — Zip](../s09-zip/README.md) | Downloaded ZIP archives can be extracted with `zip_extract()`. |
| [S05 — Mods](../s05-mods/README.md) | Downloaded mod files can be installed with `mod_install()`. |
| [S14 — Utility](../s14-utility/README.md) | `mmco_timer_start()` can implement custom request timeouts. |
| [S02 — Settings](../s02-settings/README.md) | API endpoints and keys can be stored in persistent settings. |
| [S12 — UI Dialogs](../s12-ui-dialogs/README.md) | Error messages from HTTP failures can be shown to the user. |

---

## Sub-pages

| Page | Contents |
|------|----------|
| [http-get.md](http-get.md) | Full documentation for `http_get()` — signature, parameters, examples |
| [http-post.md](http-post.md) | Full documentation for `http_post()` — signature, parameters, examples |
