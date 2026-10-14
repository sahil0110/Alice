#pragma once
#include <QString>
#include <QStringList>

// Windows media control helpers used by the match-detection feature.
// All functions are safe to call from the Qt main thread.
namespace MediaCtrl {
    // Pause any browser (Chrome / Edge / Firefox / Brave / Opera) that currently
    // has an active WASAPI audio session.  Returns list of browser exe names that
    // were found playing and sent a pause command.
    QStringList pauseActiveBrowsers();

    // Send APPCOMMAND_MEDIA_PLAY to the Spotify window.
    void ensureSpotifyPlaying();

    // Bring the first visible window owned by the given exe to the foreground.
    void focusWindow(const QString &exeName);
}
