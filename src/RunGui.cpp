// SPDX-FileCopyrightText: 2018 Contributors to Chatterino <https://chatterino.com>
//
// SPDX-License-Identifier: MIT

#include "RunGui.hpp"

#include "Application.hpp"
#include "common/Args.hpp"
#include "common/Modes.hpp"
#include "common/network/NetworkManager.hpp"
#include "common/QLogging.hpp"
#include "singletons/CrashHandler.hpp"
#include "singletons/Paths.hpp"
#include "singletons/Resources.hpp"
#include "singletons/Settings.hpp"
#include "singletons/Updates.hpp"
#include "util/CombinePath.hpp"
#include "util/SelfCheck.hpp"
#include "util/UnixSignalHandler.hpp"
#include "widgets/dialogs/LastRunCrashDialog.hpp"

#include "providers/twitch/ViewerPresence.hpp"
#include "singletons/WindowManager.hpp"
#include "widgets/Window.hpp"

#include <QApplication>
#include <QElapsedTimer>
#include <QGuiApplication>
#include <QPainter>
#include <QScreen>
#include <QTimer>
#include <QWidget>
#include <algorithm>
#include <cmath>
#include <QDir>
#include <QFile>
#include <QFontDatabase>
#include <QPalette>
#include <QStyleFactory>
#include <Qt>
#include <QtConcurrent>

#include <csignal>
#include <cstdlib>
#include <tuple>

#ifdef USEWINSDK
#    include "util/WindowsHelper.hpp"
#endif

#ifdef C_USE_BREAKPAD
#    include <QBreakpadHandler.h>
#endif

#ifdef Q_OS_MAC
#    include "corefoundation/CFBundle.h"
#endif

// Forward declaration (Qt doesn't declare this in headers)
// NOLINTNEXTLINE(readability-identifier-naming)
extern void qt_set_sequence_auto_mnemonic(bool b);

