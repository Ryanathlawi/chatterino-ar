// SPDX-FileCopyrightText: 2023 Contributors to Chatterino <https://chatterino.com>
//
// SPDX-License-Identifier: MIT

#include "controllers/commands/builtin/twitch/Ban.hpp"

#include "Application.hpp"
#include "common/QLogging.hpp"
#include "controllers/accounts/AccountController.hpp"
#include "controllers/commands/CommandContext.hpp"
#include "controllers/commands/common/ChannelAction.hpp"
#include "providers/twitch/api/Helix.hpp"
#include "providers/twitch/TwitchAccount.hpp"
#include "providers/twitch/TwitchChannel.hpp"

namespace {

using namespace chatterino;

QString formatBanTimeoutError(const char *operation, HelixBanUserError error,
                              const QString &message, const QString &userTarget)
{
    using Error = HelixBanUserError;

    QString errorMessage = QString("فشل تنفيذ %1 على المستخدم - ").arg(operation);

    switch (error)
    {
        case Error::ConflictingOperation: {
            errorMessage += "فيه عملية حظر ثانية متعارضة على هذا المستخدم. حاول مرة ثانية.";
        }
        break;

        case Error::Forwarded: {
            errorMessage += message;
        }
        break;

        case Error::Ratelimited: {
            errorMessage += "Twitch حاط عليك حد مؤقت. حاول بعد ثواني.";
        }
        break;

        case Error::TargetBanned: {
            // Equivalent IRC error
            errorMessage += QString("%1 محظور في هذي القناة من قبل.")
                                .arg(userTarget);
        }
        break;

        case Error::CannotBanUser: {
            // We can't provide the identical error as in IRC,
            // because we don't have enough information about the user.
            // The messages from IRC are formatted like this:
            // "You cannot {op} moderator {mod} unless you are the owner of this channel."
            // "You cannot {op} the broadcaster."
            errorMessage +=
                QString("ما تقدر تسوي %1 على %2.").arg(operation, userTarget);
        }
        break;

        case Error::UserMissingScope: {
            // TODO(pajlada): Phrase MISSING_REQUIRED_SCOPE
            errorMessage += "صلاحية ناقصة. سجّل دخولك من جديد وحاول مرة ثانية.";
        }
        break;

        case Error::UserNotAuthorized: {
            // TODO(pajlada): Phrase MISSING_PERMISSION
            errorMessage += "ما عندك صلاحية تسوي هذا الإجراء.";
        }
        break;

        case Error::Unknown: {
            errorMessage += "صار خطأ غير معروف.";
        }
        break;
    }
    return errorMessage;
}

void banUserByID(const ChannelPtr &channel, const QString &channelID,
                 const QString &sourceUserID, const QString &targetUserID,
                 const QString &reason, const QString &displayName)
{
    getHelix()->banUser(
        channelID, sourceUserID, targetUserID, std::nullopt, reason,
        [] {
            // No response for bans, they're emitted over pubsub/IRC instead
        },
        [channel, displayName](auto error, auto message) {
            auto errorMessage =
                formatBanTimeoutError("ban", error, message, displayName);
            channel->addSystemMessage(errorMessage);
        });
}

void timeoutUserByID(const ChannelPtr &channel, const QString &channelID,
                     const QString &sourceUserID, const QString &targetUserID,
                     int duration, const QString &reason,
                     const QString &displayName)
{
    getHelix()->banUser(
        channelID, sourceUserID, targetUserID, duration, reason,
        [] {
            // No response for timeouts, they're emitted over pubsub/IRC instead
        },
        [channel, displayName](auto error, auto message) {
            auto errorMessage =
                formatBanTimeoutError("timeout", error, message, displayName);
            channel->addSystemMessage(errorMessage);
        });
}

}  // namespace

