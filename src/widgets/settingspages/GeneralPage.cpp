// SPDX-FileCopyrightText: 2018 Contributors to Chatterino <https://chatterino.com>
//
// SPDX-License-Identifier: MIT

#include "widgets/settingspages/GeneralPage.hpp"

#include "Application.hpp"
#include "common/Literals.hpp"  // IWYU pragma: keep
#include "common/Version.hpp"
#include "controllers/hotkeys/HotkeyCategory.hpp"
#include "controllers/hotkeys/HotkeyController.hpp"
#include "providers/twitch/TwitchChannel.hpp"
#include "providers/twitch/TwitchIrcServer.hpp"
#include "singletons/CrashHandler.hpp"
#include "singletons/Fonts.hpp"
#include "singletons/NativeMessaging.hpp"
#include "singletons/Paths.hpp"
#include "singletons/Settings.hpp"
#include "providers/twitch/ViewerPresence.hpp"
#include "singletons/Theme.hpp"
#include "util/FuzzyConvert.hpp"
#include "util/Helpers.hpp"
#include "util/IncognitoBrowser.hpp"
#include "widgets/BaseWindow.hpp"
#include "widgets/helper/FontSettingWidget.hpp"
#include "widgets/settingspages/GeneralPageView.hpp"
#include "widgets/settingspages/SettingWidget.hpp"

#include <QDesktopServices>
#include <QFileDialog>
#include <QFontDialog>
#include <QFormLayout>
#include <QLabel>
#include <QMessageBox>
#include <QPalette>
#include <QSignalBlocker>

namespace {

using namespace chatterino;
using namespace chatterino::literals;

const QString CHROME_EXTENSION_LINK =
    u"https://chrome.google.com/webstore/detail/chatterino-native-host/glknmaideaikkmemifbfkhnomoknepka"_s;
const QString FIREFOX_EXTENSION_LINK =
    u"https://addons.mozilla.org/en-US/firefox/addon/chatterino-native-host/"_s;

#ifdef Q_OS_WIN
const QString META_KEY = u"Windows"_s;
#else
const QString META_KEY = u"Meta"_s;
#endif

const QStringList ZOOM_LEVELS = {
    "0.5x", "0.6x", "0.7x", "0.8x",  "0.9x",  "Default", "1.2x", "1.4x",
    "1.6x", "1.8x", "2x",   "2.33x", "2.66x", "3x",      "3.5x", "4x",
};

void addKeyboardModifierSetting(GeneralPageView &layout, const QString &title,
                                EnumSetting<Qt::KeyboardModifier> &setting)
{
    layout.addDropdown<std::underlying_type_t<Qt::KeyboardModifier>>(
        title, {"None", "Shift", "Control", "Alt", META_KEY}, setting,
        [](int index) {
            switch (index)
            {
                case Qt::ShiftModifier:
                    return 1;
                case Qt::ControlModifier:
                    return 2;
                case Qt::AltModifier:
                    return 3;
                case Qt::MetaModifier:
                    return 4;
                default:
                    return 0;
            }
        },
        [](DropdownArgs args) {
            switch (args.index)
            {
                case 1:
                    return Qt::ShiftModifier;
                case 2:
                    return Qt::ControlModifier;
                case 3:
                    return Qt::AltModifier;
                case 4:
                    return Qt::MetaModifier;
                default:
                    return Qt::NoModifier;
            }
        },
        false);
}
}  // namespace

