// SPDX-FileCopyrightText: 2023 Contributors to Chatterino <https://chatterino.com>
//
// SPDX-License-Identifier: MIT

#include "controllers/commands/builtin/Misc.hpp"

#include "Application.hpp"
#include "common/Channel.hpp"
#include "controllers/accounts/AccountController.hpp"
#include "controllers/commands/CommandContext.hpp"
#include "controllers/userdata/UserDataController.hpp"
#include "providers/twitch/api/Helix.hpp"
#include "providers/twitch/TwitchAccount.hpp"
#include "providers/twitch/TwitchChannel.hpp"
#include "providers/twitch/TwitchIrcServer.hpp"
#include "singletons/Settings.hpp"
#include "singletons/WindowManager.hpp"
#include "util/Clipboard.hpp"
#include "util/FormatTime.hpp"
#include "util/IncognitoBrowser.hpp"
#include "util/StreamLink.hpp"
#include "util/Twitch.hpp"
#include "widgets/dialogs/UserInfoPopup.hpp"
#include "widgets/helper/ChannelView.hpp"
#include "widgets/Notebook.hpp"
#include "widgets/splits/Split.hpp"
#include "widgets/splits/SplitContainer.hpp"
#include "widgets/Window.hpp"

#include <QCommandLineParser>
#include <QDesktopServices>
#include <QString>
#include <QUrl>

#include <optional>

