#include "PrivateServerSettings.h"
#include "log/Logging.h"
#include <QJsonArray>

using namespace persistence;

PrivateServerSettings::PrivateServerSettings() :
    SettingsObject("PrivateServer")
{
    qCDebug(jtSettings) << "PrivateServerSettings ctor";
    addPrivateServer("localhost", DEFAULT_SERVER_PORT);
}

void PrivateServerSettings::write(QJsonObject &out) const
{
    qCDebug(jtSettings) << "PrivateServerSettings write";
    QJsonArray serversArray;
    for (const QString &server : lastServers) {
        serversArray.append(server);
    }
    out["lastPort"] = lastPort;
    out["lastPassword"] = lastPassword;
    out["lastServers"] = serversArray;
}

void PrivateServerSettings::read(const QJsonObject &in)
{
    if (in.contains("lastServers")) {
        lastServers.clear();
        QJsonArray serversArray = in["lastServers"].toArray();
        for (int serverIndex = 0; serverIndex < serversArray.size(); ++serverIndex) {
            QString serverName = serversArray.at(serverIndex).toString();
            if (!serverName.isEmpty() && !lastServers.contains(serverName))
                lastServers.append(serverName);
        }
    }

    lastPort = getValueFromJson(in, "lastPort", DEFAULT_SERVER_PORT);
    lastPassword = getValueFromJson(in, "lastPassword", QString(""));

    qCDebug(jtSettings) << "PrivateServerSettings: lastServers " << lastServers
                        << "; lastPort " << lastPort
                        << "; lastPassword " << lastPassword;
}

void PrivateServerSettings::addPrivateServer(const QString &serverName, quint16 serverPort, const QString &password)
{
    qCDebug(jtSettings) << "PrivateServerSettings addPrivateServer";
    if (lastServers.contains(serverName))
        lastServers.removeOne(serverName);

    lastServers.insert(0, serverName); // the last used server is the first element in the list

    lastPort = serverPort;
    lastPassword = password;
}
