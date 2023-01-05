#include "ServerInfo.h"
#include "ServerMessages.h"
#include "User.h"
#include "UserChannel.h"
#include "log/Logging.h"

#include <QSet>

using ninjam::client::ServerInfo;
using ninjam::client::UserChannel;
using ninjam::client::User;

ServerInfo::ServerInfo(const QString &host, quint16 port, quint8 maxChannels, quint8 maxUsers) :
    port(port),
    host(host),
    maxUsers(maxUsers),
    bpm(0),
    bpi(0),
    activeServer(true),
    streamUrl(""),
    topic(""),
    maxChannels(maxChannels)
{
}

ServerInfo::~ServerInfo()
{
}

bool ServerInfo::containsUser(const QString &userFullName) const
{
    return users.contains(userFullName);
}

bool ServerInfo::containsUser(const User &user) const
{
    return containsUser(user.getFullName());
}

void ServerInfo::addUser(const User &user)
{
    if (!users.contains(user.getFullName())) {
        users.insert(user.getFullName(), User(user.getFullName()));
    }
}

void ServerInfo::updateUserChannel(const QString &userFullName, const UserChannel &serverChannel)
{
    auto iterator = users.find(userFullName);
    if (iterator != users.end()) {
        auto& user = iterator.value();
        user.visitChannel(serverChannel.getIndex(), [&](UserChannel& channel) {
            channel.setName(serverChannel.getName());
            channel.setActive(serverChannel.isActive());
            channel.setFlags(serverChannel.getFlags());
        });
    }
}

void ServerInfo::updateUserChannelReceiveStatus(const QString &userFullName, quint8 channelIndex, bool receive)
{
    auto iterator = users.find(userFullName);
    if (iterator != users.end()) {
        auto& user = iterator.value();
        user.visitChannel(channelIndex, [&](UserChannel& channel) {
            channel.setActive(receive);
        });
    }
}

void ServerInfo::removeUserChannel(const QString &userFullName, const UserChannel &channel)
{
    auto iterator = users.find(userFullName);
    if (iterator != users.end()) {
        auto& user = iterator.value();
        user.removeChannel(channel.getIndex());
    }
}

void ServerInfo::removeUser(const QString &fullUserName)
{
    users.remove(fullUserName);
}

void ServerInfo::addUserChannel(const QString &userFullName, const UserChannel &newChannel)
{
    if (users.contains(userFullName)) {
        int userChannelsCount = users[userFullName].getChannelsCount();
        if (userChannelsCount < maxChannels)
            users[userFullName].addChannel(newChannel);
        else
            qCritical() << "Can't add more channels for "
                        << userFullName << "(using"
                        << QString::number(userChannelsCount)
                        << " channels). The server maxChannels is "
                        << QString::number(maxChannels);
    }
}

User ServerInfo::getUser(const QString &userFullName) const
{
    if (users.contains(userFullName))
        return users[userFullName];

    return User("");
}

User ServerInfo::getOrCreateUser(const QString &userFullName)
{
    auto iterator = users.find(userFullName);
    if (iterator != users.end()) {
        return iterator.value();
    }
    return users.insert(userFullName, User(userFullName)).value();
}

QList<User> ServerInfo::getUsers() const
{
    return users.values();
}

QString ServerInfo::getUniqueName() const
{
    return host + ":" + QString::number(port);
}

bool ServerInfo::setBpm(quint16 bpm)
{
    if (bpm == this->bpm)
        return false;

    if (bpm >= ServerInfo::MIN_BPM && bpm <= ServerInfo::MAX_BPM) {
        this->bpm = bpm;
        return true;
    }

    return false;
}

bool ServerInfo::setBpi(quint16 bpi)
{
    if (bpi == this->bpi)
        return false;

    if (bpi >= ServerInfo::MIN_BPI && bpi <= ServerInfo::MAX_BPI) {
        this->bpi = bpi;
        return true;
    }

    return false;
}
