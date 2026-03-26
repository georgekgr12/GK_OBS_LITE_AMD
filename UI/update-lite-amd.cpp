#ifdef OBS_AMD_LITE

#include "update-lite-amd.hpp"
#include <obs-app.hpp>
#include <qt-wrappers.hpp>
#include <util/platform.h>

#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QFile>
#include <QDir>
#include <QProcess>
#include <QMessageBox>
#include <QDesktopServices>
#include <QUrl>
#include <QStandardPaths>
#include <QDialogButtonBox>
#include <QScrollArea>
#include <QApplication>

#include <json11.hpp>
#include "remote-text.hpp"

using namespace json11;

/* ============================================================
 * GKUpdateThread — Check GitHub releases API for new version
 * ============================================================ */

GKUpdateThread::GKUpdateThread(bool manual, QObject *parent)
	: QThread(parent),
	  manualCheck(manual)
{
}

static bool IsNewerVersion(const QString &latest, const QString &current)
{
	QStringList latestParts = latest.split('.');
	QStringList currentParts = current.split('.');

	for (int i = 0; i < 3; i++) {
		int l = (i < latestParts.size()) ? latestParts[i].toInt() : 0;
		int c = (i < currentParts.size()) ? currentParts[i].toInt() : 0;
		if (l > c)
			return true;
		if (l < c)
			return false;
	}
	return false;
}

void GKUpdateThread::run()
{
	/* Fetch latest release from GitHub API */
	std::string output;
	std::string error;
	long responseCode = 0;
	std::string versionHeader = "User-Agent: GK_OBS_Lite_AMD/";
	versionHeader += GK_OBS_LITE_VERSION;

	std::vector<std::string> extraHeaders;
	bool success = GetRemoteFile(GK_OBS_LITE_RELEASES_API, output, error, &responseCode, nullptr, std::string(),
				     versionHeader.c_str(), extraHeaders, nullptr, 15);

	if (!success || responseCode != 200) {
		if (manualCheck) {
			QString errMsg = QString("Failed to check for updates (HTTP %1)").arg(responseCode);
			if (!error.empty())
				errMsg += QString(": %1").arg(QString::fromStdString(error));
			emit UpdateError(errMsg);
		}
		return;
	}

	/* Parse JSON response */
	std::string jsonError;
	Json json = Json::parse(output, jsonError);
	if (!jsonError.empty()) {
		if (manualCheck)
			emit UpdateError(QString("Failed to parse update response: %1").arg(QString::fromStdString(jsonError)));
		return;
	}

	/* Extract version from tag_name (strip leading 'v' if present) */
	std::string tagName = json["tag_name"].string_value();
	if (tagName.empty()) {
		if (manualCheck)
			emit UpdateError("No releases found.");
		return;
	}

	QString latestVersion = QString::fromStdString(tagName);
	if (latestVersion.startsWith('v') || latestVersion.startsWith('V'))
		latestVersion = latestVersion.mid(1);

	QString currentVersion = GK_OBS_LITE_VERSION;

	if (!IsNewerVersion(latestVersion, currentVersion)) {
		emit NoUpdate(manualCheck);
		return;
	}

	/* Find download URL — prefer .exe installer, fallback to first asset */
	QString downloadUrl;
	const Json::array &assets = json["assets"].array_items();
	for (const Json &asset : assets) {
		std::string name = asset["name"].string_value();
		std::string url = asset["browser_download_url"].string_value();
		if (name.find(".exe") != std::string::npos || name.find("Setup") != std::string::npos) {
			downloadUrl = QString::fromStdString(url);
			break;
		}
	}
	if (downloadUrl.isEmpty() && !assets.empty()) {
		downloadUrl = QString::fromStdString(assets[0]["browser_download_url"].string_value());
	}

	QString releaseNotes = QString::fromStdString(json["body"].string_value());

	emit UpdateAvailable(latestVersion, downloadUrl, releaseNotes);
}

/* ============================================================
 * GKUpdateDialog — Download and apply update
 * ============================================================ */

GKUpdateDialog::GKUpdateDialog(QWidget *parent, const QString &ver, const QString &url, const QString &notes)
	: QDialog(parent),
	  version(ver),
	  downloadUrl(url)
{
	setWindowTitle("Update Available — GK_OBS_Lite_AMD");
	setMinimumWidth(450);
	setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);

	auto *layout = new QVBoxLayout(this);

	auto *titleLabel = new QLabel(QString("<h2>GK_OBS_Lite_AMD %1 is available!</h2>"
					      "<p>You are currently running version %2.</p>")
					      .arg(ver, GK_OBS_LITE_VERSION));
	titleLabel->setWordWrap(true);
	layout->addWidget(titleLabel);

	if (!notes.isEmpty()) {
		auto *notesLabel = new QLabel("<b>Release Notes:</b>");
		layout->addWidget(notesLabel);

		auto *notesBrowser = new QTextBrowser();
		notesBrowser->setMarkdown(notes);
		notesBrowser->setMaximumHeight(200);
		notesBrowser->setOpenExternalLinks(true);
		layout->addWidget(notesBrowser);
	}

	statusLabel = new QLabel("Ready to download.");
	layout->addWidget(statusLabel);

	progressBar = new QProgressBar();
	progressBar->setRange(0, 100);
	progressBar->setValue(0);
	progressBar->setVisible(false);
	layout->addWidget(progressBar);

	auto *buttonLayout = new QHBoxLayout();
	updateButton = new QPushButton("Download && Install");
	cancelButton = new QPushButton("Later");
	buttonLayout->addStretch();
	buttonLayout->addWidget(updateButton);
	buttonLayout->addWidget(cancelButton);
	layout->addLayout(buttonLayout);

	connect(updateButton, &QPushButton::clicked, this, &GKUpdateDialog::StartDownload);
	connect(cancelButton, &QPushButton::clicked, this, &QDialog::reject);

	if (downloadUrl.isEmpty()) {
		updateButton->setEnabled(false);
		statusLabel->setText("No installer found in this release. Check GitHub manually.");
	}
}

