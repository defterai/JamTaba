#include "MetronomeSettings.h"
#include "log/Logging.h"
#include <QFileInfo>

using namespace persistence;

MetronomeSoundSettings::MetronomeSoundSettings() :
    SettingsObject("metronome"),
    customPrimaryBeatAudioFile(""),
    customOffBeatAudioFile(""),
    customAccentBeatAudioFile(""),
    builtInMetronomeAlias("Default"),
    usingCustomSounds(false)
{
    qCDebug(jtSettings) << "MetronomeSoundSettings ctor";
    qRegisterMetaType<persistence::MetronomeSoundSettings>();
}

bool MetronomeSoundSettings::operator==(const MetronomeSoundSettings& rhs) const {
    return usingCustomSounds == rhs.usingCustomSounds &&
            customPrimaryBeatAudioFile == rhs.customPrimaryBeatAudioFile &&
            customOffBeatAudioFile == rhs.customOffBeatAudioFile &&
            customAccentBeatAudioFile == rhs.customAccentBeatAudioFile &&
            builtInMetronomeAlias == rhs.builtInMetronomeAlias;
}

bool MetronomeSoundSettings::operator!=(const MetronomeSoundSettings& rhs) const {
    return !operator==(rhs);
}

bool MetronomeSoundSettings::isSoundChanged(const MetronomeSoundSettings& rhs) const {
    if (usingCustomSounds != rhs.usingCustomSounds) {
        return true;
    }
    if (usingCustomSounds) {
        return customPrimaryBeatAudioFile != rhs.customPrimaryBeatAudioFile ||
                customOffBeatAudioFile != rhs.customOffBeatAudioFile ||
                customAccentBeatAudioFile != rhs.customAccentBeatAudioFile;
    }
    return builtInMetronomeAlias != rhs.builtInMetronomeAlias;
}

void MetronomeSoundSettings::read(const QJsonObject &in)
{
    setBuiltInMetronome(getValueFromJson(in, "builtInMetronome", QString("Default")));
    setCustomMetronome(getValueFromJson(in, "customPrimaryBeatAudioFile", QString("")),
                       getValueFromJson(in, "customOffBeatAudioFile", getValueFromJson(in, "customSecondaryBeatAudioFile", QString(""))),  // backward compatible
                       getValueFromJson(in, "customAccentBeatAudioFile", QString("")));
    if (!getValueFromJson(in, "usingCustomSounds", false)) {
        usingCustomSounds = false;
    }

    qCDebug(jtSettings) << "MetronomeSoundSettings: usingCustomSounds " << usingCustomSounds
                        << "; customPrimaryBeatAudioFile " << customPrimaryBeatAudioFile
                        << "; customOffBeatAudioFile " << customOffBeatAudioFile
                        << "; customAccentBeatAudioFile " << customAccentBeatAudioFile
                        << "; builtInMetronomeAlias " << builtInMetronomeAlias;
}

void MetronomeSoundSettings::write(QJsonObject &out) const
{
    qCDebug(jtSettings) << "MetronomeSoundSettings write";
    out["usingCustomSounds"] = usingCustomSounds;
    out["customPrimaryBeatAudioFile"] = customPrimaryBeatAudioFile;
    out["customOffBeatAudioFile"] = customOffBeatAudioFile;
    out["customAccentBeatAudioFile"] = customAccentBeatAudioFile;
    out["builtInMetronome"] = builtInMetronomeAlias;
}

void MetronomeSoundSettings::setCustomMetronome(const QString &primaryBeatAudioFile, const QString &offBeatAudioFile, const QString &accentBeatAudioFile)
{
    qCDebug(jtSettings) << "MetronomeSoundSettings setCustomMetronome: primaryBeatAudioFile from " << customPrimaryBeatAudioFile << " to " << primaryBeatAudioFile
                        << "; offBeatAudioFile from " << customOffBeatAudioFile << " to " << offBeatAudioFile
                        << "; accentBeatAudioFile from " << customAccentBeatAudioFile << " to " << accentBeatAudioFile;
    customPrimaryBeatAudioFile = QFileInfo::exists(primaryBeatAudioFile) ? primaryBeatAudioFile : "";
    customOffBeatAudioFile = QFileInfo::exists(offBeatAudioFile) ? offBeatAudioFile : "";
    customAccentBeatAudioFile = QFileInfo::exists(accentBeatAudioFile) ? accentBeatAudioFile : "";
    usingCustomSounds = !customPrimaryBeatAudioFile.isEmpty() && !customOffBeatAudioFile.isEmpty() && !customAccentBeatAudioFile.isEmpty();
}

void MetronomeSoundSettings::setBuiltInMetronome(const QString &metronomeAlias)
{
    qCDebug(jtSettings) << "MetronomeSoundSettings setBuiltInMetronome: from " << builtInMetronomeAlias << " to " << metronomeAlias << " (and not custom sounds)";
    builtInMetronomeAlias = metronomeAlias;
    usingCustomSounds = false;
}

MetronomeSettings::MetronomeSettings() :
    pan(0),
    gain(1),
    muted(false)
{
    qCDebug(jtSettings) << "MetronomeSettings ctor";
    //
}

void MetronomeSettings::read(const QJsonObject &in)
{
    MetronomeSoundSettings::read(in);
    setPan(getValueFromJson(in, "pan", (float)0));
    setGain(getValueFromJson(in, "gain", (float)1));
    setMuted(getValueFromJson(in, "muted", false));

    qCDebug(jtSettings) << "MetronomeSettings: pan " << pan
                        << "; gain " << gain
                        << "; muted " << muted;
}

bool MetronomeSettings::operator==(const MetronomeSettings& rhs) const {
    return muted == rhs.muted &&
            qFuzzyCompare(pan, rhs.pan) &&
            qFuzzyCompare(gain, rhs.gain) &&
            MetronomeSoundSettings::operator==(rhs);
}

bool MetronomeSettings::operator!=(const MetronomeSettings& rhs) const {
    return !operator==(rhs);
}

void MetronomeSettings::write(QJsonObject &out) const
{
    MetronomeSoundSettings::write(out);
    qCDebug(jtSettings) << "MetronomeSettings write";
    out["pan"] = pan;
    out["gain"] = gain;
    out["muted"] = muted;
}

void MetronomeSettings::setPan(float value)
{
    if (value < -1)
        pan = -1;
    else if (value > 1)
        pan = 1;
    else
        pan = value;
}
