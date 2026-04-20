/* SPDX-FileCopyrightText: 2026 Project Tick
 * SPDX-FileContributor: Project Tick
 * SPDX-License-Identifier: GPL-3.0-or-later WITH LicenseRef-MeshMC-MMCO-Module-Exception-1.0
 *
 *   MeshMC - A Custom Launcher for Minecraft
 *   Copyright (C) 2026 Project Tick
 *
 *   This program is free software: you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, either version 3 of the License, or
 *   (at your option) any later version, with the additional permission
 *   described in the MeshMC MMCO Module Exception 1.0.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program.  If not, see <https://www.gnu.org/licenses/>.
 *
 *   You should have received a copy of the MeshMC MMCO Module Exception 1.0
 *   along with this program.  If not, see <https://projecttick.org/licenses/>.
 */

#pragma once

#include "plugin/PluginLoader.h"
#include "plugin/PluginMetadata.h"
#include "plugin/PluginHooks.h"
#include "plugin/PluginAPI.h"

#include <QObject>
#include <QString>
#include <QVector>
#include <QMap>
#include <QMultiMap>
#include <memory>
#include <vector>
#include <functional>

/*
 * PluginManager — owns the plugin lifecycle and provides the bridge
 * between loaded .mmco modules and MeshMC internals.
 *
 * Responsibilities:
 *   - Discover and load modules via PluginLoader
 *   - Build MMCOContext for each module (populating function pointers)
 *   - Call mmco_init() / mmco_unload()
 *   - Dispatch hooks to registered callbacks
 *   - Implement the API functions that back MMCOContext
 */

class Application;

class PluginManager : public QObject
{
	Q_OBJECT

  public:
	explicit PluginManager(Application* app, QObject* parent = nullptr);
	~PluginManager() override;

	/*
	 * Discover and initialise all modules. Called once during
	 * Application startup, after subsystems are ready.
	 */
	void initializeAll();

	/*
	 * Gracefully shut down all modules (calls mmco_unload() in
	 * reverse load order, then dlclose).
	 */
	void shutdownAll();

	/*
	 * Dispatch a hook to all registered listeners.
	 * Returns true if any callback signalled cancellation (non-zero return).
	 */
	bool dispatchHook(uint32_t hook_id, void* payload = nullptr);

	/*
	 * Query loaded modules.
	 */
	const QVector<PluginMetadata>& modules() const
	{
		return m_modules;
	}
	int moduleCount() const
	{
		return m_modules.size();
	}

	/*
	 * ModuleRuntime — the opaque object behind module_handle.
	 * Lets static API callbacks find their way back to the manager.
	 * Public so helper functions in the .cpp can use it.
	 */
	struct ModuleRuntime {
		PluginManager* manager;
		int moduleIndex;
		std::string dataDir;
		std::string tempString;
	};

	Application* m_app;

  signals:
	void moduleLoaded(const QString& name);
	void moduleUnloaded(const QString& name);
	void moduleError(const QString& name, const QString& error);

  private:
	/* Build an MMCOContext for a specific module */
	MMCOContext buildContext(PluginMetadata& meta);

	/* Ensure the plugin data directory exists */
	void ensurePluginDataDir(PluginMetadata& meta);

	/* ── API implementation (static callbacks wired into MMCOContext) ── */
	/* These are static so they can be used as C function pointers.
	 * They resolve the PluginManager instance via the module_handle,
	 * which is actually a pointer to a ModuleRuntime struct.
	 */

	struct HookRegistration {
		void* module_handle;
		MMCOHookCallback callback;
		void* user_data;
	};

	/* Static API trampolines — Section 1: Logging */
	static void api_log_info(void* mh, const char* msg);
	static void api_log_warn(void* mh, const char* msg);
	static void api_log_error(void* mh, const char* msg);
	static void api_log_debug(void* mh, const char* msg);

	/* Section 2: Hooks */
	static int api_hook_register(void* mh, uint32_t hook_id,
								 MMCOHookCallback cb, void* ud);
	static int api_hook_unregister(void* mh, uint32_t hook_id,
								   MMCOHookCallback cb);

	/* Section 3: Settings */
	static const char* api_setting_get(void* mh, const char* key);
	static int api_setting_set(void* mh, const char* key, const char* value);

