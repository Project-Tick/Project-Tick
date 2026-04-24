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

#include "LaunchController.h"
#include "minecraft/auth/AccountList.h"
#include "Application.h"
#include "plugin/PluginManager.h"
#include "plugin/PluginHooks.h"

#include "ui/MainWindow.h"
#include "ui/InstanceWindow.h"
#include "ui/dialogs/CustomMessageBox.h"
#include "ui/dialogs/ProfileSelectDialog.h"
#include "ui/dialogs/ProgressDialog.h"
#include "ui/dialogs/EditAccountDialog.h"
#include "ui/dialogs/ProfileSetupDialog.h"

#include <QLineEdit>
#include <QInputDialog>
#include <QStringList>
#include <QHostInfo>
#include <QList>
#include <QHostAddress>
#include <QPushButton>

#include "BuildConfig.h"
#include "JavaCommon.h"
#include "tasks/Task.h"
#include "minecraft/auth/AccountTask.h"
#include "launch/steps/TextPrint.h"

LaunchController::LaunchController(QObject* parent) : Task(parent) {}

void LaunchController::executeTask()
{
	if (!m_instance) {
		emitFailed(tr("No instance specified!"));
		return;
	}

	JavaCommon::checkJVMArgs(m_instance->settings()->get("JvmArgs").toString(),
							 m_parentWidget);

	login();
}

void LaunchController::decideAccount()
{
	if (m_accountToUse) {
		return;
	}

	// Find an account to use.
	auto accounts = APPLICATION->accounts();
	if (accounts->count() <= 0) {
		// Tell the user they need to log in at least one account in order to
		// play.
		auto reply =
			CustomMessageBox::selectable(
				m_parentWidget, tr("No Accounts"),
				tr("In order to play Minecraft, you must have at least one "
				   "Microsoft "
				   "account logged in."
				   "Would you like to open the account manager to add an "
				   "account now?"),
				QMessageBox::Information, QMessageBox::Yes | QMessageBox::No)
				->exec();

		if (reply == QMessageBox::Yes) {
			// Open the account manager.
			APPLICATION->ShowGlobalSettings(m_parentWidget, "accounts");
		} else {
			// Offer demo mode as an alternative
			QMessageBox demoBox(m_parentWidget);
			demoBox.setWindowTitle(tr("No Account — Play Demo?"));
			demoBox.setIcon(QMessageBox::Question);
			demoBox.setText(
				tr("<b>No Microsoft account is linked.</b><br><br>"
				   "Without a Microsoft account you cannot play the full "
				   "version of Minecraft.<br><br>"
				   "<b>Demo Mode</b> lets you try Minecraft with the "
				   "following limitations:<br>"
				   "&nbsp;&bull;&nbsp;Only a small area of the world is "
				   "accessible<br>"
				   "&nbsp;&bull;&nbsp;Progress is not saved after the demo "
				   "ends<br>"
				   "&nbsp;&bull;&nbsp;Multiplayer is not available<br><br>"
				   "Would you like to launch Minecraft in Demo Mode?"));
			auto yesButton =
				demoBox.addButton(tr("Play Demo"), QMessageBox::YesRole);
			auto noButton =
				demoBox.addButton(tr("Cancel"), QMessageBox::NoRole);
			demoBox.setDefaultButton(noButton);
			demoBox.exec();

			if (demoBox.clickedButton() == yesButton) {
				bool ok = false;
				QString username = QInputDialog::getText(
					m_parentWidget, tr("Demo Mode — Choose Username"),
					tr("Enter a username to use in Demo Mode:"),
					QLineEdit::Normal, tr("User"), &ok);
				if (!ok) {
					// User cancelled username dialog → abort (login() will
					// handle the failure)
					return;
				}
				m_demoMode = true;
				m_demoUsername =
					username.trimmed().isEmpty() ? tr("User") : username.trimmed();
			} else {
				// User declined demo mode → abort (login() will handle the
				// failure)
				return;
			}
		}
	}

	if (m_demoMode) {
		return;
	}

	// If still no accounts after the dialog (e.g. user cancelled demo mode
	// or didn't add an account), bail out — login() will handle the failure.
	if (accounts->count() <= 0) {
		return;
	}

	m_accountToUse = accounts->defaultAccount();
	if (!m_accountToUse) {
		// If no default account is set, ask the user which one to use.
		ProfileSelectDialog selectDialog(
			tr("Which account would you like to use?"),
			ProfileSelectDialog::GlobalDefaultCheckbox, m_parentWidget);

		selectDialog.exec();

		// Launch the instance with the selected account.
		m_accountToUse = selectDialog.selectedAccount();

		// If the user said to use the account as default, do that.
		if (selectDialog.useAsGlobalDefault() && m_accountToUse) {
			accounts->setDefaultAccount(m_accountToUse);
		}
	}
}

