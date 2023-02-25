#ifndef REMEMBERSETTINGS_H
#define REMEMBERSETTINGS_H

#include "SettingsObject.h"

namespace persistence {

class RememberSettings final : public SettingsObject
{
public:
    RememberSettings();
    void write(QJsonObject &out) const override;
    void read(const QJsonObject &in) override;
    inline bool isRememberingPan() const
    {
        return rememberPan;
    }
    inline void setRememberingPan(bool value)
    {
        rememberPan = value;
    }
    inline bool isRememberingBoost() const
    {
        return rememberBoost;
    }
    inline void setRememberingBoost(bool value)
    {
        rememberBoost = value;
    }
    inline bool isRememberingLevel() const
    {
        return rememberLevel;
    }
    inline void setRememberingLevel(bool value)
    {
        rememberLevel = value;
    }
    inline bool isRememberingMute() const
    {
        return rememberMute;
    }
    inline void setRememberingMute(bool value)
    {
        rememberMute = value;
    }
    inline bool isRememberingLowCut() const
    {
        return rememberLowCut;
    }
    inline void setRememberingLowCut(bool value)
    {
        rememberLowCut = value;
    }
    inline bool isRememberLocalChannels() const
    {
        return rememberLocalChannels;
    }
    inline void setRememberLocalChannels(bool value)
    {
        rememberLocalChannels = value;
    }
    inline bool isRememberBottomSection() const
    {
        return rememberBottomSection;
    }
    inline void setRememberBottomSection(bool value)
    {
        rememberBottomSection = value;
    }
    inline bool isRememberChatSection() const
    {
        return rememberChatSection;
    }
    inline void setRememberChatSection(bool value)
    {
        rememberChatSection = value;
    }
private:
    // user settings
    bool rememberPan;
    bool rememberBoost;
    bool rememberLevel; // fader
    bool rememberMute;
    bool rememberLowCut;

    // collapsible section settings
    bool rememberLocalChannels; // local channels are collapsed?
    bool rememberBottomSection; // bottom section (master fader) is collapsed?
    bool rememberChatSection; // chat is collapsed?
};

}

#endif // REMEMBERSETTINGS_H
