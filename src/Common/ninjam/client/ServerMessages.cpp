#include "ServerMessages.h"
#include "ClientMessages.h"
#include <QDebug>
#include <QDataStream>
#include "ninjam/client/UserChannel.h"
#include "ninjam/client/User.h"
#include "ninjam/client/Service.h"
#include "ninjam/Ninjam.h"

#include <cstring>

// ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++=
// +++++++++++++  SERVER MESSAGE (Base class) +++++++++++++++=
// ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++=

using ninjam::client::ServerMessage;
using ninjam::client::AuthChallengeMessage;
using ninjam::client::AuthReplyMessage;
using ninjam::client::ServerToClientChatMessage;
using ninjam::client::ConfigChangeNotifyMessage;
using ninjam::client::UserInfoChangeNotifyMessage;
using ninjam::client::DownloadIntervalBegin;
using ninjam::client::DownloadIntervalBegin;
using ninjam::client::DownloadIntervalWrite;
using ninjam::client::UploadIntervalBegin;
using ninjam::client::UploadIntervalWrite;
using ninjam::client::ChatCommandType;
using ninjam::client::User;
using ninjam::client::UserChannel;
using ninjam::MessageType;

// +++++++++++++++++++++  SERVER AUTH CHALLENGE+++++++++++++++

AuthChallengeMessage::AuthChallengeMessage() :
    ServerMessage(MessageType::AuthChallenge),
    challenge(8, Qt::Uninitialized)
{

}

AuthChallengeMessage::AuthChallengeMessage(const QByteArray &challenge, const QString &licence, quint32 serverCapabilities, quint32 protocolVersion) :
    ServerMessage(MessageType::AuthChallenge),
    challenge(challenge),
    licenceAgreement(licence),
    serverCapabilities(serverCapabilities),
    protocolVersion(protocolVersion)
{

}

quint32 AuthChallengeMessage::getSerializePayload() const
{
    quint32 result = NinjamOutputDataStream::getByteArrayPayload(challenge) +
            NinjamOutputDataStream::getPayload<quint32>() +
            NinjamOutputDataStream::getPayload<quint32>();
    if ((serverCapabilities & 1) != 0) {
        result += NinjamOutputDataStream::getUtf8StringPayload(licenceAgreement);
    }
    return result;
}

bool AuthChallengeMessage::serializeTo(NinjamOutputDataStream& stream) const
{
    if (ServerMessage::serializeTo(stream) &&
            stream.writeByteArray(challenge) &&
            stream.write<quint32>(serverCapabilities) &&
            stream.write<quint32>(protocolVersion)) {
        if ((serverCapabilities & 1) != 0) {
            return stream.writeUtf8String(licenceAgreement);
        }
        return true;
    }
    return false;
}

bool AuthChallengeMessage::unserializeFrom(NinjamInputDataStream& stream)
{
    if (stream.readByteArray(challenge) &&
            stream.read<quint32>(serverCapabilities) &&
            stream.read<quint32>(protocolVersion)) {
        // If the Server Capabilities field has bit 0 set then the License Agreement is present.
        if ((serverCapabilities & 1) != 0) {
            return stream.readUtf8String(licenceAgreement);
        }
        licenceAgreement.clear();
        return true;
    }
    return false;
}

void AuthChallengeMessage::printDebug(QDebug &dbg) const
{
    dbg << "RECEIVED ServerAuthChallengeMessage{" << Qt::endl
        << "\t challenge=" << getChallenge() << Qt::endl
        << "\t protocolVersion=" << getProtocolVersion()<< Qt::endl
        << "\t serverKeepAlivePeriod=" << getServerKeepAlivePeriod() << Qt::endl
        <<"}" << Qt::endl;
}

quint32 AuthChallengeMessage::getServerKeepAlivePeriod() const
{
    /**
        The Server Capabilities field bits 8-15 contains the client keepalive interval in seconds.
        The client sends a Keepalive message if it has sent no messages for the interval.
    */

    return (serverCapabilities >> 8) & 0xff;
}

// ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++=
// +++++++++++++++++++++  SERVER AUTH REPLY ++++++++++++++++++
// ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++=
AuthReplyMessage::AuthReplyMessage() :
    ServerMessage(MessageType::AuthReply)
{

}

