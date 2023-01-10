#ifndef LOOPERSETTINGS_H
#define LOOPERSETTINGS_H

#include <array>
#include "SettingsObject.h"

namespace persistence {

enum class LooperMode : quint8
{
    Sequence = 0, // one layer in each interval
    AllLayers = 1, // mix and play all layers
    SelectedLayer = 2,
};

class LooperSettings final : public SettingsObject
{
public:
    static const quint8 DEFAULT_LAYERS_COUNT = 4;
    static const quint8 MIN_LAYERS_COUNT = 1;
    static const quint8 MAX_LAYERS_COUNT = 8;
    static const std::array<LooperMode, 3> LOOPER_MODES;

    LooperSettings();
    void write(QJsonObject &out) const override;
    void read(const QJsonObject &in) override;
    inline quint8 getPreferredLayersCount() const
    {
        return preferredLayersCount;
    }
    void setPreferredLayersCount(quint8 value);
    inline LooperMode getPreferredMode() const
    {
        return preferredMode;
    }
    void setPreferredMode(LooperMode value);
    inline const QString& getLoopsFolder() const
    {
        return loopsFolder;
    }
    inline void setLoopsFolder(const QString& value)
    {
        loopsFolder = value;
    }
    inline bool isEncodingAudioWhenSaving() const
    {
        return encodingAudioWhenSaving;
    }
    inline void setEncodingAudioWhenSaving(bool value)
    {
        encodingAudioWhenSaving = value;
    }
    inline quint8 getWaveFilesBitDepth() const
    {
        return waveFilesBitDepth;
    }
    bool setWaveFilesBitDepth(quint8 value);
private:
    quint8 preferredLayersCount; // how many layers in each looper?
    LooperMode preferredMode; // store the last used looper mode
    QString loopsFolder; // where looper audio files will be saved
    bool encodingAudioWhenSaving;
    quint8 waveFilesBitDepth;

    void setDefaultLooperFilesPath();
};

}

// declaring structs to use in signal/slots
Q_DECLARE_METATYPE(persistence::LooperMode)

#endif // LOOPERSETTINGS_H
