/* SPDX-FileCopyrightText: 2026 Project Tick
 * SPDX-FileContributor: Project Tick
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 *   MeshMC - A Custom Launcher for Minecraft
 *   Copyright (C) 2026 Project Tick
 *
 *   This program is free software: you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, either version 3 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#include "CrashReportDialog.h"

#include <QClipboard>
#include <QApplication>
#include <QDesktopServices>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QPushButton>
#include <QUrl>
#include <QVBoxLayout>

#ifdef MESHMC_HAS_QRENCODE
#include <qrencode.h>
#endif

CrashReportDialog::CrashReportDialog(const QString& pasteLink,
									  const QString& logContent,
									  QWidget* parent)
	: QDialog(parent)
{
	setWindowTitle(tr("MeshMC Crash Report"));
	setMinimumSize(550, 450);

	auto* mainLayout = new QVBoxLayout(this);

	// Error message
	m_messageLabel = new QLabel(this);
	m_messageLabel->setText(
		tr("<h2 style='color: #cc0000;'>MeshMC has failed</h2>"
		   "<p>An unexpected error occurred and MeshMC had to close.</p>"
		   "<p>Your logs have been uploaded to paste.ee for troubleshooting.</p>"));
	m_messageLabel->setAlignment(Qt::AlignCenter);
	m_messageLabel->setWordWrap(true);
	mainLayout->addWidget(m_messageLabel);

	// QR codes area
	auto* qrGroupBox = new QGroupBox(tr("QR Codes"), this);
	auto* qrLayout = new QHBoxLayout(qrGroupBox);

	// QR for full log text (truncated to fit QR capacity)
	// QR version 40 can hold ~4296 bytes; we keep the last 2000 chars
	// so the most recent (and most relevant) log lines are included.
	QString truncatedLog = logContent;
	if (truncatedLog.size() > 2000) {
		truncatedLog = tr("[...truncated...]\n") +
			logContent.right(2000);
	}

	auto* qrLogGroup = new QVBoxLayout();
	m_qrLogLabel = new QLabel(this);
	m_qrLogLabel->setAlignment(Qt::AlignCenter);
	m_qrLogLabel->setMinimumSize(200, 200);
	auto logQR = generateQRCode(truncatedLog, 200);
	if (!logQR.isNull()) {
		m_qrLogLabel->setPixmap(logQR);
	} else {
		m_qrLogLabel->setText(tr("[QR Code - requires libqrencode]"));
	}
	auto* logQRTitle = new QLabel(tr("Full Log"), this);
	logQRTitle->setAlignment(Qt::AlignCenter);
	qrLogGroup->addWidget(m_qrLogLabel);
	qrLogGroup->addWidget(logQRTitle);
	qrLayout->addLayout(qrLogGroup);

	// QR for paste.ee link
	auto* qrPasteGroup = new QVBoxLayout();
	m_qrPasteLabel = new QLabel(this);
	m_qrPasteLabel->setAlignment(Qt::AlignCenter);
	m_qrPasteLabel->setMinimumSize(200, 200);
	auto pasteQR = generateQRCode(pasteLink, 200);
	if (!pasteQR.isNull()) {
		m_qrPasteLabel->setPixmap(pasteQR);
	} else {
		m_qrPasteLabel->setText(tr("[QR Code - requires libqrencode]"));
	}
	auto* pasteQRTitle = new QLabel(tr("Paste.ee Log"), this);
	pasteQRTitle->setAlignment(Qt::AlignCenter);
	qrPasteGroup->addWidget(m_qrPasteLabel);
	qrPasteGroup->addWidget(pasteQRTitle);
	qrLayout->addLayout(qrPasteGroup);

	mainLayout->addWidget(qrGroupBox);

	// Paste.ee link
	m_pasteLinkLabel = new QLabel(this);
	m_pasteLinkLabel->setText(
		tr("Paste.ee link: <a href=\"%1\">%1</a>").arg(pasteLink));
	m_pasteLinkLabel->setOpenExternalLinks(true);
	m_pasteLinkLabel->setTextInteractionFlags(Qt::TextBrowserInteraction);
	m_pasteLinkLabel->setAlignment(Qt::AlignCenter);
	m_pasteLinkLabel->setWordWrap(true);
	mainLayout->addWidget(m_pasteLinkLabel);

	// Buttons
	auto* buttonLayout = new QHBoxLayout();

	auto* copyLinkBtn = new QPushButton(tr("Copy Link"), this);
	connect(copyLinkBtn, &QPushButton::clicked, this, [pasteLink]() {
		QApplication::clipboard()->setText(pasteLink);
	});
	buttonLayout->addWidget(copyLinkBtn);

	auto* openBtn = new QPushButton(tr("Open in Browser"), this);
	connect(openBtn, &QPushButton::clicked, this, [pasteLink]() {
		QDesktopServices::openUrl(QUrl(pasteLink));
	});
	buttonLayout->addWidget(openBtn);

	auto* closeBtn = new QPushButton(tr("Close"), this);
	connect(closeBtn, &QPushButton::clicked, this, &QDialog::accept);
	buttonLayout->addWidget(closeBtn);

	mainLayout->addLayout(buttonLayout);
}

QPixmap CrashReportDialog::generateQRCode(const QString& text, int size)
{
#ifdef MESHMC_HAS_QRENCODE
	QRcode* qr = QRcode_encodeString(text.toUtf8().constData(), 0,
									  QR_ECLEVEL_M, QR_MODE_8, 1);
	if (!qr) {
		return QPixmap();
	}

	int qrWidth = qr->width;
	int scale = size / qrWidth;
	if (scale < 1)
		scale = 1;

	QImage image(qrWidth * scale, qrWidth * scale, QImage::Format_RGB32);
	image.fill(Qt::white);

	for (int y = 0; y < qrWidth; y++) {
		for (int x = 0; x < qrWidth; x++) {
			if (qr->data[y * qrWidth + x] & 1) {
				for (int sy = 0; sy < scale; sy++) {
					for (int sx = 0; sx < scale; sx++) {
						image.setPixel(x * scale + sx, y * scale + sy,
									   qRgb(0, 0, 0));
					}
				}
			}
		}
	}

	QRcode_free(qr);
	return QPixmap::fromImage(
		image.scaled(size, size, Qt::KeepAspectRatio, Qt::FastTransformation));
#else
	Q_UNUSED(text)
	Q_UNUSED(size)
	return QPixmap();
#endif
}