AuthReplyMessage::AuthReplyMessage(quint8 flag, const QString &message, quint8 maxChannels) :
    ServerMessage(MessageType::AuthReply),
    flag(flag),
    message(message),
    maxChannels(maxChannels)
{

}

quint32 AuthReplyMessage::getSerializePayload() const
{
    return 2 * NinjamOutputDataStream::getPayload<quint8>() +
            NinjamOutputDataStream::getUtf8StringPayload(message);
}

bool AuthReplyMessage::serializeTo(NinjamOutputDataStream& stream) const
{
    return (ServerMessage::serializeTo(stream) &&
            stream.write<quint8>(flag) &&
            stream.writeUtf8String(message) &&
            stream.write<quint8>(maxChannels));
}

bool AuthReplyMessage::unserializeFrom(NinjamInputDataStream& stream)
{
    return (stream.read<quint8>(flag) &&
            stream.hasRemainingPayload(NinjamOutputDataStream::getPayload<quint8>()) &&
            stream.readUtf8StringFixed(message, stream.getRemainingPayload() -
                                       NinjamOutputDataStream::getPayload<quint8>()) &&
            stream.read<quint8>(maxChannels));
}

void AuthReplyMessage::printDebug(QDebug &debug) const
{
    debug << "RECEIVED ServerAuthReply{ flag=" << flag
          << " errorMessage='" << message
          << "' maxChannels=" << maxChannels
          << '}'
          << Qt::endl;
}

// ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++=
// +++++++++++++++++++++  SERVER CONFIG CHANGE NOTIFY ++++++++
// ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++=
ConfigChangeNotifyMessage::ConfigChangeNotifyMessage() :
    ServerMessage(MessageType::ServerConfigChangeNotify)
{

}

ConfigChangeNotifyMessage::ConfigChangeNotifyMessage(quint16 bpm, quint16 bpi) :
    ServerMessage(MessageType::ServerConfigChangeNotify),
    bpm(bpm),
    bpi(bpi)
{

}

quint32 ConfigChangeNotifyMessage::getSerializePayload() const
{
    return 2 * NinjamOutputDataStream::getPayload<quint16>();
}

bool ConfigChangeNotifyMessage::serializeTo(NinjamOutputDataStream& stream) const
{
    return (ServerMessage::serializeTo(stream) &&
            stream.write<quint16>(bpm) &&
            stream.write<quint16>(bpi));
}

bool ConfigChangeNotifyMessage::unserializeFrom(NinjamInputDataStream& stream)
{
    return (stream.read<quint16>(bpm) &&
            stream.read<quint16>(bpi));
}

void ConfigChangeNotifyMessage::printDebug(QDebug &dbg) const
{
    dbg << "RECEIVE ConfigChangeNotify{ bpm=" << bpm
        << ", bpi=" << bpi
        << '}'
        << Qt::endl;
}

// ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++=
// +++++++++++++++++++++  SERVER USER INFO CHANGE NOTIFY +++++
// ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++=
UserInfoChangeNotifyMessage::UserInfoChangeNotifyMessage() :
    ServerMessage(MessageType::UserInfoChangeNorify)
{

}

void UserInfoChangeNotifyMessage::addUserChannel(const QString &userFullName, const UserChannel &channel)
{
    userChannels.insert(userFullName, channel);
}

UserInfoChangeNotifyMessage UserInfoChangeNotifyMessage::buildDeactivationMessage(const ninjam::client::User &user)
{
    UserInfoChangeNotifyMessage msg;

    for (const UserChannel &channel : user.getChannels()) {
        QString userFullName = user.getFullName();
        msg.addUserChannel(userFullName, UserChannel(channel.getName(), channel.getIndex(), channel.getFlags(), false));
    }

    return msg;
}

UserInfoChangeNotifyMessage::~UserInfoChangeNotifyMessage()
{
    // qWarning() << "destrutor UserInfoChangeNotifyMessage";
}

quint32 UserInfoChangeNotifyMessage::getSerializePayload() const
{
    quint32 payload = 0;
    for (auto iterator = userChannels.begin(); iterator != userChannels.end(); ++iterator) {
        payload += getSerializeUserChannelPayload(iterator.key(), iterator.value());
    }
    return payload;
}

