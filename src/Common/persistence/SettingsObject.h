#ifndef SETTINGSOBJECT_H
#define SETTINGSOBJECT_H

#include <QString>
#include <QJsonObject>
#include <type_traits>

namespace persistence {

class SettingsObject // base class for the settings components
{

public:
    explicit SettingsObject(const QString &name);
    virtual ~SettingsObject();
    virtual void write(QJsonObject &out) const = 0;
    virtual void read(const QJsonObject &in) = 0;
    inline const QString& getName() const
    {
        return name;
    }

protected:
    QString name;

    static int getValueFromJson(const QJsonObject &json, const QString &propertyName, int fallBackValue);
    static float getValueFromJson(const QJsonObject &json, const QString &propertyName,
                                  float fallBackValue);
    static QString getValueFromJson(const QJsonObject &json, const QString &propertyName,
                                    QString fallBackValue);
    static bool getValueFromJson(const QJsonObject &json, const QString &propertyName, bool fallBackValue);
    static QJsonArray getValueFromJson(const QJsonObject &json, const QString &propertyName, QJsonArray fallBackValue);
    static QJsonObject getValueFromJson(const QJsonObject &json, const QString &propertyName, QJsonObject fallBackValue);
    template<class T>
    static auto getEnumUnderlayingValue(T value) {
        return static_cast<std::underlying_type_t<T>>(value);
    }
    template<class T>
    static T getEnumValueFromJson(const QJsonObject &json, const QString &propertyName, T fallBackValue) {
        return static_cast<T>(getValueFromJson(json, propertyName, getEnumUnderlayingValue(fallBackValue)));
    }
};

}

#endif // SETTINGSOBJECT_H
