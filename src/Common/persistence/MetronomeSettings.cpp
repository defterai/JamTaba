#include "MetronomeSettings.h"
#include "log/Logging.h"
#include <QFileInfo>

using namespace persistence;

MetronomeSettings::MetronomeSettings() :
    SettingsObject("metronome"),
    pan(0),
    gain(1),
    muted(false),
    usingCustomSounds(false),
    customPrimaryBeatAudioFile(""),
    customOffBeatAudioFile(""),
    customAccentBeatAudioFile(""),
    builtInMetronomeAlias("Default")
{
    qCDebug(jtSettings) << "MetronomeSettings ctor";
    //
}

void MetronomeSettings::read(const QJsonObject &in)
{
    setPan(getValueFromJson(in, "pan", (float)0));
    setGain(getValueFromJson(in, "gain", (float)1));
    setMuted(getValueFromJson(in, "muted", false));
    setBuiltInMetronome(getValueFromJson(in, "builtInMetronome", QString("Default")));
    setCustomMetronome(getValueFromJson(in, "customPrimaryBeatAudioFile", QString("")),
                       getValueFromJson(in, "customOffBeatAudioFile", getValueFromJson(in, "customSecondaryBeatAudioFile", QString(""))),  // backward compatible
                       getValueFromJson(in, "customAccentBeatAudioFile", QString("")));
    if (!getValueFromJson(in, "usingCustomSounds", false)) {
        usingCustomSounds = false;
    }

    qCDebug(jtSettings) << "MetronomeSettings: pan " << pan
                        << "; gain " << gain
                        << "; muted " << muted
                        << "; usingCustomSounds " << usingCustomSounds
                        << "; customPrimaryBeatAudioFile " << customPrimaryBeatAudioFile
                        << "; customOffBeatAudioFile " << customOffBeatAudioFile
                        << "; customAccentBeatAudioFile " << customAccentBeatAudioFile
                        << "; builtInMetronomeAlias " << builtInMetronomeAlias;
}

void MetronomeSettings::write(QJsonObject &out) const
{
    qCDebug(jtSettings) << "MetronomeSettings write";
    out["pan"] = pan;
    out["gain"] = gain;
    out["muted"] = muted;
    out["usingCustomSounds"] = usingCustomSounds;
    out["customPrimaryBeatAudioFile"] = customPrimaryBeatAudioFile;
    out["customOffBeatAudioFile"] = customOffBeatAudioFile;
    out["customAccentBeatAudioFile"] = customAccentBeatAudioFile;
    out["builtInMetronome"] = builtInMetronomeAlias;
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

void MetronomeSettings::setCustomMetronome(const QString &primaryBeatAudioFile, const QString &offBeatAudioFile, const QString &accentBeatAudioFile)
{
    qCDebug(jtSettings) << "MetronomeSettings setCustomMetronome: primaryBeatAudioFile from " << customPrimaryBeatAudioFile << " to " << primaryBeatAudioFile
                        << "; offBeatAudioFile from " << customOffBeatAudioFile << " to " << offBeatAudioFile
                        << "; accentBeatAudioFile from " << customAccentBeatAudioFile << " to " << accentBeatAudioFile;
    customPrimaryBeatAudioFile = QFileInfo::exists(primaryBeatAudioFile) ? primaryBeatAudioFile : "";
    customOffBeatAudioFile = QFileInfo::exists(offBeatAudioFile) ? offBeatAudioFile : "";
    customAccentBeatAudioFile = QFileInfo::exists(accentBeatAudioFile) ? accentBeatAudioFile : "";
    usingCustomSounds = !customPrimaryBeatAudioFile.isEmpty() && !customOffBeatAudioFile.isEmpty() && !customAccentBeatAudioFile.isEmpty();
}

void MetronomeSettings::setBuiltInMetronome(const QString &metronomeAlias)
{
    qCDebug(jtSettings) << "Settings setBuiltInMetronome: from " << builtInMetronomeAlias << " to " << metronomeAlias << " (and not custom sounds)";
    builtInMetronomeAlias = metronomeAlias;
    usingCustomSounds = false;
}