namespace chatterino {

GeneralPage::GeneralPage()
{
    auto *y = new QVBoxLayout;
    auto *x = new QHBoxLayout;
    auto *view = GeneralPageView::withNavigation(this);
    this->view_ = view;
    x->addWidget(view);
    auto *z = new QFrame;
    z->setLayout(x);
    y->addWidget(z);
    this->setLayout(y);

    this->initLayout(*view);

    this->initExtra();
}

bool GeneralPage::filterElements(const QString &query)
{
    if (this->view_)
    {
        return this->view_->filterElements(query) || query.isEmpty();
    }
    else
    {
        return false;
    }
}

void GeneralPage::initLayout(GeneralPageView &layout)
{
    auto &s = *getSettings();

    layout.addTitle("الواجهة");

    {
        // Arabic build: theme is fixed (Thmanyah), picker removed
    }

    layout.addDropdown<float>(
        "التكبير", ZOOM_LEVELS, s.uiScale,
        [](auto val) {
            if (val == 1)
            {
                return QString("Default");
            }
            else
            {
                return QString::number(val) + "x";
            }
        },
        [](auto args) {
            return fuzzyToFloat(args.value, 1.f);
        });
    ComboBox *tabDirectionDropdown =
        layout.addDropdown<std::underlying_type_t<NotebookTabLocation>>(
            "مكان التبويبات", {"Top", "Left", "Right", "Bottom"}, s.tabDirection,
            [](auto val) {
                switch (val)
                {
                    case NotebookTabLocation::Top:
                        return "Top";
                    case NotebookTabLocation::Left:
                        return "Left";
                    case NotebookTabLocation::Right:
                        return "Right";
                    case NotebookTabLocation::Bottom:
                        return "Bottom";
                }

                return "";
            },
            [](auto args) {
                if (args.value == "Bottom")
                {
                    return NotebookTabLocation::Bottom;
                }
                else if (args.value == "Left")
                {
                    return NotebookTabLocation::Left;
                }
                else if (args.value == "Right")
                {
                    return NotebookTabLocation::Right;
                }
                else
                {
                    // default to top
                    return NotebookTabLocation::Top;
                }
            },
            false);
    tabDirectionDropdown->setMinimumWidth(
        tabDirectionDropdown->minimumSizeHint().width());

    layout.addDropdown<std::underlying_type_t<NotebookTabVisibility>>(
        "ظهور التبويبات", {"All tabs", "Only live tabs"}, s.tabVisibility,
        [](auto val) {
            switch (val)
            {
                case NotebookTabVisibility::LiveOnly:
                    return "Only live tabs";
                case NotebookTabVisibility::AllTabs:
                default:
                    return "All tabs";
            }
        },
        [](auto args) {
            if (args.value == "Only live tabs")
            {
                return NotebookTabVisibility::LiveOnly;
            }
            else
            {
                return NotebookTabVisibility::AllTabs;
            }
        },
        false, "Choose which tabs are visible in the notebook");

    SettingWidget::dropdown("شكل التبويبات", s.tabStyle)->addTo(layout);

    SettingWidget::checkbox("رتّب القنوات اللايف فوق", s.groupLiveTabs)
        ->setTooltip("في التبويبات الجانبية: قسم للقنوات اللايف فوق، وقسم "
                     "لغير المتصلة تحت. اضغط على عنوان القسم تطويه أو تفتحه.")
        ->addTo(layout);

    SettingWidget::checkbox("احسبني مشاهد في البث",
                            viewerpresence::enabledSetting())
        ->setTooltip("يسحب أقل جودة من بث القنوات الظاهرة قدامك بالخلفية "
                     "بدون تشغيل فيديو، عشان تنحسب مشاهد واحد. خفيف جدًا.")
        ->addTo(layout);

    SettingWidget::checkbox("عرض البث فوق الشات",
                            viewerpresence::playerSetting())
        ->setTooltip("مشغّل صغير فوق الشات (480p أو أقل، يبدأ بدون صوت). "
                     "يشتغل بس في التبويب الظاهر.")
        ->addTo(layout);


    layout.addWidget(new FontSettingWidget(s.chatFontFamily, s.chatFontSize,
                                           s.chatFontWeight),
                     {"font", "weight", "size"});

    SettingWidget::inverseCheckbox("إظهار سياق الرد",
                                   s.hideReplyContext)
        ->setTooltip(
            "هذا الإعداد يغيّر طريقة عرض الرسائل بس. تقدر ترد على أي رسالة بغض النظر عنه.")
        ->addTo(layout);

    SettingWidget::checkbox("إظهار زر الرد", s.showReplyButton)
        ->setTooltip("إظهار زر رد جنب كل رسالة")
        ->addTo(layout);

    auto removeTabSeq = getApp()->getHotkeys()->getDisplaySequence(
        HotkeyCategory::Window, "removeTab");
    QString removeTabShortcut = "an assigned hotkey (Window -> remove tab)";
    if (!removeTabSeq.isEmpty())
    {
        removeTabShortcut =
            removeTabSeq.toString(QKeySequence::SequenceFormat::NativeText);
    }

    SettingWidget::checkbox("إظهار زر إغلاق التبويب", s.showTabCloseButton)
        ->setTooltip(
            "إذا عطّلته، بيختفي زر x لإغلاق التبويب.\nتقدر تسكّر التبويب بالضغط يمين أو بالضغط على " +
            removeTabShortcut + ".")
        ->addTo(layout);

    SettingWidget::checkbox("دائمًا في المقدمة", s.windowTopMost)
        ->setTooltip("خلّ Chatterino دائمًا فوق كل النوافذ.")
        ->addTo(layout);

#ifdef USEWINSDK
    SettingWidget::checkbox("التشغيل مع ويندوز", s.autorun)
        ->setTooltip("شغّل Chatterino لما يشتغل جهازك.")
        ->addTo(layout);
#endif
    if (!BaseWindow::supportsCustomWindowFrame())
    {
        auto settingsSeq = getApp()->getHotkeys()->getDisplaySequence(
            HotkeyCategory::Window, "openSettings");
        QString shortcut = " (no key bound to open them otherwise)";
        // TODO: maybe prevent the user from locking themselves out of the settings?
        if (!settingsSeq.isEmpty())
        {
            shortcut = QStringLiteral(" (%1 to show)")
                           .arg(settingsSeq.toString(
                               QKeySequence::SequenceFormat::NativeText));
        }

        SettingWidget::inverseCheckbox("إظهار زر الإعدادات" + shortcut,
                                       s.hidePreferencesButton)
            ->addTo(layout);

        SettingWidget::inverseCheckbox("إظهار زر المستخدم", s.hideUserButton)
            ->addTo(layout);
    }

    SettingWidget::checkbox("تمييز التبويبات اللي فيها قنوات مباشرة", s.showTabLive)
        ->setTooltip("يعرض نقطة حمراء في زاوية التبويب لما تكون إحدى قنواته في بث مباشر.")
        ->addTo(layout);

    layout.addTitle("الشات");

    layout.addDropdown<float>(
        "إيقاف مؤقت لما يمر الماوس",
        {"Disabled", "0.5s", "1s", "2s", "5s", "Indefinite"},
        s.pauseOnHoverDuration,
        [](auto val) {
            if (val < -0.5f)
            {
                return QString("Indefinite");
            }
            else if (val < 0.001f)
            {
                return QString("Disabled");
            }
            else
            {
                return QString::number(val) + "s";
            }
        },
        [](auto args) {
            if (args.index == 0)
            {
                return 0.0f;
            }
            else if (args.value == "Indefinite")
            {
                return -1.0f;
            }
            else
            {
                return fuzzyToFloat(args.value,
                                    std::numeric_limits<float>::infinity());
            }
        });
    addKeyboardModifierSetting(layout, "Pause while holding a key",
                               s.pauseChatModifier);
    layout.addDropdown<float>(
        "سرعة التمرير بعجلة الماوس", {"0.5x", "0.75x", "Default", "1.5x", "2x"},
        s.mouseScrollMultiplier,
        [](auto val) {
            if (val == 1)
            {
                return QString("Default");
            }
            else
            {
                return QString::number(val) + "x";
            }
        },
        [](auto args) {
            return fuzzyToFloat(args.value, 1.f);
        });

    SettingWidget::checkbox("تمرير ناعم", s.enableSmoothScrolling)
        ->addTo(layout);

    SettingWidget::checkbox("تمرير ناعم مع الرسائل الجديدة",
                            s.enableSmoothScrollingNewMessages)
        ->addTo(layout);

    SettingWidget::checkbox("إظهار خانة الكتابة وهي فاضية", s.showEmptyInput)
        ->setTooltip("إظهار خانة الكتابة حتى لو ما فيها شي.")
        ->addTo(layout);

    SettingWidget::checkbox("إظهار طول الرسالة أثناء الكتابة",
                            s.showMessageLength)
        ->setTooltip(
            "يعرض كم حرف في خانة الكتابة.\nمفيد عشان ما تتجاوز حد Twitch (500 حرف) أو حد أقل يفرضه بوت إشراف")
        ->addTo(layout);

    SettingWidget::checkbox("إظهار العد التنازلي في الوضع البطيء أو عند الطرد المؤقت",
                            s.showSendWaitTimer)
        ->setTooltip("يعرض كم لازم تنتظر قبل لا تقدر ترسل مرة ثانية إذا القناة في الوضع البطيء أو انطردت مؤقتًا")
        ->addKeywords({"slowmode", "timeout"})
        ->addTo(layout);

    SettingWidget::checkbox("السماح بإرسال رسائل مكررة",
                            s.allowDuplicateMessages)
        ->setTooltip(
            "السماح بإرسال نفس الرسالة أكثر من مرة بدون تغيير.")
        ->addTo(layout);

    layout.addDropdown<std::underlying_type_t<MessageOverflow>>(
        "تجاوز طول الرسالة", {"Highlight", "Prevent", "Allow"},
        s.messageOverflow,
        [](auto index) {
            return index;
        },
        [](auto args) {
            return static_cast<MessageOverflow>(args.index);
        },
        false,
        "Specify how Chatterino will handle messages that exceed Twitch "
        "message limits");
    layout.addDropdown<std::underlying_type_t<UsernameRightClickBehavior>>(
        "الزر اليمين على اسم المستخدم",
        {
            "Reply",
            "Mention",
            "Ignore",
        },
        s.usernameRightClickBehavior,
        [](auto index) {
            return index;
        },
        [](auto args) {
            return static_cast<UsernameRightClickBehavior>(args.index);
        },
        false,
        "Specify how Chatterino will handle right-clicking a username in "
        "chat when not holding the modifier.");
    layout.addDropdown<std::underlying_type_t<UsernameRightClickBehavior>>(
        "الزر اليمين مع مفتاح على اسم المستخدم",
        {
            "Reply",
            "Mention",
            "Ignore",
        },
        s.usernameRightClickModifierBehavior,
        [](auto index) {
            return index;
        },
        [](auto args) {
            return static_cast<UsernameRightClickBehavior>(args.index);
        },
        false,
        "Specify how Chatterino will handle right-clicking a username in "
        "chat when holding down the modifier.");
    layout.addDropdown<std::underlying_type_t<Qt::KeyboardModifier>>(
        "مفتاح الإجراء البديل للزر اليمين",
        {"Shift", "Control", "Alt", META_KEY}, s.usernameRightClickModifier,
        [](int index) {
            switch (index)
            {
                case Qt::ShiftModifier:
                    return 0;
                case Qt::ControlModifier:
                    return 1;
                case Qt::AltModifier:
                    return 2;
                case Qt::MetaModifier:
                    return 3;
                default:
                    return 0;
            }
        },
        [](DropdownArgs args) {
            switch (args.index)
            {
                case 0:
                    return Qt::ShiftModifier;
                case 1:
                    return Qt::ControlModifier;
                case 2:
                    return Qt::AltModifier;
                case 3:
                    return Qt::MetaModifier;
                default:
                    return Qt::NoModifier;
            }
        },
        false);

    SettingWidget::checkbox("إخفاء مقبض شريط التمرير", s.hideScrollbarThumb)
        ->setTooltip("إخفاء المقبض (اللي تسحبه) يعطّل التفاعل بالماوس مع شريط التمرير.")
        ->addKeywords({"scroll bar"})
        ->addTo(layout);

    SettingWidget::checkbox("إخفاء التمييزات في شريط التمرير",
                            s.hideScrollbarHighlights)
        ->addKeywords({"scroll bar"})
        ->addTo(layout);

    SettingWidget::checkbox(
        "وميض خانة الكتابة لما تنرسل رسالتك",
        s.pulseTextInputOnSelfMessage)
        ->setTooltip(
            "تومض خانة الكتابة بالأخضر كل ما تنرسل رسالتك بنجاح في القناة.")
        ->addTo(layout);

    layout.addTitle("الرسائل");

    SettingWidget::checkbox("فصل بخطوط", s.separateMessages)
        ->setTooltip(
            "يضيف خط بين كل رسالة والثانية عشان تتميز.")
        ->addTo(layout);

    SettingWidget::checkbox("تلوين متناوب للخلفية", s.alternateMessages)
        ->setTooltip("يغيّر لون الخلفية شوي لكل رسالة ثانية عشان تتميز.")
        ->addTo(layout);

    SettingWidget::checkbox("تخفيف شفافية الرسائل القديمة",
                            s.fadeMessageHistory)
        ->setTooltip(
            "يخفف شفافية الرسائل اللي انرسلت قبل تشغيل Chatterino أو أثناء إعادة الاتصال.")
        ->addTo(layout);

    SettingWidget::checkbox("إخفاء الرسائل المحذوفة", s.hideModerated)
        ->setTooltip(
            "إذا فعّلته، الرسائل اللي حذفها المشرفين تنخفي.")
        ->addTo(layout);

    layout.addDropdown<QString>(
        "صيغة وقت الرسالة",
        {"Disable", "h:mm", "hh:mm", "h:mm a", "hh:mm a", "h:mm:ss", "hh:mm:ss",
         "h:mm:ss a", "hh:mm:ss a", "h:mm:ss.zzz", "h:mm:ss.zzz a",
         "hh:mm:ss.zzz", "hh:mm:ss.zzz a"},
        s.timestampFormat,
        [](auto val) {
            return getSettings()->showTimestamps.getValue()
                       ? val
                       : QString("Disable");
        },
        [](auto args) {
            getSettings()->showTimestamps.setValue(args.index != 0);

            return args.index == 0 ? getSettings()->timestampFormat.getValue()
                                   : args.value;
        },
        true, "a = am/pm, zzz = milliseconds");
    layout.addDropdown<int>(
        "تحديد ارتفاع الرسالة",
        {"Never", "2 lines", "3 lines", "4 lines", "5 lines"},
        s.collpseMessagesMinLines,
        [](auto val) {
            return val ? QString::number(val) + " lines" : QString("Never");
        },
        [](auto args) {
            return fuzzyToInt(args.value, 0);
        });
    layout.addDropdown<int>(
        "تحديد طول الرسائل المحذوفة",
        {"No limit", "50 characters", "100 characters", "200 characters",
         "300 characters", "400 characters"},
        s.deletedMessageLengthLimit,
        [](auto val) {
            return val ? QString::number(val) + " characters"
                       : QString("No limit");
        },
        [](const auto &args) {
            return fuzzyToInt(args.value, 0);
        },
        true,
        {"Limits the amount of characters displayed in deleted messages "
         "when announced via system message."});

    layout.addSeparator();

    SettingWidget::checkbox("رسم خط تحت آخر رسالة قبل التبديل لبرنامج ثاني.",
                            s.showLastMessageIndicator)
        ->setTooltip("يضيف خط تحت آخر رسالة انرسلت قبل لا تطلع من Chatterino.")
        ->addTo(layout);

    SettingWidget::dropdown("نوع الخط", s.lastMessagePattern)->addTo(layout);

    SettingWidget::colorButton("لون الخط", s.lastMessageColor)->addTo(layout);

    layout.addTitle("الإيموتات");

    SettingWidget::checkbox("تفعيل", s.enableEmoteImages)->addTo(layout);

    SettingWidget::checkbox("تحريك", s.animateEmotes)->addTo(layout);

    SettingWidget::checkbox("التحريك فقط لما يكون Chatterino نشط",
                            s.animationsWhenFocused)
        ->addTo(layout);

    SettingWidget::checkbox("تفعيل الإيموتات صفرية العرض", s.enableZeroWidthEmotes)
        ->setTooltip(
            "إذا عطّلته، الإيموتات اللي تتراكب فوق غيرها مثل cvMask من BTTV و RainTime من 7TV بتظهر كإيموتات عادية.")
        ->addTo(layout);

    SettingWidget::checkbox("إكمال الإيموتات بكتابة :",
                            s.emoteCompletionWithColon)
        ->setTooltip(
            "مع هذا الإعداد، كتابة النقطتين : تفتح قائمة إيموتات تتحدث حسب النص اللي بعدها.")
        ->addTo(layout);

    SettingWidget::checkbox("استخدام إكمال إيموتات ذكي (تجريبي).",
                            s.useSmartEmoteCompletion)
        ->addTo(layout);

    layout.addDropdown<float>(
        "الحجم", {"0.5x", "0.75x", "Default", "1.25x", "1.5x", "2x"},
        s.emoteScale,
        [](auto val) {
            if (val == 1)
            {
                return QString("Default");
            }
            else
            {
                return QString::number(val) + "x";
            }
        },
        [](auto args) {
            return fuzzyToFloat(args.value, 1.f);
        });

    SettingWidget::checkbox("إزالة المسافات بين الإيموتات",
                            s.removeSpacesBetweenEmotes)
        ->setTooltip("إذا فعّلته، الإيموتات المتجاورة ما بيكون بينها مسافة.")
        ->addTo(layout);

    SettingWidget::checkbox("إظهار إيموتات 7TV غير المدرجة",
                            s.showUnlistedSevenTVEmotes)
        ->addKeywords({"seventv"})
        ->addTo(layout);
    // TODO: Add a tooltip explaining what an unlisted 7TV emote is
    // but wait until https://github.com/Chatterino/wiki/pull/255 is resolved,
    // as an official description from 7TV devs is best
    s.showUnlistedSevenTVEmotes.connect(
        []() {
            getApp()->getTwitch()->forEachChannelAndSpecialChannels(
                [](const auto &c) {
                    if (c->isTwitchChannel())
                    {
                        auto *channel = dynamic_cast<TwitchChannel *>(c.get());
                        if (channel != nullptr)
                        {
                            channel->refreshSevenTVChannelEmotes(false);
                        }
                    }
                });
        },
        false);

    SettingWidget::dropdown("إظهار معاينة الإيموت والشارة عند المرور",
                            s.emotesTooltipPreview)
        ->addTo(layout);

    SettingWidget::dropdown("حجم معاينة الإيموت والشارة عند المرور",
                            s.emoteTooltipScale)
        ->addTo(layout);

    SettingWidget::dropdown("نمط الإيموجي", s.emojiSet)->addTo(layout);

    SettingWidget::checkbox("إظهار إيموتات BetterTTV العامة",
                            s.enableBTTVGlobalEmotes)
        ->addKeywords({"bttv"})
        ->addTo(layout);
    SettingWidget::checkbox("إظهار إيموتات BetterTTV للقناة",
                            s.enableBTTVChannelEmotes)
        ->addKeywords({"bttv"})
        ->addTo(layout);
    SettingWidget::checkbox(
        "تحديث إيموتات BetterTTV مباشرة (يتطلب إعادة تشغيل)",
        s.enableBTTVLiveUpdates)
        ->addKeywords({"bttv"})
        ->addTo(layout);
    SettingWidget::checkbox("إرسال النشاط إلى BetterTTV", s.sendBTTVActivity)
        ->setTooltip(
            "إذا فعّلته، Chatterino يبلّغ BetterTTV بنشاطك لما ترسل رسالة. يُستخدم للشارات والإيموتات الشخصية. إذا عطّلته، ما ينرسل شي والباقين ما بيشوفون مظهرك.")
        ->addKeywords({"bttv"})
        ->addTo(layout);

    SettingWidget::checkbox("إظهار إيموتات FrankerFaceZ العامة",
                            s.enableFFZGlobalEmotes)
        ->addKeywords({"ffz"})
        ->addTo(layout);
    SettingWidget::checkbox("إظهار إيموتات FrankerFaceZ للقناة",
                            s.enableFFZChannelEmotes)
        ->addKeywords({"ffz"})
        ->addTo(layout);

    SettingWidget::checkbox("إظهار إيموتات 7TV العامة",
                            s.enableSevenTVGlobalEmotes)
        ->addKeywords({"seventv"})
        ->addTo(layout);
    SettingWidget::checkbox("إظهار إيموتات 7TV للقناة",
                            s.enableSevenTVChannelEmotes)
        ->addKeywords({"seventv"})
        ->addTo(layout);
    SettingWidget::checkbox("تحديث إيموتات 7TV مباشرة (يتطلب إعادة تشغيل)",
                            s.enableSevenTVEventAPI)
        ->addKeywords({"seventv"})
        ->addTo(layout);
    SettingWidget::checkbox("إرسال النشاط إلى 7TV", s.sendSevenTVActivity)
        ->setTooltip("إذا فعّلته، Chatterino يبلّغ 7TV بنشاطك لما ترسل رسالة. يُستخدم للشارات والألوان والإيموتات الشخصية. إذا عطّلته، ما ينرسل شي والباقين ما بيشوفون مظهرك.")
        ->addKeywords({"seventv"})
        ->addTo(layout);

    layout.addTitle("وضع الستريمر");
    layout.addDescription(
        "يقدر Chatterino يغيّر سلوكه تلقائيًا إذا اكتشف إن برنامج بث شغّال.\nاختر وش تبي يتغيّر أثناء البث");

    SettingWidget::dropdown("تفعيل وضع الستريمر", s.enableStreamerMode)
        ->addTo(layout);

    SettingWidget::checkbox("إخفاء صور المستخدمين في البطاقة",
                            s.streamerModeHideUsercardAvatars)
        ->setTooltip("منع ظهور صور قد تكون غير لائقة.")
        ->addTo(layout);

    SettingWidget::checkbox("إخفاء معاينات الروابط",
                            s.streamerModeHideLinkThumbnails)
        ->setTooltip("منع ظهور معاينات قد تكون غير لائقة عند المرور على الروابط.")
        ->addTo(layout);

    SettingWidget::checkbox(
        "إخفاء عدد المشاهدين ومدة البث عند المرور على رأس القسم",
        s.streamerModeHideViewerCountAndDuration)
        ->addTo(layout);

    SettingWidget::checkbox("إخفاء إجراءات الإشراف",
                            s.streamerModeHideModActions)
        ->setTooltip(
            "إخفاء الحظر والطرد المؤقت ورسائل AutoMod من الشات.")
        ->addTo(layout);

    SettingWidget::checkbox("إخفاء رسائل المستخدمين المقيّدين",
                            s.streamerModeHideRestrictedUsers)
        ->setTooltip("المستخدمين المقيّدين ممكن تحددهم أنت أو مشرفينك أو AutoMod من Twitch")
        ->addTo(layout);

    SettingWidget::checkbox("إخفاء الكلمات المحظورة",
                            s.streamerModeHideBlockedTermText)
        ->setTooltip(
            "إخفاء الكلمات المحظورة من أماكن مثل رسائل AutoMod. مفيد إذا عندك كلمات محظورة ما تبيها تطلع في البث.")
        ->addTo(layout);

    SettingWidget::checkbox("كتم أصوات الإشارات", s.streamerModeMuteMentions)
        ->setTooltip("منع تشغيل صوت التنبيه لما أحد يذكرك.")
        ->addTo(layout);

    SettingWidget::checkbox("إيقاف تنبيهات البث",
                            s.streamerModeSuppressLiveNotifications)
        ->setTooltip(
            "إخفاء نوافذ تنبيهات البث المنبثقة. (ويندوز فقط)")
        ->addTo(layout);

    SettingWidget::checkbox("إيقاف الهمسات داخل الشات",
                            s.streamerModeSuppressInlineWhispers)
        ->setTooltip("إخفاء الهمسات اللي توصلك من الظهور في الشات.")
        ->addTo(layout);

    layout.addTitle("معاينة الروابط");
    layout.addDescription(
        "معلومات إضافية مثل \"إحصائيات فيديوهات يوتيوب\" أو عناوين الصفحات تنحمّل لكل الروابط إذا فعّلتها. وتقدر كمان تعرض صور مصغّرة للإيموتات والفيديوهات وغيرها. المعلومات تجي من سيرفراتنا. معاينات الروابط تنحمّل عن طريق <a href=\"https://github.com/Chatterino/api\">واجهة API</a> يستضيفها مطورين Chatterino. هذي <a href=\"https://braize.pajlada.com/chatterino/legal/terms-of-service\">شروط الخدمة</a> و<a href=\"https://braize.pajlada.com/chatterino/legal/privacy-policy\">سياسة الخصوصية</a> للـ API.");

    SettingWidget::checkbox("تفعيل", s.linkInfoTooltip)->addTo(layout);

    layout.addDropdown<int>(
        "إظهار الصور المصغّرة إذا توفرت",
        {"Off", "Small", "Medium", "Large"}, s.thumbnailSize,
        [](auto val) {
            if (val == 0)
            {
                return QString("Off");
            }
            else if (val == 100)
            {
                return QString("Small");
            }
            else if (val == 200)
            {
                return QString("Medium");
            }
            else if (val == 300)
            {
                return QString("Large");
            }
            else
            {
                return QString::number(val);
            }
        },
        [](auto args) {
            if (args.value == "Small")
            {
                return 100;
            }
            else if (args.value == "Medium")
            {
                return 200;
            }
            else if (args.value == "Large")
            {
                return 300;
            }

            return fuzzyToInt(args.value, 0);
        });
    layout.addDropdown<int>(
        "إظهار صور مصغّرة للبثوث", {"Off", "Small", "Medium", "Large"},
        s.thumbnailSizeStream,
        [](auto val) {
            if (val == 0)
            {
                return QString("Off");
            }
            else if (val == 1)
            {
                return QString("Small");
            }
            else if (val == 2)
            {
                return QString("Medium");
            }
            else if (val == 3)
            {
                return QString("Large");
            }
            else
            {
                return QString::number(val);
            }
        },
        [](auto args) {
            if (args.value == "Small")
            {
                return 1;
            }
            else if (args.value == "Medium")
            {
                return 2;
            }
            else if (args.value == "Large")
            {
                return 3;
            }

            return fuzzyToInt(args.value, 0);
        });

    layout.addNavigationSpacing();
    layout.addTitle("تجريبي");
    if (Version::instance().isSupportedOS())
    {
        layout.addDescription(
            "تقدر تستلم التحديثات بدري بتفعيل الخيار تحت. بلّغ عن المشاكل <a href=\'https://chatterino.com/link/issues\'>هنا</a>.");

        SettingWidget::checkbox("استلام التحديثات التجريبية", s.betaUpdates)
            ->addTo(layout);
    }
    else
    {
        layout.addDescription(
            "نظامك ما له نسخ رسمية. للتحديث، ابنِ Chatterino من المصدر. بلّغ عن المشاكل <a href=\'https://chatterino.com/link/issues\'>هنا</a>.");
    }

    layout.addTitle("الربط مع المتصفح");
#ifdef Q_OS_WIN
    layout.addDescription(
        "إضافة المتصفح تستبدل شات Twitch.tv الافتراضي بـ Chatterino، وتحدّث قسم /watching في Chatterino لما يكون Twitch.tv مفتوح.");
#else
    layout.addDescription("إضافة المتصفح تحدّث قسم /watching في Chatterino لما يكون Twitch.tv مفتوح.");
#endif

    {
        if (auto err = nmIpcError().get())
        {
            layout.addDescription(
                "صار خطأ أثناء تهيئة إضافة المتصفح: " +
                *err);
        }
    }

    layout.addDescription(formatRichNamedLink(
        CHROME_EXTENSION_LINK,
        "Download for Google Chrome and similar browsers."));
    layout.addDescription(
        formatRichNamedLink(FIREFOX_EXTENSION_LINK, "Download for Firefox"));

#ifdef Q_OS_WIN
    layout.addDescription("Chatterino يرتبط بس بالمتصفحات المعروفة عشان ما يرتبط بنوافذ ثانية بالغلط.");
    SettingWidget::checkbox("الارتباط بأي متصفح (ممكن يسبب مشاكل)",
                            s.attachExtensionToAnyProcess)
        ->setTooltip(
            "محاولة إجبار إضافة Chatterino تشتغل في متصفحات ما تشتغل فيها تلقائيًا.\nمثل Librewolf")
        ->addTo(layout);
#endif

    {
        auto *note = layout.addDescription(
            "A semicolon-separated list of Chrome or Firefox extension IDs "
            "allowed to interact with Chatterino's browser integration "
            "(requires restart).\n"
            "Using multiple extension IDs from different browsers may cause "
            "issues.");
        note->setWordWrap(true);
        note->setStyleSheet("color: #bbb");

        layout.addWidget(note);

        auto *form = new QFormLayout();
        layout.addLayout(form);

        SettingWidget::lineEdit("معرّفات إضافات أخرى", s.additionalExtensionIDs,
                                "Extension;IDs;separated;by;semicolons")
            ->addTo(layout, form);
    }

    layout.addTitle("بيانات التطبيق والكاش");

    layout.addSubtitle("بيانات التطبيق");
    layout.addDescription("كل الملفات المحلية مثل الإعدادات والكاش محفوظة في هذا المجلد.");
    layout.addButton("فتح مجلد البيانات", [] {
#ifdef Q_OS_DARWIN
        QDesktopServices::openUrl("file://" +
                                  getApp()->getPaths().rootAppDataDirectory);
#else
        QDesktopServices::openUrl(getApp()->getPaths().rootAppDataDirectory);
#endif
    });

    layout.addSubtitle("الملفات المؤقتة (الكاش)");
    layout.addDescription(
        "الملفات اللي تُستخدم كثير (مثل الإيموتات) تنحفظ على الجهاز عشان توفر الإنترنت وتسرّع التحميل.");

    auto *cachePathLabel = layout.addDescription("placeholder :D");
    getSettings()->cachePath.connect([cachePathLabel](const auto &,
                                                      auto) mutable {
        QString newPath = getApp()->getPaths().cacheDirectory();

        QString pathShortened = "Cache saved at <a href=\"file:///" + newPath +
                                R"("><span style="color: white;">)" +
                                shortenString(newPath, 50) + "</span></a>";
        cachePathLabel->setText(pathShortened);
        cachePathLabel->setToolTip(newPath);
    });

