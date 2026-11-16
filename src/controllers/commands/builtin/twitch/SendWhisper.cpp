// SPDX-FileCopyrightText: 2023 Contributors to Chatterino <https://chatterino.com>
//
// SPDX-License-Identifier: MIT

#include "controllers/commands/builtin/twitch/SendWhisper.hpp"

#include "Application.hpp"
#include "common/LinkParser.hpp"
#include "controllers/accounts/AccountController.hpp"
#include "controllers/commands/CommandContext.hpp"
#include "controllers/emotes/EmoteController.hpp"
#include "messages/Message.hpp"
#include "messages/MessageBuilder.hpp"
#include "messages/MessageElement.hpp"
#include "providers/bttv/BttvEmotes.hpp"
#include "providers/emoji/Emojis.hpp"
#include "providers/ffz/FfzEmotes.hpp"
#include "providers/twitch/api/Helix.hpp"
#include "providers/twitch/TwitchAccount.hpp"
#include "providers/twitch/TwitchIrcServer.hpp"
#include "singletons/Settings.hpp"
#include "singletons/StreamerMode.hpp"
#include "singletons/Theme.hpp"
#include "util/Twitch.hpp"

namespace {

using namespace chatterino;

QString formatWhisperError(HelixWhisperError error, const QString &message)
{
    using Error = HelixWhisperError;

    QString errorMessage = "فشل إرسال الهمسة - ";

    switch (error)
    {
        case Error::NoVerifiedPhone: {
            errorMessage += "بسبب قيود Twitch، لازم يكون عندك رقم جوال موثّق عشان ترسل همسات. تقدر تضيفه من إعدادات Twitch. https://www.twitch.tv/settings/security";
        };
        break;

        case Error::RecipientBlockedUser: {
            errorMessage += "المستلم ما يستقبل همسات من الغرباء أو منك.";
        };
        break;

        case Error::WhisperSelf: {
            errorMessage += "ما تقدر تهمس لنفسك.";
        };
        break;

        case Error::Forwarded: {
            errorMessage += message;
        }
        break;

        case Error::Ratelimited: {
            errorMessage += "You may only whisper a maximum of 40 "
                            "unique recipients per day. Within the "
                            "per day limit, you may whisper a "
                            "maximum of 3 whispers per second and "
                            "a maximum of 100 whispers per minute.";
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

bool appendWhisperMessageWordsLocally(const QStringList &words)
{
    auto *app = getApp();

    MessageBuilder b;

    b.emplace<TimestampElement>();
    b.emplace<TextElement>(
        app->getAccounts()->twitch.getCurrent()->getUserName(),
        MessageElementFlag::Text, MessageColor::Text,
        FontStyle::ChatMediumBold);
    b.emplace<TextElement>("->", MessageElementFlag::Text,
                           getApp()->getThemes()->messages.textColors.system);
    b.emplace<TextElement>(words[1] + ":", MessageElementFlag::Text,
                           MessageColor::Text, FontStyle::ChatMediumBold);

    const auto &acc = app->getAccounts()->twitch.getCurrent();
    const auto &accemotes = *acc->accessEmotes();
    const auto *bttvemotes = app->getBttvEmotes();
    const auto *ffzemotes = app->getFfzEmotes();
    auto emote = std::optional<EmotePtr>{};
    for (int i = 2; i < words.length(); i++)
    {
        {  // Twitch emote
            auto it = accemotes->find({words[i]});
            if (it != accemotes->end())
            {
                b.emplace<EmoteElement>(it->second, MessageElementFlag::Emote);
                continue;
            }
        }  // Twitch emote

        {  // bttv/ffz emote
            emote = bttvemotes->emote({words[i]});
            if (!emote)
            {
                emote = ffzemotes->emote({words[i]});
            }
            // TODO: Load 7tv global emotes
            if (emote)
            {
                b.emplace<EmoteElement>(*emote, MessageElementFlag::Emote);
                continue;
            }
        }  // bttv/ffz emote
        {  // emoji/text
            for (auto &variant : app->getEmotes()->getEmojis()->parse(words[i]))
            {
                constexpr const static struct {
                    void operator()(EmotePtr emote, MessageBuilder &b) const
                    {
                        b.emplace<EmoteElement>(emote,
                                                MessageElementFlag::EmojiAll);
                    }
                    void operator()(QStringView string, MessageBuilder &b) const
                    {
                        auto link = linkparser::parse(string);
                        if (link)
                        {
                            b.addLink(*link, string);
                        }
                        else
                        {
                            b.emplace<TextElement>(string.toString(),
                                                   MessageElementFlag::Text);
                        }
                    }
                } visitor;
                std::visit(
                    [&b](auto &&arg) {
                        visitor(arg, b);
                    },
                    variant);
            }  // emoji/text
        }
    }

    b->flags.set(MessageFlag::DoNotTriggerNotification);
    b->flags.set(MessageFlag::Whisper);
    auto messagexD = b.release();

    getApp()->getTwitch()->getWhispersChannel()->addMessage(
        messagexD, MessageContext::Original);

    if (getSettings()->inlineWhispers &&
        !(getSettings()->streamerModeSuppressInlineWhispers &&
          getApp()->getStreamerMode()->isEnabled()))
    {
        app->getTwitch()->forEachChannel([&messagexD](ChannelPtr _channel) {
            _channel->addMessage(messagexD, MessageContext::Repost);
        });
    }

    return true;
}

}  // namespace

namespace chatterino::commands {

QString sendWhisper(const CommandContext &ctx)
{
    if (ctx.channel == nullptr)
    {
        return "";
    }

    if (ctx.words.size() < 3)
    {
        ctx.channel->addSystemMessage("الاستخدام: /w <المستخدم> <رسالة>");
        return "";
    }

    auto currentUser = getApp()->getAccounts()->twitch.getCurrent();
    if (currentUser->isAnon())
    {
        ctx.channel->addSystemMessage(
            "لازم تسجّل دخول عشان ترسل همسة!");
        return "";
    }
    auto target = ctx.words.at(1);
    stripChannelName(target);
    auto message = ctx.words.mid(2).join(' ');
    if (ctx.channel->isTwitchChannel())
    {
        getHelix()->getUserByName(
            target,
            [channel{ctx.channel}, currentUser, target, message,
             words{ctx.words}](const auto &targetUser) {
                getHelix()->sendWhisper(
                    currentUser->getUserId(), targetUser.id, message,
                    [words] {
                        appendWhisperMessageWordsLocally(words);
                    },
                    [channel, target, targetUser](auto error, auto message) {
                        auto errorMessage = formatWhisperError(error, message);
                        channel->addSystemMessage(errorMessage);
                    });
            },
            [channel{ctx.channel}] {
                channel->addSystemMessage("ما فيه مستخدم بهذا الاسم.");
            });
        return "";
    }

    return "";
}

}  // namespace chatterino::commands