	/* Section 4: Instance Management */
	static int api_instance_count(void* mh);
	static const char* api_instance_get_id(void* mh, int index);
	static const char* api_instance_get_name(void* mh, const char* id);
	static int api_instance_set_name(void* mh, const char* id,
									 const char* name);
	static const char* api_instance_get_path(void* mh, const char* id);
	static const char* api_instance_get_game_root(void* mh, const char* id);
	static const char* api_instance_get_mods_root(void* mh, const char* id);
	static const char* api_instance_get_icon_key(void* mh, const char* id);
	static int api_instance_set_icon_key(void* mh, const char* id,
										 const char* key);
	static const char* api_instance_get_type(void* mh, const char* id);
	static const char* api_instance_get_notes(void* mh, const char* id);
	static int api_instance_set_notes(void* mh, const char* id,
									  const char* notes);
	static int api_instance_is_running(void* mh, const char* id);
	static int api_instance_can_launch(void* mh, const char* id);
	static int api_instance_has_crashed(void* mh, const char* id);
	static int api_instance_has_update(void* mh, const char* id);
	static int64_t api_instance_get_total_play_time(void* mh, const char* id);
	static int64_t api_instance_get_last_play_time(void* mh, const char* id);
	static int64_t api_instance_get_last_launch(void* mh, const char* id);
	static int api_instance_launch(void* mh, const char* id, int online);
	static int api_instance_kill(void* mh, const char* id);
	static int api_instance_delete(void* mh, const char* id);
	static const char* api_instance_get_group(void* mh, const char* id);
	static int api_instance_set_group(void* mh, const char* id,
									  const char* group);
	static int api_instance_group_count(void* mh);
	static const char* api_instance_group_at(void* mh, int index);
	static int api_instance_component_count(void* mh, const char* id);
	static const char* api_instance_component_get_uid(void* mh, const char* id,
													  int idx);
	static const char* api_instance_component_get_name(void* mh, const char* id,
													   int idx);
	static const char*
	api_instance_component_get_version(void* mh, const char* id, int idx);
	static const char* api_instance_get_mc_version(void* mh, const char* id);
	static const char* api_instance_get_jar_mods_dir(void* mh, const char* id);
	static const char* api_instance_get_resource_packs_dir(void* mh,
														   const char* id);
	static const char* api_instance_get_texture_packs_dir(void* mh,
														  const char* id);
	static const char* api_instance_get_shader_packs_dir(void* mh,
														 const char* id);
	static const char* api_instance_get_worlds_dir(void* mh, const char* id);

	/* Section 5: Mod Management */
	static int api_mod_count(void* mh, const char* inst, const char* type);
	static const char* api_mod_get_name(void* mh, const char* inst,
										const char* type, int idx);
	static const char* api_mod_get_version(void* mh, const char* inst,
										   const char* type, int idx);
	static const char* api_mod_get_filename(void* mh, const char* inst,
											const char* type, int idx);
	static const char* api_mod_get_description(void* mh, const char* inst,
											   const char* type, int idx);
	static int api_mod_is_enabled(void* mh, const char* inst, const char* type,
								  int idx);
	static int api_mod_set_enabled(void* mh, const char* inst, const char* type,
								   int idx, int e);
	static int api_mod_remove(void* mh, const char* inst, const char* type,
							  int idx);
	static int api_mod_install(void* mh, const char* inst, const char* type,
							   const char* path);
	static int api_mod_refresh(void* mh, const char* inst, const char* type);

	/* Section 6: World Management */
	static int api_world_count(void* mh, const char* inst);
	static const char* api_world_get_name(void* mh, const char* inst, int idx);
	static const char* api_world_get_folder(void* mh, const char* inst,
											int idx);
	static int64_t api_world_get_seed(void* mh, const char* inst, int idx);
	static int api_world_get_game_type(void* mh, const char* inst, int idx);
	static int64_t api_world_get_last_played(void* mh, const char* inst,
											 int idx);
	static int api_world_delete(void* mh, const char* inst, int idx);
	static int api_world_rename(void* mh, const char* inst, int idx,
								const char* name);
	static int api_world_install(void* mh, const char* inst, const char* path);
	static int api_world_refresh(void* mh, const char* inst);

	/* Section 7: Account Management */
	static int api_account_count(void* mh);
	static const char* api_account_get_profile_name(void* mh, int idx);
	static const char* api_account_get_profile_id(void* mh, int idx);
	static const char* api_account_get_type(void* mh, int idx);
	static int api_account_get_state(void* mh, int idx);
	static int api_account_is_active(void* mh, int idx);
	static int api_account_get_default_index(void* mh);

	/* Section 8: Java Management */
	static int api_java_count(void* mh);
	static const char* api_java_get_version(void* mh, int idx);
	static const char* api_java_get_arch(void* mh, int idx);
	static const char* api_java_get_path(void* mh, int idx);
	static int api_java_is_recommended(void* mh, int idx);
	static const char* api_instance_get_java_version(void* mh, const char* id);

	/* Section 9: Filesystem */
	static const char* api_fs_plugin_data_dir(void* mh);
	static int64_t api_fs_read(void* mh, const char* rel, void* buf, size_t sz);
	static int api_fs_write(void* mh, const char* rel, const void* data,
							size_t sz);
	static int api_fs_exists(void* mh, const char* rel);
	static int api_fs_mkdir(void* mh, const char* path);
	static int api_fs_exists_abs(void* mh, const char* path);
	static int api_fs_remove(void* mh, const char* path);
	static int api_fs_copy_file(void* mh, const char* src, const char* dst);
	static int64_t api_fs_file_size(void* mh, const char* path);
	static int api_fs_list_dir(void* mh, const char* path, int type,
							   MMCODirEntryCallback cb, void* ud);

	/* Section 10: Zip */
	static int api_zip_compress_dir(void* mh, const char* zip, const char* dir);
	static int api_zip_extract(void* mh, const char* zip, const char* target);

