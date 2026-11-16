// SPDX-FileCopyrightText: 2023 Contributors to Chatterino <https://chatterino.com>
//
// SPDX-License-Identifier: MIT

#include "controllers/commands/builtin/twitch/DeleteMessages.hpp"

#include "Application.hpp"
#include "common/Channel.hpp"
#include "controllers/accounts/AccountController.hpp"
#include "controllers/commands/CommandContext.hpp"
#include "messages/Message.hpp"
#include "messages/MessageBuilder.hpp"
#include "providers/twitch/TwitchAccount.hpp"
#include "providers/twitch/TwitchChannel.hpp"

#include <QUuid>

namespace {

using namespace chatterino;

QString deleteMessages(TwitchChannel *twitchChannel, const QString &messageID)
{
    const auto *commandName = messageID.isEmpty() ? "/clear" : "/delete";

    auto user = getApp()->getAccounts()->twitch.getCurrent();

    // Avoid Helix calls without Client ID and/or OAuth Token
    if (user->isAnon())
    {
        twitchChannel->addSystemMessage(
            QString("لازم تسجّل دخول عشان تستخدم الأمر %1.")
                .arg(commandName));
        return "";
    }

    twitchChannel->deleteMessagesAs(messageID, user.get());

    return "";
}

}  // namespace

namespace chatterino::commands {

QString deleteAllMessages(const CommandContext &ctx)
{
    if (ctx.channel == nullptr)
    {
        return "";
    }

    if (ctx.twitchChannel == nullptr)
    {
        ctx.channel->addSystemMessage(
            "الأمر /clear يشتغل بس في قنوات Twitch.");
        return "";
    }

    return deleteMessages(ctx.twitchChannel, QString());
}

QString deleteOneMessage(const CommandContext &ctx)
{
    if (ctx.channel == nullptr)
    {
        return "";
    }

    // This is a wrapper over the Helix delete messages endpoint
    // We use this to ensure the user gets better error messages for missing or malformed arguments
    if (ctx.twitchChannel == nullptr)
    {
        ctx.channel->addSystemMessage(
            "الأمر /delete يشتغل بس في قنوات Twitch.");
        return "";
    }

    if (ctx.words.size() < 2)
    {
        ctx.channel->addSystemMessage("الاستخدام: /delete <معرّف الرسالة> - يحذف الرسالة.");
        return "";
    }

    auto messageID = ctx.words.at(1);
    auto uuid = QUuid(messageID);
    if (uuid.isNull())
    {
        // The message id must be a valid UUID
        ctx.channel->addSystemMessage(
            QString("معرّف رسالة غير صالح: \"%1\"").arg(messageID));
        return "";
    }

    auto msg = ctx.channel->findMessageByID(messageID);
    if (msg != nullptr)
    {
        if (msg->loginName == ctx.channel->getName() &&
            !ctx.channel->isBroadcaster())
        {
            ctx.channel->addSystemMessage(
                "ما تقدر تحذف رسائل صاحب القناة إلا إذا كنت أنت صاحبها.");
            return "";
        }
    }

    return deleteMessages(ctx.twitchChannel, messageID);
}

}  // namespace chatterino::commands
