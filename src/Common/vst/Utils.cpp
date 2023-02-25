#include "Utils.h"
#include <QFileInfo>

static QString getVstPluginString(AEffect *plugin, VstInt32 opcode) {
    Q_ASSERT(plugin != nullptr);
    char buf[256]; // some dumb plugins don't respect kVstMaxVendorStrLen, kVstMaxEffectNameLen
    memset(buf, 0, sizeof(buf)); // make sure no garbage characters returned if 0 terminate character missed
    plugin->dispatcher(plugin, opcode, 0, 0, buf, 0);
    if (buf[sizeof(buf) - 1] == '\0') { // protection from code which overwrite last character with non 0
        return QString::fromUtf8(buf);
    }
    return QString::fromUtf8(buf, sizeof(buf));
}

QString vst::utils::getPluginVendor(AEffect *plugin)
{
    return getVstPluginString(plugin, effGetVendorString);
}

QString vst::utils::getPluginName(AEffect *plugin)
{
    return getVstPluginString(plugin, effGetEffectName);
}

audio::PluginDescriptor vst::utils::createDescriptor(AEffect *plugin, const QString &pluginPath)
{
    auto pluginName = vst::utils::getPluginName(plugin);
    if (pluginName.isEmpty()) {
        pluginName = QFileInfo(pluginPath).baseName();
    }
    auto manufacturer = vst::utils::getPluginVendor(plugin);
    auto category = audio::PluginDescriptor::VST_Plugin;
    return audio::PluginDescriptor(pluginName, category, manufacturer, pluginPath);
}

