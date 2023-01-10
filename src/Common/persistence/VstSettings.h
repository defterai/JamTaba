#ifndef VSTSETTINGS_H
#define VSTSETTINGS_H

#include "SettingsObject.h"

namespace persistence {

class VstSettings final : public SettingsObject
{
public:
    VstSettings();
    void write(QJsonObject &out) const override;
    void read(const QJsonObject &in) override;
    inline const QStringList& getPluginPaths() const
    {
        return cachedPlugins;
    }
    inline void addPlugin(const QString& pluginPath)
    {
        if (!pluginPath.isEmpty() && !cachedPlugins.contains(pluginPath)) {
            cachedPlugins.append(pluginPath);
        }
    }
    inline void clearPluginsCache()
    {
        cachedPlugins.clear();
    }
    inline const QStringList& getPluginScanPaths() const
    {
        return foldersToScan;
    }
    inline void addPluginScanPath(const QString &path)
    {
        if (!path.isEmpty() && !foldersToScan.contains(path)) {
            foldersToScan.append(path);
        }
    }
    inline void removePluginScanPath(const QString &path)
    {
        foldersToScan.removeOne(path);
    }
    inline const QStringList& getIgnoredPlugins() const
    {
        return ignoredPlugins;
    }
    inline void addIgnoredPlugin(const QString& pluginPath)
    {
        if (!pluginPath.isEmpty() && !ignoredPlugins.contains(pluginPath)) {
            ignoredPlugins.append(pluginPath);
        }
    }
    inline void removeIgnoredPlugin(const QString &pluginPath)
    {
        ignoredPlugins.removeOne(pluginPath);
    }
    inline void clearIgnoredPlugins()
    {
        ignoredPlugins.clear();
    }
private:
    QStringList cachedPlugins;
    QStringList foldersToScan;
    QStringList ignoredPlugins;
};

}

#endif // VSTSETTINGS_H