namespace chatterino::commands {

QString sendBan(const CommandContext &ctx)
{
    const auto command = QStringLiteral("/ban");
    const auto usage = QStringLiteral(
        R"(Usage: "/ban [options...] <username> [reason]" - Permanently prevent a user from chatting via their username. Reason is optional and will be shown to the target user and other moderators. Options: --channel <channel> to override which channel the ban takes place in (can be specified multiple times).)");
    const auto actions = parseChannelAction(ctx, command, usage, false, true);

    if (!actions.has_value())
    {
        if (ctx.channel != nullptr)
        {
            ctx.channel->addSystemMessage(actions.error());
        }
        else
        {
            qCWarning(chatterinoCommands)
                << "Error parsing command:" << actions.error();
        }

        return "";
    }

    assert(!actions.value().empty());

    auto currentUser = getApp()->getAccounts()->twitch.getCurrent();
    if (currentUser->isAnon())
    {
        ctx.channel->addSystemMessage("لازم تسجّل دخول عشان تحظر أحد!");
        return "";
    }

    for (const auto &action : actions.value())
    {
        const auto &reason = action.reason;

        QStringList userLoginsToFetch;
        QStringList userIDs;
        if (action.target.id.isEmpty())
        {
            assert(!action.target.login.isEmpty() &&
                   "Ban Action target username AND user ID may not be "
                   "empty at the same time");
            userLoginsToFetch.append(action.target.login);
        }
        else
        {
            // For hydration
            userIDs.append(action.target.id);
        }
        if (action.channel.id.isEmpty())
        {
            assert(!action.channel.login.isEmpty() &&
                   "Ban Action channel username AND user ID may not be "
                   "empty at the same time");
            userLoginsToFetch.append(action.channel.login);
        }
        else
        {
            // For hydration
            userIDs.append(action.channel.id);
        }

        if (!userLoginsToFetch.isEmpty())
        {
            // At least 1 user ID needs to be resolved before we can take action
            // userIDs is filled up with the data we already have to hydrate the action channel & action target
            getHelix()->fetchUsers(
                userIDs, userLoginsToFetch,
                [channel{ctx.channel}, actionChannel{action.channel},
                 actionTarget{action.target}, currentUser, reason,
                 userLoginsToFetch](const auto &users) mutable {
                    if (!actionChannel.hydrateFrom(users))
                    {
                        channel->addSystemMessage(
                            QString("فشل الحظر، اسم القناة غلط: %1")
                                .arg(actionChannel.login));
                        return;
                    }
                    if (!actionTarget.hydrateFrom(users))
                    {
                        channel->addSystemMessage(
                            QString("فشل الحظر، اسم المستخدم غلط: %1")
                                .arg(actionTarget.login));
                        return;
                    }

                    banUserByID(channel, actionChannel.id,
                                currentUser->getUserId(), actionTarget.id,
                                reason, actionTarget.displayName);
                },
                [channel{ctx.channel}, userLoginsToFetch] {
                    channel->addSystemMessage(
                        QString("فشل الحظر، أسماء غلط: %1")
                            .arg(userLoginsToFetch.join(", ")));
                });
        }
        else
        {
            // If both IDs are available, we do no hydration & just use the id as the display name
            banUserByID(ctx.channel, action.channel.id,
                        currentUser->getUserId(), action.target.id, reason,
                        action.target.id);
        }
    }

    return "";
}

QString sendBanById(const CommandContext &ctx)
{
    const auto &words = ctx.words;
    const auto &channel = ctx.channel;
    const auto *twitchChannel = ctx.twitchChannel;

    if (channel == nullptr)
    {
        return "";
    }
    if (twitchChannel == nullptr)
    {
        channel->addSystemMessage(
            "الأمر /banid يشتغل بس في قنوات Twitch.");
        return "";
    }

    const auto *usageStr =
        "الاستخدام: \"/banid <معرّف المستخدم> [السبب]\" - يمنع المستخدم من الكتابة نهائيًا عن طريق معرّفه. السبب اختياري ويظهر للمستخدم والمشرفين.";
    if (words.size() < 2)
    {
        channel->addSystemMessage(usageStr);
        return "";
    }

    auto currentUser = getApp()->getAccounts()->twitch.getCurrent();
    if (currentUser->isAnon())
    {
        channel->addSystemMessage("لازم تسجّل دخول عشان تحظر أحد!");
        return "";
    }

    auto target = words.at(1);
    auto reason = words.mid(2).join(' ');

    banUserByID(channel, twitchChannel->roomId(), currentUser->getUserId(),
                target, reason, target);

    return "";
}

QString sendTimeout(const CommandContext &ctx)
{
    const auto command = QStringLiteral("/timeout");
    const auto usage = QStringLiteral(
        R"(Usage: "/timeout [options...] <username> [duration][time unit] [reason]" - Temporarily prevent a user from chatting. Duration (optional, default=10 minutes) must be a positive integer; time unit (optional, default=s) must be one of s, m, h, d, w; maximum duration is 2 weeks. Combinations like 1d2h are also allowed. Reason is optional and will be shown to the target user and other moderators. Use "/untimeout" to remove a timeout. Options: --channel <channel> to override which channel the timeout takes place in (can be specified multiple times).)");
    const auto actions = parseChannelAction(ctx, command, usage, true, true);

    if (!actions.has_value())
    {
        if (ctx.channel != nullptr)
        {
            ctx.channel->addSystemMessage(actions.error());
        }
        else
        {
            qCWarning(chatterinoCommands)
                << "Error parsing command:" << actions.error();
        }

        return "";
    }

    assert(!actions.value().empty());

    auto currentUser = getApp()->getAccounts()->twitch.getCurrent();
    if (currentUser->isAnon())
    {
        ctx.channel->addSystemMessage(
            "لازم تسجّل دخول عشان تطرد أحد مؤقتًا!");
        return "";
    }

    for (const auto &action : actions.value())
    {
        const auto &reason = action.reason;

        QStringList userLoginsToFetch;
        QStringList userIDs;
        if (action.target.id.isEmpty())
        {
            assert(!action.target.login.isEmpty() &&
                   "Timeout Action target username AND user ID may not be "
                   "empty at the same time");
            userLoginsToFetch.append(action.target.login);
        }
        else
        {
            // For hydration
            userIDs.append(action.target.id);
        }
        if (action.channel.id.isEmpty())
        {
            assert(!action.channel.login.isEmpty() &&
                   "Timeout Action channel username AND user ID may not be "
                   "empty at the same time");
            userLoginsToFetch.append(action.channel.login);
        }
        else
        {
            // For hydration
            userIDs.append(action.channel.id);
        }

        if (!userLoginsToFetch.isEmpty())
        {
            // At least 1 user ID needs to be resolved before we can take action
            // userIDs is filled up with the data we already have to hydrate the action channel & action target
            getHelix()->fetchUsers(
                userIDs, userLoginsToFetch,
                [channel{ctx.channel}, duration{action.duration},
                 actionChannel{action.channel}, actionTarget{action.target},
                 currentUser, reason,
                 userLoginsToFetch](const auto &users) mutable {
                    if (!actionChannel.hydrateFrom(users))
                    {
                        channel->addSystemMessage(
                            QString("فشل الطرد المؤقت، اسم القناة غلط: %1")
                                .arg(actionChannel.login));
                        return;
                    }
                    if (!actionTarget.hydrateFrom(users))
                    {
                        channel->addSystemMessage(
                            QString("فشل الطرد المؤقت، اسم المستخدم غلط: %1")
                                .arg(actionTarget.login));
                        return;
                    }

                    timeoutUserByID(channel, actionChannel.id,
                                    currentUser->getUserId(), actionTarget.id,
                                    duration, reason, actionTarget.displayName);
                },
                [channel{ctx.channel}, userLoginsToFetch] {
                    channel->addSystemMessage(
                        QString("فشل الطرد المؤقت، أسماء غلط: %1")
                            .arg(userLoginsToFetch.join(", ")));
                });
        }
        else
        {
            // If both IDs are available, we do no hydration & just use the id as the display name
            timeoutUserByID(ctx.channel, action.channel.id,
                            currentUser->getUserId(), action.target.id,
                            action.duration, reason, action.target.id);
        }
    }

    return "";
}

}  // namespace chatterino::commands
