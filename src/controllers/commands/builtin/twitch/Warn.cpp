// SPDX-FileCopyrightText: 2023 Contributors to Chatterino <https://chatterino.com>
//
// SPDX-License-Identifier: MIT

#include "controllers/commands/builtin/twitch/Warn.hpp"

#include "Application.hpp"
#include "common/Channel.hpp"
#include "common/QLogging.hpp"
#include "controllers/accounts/AccountController.hpp"
#include "controllers/commands/CommandContext.hpp"
#include "controllers/commands/common/ChannelAction.hpp"
#include "providers/twitch/api/Helix.hpp"
#include "providers/twitch/TwitchAccount.hpp"

namespace {

using namespace chatterino;

void warnUserByID(const ChannelPtr &channel, const QString &channelID,
                  const QString &sourceUserID, const QString &targetUserID,
                  const QString &reason, const QString &displayName)
{
    using Error = HelixWarnUserError;

    getHelix()->warnUser(
        channelID, sourceUserID, targetUserID, reason,
        [] {
            // No response for warns, they're emitted over pubsub instead
        },
        [channel, displayName](auto error, auto message) {
            QString errorMessage = QString("فشل تحذير المستخدم - ");
            switch (error)
            {
                case Error::ConflictingOperation: {
                    errorMessage += "فيه عملية تحذير ثانية متعارضة على هذا المستخدم. حاول مرة ثانية.";
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

                case Error::CannotWarnUser: {
                    errorMessage +=
                        QString("ما تقدر تحذّر %1.").arg(displayName);
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

            channel->addSystemMessage(errorMessage);
        });
}

}  // namespace

namespace chatterino::commands {

QString sendWarn(const CommandContext &ctx)
{
    const auto command = QStringLiteral("/warn");
    const auto usage = QStringLiteral(
        R"(Usage: "/warn [options...] <username> <reason>" - Warn a user via their username. Reason is required and will be shown to the target user and other moderators. Options: --channel <channel> to override which channel the warn takes place in (can be specified multiple times).)");
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
        ctx.channel->addSystemMessage("لازم تسجّل دخول عشان تحذّر أحد!");
        return "";
    }

    for (const auto &action : actions.value())
    {
        const auto &reason = action.reason;
        if (reason.isEmpty())
        {
            ctx.channel->addSystemMessage(
                "فشل التحذير، لازم تكتب سبب");
            break;
        }

        QStringList userLoginsToFetch;
        QStringList userIDs;
        if (action.target.id.isEmpty())
        {
            assert(!action.target.login.isEmpty() &&
                   "Warn Action target username AND user ID may not be "
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
                   "Warn Action channel username AND user ID may not be "
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
                            QString("فشل التحذير، اسم القناة غلط: %1")
                                .arg(actionChannel.login));
                        return;
                    }
                    if (!actionTarget.hydrateFrom(users))
                    {
                        channel->addSystemMessage(
                            QString("فشل التحذير، اسم المستخدم غلط: %1")
                                .arg(actionTarget.login));
                        return;
                    }

                    warnUserByID(channel, actionChannel.id,
                                 currentUser->getUserId(), actionTarget.id,
                                 reason, actionTarget.displayName);
                },
                [channel{ctx.channel}, userLoginsToFetch] {
                    channel->addSystemMessage(
                        QString("فشل التحذير، أسماء غلط: %1")
                            .arg(userLoginsToFetch.join(", ")));
                });
        }
        else
        {
            // If both IDs are available, we do no hydration & just use the id as the display name
            warnUserByID(ctx.channel, action.channel.id,
                         currentUser->getUserId(), action.target.id, reason,
                         action.target.id);
        }
    }

    return "";
}

}  // namespace chatterino::commands
