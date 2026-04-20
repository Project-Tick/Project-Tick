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

#include "plugin/PluginManager.h"
#include "Application.h"
#include "BuildConfig.h"
#include "InstanceList.h"
#include "BaseInstance.h"
#include "MMCZip.h"
#include "minecraft/MinecraftInstance.h"
#include "minecraft/PackProfile.h"
#include "minecraft/Component.h"
#include "minecraft/mod/ModFolderModel.h"
#include "minecraft/mod/Mod.h"
#include "minecraft/WorldList.h"
#include "minecraft/World.h"
#include "minecraft/auth/AccountList.h"
#include "minecraft/auth/MinecraftAccount.h"
#include "java/JavaInstallList.h"
#include "java/JavaInstall.h"
#include "settings/SettingsObject.h"

#include <QDateTime>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QDebug>
#include <QInputDialog>
#include <QApplication>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QMessageBox>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QPushButton>
#include <QSpacerItem>
#include <QStandardPaths>
#include <QTreeWidget>
#include <QHeaderView>

PluginManager::PluginManager(Application* app, QObject* parent)
	: QObject(parent), m_app(app)
{
}

PluginManager::~PluginManager()
{
	shutdownAll();
}

void PluginManager::initializeAll()
{
	qDebug() << "[PluginManager] Discovering modules...";

	m_modules = m_loader.discoverModules();

	if (m_modules.isEmpty()) {
		qDebug() << "[PluginManager] No modules found.";
		return;
	}

	// Prepare runtimes and contexts
	m_runtimes.resize(static_cast<size_t>(m_modules.size()));
	m_contexts.resize(static_cast<size_t>(m_modules.size()));

	for (int i = 0; i < m_modules.size(); ++i) {
		auto& meta = m_modules[i];

		ensurePluginDataDir(meta);

		// Create runtime
		auto runtime = std::make_unique<ModuleRuntime>();
		runtime->manager = this;
		runtime->moduleIndex = i;
		runtime->dataDir = meta.dataDir.toStdString();
		m_runtimes[i] = std::move(runtime);

		// Build context
		m_contexts[i] = buildContext(meta);
		m_contexts[i].module_handle = m_runtimes[i].get();

		// Call mmco_init
		qDebug() << "[PluginManager] Initializing module:" << meta.name;
		int rc = meta.initFunc(&m_contexts[i]);
		if (rc != 0) {
			qWarning() << "[PluginManager] Module" << meta.name
					   << "mmco_init() returned" << rc << "- skipping";
			emit moduleError(meta.name,
							 QString("mmco_init returned %1").arg(rc));
			PluginLoader::unloadModule(meta);
			continue;
		}

		meta.initialized = true;
		qDebug() << "[PluginManager] Module" << meta.name
				 << "initialized successfully";
		emit moduleLoaded(meta.name);
	}

	// Fire app-initialized hook
	dispatchHook(MMCO_HOOK_APP_INITIALIZED);
}

void PluginManager::shutdownAll()
{
	if (m_shutdownDone)
		return;
	m_shutdownDone = true;

	// Fire shutdown hook before unloading
	dispatchHook(MMCO_HOOK_APP_SHUTDOWN);

	// Unload in reverse order
	for (int i = m_modules.size() - 1; i >= 0; --i) {
		auto& meta = m_modules[i];
		if (!meta.initialized)
			continue;

		qDebug() << "[PluginManager] Unloading module:" << meta.name;
		if (meta.unloadFunc) {
			meta.unloadFunc();
		}
		meta.initialized = false;
	}

	// DO NOT call dlclose() or clear() data structures here.
	//
	// Plugin .mmco shared libraries statically link MeshMC_logic
	// which contains global objects with non-trivial destructors
	// (e.g. `const Config BuildConfig`).  Modules are opened with
	// RTLD_NODELETE to prevent their static destructors from running
	// at exit and corrupting the heap.  Calling dlclose() would
	// undo that protection.
	//
	// Additionally, plugin Q_OBJECT classes (e.g. BackupPage) have
	// MOC-generated QMetaObject statics registered in Qt's type
	// system; unmapping that memory causes crashes.
	// The OS reclaims all process memory at exit.
}

bool PluginManager::dispatchHook(uint32_t hook_id, void* payload)
{
	auto range = m_hooks.equal_range(hook_id);
	for (auto it = range.first; it != range.second; ++it) {
		const auto& reg = it.value();
		int rc =
			reg.callback(reg.module_handle, hook_id, payload, reg.user_data);
		if (rc != 0) {
			return true; // cancelled
		}
	}
	return false;
}

