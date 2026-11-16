// SPDX-FileCopyrightText: 2023 Contributors to Chatterino <https://chatterino.com>
//
// SPDX-License-Identifier: MIT

#include "controllers/commands/builtin/twitch/AddModerator.hpp"

#include "Application.hpp"
#include "controllers/accounts/AccountController.hpp"
#include "controllers/commands/CommandContext.hpp"
#include "providers/twitch/api/Helix.hpp"
#include "providers/twitch/TwitchAccount.hpp"
#include "providers/twitch/TwitchChannel.hpp"
#include "util/Twitch.hpp"

namespace chatterino::commands {

QString addModerator(const CommandContext &ctx)
{
    if (ctx.channel == nullptr)
    {
        return "";
    }

    if (ctx.twitchChannel == nullptr)
    {
        ctx.channel->addSystemMessage(
            "الأمر /mod يشتغل بس في قنوات Twitch.");
        return "";
    }
    if (ctx.words.size() < 2)
    {
        ctx.channel->addSystemMessage(
            "الاستخدام: \"/mod <المستخدم>\" - يعطي المستخدم صلاحية مشرف. استخدم \"/mods\" لعرض مشرفي القناة.");
        return "";
    }

    auto currentUser = getApp()->getAccounts()->twitch.getCurrent();
    if (currentUser->isAnon())
    {
        ctx.channel->addSystemMessage("لازم تسجّل دخول عشان تعيّن مشرف!");
        return "";
    }

    auto target = ctx.words.at(1);
    stripChannelName(target);

    getHelix()->getUserByName(
        target,
        [twitchChannel{ctx.twitchChannel},
         channel{ctx.channel}](const HelixUser &targetUser) {
            getHelix()->addChannelModerator(
                twitchChannel->roomId(), targetUser.id,
                [channel, targetUser] {
                    channel->addSystemMessage(
                        QString("أضفت %1 كمشرف في هذي القناة.")
                            .arg(targetUser.displayName));
                },
                [channel, targetUser](auto error, auto message) {
                    QString errorMessage =
                        QString("فشل إضافة مشرف - ");

                    using Error = HelixAddChannelModeratorError;

                    switch (error)
                    {
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

                        case Error::Ratelimited: {
                            errorMessage +=
                                "Twitch حاط عليك حد مؤقت. حاول بعد ثواني.";
                        }
                        break;

                        case Error::TargetIsVIP: {
                            errorMessage +=
                                QString("%1 حاليًا VIP، استخدم \"/unvip\" عليه وأعد الأمر.")
                                    .arg(targetUser.displayName);
                        }
                        break;

                        case Error::TargetAlreadyModded: {
                            // Equivalent irc error
                            errorMessage =
                                QString("%1 مشرف في هذي القناة من قبل.")
                                    .arg(targetUser.displayName);
                        }
                        break;

                        case Error::Forwarded: {
                            errorMessage += message;
                        }
                        break;

                        case Error::Unknown:
                        default: {
                            errorMessage += "صار خطأ غير معروف.";
                        }
                        break;
                    }
                    channel->addSystemMessage(errorMessage);
                });
        },
        [channel{ctx.channel}, target] {
            // Equivalent error from IRC
            channel->addSystemMessage(
                QString("اسم مستخدم غير صالح: %1").arg(target));
        });

    return "";
}

}  // namespace chatterino::commands
