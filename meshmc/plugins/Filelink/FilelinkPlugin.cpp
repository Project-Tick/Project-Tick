/* SPDX-FileCopyrightText: 2026 Project Tick
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * FilelinkPlugin — MMCO plugin that creates launcher shortcuts for
 * MeshMC instances.
 *
 * Creates .desktop files (Linux) or .lnk shortcuts (Windows) so that
 * instances can be launched directly without opening MeshMC first.
 *
 * macOS is NOT supported.
 *
 * The shortcut is created via a context-menu action that opens a
 * dialog where the user can customise the shortcut name, description,
 * icon, and (optionally) a server to auto-join.
 */

#include "plugin/sdk/mmco_sdk.h"

#include <QComboBox>
#include <QStandardPaths>

MMCO_DEFINE_MODULE("Filelink", "2.1.0", "Project Tick",
				   "Create desktop shortcuts for instances",
				   "GPL-3.0-or-later");

static MMCOContext* g_ctx = nullptr;

static QString sanitizeShortcutName(const QString& shortcutName)
{
	QString safeName = shortcutName;
#ifdef Q_OS_WIN
	safeName.replace(QRegularExpression(R"([<>:"/\\|?*])"), "_");
#else
	safeName.replace('/', '_');
#endif
	return safeName;
}

static QString shortcutFileExtension()
{
#ifdef Q_OS_WIN
	return QStringLiteral(".lnk");
#elif defined(Q_OS_LINUX)
	return QStringLiteral(".desktop");
#else
	return QString();
#endif
}

static QString defaultShortcutFileName(const QString& shortcutName)
{
	return sanitizeShortcutName(shortcutName) + shortcutFileExtension();
}

static QString ensureShortcutExtension(QString path)
{
	QString extension = shortcutFileExtension();
	if (!extension.isEmpty() &&
		!path.endsWith(extension, Qt::CaseInsensitive)) {
		path += extension;
	}
	return path;
}

static bool isFlatpakSandbox()
{
#ifdef Q_OS_LINUX
	return QFileInfo::exists(QStringLiteral("/.flatpak-info")) ||
		   qEnvironmentVariableIsSet("FLATPAK_ID");
#else
	return false;
#endif
}

static QString currentSelectedInstanceId()
{
	if (!g_ctx)
		return {};
	const char* selected =
		g_ctx->app_setting_get(g_ctx->module_handle, "SelectedInstance");
	return selected ? QString::fromUtf8(selected) : QString();
}

static QString resolveShortcutIconPath(const QString& iconKey)
{
	auto iconList = APPLICATION->icons();
	if (!iconList)
		return {};

	QString effectiveKey =
		iconKey.isEmpty() ? QStringLiteral("default") : iconKey;
	if (auto* iconEntry = iconList->icon(effectiveKey)) {
		QString existingPath = iconEntry->getFilePath();
		if (!existingPath.isEmpty() && QFileInfo::exists(existingPath))
			return existingPath;
	}

	QString dataRoot =
		QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
	if (dataRoot.isEmpty())
		dataRoot = QDir::tempPath();

	QString iconDir =
		QDir(dataRoot).filePath(QStringLiteral("filelink-shortcut-icons"));
	if (!QDir().mkpath(iconDir))
		return {};

	QString safeKey = effectiveKey;
	safeKey.replace(QRegularExpression(R"([^A-Za-z0-9_.-])"), "_");
	if (safeKey.isEmpty())
		safeKey = QStringLiteral("default");

	QString exportedPath =
		QDir(iconDir).filePath(safeKey + QStringLiteral(".png"));
	iconList->saveIcon(effectiveKey, exportedPath, "PNG");
	return QFileInfo::exists(exportedPath) ? exportedPath : QString();
}

static QString desktopExecQuote(QString argument)
{
	argument.replace('\\', QStringLiteral("\\\\"));
	argument.replace('"', QStringLiteral("\\\""));
	return QStringLiteral("\"%1\"").arg(argument);
}

static QString buildShortcutExecLine(const QString& instanceId,
							 const QString& serverAddress)
{
#ifdef Q_OS_LINUX
	if (isFlatpakSandbox()) {
		QString appId = qEnvironmentVariable("FLATPAK_ID");
		if (appId.isEmpty())
			appId = QStringLiteral("org.projecttick.MeshMC");

		QString execLine =
			QStringLiteral("flatpak run %1 --launch %2")
				.arg(appId, desktopExecQuote(instanceId));
		if (!serverAddress.isEmpty()) {
			execLine +=
				QStringLiteral(" --server %1").arg(desktopExecQuote(serverAddress));
		}
		return execLine;
	}
#endif

	QString execLine = QStringLiteral("%1 --launch %2")
					   .arg(
						   desktopExecQuote(QCoreApplication::applicationFilePath()),
						   desktopExecQuote(instanceId));
	if (!serverAddress.isEmpty()) {
		execLine +=
			QStringLiteral(" --server %1").arg(desktopExecQuote(serverAddress));
	}
	return execLine;
}

