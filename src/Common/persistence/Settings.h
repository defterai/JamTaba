#ifndef SETTINGS_H
#define SETTINGS_H

#include <QString>
#include <QJsonObject>
#include <QList>
#include <QMap>
#include <QStringList>
#include "Configurator.h"

#include "SettingsObject.h"
#include "AudioSettings.h"
#include "WindowSettings.h"
#include "MetronomeSettings.h"
#include "VstSettings.h"
#ifdef Q_OS_MAC
#include "AudioUnitSettings.h"
#endif
#include "MultiTrackRecordingSettings.h"
#include "LocalInputTrackSettings.h"
#include "PrivateServerSettings.h"
#include "MeteringSettings.h"
#include "LooperSettings.h"
#include "RememberSettings.h"
#include "CollapseSettings.h"

namespace persistence {

class Settings;

// +++++++++++++++++++++++++++++++++++++

class MidiSettings : public SettingsObject
{
public:
    MidiSettings();
    void write(QJsonObject &out) const override;
    void read(const QJsonObject &in) override;
    inline const QList<bool>& getInputDevicesStatus() const {
        return inputDevicesStatus;
    }
    inline void setInputDevicesStatus(const QList<bool>& value) {
        inputDevicesStatus = value;
    }
private:
    QList<bool> inputDevicesStatus;
};

// +++++++++++++++++++++++++++++++++++

class SyncSettings : public SettingsObject
{
public:
    SyncSettings();
    void write(QJsonObject &out) const override;
    void read(const QJsonObject &in) override;
    inline const QList<bool>& getOutputDevicesStatus() const {
        return syncOutputDevicesStatus;
    }
    inline void setOutputDevicesStatus(const QList<bool>& value) {
        syncOutputDevicesStatus = value;
    }
private:
    QList<bool> syncOutputDevicesStatus;
};

// +++++++++PRESETS+++++++++++++++

class Preset
{
public:
    Preset(const QString &name = "default",
           const LocalInputTrackSettings &inputSettings = LocalInputTrackSettings());
    void write(QJsonObject &out) const;
    void read(const QJsonObject &in);

    bool isValid() const
    {
        return inputTrackSettings.isValid();
    }

    LocalInputTrackSettings inputTrackSettings;
    QString name;
};

// ++++++++++++++++++++++++

class Settings
{
private:
    static QString fileName;
public:
    AudioSettings audioSettings;
    MidiSettings midiSettings;
    SyncSettings syncSettings;
    WindowSettings windowSettings;
    MetronomeSettings metronomeSettings;
    VstSettings vstSettings;
#ifdef Q_OS_MAC
    AudioUnitSettings audioUnitSettings;
#endif
    LocalInputTrackSettings inputsSettings;
    MultiTrackRecordingSettings recordingSettings;
    PrivateServerSettings privateServerSettings;
    MeteringSettings meteringSettings;
    LooperSettings looperSettings;
    RememberSettings rememberSettings;
    CollapseSettings collapseSettings;

private:
    QStringList recentEmojis;

    QString lastUserName;               // the last nick name choosed by user
    QString translation;                // the translation language (en, fr, jp, pt, etc.) being used in chat
    QString theme;                      // the style sheet used
    int ninjamIntervalProgressShape;    // Circle, Ellipe or Line
    float masterFaderGain;              // last master fader gain
    quint8 tracksLayoutOrientation;     // horizontal or vertical
    bool usingNarrowedTracks;           // narrow or wide tracks?
    bool publicChatActivated;

    uint intervalsBeforeInactivityWarning;

    qint8 chatFontSizeOffset;

    bool readFile(const QList<SettingsObject *> &sections);
    bool writeFile(const QList<SettingsObject *> &sections);

public:
    Settings();
    ~Settings();

    void setTheme(const QString theme);
    QString getTheme() const;

    void storeTracksSize(bool narrowedTracks);

    bool isUsingNarrowedTracks() const;

    void save(const LocalInputTrackSettings &inputsSettings);
    void load();

    float getLastMasterGain() const;
    void storeMasterGain(float newMasterFaderGain);

    quint8 getLastTracksLayoutOrientation() const;
    void storeTracksLayoutOrientation(quint8 newOrientation);

    bool writePresetToFile(const Preset &preset);
    void deletePreset(const QString &name);
    QStringList getPresetList();
    Preset readPresetFromFile(const QString &presetFileName, bool allowMultiSubchannels = true);

    // user name
    QString getUserName() const;
    void storeUserName(const QString &newUserName);
    void storeLastChannelName(int channelIndex, const QString &channelName);

    void setIntervalProgressShape(int shape);
    int getIntervalProgressShape() const;

    QStringList getRecentEmojis() const;
    void setRecentEmojis(const QStringList &emojis);

    // ++++++++++++++++++++++++++++++++++++++++

    qint8 getChatFontSizeOffset() const;
    void storeChatFontSizeOffset(qint8 sizeOffset);

    // ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

    // TRANSLATION
    QString getTranslation() const;
    void setTranslation(const QString &localeName);

    QString getLastChannelName(int channelIndex) const;

    uint getIntervalsBeforeInactivityWarning() const;

    bool publicChatIsActivated() const;
    void setPublicChatActivated(bool activated);
};

inline void Settings::setPublicChatActivated(bool activated)
{
    publicChatActivated = activated;
}

inline bool Settings::publicChatIsActivated() const
{
    return publicChatActivated;
}

inline void Settings::storeChatFontSizeOffset(qint8 sizeOffset)
{
    chatFontSizeOffset = sizeOffset;
}

inline qint8 Settings::getChatFontSizeOffset() const
{
    return chatFontSizeOffset;
}

inline QStringList Settings::getRecentEmojis() const
{
    return recentEmojis;
}

inline void Settings::setRecentEmojis(const QStringList &emojis)
{
    recentEmojis = emojis;
}

inline uint Settings::getIntervalsBeforeInactivityWarning() const
{
    return intervalsBeforeInactivityWarning;
}

// -----------------------------------------------------

inline void Settings::setIntervalProgressShape(int shape)
{
    ninjamIntervalProgressShape = shape;
}

inline int Settings::getIntervalProgressShape() const
{
    return ninjamIntervalProgressShape;
}

// user name
inline QString Settings::getUserName() const
{
    return lastUserName;
}

inline float Settings::getLastMasterGain() const
{
    return masterFaderGain;
}

inline void Settings::storeMasterGain(float newMasterFaderGain)
{
    masterFaderGain = newMasterFaderGain;
}

inline quint8 Settings::getLastTracksLayoutOrientation() const
{
    return tracksLayoutOrientation;
}

inline void Settings::storeTracksLayoutOrientation(quint8 newOrientation)
{
    tracksLayoutOrientation = newOrientation;
}

inline QString Settings::getTheme() const
{
    return theme;
}

inline void Settings::storeTracksSize(bool narrowedTracks)
{
    usingNarrowedTracks = narrowedTracks;
}

inline bool Settings::isUsingNarrowedTracks() const
{
    return usingNarrowedTracks;
}

} // namespace

#endif
