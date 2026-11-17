// SPDX-FileCopyrightText: 2018 Contributors to Chatterino <https://chatterino.com>
//
// SPDX-License-Identifier: MIT

#include "singletons/Updates.hpp"

#include "common/Literals.hpp"
#include "common/Modes.hpp"
#include "common/network/NetworkRequest.hpp"
#include "common/network/NetworkResult.hpp"
#include "common/QLogging.hpp"
#include "common/Version.hpp"
#include "singletons/Paths.hpp"
#include "singletons/Settings.hpp"
#include "util/CombinePath.hpp"
#include "util/PostToThread.hpp"

#include <QApplication>
#include <QDesktopServices>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QMessageBox>
#include <QProcess>
#include <QRegularExpression>
#include <QStringBuilder>
#include <QtConcurrent>
#include <semver/semver.hpp>

namespace {

using namespace chatterino;
using namespace literals;

QString currentBranch()
{
    return getSettings()->betaUpdates ? "beta" : "stable";
}

#if defined(Q_OS_WIN)
const QString CHATTERINO_OS = u"win"_s;
#elif defined(Q_OS_MACOS)
const QString CHATTERINO_OS = u"macos"_s;
#elif defined(Q_OS_LINUX)
const QString CHATTERINO_OS = u"linux"_s;
#elif defined(Q_OS_FREEBSD)
const QString CHATTERINO_OS = u"freebsd"_s;
#else
const QString CHATTERINO_OS = u"unknown"_s;
#endif

}  // namespace