    // Choose and reset buttons
    {
        auto *box = new QHBoxLayout;

        box->addWidget(layout.makeButton("اختيار مسار الكاش", [this]() {
            getSettings()->cachePath = QFileDialog::getExistingDirectory(this);
        }));
        box->addWidget(layout.makeButton("إعادة تعيين", []() {
            getSettings()->cachePath = "";
        }));
        box->addWidget(layout.makeButton("مسح الكاش", [&layout]() {
            auto reply = QMessageBox::question(
                layout.window(), "مسح الكاش",
                "متأكد تبي تمسح الكاش؟ ممكن تتأخر الإيموتات في التحميل المرة الجاية اللي تشغّل فيها Chatterino.",
                QMessageBox::Yes | QMessageBox::No);

            if (reply == QMessageBox::Yes)
            {
                auto cacheDir = QDir(getApp()->getPaths().cacheDirectory());
                cacheDir.removeRecursively();
                cacheDir.mkdir(getApp()->getPaths().cacheDirectory());
            }
        }));
        box->addStretch(1);

        layout.addLayout(box);
    }

    layout.addTitle("الصوت");

    SettingWidget::dropdown("محرك الصوت (يتطلب إعادة تشغيل)", s.soundBackend)
        ->setTooltip("غيّره بس إذا لاحظت مشاكل في تشغيل الصوت على جهازك")
        ->addTo(layout);
    SettingWidget::checkbox("إبقاء محرك الصوت شغّال (يتطلب إعادة تشغيل)",
                            s.soundMiniaudioKeepEngineAlive)
        ->setTooltip(
            "هذا الإعداد يخلّي Chatterino يرسل صمت لجهاز الصوت حتى لو ما في تنبيه. جرّبه إذا عندك سماعات لاسلكية ما تشغّل أول صوت. ملاحظة: ممكن يمنع الشاشة أو الجهاز من السكون.")
        ->addTo(layout);