MMCOContext PluginManager::buildContext(PluginMetadata& meta)
{
	MMCOContext ctx{};
	ctx.struct_size = sizeof(MMCOContext);
	ctx.abi_version = MMCO_ABI_VERSION;
	ctx.module_handle = nullptr; // set by caller

	// S1 — Logging
	ctx.log_info = api_log_info;
	ctx.log_warn = api_log_warn;
	ctx.log_error = api_log_error;
	ctx.log_debug = api_log_debug;

	// S2 — Hooks
	ctx.hook_register = api_hook_register;
	ctx.hook_unregister = api_hook_unregister;

	// S3 — Settings
	ctx.setting_get = api_setting_get;
	ctx.setting_set = api_setting_set;

	// S4 — Instance Management
	ctx.instance_count = api_instance_count;
	ctx.instance_get_id = api_instance_get_id;
	ctx.instance_get_name = api_instance_get_name;
	ctx.instance_set_name = api_instance_set_name;
	ctx.instance_get_path = api_instance_get_path;
	ctx.instance_get_game_root = api_instance_get_game_root;
	ctx.instance_get_mods_root = api_instance_get_mods_root;
	ctx.instance_get_icon_key = api_instance_get_icon_key;
	ctx.instance_set_icon_key = api_instance_set_icon_key;
	ctx.instance_get_type = api_instance_get_type;
	ctx.instance_get_notes = api_instance_get_notes;
	ctx.instance_set_notes = api_instance_set_notes;
	ctx.instance_is_running = api_instance_is_running;
	ctx.instance_can_launch = api_instance_can_launch;
	ctx.instance_has_crashed = api_instance_has_crashed;
	ctx.instance_has_update = api_instance_has_update;
	ctx.instance_get_total_play_time = api_instance_get_total_play_time;
	ctx.instance_get_last_play_time = api_instance_get_last_play_time;
	ctx.instance_get_last_launch = api_instance_get_last_launch;
	ctx.instance_launch = api_instance_launch;
	ctx.instance_kill = api_instance_kill;
	ctx.instance_delete = api_instance_delete;
	ctx.instance_get_group = api_instance_get_group;
	ctx.instance_set_group = api_instance_set_group;
	ctx.instance_group_count = api_instance_group_count;
	ctx.instance_group_at = api_instance_group_at;
	ctx.instance_component_count = api_instance_component_count;
	ctx.instance_component_get_uid = api_instance_component_get_uid;
	ctx.instance_component_get_name = api_instance_component_get_name;
	ctx.instance_component_get_version = api_instance_component_get_version;
	ctx.instance_get_mc_version = api_instance_get_mc_version;
	ctx.instance_get_jar_mods_dir = api_instance_get_jar_mods_dir;
	ctx.instance_get_resource_packs_dir = api_instance_get_resource_packs_dir;
	ctx.instance_get_texture_packs_dir = api_instance_get_texture_packs_dir;
	ctx.instance_get_shader_packs_dir = api_instance_get_shader_packs_dir;
	ctx.instance_get_worlds_dir = api_instance_get_worlds_dir;

	// S5 — Mod Management
	ctx.mod_count = api_mod_count;
	ctx.mod_get_name = api_mod_get_name;
	ctx.mod_get_version = api_mod_get_version;
	ctx.mod_get_filename = api_mod_get_filename;
	ctx.mod_get_description = api_mod_get_description;
	ctx.mod_is_enabled = api_mod_is_enabled;
	ctx.mod_set_enabled = api_mod_set_enabled;
	ctx.mod_remove = api_mod_remove;
	ctx.mod_install = api_mod_install;
	ctx.mod_refresh = api_mod_refresh;

	// S6 — World Management
	ctx.world_count = api_world_count;
	ctx.world_get_name = api_world_get_name;
	ctx.world_get_folder = api_world_get_folder;
	ctx.world_get_seed = api_world_get_seed;
	ctx.world_get_game_type = api_world_get_game_type;
	ctx.world_get_last_played = api_world_get_last_played;
	ctx.world_delete = api_world_delete;
	ctx.world_rename = api_world_rename;
	ctx.world_install = api_world_install;
	ctx.world_refresh = api_world_refresh;

	// S7 — Account Management
	ctx.account_count = api_account_count;
	ctx.account_get_profile_name = api_account_get_profile_name;
	ctx.account_get_profile_id = api_account_get_profile_id;
	ctx.account_get_type = api_account_get_type;
	ctx.account_get_state = api_account_get_state;
	ctx.account_is_active = api_account_is_active;
	ctx.account_get_default_index = api_account_get_default_index;

	// S8 — Java Management
	ctx.java_count = api_java_count;
	ctx.java_get_version = api_java_get_version;
	ctx.java_get_arch = api_java_get_arch;
	ctx.java_get_path = api_java_get_path;
	ctx.java_is_recommended = api_java_is_recommended;
	ctx.instance_get_java_version = api_instance_get_java_version;

	// S9 — Filesystem
	ctx.fs_plugin_data_dir = api_fs_plugin_data_dir;
	ctx.fs_read = api_fs_read;
	ctx.fs_write = api_fs_write;
	ctx.fs_exists = api_fs_exists;
	ctx.fs_mkdir = api_fs_mkdir;
	ctx.fs_exists_abs = api_fs_exists_abs;
	ctx.fs_remove = api_fs_remove;
	ctx.fs_copy_file = api_fs_copy_file;
	ctx.fs_file_size = api_fs_file_size;
	ctx.fs_list_dir = api_fs_list_dir;

	// S10 — Zip
	ctx.zip_compress_dir = api_zip_compress_dir;
	ctx.zip_extract = api_zip_extract;

	// S11 — Network
	ctx.http_get = api_http_get;
	ctx.http_post = api_http_post;

	// S12 — UI Dialogs
	ctx.ui_show_message = api_ui_show_message;
	ctx.ui_add_menu_item = api_ui_add_menu_item;
	ctx.ui_file_open_dialog = api_ui_file_open_dialog;
	ctx.ui_file_save_dialog = api_ui_file_save_dialog;
	ctx.ui_input_dialog = api_ui_input_dialog;
	ctx.ui_confirm_dialog = api_ui_confirm_dialog;
	ctx.ui_register_instance_action = api_ui_register_instance_action;
	ctx.ui_register_instance_action_cb = api_ui_register_instance_action_cb;

	// S13 — UI Page Builder
	ctx.ui_page_create = api_ui_page_create;
	ctx.ui_page_add_to_list = api_ui_page_add_to_list;
	ctx.ui_layout_create = api_ui_layout_create;
	ctx.ui_layout_add_widget = api_ui_layout_add_widget;
	ctx.ui_layout_add_layout = api_ui_layout_add_layout;
	ctx.ui_layout_add_spacer = api_ui_layout_add_spacer;
	ctx.ui_page_set_layout = api_ui_page_set_layout;
	ctx.ui_button_create = api_ui_button_create;
	ctx.ui_button_set_enabled = api_ui_button_set_enabled;
	ctx.ui_button_set_text = api_ui_button_set_text;
	ctx.ui_label_create = api_ui_label_create;
	ctx.ui_label_set_text = api_ui_label_set_text;
	ctx.ui_tree_create = api_ui_tree_create;
	ctx.ui_tree_clear = api_ui_tree_clear;
	ctx.ui_tree_add_row = api_ui_tree_add_row;
	ctx.ui_tree_selected_row = api_ui_tree_selected_row;
	ctx.ui_tree_set_row_data = api_ui_tree_set_row_data;
	ctx.ui_tree_get_row_data = api_ui_tree_get_row_data;
	ctx.ui_tree_row_count = api_ui_tree_row_count;

	// S14 — Utility
	ctx.get_app_version = api_get_app_version;
	ctx.get_app_name = api_get_app_name;
	ctx.get_timestamp = api_get_timestamp;

	// S15 — Launch Modifiers
	ctx.launch_set_env = api_launch_set_env;
	ctx.launch_prepend_wrapper = api_launch_prepend_wrapper;

	// S16 — Application Settings
	ctx.app_setting_get = api_app_setting_get;

	return ctx;
}

void PluginManager::ensurePluginDataDir(PluginMetadata& meta)
{
	QString baseDir;
#ifdef Q_OS_WIN
	baseDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
#else
	baseDir = QDir::homePath() + "/.local/share/meshmc";
#endif
	meta.dataDir = QDir(baseDir).filePath("plugin-data/" + meta.moduleId());
	QDir().mkpath(meta.dataDir);
}

PluginManager::ModuleRuntime* PluginManager::rt(void* mh)
{
	return static_cast<ModuleRuntime*>(mh);
}

void PluginManager::api_log_info(void* mh, const char* msg)
{
	auto* r = rt(mh);
	auto& meta = r->manager->m_modules[r->moduleIndex];
	qInfo().noquote() << "[Plugin:" << meta.name << "]" << msg;
}

void PluginManager::api_log_warn(void* mh, const char* msg)
{
	auto* r = rt(mh);
	auto& meta = r->manager->m_modules[r->moduleIndex];
	qWarning().noquote() << "[Plugin:" << meta.name << "]" << msg;
}

void PluginManager::api_log_error(void* mh, const char* msg)
{
	auto* r = rt(mh);
	auto& meta = r->manager->m_modules[r->moduleIndex];
	qCritical().noquote() << "[Plugin:" << meta.name << "]" << msg;
}

void PluginManager::api_log_debug(void* mh, const char* msg)
{
	auto* r = rt(mh);
	auto& meta = r->manager->m_modules[r->moduleIndex];
	qDebug().noquote() << "[Plugin:" << meta.name << "]" << msg;
}

int PluginManager::api_hook_register(void* mh, uint32_t hook_id,
									 MMCOHookCallback cb, void* ud)
{
	auto* r = rt(mh);
	if (!cb)
		return -1;

	HookRegistration reg;
	reg.module_handle = mh;
	reg.callback = cb;
	reg.user_data = ud;

	r->manager->m_hooks.insert(hook_id, reg);
	return 0;
}

int PluginManager::api_hook_unregister(void* mh, uint32_t hook_id,
									   MMCOHookCallback cb)
{
	auto* r = rt(mh);
	auto& hooks = r->manager->m_hooks;

	auto range = hooks.equal_range(hook_id);
	for (auto it = range.first; it != range.second; ++it) {
		if (it.value().module_handle == mh && it.value().callback == cb) {
			hooks.erase(it);
			return 0;
		}
	}
	return -1;
}

