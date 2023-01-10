#include "CollapseSettings.h"
#include "log/Logging.h"

using namespace persistence;

CollapseSettings::CollapseSettings() :
    SettingsObject("Collapse"),
    localChannelsCollapsed(false),
    bottomSectionCollapsed(false),
    chatSectionCollapsed(false)
{
    qCDebug(jtSettings) << "CollapseSettings ctor";
}

void CollapseSettings::read(const QJsonObject &in)
{
    localChannelsCollapsed = getValueFromJson(in, "localChannels", false);
    bottomSectionCollapsed = getValueFromJson(in, "bottomSection", false);
    chatSectionCollapsed = getValueFromJson(in, "chatSection", false);

    qCDebug(jtSettings) << "CollapseSettings: localChannelsCollapsed " << localChannelsCollapsed
                        << "; bottomSectionCollapsed" << bottomSectionCollapsed
                        << "; chatSectionCollapsed " << chatSectionCollapsed;
}

void CollapseSettings::write(QJsonObject &out) const
{
    qCDebug(jtSettings) << "CollapseSettings write";
    out["localChannels"] = localChannelsCollapsed;
    out["bottomSection"] = bottomSectionCollapsed;
    out["chatSection"] = chatSectionCollapsed;
}
