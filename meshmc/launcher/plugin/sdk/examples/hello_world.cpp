/* SPDX-FileCopyrightText: Project Tick
 * SPDX-FileContributor: Project Tick
 * SPDX-License-Identifier: CC0-1.0
 *
 * Example MeshMC plugin (.mmco module)
 *
 */

#include "mmco_sdk.h"
#include <cstring>

/* Module declaration */

MMCO_DEFINE_MODULE(
    "Hello World",      /* name */
    "1.0.0",            /* version */
    "Project Tick",     /* author */
    "Example plugin that logs instance info on app init", /* description */
    "GPL-3.0-or-later"  /* license */
);

/* State */

static MMCOContext* g_ctx = nullptr;

/* Hook callback */

static int on_app_initialized(void* /*mh*/, uint32_t /*hook_id*/,
                              void* /*payload*/, void* /*user_data*/)
{
    MMCO_LOG(g_ctx, "Hello from the Hello World plugin!");

    int count = g_ctx->instance_count(g_ctx->module_handle);
    char buf[256];
    snprintf(buf, sizeof(buf), "MeshMC has %d instance(s):", count);
    MMCO_LOG(g_ctx, buf);

    for (int i = 0; i < count; ++i) {
        const char* id = g_ctx->instance_get_id(g_ctx->module_handle, i);
        if (id) {
            const char* name = g_ctx->instance_get_name(g_ctx->module_handle, id);
            snprintf(buf, sizeof(buf), "  [%d] %s (%s)", i, name ? name : "?", id);
            MMCO_LOG(g_ctx, buf);
        }
    }

    // Demonstrate settings
    g_ctx->setting_set(g_ctx->module_handle, "last_run", "just now");
    const char* val = g_ctx->setting_get(g_ctx->module_handle, "last_run");
    if (val) {
        snprintf(buf, sizeof(buf), "Stored setting 'last_run' = '%s'", val);
        MMCO_DBG(g_ctx, buf);
    }

    // Demonstrate version query
    const char* appName = g_ctx->get_app_name(g_ctx->module_handle);
    const char* appVer  = g_ctx->get_app_version(g_ctx->module_handle);
    snprintf(buf, sizeof(buf), "Running on %s %s", appName, appVer);
    MMCO_LOG(g_ctx, buf);

    return 0;  // continue hook chain
}

static int on_instance_pre_launch(void* /*mh*/, uint32_t /*hook_id*/,
                                  void* payload, void* /*user_data*/)
{
    auto* info = static_cast<MMCOInstanceInfo*>(payload);
    if (info && info->instance_name) {
        char buf[256];
        snprintf(buf, sizeof(buf), "Instance '%s' is about to launch!",
                 info->instance_name);
        MMCO_LOG(g_ctx, buf);
    }
    return 0;
}

/* Entry points */

extern "C" int mmco_init(MMCOContext* ctx)
{
    g_ctx = ctx;

    MMCO_LOG(ctx, "Hello World plugin initializing...");

    // Register hooks
    ctx->hook_register(ctx->module_handle,
                       MMCO_HOOK_APP_INITIALIZED,
                       on_app_initialized, nullptr);

    ctx->hook_register(ctx->module_handle,
                       MMCO_HOOK_INSTANCE_PRE_LAUNCH,
                       on_instance_pre_launch, nullptr);

    MMCO_LOG(ctx, "Hello World plugin initialized.");
    return 0;
}

extern "C" void mmco_unload()
{
    if (g_ctx) {
        MMCO_LOG(g_ctx, "Hello World plugin unloading. Goodbye!");
    }
    g_ctx = nullptr;
}