const char* PluginManager::api_setting_get(void* mh, const char* key)
{
	auto* r = rt(mh);
	auto* app = r->manager->m_app;
	if (!app || !app->settings())
		return nullptr;

	auto& meta = r->manager->m_modules[r->moduleIndex];
	QString fullKey =
		QString("plugin.%1.%2").arg(meta.moduleId(), QString::fromUtf8(key));
	QVariant val = app->settings()->get(fullKey);
	if (!val.isValid())
		return nullptr;

	r->tempString = val.toString().toStdString();
	return r->tempString.c_str();
}

int PluginManager::api_setting_set(void* mh, const char* key, const char* value)
{
	auto* r = rt(mh);
	auto* app = r->manager->m_app;
	if (!app || !app->settings())
		return -1;

	auto& meta = r->manager->m_modules[r->moduleIndex];
	QString fullKey =
		QString("plugin.%1.%2").arg(meta.moduleId(), QString::fromUtf8(key));

	// Auto-register the setting if it doesn't exist yet
	if (!app->settings()->contains(fullKey)) {
		app->settings()->registerSetting(fullKey, QString());
	}

	app->settings()->set(fullKey, QString::fromUtf8(value));
	return 0;
}

int PluginManager::api_instance_count(void* mh)
{
	auto* r = rt(mh);
	auto* app = r->manager->m_app;
	if (!app || !app->instances())
		return 0;
	return app->instances()->count();
}

const char* PluginManager::api_instance_get_id(void* mh, int index)
{
	auto* r = rt(mh);
	auto* app = r->manager->m_app;
	if (!app || !app->instances())
		return nullptr;

	auto list = app->instances();
	if (index < 0 || index >= list->count())
		return nullptr;

	auto inst = list->at(index);
	if (!inst)
		return nullptr;

	r->tempString = inst->id().toStdString();
	return r->tempString.c_str();
}

const char* PluginManager::api_instance_get_name(void* mh, const char* id)
{
	auto* r = rt(mh);
	auto* app = r->manager->m_app;
	if (!app || !app->instances())
		return nullptr;

	auto inst = app->instances()->getInstanceById(QString::fromUtf8(id));
	if (!inst)
		return nullptr;

	r->tempString = inst->name().toStdString();
	return r->tempString.c_str();
}

const char* PluginManager::api_instance_get_path(void* mh, const char* id)
{
	auto* r = rt(mh);
	auto* app = r->manager->m_app;
	if (!app || !app->instances())
		return nullptr;

	auto inst = app->instances()->getInstanceById(QString::fromUtf8(id));
	if (!inst)
		return nullptr;

	r->tempString = inst->instanceRoot().toStdString();
	return r->tempString.c_str();
}

static BaseInstance* resolveInstance(PluginManager::ModuleRuntime* r,
									 const char* id)
{
	auto* app = r->manager->m_app;
	if (!app || !app->instances() || !id)
		return nullptr;
	auto inst = app->instances()->getInstanceById(QString::fromUtf8(id));
	return inst.get();
}

static MinecraftInstance* resolveMC(PluginManager::ModuleRuntime* r,
									const char* id)
{
	return dynamic_cast<MinecraftInstance*>(resolveInstance(r, id));
}

int PluginManager::api_instance_set_name(void* mh, const char* id,
										 const char* name)
{
	auto* r = rt(mh);
	auto* inst = resolveInstance(r, id);
	if (!inst || !name)
		return -1;
	inst->setName(QString::fromUtf8(name));
	return 0;
}

const char* PluginManager::api_instance_get_game_root(void* mh, const char* id)
{
	auto* r = rt(mh);
	auto* inst = resolveInstance(r, id);
	if (!inst)
		return nullptr;
	r->tempString = inst->gameRoot().toStdString();
	return r->tempString.c_str();
}

const char* PluginManager::api_instance_get_mods_root(void* mh, const char* id)
{
	auto* r = rt(mh);
	auto* inst = resolveInstance(r, id);
	if (!inst)
		return nullptr;
	r->tempString = inst->modsRoot().toStdString();
	return r->tempString.c_str();
}

const char* PluginManager::api_instance_get_icon_key(void* mh, const char* id)
{
	auto* r = rt(mh);
	auto* inst = resolveInstance(r, id);
	if (!inst)
		return nullptr;
	r->tempString = inst->iconKey().toStdString();
	return r->tempString.c_str();
}

int PluginManager::api_instance_set_icon_key(void* mh, const char* id,
											 const char* key)
{
	auto* r = rt(mh);
	auto* inst = resolveInstance(r, id);
	if (!inst || !key)
		return -1;
	inst->setIconKey(QString::fromUtf8(key));
	return 0;
}

const char* PluginManager::api_instance_get_type(void* mh, const char* id)
{
	auto* r = rt(mh);
	auto* inst = resolveInstance(r, id);
	if (!inst)
		return nullptr;
	r->tempString = inst->instanceType().toStdString();
	return r->tempString.c_str();
}

const char* PluginManager::api_instance_get_notes(void* mh, const char* id)
{
	auto* r = rt(mh);
	auto* inst = resolveInstance(r, id);
	if (!inst)
		return nullptr;
	r->tempString = inst->notes().toStdString();
	return r->tempString.c_str();
}

int PluginManager::api_instance_set_notes(void* mh, const char* id,
										  const char* notes)
{
	auto* r = rt(mh);
	auto* inst = resolveInstance(r, id);
	if (!inst || !notes)
		return -1;
	inst->setNotes(QString::fromUtf8(notes));
	return 0;
}

int PluginManager::api_instance_is_running(void* mh, const char* id)
{
	auto* r = rt(mh);
	auto* inst = resolveInstance(r, id);
	return inst ? (inst->isRunning() ? 1 : 0) : 0;
}

int PluginManager::api_instance_can_launch(void* mh, const char* id)
{
	auto* r = rt(mh);
	auto* inst = resolveInstance(r, id);
	return inst ? (inst->canLaunch() ? 1 : 0) : 0;
}

int PluginManager::api_instance_has_crashed(void* mh, const char* id)
{
	auto* r = rt(mh);
	auto* inst = resolveInstance(r, id);
	return inst ? (inst->hasCrashed() ? 1 : 0) : 0;
}

int PluginManager::api_instance_has_update(void* mh, const char* id)
{
	auto* r = rt(mh);
	auto* inst = resolveInstance(r, id);
	return inst ? (inst->hasUpdateAvailable() ? 1 : 0) : 0;
}

int64_t PluginManager::api_instance_get_total_play_time(void* mh,
														const char* id)
{
	auto* r = rt(mh);
	auto* inst = resolveInstance(r, id);
	return inst ? inst->totalTimePlayed() : 0;
}

int64_t PluginManager::api_instance_get_last_play_time(void* mh, const char* id)
{
	auto* r = rt(mh);
	auto* inst = resolveInstance(r, id);
	return inst ? inst->lastTimePlayed() : 0;
}

int64_t PluginManager::api_instance_get_last_launch(void* mh, const char* id)
{
	auto* r = rt(mh);
	auto* inst = resolveInstance(r, id);
	return inst ? inst->lastLaunch() : 0;
}

int PluginManager::api_instance_launch(void* mh, const char* id, int online)
{
	auto* r = rt(mh);
	auto* app = r->manager->m_app;
	if (!app || !app->instances() || !id)
		return -1;
	auto inst = app->instances()->getInstanceById(QString::fromUtf8(id));
	if (!inst)
		return -1;
	return app->launch(inst, online != 0) ? 0 : -1;
}

int PluginManager::api_instance_kill(void* mh, const char* id)
{
	auto* r = rt(mh);
	auto* app = r->manager->m_app;
	if (!app || !app->instances() || !id)
		return -1;
	auto inst = app->instances()->getInstanceById(QString::fromUtf8(id));
	if (!inst)
		return -1;
	return app->kill(inst) ? 0 : -1;
}