#ifdef Q_OS_WIN

#include <shlobj.h>
#include <objbase.h>
#include <shobjidl.h>

static QString desktopPath()
{
	wchar_t path[MAX_PATH];
	if (SUCCEEDED(SHGetFolderPathW(nullptr, CSIDL_DESKTOPDIRECTORY, nullptr, 0,
								   path))) {
		return QString::fromWCharArray(path);
	}
	return QStandardPaths::writableLocation(QStandardPaths::DesktopLocation);
}

static bool createShortcut(const QString& instanceId,
						   const QString& shortcutPath,
						   const QString& shortcutName,
						   const QString& description, const QString& iconPath,
						   const QString& serverAddress)
{
	if (shortcutPath.isEmpty())
		return false;

	QFileInfo shortcutInfo(shortcutPath);
	if (!QDir().mkpath(shortcutInfo.absolutePath()))
		return false;

	QString exePath = QCoreApplication::applicationFilePath();

	CoInitialize(nullptr);

	IShellLinkW* psl = nullptr;
	HRESULT hr =
		CoCreateInstance(CLSID_ShellLink, nullptr, CLSCTX_INPROC_SERVER,
						 IID_IShellLinkW, (void**)&psl);
	if (FAILED(hr)) {
		CoUninitialize();
		return false;
	}

	psl->SetPath(reinterpret_cast<LPCWSTR>(exePath.utf16()));

	QString args = QString("--launch \"%1\"").arg(instanceId);
	if (!serverAddress.isEmpty())
		args += QString(" --server \"%1\"").arg(serverAddress);
	psl->SetArguments(reinterpret_cast<LPCWSTR>(args.utf16()));

	psl->SetDescription(reinterpret_cast<LPCWSTR>(description.utf16()));
	if (!iconPath.isEmpty()) {
		psl->SetIconLocation(reinterpret_cast<LPCWSTR>(iconPath.utf16()), 0);
	}

	IPersistFile* ppf = nullptr;
	hr = psl->QueryInterface(IID_IPersistFile, (void**)&ppf);
	if (SUCCEEDED(hr)) {
		hr = ppf->Save(reinterpret_cast<LPCOLESTR>(shortcutPath.utf16()), TRUE);
		ppf->Release();
	}

	psl->Release();
	CoUninitialize();
	return SUCCEEDED(hr);
}

static bool removeShortcut(const QString& shortcutName)
{
	QString desktop = desktopPath();
	return QFile::remove(
		QDir(desktop).filePath(sanitizeShortcutName(shortcutName) + ".lnk"));
}

#elif defined(Q_OS_LINUX)

static QString desktopPath()
{
	return QStandardPaths::writableLocation(QStandardPaths::DesktopLocation);
}

static bool createShortcut(const QString& instanceId,
						   const QString& shortcutPath,
						   const QString& shortcutName,
						   const QString& description, const QString& iconPath,
						   const QString& serverAddress)
{
	if (shortcutPath.isEmpty())
		return false;

	QFileInfo shortcutInfo(shortcutPath);
	if (!QDir().mkpath(shortcutInfo.absolutePath()))
		return false;

	QString execLine = buildShortcutExecLine(instanceId, serverAddress);

	QString iconName = iconPath.isEmpty()
						   ? QStringLiteral("org.projecttick.MeshMC")
						   : iconPath;

	QString content = QStringLiteral("[Desktop Entry]\n"
									 "Type=Application\n"
									 "Name=%1\n"
									 "Comment=%2\n"
									 "Exec=%3\n"
									 "Icon=%4\n"
									 "Terminal=false\n"
									 "Categories=Game;\n")
						  .arg(shortcutName, description, execLine, iconName);

	QFile f(shortcutPath);
	if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text))
		return false;

	f.write(content.toUtf8());
	f.close();

	f.setPermissions(f.permissions() | QFileDevice::ExeUser);
	return true;
}

static bool removeShortcut(const QString& shortcutName)
{
	QString desktop = desktopPath();
	return QFile::remove(QDir(desktop).filePath(
		sanitizeShortcutName(shortcutName) + ".desktop"));
}

