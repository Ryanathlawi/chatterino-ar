// Arabic build: counts the user as one viewer of the channels visible in the
// selected tab, by pulling the lowest-quality HLS stream and discarding it
// (no decoding, no player). One connection per visible channel, never more.
#pragma once

#include "common/ChatterinoSetting.hpp"

#include <QObject>
#include <QUrl>

#include <functional>

namespace chatterino::viewerpresence {

BoolSetting &enabledSetting();
BoolSetting &playerSetting();

// usher master playlist URL for a live channel; empty URL if offline
void masterPlaylistUrl(const QString &login, QObject *ctx,
                       std::function<void(const QUrl &)> done);

// call once after the main window exists
void start();

}  // namespace chatterino::viewerpresence