int PluginManager::api_instance_delete(void* mh, const char* id)
{
	auto* r = rt(mh);
	auto* app = r->manager->m_app;
	if (!app || !app->instances() || !id)
		return -1;
	app->instances()->deleteInstance(QString::fromUtf8(id));
	return 0;
}

const char* PluginManager::api_instance_get_group(void* mh, const char* id)
{
	auto* r = rt(mh);
	auto* app = r->manager->m_app;
	if (!app || !app->instances() || !id)
		return nullptr;
	r->tempString =
		app->instances()->getInstanceGroup(QString::fromUtf8(id)).toStdString();
	return r->tempString.c_str();
}

int PluginManager::api_instance_set_group(void* mh, const char* id,
										  const char* group)
{
	auto* r = rt(mh);
	auto* app = r->manager->m_app;
	if (!app || !app->instances() || !id)
		return -1;
	app->instances()->setInstanceGroup(
		QString::fromUtf8(id), group ? QString::fromUtf8(group) : QString());
	return 0;
}

int PluginManager::api_instance_group_count(void* mh)
{
	auto* r = rt(mh);
	auto* app = r->manager->m_app;
	if (!app || !app->instances())
		return 0;
	return app->instances()->getGroups().size();
}

const char* PluginManager::api_instance_group_at(void* mh, int index)
{
	auto* r = rt(mh);
	auto* app = r->manager->m_app;
	if (!app || !app->instances())
		return nullptr;
	auto groups = app->instances()->getGroups();
	if (index < 0 || index >= groups.size())
		return nullptr;
	r->tempString = groups.at(index).toStdString();
	return r->tempString.c_str();
}

int PluginManager::api_instance_component_count(void* mh, const char* id)
{
	auto* r = rt(mh);
	auto* mc = resolveMC(r, id);
	if (!mc || !mc->getPackProfile())
		return 0;
	return mc->getPackProfile()->rowCount(QModelIndex());
}

const char*
PluginManager::api_instance_component_get_uid(void* mh, const char* id, int idx)
{
	auto* r = rt(mh);
	auto* mc = resolveMC(r, id);
	if (!mc || !mc->getPackProfile())
		return nullptr;
	auto* comp = mc->getPackProfile()->getComponent(idx);
	if (!comp)
		return nullptr;
	r->tempString = comp->getID().toStdString();
	return r->tempString.c_str();
}

const char* PluginManager::api_instance_component_get_name(void* mh,
														   const char* id,
														   int idx)
{
	auto* r = rt(mh);
	auto* mc = resolveMC(r, id);
	if (!mc || !mc->getPackProfile())
		return nullptr;
	auto* comp = mc->getPackProfile()->getComponent(idx);
	if (!comp)
		return nullptr;
	r->tempString = comp->getName().toStdString();
	return r->tempString.c_str();
}

const char* PluginManager::api_instance_component_get_version(void* mh,
															  const char* id,
															  int idx)
{
	auto* r = rt(mh);
	auto* mc = resolveMC(r, id);
	if (!mc || !mc->getPackProfile())
		return nullptr;
	auto* comp = mc->getPackProfile()->getComponent(idx);
	if (!comp)
		return nullptr;
	r->tempString = comp->getVersion().toStdString();
	return r->tempString.c_str();
}

const char* PluginManager::api_instance_get_mc_version(void* mh, const char* id)
{
	auto* r = rt(mh);
	auto* mc = resolveMC(r, id);
	if (!mc || !mc->getPackProfile())
		return nullptr;
	r->tempString = mc->getPackProfile()
						->getComponentVersion("net.minecraft")
						.toStdString();
	return r->tempString.c_str();
}

const char* PluginManager::api_instance_get_jar_mods_dir(void* mh,
														 const char* id)
{
	auto* r = rt(mh);
	auto* mc = resolveMC(r, id);
	if (!mc)
		return nullptr;
	r->tempString = mc->jarModsDir().toStdString();
	return r->tempString.c_str();
}

const char* PluginManager::api_instance_get_resource_packs_dir(void* mh,
															   const char* id)
{
	auto* r = rt(mh);
	auto* mc = resolveMC(r, id);
	if (!mc)
		return nullptr;
	r->tempString = mc->resourcePacksDir().toStdString();
	return r->tempString.c_str();
}

const char* PluginManager::api_instance_get_texture_packs_dir(void* mh,
															  const char* id)
{
	auto* r = rt(mh);
	auto* mc = resolveMC(r, id);
	if (!mc)
		return nullptr;
	r->tempString = mc->texturePacksDir().toStdString();
	return r->tempString.c_str();
}

const char* PluginManager::api_instance_get_shader_packs_dir(void* mh,
															 const char* id)
{
	auto* r = rt(mh);
	auto* mc = resolveMC(r, id);
	if (!mc)
		return nullptr;
	r->tempString = mc->shaderPacksDir().toStdString();
	return r->tempString.c_str();
}

const char* PluginManager::api_instance_get_worlds_dir(void* mh, const char* id)
{
	auto* r = rt(mh);
	auto* mc = resolveMC(r, id);
	if (!mc)
		return nullptr;
	r->tempString = mc->worldDir().toStdString();
	return r->tempString.c_str();
}

const char* PluginManager::api_fs_plugin_data_dir(void* mh)
{
	auto* r = rt(mh);
	return r->dataDir.c_str();
}

static bool validateRelativePath(const char* rel)
{
	if (!rel || rel[0] == '\0')
		return false;
	QString p = QString::fromUtf8(rel);
	// Reject path traversal
	if (p.contains("..") || p.startsWith('/') || p.startsWith('\\'))
		return false;
	return true;
}

int64_t PluginManager::api_fs_read(void* mh, const char* rel, void* buf,
								   size_t sz)
{
	if (!validateRelativePath(rel))
		return -1;

	auto* r = rt(mh);
	QString path = QDir(QString::fromStdString(r->dataDir))
					   .filePath(QString::fromUtf8(rel));

	QFile f(path);
	if (!f.open(QIODevice::ReadOnly))
		return -1;

	qint64 bytesRead = f.read(static_cast<char*>(buf), static_cast<qint64>(sz));
	return bytesRead;
}

int PluginManager::api_fs_write(void* mh, const char* rel, const void* data,
								size_t sz)
{
	if (!validateRelativePath(rel))
		return -1;

	auto* r = rt(mh);
	QString path = QDir(QString::fromStdString(r->dataDir))
					   .filePath(QString::fromUtf8(rel));

	// Ensure parent directory exists
	QDir().mkpath(QFileInfo(path).absolutePath());

	QFile f(path);
	if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate))
		return -1;

	qint64 written =
		f.write(static_cast<const char*>(data), static_cast<qint64>(sz));
	return (written == static_cast<qint64>(sz)) ? 0 : -1;
}

int PluginManager::api_fs_exists(void* mh, const char* rel)
{
	if (!validateRelativePath(rel))
		return 0;

	auto* r = rt(mh);
	QString path = QDir(QString::fromStdString(r->dataDir))
					   .filePath(QString::fromUtf8(rel));
	return QFile::exists(path) ? 1 : 0;
}

int PluginManager::api_http_get(void* mh, const char* url, MMCOHttpCallback cb,
								void* ud)
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
	request.setHeader(QNetworkRequest::UserAgentHeader, BuildConfig.USER_AGENT);

	QNetworkReply* reply = nam->get(request);

	QObject::connect(reply, &QNetworkReply::finished, [reply, cb, ud]() {
		int status =
			reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
		QByteArray body = reply->readAll();
		cb(ud, status, body.constData(), static_cast<size_t>(body.size()));
		reply->deleteLater();
	});

	return 0;
}

