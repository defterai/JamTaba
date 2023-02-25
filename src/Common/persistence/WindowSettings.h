#ifndef WINDOWSETTINGS_H
#define WINDOWSETTINGS_H

#include "SettingsObject.h"
#include <QPointF>
#include <QSize>

namespace persistence {

class WindowSettings : public SettingsObject
{
public:
    WindowSettings();
    void write(QJsonObject &out) const override;
    void read(const QJsonObject &in) override;
    inline const QPointF& getLocation() const
    {
        return location;
    }
    inline void setLocation(const QPointF& value)
    {
        location.setX((value.x() >= 0) ? value.x() : 0);
        location.setY((value.y() >= 0) ? value.y() : 0);
    }
    inline const QSize& getSize() const
    {
        return size;
    }
    inline void setSize(const QSize& value)
    {
        size.setWidth((value.width() >= 0) ? value.width() : 0);
        size.setHeight((value.height() >= 0) ? value.height() : 0);
    }
    inline bool isMaximized() const
    {
        return maximized;
    }
    inline void setMaximized(bool value)
    {
        maximized = value;
    }
    inline bool isFullScreenMode() const
    {
        return fullScreenMode;
    }
    inline void setFullScreenMode(bool value)
    {
        fullScreenMode = value;
    }
private:
    QPointF location;
    QSize size;
    bool maximized;
    bool fullScreenMode;
};

}

#endif // WINDOWSETTINGS_H
