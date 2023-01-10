#ifndef COLLAPSESETTINGS_H
#define COLLAPSESETTINGS_H

#include "SettingsObject.h"

namespace persistence {

class CollapseSettings final : public SettingsObject
{
public:
    CollapseSettings();
    void write(QJsonObject &out) const override;
    void read(const QJsonObject &in) override;
    inline bool isLocalChannelsCollapsed() const
    {
        return localChannelsCollapsed;
    }
    inline void setLocalChannelsCollapsed(bool value)
    {
        localChannelsCollapsed = value;
    }
    inline bool isBottomSectionCollapsed() const
    {
        return bottomSectionCollapsed;
    }
    inline void setBottomSectionCollapsed(bool value)
    {
        bottomSectionCollapsed = value;
    }
    inline bool isChatSectionCollapsed() const
    {
        return chatSectionCollapsed;
    }
    inline void setChatSectionCollapsed(bool value)
    {
        chatSectionCollapsed = value;
    }
private:
    bool localChannelsCollapsed;
    bool bottomSectionCollapsed;
    bool chatSectionCollapsed;
};

}

#endif // COLLAPSESETTINGS_H
