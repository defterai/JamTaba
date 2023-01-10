#ifndef METRONOMESETTINGS_H
#define METRONOMESETTINGS_H

#include "SettingsObject.h"

namespace persistence {

class MetronomeSettings final : public SettingsObject
{
public:
    MetronomeSettings();
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
    float pan;
    float gain;
    bool muted;
    bool usingCustomSounds;
    QString customPrimaryBeatAudioFile;
    QString customOffBeatAudioFile;
    QString customAccentBeatAudioFile;
    QString builtInMetronomeAlias;
};

}

#endif // METRONOMESETTINGS_H