    layout.addTitle("متقدم");

    layout.addSubtitle("عنوان الشات");
    layout.addDescription("في القنوات المباشرة اعرض:");
    SettingWidget::checkbox("مدة البث", s.headerUptime)
        ->setTooltip("يعرض كم صار للقناة في البث")
        ->addTo(layout);
    SettingWidget::checkbox("عدد المشاهدين", s.headerViewerCount)
        ->setTooltip("يعرض كم واحد يشاهد")
        ->addTo(layout);
    SettingWidget::checkbox("التصنيف", s.headerGame)
        ->setTooltip("يعرض تصنيف البث")
        ->addTo(layout);
    SettingWidget::checkbox("العنوان", s.headerStreamTitle)
        ->setTooltip("يعرض عنوان البث")
        ->addTo(layout);

    layout.addSubtitle("R9K");
    auto toggleLocalr9kSeq = getApp()->getHotkeys()->getDisplaySequence(
        HotkeyCategory::Window, "toggleLocalR9K");
    QString toggleLocalr9kShortcut =
        "an assigned hotkey (Window -> Toggle local R9K)";
    if (!toggleLocalr9kSeq.isEmpty())
    {
        toggleLocalr9kShortcut = toggleLocalr9kSeq.toString(
            QKeySequence::SequenceFormat::NativeText);
    }
    layout.addDescription("إخفاء الرسائل المشابهة لرسائل سابقة. بدّل ظهور الرسائل المخفية بالضغط على " +
                          toggleLocalr9kShortcut + ".");

