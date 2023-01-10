#include "LooperSettings.h"
#include "log/Logging.h"
#include <QDir>
#include <QStandardPaths>

using namespace persistence;

static_assert(LooperSettings::DEFAULT_LAYERS_COUNT >= LooperSettings::MIN_LAYERS_COUNT &&
    LooperSettings::DEFAULT_LAYERS_COUNT <= LooperSettings::MAX_LAYERS_COUNT, "Wrong LooperSettings::DEFAULT_LAYERS_COUNT constant value");
static_assert(LooperSettings::MIN_LAYERS_COUNT > 0, "Wrong LooperSettings::MIN_LAYERS_COUNT constant value");
static_assert(LooperSettings::MAX_LAYERS_COUNT >= LooperSettings::MIN_LAYERS_COUNT, "Wrong LooperSettings::MAX_LAYERS_COUNT constant value");

const std::array<LooperMode, 3> LooperSettings::LOOPER_MODES = {
    LooperMode::Sequence,
    LooperMode::AllLayers,
    LooperMode::SelectedLayer,
};

LooperSettings::LooperSettings() :
    SettingsObject("Looper"),
    preferredLayersCount(DEFAULT_LAYERS_COUNT),
    preferredMode(LooperMode::Sequence),
    loopsFolder(""),
    encodingAudioWhenSaving(false),
    waveFilesBitDepth(16) // 16 bits
{
    qCDebug(jtSettings) << "LooperSettings ctor";
    setDefaultLooperFilesPath();
}

void LooperSettings::read(const QJsonObject &in)
{
    setPreferredLayersCount(getValueFromJson(in, "preferresLayersCount", DEFAULT_LAYERS_COUNT));
    setPreferredMode(getEnumValueFromJson(in, "preferredMode", LooperMode::Sequence));
    setLoopsFolder(getValueFromJson(in, "loopsFolder", QString()));
    setEncodingAudioWhenSaving(getValueFromJson(in, "encodeAudio", false));

    quint8 bitDepth = getValueFromJson(in, "bitDepth", quint8(16));
    if (!setWaveFilesBitDepth(bitDepth)) { // 16 bit as default value
         qWarning() << "Invalid bit depth " << bitDepth;
    }

    bool useDefaultSavePath = false;
    if (!loopsFolder.isEmpty()) {
        QDir saveDir(QDir::fromNativeSeparators(loopsFolder));
        if (!saveDir.exists()) {
            qDebug() << "Creating looper save dir " << saveDir;
            if (!saveDir.mkpath(".")) {
                qWarning() << "Dir " << saveDir << " not exists, using the application directory to save looper data!";
                useDefaultSavePath = true;
            }
        }
    } else {
        useDefaultSavePath = true;
    }

    if (useDefaultSavePath) {
       setDefaultLooperFilesPath();
    }

    qCDebug(jtSettings) << "LooperSettings: preferredLayersCount " << preferredLayersCount
                    << "; preferredMode " << getEnumUnderlayingValue(preferredMode)
                    << "; loopsFolder " << loopsFolder
                    << " (useDefaultSavePath " << useDefaultSavePath << ")"
                    << "; encodingAudioWhenSaving " << encodingAudioWhenSaving
                    << "; waveFilesBitDepth " << waveFilesBitDepth;

}

void LooperSettings::setDefaultLooperFilesPath()
{
    qCDebug(jtSettings) << "LooperSettings setDefaultLooperFilesPath";
    QString userDocuments = QStandardPaths::displayName(QStandardPaths::DocumentsLocation);
    QDir pathDir(QDir::homePath());
    QDir documentsDir(pathDir.absoluteFilePath(userDocuments));
    loopsFolder = QDir(documentsDir).absoluteFilePath("JamTaba/Looper");
    QDir saveDir(loopsFolder);
    if (!saveDir.exists()) {
        qDebug() << "Creating looper data folder " << saveDir;
        if (!saveDir.mkpath(".")) {
             qWarning() << "Unable create looper data folder " << saveDir;
        }
    }
}

void LooperSettings::write(QJsonObject &out) const
{
    qCDebug(jtSettings) << "LooperSettings write";
    out["preferresLayersCount"] = preferredLayersCount;
    out["preferredMode"] = getEnumUnderlayingValue(preferredMode);
    out["loopsFolder"] = loopsFolder;
    out["encodeAudio"] = encodingAudioWhenSaving;
    out["bitDepth"] = waveFilesBitDepth;
}

void LooperSettings::setPreferredLayersCount(quint8 value)
{
    if (value < MIN_LAYERS_COUNT)
        preferredLayersCount = MIN_LAYERS_COUNT;
    else if (value > MAX_LAYERS_COUNT)
        preferredLayersCount = MAX_LAYERS_COUNT;
    else
        preferredLayersCount = value;
}

void LooperSettings::setPreferredMode(LooperMode value)
{
    if (value == LooperMode::Sequence ||
            value == LooperMode::AllLayers ||
            value == LooperMode::SelectedLayer) {
        preferredMode = value;
    }
}

bool LooperSettings::setWaveFilesBitDepth(quint8 value) {
    if (waveFilesBitDepth == 16 || waveFilesBitDepth == 32) {
        waveFilesBitDepth = value;
        return true;
    }
    return false;
}