void LaunchController::login()
{
	decideAccount();

	// Demo mode: bypass normal account login entirely
	if (m_demoMode) {
		m_session = std::make_shared<AuthSession>();
		m_session->wants_online = m_online;
		m_session->status = AuthSession::PlayableOnline;
		m_session->user_type = "legacy";
		m_session->MakeDemo();
		m_session->player_name = m_demoUsername; // use the chosen username
		launchInstance();
		return;
	}

	// if no account is selected, we bail
	if (!m_accountToUse) {
		emitFailed(tr("No account selected for launch."));
		return;
	}

	// we try empty password first :)
	QString password;
	// we loop until the user succeeds in logging in or gives up
	bool tryagain = true;
	// the failure. the default failure.
	const QString needLoginAgain =
		tr("Your account is currently not logged in. Please enter your "
		   "password to log in again. <br /> <br /> This could be caused by a "
		   "password change.");
	QString failReason = needLoginAgain;

	while (tryagain) {
		m_session = std::make_shared<AuthSession>();
		m_session->wants_online = m_online;
		m_accountToUse->fillSession(m_session);

		switch (m_accountToUse->accountState()) {
			case AccountState::Offline: {
				m_session->wants_online = false;
				// NOTE: fallthrough is intentional
			}
			case AccountState::Online: {
				if (!m_session->wants_online) {
					if (m_accountToUse->isMSA()) {
						// MSA account in offline mode: ask for a player name
						bool ok = false;
						QString usedname = m_session->player_name;
						QString name = QInputDialog::getText(
							m_parentWidget, tr("Player name"),
							tr("Choose your offline mode player name."),
							QLineEdit::Normal, m_session->player_name, &ok);
						if (!ok) {
							tryagain = false;
							break;
						}
						if (name.length()) {
							usedname = name;
						}
						m_session->MakeOffline(usedname);
					} else {
						// Offline account: username is already stored, just
						// launch
						m_session->MakeOffline(m_session->player_name);
					}
					// offline flavored game from here :3
				}
				if (m_accountToUse->ownsMinecraft()) {
					if (!m_accountToUse->hasProfile()) {
						// Now handle setting up a profile name here...
						ProfileSetupDialog dialog(m_accountToUse,
												  m_parentWidget);
						if (dialog.exec() == QDialog::Accepted) {
							tryagain = true;
							continue;
						} else {
							emitFailed(tr("Received undetermined session "
										  "status during login."));
							return;
						}
					}
					// we own Minecraft, there is a profile, it's all ready to
					// go!
					launchInstance();
					return;
				} else {
					// play demo ?
					QMessageBox box(m_parentWidget);
					box.setWindowTitle(tr("Play demo?"));
					box.setText(tr("This account does not own Minecraft.\nYou "
								   "need to purchase the game first to play "
								   "it.\n\nDo you want to play the demo?"));
					box.setIcon(QMessageBox::Warning);
					auto demoButton = box.addButton(
						tr("Play Demo"), QMessageBox::ButtonRole::YesRole);
					auto cancelButton = box.addButton(
						tr("Cancel"), QMessageBox::ButtonRole::NoRole);
					box.setDefaultButton(cancelButton);

					box.exec();
					if (box.clickedButton() == demoButton) {
						// play demo here
						m_session->MakeDemo();
						launchInstance();
					} else {
						emitFailed(tr("Launch cancelled - account does not own "
									  "Minecraft."));
					}
				}
				return;
			}
			case AccountState::Errored:
				// This means some sort of soft error that we can fix with a
				// refresh ... so let's refresh.
			case AccountState::Unchecked: {
				m_accountToUse->refresh();
				// NOTE: fallthrough intentional
			}
			case AccountState::Working: {
				// refresh is in progress, we need to wait for it to finish to
				// proceed.
				ProgressDialog progDialog(m_parentWidget);
				if (m_online) {
					progDialog.setSkipButton(true, tr("Play Offline"));
				}
				auto task = m_accountToUse->currentTask();
				progDialog.execWithTask(task.get());
				continue;
			}
			// FIXME: this is missing - the meaning is that the account is
			// queued for refresh and we should wait for that
			/*
			case AccountState::Queued: {
				return;
			}
			*/
			case AccountState::Expired: {
				auto errorString = tr("The account has expired and needs to be "
									  "logged into manually again.");
				QMessageBox::warning(m_parentWidget,
									 tr("Account refresh failed"), errorString,
									 QMessageBox::StandardButton::Ok,
									 QMessageBox::StandardButton::Ok);
				emitFailed(errorString);
				return;
			}
			case AccountState::Gone: {
				auto errorString =
					tr("The account no longer exists on the servers. It may "
					   "have been migrated, in which case please add the new "
					   "account you migrated this one to.");
				QMessageBox::warning(m_parentWidget, tr("Account gone"),
									 errorString,
									 QMessageBox::StandardButton::Ok,
									 QMessageBox::StandardButton::Ok);
				emitFailed(errorString);
				return;
			}
		}
	}
	emitFailed(tr("Failed to launch."));
}