int PluginManager::api_http_post(void* mh, const char* url, const void* body,
								 size_t body_sz, const char* ct,
								 MMCOHttpCallback cb, void* ud)
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
	request.setHeader(QNetworkRequest::UserAgentHeader, BuildConfig.USER_AGENT);
	if (ct)
		request.setHeader(QNetworkRequest::ContentTypeHeader,
						  QString::fromUtf8(ct));

	QByteArray postData(static_cast<const char*>(body),
						static_cast<int>(body_sz));
	QNetworkReply* reply = nam->post(request, postData);

	QObject::connect(reply, &QNetworkReply::finished, [reply, cb, ud]() {
		int status =
			reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
		QByteArray respBody = reply->readAll();
		cb(ud, status, respBody.constData(),
		   static_cast<size_t>(respBody.size()));
		reply->deleteLater();
	});

	return 0;
}

void PluginManager::api_ui_show_message(void* mh, int type, const char* title,
										const char* msg)
{
	auto* r = rt(mh);
	auto& meta = r->manager->m_modules[r->moduleIndex];

	QString qtitle =
		QString("[%1] %2").arg(meta.name, QString::fromUtf8(title));
	QString qmsg = QString::fromUtf8(msg);

	switch (type) {
		case 1:
			QMessageBox::warning(nullptr, qtitle, qmsg);
			break;
		case 2:
			QMessageBox::critical(nullptr, qtitle, qmsg);
			break;
		default:
			QMessageBox::information(nullptr, qtitle, qmsg);
			break;
	}
}

int PluginManager::api_ui_add_menu_item(void* /* mh */, void* menu_handle,
										const char* label,
										const char* /* icon */,
										MMCOMenuActionCallback cb, void* ud)
{
	if (!menu_handle || !label || !cb)
		return -1;

	auto* menu = static_cast<QMenu*>(menu_handle);
	QString qlabel = QString::fromUtf8(label);

	QAction* action = menu->addAction(qlabel);
	QObject::connect(action, &QAction::triggered, [cb, ud]() { cb(ud); });

	return 0;
}

const char* PluginManager::api_get_app_version(void* mh)
{
	auto* r = rt(mh);
	r->tempString = BuildConfig.VERSION_STR.toStdString();
	return r->tempString.c_str();
}

const char* PluginManager::api_get_app_name(void* mh)
{
	auto* r = rt(mh);
	r->tempString = BuildConfig.MESHMC_NAME.toStdString();
	return r->tempString.c_str();
}

int PluginManager::api_zip_compress_dir(void* mh, const char* zip,
										const char* dir)
{
	(void)mh;
	if (!zip || !dir)
		return -1;
	bool ok = MMCZip::compressDir(QString::fromUtf8(zip),
								  QString::fromUtf8(dir), nullptr);
	return ok ? 0 : -1;
}

int PluginManager::api_zip_extract(void* mh, const char* zip,
								   const char* target)
{
	(void)mh;
	if (!zip || !target)
		return -1;
	auto result =
		MMCZip::extractDir(QString::fromUtf8(zip), QString::fromUtf8(target));
	return result.has_value() ? 0 : -1;
}

int PluginManager::api_fs_list_dir(void* mh, const char* path, int type,
								   MMCODirEntryCallback cb, void* ud)
{
	(void)mh;
	if (!path || !cb)
		return -1;

	QDir dir(QString::fromUtf8(path));
	if (!dir.exists())
		return -1;

	QDir::Filters filters;
	if (type == 1)
		filters = QDir::Files;
	else if (type == 2)
		filters = QDir::Dirs | QDir::NoDotAndDotDot;
	else
		filters = QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot;

	const auto entries = dir.entryInfoList(filters);
	for (const auto& entry : entries) {
		cb(ud, entry.fileName().toUtf8().constData(), entry.isDir() ? 1 : 0);
	}
	return 0;
}

int PluginManager::api_fs_copy_file(void* mh, const char* src, const char* dst)
{
	(void)mh;
	if (!src || !dst)
		return -1;
	return QFile::copy(QString::fromUtf8(src), QString::fromUtf8(dst)) ? 0 : -1;
}

int PluginManager::api_fs_remove(void* mh, const char* path)
{
	(void)mh;
	if (!path)
		return -1;

	QFileInfo fi(QString::fromUtf8(path));
	if (fi.isDir()) {
		return QDir(fi.absoluteFilePath()).removeRecursively() ? 0 : -1;
	}
	return QFile::remove(fi.absoluteFilePath()) ? 0 : -1;
}

int PluginManager::api_fs_mkdir(void* mh, const char* path)
{
	(void)mh;
	if (!path)
		return -1;
	return QDir().mkpath(QString::fromUtf8(path)) ? 0 : -1;
}

int PluginManager::api_fs_exists_abs(void* mh, const char* path)
{
	(void)mh;
	if (!path)
		return 0;
	return QFileInfo::exists(QString::fromUtf8(path)) ? 1 : 0;
}

int64_t PluginManager::api_fs_file_size(void* mh, const char* path)
{
	(void)mh;
	if (!path)
		return -1;
	QFileInfo fi(QString::fromUtf8(path));
	if (!fi.exists())
		return -1;
	return fi.size();
}

int64_t PluginManager::api_get_timestamp(void* mh)
{
	(void)mh;
	return QDateTime::currentSecsSinceEpoch();
}

static std::shared_ptr<ModFolderModel>
resolveModList(PluginManager::ModuleRuntime* r, const char* inst_id,
			   const char* type)
{
	auto* mc = resolveMC(r, inst_id);
	if (!mc || !type)
		return nullptr;

	QString t = QString::fromUtf8(type).toLower();
	if (t == "loader")
		return mc->loaderModList();
	if (t == "core")
		return mc->coreModList();
	if (t == "resourcepack")
		return mc->resourcePackList();
	if (t == "texturepack")
		return mc->texturePackList();
	if (t == "shaderpack")
		return mc->shaderPackList();
	return nullptr;
}

int PluginManager::api_mod_count(void* mh, const char* inst, const char* type)
{
	auto* r = rt(mh);
	auto model = resolveModList(r, inst, type);
	return model ? static_cast<int>(model->size()) : 0;
}

const char* PluginManager::api_mod_get_name(void* mh, const char* inst,
											const char* type, int idx)
{
	auto* r = rt(mh);
	auto model = resolveModList(r, inst, type);
	if (!model || idx < 0 || idx >= static_cast<int>(model->size()))
		return nullptr;
	r->tempString = model->at(idx).name().toStdString();
	return r->tempString.c_str();
}

const char* PluginManager::api_mod_get_version(void* mh, const char* inst,
											   const char* type, int idx)
{
	auto* r = rt(mh);
	auto model = resolveModList(r, inst, type);
	if (!model || idx < 0 || idx >= static_cast<int>(model->size()))
		return nullptr;
	r->tempString = model->at(idx).version().toStdString();
	return r->tempString.c_str();
}

const char* PluginManager::api_mod_get_filename(void* mh, const char* inst,
												const char* type, int idx)
{
	auto* r = rt(mh);
	auto model = resolveModList(r, inst, type);
	if (!model || idx < 0 || idx >= static_cast<int>(model->size()))
		return nullptr;
	r->tempString = model->at(idx).filename().fileName().toStdString();
	return r->tempString.c_str();
}