    SettingWidget::checkbox("تفعيل فحص التشابه", s.similarityEnabled)
        ->addTo(layout);

    // SettingWidget::checkbox("Gray out matches", s.colorSimilarDisabled)->addTo(layout);

    SettingWidget::checkbox("فقط إذا من نفس المستخدم", s.hideSimilarBySameUser)
        ->setTooltip(
            "إذا فعّلته، الرسائل المتشابهة جدًا تبقى ظاهرة ما دامها من مستخدمين مختلفين.")
        ->addTo(layout);

    SettingWidget::checkbox("إخفاء رسائلي", s.hideSimilarMyself)
        ->addTo(layout);

    SettingWidget::checkbox("تشغيل أصوات التنبيه للرسائل المخفية",
                            s.shownSimilarTriggerHighlights)
        ->addTo(layout);

    s.hideSimilar.connect(
        []() {
            getApp()->getWindows()->forceLayoutChannelViews();
        },
        false);
    layout.addDropdown<float>(
        "حد التشابه", {"0.5", "0.75", "0.9"}, s.similarityPercentage,
        [](auto val) {
            return QString::number(val);
        },
        [](auto args) {
            return fuzzyToFloat(args.value, 0.9F);
        },
        true,
        "A value of 0.9 means the messages need to be 90% similar to be marked "
        "as similar.");
    layout.addDropdown<int>(
        "أقصى مدة بين الرسائل",
        {"5s", "10s", "15s", "30s", "60s", "120s"}, s.hideSimilarMaxDelay,
        [](auto val) {
            return QString::number(val) + "s";
        },
        [](auto args) {
            return fuzzyToInt(args.value, 5);
        },
        true,
        "A value of 5s means if there's a 5s break between messages, we will "
        "stop looking further through the messages for similarities.");
    layout.addDropdown<int>(
        "عدد الرسائل السابقة للفحص", {"1", "2", "3", "4", "5"},
        s.hideSimilarMaxMessagesToCheck,
        [](auto val) {
            return QString::number(val);
        },
        [](auto args) {
            return fuzzyToInt(args.value, 3);
        },
        true,
        "How many messages in the history should be compared to a new one to "
        "establish its similarity rating. Messages in the history will be "
        "compared to only if they are new enough.");