void LaunchController::launchInstance()
{
	Q_ASSERT_X(m_instance != NULL, "launchInstance", "instance is NULL");
	Q_ASSERT_X(m_session.get() != nullptr, "launchInstance", "session is NULL");

	if (!m_instance->reloadSettings()) {
		QMessageBox::critical(m_parentWidget, tr("Error!"),
							  tr("Couldn't load the instance profile."));
		emitFailed(tr("Couldn't load the instance profile."));
		return;
	}

	m_launcher = m_instance->createLaunchTask(m_session, m_serverToJoin);
	if (!m_launcher) {
		emitFailed(tr("Couldn't instantiate a launcher."));
		return;
	}

	auto console = qobject_cast<InstanceWindow*>(m_parentWidget);
	auto showConsole = m_instance->settings()->get("ShowConsole").toBool();
	if (!console && showConsole) {
		APPLICATION->showInstanceWindow(m_instance);
	}
	connect(m_launcher.get(), &LaunchTask::readyForLaunch, this,
			&LaunchController::readyForLaunch);
	connect(m_launcher.get(), &LaunchTask::succeeded, this,
			&LaunchController::onSucceeded);
	connect(m_launcher.get(), &LaunchTask::failed, this,
			&LaunchController::onFailed);
	connect(m_launcher.get(), &LaunchTask::requestProgress, this,
			&LaunchController::onProgressRequested);

	// Prepend Online and Auth Status
	QString online_mode;
	if (m_session->wants_online) {
		online_mode = "online";

		// Prepend Server Status
		QStringList servers = {"session.minecraft.net",
							   "textures.minecraft.net", "api.mojang.com"};
		QString resolved_servers = "";
		QHostInfo host_info;

		for (QString server : servers) {
			host_info = QHostInfo::fromName(server);
			resolved_servers =
				resolved_servers + server + " resolves to:\n    [";
			if (!host_info.addresses().isEmpty()) {
				for (QHostAddress address : host_info.addresses()) {
					resolved_servers = resolved_servers + address.toString();
					if (!host_info.addresses().endsWith(address)) {
						resolved_servers = resolved_servers + ", ";
					}
				}
			} else {
				resolved_servers = resolved_servers + "N/A";
			}
			resolved_servers = resolved_servers + "]\n\n";
		}
		m_launcher->prependStep(new TextPrint(
			m_launcher.get(), resolved_servers, MessageLevel::MeshMC));
	} else {
		online_mode = "offline";
	}

	m_launcher->prependStep(new TextPrint(
		m_launcher.get(), "Launched instance in " + online_mode + " mode\n",
		MessageLevel::MeshMC));

	// Prepend Version
	m_launcher->prependStep(new TextPrint(
		m_launcher.get(),
		BuildConfig.MESHMC_NAME +
			" version: " + BuildConfig.printableVersionString() + "\n\n",
		MessageLevel::MeshMC));
	// Dispatch pre-launch hook to plugins
	if (APPLICATION->pluginManager()) {
		APPLICATION->pluginManager()->clearPendingLaunchMods();
		QByteArray idUtf8 = m_instance->id().toUtf8();
		QByteArray nameUtf8 = m_instance->name().toUtf8();
		QByteArray pathUtf8 = m_instance->instanceRoot().toUtf8();
		MMCOInstanceInfo hookInfo{};
		hookInfo.instance_id = idUtf8.constData();
		hookInfo.instance_name = nameUtf8.constData();
		hookInfo.instance_path = pathUtf8.constData();
		hookInfo.minecraft_version = nullptr;
		APPLICATION->pluginManager()->dispatchHook(
			MMCO_HOOK_INSTANCE_PRE_LAUNCH, &hookInfo);

		// Apply plugin-requested environment variables via qputenv.
		// CleanEnviroment() reads systemEnvironment() so these will be
		// inherited by the child process.
		auto pendingEnv = APPLICATION->pluginManager()->takePendingLaunchEnv();
		for (auto it = pendingEnv.constBegin(); it != pendingEnv.constEnd();
			 ++it) {
			qputenv(it.key().toUtf8().constData(), it.value().toUtf8());
		}

		// Apply plugin-requested wrapper command (save original for restore)
		QString pendingWrapper =
			APPLICATION->pluginManager()->takePendingLaunchWrapper();
		QString savedWrapper;
		if (!pendingWrapper.isEmpty()) {
			savedWrapper = m_instance->getWrapperCommand().trimmed();
			if (savedWrapper.isEmpty()) {
				m_instance->settings()->set("WrapperCommand", pendingWrapper);
			} else {
				m_instance->settings()->set(
					"WrapperCommand", pendingWrapper + " " + savedWrapper);
			}
		}

		// Restore env vars and wrapper after the launch task finishes
		if (!pendingEnv.isEmpty() || !pendingWrapper.isEmpty()) {
			auto inst = m_instance;
			connect(m_launcher.get(), &Task::finished, this,
					[pendingEnv, pendingWrapper, savedWrapper, inst]() {
						for (auto it = pendingEnv.constBegin();
							 it != pendingEnv.constEnd(); ++it) {
							qunsetenv(it.key().toUtf8().constData());
						}
						if (!pendingWrapper.isEmpty()) {
							inst->settings()->set("WrapperCommand",
												  savedWrapper);
						}
					});
		}
	}

	m_launcher->start();
}