bool UserInfoChangeNotifyMessage::serializeTo(NinjamOutputDataStream& stream) const
{
    if (ServerMessage::serializeTo(stream)) {
        for (auto iterator = userChannels.begin(); iterator != userChannels.end(); ++iterator) {
            if (!serializeUserChannelTo(stream, iterator.key(), iterator.value())) {
                return false;
            }
        }
        return true;
    }
    return false;
}

bool UserInfoChangeNotifyMessage::unserializeFrom(NinjamInputDataStream& stream)
{
    while (stream.getRemainingPayload() != 0) {
        quint8 active;
        quint8 channelIndex;
        quint16 volume;
        quint8 pan;
        quint8 flags;
        QString userFullName;
        QString channelName;
        if (stream.read<quint8>(active) &&
                stream.read<quint8>(channelIndex) &&
                stream.read<quint16>(volume) &&
                stream.read<quint8>(pan) &&
                stream.read<quint8>(flags) &&
                stream.readUtf8String(userFullName) &&
                stream.readUtf8String(channelName)) {
            bool channelIsActive = active > 0;
            userChannels.insert(userFullName, UserChannel(channelName, channelIndex,
                                                          static_cast<UserChannel::Flags>(flags),
                                                          channelIsActive, volume, pan));
        } else {
            return false;
        }
    }
    return true;
}

void UserInfoChangeNotifyMessage::printDebug(QDebug &dbg) const
{
    dbg << "UserInfoChangeNotify{" << Qt::endl;
    for (auto iterator = userChannels.begin(); iterator != userChannels.end(); ++iterator) {
        const QString& userFullName = iterator.key();
         const UserChannel& channel = iterator.value();
        dbg << " ->" << userFullName << Qt::endl;
        dbg << "  ->>" << channel.getIndex() << " - " << channel.getName() <<
               " active:" << channel.isActive() <<
               " flags:" << static_cast<quint8>(channel.getFlags()) << Qt::endl;
        dbg << Qt::endl;
    }
    dbg << "}" << Qt::endl;
}

quint32 UserInfoChangeNotifyMessage::getSerializeUserChannelPayload(const QString &userName, const UserChannel &channel)
{
    return 4 * NinjamOutputDataStream::getPayload<quint8>() +
            NinjamOutputDataStream::getPayload<quint16>() +
            NinjamOutputDataStream::getUtf8StringPayload(userName) +
            NinjamOutputDataStream::getUtf8StringPayload(channel.getName());
}

bool UserInfoChangeNotifyMessage::serializeUserChannelTo(NinjamOutputDataStream& stream, const QString& userName, const UserChannel &channel)
{
    return (stream.write<quint8>(channel.isActive() ? 1 : 0) &&
            stream.write<quint8>(channel.getIndex()) &&
            stream.write<quint16>(0) && // not sending volume
            stream.write<quint8>(0) && // not sending pan
            stream.write<quint8>(static_cast<quint8>(channel.getFlags())) &&
            stream.writeUtf8String(userName) &&
            stream.writeUtf8String(channel.getName()));
}

// +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
// +++++++++++++++++ SERVER CHAT MESSAGE +++++++++++++++++++++
// +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

ServerToClientChatMessage::ServerToClientChatMessage() :
    ServerMessage(MessageType::ChatMessage)
{

}

ServerToClientChatMessage::ServerToClientChatMessage(const QString &command, const QString &arg1, const QString &arg2, const QString &arg3, const QString &arg4) :
    ServerMessage(MessageType::ChatMessage),
    command(command)
{
    arguments.append(arg1);
    arguments.append(arg2);
    arguments.append(arg3);
    arguments.append(arg4);
}

ServerToClientChatMessage ServerToClientChatMessage::buildVoteSystemMessage(const QString &message)
{
    return ServerToClientChatMessage("MSG", QString(), QString("[voting system] %1").arg(message), QString(), QString());
}

ServerToClientChatMessage ServerToClientChatMessage::buildPrivateMessage(const QString &destinationUserName, const QString &message)
{
    return ServerToClientChatMessage("PRIVMSG", destinationUserName, message);
}

ServerToClientChatMessage ServerToClientChatMessage::buildPublicMessage(const QString &userName, const QString &message)
{
    return ServerToClientChatMessage("MSG", userName, message);
}