namespace chatterino {
namespace {
void installCustomPalette()
{
    // borrowed from
    // https://stackoverflow.com/questions/15035767/is-the-qt-5-dark-fusion-theme-available-for-windows
    auto dark = QApplication::palette();

    dark.setColor(QPalette::Window, QColor(22, 22, 22));
    dark.setColor(QPalette::WindowText, Qt::white);
    dark.setColor(QPalette::Text, Qt::white);
    dark.setColor(QPalette::Base, QColor("#333"));
    dark.setColor(QPalette::AlternateBase, QColor("#444"));
    dark.setColor(QPalette::ToolTipBase, Qt::white);
    dark.setColor(QPalette::ToolTipText, Qt::black);
    dark.setColor(QPalette::Dark, QColor(35, 35, 35));
    dark.setColor(QPalette::Shadow, QColor(20, 20, 20));
    dark.setColor(QPalette::Button, QColor(70, 70, 70));
    dark.setColor(QPalette::ButtonText, Qt::white);
    dark.setColor(QPalette::BrightText, Qt::red);
    dark.setColor(QPalette::Link, QColor(42, 130, 218));
    dark.setColor(QPalette::Highlight, QColor(42, 130, 218));
    dark.setColor(QPalette::HighlightedText, Qt::white);
    dark.setColor(QPalette::PlaceholderText, QColor(127, 127, 127));

    dark.setColor(QPalette::Disabled, QPalette::Highlight, QColor(80, 80, 80));
    dark.setColor(QPalette::Disabled, QPalette::HighlightedText,
                  QColor(127, 127, 127));
    dark.setColor(QPalette::Disabled, QPalette::ButtonText,
                  QColor(127, 127, 127));
    dark.setColor(QPalette::Disabled, QPalette::Text, QColor(127, 127, 127));
    dark.setColor(QPalette::Disabled, QPalette::WindowText,
                  QColor(127, 127, 127));

    QApplication::setPalette(dark);
}

void initQt()
{
    // set up the QApplication flags
    QApplication::setAttribute(Qt::AA_Use96Dpi, true);

#ifdef Q_OS_WIN32
    // Avoid promoting child widgets to child windows
    // This causes bugs with frameless windows as not all child events
    // get sent to the parent - effectively making the window immovable.
    QApplication::setAttribute(Qt::AA_DontCreateNativeWidgetSiblings);
#endif

    QApplication::setStyle(QStyleFactory::create("Fusion"));

    // Arabic build: bundled Thmanyah font + right-to-left UI
    for (const auto &f : QDir(":/fonts").entryList({"*.otf", "*.ttf"}))
    {
        QFontDatabase::addApplicationFont(":/fonts/" + f);
    }
    // Thmanyah's license forbids publishing its files, so public builds ship
    // IBM Plex Sans Arabic (OFL) and every "thmanyah sans" lookup maps to it.
    if (!QFontDatabase::families().contains("thmanyah sans",
                                            Qt::CaseInsensitive))
    {
        QFont::insertSubstitution("thmanyah sans", "IBM Plex Sans Arabic");
    }
    QFont uiFont("thmanyah sans", 10, QFont::Medium);
    uiFont.setHintingPreference(QFont::PreferFullHinting);
    QApplication::setFont(uiFont);
    QApplication::setLayoutDirection(Qt::RightToLeft);
    qApp->setStyleSheet(R"(
QMenu {
    background: #18181d;
    border: 1px solid #2a2a31;
    border-radius: 8px;
    padding: 4px;
    color: #e6e6ea;
}
QMenu::item {
    padding: 7px 16px;
    border-radius: 5px;
}
QMenu::item:selected {
    background: #26262d;
    color: #ffffff;
}
QMenu::item:disabled { color: #5c5c66; }
chatterino--ResizingTextEdit, chatterino--ResizingTextEdit > QWidget {
    background: transparent;
    border: none;
}
QMenu::separator {
    height: 1px;
    background: #26262d;
    margin: 4px 6px;
}
)");


#ifndef Q_OS_MAC
    QApplication::setWindowIcon(QIcon(":/icon.ico"));
#endif

#ifdef Q_OS_MAC
    // On the Mac/Cocoa platform this attribute is enabled by default
    // We override it to ensure shortcuts show in context menus on that platform
    QApplication::setAttribute(Qt::AA_DontShowShortcutsInContextMenus, false);

    // Enable mnemonics (menu hotkeys) on macOS - they are disabled by default
    qt_set_sequence_auto_mnemonic(true);
#endif

    installCustomPalette();
}

// Arabic build: animated loading screen (~1.8s), deletes itself.
class SplashScreen : public QWidget
{
public:
    // overlay that covers the main window and follows its size
    SplashScreen(QWidget *host)
        : QWidget(host)
    {
        this->setAttribute(Qt::WA_DeleteOnClose);
        this->setGeometry(host->rect());
        host->installEventFilter(this);
        this->clock_.start();
        auto *timer = new QTimer(this);
        QObject::connect(timer, &QTimer::timeout, this, [this] {
            if (this->clock_.elapsed() > 1800)
            {
                this->close();
                return;
            }
            this->update();
        });
        timer->start(16);
    }

protected:
    bool eventFilter(QObject *o, QEvent *e) override
    {
        if (e->type() == QEvent::Resize)
        {
            this->setGeometry(this->parentWidget()->rect());
        }
        return QWidget::eventFilter(o, e);
    }

    void paintEvent(QPaintEvent *) override
    {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);
        double t = this->clock_.elapsed() / 1000.0;
        // only the last 0.3s fades, revealing the app underneath
        p.setOpacity(std::clamp((1.8 - t) / 0.3, 0.0, 1.0));
        p.fillRect(this->rect(), QColor("#0b0b0d"));

        QRectF r(this->width() / 2.0 - 170, this->height() / 2.0 - 135, 340,
                 270);

        // logo with a green spinner orbiting it
        QPointF c(r.center().x(), r.top() + 92);
        static const QPixmap logo = QPixmap(":/icon.png");
        p.setRenderHint(QPainter::SmoothPixmapTransform);
        p.drawPixmap(QRectF(c.x() - 40, c.y() - 40, 80, 80), logo,
                     logo.rect());
        p.setBrush(Qt::NoBrush);
        p.setPen(QPen(QColor("#1d1d22"), 3));
        p.drawEllipse(c, 54, 54);
        p.setPen(QPen(QColor("#2ee59d"), 3, Qt::SolidLine, Qt::RoundCap));
        p.drawArc(QRectF(c.x() - 54, c.y() - 54, 108, 108),
                  int(-t * 400 * 16) % (360 * 16), 90 * 16);

        QFont f("thmanyah sans", 15, QFont::Bold);
        p.setFont(f);
        p.setPen(QColor("#ffffff"));
        p.drawText(QRectF(r.left(), r.top() + 160, r.width(), 30),
                   Qt::AlignCenter, "Chatterino");
        f.setPointSize(10);
        f.setWeight(QFont::Medium);
        p.setFont(f);
        p.setPen(QColor("#8b8b95"));
        p.drawText(QRectF(r.left(), r.top() + 192, r.width(), 22),
                   Qt::AlignCenter, "جاري التحميل");
        f.setPointSize(8);
        p.setFont(f);
        p.setPen(QColor("#5c5c66"));
        p.drawText(QRectF(r.left(), r.bottom() - 28, r.width(), 18),
                   Qt::AlignCenter, "Made By RayanAthlawi");
    }

private:
    QElapsedTimer clock_;
};

void showSplash()
{
    auto *s = new SplashScreen(&getApp()->getWindows()->getMainWindow());
    s->raise();
    s->show();
}

void showLastCrashDialog(const Args &args, const Paths &paths)
{
    auto *dialog = new LastRunCrashDialog(args, paths);
    // Use exec() over open() to block the app from being loaded
    // and to be able to set the safe mode.
    dialog->exec();
}

#if defined(NDEBUG) && !defined(CHATTERINO_WITH_CRASHPAD)
std::chrono::steady_clock::time_point signalsInitTime;

[[noreturn]] void handleSignal(int signum)
{
    using namespace std::chrono_literals;

    if (std::chrono::steady_clock::now() - signalsInitTime > 30s &&
        getApp()->getCrashHandler()->shouldRecover())
    {
        QProcess proc;

#    ifdef Q_OS_MAC
        // On macOS, programs are bundled into ".app" Application bundles,
        // when restarting Chatterino that bundle should be opened with the "open"
        // terminal command instead of directly starting the underlying executable,
        // as those are 2 different things for the OS and i.e. do not use
        // the same dock icon (resulting in a second Chatterino icon on restarting)
        CFURLRef appUrlRef = CFBundleCopyBundleURL(CFBundleGetMainBundle());
        CFStringRef macPath =
            CFURLCopyFileSystemPath(appUrlRef, kCFURLPOSIXPathStyle);
        const char *pathPtr =
            CFStringGetCStringPtr(macPath, CFStringGetSystemEncoding());

        proc.setProgram("open");
        proc.setArguments({pathPtr, "-n", "--args", "--crash-recovery"});

        CFRelease(appUrlRef);
        CFRelease(macPath);
#    else
        proc.setProgram(QApplication::applicationFilePath());
        proc.setArguments({"--crash-recovery"});
#    endif

        proc.startDetached();
    }

    std::_Exit(signum);
}
#endif

// We want to restart Chatterino when it crashes and the setting is set to
// true.
void initSignalHandler()
{
#if defined(NDEBUG) && !defined(CHATTERINO_WITH_CRASHPAD)
    signalsInitTime = std::chrono::steady_clock::now();

    signal(SIGSEGV, handleSignal);
#endif

#if defined(Q_OS_UNIX)
    auto *sigintHandler = new UnixSignalHandler(SIGINT);
    QObject::connect(sigintHandler, &UnixSignalHandler::signalFired, [] {
        qCInfo(chatterinoApp) << "Received SIGINT, request application quit";
        QApplication::quit();
    });
    auto *sigtermHandler = new UnixSignalHandler(SIGTERM);
    QObject::connect(sigtermHandler, &UnixSignalHandler::signalFired, [] {
        qCInfo(chatterinoApp) << "Received SIGTERM, request application quit";
        QApplication::quit();
    });
#endif
}

// We delete cache files that haven't been modified in 14 days. This strategy may be
// improved in the future.
void clearCache(const QDir &dir)
{
    size_t deletedCount = 0;
    for (const auto &info : dir.entryInfoList(QDir::Files))
    {
        if (info.lastModified().addDays(14) < QDateTime::currentDateTime())
        {
            bool res = QFile(info.absoluteFilePath()).remove();
            if (res)
            {
                ++deletedCount;
            }
        }
    }
    qCDebug(chatterinoCache)
        << "Deleted" << deletedCount << "files in" << dir.path();
}

// We delete all but the five most recent crashdumps. This strategy may be
// improved in the future.
void clearCrashes(QDir dir)
{
    // crashpad crashdumps are stored inside the Crashes/report directory
    if (!dir.cd("reports"))
    {
        // no reports directory exists = no files to delete
        return;
    }

    dir.setNameFilters({"*.dmp"});

    size_t deletedCount = 0;
    // TODO: use std::views::drop once supported by all compilers
    size_t filesToSkip = 5;
    for (auto &&info : dir.entryInfoList(QDir::Files, QDir::Time))
    {
        if (filesToSkip > 0)
        {
            filesToSkip--;
            continue;
        }

        if (QFile(info.absoluteFilePath()).remove())
        {
            deletedCount++;
        }
    }
    qCDebug(chatterinoApp) << "Deleted" << deletedCount << "crashdumps";
}
}  // namespace

void runGui(QApplication &a, const Paths &paths, Settings &settings,
            const Args &args, Updates &updates)
{
    initQt();
    initResources();
    initSignalHandler();

#ifdef Q_OS_WIN
    if (args.crashRecovery)
    {
        showLastCrashDialog(args, paths);
    }
#endif

    selfcheck::checkWebp();

    updates.deleteOldFiles();

    // Clear the cache 1 minute after start.
    QTimer::singleShot(60 * 1000, [cachePath = paths.cacheDirectory(),
                                   crashDirectory = paths.crashdumpDirectory,
                                   avatarPath = paths.twitchProfileAvatars] {
        std::ignore = QtConcurrent::run([cachePath] {
            clearCache(cachePath);
        });
        std::ignore = QtConcurrent::run([avatarPath] {
            clearCache(avatarPath);
        });
        std::ignore = QtConcurrent::run([crashDirectory] {
            clearCrashes(crashDirectory);
        });
    });

    chatterino::NetworkManager::init();
    updates.checkForUpdates();

    QObject::connect(qApp, &QApplication::aboutToQuit, [] {
        auto *app = dynamic_cast<Application *>(tryGetApp());
        assert(app != nullptr);
        app->aboutToQuit();

        getSettings()->requestSave();
        getSettings()->disableSave();

        app->stop();
    });

    Application app(settings, paths, args, updates);
    app.initialize(settings, paths);
    showSplash();
    viewerpresence::start();
    app.run();

    chatterino::NetworkManager::deinit();

#ifdef USEWINSDK
    // flushing windows clipboard to keep copied messages
    flushClipboard();
#endif
}

}  // namespace chatterino