namespace chatterino {

// Arabic build: versioned and released separately on GitHub. Bump this and
// tag the release "v<AR_VERSION>" together.
const QString AR_VERSION = u"1.3.0"_s;
const QString AR_RELEASES_API =
    u"https://api.github.com/repos/Ryanathlawi/chatterino-ar/releases/latest"_s;

Updates::Updates(const Paths &paths_, Settings &settings)
    : paths(paths_)
    , currentVersion_(AR_VERSION)
    , updateGuideLink_("https://github.com/Ryanathlawi/chatterino-ar/releases/latest")
{
    qCDebug(chatterinoUpdate) << "init UpdateManager";

    settings.betaUpdates.connect(
        [this] {
            this->checkForUpdates();
        },
        this->managedConnections, false);
}

/// Checks if the online version is newer or older than the current version.
bool Updates::isDowngradeOf(const QString &online, const QString &current)
{
    semver::version onlineVersion;
    if (!onlineVersion.from_string_noexcept(online.toStdString()))
    {
        qCWarning(chatterinoUpdate) << "Unable to parse online version"
                                    << online << "into a proper semver string";
        return false;
    }

    semver::version currentVersion;
    if (!currentVersion.from_string_noexcept(current.toStdString()))
    {
        qCWarning(chatterinoUpdate) << "Unable to parse current version"
                                    << current << "into a proper semver string";
        return false;
    }

    return onlineVersion < currentVersion;
}

void Updates::deleteOldFiles()
{
    std::ignore = QtConcurrent::run([dir{this->paths.miscDirectory}] {
        {
            auto path = combinePath(dir, "Update.exe");
            if (QFile::exists(path))
            {
                QFile::remove(path);
            }
        }
        {
            auto path = combinePath(dir, "update.zip");
            if (QFile::exists(path))
            {
                QFile::remove(path);
            }
        }
    });
}

const QString &Updates::getCurrentVersion() const
{
    return this->currentVersion_;
}

const QString &Updates::getOnlineVersion() const
{
    return this->onlineVersion_;
}

void Updates::installUpdates()
{
    if (this->status_ != UpdateAvailable)
    {
        assert(false);
        return;
    }

#ifdef Q_OS_WIN
    // Arabic build: download the release zip, then a small PowerShell script
    // swaps the files in once we have exited and starts the new version.
    if (!this->updatePortable_.isEmpty())
    {
        static auto *nam = new QNetworkAccessManager(qApp);
        this->downloadPercent_ = 0;
        this->setStatus_(Downloading);
        auto *reply = nam->get(QNetworkRequest(QUrl(this->updatePortable_)));
        QObject::connect(
            reply, &QNetworkReply::downloadProgress, qApp,
            [this](qint64 got, qint64 total) {
                int p = total > 0 ? int(got * 100 / total) : 0;
                if (p != this->downloadPercent_)
                {
                    this->downloadPercent_ = p;
                    this->statusUpdated.invoke(Downloading);
                }
            });
        QObject::connect(reply, &QNetworkReply::finished, qApp, [this, reply] {
            reply->deleteLater();
            if (reply->error() != QNetworkReply::NoError)
            {
                this->setStatus_(DownloadFailed);
                return;
            }
            const QString zip = QDir::tempPath() + "/ChatterinoAR-update.zip";
            const QString script = QDir::tempPath() + "/ChatterinoAR-update.ps1";
            QFile zf(zip);
            QFile sf(script);
            if (!zf.open(QIODevice::WriteOnly) ||
                zf.write(reply->readAll()) < 0 ||
                !sf.open(QIODevice::WriteOnly | QIODevice::Text) ||
                sf.write(
        "param($zip, $dir, $procId, $exe)\n"
        "try { Wait-Process -Id $procId -Timeout 30 -ErrorAction SilentlyContinue } catch {}\n"
        "$tmp = Join-Path $env:TEMP 'ChatterinoAR-update'\n"
        "Remove-Item $tmp -Recurse -Force -ErrorAction SilentlyContinue\n"
        "Expand-Archive -LiteralPath $zip -DestinationPath $tmp -Force\n"
        "$src = Join-Path $tmp 'Chatterino-AR'\n"
        "if (-not (Test-Path $src)) { $src = $tmp }\n"
        "for ($i = 0; $i -lt 10; $i++) {\n"
        "  try { Copy-Item -Path (Join-Path $src '*') -Destination $dir -Recurse -Force -ErrorAction Stop; break }\n"
        "  catch { Start-Sleep -Milliseconds 500 }\n"
        "}\n"
        "Remove-Item $tmp, $zip -Recurse -Force -ErrorAction SilentlyContinue\n"
        "Start-Process -FilePath $exe\n"
        "Remove-Item -LiteralPath $PSCommandPath -Force -ErrorAction SilentlyContinue\n") < 0)
            {
                this->setStatus_(WriteFileFailed);
                return;
            }
            zf.close();
            sf.close();
            bool started = QProcess::startDetached(
                "powershell.exe",
                {"-NoProfile", "-ExecutionPolicy", "Bypass", "-WindowStyle",
                 "Hidden", "-File", QDir::toNativeSeparators(script),
                 QDir::toNativeSeparators(zip),
                 QDir::toNativeSeparators(QCoreApplication::applicationDirPath()),
                 QString::number(QCoreApplication::applicationPid()),
                 QDir::toNativeSeparators(
                     QCoreApplication::applicationFilePath())});
            if (!started)
            {
                this->setStatus_(RunUpdaterFailed);
                return;
            }
            QApplication::quit();
        });
        return;
    }
#endif

    // no zip found: open the release page instead
    QDesktopServices::openUrl(this->updateGuideLink_);
    if (true)
    {
        return;
    }

    if (Version::instance().isNightly())
    {
        // Since Nightly builds can be installed in many different ways, we ask the user to download the update manually.
        QDesktopServices::openUrl(QUrl("https://chatterino.com/#downloads"));
        return;
    }

#ifdef Q_OS_MACOS
    QMessageBox *box = new QMessageBox(
        QMessageBox::Information, "Chatterino Update",
        "A link will open in your browser. Download and install to update.");
    box->setAttribute(Qt::WA_DeleteOnClose);
    box->open();
    QDesktopServices::openUrl(this->updateExe_);
#elif defined Q_OS_LINUX
    QMessageBox *box =
        new QMessageBox(QMessageBox::Information, "Chatterino Update",
                        "Automatic updates are currently not available on "
                        "Linux. Please redownload the app to update.");
    box->setAttribute(Qt::WA_DeleteOnClose);
    box->open();
    QDesktopServices::openUrl(this->updateGuideLink_);
#elif defined Q_OS_WIN
    if (Modes::instance().isPortable)
    {
        QMessageBox *box =
            new QMessageBox(QMessageBox::Information, "Chatterino Update",
                            "Chatterino is downloading the update "
                            "in the background and will run the "
                            "updater once it is finished.");
        box->setAttribute(Qt::WA_DeleteOnClose);
        box->show();

        NetworkRequest(this->updatePortable_)
            .timeout(600000)
            .onError([this](NetworkResult) {
                this->setStatus_(DownloadFailed);

                postToThread([] {
                    QMessageBox *box = new QMessageBox(
                        QMessageBox::Information, "Chatterino Update",
                        "Failed while trying to download the update.");
                    box->setAttribute(Qt::WA_DeleteOnClose);
                    box->show();
                    box->raise();
                });
            })
            .onSuccess([this](auto result) {
                if (result.status() != 200)
                {
                    auto *box = new QMessageBox(
                        QMessageBox::Information, "Chatterino Update",
                        QStringLiteral("The update couldn't be downloaded "
                                       "(Error: %1).")
                            .arg(result.formatError()));
                    box->setAttribute(Qt::WA_DeleteOnClose);
                    box->exec();
                    return;
                }

                QByteArray object = result.getData();
                auto filename =
                    combinePath(this->paths.miscDirectory, "update.zip");

                QFile file(filename);
                if (!file.open(QIODevice::Truncate | QIODevice::WriteOnly))
                {
                    qCWarning(chatterinoUpdate)
                        << "Failed to save update.zip" << file.errorString();
                    this->setStatus_(WriteFileFailed);
                    return;
                }

                if (file.write(object) == -1)
                {
                    this->setStatus_(WriteFileFailed);
                    return;
                }
                file.flush();
                file.close();

                auto updaterPath = Updates::portableUpdaterPath();
                if (!QFile::exists(updaterPath))
                {
                    this->setStatus_(MissingPortableUpdater);
                    return;
                }
                bool ok =
                    QProcess::startDetached(updaterPath, {filename, "restart"});
                if (!ok)
                {
                    this->setStatus_(RunUpdaterFailed);
                    return;
                }

                QApplication::exit(0);
            })
            .execute();
        this->setStatus_(Downloading);
    }
    else
    {
        QMessageBox *box =
            new QMessageBox(QMessageBox::Information, "Chatterino Update",
                            "Chatterino is downloading the update "
                            "in the background and will run the "
                            "updater once it is finished.");
        box->setAttribute(Qt::WA_DeleteOnClose);
        box->show();

        NetworkRequest(this->updateExe_)
            .timeout(600000)
            .onError([this](NetworkResult) {
                this->setStatus_(DownloadFailed);

                QMessageBox *box = new QMessageBox(
                    QMessageBox::Information, "Chatterino Update",
                    "Failed to download the update. \n\nTry manually "
                    "downloading the update.");
                box->setAttribute(Qt::WA_DeleteOnClose);
                box->exec();
            })
            .onSuccess([this](auto result) {
                if (result.status() != 200)
                {
                    auto *box = new QMessageBox(
                        QMessageBox::Information, "Chatterino Update",
                        QStringLiteral("The update couldn't be downloaded "
                                       "(Error: %1).")
                            .arg(result.formatError()));
                    box->setAttribute(Qt::WA_DeleteOnClose);
                    box->exec();
                    return;
                }

                QByteArray object = result.getData();
                auto filePath =
                    combinePath(this->paths.miscDirectory, "Update.exe");

                QFile file(filePath);
                // write() will fail if we couldn't open
                std::ignore =
                    file.open(QIODevice::Truncate | QIODevice::WriteOnly);

                if (file.write(object) == -1)
                {
                    this->setStatus_(WriteFileFailed);
                    QMessageBox *box = new QMessageBox(
                        QMessageBox::Information, "Chatterino Update",
                        "Failed to save the update file. This could be due to "
                        "window settings or antivirus software.\n\nTry "
                        "manually "
                        "downloading the update.");
                    box->setAttribute(Qt::WA_DeleteOnClose);
                    box->exec();

                    QDesktopServices::openUrl(this->updateExe_);
                    return;
                }
                file.flush();
                file.close();

                if (QProcess::startDetached(filePath, {}))
                {
                    QApplication::exit(0);
                }
                else
                {
                    QMessageBox *box = new QMessageBox(
                        QMessageBox::Information, "Chatterino Update",
                        "Failed to execute update binary. This could be due to "
                        "window "
                        "settings or antivirus software.\n\nTry manually "
                        "downloading "
                        "the update.");
                    box->setAttribute(Qt::WA_DeleteOnClose);
                    box->exec();

                    QDesktopServices::openUrl(this->updateExe_);
                }
            })
            .execute();
        this->setStatus_(Downloading);
    }
#endif
}

void Updates::checkForUpdates()
{
#ifndef CHATTERINO_DISABLE_UPDATER
    auto version = Version::instance();

    if (!version.isSupportedOS())
    {
        qCDebug(chatterinoUpdate)
            << "Update checking disabled because OS doesn't appear to be one "
               "of Windows, GNU/Linux or macOS.";
        return;
    }

    // Disable updates on Flatpak
    if (version.isFlatpak())
    {
        return;
    }

    NetworkRequest(AR_RELEASES_API)
        .timeout(60000)
        .onSuccess([this](auto result) {
            const auto object = result.parseJson();
            auto tag = object["tag_name"].toString();
            if (tag.isEmpty())
            {
                this->setStatus_(SearchFailed);
                qCDebug(chatterinoUpdate)
                    << "error checking version - missing 'tag_name'" << object;
                return;
            }
            this->onlineVersion_ = tag.startsWith('v') ? tag.mid(1) : tag;
            auto page = object["html_url"].toString();
            if (!page.isEmpty())
            {
                this->updateGuideLink_ = page;
            }
            for (const auto &asset : object["assets"].toArray())
            {
                auto a = asset.toObject();
                if (a["name"].toString().endsWith(".zip"))
                {
                    this->updatePortable_ =
                        a["browser_download_url"].toString();
                    break;
                }
            }

            // only offer real upgrades
            bool newer = this->currentVersion_ != this->onlineVersion_ &&
                         Updates::isDowngradeOf(this->currentVersion_,
                                                this->onlineVersion_);
            this->isDowngrade_ = false;
            this->setStatus_(newer ? UpdateAvailable : NoUpdateAvailable);
        })
        .execute();
    this->setStatus_(Searching);
#endif
}

Updates::Status Updates::getStatus() const
{
    return this->status_;
}

QString Updates::portableUpdaterPath()
{
    return combinePath(QCoreApplication::applicationDirPath(),
                       "updater.1/ChatterinoUpdater.exe");
}

bool Updates::shouldShowUpdateButton() const
{
    switch (this->getStatus())
    {
        case UpdateAvailable:
        case SearchFailed:
        case Downloading:
        case DownloadFailed:
        case WriteFileFailed:
            return true;

        default:
            return false;
    }
}

bool Updates::isError() const
{
    switch (this->getStatus())
    {
        case SearchFailed:
        case DownloadFailed:
        case WriteFileFailed:
        case MissingPortableUpdater:
        case RunUpdaterFailed:
            return true;

        default:
            return false;
    }
}

bool Updates::isDowngrade() const
{
    return this->isDowngrade_;
}

QString Updates::buildUpdateAvailableText() const
{
    return QString("فيه تحديث جديد (%1)، وأنت على %2."
                   "\n\nاضغط «تحديث» وينزل لحاله، وبعدها البرنامج يسكّر "
                   "ويفتح نفسه على النسخة الجديدة. إعداداتك ما تضيع.")
        .arg(this->getOnlineVersion(), this->getCurrentVersion());
}

int Updates::downloadPercent() const
{
    return this->downloadPercent_;
}

void Updates::setStatus_(Status status)
{
    if (this->status_ != status)
    {
        this->status_ = status;
        postToThread([this, status] {
            this->statusUpdated.invoke(status);
        });
    }
}

}  // namespace chatterino