namespace chatterino::commands {

QString follow(const CommandContext &ctx)
{
    if (ctx.twitchChannel == nullptr)
    {
        return "";
    }
    ctx.channel->addSystemMessage(
        "Twitch شال خاصية المتابعة من البرامج الخارجية. للمزيد: https://github.com/Chatterino/chatterino2/issues/3076");
    return "";
}

QString unfollow(const CommandContext &ctx)
{
    if (ctx.twitchChannel == nullptr)
    {
        return "";
    }
    ctx.channel->addSystemMessage(
        "Twitch شال خاصية إلغاء المتابعة من البرامج الخارجية. للمزيد: https://github.com/Chatterino/chatterino2/issues/3076");
    return "";
}

QString uptime(const CommandContext &ctx)
{
    if (ctx.channel == nullptr)
    {
        return "";
    }

    if (ctx.twitchChannel == nullptr)
    {
        ctx.channel->addSystemMessage(
            "الأمر /uptime يشتغل بس في قنوات Twitch.");
        return "";
    }

    const auto &streamStatus = ctx.twitchChannel->accessStreamStatus();

    QString messageText =
        streamStatus->live ? streamStatus->uptime : "القناة مو في بث مباشر.";

    ctx.channel->addSystemMessage(messageText);

    return "";
}

QString user(const CommandContext &ctx)
{
    if (ctx.channel == nullptr)
    {
        return "";
    }

    if (ctx.words.size() < 2)
    {
        ctx.channel->addSystemMessage("الاستخدام: /user <المستخدم> [القناة]");
        return "";
    }
    QString userName = ctx.words[1];
    stripUserName(userName);

    QString channelName = ctx.channel->getName();

    if (ctx.words.size() > 2)
    {
        channelName = ctx.words[2];
        stripChannelName(channelName);
    }
    openTwitchUsercard(channelName, userName);

    return "";
}

QString requests(const CommandContext &ctx)
{
    if (ctx.channel == nullptr)
    {
        return "";
    }

    QString target(ctx.words.value(1));

    if (target.isEmpty())
    {
        if (ctx.channel->getType() == Channel::Type::Twitch &&
            !ctx.channel->isEmpty())
        {
            target = ctx.channel->getName();
        }
        else
        {
            ctx.channel->addSystemMessage(
                "الاستخدام: /requests [القناة]. تقدر تستخدمه بدون قناة لفتح طلبات نقاط القناة الحالية. صاحب القناة والمشرفين بس يقدرون يشوفونها.");
            return "";
        }
    }

    stripChannelName(target);
    QDesktopServices::openUrl(QUrl(
        QString("https://www.twitch.tv/popout/%1/reward-queue").arg(target)));

    return "";
}

QString lowtrust(const CommandContext &ctx)
{
    if (ctx.channel == nullptr)
    {
        return "";
    }

    QString target(ctx.words.value(1));

    if (target.isEmpty())
    {
        if (ctx.channel->getType() == Channel::Type::Twitch &&
            !ctx.channel->isEmpty())
        {
            target = ctx.channel->getName();
        }
        else
        {
            ctx.channel->addSystemMessage(
                "الاستخدام: /lowtrust [القناة]. تقدر تستخدمه بدون قناة لفتح نشاط المستخدمين المشبوهين. صاحب القناة والمشرفين بس يقدرون يشوفونه.");
            return "";
        }
    }

    stripChannelName(target);
    QDesktopServices::openUrl(QUrl(
        QString("https://www.twitch.tv/popout/moderator/%1/low-trust-users")
            .arg(target)));

    return "";
}

QString clip(const CommandContext &ctx)
{
    if (ctx.channel == nullptr)
    {
        return "";
    }

    if (const auto type = ctx.channel->getType();
        type != Channel::Type::Twitch && type != Channel::Type::TwitchWatching)
    {
        ctx.channel->addSystemMessage(
            "الأمر /clip يشتغل بس في قنوات Twitch.");
        return "";
    }

    if (ctx.twitchChannel == nullptr)
    {
        ctx.channel->addSystemMessage(
            "الأمر /clip يشتغل بس في قنوات Twitch.");
        return "";
    }

    QString title = "";
    std::optional<int> duration = std::nullopt;
    if (!ctx.words.empty())
    {
        QCommandLineParser parser;
        parser.setSingleDashWordOptionMode(
            QCommandLineParser::ParseAsLongOptions);
        parser.setOptionsAfterPositionalArgumentsMode(
            QCommandLineParser::ParseAsPositionalArguments);
        parser.addPositionalArgument("title", "The title of the clip");

        QCommandLineOption durationOption(
            {"d", "duration"}, "The duration of the clip", "duration");
        parser.addOptions({
            durationOption,
        });
        parser.parse(ctx.words);

        title = parser.positionalArguments().join(' ');

        if (parser.isSet(durationOption))
        {
            bool ok = false;
            duration = parser.value(durationOption).toInt(&ok);

            if (!ok)
            {
                ctx.channel->addSystemMessage(
                    "تعذّرت قراءة مدة المقطع كرقم.");
                return "";
            }
        }
    }

    ctx.twitchChannel->createClip(title, duration);

    return "";
}

QString marker(const CommandContext &ctx)
{
    if (ctx.channel == nullptr)
    {
        return "";
    }

    if (ctx.twitchChannel == nullptr)
    {
        ctx.channel->addSystemMessage(
            "الأمر /marker يشتغل بس في قنوات Twitch.");
        return "";
    }

    // Avoid Helix calls without Client ID and/or OAuth Token
    if (getApp()->getAccounts()->twitch.getCurrent()->isAnon())
    {
        ctx.channel->addSystemMessage(
            "لازم تسجّل دخول عشان تضيف علامة في البث!");
        return "";
    }

    // Exact same message as in webchat
    if (!ctx.twitchChannel->isLive())
    {
        ctx.channel->addSystemMessage(
            "تقدر تضيف علامات بس أثناء البث المباشر. حاول لما تكون القناة تبث.");
        return "";
    }

    auto arguments = ctx.words;
    arguments.removeFirst();

    getHelix()->createStreamMarker(
        // Limit for description is 140 characters, webchat just crops description
        // if it's >140 characters, so we're doing the same thing
        ctx.twitchChannel->roomId(), arguments.join(" ").left(140),
        [channel{ctx.channel},
         arguments](const HelixStreamMarker &streamMarker) {
            channel->addSystemMessage(
                QString("تمت إضافة علامة في البث عند %1%2")
                    .arg(formatTime(streamMarker.positionSeconds))
                    .arg(streamMarker.description.isEmpty()
                             ? ""
                             : QString(": \"%1\"")
                                   .arg(streamMarker.description)));
        },
        [channel{ctx.channel}](auto error) {
            QString errorMessage("فشل إنشاء علامة البث - ");

            switch (error)
            {
                case HelixStreamMarkerError::UserNotAuthorized: {
                    errorMessage +=
                        "ما عندك صلاحية تسوي هذا الإجراء.";
                }
                break;

                case HelixStreamMarkerError::UserNotAuthenticated: {
                    errorMessage += "لازم تسجّل دخولك من جديد.";
                }
                break;

                // This would most likely happen if the service is down, or if the JSON payload returned has changed format
                case HelixStreamMarkerError::Unknown:
                default: {
                    errorMessage += "صار خطأ غير معروف.";
                }
                break;
            }

            channel->addSystemMessage(errorMessage);
        });

    return "";
}

QString streamlink(const CommandContext &ctx)
{
    if (ctx.channel == nullptr)
    {
        return "";
    }

    QString target(ctx.words.value(1));

    if (target.isEmpty())
    {
        if (ctx.channel->getType() == Channel::Type::Twitch &&
            !ctx.channel->isEmpty())
        {
            target = ctx.channel->getName();
        }
        else
        {
            ctx.channel->addSystemMessage(
                "/streamlink [channel]. Open specified Twitch channel in "
                "streamlink. If no channel argument is specified, open the "
                "current Twitch channel instead.");
            return "";
        }
    }

    stripChannelName(target);
    openStreamlinkForChannel(target);

    return "";
}

QString popout(const CommandContext &ctx)
{
    if (ctx.channel == nullptr)
    {
        return "";
    }

    QString target(ctx.words.value(1));

    if (target.isEmpty())
    {
        if (ctx.channel->getType() == Channel::Type::Twitch &&
            !ctx.channel->isEmpty())
        {
            target = ctx.channel->getName();
        }
        else
        {
            ctx.channel->addSystemMessage(
                "الاستخدام: /popout <القناة>. تقدر تستخدمه بدون قناة لفتح شات القناة الحالية في نافذة منفصلة.");
            return "";
        }
    }

    stripChannelName(target);
    QDesktopServices::openUrl(QUrl(
        QString("https://www.twitch.tv/popout/%1/chat?popout=").arg(target)));

    return "";
}

QString popup(const CommandContext &ctx)
{
    if (ctx.channel == nullptr)
    {
        return "";
    }

    static const auto *usageMessage =
        "Usage: /popup [channel]. Open specified Twitch channel in "
        "a new window. If no channel argument is specified, open "
        "the currently selected split instead.";

    QString target(ctx.words.value(1));
    stripChannelName(target);

    // Popup the current split
    if (target.isEmpty())
    {
        auto *currentPage =
            dynamic_cast<SplitContainer *>(getApp()
                                               ->getWindows()
                                               ->getMainWindow()
                                               .getNotebook()
                                               .getSelectedPage());
        if (currentPage != nullptr)
        {
            auto *currentSplit = currentPage->getSelectedSplit();
            if (currentSplit != nullptr)
            {
                currentSplit->popup();

                return "";
            }
        }

        ctx.channel->addSystemMessage(usageMessage);
        return "";
    }

    // Open channel passed as argument in a popup
    auto targetChannel = getApp()->getTwitch()->getOrAddChannel(target);
    getApp()->getWindows()->openInPopup(targetChannel);

    return "";
}

QString clearmessages(const CommandContext &ctx)
{
    (void)ctx;

    auto *currentPage = getApp()
                            ->getWindows()
                            ->getLastSelectedWindow()
                            ->getNotebook()
                            .getSelectedPage();

    if (auto *split = currentPage->getSelectedSplit())
    {
        split->getChannelView().clearMessages();
    }

    return "";
}

QString openURL(const CommandContext &ctx)
{
    /**
     * The /openurl command
     * Takes a positional argument as the URL to open
     *
     * Accepts the option --private or --no-private (or --incognito or --no-incognito).
     * These options will force the URL to be opened in private or non-private mode, regardless of the
     * default incognito mode setting.
     *
     * Examples:
     *  - /openurl https://twitch.tv/forsen
     *    with the setting "Open links in incognito/private mode" enabled
     *    Opens https://twitch.tv/forsen in private mode
     *  - /openurl https://twitch.tv/forsen
     *    with the setting "Open links in incognito/private mode" disabled
     *    Opens https://twitch.tv/forsen in normal mode
     *  - /openurl https://twitch.tv/forsen --private
     *    with the setting "Open links in incognito/private mode" disabled
     *    Opens https://twitch.tv/forsen in private mode
     *  - /openurl https://twitch.tv/forsen --no-private
     *    with the setting "Open links in incognito/private mode" enabled
     *    Opens https://twitch.tv/forsen in normal mode
     */
    if (ctx.channel == nullptr)
    {
        return "";
    }

    QCommandLineParser parser;
    parser.setOptionsAfterPositionalArgumentsMode(
        QCommandLineParser::ParseAsPositionalArguments);
    parser.addPositionalArgument("URL", "The URL to open");
    QCommandLineOption privateModeOption(
        {
            "private",
            "incognito",
        },
        "Force private mode. Cannot be used together with --no-private");
    QCommandLineOption noPrivateModeOption(
        {
            "no-private",
            "no-incognito",
        },
        "Force non-private mode. Cannot be used together with --private");
    parser.addOptions({
        privateModeOption,
        noPrivateModeOption,
    });
    parser.parse(ctx.words);

    const auto &positionalArguments = parser.positionalArguments();
    if (positionalArguments.isEmpty())
    {
        ctx.channel->addSystemMessage(
            "الاستخدام: /openurl <الرابط> [--incognito/--no-incognito]");
        return "";
    }
    auto urlString = parser.positionalArguments().join(' ');

    QUrl url = QUrl::fromUserInput(urlString);
    if (!url.isValid())
    {
        ctx.channel->addSystemMessage("الرابط غير صالح.");
        return "";
    }

    auto preferPrivateMode = getSettings()->openLinksIncognito.getValue();
    auto forcePrivateMode = parser.isSet(privateModeOption);
    auto forceNonPrivateMode = parser.isSet(noPrivateModeOption);

    if (forcePrivateMode && forceNonPrivateMode)
    {
        ctx.channel->addSystemMessage(
            "خطأ: استخدم --incognito أو --no-incognito، مو الاثنين مع بعض.");
        return "";
    }

    bool usePrivateMode = false;

    if (forceNonPrivateMode)
    {
        usePrivateMode = false;
    }
    else if (supportsIncognitoLinks() &&
             (forcePrivateMode || preferPrivateMode))
    {
        usePrivateMode = true;
    }

    bool res = false;
    if (usePrivateMode)
    {
        res = openLinkIncognito(url.toString(QUrl::FullyEncoded));
    }
    else
    {
        res = QDesktopServices::openUrl(url);
    }

    if (!res)
    {
        ctx.channel->addSystemMessage("تعذّر فتح الرابط.");
    }

    return "";
}

QString sendRawMessage(const CommandContext &ctx)
{
    if (ctx.channel == nullptr)
    {
        return "";
    }

    if (ctx.channel->isTwitchChannel())
    {
        getApp()->getTwitch()->sendRawMessage(ctx.words.mid(1).join(" "));
    }
    else
    {
        // other code down the road handles this for IRC
        return ctx.words.join(" ");
    }
    return "";
}

QString injectFakeMessage(const CommandContext &ctx)
{
    if (ctx.channel == nullptr)
    {
        return "";
    }

    if (!ctx.channel->isTwitchChannel())
    {
        ctx.channel->addSystemMessage(
            "The /fakemsg command only works in Twitch channels.");
        return "";
    }

    if (ctx.words.size() < 2)
    {
        ctx.channel->addSystemMessage(
            "Usage: /fakemsg (raw irc text) - injects raw irc text as "
            "if it was a message received from TMI");
        return "";
    }

    auto ircText = ctx.words.mid(1).join(" ");
    getApp()->getTwitch()->addFakeMessage(ircText);

    return "";
}

QString injectStreamUpdateNoStream(const CommandContext &ctx)
{
    /**
     * /debug-update-to-no-stream makes the current channel mimic going offline
     */
    if (ctx.channel == nullptr)
    {
        return "";
    }
    if (ctx.twitchChannel == nullptr)
    {
        ctx.channel->addSystemMessage(
            "The /debug-update-to-no-stream command only "
            "works in Twitch channels");
        return "";
    }

    ctx.twitchChannel->updateStreamStatus(std::nullopt, false);
    return "";
}

QString copyToClipboard(const CommandContext &ctx)
{
    if (ctx.channel == nullptr)
    {
        return "";
    }

    if (ctx.words.size() < 2)
    {
        ctx.channel->addSystemMessage("الاستخدام: /copy <نص> - ينسخ النص إلى الحافظة.");
        return "";
    }

    crossPlatformCopy(ctx.words.mid(1).join(" "));
    return "";
}

QString unstableSetUserClientSideColor(const CommandContext &ctx)
{
    if (ctx.channel == nullptr)
    {
        return "";
    }

    if (ctx.twitchChannel == nullptr)
    {
        ctx.channel->addSystemMessage(
            "The /unstable-set-user-color command only "
            "works in Twitch channels.");
        return "";
    }
    if (ctx.words.size() < 2)
    {
        ctx.channel->addSystemMessage(
            QString("Usage: %1 <TwitchUserID> [color]").arg(ctx.words.at(0)));
        return "";
    }

    auto userID = ctx.words.at(1);

    auto color = ctx.words.value(2);

    getApp()->getUserData()->setUserColor(userID, color);

    return "";
}

QString openUsercard(const CommandContext &ctx)
{
    auto channel = ctx.channel;

    if (channel == nullptr)
    {
        return "";
    }

    if (ctx.words.size() < 2)
    {
        channel->addSystemMessage("الاستخدام: /usercard <المستخدم> [القناة] أو /usercard id:<المعرّف> [القناة]");
        return "";
    }

    QString userName = ctx.words[1];
    stripUserName(userName);

    if (ctx.words.size() > 2)
    {
        QString channelName = ctx.words[2];
        stripChannelName(channelName);

        ChannelPtr channelTemp =
            getApp()->getTwitch()->getChannelOrEmpty(channelName);

        if (channelTemp->isEmpty())
        {
            channel->addSystemMessage(
                "بطاقة المستخدم تنعرض بس لقناة مفتوحة حاليًا في Chatterino.");
            return "";
        }

        channel = channelTemp;
    }

    // try to link to current split if possible
    Split *currentSplit = nullptr;
    auto *currentPage = dynamic_cast<SplitContainer *>(getApp()
                                                           ->getWindows()
                                                           ->getMainWindow()
                                                           .getNotebook()
                                                           .getSelectedPage());
    if (currentPage != nullptr)
    {
        currentSplit = currentPage->getSelectedSplit();
    }

    auto differentChannel =
        currentSplit != nullptr && currentSplit->getChannel() != channel;
    if (differentChannel || currentSplit == nullptr)
    {
        // not possible to use current split, try searching for one
        const auto &notebook =
            getApp()->getWindows()->getMainWindow().getNotebook();
        auto count = notebook.getPageCount();
        for (int i = 0; i < count; i++)
        {
            auto *page = notebook.getPageAt(i);
            auto *container = dynamic_cast<SplitContainer *>(page);
            assert(container != nullptr);
            for (auto *split : container->getSplits())
            {
                if (split->getChannel() == channel)
                {
                    currentSplit = split;
                    break;
                }
            }
        }

        // This would have crashed either way.
        assert(currentSplit != nullptr &&
               "something went HORRIBLY wrong with the /usercard "
               "command. It couldn't find a split for a channel which "
               "should be open.");
    }

    auto *userPopup =
        new UserInfoPopup(getSettings()->autoCloseUserPopup, currentSplit);
    userPopup->setData(userName, channel);
    userPopup->moveTo(QCursor::pos(), widgets::BoundsChecking::CursorPosition);
    userPopup->show();
    return "";
}

}  // namespace chatterino::commands