	/* Section 11: Network */
	static int api_http_get(void* mh, const char* url, MMCOHttpCallback cb,
							void* ud);
	static int api_http_post(void* mh, const char* url, const void* body,
							 size_t body_sz, const char* ct,
							 MMCOHttpCallback cb, void* ud);

	/* Section 12: UI Dialogs */
	static void api_ui_show_message(void* mh, int type, const char* title,
									const char* msg);
	static int api_ui_add_menu_item(void* mh, void* menu_handle,
									const char* label, const char* icon,
									MMCOMenuActionCallback cb, void* ud);
	static const char* api_ui_file_open_dialog(void* mh, const char* title,
											   const char* filter);
	static const char* api_ui_file_save_dialog(void* mh, const char* title,
											   const char* def,
											   const char* filter);
	static const char* api_ui_input_dialog(void* mh, const char* title,
										   const char* prompt, const char* def);
	static int api_ui_confirm_dialog(void* mh, const char* title,
									 const char* msg);
	static int api_ui_register_instance_action(void* mh, const char* text,
											   const char* tooltip,
											   const char* icon_name,
											   const char* page_id);
	static int api_ui_register_instance_action_cb(void* mh, const char* text,
												  const char* tooltip,
												  const char* icon_name,
												  void (*cb)(void* ud),
												  void* ud);

	/* Section 13: UI Page Builder */
	static void* api_ui_page_create(void* mh, const char* id, const char* name,
									const char* icon);
	static int api_ui_page_add_to_list(void* mh, void* page, void* list);
	static void* api_ui_layout_create(void* mh, void* parent, int type);
	static int api_ui_layout_add_widget(void* mh, void* layout, void* widget);
	static int api_ui_layout_add_layout(void* mh, void* parent, void* child);
	static int api_ui_layout_add_spacer(void* mh, void* layout, int horizontal);
	static int api_ui_page_set_layout(void* mh, void* page, void* layout);
	static void* api_ui_button_create(void* mh, void* parent, const char* text,
									  const char* icon, MMCOButtonCallback cb,
									  void* ud);
	static int api_ui_button_set_enabled(void* mh, void* btn, int enabled);
	static int api_ui_button_set_text(void* mh, void* btn, const char* text);
	static void* api_ui_label_create(void* mh, void* parent, const char* text);
	static int api_ui_label_set_text(void* mh, void* label, const char* text);
	static void* api_ui_tree_create(void* mh, void* parent, const char** cols,
									int ncols, MMCOTreeSelectionCallback cb,
									void* ud);
	static int api_ui_tree_clear(void* mh, void* tree);
	static int api_ui_tree_add_row(void* mh, void* tree, const char** vals,
								   int ncols);
	static int api_ui_tree_selected_row(void* mh, void* tree);
	static int api_ui_tree_set_row_data(void* mh, void* tree, int row,
										int64_t data);
	static int64_t api_ui_tree_get_row_data(void* mh, void* tree, int row);
	static int api_ui_tree_row_count(void* mh, void* tree);

	/* Section 14: Utility */
	static const char* api_get_app_version(void* mh);
	static const char* api_get_app_name(void* mh);
	static int64_t api_get_timestamp(void* mh);

	/* Section 15: Launch Modifiers */
	static int api_launch_set_env(void* mh, const char* key, const char* value);
	static int api_launch_prepend_wrapper(void* mh, const char* wrapper_cmd);

	/* Section 16: Application Settings */
	static const char* api_app_setting_get(void* mh, const char* key);

	/* Helpers */
	static ModuleRuntime* rt(void* mh);

	PluginLoader m_loader;
	QVector<PluginMetadata> m_modules;
	std::vector<std::unique_ptr<ModuleRuntime>> m_runtimes;
	std::vector<MMCOContext> m_contexts;

	/* hook_id -> list of registrations */
	QMultiMap<uint32_t, HookRegistration> m_hooks;

  public:
	/* Registered instance toolbar actions (for MainWindow to consume) */
	struct InstanceAction {
		QString text;
		QString tooltip;
		QString iconName;
		QString pageId;
	};
	const QVector<InstanceAction>& instanceActions() const
	{
		return m_instanceActions;
	}

	struct InstanceCallbackAction {
		QString text;
		QString tooltip;
		QString iconName;
		void (*callback)(void* ud);
		void* userData;
	};
	const QVector<InstanceCallbackAction>& instanceCallbackActions() const
	{
		return m_instanceCallbackActions;
	}

  private:
	QVector<InstanceAction> m_instanceActions;
	QVector<InstanceCallbackAction> m_instanceCallbackActions;

	/* Pending launch modifications (set by plugins during PRE_LAUNCH hooks) */
	QMap<QString, QString> m_pendingLaunchEnv;
	QString m_pendingLaunchWrapper;

	bool m_shutdownDone = false;

  public:
	/* Called by LaunchController before/after dispatching PRE_LAUNCH hook */
	void clearPendingLaunchMods();
	QMap<QString, QString> takePendingLaunchEnv();
	QString takePendingLaunchWrapper();
};
