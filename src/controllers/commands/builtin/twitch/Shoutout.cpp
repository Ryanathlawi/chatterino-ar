// SPDX-FileCopyrightText: 2023 Contributors to Chatterino <https://chatterino.com>
//
// SPDX-License-Identifier: MIT

#include "controllers/commands/builtin/twitch/Shoutout.hpp"

#include "Application.hpp"
#include "controllers/accounts/AccountController.hpp"
#include "controllers/commands/CommandContext.hpp"
#include "providers/twitch/api/Helix.hpp"
#include "providers/twitch/TwitchAccount.hpp"
#include "providers/twitch/TwitchChannel.hpp"
#include "util/Twitch.hpp"

namespace chatterino::commands {

QString sendShoutout(const CommandContext &ctx)
{
    auto *twitchChannel = ctx.twitchChannel;
    auto channel = ctx.channel;
    const auto *words = &ctx.words;

    if (twitchChannel == nullptr)
    {
        channel->addSystemMessage(
            "الأمر /shoutout يشتغل بس في قنوات Twitch.");
        return "";
    }

    auto currentUser = getApp()->getAccounts()->twitch.getCurrent();
    if (currentUser->isAnon())
    {
        channel->addSystemMessage("لازم تسجّل دخول عشان تسوي شاوت أوت.");
        return "";
    }

    if (words->size() < 2)
    {
        channel->addSystemMessage("الاستخدام: \"/shoutout <المستخدم>\" - يسوي شاوت أوت للمستخدم");
        return "";
    }

    auto target = words->at(1);
    stripChannelName(target);

    using Error = HelixSendShoutoutError;

    getHelix()->getUserByName(
        target,
        [twitchChannel, channel, currentUser](const auto targetUser) {
            getHelix()->sendShoutout(
                twitchChannel->roomId(), targetUser.id,
                currentUser->getUserId(),
                [channel, targetUser]() {
                    channel->addSystemMessage(
                        QString("تم الشاوت أوت لـ %1").arg(targetUser.login));
                },
                [channel](auto error, auto message) {
                    QString errorMessage = "فشل الشاوت أوت - ";

                    switch (error)
                    {
                        case Error::UserNotAuthorized: {
                            errorMessage += "ما عندك صلاحية تسوي هذا الإجراء.";
                        }
                        break;

                        case Error::UserMissingScope: {
                            errorMessage += "صلاحية ناقصة. سجّل دخولك من جديد وحاول مرة ثانية.";
                        }
                        break;

                        case Error::Ratelimited: {
                            errorMessage +=
                                "Twitch حاط عليك حد مؤقت. حاول بعد ثواني.";
                        }
                        break;

                        case Error::UserIsBroadcaster: {
                            errorMessage += "صاحب القناة ما يقدر يسوي شاوت أوت لنفسه.";
                        }
                        break;

                        case Error::BroadcasterNotLive: {
                            errorMessage +=
                                "صاحب القناة مو في بث مباشر أو ما عنده مشاهدين.";
                        }
                        break;

                        case Error::Unknown: {
                            errorMessage += message;
                        }
                        break;
                    }

                    channel->addSystemMessage(errorMessage);
                });
        },
        [channel, target] {
            // Equivalent error from IRC
            channel->addSystemMessage(
                QString("اسم مستخدم غير صالح: %1").arg(target));
        });

    return "";
}

}  // namespace chatterino::commands
