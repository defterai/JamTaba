#include "MeteringSettings.h"
#include "log/Logging.h"

using namespace persistence;

MeteringSettings::MeteringSettings() :
    SettingsObject(QStringLiteral("Metering")),
    showingMaxPeakMarkers(true),
    meterOption(MeterMode::PeakAndRms),
    refreshRate(DEFAULT_REFRESH_RATE),
    waveDrawingMode(WaveDrawingMode::PixeledBuildings)
{
    qCDebug(jtSettings) << "MeteringSettings ctor";
}

void MeteringSettings::read(const QJsonObject &in)
{
    setShowingMaxPeakMarkers(getValueFromJson(in, "showMaxPeak", true));
    setOption(getEnumValueFromJson(in, "meterOption", MeterMode::PeakAndRms));
    setRefreshRate(getValueFromJson(in, "refreshRate", quint8(DEFAULT_REFRESH_RATE)));
    setWaveDrawingMode(getEnumValueFromJson(in, "waveDrawingMode", WaveDrawingMode::PixeledBuildings));

    qCDebug(jtSettings) << "MeteringSettings: showingMaxPeakMarkers " << showingMaxPeakMarkers
                        << "; meterOption " << getEnumUnderlayingValue(meterOption)
                        << "; refreshRate " << refreshRate
                        << "; waveDrawingMode " << getEnumUnderlayingValue(waveDrawingMode);
}

void MeteringSettings::write(QJsonObject &out) const
{
    qCDebug(jtSettings) << "MeteringSettings write";
    out["showMaxPeak"]      = showingMaxPeakMarkers;
    out["meterOption"]      = getEnumUnderlayingValue(meterOption);
    out["refreshRate"]      = refreshRate;
    out["waveDrawingMode"]  = getEnumUnderlayingValue(waveDrawingMode);
}

void MeteringSettings::setOption(MeterMode value)
{
    if (value == MeterMode::PeakAndRms ||
            value == MeterMode::PeakOnly ||
            value == MeterMode::RmsOnly) {
        meterOption = value;
    }
}

void MeteringSettings::setRefreshRate(quint8 value)
{
    if (value < MIN_REFRESH_RATE)
        refreshRate = MIN_REFRESH_RATE;
    else if (value > MAX_REFRESH_RATE)
        refreshRate = MAX_REFRESH_RATE;
    else
        refreshRate = value;
}

void MeteringSettings::setWaveDrawingMode(WaveDrawingMode value)
{
    if (value == WaveDrawingMode::SoundWave ||
            value == WaveDrawingMode::Buildings ||
            value == WaveDrawingMode::PixeledSoundWave ||
            value == WaveDrawingMode::PixeledBuildings) {
        waveDrawingMode = value;
    }
}
