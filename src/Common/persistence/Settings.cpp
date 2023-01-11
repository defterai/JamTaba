#include "Settings.h"
#include <QDebug>
#include <QApplication>
#include <QStandardPaths>
#include <QJsonArray>
#include <QJsonObject>
#include <QJsonDocument>

#include <QDir>
#include <QList>
#include <QStringList>
#include <QSettings>
#include "log/Logging.h"
#include "audio/vorbis/Vorbis.h"

using namespace persistence;

#if defined(Q_OS_WIN64) || defined(Q_OS_MAC64)
QString Settings::fileName = "Jamtaba64.json";
#else
QString Settings::fileName = "Jamtaba.json";
#endif

// +++++++++++++++++++++++++++++

MidiSettings::MidiSettings() :
    SettingsObject("midi")
{
    qCDebug(jtSettings) << "MidiSettings ctor";
}

void MidiSettings::write(QJsonObject &out) const
{
    qCDebug(jtSettings) << "MidiSettings write";
    QJsonArray midiArray;

    for (bool state : inputDevicesStatus)
        midiArray.append(state);

    out["inputsState"] = midiArray;
}

void MidiSettings::read(const QJsonObject &in)
{
    inputDevicesStatus.clear();
    if (in.contains("inputsState")) {
        QJsonArray inputsArray = in["inputsState"].toArray();
        for (int i = 0; i < inputsArray.size(); ++i)
            inputDevicesStatus.append(inputsArray.at(i).toBool(true));
    }

    qCDebug(jtSettings) << "MidiSettings: inputDevicesStatus " << inputDevicesStatus;
}

// ++++++++++++++++++++++++++++++

SyncSettings::SyncSettings() :
    SettingsObject("sync")
{
    qCDebug(jtSettings) << "SyncSettings ctor";
}

void SyncSettings::write(QJsonObject &out) const
{
    qCDebug(jtSettings) << "SyncSettings write";
    QJsonArray syncArray;

    for (bool state : syncOutputDevicesStatus)
        syncArray.append(state);

    out["syncOutputsState"] = syncArray;
}

void SyncSettings::read(const QJsonObject &in)
{
    syncOutputDevicesStatus.clear();
    if (in.contains("syncOutputsState")) {
        QJsonArray outputsArray = in["syncOutputsState"].toArray();
        for (int i = 0; i < outputsArray.size(); ++i)
            syncOutputDevicesStatus.append(outputsArray.at(i).toBool(true));
    }

    qCDebug(jtSettings) << "SyncSettings: syncOutputDevicesStatus " << syncOutputDevicesStatus;
}

// ++++++++++++++++++++++++++++++++++++++++++

void Settings::storeUserName(const QString &newUserName)
{
    qCDebug(jtSettings) << "Settings storeUserName: from " << lastUserName << "; to " << newUserName;
    this->lastUserName = newUserName;
}

void Settings::setTranslation(const QString &localeName)
{
    qCDebug(jtSettings) << "Settings setTranslation: from " << translation << "; to " << localeName;
    QString name = localeName;
    if (name.contains(".")){
        name = name.left(name.indexOf("."));
    }
    translation = name;
    qDebug() << "Setting translation to" << translation;
}

// ++++++++++++++++++++++++++++++++++++++++