ServerToClientChatMessage ServerToClientChatMessage::buildTopicMessage(const QString &topic)
{
    return ServerToClientChatMessage("TOPIC", QString(), topic, QString(), QString());
}

ServerToClientChatMessage ServerToClientChatMessage::buildUserJoinMessage(const QString &userName)
{
    return ServerToClientChatMessage("JOIN", userName, QString(), QString(), QString());
}

ServerToClientChatMessage ServerToClientChatMessage::buildUserPartMessage(const QString &userName)
{
    return ServerToClientChatMessage("PART", userName, QString(), QString(), QString());
}

ChatCommandType ServerToClientChatMessage::commandTypeFromString(const QString &string)
{
    if (string == "MSG")
        return ChatCommandType::MSG;

    if (string == "PRIVMSG")
        return ChatCommandType::PRIVMSG;

    if (string == "TOPIC")
        return ChatCommandType::TOPIC;

    if (string == "JOIN")
        return ChatCommandType::JOIN;

    if (string == "PART")
        return ChatCommandType::PART;

    return ChatCommandType::USERCOUNT; /*if(string == "USERCOUNT")*/
}

/*
 Offset Type Field
 0x0    ...  Command (NUL-terminated)
 a+0x0  ...  Argument 1 (NUL-terminated)
 b+0x0  ...  Argument 2 (NUL-terminated)
 c+0x0  ...  Argument 3 (NUL-terminated)
 d+0x0  ...  Argument 4 (NUL-terminated)

 The server-to-client commands are:
 MSG <username> <text> -- a broadcast message
 PRIVMSG <username> <text> -- a private message
 TOPIC <username> <text> -- server topic change
 JOIN <username> -- user enters server
 PART <username> -- user leaves server
 USERCOUNT <users> <maxusers> -- server status
 */
quint32 ServerToClientChatMessage::getSerializePayload() const
{
    quint32 payload = NinjamOutputDataStream::getUtf8StringPayload(command);
    for (const QString &argument : arguments) {
        payload += NinjamOutputDataStream::getUtf8StringPayload(argument);
    }
    return payload;
}

bool ServerToClientChatMessage::serializeTo(NinjamOutputDataStream& stream) const
{
    if (arguments.size() != 4) {
        return false; // not allowed write invalid messages
    }
    if (ServerMessage::serializeTo(stream) &&
            stream.writeUtf8String(command)) {
        for (const QString &argument : arguments) {
            if (!stream.writeUtf8String(argument)) {
                return false;
            }
        }
        return true;
    }
    return false;
}

bool ServerToClientChatMessage::unserializeFrom(NinjamInputDataStream& stream)
{
    QStringList strings;
    if (stream.readUtf8Strings(strings, stream.getRemainingPayload()) &&
            strings.size() >= 5) {
        command = strings[0];
        arguments.clear();
        arguments.append(strings[1]);
        arguments.append(strings[2]);
        arguments.append(strings[3]);
        arguments.append(strings[4]);
        return true;
    }
    return false;
}

void ServerToClientChatMessage::printDebug(QDebug &dbg) const
{
    dbg << "RECEIVE ServerChatMessage{ command=" << command
        << " arguments=" << arguments << "}"
        << Qt::endl;
}

// ++++++++++++++++++++++++++++++++++++++++++++++
DownloadIntervalBegin::DownloadIntervalBegin() :
    ServerMessage(MessageType::DownloadIntervalBegin)
{

}

DownloadIntervalBegin::DownloadIntervalBegin(const MessageGuid &GUID, quint32 estimatedSize, const MessageFourCC &fourCC, quint8 channelIndex, const QString &userName) :
    ServerMessage(MessageType::DownloadIntervalBegin),
    GUID(GUID),
    estimatedSize(estimatedSize),
    fourCC(fourCC),
    channelIndex(channelIndex),
    userName(userName)
{
}

DownloadIntervalBegin DownloadIntervalBegin::from(const UploadIntervalBegin &msg, const QString &userName)
{
    auto guid = msg.getGUID();
    auto estimatedSize = msg.getEstimatedSize();
    auto fourCC = msg.getFourCC();
    auto channelIndex = msg.getChannelIndex();

    return DownloadIntervalBegin(guid, estimatedSize, fourCC, channelIndex, userName);
}

