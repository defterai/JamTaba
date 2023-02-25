#include "SettingsObject.h"
#include "log/Logging.h"

#include <QJsonArray>
#include <QJsonObject>

using namespace persistence;

SettingsObject::SettingsObject(const QString &name) :
    name(name)
{
    qCDebug(jtSettings) << "SettingsObject ctor";
}

SettingsObject::~SettingsObject()
{
    // qCDebug(jtSettings) << "SettingsObject destroyed";
}

int SettingsObject::getValueFromJson(const QJsonObject &json, const QString &propertyName,
                                     int fallBackValue)
{
    auto val = json.contains(propertyName) ? json[propertyName].toInt() : fallBackValue;
    qCDebug(jtSettings) << "SettingsObject getValueFromJson: " << propertyName << " = " << val;

    return val;
}

bool SettingsObject::getValueFromJson(const QJsonObject &json, const QString &propertyName,
                                      bool fallBackValue)
{
    auto val = json.contains(propertyName) ? json[propertyName].toBool() : fallBackValue;
    qCDebug(jtSettings) << "SettingsObject getValueFromJson: " << propertyName << " = " << val;

    return val;
}

QString SettingsObject::getValueFromJson(const QJsonObject &json, const QString &propertyName,
                                         QString fallBackValue)
{
    auto val = json.contains(propertyName) ? json[propertyName].toString() : fallBackValue;
    qCDebug(jtSettings) << "SettingsObject getValueFromJson: " << propertyName << " = " << val;
    return val;
}

float SettingsObject::getValueFromJson(const QJsonObject &json, const QString &propertyName,
                                       float fallBackValue)
{
    auto val = json.contains(propertyName) ? static_cast<float>(json[propertyName].toDouble()) : fallBackValue;
    qCDebug(jtSettings) << "SettingsObject getValueFromJson: " << propertyName << " = " << val;
    return val;
}

QJsonArray SettingsObject::getValueFromJson(const QJsonObject &json, const QString &propertyName,
                                           QJsonArray fallBackValue)
{
    qCDebug(jtSettings) << "SettingsObject getValueFromJson: propertyName = " << propertyName;

    if (json.contains(propertyName))
        return json[propertyName].toArray();

    return fallBackValue;
}

QJsonObject SettingsObject::getValueFromJson(const QJsonObject &json, const QString &propertyName,
                                           QJsonObject fallBackValue)
{
    qCDebug(jtSettings) << "SettingsObject getValueFromJson: propertyName = " << propertyName;

    if (json.contains(propertyName))
        return json[propertyName].toObject();

    return fallBackValue;
}