#else
static bool createShortcut(const QString&, const QString&, const QString&,
						   const QString&, const QString&, const QString&)
{
	return false;
}
static bool removeShortcut(const QString&)
{
	return false;
}
#endif

static void showFilelinkDialog(const QString& preselectedId = {})
{
	auto* win = QApplication::activeWindow();
	if (!win)
		return;

	auto instList = APPLICATION->instances();
	if (!instList || instList->count() == 0) {
		QMessageBox::information(win, QObject::tr("Filelink"),
								 QObject::tr("No instances available."));
		return;
	}

	// Build instance name/id lists
	QStringList instNames, instIds;
	for (int i = 0; i < instList->count(); i++) {
		auto inst = instList->at(i);
		if (inst && (preselectedId.isEmpty() || inst->id() == preselectedId)) {
			instNames << inst->name();
			instIds << inst->id();
		}
	}
	if (instIds.isEmpty()) {
		QMessageBox::information(
			win, QObject::tr("Filelink"),
			preselectedId.isEmpty()
				? QObject::tr("No instances available.")
				: QObject::tr("The selected instance is no longer available."));
		return;
	}

	QDialog dlg(win);
	dlg.setWindowTitle(QObject::tr("Create Shortcut"));
	dlg.setMinimumWidth(440);

	auto* mainLayout = new QVBoxLayout(&dlg);

	// Instance selector
	auto* instGroup = new QGroupBox(QObject::tr("Instance"), &dlg);
	auto* instLayout = new QVBoxLayout(instGroup);
	auto* instTree = new QTreeWidget(instGroup);
	instTree->setHeaderHidden(true);
	instTree->setRootIsDecorated(false);
	instTree->setColumnCount(1);
	instTree->setMaximumHeight(120);
	for (int i = 0; i < instNames.size(); i++) {
		auto* item = new QTreeWidgetItem(instTree);
		item->setText(0, instNames[i]);
		item->setData(0, Qt::UserRole, instIds[i]);
		// Instance icon
		QString iconKey = g_ctx->instance_get_icon_key(g_ctx->module_handle,
													   qPrintable(instIds[i]));
		if (!iconKey.isEmpty())
			item->setIcon(0, APPLICATION->icons()->getIcon(iconKey));
	}
	if (instTree->topLevelItemCount() > 0) {
		// Pre-select the requested instance, or fall back to the first
		QTreeWidgetItem* presel = nullptr;
		if (!preselectedId.isEmpty()) {
			for (int i = 0; i < instTree->topLevelItemCount(); i++) {
				auto* it = instTree->topLevelItem(i);
				if (it->data(0, Qt::UserRole).toString() == preselectedId) {
					presel = it;
					break;
				}
			}
		}
		instTree->setCurrentItem(presel ? presel : instTree->topLevelItem(0));
	}
	instLayout->addWidget(instTree);
	mainLayout->addWidget(instGroup);

	// Shortcut details
	auto* detailGroup = new QGroupBox(QObject::tr("Shortcut Details"), &dlg);
	auto* detailLayout = new QVBoxLayout(detailGroup);

	auto* nameRow = new QHBoxLayout();
	nameRow->addWidget(new QLabel(QObject::tr("Name:"), detailGroup));
	auto* nameEdit = new QLineEdit(detailGroup);
	nameRow->addWidget(nameEdit);
	detailLayout->addLayout(nameRow);

	auto* descRow = new QHBoxLayout();
	descRow->addWidget(new QLabel(QObject::tr("Description:"), detailGroup));
	auto* descEdit = new QLineEdit(detailGroup);
	descEdit->setPlaceholderText(QObject::tr("Launch via MeshMC"));
	descRow->addWidget(descEdit);
	detailLayout->addLayout(descRow);

	auto* serverRow = new QHBoxLayout();
	serverRow->addWidget(
		new QLabel(QObject::tr("Auto-join server:"), detailGroup));
	auto* serverEdit = new QLineEdit(detailGroup);
	serverEdit->setPlaceholderText(
		QObject::tr("(optional) e.g. play.example.com:25565"));
	serverRow->addWidget(serverEdit);
	detailLayout->addLayout(serverRow);

	mainLayout->addWidget(detailGroup);

	enum class ShortcutSaveTarget {
		Desktop = 0,
		Other = 1,
	};

	const bool flatpakSandbox = isFlatpakSandbox();
	auto* saveGroup = new QGroupBox(QObject::tr("Save Location"), &dlg);
	auto* saveLayout = new QVBoxLayout(saveGroup);
	auto* saveTargetCombo = new QComboBox(saveGroup);
	if (!flatpakSandbox) {
		saveTargetCombo->addItem(QObject::tr("Desktop"),
								 static_cast<int>(ShortcutSaveTarget::Desktop));
	}
	saveTargetCombo->addItem(QObject::tr("Other"),
							 static_cast<int>(ShortcutSaveTarget::Other));
	if (flatpakSandbox) {
		auto* saveHint =
			new QLabel(QObject::tr("Flatpak builds use 'Other' so the file "
								   "picker can save outside the sandbox."),
					   saveGroup);
		saveHint->setWordWrap(true);
		saveLayout->addWidget(saveHint);
	}
	saveLayout->addWidget(saveTargetCombo);
	mainLayout->addWidget(saveGroup);

	// Icon selector
	auto* iconGroup = new QGroupBox(QObject::tr("Icon"), &dlg);
	auto* iconLayout = new QVBoxLayout(iconGroup);
	auto* iconTree = new QTreeWidget(iconGroup);
	iconTree->setHeaderHidden(true);
	iconTree->setRootIsDecorated(false);
	iconTree->setColumnCount(1);
	iconTree->setIconSize(QSize(32, 32));
	iconTree->setMaximumHeight(180);

	// Populate icon list from APPLICATION->icons()
	QString selectedIconKey;
	auto* iconListModel = APPLICATION->icons().get();
	for (int i = 0; i < iconListModel->rowCount(); i++) {
		QModelIndex idx = iconListModel->index(i, 0);
		QString key = iconListModel->data(idx, Qt::UserRole).toString();
		QString name = iconListModel->data(idx, Qt::DisplayRole).toString();
		QIcon ico = iconListModel->data(idx, Qt::DecorationRole).value<QIcon>();

		auto* item = new QTreeWidgetItem(iconTree);
		item->setText(0, name.isEmpty() ? key : name);
		item->setIcon(0, ico);
		item->setData(0, Qt::UserRole, key);
	}
	iconLayout->addWidget(iconTree);
	mainLayout->addWidget(iconGroup);

	// Auto-fill name when instance selection changes
	auto updateName = [&]() {
		auto* cur = instTree->currentItem();
		if (!cur)
			return;
		nameEdit->setText(cur->text(0));
		descEdit->setText(
			QObject::tr("Launch %1 via MeshMC").arg(cur->text(0)));

		// Pre-select the instance's current icon
		QString id = cur->data(0, Qt::UserRole).toString();
		QString icoKey =
			g_ctx->instance_get_icon_key(g_ctx->module_handle, qPrintable(id));
		if (!icoKey.isEmpty()) {
			for (int i = 0; i < iconTree->topLevelItemCount(); i++) {
				auto* it = iconTree->topLevelItem(i);
				if (it->data(0, Qt::UserRole).toString() == icoKey) {
					iconTree->setCurrentItem(it);
					break;
				}
			}
		}
	};
	QObject::connect(instTree, &QTreeWidget::currentItemChanged,
					 [&](QTreeWidgetItem*, QTreeWidgetItem*) { updateName(); });
	updateName(); // initial fill

	// Buttons
	auto* btnLayout = new QHBoxLayout();
	auto* createBtn = new QPushButton(QObject::tr("Create Shortcut"), &dlg);
	auto* cancelBtn = new QPushButton(QObject::tr("Cancel"), &dlg);
	btnLayout->addStretch();
	btnLayout->addWidget(createBtn);
	btnLayout->addWidget(cancelBtn);
	mainLayout->addLayout(btnLayout);

	QObject::connect(cancelBtn, &QPushButton::clicked, &dlg, &QDialog::reject);
	QObject::connect(createBtn, &QPushButton::clicked, [&]() {
		auto* instItem = instTree->currentItem();
		if (!instItem) {
			QMessageBox::warning(&dlg, QObject::tr("Filelink"),
								 QObject::tr("Please select an instance."));
			return;
		}
		QString name = nameEdit->text().trimmed();
		if (name.isEmpty()) {
			QMessageBox::warning(&dlg, QObject::tr("Filelink"),
								 QObject::tr("Please enter a shortcut name."));
			return;
		}

		QString id = instItem->data(0, Qt::UserRole).toString();
		QString desc = descEdit->text().trimmed();
		if (desc.isEmpty())
			desc = QObject::tr("Launch %1 via MeshMC").arg(name);

		QString iconKey;
		auto* iconItem = iconTree->currentItem();
		if (iconItem)
			iconKey = iconItem->data(0, Qt::UserRole).toString();

		QString server = serverEdit->text().trimmed();
		QString shortcutPath;
		QString defaultFilePath = defaultShortcutFileName(name);
		auto saveTarget = static_cast<ShortcutSaveTarget>(
			saveTargetCombo->currentData().toInt());
		if (saveTarget == ShortcutSaveTarget::Desktop) {
			QString desktop = desktopPath();
			if (desktop.isEmpty()) {
				QMessageBox::warning(
					&dlg, QObject::tr("Filelink"),
					QObject::tr(
						"Desktop location is not available on this system."));
				return;
			}
			shortcutPath = QDir(desktop).filePath(defaultFilePath);
		} else {
			QByteArray title = QObject::tr("Save Shortcut").toUtf8();
			QByteArray defaultName = defaultFilePath.toUtf8();
#ifdef Q_OS_WIN
			constexpr auto filter = "Windows Shortcuts (*.lnk);;All Files (*)";
#elif defined(Q_OS_LINUX)
			constexpr auto filter = "Desktop Entries (*.desktop);;All Files (*)";
#else
			constexpr auto filter = "All Files (*)";
#endif
			const char* selectedPath = g_ctx->ui_file_save_dialog(
				g_ctx->module_handle, title.constData(),
				defaultName.constData(), filter);
			if (!selectedPath)
				return;
			shortcutPath =
				ensureShortcutExtension(QString::fromUtf8(selectedPath));
		}

		QString iconPath = resolveShortcutIconPath(iconKey);

		if (createShortcut(id, shortcutPath, name, desc, iconPath, server)) {
			QMessageBox::information(
				&dlg, QObject::tr("Filelink"),
				QObject::tr("Shortcut '%1' created at '%2'.")
					.arg(name, QDir::toNativeSeparators(shortcutPath)));
			dlg.accept();
		} else {
			QMessageBox::warning(
				&dlg, QObject::tr("Filelink"),
				QObject::tr("Failed to create shortcut '%1'.").arg(name));
		}
	});

	dlg.exec();
}