const char* PluginManager::api_mod_get_description(void* mh, const char* inst,
												   const char* type, int idx)
{
	auto* r = rt(mh);
	auto model = resolveModList(r, inst, type);
	if (!model || idx < 0 || idx >= static_cast<int>(model->size()))
		return nullptr;
	r->tempString = model->at(idx).description().toStdString();
	return r->tempString.c_str();
}

int PluginManager::api_mod_is_enabled(void* mh, const char* inst,
									  const char* type, int idx)
{
	auto* r = rt(mh);
	auto model = resolveModList(r, inst, type);
	if (!model || idx < 0 || idx >= static_cast<int>(model->size()))
		return 0;
	return model->at(idx).enabled() ? 1 : 0;
}

int PluginManager::api_mod_set_enabled(void* mh, const char* inst,
									   const char* type, int idx, int e)
{
	auto* r = rt(mh);
	auto model = resolveModList(r, inst, type);
	if (!model || idx < 0 || idx >= static_cast<int>(model->size()))
		return -1;
	QModelIndexList indices;
	indices.append(model->index(idx, 0));
	return model->setModStatus(indices, e ? ModFolderModel::Enable
										  : ModFolderModel::Disable)
			   ? 0
			   : -1;
}

int PluginManager::api_mod_remove(void* mh, const char* inst, const char* type,
								  int idx)
{
	auto* r = rt(mh);
	auto model = resolveModList(r, inst, type);
	if (!model || idx < 0 || idx >= static_cast<int>(model->size()))
		return -1;
	QModelIndexList indices;
	indices.append(model->index(idx, 0));
	return model->deleteMods(indices) ? 0 : -1;
}

int PluginManager::api_mod_install(void* mh, const char* inst, const char* type,
								   const char* path)
{
	auto* r = rt(mh);
	auto model = resolveModList(r, inst, type);
	if (!model || !path)
		return -1;
	return model->installMod(QString::fromUtf8(path)) ? 0 : -1;
}

int PluginManager::api_mod_refresh(void* mh, const char* inst, const char* type)
{
	auto* r = rt(mh);
	auto model = resolveModList(r, inst, type);
	if (!model)
		return -1;
	return model->update() ? 0 : -1;
}

static std::shared_ptr<WorldList>
resolveWorldList(PluginManager::ModuleRuntime* r, const char* inst_id)
{
	auto* mc = resolveMC(r, inst_id);
	if (!mc)
		return nullptr;
	return mc->worldList();
}

int PluginManager::api_world_count(void* mh, const char* inst)
{
	auto* r = rt(mh);
	auto wl = resolveWorldList(r, inst);
	return wl ? static_cast<int>(wl->size()) : 0;
}

const char* PluginManager::api_world_get_name(void* mh, const char* inst,
											  int idx)
{
	auto* r = rt(mh);
	auto wl = resolveWorldList(r, inst);
	if (!wl || idx < 0 || idx >= static_cast<int>(wl->size()))
		return nullptr;
	r->tempString = wl->allWorlds().at(idx).name().toStdString();
	return r->tempString.c_str();
}

const char* PluginManager::api_world_get_folder(void* mh, const char* inst,
												int idx)
{
	auto* r = rt(mh);
	auto wl = resolveWorldList(r, inst);
	if (!wl || idx < 0 || idx >= static_cast<int>(wl->size()))
		return nullptr;
	r->tempString = wl->allWorlds().at(idx).folderName().toStdString();
	return r->tempString.c_str();
}

int64_t PluginManager::api_world_get_seed(void* mh, const char* inst, int idx)
{
	auto* r = rt(mh);
	auto wl = resolveWorldList(r, inst);
	if (!wl || idx < 0 || idx >= static_cast<int>(wl->size()))
		return 0;
	return wl->allWorlds().at(idx).seed();
}

int PluginManager::api_world_get_game_type(void* mh, const char* inst, int idx)
{
	auto* r = rt(mh);
	auto wl = resolveWorldList(r, inst);
	if (!wl || idx < 0 || idx >= static_cast<int>(wl->size()))
		return -1;
	return wl->allWorlds().at(idx).gameType().type;
}

int64_t PluginManager::api_world_get_last_played(void* mh, const char* inst,
												 int idx)
{
	auto* r = rt(mh);
	auto wl = resolveWorldList(r, inst);
	if (!wl || idx < 0 || idx >= static_cast<int>(wl->size()))
		return 0;
	return wl->allWorlds().at(idx).lastPlayed().toMSecsSinceEpoch();
}

int PluginManager::api_world_delete(void* mh, const char* inst, int idx)
{
	auto* r = rt(mh);
	auto wl = resolveWorldList(r, inst);
	if (!wl || idx < 0 || idx >= static_cast<int>(wl->size()))
		return -1;
	return wl->deleteWorld(idx) ? 0 : -1;
}

int PluginManager::api_world_rename(void* mh, const char* inst, int idx,
									const char* name)
{
	auto* r = rt(mh);
	auto wl = resolveWorldList(r, inst);
	if (!wl || idx < 0 || idx >= static_cast<int>(wl->size()) || !name)
		return -1;
	// WorldList doesn't expose rename by index; access the world directly
	auto& worlds = wl->allWorlds();
	World w = worlds.at(idx);
	return w.rename(QString::fromUtf8(name)) ? 0 : -1;
}

int PluginManager::api_world_install(void* mh, const char* inst,
									 const char* path)
{
	auto* r = rt(mh);
	auto wl = resolveWorldList(r, inst);
	if (!wl || !path)
		return -1;
	wl->installWorld(QFileInfo(QString::fromUtf8(path)));
	return 0;
}

int PluginManager::api_world_refresh(void* mh, const char* inst)
{
	auto* r = rt(mh);
	auto wl = resolveWorldList(r, inst);
	if (!wl)
		return -1;
	return wl->update() ? 0 : -1;
}

int PluginManager::api_account_count(void* mh)
{
	auto* r = rt(mh);
	auto* app = r->manager->m_app;
	if (!app || !app->accounts())
		return 0;
	return app->accounts()->count();
}

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

int PluginManager::api_account_is_active(void* mh, int idx)
{
	auto* r = rt(mh);
	auto* app = r->manager->m_app;
	if (!app || !app->accounts())
		return 0;
	auto acc = app->accounts()->at(idx);
	return (acc && acc->isActive()) ? 1 : 0;
}

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

int PluginManager::api_java_count(void* mh)
{
	auto* r = rt(mh);
	auto* app = r->manager->m_app;
	if (!app || !app->javalist() || !app->javalist()->isLoaded())
		return 0;
	return app->javalist()->count();
}

const char* PluginManager::api_java_get_version(void* mh, int idx)
{
	auto* r = rt(mh);
	auto* app = r->manager->m_app;
	if (!app || !app->javalist() || !app->javalist()->isLoaded())
		return nullptr;
	if (idx < 0 || idx >= app->javalist()->count())
		return nullptr;
	auto ver = std::dynamic_pointer_cast<JavaInstall>(app->javalist()->at(idx));
	if (!ver)
		return nullptr;
	r->tempString = ver->id.toString().toStdString();
	return r->tempString.c_str();
}

const char* PluginManager::api_java_get_arch(void* mh, int idx)
{
	auto* r = rt(mh);
	auto* app = r->manager->m_app;
	if (!app || !app->javalist() || !app->javalist()->isLoaded())
		return nullptr;
	if (idx < 0 || idx >= app->javalist()->count())
		return nullptr;
	auto ver = std::dynamic_pointer_cast<JavaInstall>(app->javalist()->at(idx));
	if (!ver)
		return nullptr;
	r->tempString = ver->arch.toStdString();
	return r->tempString.c_str();
}

