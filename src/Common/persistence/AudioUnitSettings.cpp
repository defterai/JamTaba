#include "AudioUnitSettings.h"
#include "log/Logging.h"
#include <QJsonArray>
#include <QFile>

using namespace persistence;

AudioUnitSettings::AudioUnitSettings() :
    SettingsObject("AU")
{
    qCDebug(jtSettings) << "AudioUnitSettings ctor";
}

void AudioUnitSettings::write(QJsonObject &out) const
{
    qCDebug(jtSettings) << "AudioUnitSettings write";
    QJsonArray cacheArray;
    for (const QString &pluginPath : cachedPlugins) {
        cacheArray.append(pluginPath);
    }
    out["cachedPlugins"] = cacheArray;
}

void AudioUnitSettings::read(const QJsonObject &in)
{
    cachedPlugins.clear();
    if (in.contains("cachedPlugins")) {
        QJsonArray cacheArray = in["cachedPlugins"].toArray();
        for (int x = 0; x < cacheArray.size(); ++x) {
            QString pluginFile = cacheArray.at(x).toString();
            if (!pluginFile.isEmpty() && QFile::exists(pluginFile)) {
                addPlugin(pluginFile);
            }
        }
    }
    qCDebug(jtSettings) << "AudioUnitSettings: cachedPlugins " << cachedPlugins;
}

void AudioUnitSettings::addPlugin(const QString &pluginPath)
{
    qCDebug(jtSettings) << "AudioUnitSettings addPlugin: " << pluginPath;
    if (!pluginPath.isEmpty() && !cachedPlugins.contains(pluginPath)) {
        cachedPlugins.append(pluginPath);
    }
}

void AudioUnitSettings::clearPluginsCache()
{
    qCDebug(jtSettings) << "AudioUnitSettings clearPluginsCache";
    cachedPlugins.clear();
}