    layout.addSubtitle("الشارات الظاهرة");
    SettingWidget::checkbox("الصلاحيات", s.showBadgesGlobalAuthority)
        ->setTooltip("e.g. staff, admin")
        ->addTo(layout);

    SettingWidget::checkbox("التوقعات", s.showBadgesPredictions)
        ->addTo(layout);

    SettingWidget::checkbox("القناة", s.showBadgesChannelAuthority)
        ->setTooltip("e.g. broadcaster, moderator")
        ->addTo(layout);

    SettingWidget::checkbox("المشترك ", s.showBadgesSubscription)
        ->addTo(layout);

    SettingWidget::checkbox("الزينة", s.showBadgesVanity)
        ->setTooltip("e.g. prime, bits, sub gifter")
        ->addTo(layout);

    SettingWidget::checkbox("Chatterino", s.showBadgesChatterino)
        ->setTooltip("e.g. Chatterino Supporter/Contributor/Developer")
        ->addTo(layout);

    SettingWidget::checkbox("FrankerFaceZ", s.showBadgesFfz)
        ->addKeywords({"ffz"})
        ->setTooltip("e.g. Bot, FrankerFaceZ supporter, FrankerFaceZ developer")
        ->addTo(layout);
    SettingWidget::checkbox("7TV", s.showBadgesSevenTV)
        ->addKeywords({"seventv"})
        ->setTooltip("شارات مشرفي ومطوري وداعمي 7TV")
        ->addTo(layout);
    SettingWidget::checkbox("BetterTTV", s.showBadgesBttv)
        ->addKeywords({"bttv"})
        ->addTo(layout);
    layout.addSeparator();
    SettingWidget::checkbox("استخدام شارات مشرفين مخصصة من FrankerFaceZ",
                            s.useCustomFfzModeratorBadges)
        ->addKeywords({"ffz"})
        ->addTo(layout);
    SettingWidget::checkbox("استخدام شارات VIP مخصصة من FrankerFaceZ",
                            s.useCustomFfzVipBadges)
        ->addKeywords({"ffz"})
        ->addTo(layout);

    layout.addSubtitle("النافذة الطافية");
    layout.addDropdown<float>(
        "معامل التكبير", ZOOM_LEVELS, s.overlayScaleFactor,
        [](auto val) {
            if (val == 1)
            {
                return u"Default"_s;
            }
            return QString::number(val) + 'x';
        },
        [](const auto &args) {
            return fuzzyToFloat(args.value, 1.F);
        },
        true,
        "The final scale of the messages in the overlay is computed by "
        "multiplying this zoom factor with the global zoom level.");

    SettingWidget::intInput("شفافية الخلفية (0-255)",
                            s.overlayBackgroundOpacity,
                            {
                                .min = 0,
                                .max = 255,
                                .singleStep = 1,
                            })
        ->setTooltip(
            "يتحكم في شفافية الخلفية خلف الرسائل. اللون يُحدد من الثيم الحالي. 255 تعني خلفية معتمة بالكامل.")
        ->addTo(layout);

    SettingWidget::checkbox("تفعيل الظل", s.enableOverlayShadow)
        ->setTooltip("يضيف ظل للنافذة الطافية. يستهلك معالجة أكثر.")
        ->addTo(layout);

    SettingWidget::intInput("شفافية الظل (0-255)", s.overlayShadowOpacity,
                            {
                                .min = 0,
                                .max = 255,
                                .singleStep = 1,
                            })
        ->setTooltip("يتحكم في شفافية الظل. 255 تعني ظل معتم بالكامل.")
        ->addTo(layout);

    SettingWidget::colorButton("لون الظل", s.overlayShadowColor)
        ->addTo(layout);

    SettingWidget::intInput("نصف قطر الظل", s.overlayShadowRadius,
                            {
                                .min = 0,
                                .max = 40,
                                .singleStep = 1,
                                .suffix = "dp",
                            })
        ->setTooltip("يتحكم في انتشار الظل (نصف قطر التمويه) بالبكسل.")
        ->addTo(layout);

    SettingWidget::intInput("إزاحة الظل أفقيًا", s.overlayShadowOffsetX,
                            {
                                .min = -20,
                                .max = 20,
                                .singleStep = 1,
                                .suffix = "dp",
                            })
        ->setTooltip("يتحكم في إزاحة الظل أفقيًا بالبكسل. القيمة السالبة لليسار والموجبة لليمين.")
        ->addTo(layout);

    SettingWidget::intInput("إزاحة الظل عموديًا", s.overlayShadowOffsetY,
                            {
                                .min = -20,
                                .max = 20,
                                .singleStep = 1,
                                .suffix = "dp",
                            })
        ->setTooltip("يتحكم في إزاحة الظل عموديًا بالبكسل. القيمة السالبة للأعلى والموجبة للأسفل.")
        ->addTo(layout);