bool DownloadIntervalBegin::isAudio() const
{
   return  fourCC[0] == 'O' &&
           fourCC[1] == 'G' &&
           fourCC[2] == 'G' &&
           fourCC[3] == 'v';
}

bool DownloadIntervalBegin::isVideo() const
{
   return !isAudio();
}

quint32 DownloadIntervalBegin::getSerializePayload() const
{
    return sizeof(GUID) + sizeof(estimatedSize) + sizeof(fourCC) + sizeof(quint8) +
            NinjamOutputDataStream::getUtf8StringPayload(userName);
}

bool DownloadIntervalBegin::serializeTo(NinjamOutputDataStream& stream) const
{
    return (ServerMessage::serializeTo(stream) &&
            stream.writeByteArray(GUID) &&
            stream.write<quint32>(estimatedSize) &&
            stream.writeByteArray(fourCC) &&
            stream.write<quint8>(channelIndex) &&
            stream.writeUtf8String(userName));
}

bool DownloadIntervalBegin::unserializeFrom(NinjamInputDataStream& stream)
{
    return (stream.readByteArray(GUID) &&
            stream.read<quint32>(estimatedSize) &&
            stream.readByteArray(fourCC) &&
            stream.read<quint8>(channelIndex) &&
            stream.readUtf8StringFixed(userName, stream.getRemainingPayload()));
}

void DownloadIntervalBegin::printDebug(QDebug &dbg) const
{
    dbg << "DownloadIntervalBegin{ " << Qt::endl
        << "\tfourCC='"<< fourCC[0] << fourCC[1] << fourCC[2] << fourCC[3] << Qt::endl
        << "\tGUID={"<< QString::fromUtf8(GUID.data(), GUID.size()) << "} " << Qt::endl
        << "\tisValidOggDownload="<< isAudio() << Qt::endl
        << "\tdownloadShoudBeStopped="<< shouldBeStopped() << Qt::endl
        << "\tdownloadIsComplete="<< isComplete() << Qt::endl
        << "\testimatedSize=" << estimatedSize << Qt::endl
        << "\tchannelIndex=" << channelIndex  << Qt::endl
        << "}" << Qt::endl;
}

// -------------------------------------------------------------------

DownloadIntervalWrite::DownloadIntervalWrite() :
    ServerMessage(MessageType::DownloadIntervalWrite),
    encodedData(QSharedPointer<QByteArray>::create())
{

}

DownloadIntervalWrite::DownloadIntervalWrite(const MessageGuid &GUID, quint8 flags, const QByteArray &encodedData) :
    ServerMessage(MessageType::DownloadIntervalWrite),
    GUID(GUID),
    flags(flags),
    encodedData(QSharedPointer<QByteArray>::create(encodedData))
{

}

DownloadIntervalWrite DownloadIntervalWrite::from(const UploadIntervalWrite &msg)
{
    auto guid = msg.getGUID();
    auto flags = static_cast<quint8>(msg.isLastPart() ? 1 : 0); // If the Flag field bit 0 is set then the upload is complete.
    auto encodedData = msg.getEncodedData();

    return DownloadIntervalWrite(guid, flags, encodedData);
}

quint32 DownloadIntervalWrite::getSerializePayload() const
{
    return sizeof(GUID) + sizeof(quint8) + NinjamOutputDataStream::getByteArrayPayload(*encodedData);
}

bool DownloadIntervalWrite::serializeTo(NinjamOutputDataStream& stream) const
{
    return (ServerMessage::serializeTo(stream) &&
            stream.writeByteArray(GUID) &&
            stream.write<quint8>(flags) &&
            stream.writeByteArray(*encodedData));
}

bool DownloadIntervalWrite::unserializeFrom(NinjamInputDataStream& stream)
{
    if (stream.readByteArray(GUID) &&
            stream.read<quint8>(flags)) {
        encodedData->resize(stream.getRemainingPayload());
        return stream.readByteArray(*encodedData);
    }
    return false;
}

void DownloadIntervalWrite::printDebug(QDebug &dbg) const
{
    dbg << "RECEIVE DownloadIntervalWrite{ flags='" << flags
        << "' GUID={" << QString::fromUtf8(GUID.data(), GUID.size())
        << "} downloadIsComplete=" << downloadIsComplete()
        << ", audioData=" << encodedData->size() << " bytes }"
        << Qt::endl;
}
