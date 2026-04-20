/* SPDX-FileCopyrightText: 2026 Project Tick
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * FilelinkPage — instance page for managing cross-instance file links.
 */

#pragma once

#include "ui_FilelinkPage.h"
#include "FilelinkManager.h"

class FilelinkPage : public QWidget, public BasePage
{
	Q_OBJECT

  public:
	explicit FilelinkPage(InstancePtr inst, FilelinkManager* mgr);

	QString displayName() const override
	{
		return tr("Filelink");
	}
	QIcon icon() const override;
	QString id() const override
	{
		return "filelink";
	}
	QString helpPage() const override
	{
		return "Filelink";
	}
	bool apply() override
	{
		return true;
	}

  private slots:
	void on_linkButton_clicked();
	void on_unlinkButton_clicked();
	void on_verifyButton_clicked();

  private:
	void refreshTable();

	Ui::FilelinkPage ui;
	InstancePtr m_instance;
	FilelinkManager* m_manager;
};
