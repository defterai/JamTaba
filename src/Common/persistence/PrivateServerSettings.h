#ifndef PRIVATESERVERSETTINGS_H
#define PRIVATESERVERSETTINGS_H

#include "SettingsObject.h"

namespace persistence {

class PrivateServerSettings final : public SettingsObject
{
public:
    static const quint16 DEFAULT_SERVER_PORT = 2049;

    PrivateServerSettings();
    void write(QJsonObject &out) const override;
    void read(const QJsonObject &in) override;
    void addPrivateServer(const QString &serverName, quint16 serverPort, const QString &password = QString());
    inline const QList<QString>& getLastServers() const
    {
        return lastServers;
    }
    inline quint16 getLastPort() const
    {
        return lastPort;
    }
    inline const QString& getLastPassword() const
    {
        return lastPassword;
    }
private:
    QList<QString> lastServers;
    quint16 lastPort;
    QString lastPassword;
};

}

#endif // PRIVATESERVERSETTINGS_H