const char* PluginManager::api_java_get_path(void* mh, int idx)
{
	auto* r = rt(mh);
	auto* app = r->manager->m_app;
	if (!app || !app->javalist() || !app->javalist()->isLoaded())
		return nullptr;
	if (idx < 0 || idx >= app->javalist()->count())
		return nullptr;
	auto ver = std::dynamic_pointer_cast<JavaInstall>(app->javalist()->at(idx));
	if (!ver)
		return nullptr;
	r->tempString = ver->path.toStdString();
	return r->tempString.c_str();
}

int PluginManager::api_java_is_recommended(void* mh, int idx)
{
	auto* r = rt(mh);
	auto* app = r->manager->m_app;
	if (!app || !app->javalist() || !app->javalist()->isLoaded())
		return 0;
	if (idx < 0 || idx >= app->javalist()->count())
		return 0;
	auto ver = std::dynamic_pointer_cast<JavaInstall>(app->javalist()->at(idx));
	return (ver && ver->recommended) ? 1 : 0;
}

const char* PluginManager::api_instance_get_java_version(void* mh,
														 const char* id)
{
	auto* r = rt(mh);
	auto* mc = resolveMC(r, id);
	if (!mc)
		return nullptr;
	r->tempString = mc->getJavaVersion().toString().toStdString();
	return r->tempString.c_str();
}

const char* PluginManager::api_ui_file_open_dialog(void* mh, const char* title,
												   const char* filter)
{
	auto* r = rt(mh);
	QString result = QFileDialog::getOpenFileName(
		QApplication::activeWindow(),
		title ? QString::fromUtf8(title) : QString(), QString(),
		filter ? QString::fromUtf8(filter) : QString());
	if (result.isEmpty())
		return nullptr;
	r->tempString = result.toStdString();
	return r->tempString.c_str();
}

const char* PluginManager::api_ui_file_save_dialog(void* mh, const char* title,
												   const char* def,
												   const char* filter)
{
	auto* r = rt(mh);
	QString result = QFileDialog::getSaveFileName(
		QApplication::activeWindow(),
		title ? QString::fromUtf8(title) : QString(),
		def ? QString::fromUtf8(def) : QString(),
		filter ? QString::fromUtf8(filter) : QString());
	if (result.isEmpty())
		return nullptr;
	r->tempString = result.toStdString();
	return r->tempString.c_str();
}

const char* PluginManager::api_ui_input_dialog(void* mh, const char* title,
											   const char* prompt,
											   const char* def)
{
	auto* r = rt(mh);
	bool ok = false;
	QString result = QInputDialog::getText(
		nullptr, title ? QString::fromUtf8(title) : QString(),
		prompt ? QString::fromUtf8(prompt) : QString(), QLineEdit::Normal,
		def ? QString::fromUtf8(def) : QString(), &ok);
	if (!ok)
		return nullptr;
	r->tempString = result.toStdString();
	return r->tempString.c_str();
}