void LaunchController::readyForLaunch()
{
	if (!m_profiler) {
		m_launcher->proceed();
		return;
	}

	QString error;
	if (!m_profiler->check(&error)) {
		m_launcher->abort();
		QMessageBox::critical(m_parentWidget, tr("Error!"),
							  tr("Couldn't start profiler: %1").arg(error));
		emitFailed("Profiler startup failed!");
		return;
	}
	BaseProfiler* profilerInstance =
		m_profiler->createProfiler(m_launcher->instance(), this);

	connect(profilerInstance, &BaseProfiler::readyToLaunch,
			[this](const QString& message) {
				QMessageBox msg;
				msg.setText(tr("The game launch is delayed until you press the "
							   "button. This is the right time to setup the "
							   "profiler, as the "
							   "profiler server is running now.\n\n%1")
								.arg(message));
				msg.setWindowTitle(tr("Waiting."));
				msg.setIcon(QMessageBox::Information);
				msg.addButton(tr("Launch"), QMessageBox::AcceptRole);
				msg.setModal(true);
				msg.exec();
				m_launcher->proceed();
			});
	connect(profilerInstance, &BaseProfiler::abortLaunch,
			[this](const QString& message) {
				QMessageBox msg;
				msg.setText(tr("Couldn't start the profiler: %1").arg(message));
				msg.setWindowTitle(tr("Error"));
				msg.setIcon(QMessageBox::Critical);
				msg.addButton(QMessageBox::Ok);
				msg.setModal(true);
				msg.exec();
				m_launcher->abort();
				emitFailed("Profiler startup failed!");
			});
	profilerInstance->beginProfiling(m_launcher);
}

void LaunchController::onSucceeded()
{
	// Dispatch post-launch hook to plugins
	if (APPLICATION->pluginManager() && m_instance) {
		QByteArray idUtf8 = m_instance->id().toUtf8();
		QByteArray nameUtf8 = m_instance->name().toUtf8();
		QByteArray pathUtf8 = m_instance->instanceRoot().toUtf8();
		MMCOInstanceInfo hookInfo{};
		hookInfo.instance_id = idUtf8.constData();
		hookInfo.instance_name = nameUtf8.constData();
		hookInfo.instance_path = pathUtf8.constData();
		hookInfo.minecraft_version = nullptr;
		APPLICATION->pluginManager()->dispatchHook(
			MMCO_HOOK_INSTANCE_POST_LAUNCH, &hookInfo);
	}

	emitSucceeded();
}

void LaunchController::onFailed(QString reason)
{
	if (m_instance->settings()->get("ShowConsoleOnError").toBool()) {
		APPLICATION->showInstanceWindow(m_instance, "console");
	}
	emitFailed(reason);
}

void LaunchController::onProgressRequested(Task* task)
{
	ProgressDialog progDialog(m_parentWidget);
	progDialog.setSkipButton(true, tr("Abort"));
	m_launcher->proceed();
	progDialog.execWithTask(task);
}

bool LaunchController::abort()
{
	if (!m_launcher) {
		return true;
	}
	if (!m_launcher->canAbort()) {
		return false;
	}
	auto response = CustomMessageBox::selectable(
						m_parentWidget, tr("Kill Minecraft?"),
						tr("This can cause the instance to get corrupted and "
						   "should only be used if Minecraft "
						   "is frozen for some reason"),
						QMessageBox::Question,
						QMessageBox::Yes | QMessageBox::No, QMessageBox::Yes)
						->exec();
	if (response == QMessageBox::Yes) {
		return m_launcher->abort();
	}
	return false;
}