    {
        layout.addSubtitle("البحث");
        layout.addDescription(
            "محرك البحث اللي يظهر لما تحدد نص وتضغط يمين على رسالة. اختر محرك جاهز من القائمة تحت، أو اكتب رابط واسم محرك بحث مخصص.");
        SettingWidget::checkbox("تفعيل البحث في قائمة الزر اليمين",
                                s.searchEnabled)
            ->setTooltip(
                "السماح بالبحث عن النص المحدد بمحرك بحث من قائمة الزر اليمين.")
            ->addTo(layout);

        // Preset dropdown
        QStringList presetList = {"DuckDuckGo", "Bing", "Google"};
        auto *presetCombo =
            layout.addDropdown("محرك بحث جاهز", presetList,
                               "اختر محرك بحث جاهز");
        presetCombo->setPlaceholderText("اختر...");
        presetCombo->setCurrentIndex(-1);
        // Make placeholder text more visible
        QPalette palette = presetCombo->palette();
        palette.setColor(QPalette::PlaceholderText,
                         QColor(255, 255, 255));  // white
        presetCombo->setPalette(palette);
        s.searchEnabled.connect([presetCombo](bool value) {
            presetCombo->setEnabled(value);
        });

        // Connect preset dropdown to update URL and name settings
        QObject::connect(
            presetCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            [&s, presetCombo](int index) {
                if (index < 0)
                    return;

                QString preset = presetCombo->itemText(index);
                if (preset == "DuckDuckGo")
                {
                    s.searchEngineUrl = "https://duckduckgo.com/?q=";
                    s.searchEngineName = "DuckDuckGo";
                }
                else if (preset == "Bing")
                {
                    s.searchEngineUrl = "https://www.bing.com/search?q=";
                    s.searchEngineName = "Bing";
                }
                else if (preset == "Google")
                {
                    s.searchEngineUrl = "https://www.google.com/search?q=";
                    s.searchEngineName = "Google";
                }
                // Reset to -1 after selection
                {
                    QSignalBlocker blocker(presetCombo);
                    presetCombo->setCurrentIndex(-1);
                }
            });

        // URL and Name text inputs
        SettingWidget::lineEdit("رابط محرك البحث", s.searchEngineUrl)
            ->conditionallyEnabledBy(s.searchEnabled)
            ->addTo(layout);

        SettingWidget::lineEdit("اسم محرك البحث", s.searchEngineName)
            ->conditionallyEnabledBy(s.searchEnabled)
            ->addTo(layout);
    }
    if (supportsIncognitoLinks())
    {
        SettingWidget::checkbox("البحث في وضع التصفح الخفي",
                                s.searchIncognito)
            ->addTo(layout);
    }

    layout.addSubtitle("متفرقات");

    if (supportsIncognitoLinks())
    {
        SettingWidget::checkbox("فتح الروابط في وضع التصفح الخفي",
                                s.openLinksIncognito)
            ->addTo(layout);
    }

    SettingWidget::customCheckbox(
        "إعادة التشغيل عند الانهيار (يتطلب إعادة تشغيل)",
        getApp()->getCrashHandler()->shouldRecover(),
        [](bool on) {
            getApp()->getCrashHandler()->saveShouldRecover(on);
        })
        ->setTooltip("إعادة تشغيل Chatterino تلقائيًا إذا انهار (إذا أمكن)")
        ->addTo(layout);

#if defined(Q_OS_LINUX) && !defined(NO_QTKEYCHAIN)
    if (!getApp()->getPaths().isPortable())
    {
        SettingWidget::checkbox(
            "استخدام libsecret/KWallet/Gnome keychain لحماية كلمات المرور",
            s.useKeyring)
            ->addTo(layout);
    }
#endif

    SettingWidget::inverseCheckbox("إظهار رسائل الإشراف",
                                   s.hideModerationActions)
        ->setTooltip(
            "إظهار رسائل الطرد المؤقت والحظر وباقي إجراءات المشرفين.")
        ->addTo(layout);

    SettingWidget::inverseCheckbox("إظهار حذف الرسائل المفردة",
                                   s.hideDeletionActions)
        ->setTooltip("إظهار متى انحذفت رسالة وحدة.\nمثل: انحذفت رسالة من TreuKS: abc")
        ->addTo(layout);

    SettingWidget::checkbox("تلوين المستخدمين اللي ما عندهم لون (الأسماء الرمادية)",
                            s.colorizeNicknames)
        ->setTooltip("إعطاء لون عشوائي للمستخدمين اللي ما اختاروا لون")
        ->addTo(layout);

    SettingWidget::checkbox("الإشارة للمستخدمين مع فاصلة",
                            s.mentionUsersWithComma)
        ->setTooltip("عند الإكمال بـ Tab، إذا اسم المستخدم في بداية الرسالة يضيف فاصلة بعده.\nمثل: pajl -> pajlada,")
        ->addTo(layout);

    SettingWidget::checkbox("إظهار المنضمين (أقل من 1000 متواجد)", s.showJoins)
        ->setTooltip(
            "إظهار رسالة نظام من Twitch بالمستخدمين اللي دخلوا الشات، متاحة بس إذا الشات فيه أقل من 1000")
        ->addTo(layout);

    SettingWidget::checkbox("إظهار المغادرين (أقل من 1000 متواجد)", s.showParts)
        ->setTooltip(
            "إظهار رسالة نظام من Twitch بالمستخدمين اللي طلعوا من الشات، متاحة بس إذا الشات فيه أقل من 1000")
        ->addTo(layout);

    SettingWidget::checkbox(
        "إغلاق بطاقة المستخدم تلقائيًا لما تفقد التركيز",
        s.autoCloseUserPopup)
        ->addTo(layout);

    SettingWidget::checkbox(
        "إغلاق نافذة المحادثة تلقائيًا لما تفقد التركيز",
        s.autoCloseThreadPopup)
        ->addTo(layout);

    SettingWidget::checkbox("تصغير أحرف النطاقات (ضد التصيّد)",
                            s.lowercaseDomains)
        ->setTooltip(
            "تحويل كل الروابط لأحرف صغيرة لتصعيب محاولات التصيّد.")
        ->addTo(layout);

