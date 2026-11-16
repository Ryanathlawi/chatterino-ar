// SPDX-FileCopyrightText: 2023 Contributors to Chatterino <https://chatterino.com>
//
// SPDX-License-Identifier: MIT

#include "controllers/commands/builtin/twitch/Block.hpp"

#include "Application.hpp"
#include "common/Channel.hpp"
#include "controllers/accounts/AccountController.hpp"
#include "controllers/commands/CommandContext.hpp"
#include "providers/twitch/api/Helix.hpp"
#include "providers/twitch/TwitchAccount.hpp"
#include "util/Twitch.hpp"

namespace {

using namespace chatterino;

}  // namespace

namespace chatterino::commands {

QString blockUser(const CommandContext &ctx)
{
    if (ctx.channel == nullptr)
    {
        return "";
    }

    if (ctx.twitchChannel == nullptr)
    {
        ctx.channel->addSystemMessage(
            "الأمر /block يشتغل بس في قنوات Twitch.");
        return "";
    }

    if (ctx.words.size() < 2)
    {
        ctx.channel->addSystemMessage("الاستخدام: /block <المستخدم>");
        return "";
    }

    auto currentUser = getApp()->getAccounts()->twitch.getCurrent();

    if (currentUser->isAnon())
    {
        ctx.channel->addSystemMessage(
            "لازم تسجّل دخول عشان تحظر أحد!");
        return "";
    }

    auto target = ctx.words.at(1);
    stripChannelName(target);

    getHelix()->getUserByName(
        target,
        [currentUser, channel{ctx.channel},
         target](const HelixUser &targetUser) {
            getApp()->getAccounts()->twitch.getCurrent()->blockUser(
                targetUser.id, targetUser.login, nullptr,
                [channel, target, targetUser] {
                    channel->addSystemMessage(
                        QString("حظرت %1 بنجاح")
                            .arg(target));
                },
                [channel, target] {
                    channel->addSystemMessage(
                        QString("ما قدرنا نحظر %1، صار خطأ غير معروف!")
                            .arg(target));
                });
        },
        [channel{ctx.channel}, target] {
            channel->addSystemMessage(QString("ما قدرنا نحظر %1، ما فيه مستخدم بهذا الاسم!")
                                          .arg(target));
        });

    return "";
}

QString ignoreUser(const CommandContext &ctx)
{
    if (ctx.channel == nullptr)
    {
        return "";
    }

    ctx.channel->addSystemMessage(
        "أمر /ignore صار اسمه /block، استخدمه من الحين لأن /ignore بينشال قريب.");

    return blockUser(ctx);
}

QString unblockUser(const CommandContext &ctx)
{
    if (ctx.channel == nullptr)
    {
        return "";
    }

    if (ctx.twitchChannel == nullptr)
    {
        ctx.channel->addSystemMessage(
            "الأمر /unblock يشتغل بس في قنوات Twitch.");
        return "";
    }

    if (ctx.words.size() < 2)
    {
        ctx.channel->addSystemMessage("الاستخدام: /unblock <المستخدم>");
        return "";
    }

    auto currentUser = getApp()->getAccounts()->twitch.getCurrent();

    if (currentUser->isAnon())
    {
        ctx.channel->addSystemMessage(
            "لازم تسجّل دخول عشان تفك حظر أحد!");
        return "";
    }

    auto target = ctx.words.at(1);
    stripChannelName(target);

    getHelix()->getUserByName(
        target,
        [currentUser, channel{ctx.channel}, target](const auto &targetUser) {
            getApp()->getAccounts()->twitch.getCurrent()->unblockUser(
                targetUser.id, targetUser.login, nullptr,
                [channel, target, targetUser] {
                    channel->addSystemMessage(
                        QString("فكيت حظر %1 بنجاح")
                            .arg(target));
                },
                [channel, target] {
                    channel->addSystemMessage(
                        QString("ما قدرنا نفك حظر %1، صار خطأ غير معروف!")
                            .arg(target));
                });
        },
        [channel{ctx.channel}, target] {
            channel->addSystemMessage(QString("ما قدرنا نفك حظر %1، ما فيه مستخدم بهذا الاسم!")
                                          .arg(target));
        });

    return "";
}

QString unignoreUser(const CommandContext &ctx)
{
    if (ctx.channel == nullptr)
    {
        return "";
    }

    ctx.channel->addSystemMessage(
        "أمر /unignore صار اسمه /unblock، استخدمه من الحين لأن /unignore بينشال قريب.");
    return unblockUser(ctx);
}

}  // namespace chatterino::commands
