#include "WindowSettings.h"
#include "log/Logging.h"

using namespace persistence;

WindowSettings::WindowSettings() :
    SettingsObject("window"),
    maximized(false),
    fullScreenMode(false)
{
    qCDebug(jtSettings) << "WindowSettings ctor";
}

void WindowSettings::read(const QJsonObject &in)
{
    setMaximized(getValueFromJson(in, "maximized", false)); // not maximized as default
    setFullScreenMode(getValueFromJson(in, "fullScreenView", false)); // use normal mode as default;
    if (in.contains("location")) {
        QJsonObject locationObj = in["location"].toObject();
        setLocation(QPointF(getValueFromJson(locationObj, "x", (float)0),
                            getValueFromJson(locationObj, "y", (float)0)));
    }

    if (in.contains("size")) {
        QJsonObject sizeObject = in["size"].toObject();
        setSize(QSize(getValueFromJson(sizeObject, "width", (int)800),
                      getValueFromJson(sizeObject, "height", (int)600)));
    }

    qCDebug(jtSettings) << "WindowSettings: maximized " << maximized
                        << "; fullScreenMode " << fullScreenMode
                        << "; location " << location
                        << "; size " << size;
}

void WindowSettings::write(QJsonObject &out) const
{
    qCDebug(jtSettings) << "WindowSettings write";
    out["maximized"] = maximized;
    out["fullScreenView"] = fullScreenMode;

    QJsonObject locationObject;
    locationObject["x"] = this->location.x();
    locationObject["y"] = this->location.y();
    out["location"] = locationObject;

    QJsonObject sizeObject;
    sizeObject["width"] = this->size.width();
    sizeObject["height"] = this->size.height();
    out["size"] = sizeObject;
}