bool Settings::readFile(const QList<SettingsObject *> &sections)
{
    qCDebug(jtSettings) << "Settings readFile";
    QDir configFileDir = Configurator::getInstance()->getBaseDir();
    QString absolutePath = configFileDir.absoluteFilePath(fileName);
    QFile configFile(absolutePath);

    if (configFile.open(QIODevice::ReadOnly)) {
        QJsonDocument doc = QJsonDocument::fromJson(configFile.readAll());
        QJsonObject root = doc.object();

        if (root.contains("masterGain")) // read last master gain
            this->masterFaderGain = root["masterGain"].toDouble();
        else
            this->masterFaderGain = 1; // unit gain as default

        if (root.contains("userName")) // read user name
            this->lastUserName = root["userName"].toString();

        if (root.contains("translation")) // read Translation
            this->translation = root["translation"].toString();
        if (this->translation.isEmpty())
            this->translation = QLocale().bcp47Name().left(2);

        if (root.contains("theme"))
            this->theme = root["theme"].toString();
        if (this->theme.isEmpty())
            this->theme = "Navy_nm"; //using Navy as the new default theme

        if (root.contains("intervalProgressShape")) // read intervall progress shape
            this->ninjamIntervalProgressShape = root["intervalProgressShape"].toInt(0); // zero as default value
        else
            this->ninjamIntervalProgressShape = 0;

        tracksLayoutOrientation = root["tracksLayoutOrientation"].toInt(0); // vertical as fallback value;

        if (root.contains("usingNarrowTracks"))
            this->usingNarrowedTracks = root["usingNarrowTracks"].toBool(false);
        else
            this->usingNarrowedTracks = false;

        if (root.contains("publicChatActivated")) {
            publicChatActivated = root["publicChatActivated"].toBool(true);
        }

        // read settings sections (Audio settings, Midi settings, ninjam settings, etc...)
        for (SettingsObject *so : sections)
            so->read(root[so->getName()].toObject());

        if(root.contains("intervalsBeforeInactivityWarning")) {
            intervalsBeforeInactivityWarning = root["intervalsBeforeInactivityWarning"].toInt();
            if (intervalsBeforeInactivityWarning < 1)
                intervalsBeforeInactivityWarning = 0;
        }

        if (root.contains("recentEmojis")) {
            QJsonArray array = root["recentEmojis"].toArray();
            for (int i = 0; i < array.count(); ++i) {
                recentEmojis << array.at(i).toString();
            }

        }

        if (root.contains("chatFontSizeOffset")) {
            chatFontSizeOffset = root["chatFontSizeOffset"].toInt();
        }

        return true;
    }
    else {
        qWarning(jtConfigurator) << "Settings : Can't load Jamtaba 2 config file:"
                                 << configFile.errorString();
    }

    return false;
}

bool Settings::writeFile(const QList<SettingsObject *> &sections) // io ops ...
{
    qCDebug(jtSettings) << "Settings writeFile...";
    QDir configFileDir = Configurator::getInstance()->getBaseDir();
    QFile file(configFileDir.absoluteFilePath(fileName));
    if (file.open(QIODevice::WriteOnly)) {
        QJsonObject root;

        // writing global settings
        root["userName"] = lastUserName; // write user name
        root["translation"] = translation; // write translate locale
        root["theme"] = theme;
        root["intervalProgressShape"] = ninjamIntervalProgressShape;
        root["tracksLayoutOrientation"] = tracksLayoutOrientation;
        root["usingNarrowTracks"] = usingNarrowedTracks;
        root["masterGain"] = masterFaderGain;
        root["intervalsBeforeInactivityWarning"] = static_cast<int>(intervalsBeforeInactivityWarning);
        root["chatFontSizeOffset"] = static_cast<int>(chatFontSizeOffset);
        root["publicChatActivated"] = publicChatIsActivated();

        if (!recentEmojis.isEmpty()) {
            root["recentEmojis"] = QJsonArray::fromStringList(recentEmojis);
        }

        // write settings sections
        for (SettingsObject *so : sections) {
            QJsonObject sectionObject;
            so->write(sectionObject);
            root[so->getName()] = sectionObject;
        }
        QJsonDocument doc(root);
        file.write(doc.toJson());
        qCDebug(jtCore) << "Settings writeFile: " << doc;
        return true;
    } else {
        qCritical() << file.errorString();
    }
    return false;
}

// PRESETS
bool Settings::writePresetToFile(const Preset &preset)
{
    qCDebug(jtSettings) << "Settings writePresetToFile...";
    QString absolutePath = Configurator::getInstance()->getPresetPath(preset.name);
    QFile file(absolutePath);
    if (file.open(QIODevice::WriteOnly)) {
        QJsonObject inputTracksJsonObject;
        preset.inputTrackSettings.write(inputTracksJsonObject); // write the channels and subchannels in the json object

        QJsonObject root;
        root[preset.name] = inputTracksJsonObject;

        QJsonDocument doc(root);
        file.write(doc.toJson());
        qCDebug(jtSettings) << "Settings writePresetToFile: " << doc;
        return true;
    } else {
        qCritical() << file.errorString();
    }
    return false;
}

