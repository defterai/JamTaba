#include "RememberSettings.h"
#include "log/Logging.h"

using namespace persistence;

RememberSettings::RememberSettings() :
    SettingsObject(QStringLiteral("Remember")),
    rememberPan(true),
    rememberBoost(true),
    rememberLevel(true),
    rememberMute(true),
    rememberLowCut(true),
    rememberLocalChannels(true),
    rememberBottomSection(true),
    rememberChatSection(true)
{
    qCDebug(jtSettings) << "RememberSettings ctor";
}

void RememberSettings::write(QJsonObject &out) const
{
    qCDebug(jtSettings) << "RememberSettings write";
    out["boost"] = rememberBoost;
    out["level"] = rememberLevel;
    out["pan"] = rememberPan;
    out["mute"] = rememberMute;
    out["lowCut"] = rememberLowCut;

    out["localChannels"] = rememberLocalChannels;
    out["bottomSection"] = rememberBottomSection;
    out["chatSection"] = rememberChatSection;
}

void RememberSettings::read(const QJsonObject &in)
{
    rememberBoost = getValueFromJson(in, "boost", true);
    rememberLevel = getValueFromJson(in, "level", true);
    rememberPan = getValueFromJson(in, "pan", true);
    rememberMute = getValueFromJson(in, "mute", true);
    rememberLowCut = getValueFromJson(in, "lowCut", true);

    rememberLocalChannels = getValueFromJson(in, "localChannels", true);
    rememberBottomSection = getValueFromJson(in, "bottomSection", false);
    rememberChatSection = getValueFromJson(in, "chatSection", false);

    qCDebug(jtSettings) << "RememberSettings: rememberBoost " << rememberBoost
                        << "; rememberLevel " << rememberLevel
                        << "; rememberPan " << rememberPan
                        << "; rememberMute " << rememberMute
                        << "; rememberLocalChannels " << rememberLocalChannels
                        << "; rememberBottomSection " << rememberBottomSection
                        << "; rememberChatSection " << rememberChatSection;
}