void GKUpdateDialog::StartDownload()
{
	updateButton->setEnabled(false);
	cancelButton->setText("Cancel");
	progressBar->setVisible(true);
	statusLabel->setText("Downloading...");

	/* Download to temp directory */
	QString tempDir = QStandardPaths::writableLocation(QStandardPaths::TempLocation);
	QString filename = QUrl(downloadUrl).fileName();
	if (filename.isEmpty())
		filename = "GK_OBS_Lite_AMD_Setup.exe";
	installerPath = tempDir + "/" + filename;

	auto *manager = new QNetworkAccessManager(this);
	QUrl url(downloadUrl);
	QNetworkRequest request(url);
	request.setHeader(QNetworkRequest::UserAgentHeader, QByteArray("GK_OBS_Lite_AMD/" GK_OBS_LITE_VERSION));
	/* Follow redirects (GitHub uses them for release assets) */
	request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
			     (int)QNetworkRequest::NoLessSafeRedirectPolicy);

	QNetworkReply *reply = manager->get(request);
	connect(reply, &QNetworkReply::downloadProgress, this, &GKUpdateDialog::OnDownloadProgress);
	connect(reply, &QNetworkReply::finished, this, [this, reply]() {
		if (reply->error() != QNetworkReply::NoError) {
			OnDownloadError(reply->errorString());
		} else {
			/* Save to file */
			QFile file(installerPath);
			if (file.open(QIODevice::WriteOnly)) {
				file.write(reply->readAll());
				file.close();
				OnDownloadFinished();
			} else {
				OnDownloadError("Failed to write installer to disk.");
			}
		}
		reply->deleteLater();
	});
}

void GKUpdateDialog::OnDownloadProgress(qint64 received, qint64 total)
{
	if (total > 0) {
		int percent = (int)((received * 100) / total);
		progressBar->setValue(percent);
		statusLabel->setText(QString("Downloading: %1 MB / %2 MB")
					    .arg(received / 1048576.0, 0, 'f', 1)
					    .arg(total / 1048576.0, 0, 'f', 1));
	}
}

void GKUpdateDialog::OnDownloadFinished()
{
	progressBar->setValue(100);
	statusLabel->setText("Download complete. Installing...");

	/* Launch the installer and exit OBS */
	QProcess::startDetached(installerPath, QStringList());

	/* Quit the app so the installer can replace files */
	QApplication::quit();
}

void GKUpdateDialog::OnDownloadError(const QString &error)
{
	statusLabel->setText("Download failed: " + error);
	updateButton->setEnabled(true);
	updateButton->setText("Retry");
	progressBar->setVisible(false);
}

/* ============================================================
 * GKAboutDialog — About + MIT EULA
 * ============================================================ */

GKAboutDialog::GKAboutDialog(QWidget *parent) : QDialog(parent)
{
	setWindowTitle("About GK_OBS_Lite_AMD");
	setFixedSize(480, 420);
	setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);

	auto *layout = new QVBoxLayout(this);

	auto *titleLabel = new QLabel(
		"<div style='text-align:center;'>"
		"<h1 style='color:#e63946;'>GK_OBS_Lite_AMD</h1>"
		"<p style='font-size:14px;'>Version " GK_OBS_LITE_VERSION " (64-bit)</p>"
		"<p>Lightweight, AMD-optimized OBS for streaming &amp; recording.</p>"
		"<p><b>Developer:</b> George Karagioules</p>"
		"<p><b>Based on:</b> OBS Studio 31.0.3 by the OBS Project</p>"
		"<hr>"
		"</div>");
	titleLabel->setWordWrap(true);
	titleLabel->setAlignment(Qt::AlignCenter);
	layout->addWidget(titleLabel);

	auto *eulaLabel = new QLabel("<b>MIT License</b>");
	layout->addWidget(eulaLabel);

	auto *eulaBrowser = new QTextBrowser();
	eulaBrowser->setPlainText(
		"MIT License\n\n"
		"Copyright (c) 2026 George Karagioules\n\n"
		"Permission is hereby granted, free of charge, to any person obtaining a copy "
		"of this software and associated documentation files (the \"Software\"), to deal "
		"in the Software without restriction, including without limitation the rights "
		"to use, copy, modify, merge, publish, distribute, sublicense, and/or sell "
		"copies of the Software, and to permit persons to whom the Software is "
		"furnished to do so, subject to the following conditions:\n\n"
		"The above copyright notice and this permission notice shall be included in all "
		"copies or substantial portions of the Software.\n\n"
		"THE SOFTWARE IS PROVIDED \"AS IS\", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR "
		"IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, "
		"FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE "
		"AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER "
		"LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, "
		"OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE "
		"SOFTWARE.\n\n"
		"---\n\n"
		"OBS Studio is licensed under the GNU General Public License v2.0.\n"
		"AMD AMF SDK is Copyright (c) Advanced Micro Devices, Inc.");
	eulaBrowser->setReadOnly(true);
	layout->addWidget(eulaBrowser);

	auto *closeButton = new QPushButton("Close");
	closeButton->setDefault(true);
	auto *btnLayout = new QHBoxLayout();
	btnLayout->addStretch();
	btnLayout->addWidget(closeButton);
	layout->addLayout(btnLayout);

	connect(closeButton, &QPushButton::clicked, this, &QDialog::accept);
}

#endif /* OBS_AMD_LITE */
