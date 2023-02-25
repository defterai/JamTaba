#ifndef METRONOMESETTINGS_H
#define METRONOMESETTINGS_H

#include "SettingsObject.h"

namespace persistence {

class MetronomeSoundSettings : public SettingsObject
{
public:
    MetronomeSoundSettings();
    bool operator==(const MetronomeSoundSettings& rhs) const;
    bool operator!=(const MetronomeSoundSettings& rhs) const;
    bool isSoundChanged(const MetronomeSoundSettings& rhs) const;
    void write(QJsonObject &out) const override;
    void read(const QJsonObject &in) override;

    inline bool isUsingCustomSounds() const
    {
        return usingCustomSounds;
    }
    inline const QString& getCustomPrimaryBeatFile() const
    {
        return customPrimaryBeatAudioFile;
    }
    inline const QString& getCustomOffBeatFile() const
    {
        return customOffBeatAudioFile;
    }
    inline const QString& getCustomAccentBeatFile() const
    {
        return customAccentBeatAudioFile;
    }
    void setCustomMetronome(const QString &primaryBeatAudioFile,
                            const QString &offBeatAudioFile,
                            const QString &accentBeatAudioFile);
    inline const QString& getBuiltInMetronomeAlias() const
    {
        return builtInMetronomeAlias;
    }
    void setBuiltInMetronome(const QString &metronomeAlias);
private:
    QString customPrimaryBeatAudioFile;
    QString customOffBeatAudioFile;
    QString customAccentBeatAudioFile;
    QString builtInMetronomeAlias;
    bool usingCustomSounds;
};

class MetronomeSettings final : public MetronomeSoundSettings
{
public:
    MetronomeSettings();
    bool operator==(const MetronomeSettings& rhs) const;
    bool operator!=(const MetronomeSettings& rhs) const;
    void write(QJsonObject &out) const override;
    void read(const QJsonObject &in) override;
    inline float getPan() const
    {
        return pan;
    }
    void setPan(float value);
    inline float getGain() const
    {
        return gain;
    }
    inline void setGain(float value)
    {
        gain = value;
    }
    inline bool isMuted() const
    {
        return muted;
    }
    inline void setMuted(bool value)
    {
        muted = value;
    }
private:
    float pan;
    float gain;
    bool muted;
};

}

Q_DECLARE_METATYPE(persistence::MetronomeSoundSettings)

#endif // METRONOMESETTINGS_H