static int on_context_menu(void* /*mh*/, uint32_t /*hook_id*/, void* payload,
						   void* /*ud*/)
{
	auto* evt = static_cast<MMCOMenuEvent*>(payload);
	if (!evt || !evt->menu_handle)
		return 0;

	if (QString::fromUtf8(evt->context) != "instance")
		return 0;

	g_ctx->ui_add_menu_item(
		g_ctx->module_handle, evt->menu_handle, "Create Shortcut",
		"emblem-symbolic-link",
		[](void* /*ud*/) { showFilelinkDialog(currentSelectedInstanceId()); },
		nullptr);

	return 0;
}

static int on_instance_removed(void* /*mh*/, uint32_t /*hook_id*/,
							   void* payload, void* /*ud*/)
{
	auto* info = static_cast<MMCOInstanceInfo*>(payload);
	if (!info || !info->instance_name)
		return 0;

	QString name = QString::fromUtf8(info->instance_name);
	if (removeShortcut(name) && g_ctx) {
		char buf[256];
		snprintf(buf, sizeof(buf),
				 "Removed desktop shortcut for deleted instance '%s'",
				 info->instance_name);
		MMCO_LOG(g_ctx, buf);
	}
	return 0;
}

extern "C" {

MMCO_EXPORT int mmco_init(MMCOContext* ctx)
{
	g_ctx = ctx;
	MMCO_LOG(ctx, "Filelink plugin initializing...");

#ifdef Q_OS_MAC
	MMCO_ERR(ctx, "Filelink plugin is not supported on macOS.");
	return -1;
#endif

	ctx->hook_register(ctx->module_handle, MMCO_HOOK_UI_CONTEXT_MENU,
					   on_context_menu, nullptr);

	ctx->hook_register(ctx->module_handle, MMCO_HOOK_INSTANCE_REMOVED,
					   on_instance_removed, nullptr);

	ctx->ui_register_instance_action_cb(
		ctx->module_handle, "Create Shortcut",
		"Create a desktop shortcut for this instance", "emblem-symbolic-link",
		[](void* /*ud*/) { showFilelinkDialog(currentSelectedInstanceId()); },
		nullptr);

	MMCO_LOG(ctx, "Filelink plugin initialized.");
	return 0;
}

MMCO_EXPORT void mmco_unload()
{
	if (g_ctx) {
		MMCO_LOG(g_ctx, "Filelink plugin unloading.");
	}
	g_ctx = nullptr;
}

} /* extern "C" */
