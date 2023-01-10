#ifndef AUDIOUNITSETTINGS_H
#define AUDIOUNITSETTINGS_H

#include "SettingsObject.h"

namespace persistence {

class AudioUnitSettings final : public SettingsObject
{
public:
    AudioUnitSettings();
    void write(QJsonObject &out) const override;
    void read(const QJsonObject &in) override;
    inline const QStringList& getPluginPaths() const
    {
         return cachedPlugins;
    }
    void addPlugin(const QString &pluginPath);
    void clearPluginsCache();
private:
    QStringList cachedPlugins;
};

}

#endif // AUDIOUNITSETTINGS_H
