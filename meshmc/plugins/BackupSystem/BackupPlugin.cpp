/* SPDX-FileCopyrightText: 2026 Project Tick
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * BackupPlugin — MMCO entry point for the BackupSystem plugin.
 *
 * All Qt and MeshMC types come through the SDK header. The plugin
 * does not directly #include any Qt or MeshMC headers.
 */

#include "plugin/sdk/mmco_sdk.h"
#include "BackupPage.h"

MMCO_DEFINE_MODULE(
	"BackupSystem", "1.0.0", "Project Tick",
	"Instance backup snapshots — create, restore, export, import",
	"GPL-3.0-or-later");

static MMCOContext* g_ctx = nullptr;

static int on_instance_pages(void* /*mh*/, uint32_t /*hook_id*/, void* payload,
							 void* /*ud*/)
{
	auto* evt = static_cast<MMCOInstancePagesEvent*>(payload);
	if (!evt || !evt->page_list_handle || !evt->instance_handle)
		return 0;

	auto* pages = static_cast<QList<BasePage*>*>(evt->page_list_handle);
	auto* instRaw = static_cast<BaseInstance*>(evt->instance_handle);

	// Non-owning shared_ptr — the instance is managed elsewhere
	InstancePtr inst = std::shared_ptr<BaseInstance>(
		instRaw, [](BaseInstance*) { /* no-op deleter */ });

	pages->append(new BackupPage(inst));
	return 0;
}

extern "C" {

MMCO_EXPORT int mmco_init(MMCOContext* ctx)
{
	g_ctx = ctx;

	MMCO_LOG(ctx, "BackupSystem initializing...");

	int rc = ctx->hook_register(ctx->module_handle, MMCO_HOOK_UI_INSTANCE_PAGES,
								on_instance_pages, nullptr);
	if (rc != 0) {
		MMCO_ERR(ctx, "Failed to register INSTANCE_PAGES hook");
		return rc;
	}

	/* Register toolbar action so "View Backups" appears in the
	 * main window's instance toolbar. */
	ctx->ui_register_instance_action(
		ctx->module_handle, "View Backups",
		"View and manage backups for this instance.", "backup",
		"backup-system");

	MMCO_LOG(ctx, "BackupSystem initialized successfully.");
	return 0;
}

MMCO_EXPORT void mmco_unload()
{
	if (g_ctx) {
		MMCO_LOG(g_ctx, "BackupSystem unloading.");
	}
	g_ctx = nullptr;
}

} /* extern "C" */