// ++++++++++++++++++++++++++++++
Preset::Preset(const QString &name, const LocalInputTrackSettings &inputSettings) :
    inputTrackSettings(inputSettings),
    name(name)
{
    qCDebug(jtSettings) << "Preset ctor";
}

// ++++++++++++++
Preset Settings::readPresetFromFile(const QString &presetFileName, bool allowMultiSubchannels)
{
    qCDebug(jtSettings) << "Preset readPresetFromFile";
    QString absolutePath = Configurator::getInstance()->getPresetPath(presetFileName);
    QFile presetFile(absolutePath);
    if (presetFile.open(QIODevice::ReadOnly)) {
        QJsonDocument doc = QJsonDocument::fromJson(presetFile.readAll());
        QJsonObject root = doc.object();
        QString presetName = "default";
        if (!root.keys().isEmpty()) {
            presetName = root.keys().first();
            Preset preset(presetName);
            preset.inputTrackSettings.read(root[presetName].toObject(), allowMultiSubchannels);
            return preset;
        }
    } else {
        qWarning(jtConfigurator) << "Settings : Can't load PRESET file:"
                                 << presetFile.errorString();
    }
    return Preset(); // returning an empty/invalid preset
}

// ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

void Settings::load()
{
    qCDebug(jtSettings) << "Settings load";
    QList<persistence::SettingsObject *> sections;
    sections.append(&audioSettings);
    sections.append(&midiSettings);
    sections.append(&syncSettings);
    sections.append(&windowSettings);
    sections.append(&metronomeSettings);
    sections.append(&vstSettings);
#ifdef Q_OS_MAC
    sections.append(&audioUnitSettings);
#endif
    sections.append(&inputsSettings);
    sections.append(&recordingSettings);
    sections.append(&privateServerSettings);
    sections.append(&meteringSettings);
    sections.append(&looperSettings);
    sections.append(&rememberSettings);
    sections.append(&collapseSettings);

    readFile(sections);
}

Settings::Settings() :
    translation("en"), // english as default language
    theme("Navy_nm"), // flat as default theme,
    ninjamIntervalProgressShape(0),
    masterFaderGain(1.0), 
    tracksLayoutOrientation(Qt::Vertical),
    usingNarrowedTracks(false),
    publicChatActivated(true),
    intervalsBeforeInactivityWarning(5), // 5 intervals by default,
    chatFontSizeOffset(0)
{
    qCDebug(jtSettings) << "Settings ctor";
    // qDebug() << "Settings in " << fileDir;
}

void Settings::save(const LocalInputTrackSettings &localInputsSettings)
{
    qCDebug(jtSettings) << "Settings save";
    this->inputsSettings = localInputsSettings;
    QList<persistence::SettingsObject *> sections;
    sections.append(&audioSettings);
    sections.append(&midiSettings);
    sections.append(&syncSettings);
    sections.append(&windowSettings);
    sections.append(&metronomeSettings);
    sections.append(&vstSettings);
#ifdef Q_OS_MAC
    sections.append(&audioUnitSettings);
#endif
    sections.append(&inputsSettings);
    sections.append(&recordingSettings);
    sections.append(&privateServerSettings);
    sections.append(&meteringSettings);
    sections.append(&looperSettings);
    sections.append(&rememberSettings);
    sections.append(&collapseSettings);

    writeFile(sections);
}

void Settings::deletePreset(const QString &name)
{
    qCDebug(jtSettings) << "Settings deletePreset " << name;
    Configurator::getInstance()->deletePreset(name);
}

Settings::~Settings()
{
    // qCDebug(jtSettings) << "Settings destructor";
}

void Settings::setTheme(const QString theme)
{
    qCDebug(jtSettings) << "Settings setTheme: from " << this->theme << " to " << theme;
    this->theme = theme;
}

// ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

QString Settings::getTranslation() const
{
    // qCDebug(jtSettings) << "Settings getTranslation";
    return translation;
}

//________________________________________________________________


