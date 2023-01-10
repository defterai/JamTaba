#ifndef METERINGSETTINGS_H
#define METERINGSETTINGS_H

#include "SettingsObject.h"

namespace persistence {

enum class MeterMode : quint8 {
    PeakAndRms = 0,
    PeakOnly = 1,
    RmsOnly = 2,
};

enum class WaveDrawingMode : quint8 {
    SoundWave = 0,
    Buildings = 1,
    PixeledSoundWave = 2,
    PixeledBuildings = 3,
};

class MeteringSettings final : public SettingsObject
{
public:
    static const quint8 DEFAULT_REFRESH_RATE = 30; // in Hertz
    static const quint8 MAX_REFRESH_RATE = 60; // in Hertz
    static const quint8 MIN_REFRESH_RATE = 10; // in Hertz

    MeteringSettings();
    void write(QJsonObject &out) const override;
    void read(const QJsonObject &in) override;
    inline bool isShowingMaxPeakMarkers() const
    {
        return showingMaxPeakMarkers;
    }
    inline void setShowingMaxPeakMarkers(bool value)
    {
        showingMaxPeakMarkers = value;
    }
    inline MeterMode getOption() const
    {
        return meterOption;
    }
    void setOption(MeterMode value);
    inline quint8 getRefreshRate() const
    {
        return refreshRate;
    }
    void setRefreshRate(quint8 value);
    inline WaveDrawingMode getWaveDrawingMode() const
    {
        return waveDrawingMode;
    }
    void setWaveDrawingMode(WaveDrawingMode value);
private:
    bool showingMaxPeakMarkers;
    MeterMode meterOption;
    quint8 refreshRate; // in Hertz
    WaveDrawingMode waveDrawingMode;
};

}

#endif // METERINGSETTINGS_H