int PluginManager::api_ui_confirm_dialog(void* mh, const char* title,
										 const char* msg)
{
	(void)mh;
	auto ret = QMessageBox::question(
		nullptr, title ? QString::fromUtf8(title) : QString(),
		msg ? QString::fromUtf8(msg) : QString(),
		QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
	return ret == QMessageBox::Yes ? 1 : 0;
}

int PluginManager::api_ui_register_instance_action(void* mh, const char* text,
												   const char* tooltip,
												   const char* icon_name,
												   const char* page_id)
{
	(void)mh;
	auto* pm = APPLICATION->pluginManager();
	if (!pm)
		return 0;
	InstanceAction action;
	action.text = text ? QString::fromUtf8(text) : QString();
	action.tooltip = tooltip ? QString::fromUtf8(tooltip) : QString();
	action.iconName = icon_name ? QString::fromUtf8(icon_name) : QString();
	action.pageId = page_id ? QString::fromUtf8(page_id) : QString();
	pm->m_instanceActions.append(action);
	return 1;
}

int PluginManager::api_ui_register_instance_action_cb(
	void* mh, const char* text, const char* tooltip, const char* icon_name,
	void (*cb)(void* ud), void* ud)
{
	(void)mh;
	auto* pm = APPLICATION->pluginManager();
	if (!pm || !cb)
		return 0;
	InstanceCallbackAction action;
	action.text = text ? QString::fromUtf8(text) : QString();
	action.tooltip = tooltip ? QString::fromUtf8(tooltip) : QString();
	action.iconName = icon_name ? QString::fromUtf8(icon_name) : QString();
	action.callback = cb;
	action.userData = ud;
	pm->m_instanceCallbackActions.append(action);
	return 1;
}

#include "ui/pages/BasePage.h"

namespace
{

	class PluginPage : public QWidget, public BasePage
	{
		Q_OBJECT
	  public:
		PluginPage(const QString& pageId, const QString& displayName,
				   const QString& iconName, QWidget* parent = nullptr)
			: QWidget(parent), m_id(pageId), m_displayName(displayName),
			  m_iconName(iconName)
		{
		}

		QString id() const override
		{
			return m_id;
		}
		QString displayName() const override
		{
			return m_displayName;
		}
		QIcon icon() const override
		{
			return QIcon::fromTheme(m_iconName);
		}
		bool shouldDisplay() const override
		{
			return true;
		}

	  private:
		QString m_id;
		QString m_displayName;
		QString m_iconName;
	};

} // anonymous namespace

void* PluginManager::api_ui_page_create(void* mh, const char* id,
										const char* name, const char* iconName)
{
	(void)mh;
	if (!id || !name)
		return nullptr;
	auto* page = new PluginPage(QString::fromUtf8(id), QString::fromUtf8(name),
								iconName ? QString::fromUtf8(iconName)
										 : QStringLiteral("plugin"));
	return static_cast<QWidget*>(page);
}

int PluginManager::api_ui_page_add_to_list(void* mh, void* page, void* list)
{
	(void)mh;
	if (!page || !list)
		return -1;
	auto* pageWidget = static_cast<QWidget*>(page);
	auto* pageBase = dynamic_cast<BasePage*>(pageWidget);
	if (!pageBase)
		return -1;
	auto* pages = static_cast<QList<BasePage*>*>(list);
	pages->append(pageBase);
	return 0;
}

void* PluginManager::api_ui_layout_create(void* mh, void* parent, int type)
{
	(void)mh;
	QWidget* pw = parent ? static_cast<QWidget*>(parent) : nullptr;
	QBoxLayout* layout;
	if (type == 1)
		layout = new QHBoxLayout();
	else
		layout = new QVBoxLayout();
	// Don't set on parent yet — let page_set_layout do that
	(void)pw;
	return layout;
}

int PluginManager::api_ui_layout_add_widget(void* mh, void* layout,
											void* widget)
{
	(void)mh;
	if (!layout || !widget)
		return -1;
	auto* l = static_cast<QBoxLayout*>(layout);
	l->addWidget(static_cast<QWidget*>(widget));
	return 0;
}

int PluginManager::api_ui_layout_add_layout(void* mh, void* parent, void* child)
{
	(void)mh;
	if (!parent || !child)
		return -1;
	auto* p = static_cast<QBoxLayout*>(parent);
	p->addLayout(static_cast<QLayout*>(child));
	return 0;
}

int PluginManager::api_ui_layout_add_spacer(void* mh, void* layout,
											int horizontal)
{
	(void)mh;
	if (!layout)
		return -1;
	auto* l = static_cast<QBoxLayout*>(layout);
	if (horizontal)
		l->addItem(new QSpacerItem(0, 0, QSizePolicy::Expanding,
								   QSizePolicy::Minimum));
	else
		l->addItem(new QSpacerItem(0, 0, QSizePolicy::Minimum,
								   QSizePolicy::Expanding));
	return 0;
}

int PluginManager::api_ui_page_set_layout(void* mh, void* page, void* layout)
{
	(void)mh;
	if (!page || !layout)
		return -1;
	auto* w = static_cast<QWidget*>(page);
	w->setLayout(static_cast<QLayout*>(layout));
	return 0;
}

void* PluginManager::api_ui_button_create(void* mh, void* parent,
										  const char* text,
										  const char* iconName,
										  MMCOButtonCallback cb, void* ud)
{
	(void)mh;
	auto* btn = new QPushButton(text ? QString::fromUtf8(text) : QString());
	if (iconName && iconName[0] != '\0')
		btn->setIcon(QIcon::fromTheme(QString::fromUtf8(iconName)));
	if (parent)
		btn->setParent(static_cast<QWidget*>(parent));
	if (cb) {
		QObject::connect(btn, &QPushButton::clicked, [cb, ud]() { cb(ud); });
	}
	return btn;
}

int PluginManager::api_ui_button_set_enabled(void* mh, void* btn, int enabled)
{
	(void)mh;
	if (!btn)
		return -1;
	static_cast<QPushButton*>(btn)->setEnabled(enabled != 0);
	return 0;
}

int PluginManager::api_ui_button_set_text(void* mh, void* btn, const char* text)
{
	(void)mh;
	if (!btn)
		return -1;
	static_cast<QPushButton*>(btn)->setText(text ? QString::fromUtf8(text)
												 : QString());
	return 0;
}

void* PluginManager::api_ui_label_create(void* mh, void* parent,
										 const char* text)
{
	(void)mh;
	auto* lbl = new QLabel(text ? QString::fromUtf8(text) : QString());
	if (parent)
		lbl->setParent(static_cast<QWidget*>(parent));
	return lbl;
}

int PluginManager::api_ui_label_set_text(void* mh, void* label,
										 const char* text)
{
	(void)mh;
	if (!label)
		return -1;
	static_cast<QLabel*>(label)->setText(text ? QString::fromUtf8(text)
											  : QString());
	return 0;
}

void* PluginManager::api_ui_tree_create(void* mh, void* parent,
										const char** cols, int ncols,
										MMCOTreeSelectionCallback cb, void* ud)
{
	(void)mh;
	auto* tree = new QTreeWidget();
	tree->setRootIsDecorated(false);
	tree->setSortingEnabled(true);
	tree->setAlternatingRowColors(true);
	tree->setSelectionMode(QAbstractItemView::SingleSelection);

	if (parent)
		tree->setParent(static_cast<QWidget*>(parent));

	QStringList headers;
	for (int i = 0; i < ncols; ++i)
		headers << (cols[i] ? QString::fromUtf8(cols[i]) : QString());
	tree->setHeaderLabels(headers);

	// First column stretches
	if (ncols > 0) {
		tree->header()->setStretchLastSection(false);
		tree->header()->setSectionResizeMode(0, QHeaderView::Stretch);
		for (int i = 1; i < ncols; ++i)
			tree->header()->setSectionResizeMode(i,
												 QHeaderView::ResizeToContents);
	}

	if (cb) {
		QObject::connect(
			tree, &QTreeWidget::itemSelectionChanged, [tree, cb, ud]() {
				auto items = tree->selectedItems();
				int row = items.isEmpty()
							  ? -1
							  : tree->indexOfTopLevelItem(items.first());
				cb(ud, row);
			});
	}
	return tree;
}

int PluginManager::api_ui_tree_clear(void* mh, void* tree)
{
	(void)mh;
	if (!tree)
		return -1;
	static_cast<QTreeWidget*>(tree)->clear();
	return 0;
}

int PluginManager::api_ui_tree_add_row(void* mh, void* tree, const char** vals,
									   int ncols)
{
	(void)mh;
	if (!tree)
		return -1;
	auto* tw = static_cast<QTreeWidget*>(tree);
	auto* item = new QTreeWidgetItem(tw);
	for (int i = 0; i < ncols; ++i)
		item->setText(i, vals[i] ? QString::fromUtf8(vals[i]) : QString());
	return tw->indexOfTopLevelItem(item);
}

int PluginManager::api_ui_tree_selected_row(void* mh, void* tree)
{
	(void)mh;
	if (!tree)
		return -1;
	auto* tw = static_cast<QTreeWidget*>(tree);
	auto items = tw->selectedItems();
	if (items.isEmpty())
		return -1;
	return tw->indexOfTopLevelItem(items.first());
}

int PluginManager::api_ui_tree_set_row_data(void* mh, void* tree, int row,
											int64_t data)
{
	(void)mh;
	if (!tree)
		return -1;
	auto* tw = static_cast<QTreeWidget*>(tree);
	auto* item = tw->topLevelItem(row);
	if (!item)
		return -1;
	item->setData(0, Qt::UserRole, QVariant::fromValue(data));
	return 0;
}

int64_t PluginManager::api_ui_tree_get_row_data(void* mh, void* tree, int row)
{
	(void)mh;
	if (!tree)
		return 0;
	auto* tw = static_cast<QTreeWidget*>(tree);
	auto* item = tw->topLevelItem(row);
	if (!item)
		return 0;
	return item->data(0, Qt::UserRole).toLongLong();
}

int PluginManager::api_ui_tree_row_count(void* mh, void* tree)
{
	(void)mh;
	if (!tree)
		return 0;
	return static_cast<QTreeWidget*>(tree)->topLevelItemCount();
}

/* ── S15 — Launch Modifiers ───────────────────────────────────────── */

int PluginManager::api_launch_set_env(void* mh, const char* key,
									  const char* value)
{
	auto* r = rt(mh);
	if (!key || !value)
		return -1;
	r->manager->m_pendingLaunchEnv.insert(QString::fromUtf8(key),
										  QString::fromUtf8(value));
	return 0;
}

int PluginManager::api_launch_prepend_wrapper(void* mh, const char* wrapper_cmd)
{
	auto* r = rt(mh);
	if (!wrapper_cmd || wrapper_cmd[0] == '\0')
		return -1;
	QString cmd = QString::fromUtf8(wrapper_cmd);
	if (r->manager->m_pendingLaunchWrapper.isEmpty()) {
		r->manager->m_pendingLaunchWrapper = cmd;
	} else {
		r->manager->m_pendingLaunchWrapper =
			cmd + " " + r->manager->m_pendingLaunchWrapper;
	}
	return 0;
}

void PluginManager::clearPendingLaunchMods()
{
	m_pendingLaunchEnv.clear();
	m_pendingLaunchWrapper.clear();
}

/* ── S16 — Application Settings ───────────────────────────────────── */

const char* PluginManager::api_app_setting_get(void* mh, const char* key)
{
	auto* r = rt(mh);
	auto* app = r->manager->m_app;
	if (!app || !app->settings() || !key)
		return nullptr;

	QString qKey = QString::fromUtf8(key);
	if (!app->settings()->contains(qKey))
		return nullptr;

	QVariant val = app->settings()->get(qKey);
	if (!val.isValid())
		return nullptr;

	r->tempString = val.toString().toStdString();
	return r->tempString.c_str();
}

QMap<QString, QString> PluginManager::takePendingLaunchEnv()
{
	QMap<QString, QString> env;
	env.swap(m_pendingLaunchEnv);
	return env;
}

QString PluginManager::takePendingLaunchWrapper()
{
	QString w;
	w.swap(m_pendingLaunchWrapper);
	return w;
}

/* PluginPage MOC — required because PluginPage has Q_OBJECT */
#include "PluginManager.moc"