    SettingWidget::checkbox("إظهار ضمائر المستخدم في البطاقة", s.showPronouns)
        ->setDescription(
            R"(Pronouns are retrieved from <a href="https://pr.alejo.io">pr.alejo.io</a> when a user card is opened.)")
        ->addTo(layout);

    SettingWidget::checkbox("إظهار عنوان البث في رسالة البث المباشر",
                            s.showTitleInLiveMessage)
        ->setTooltip("العنوان في الرسالة هو اللي حطه الستريمر لما بدأ البث، وما يتحدث لو غيّره.")
        ->addTo(layout);

    SettingWidget::checkbox("@أسماء بخط عريض", s.boldUsernames)
        ->setTooltip("جعل الإشارات @ بخط عريض عشان تبان أكثر.")
        ->addTo(layout);

    SettingWidget::checkbox("تلوين @الأسماء", s.colorUsernames)
        ->setTooltip("إذا Chatterino شاف المستخدم قبل، يلوّن الإشارة له بلونه في Twitch.")
        ->addTo(layout);

    SettingWidget::checkbox("محاولة اكتشاف الأسماء بدون @",
                            s.findAllUsernames)
        ->setTooltip("اكتشاف الإشارات للمستخدمين في الشات حتى بدون @.")
        ->addTo(layout);

    SettingWidget::checkbox("إظهار قائمة إكمال أسماء المستخدمين",
                            s.showUsernameCompletionMenu)
        ->addTo(layout);

    SettingWidget::checkbox("تضمين صاحب القناة دائمًا في إكمال الأسماء",
                            s.alwaysIncludeBroadcasterInUserCompletions)
        ->setTooltip(
            "هذا يضمن إنك تقدر تذكر صاحب القناة بسهولة، حتى لو ما كان فاتح الشات أو ما كتب من فترة.")
        ->addTo(layout);

    const QStringList usernameDisplayModes = {"Username", "Localized name",
                                              "Username and localized name"};

    ComboBox *nameDropdown =
        layout.addDropdown<std::underlying_type_t<UsernameDisplayMode>>(
            "شكل اسم المستخدم", usernameDisplayModes, s.usernameDisplayMode,
            [usernameDisplayModes](auto val) {
                return usernameDisplayModes.at(val - 1);
                // UsernameDisplayMode enum indexes from 1
            },
            [](auto args) {
                return args.index + 1;
            },
            false,
            "Customizes how you see Asian Language names.\nUsing an option "
            "that includes \"localized\" will display the username in it's "
            "respective Asian language.\ne.g. "
            "Username and localized: testaccount_420(테스트계정420)\n"
            "Username: testaccount_420\n"
            "Localized name: 테스트계정420");
    nameDropdown->setMinimumWidth(nameDropdown->minimumSizeHint().width());

    layout.addDropdown<float>(
        "سماكة خط اسم المستخدم", {"50", "Default", "75", "100"}, s.boldScale,
        [](auto val) {
            if (val == 63)
            {
                return QString("Default");
            }
            else
            {
                return QString::number(val);
            }
        },
        [](auto args) {
            return fuzzyToFloat(args.value, 63.f);
        });

    SettingWidget::checkbox(
        "نقرتين لفتح الروابط والعناصر في الشات",
        s.linksDoubleClickOnly)
        ->setTooltip("إذا فعّلته، فتح الروابط وبطاقات المستخدمين يحتاج نقرتين.\nمفيد عشان ما تضغط على رابط مشبوه بالغلط.")
        ->addTo(layout);

    SettingWidget::checkbox("فك اختصار الروابط", s.unshortLinks)
        ->setTooltip("إذا فعّلته، \"زر يمين + نسخ الرابط\" بينسخ الرابط الأصلي بدون اختصار.\nمثل: https://bit.ly/mrfors -> https://forsen.tv/")
        ->addTo(layout);

    SettingWidget::checkbox(
        "إكمال الإيموتات من بداية الاسم فقط",
        s.prefixOnlyEmoteCompletion)
        ->setTooltip("إذا عطّلته، إكمال الإيموتات بـ Tab يطابق أي جزء من الاسم.\nمثل: sheffy -> DatSheffy")
        ->addTo(layout);

    SettingWidget::checkbox("إكمال أسماء المستخدمين مع @ فقط",
                            s.userCompletionOnlyWithAt)
        ->setTooltip("إذا فعّلته، إكمال الأسماء بـ Tab يشتغل بس مع @\nمثل: pajl -> pajl | @pajl -> @pajlada")
        ->addTo(layout);

    SettingWidget::checkbox("إظهار همسات Twitch داخل الشات", s.inlineWhispers)
        ->setTooltip("إظهار الهمسات كرسائل في كل الأقسام بدل /whispers بس.")
        ->addTo(layout);

    SettingWidget::checkbox("تمييز الهمسات داخل الشات",
                            s.highlightInlineWhispers)
        ->setTooltip(
            "تمييز الهمسات الظاهرة في كل الأقسام.\nإذا \"إظهار همسات Twitch داخل الشات\" معطّل، هذا الإعداد ما يسوي شي.")
        ->addTo(layout);

    SettingWidget::checkbox(
        "الاشتراك تلقائيًا في المحادثات اللي تشارك فيها",
        s.autoSubToParticipatedThreads)
        ->setTooltip(
            "إذا فعّلته، بتشترك تلقائيًا في المحادثات اللي ترد فيها.\nيعني بتستخدم إعدادات تمييز \"المحادثات المشترك فيها\".")
        ->addTo(layout);

    SettingWidget::checkbox("تحميل سجل الرسائل عند الاتصال",
                            s.loadTwitchMessageHistoryOnConnect)
        ->addTo(layout);

    // TODO: Change phrasing to use better english once we can tag settings, right now it's kept as history instead of historical so that the setting shows up when the user searches for history
    SettingWidget::intInput("أقصى عدد رسائل من السجل عند الاتصال",
                            s.twitchMessageHistoryLimit,
                            {
                                .min = 10,
                                .max = 800,
                                .singleStep = 10,
                            })
        ->addTo(layout);

    SettingWidget::intInput("حد الرسائل المحفوظة في القسم (يتطلب إعادة تشغيل)",
                            s.scrollbackSplitLimit,
                            {
                                .min = 100,
                                .max = 100000,
                                .singleStep = 100,
                            })
        ->addTo(layout);

    SettingWidget::intInput("حد الرسائل في بطاقة المستخدم (يتطلب إعادة تشغيل)",
                            s.scrollbackUsercardLimit,
                            {
                                .min = 100,
                                .max = 100000,
                                .singleStep = 100,
                            })
        ->addTo(layout);

    SettingWidget::dropdown("إظهار رسائل AutoMod للكلمات المحظورة",
                            s.showBlockedTermAutomodMessages)
        ->setTooltip("إظهار الرسائل اللي حجبها AutoMod لأنها فيها كلمة محظورة في القناة الحالية.")
        ->addTo(layout);

    layout.addDropdown<int>(
        "تجميع الطرد المؤقت", {"Stack", "Stack until timeout", "Don't stack"},
        s.timeoutStackStyle,
        [](int index) {
            return index;
        },
        [](auto args) {
            return args.index;
        },
        false, "Combine consecutive timeout messages into a single message.");

    SettingWidget::checkbox("دمج عدة Bits في وحدة", s.stackBits)
        ->setTooltip("دمج الـ cheermotes المتتالية (في نفس الرسالة) في وحدة.")
        ->addTo(layout);

    // update this tooltip if https://github.com/Chatterino/chatterino2/pull/1557 is ever merged
    SettingWidget::checkbox("الرسائل في تبويب /mentions",
                            s.highlightMentions)
        ->setTooltip("إذا عطّلته، تبويب /mentions ما يتلوّن بالأحمر لما أحد يذكرك.")
        ->addTo(layout);

    SettingWidget::checkbox("حذف الإشارة في بداية الردود",
                            s.stripReplyMention)
        ->setTooltip(
            "إذا عطّلته، الردود في المحادثات بتتضمن الإشارة @ لصاحب المحادثة. إذا سياق الرد مخفي، الإشارة ما تنحذف أبدًا.")
        ->addTo(layout);

    SettingWidget::dropdown("طريقة إرسال الشات", s.chatSendProtocol)
        ->setTooltip("'Helix' will use Twitch's Helix API to send message. "
                     "'IRC' will use IRC to send messages.")
        ->addTo(layout);

    SettingWidget::checkbox("إظهار زر الإرسال", s.showSendButton)
        ->setTooltip("إظهار زر إرسال جنب خانة الكتابة في كل قسم")
        ->addTo(layout);

    SettingWidget::checkbox(
        "تفعيل دعم Twitch EventSub التجريبي (يتطلب إعادة تشغيل)",
        s.enableExperimentalEventSub)
        ->addTo(layout);

    SettingWidget::checkbox("تعطيل إعادة تسمية التبويبات بالنقر مرتين",
                            s.disableTabRenamingOnClick)
        ->setTooltip("يمنع نافذة إعادة التسمية من الظهور لما تنقر مرتين على تبويب")
        ->addTo(layout);

    layout.addStretch();

    // invisible element for width
    auto *inv = new BaseWidget(this);
    //    inv->setScaleIndependentWidth(600);
    layout.addWidget(inv);
}

void GeneralPage::initExtra()
{
    /// update cache path
    if (this->cachePath_)
    {
        getSettings()->cachePath.connect(
            [cachePath = this->cachePath_](const auto &, auto) mutable {
                QString newPath = getApp()->getPaths().cacheDirectory();

                QString pathShortened = "Current location: <a href=\"file:///" +
                                        newPath + "\">" +
                                        shortenString(newPath, 50) + "</a>";

                cachePath->setText(pathShortened);
                cachePath->setToolTip(newPath);
            });
    }
}

}  // namespace chatterino
