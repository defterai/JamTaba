#include "VstSettings.h"
#include "log/Logging.h"
#include <QJsonArray>
#include <QFile>

using namespace persistence;

VstSettings::VstSettings() :
    SettingsObject("VST")
{
    qCDebug(jtSettings) << "VstSettings ctor";
}

void VstSettings::write(QJsonObject &out) const
{
    qCDebug(jtSettings) << "VstSettings write";

    QJsonArray scanPathsArray;
    for (const QString &scanPath : foldersToScan)
        scanPathsArray.append(scanPath);

    out["scanPaths"] = scanPathsArray;

    QJsonArray cacheArray;
    for (const QString &pluginPath : cachedPlugins)
        cacheArray.append(pluginPath);

    out["cachedPlugins"] = cacheArray;

    QJsonArray ignoredArray;
    for (const QString &ignoredPlugin: ignoredPlugins)
        ignoredArray.append(ignoredPlugin);

    out["BlackListPlugins"] = ignoredArray;
}

void VstSettings::read(const QJsonObject &in)
{
    foldersToScan.clear();
    if (in.contains("scanPaths")) {
        QJsonArray scanPathsArray = in["scanPaths"].toArray();
        for (int i = 0; i < scanPathsArray.size(); ++i) {
            addPluginScanPath(scanPathsArray.at(i).toString());
        }
    }
    cachedPlugins.clear();
    if (in.contains("cachedPlugins")) {
        QJsonArray cacheArray = in["cachedPlugins"].toArray();
        for (int x = 0; x < cacheArray.size(); ++x) {
            QString pluginFile = cacheArray.at(x).toString();
            if (QFile::exists(pluginFile)) { // if a cached plugin is removed from the disk we need skip this file
                addPlugin(pluginFile);
            }
        }
    }
    ignoredPlugins.clear();
    if (in.contains("BlackListPlugins")) {
        QJsonArray cacheArray = in["BlackListPlugins"].toArray();
        for (int x = 0; x < cacheArray.size(); ++x) {
            addIgnoredPlugin(cacheArray.at(x).toString());
        }
    }

    qCDebug(jtSettings) << "VstSettings: foldersToScan " << foldersToScan
                        << "; cachedPlugins " << cachedPlugins
                        << "; ignoredPlugins " << ignoredPlugins;
}
