#ifndef NINJAMJAMROOMCONTROLLER_H
#define NINJAMJAMROOMCONTROLLER_H

#include <QObject>
#include <QMutex>
#include <QThread>
#include <QMap>
#include <QSharedPointer>
#include <QScopedPointer>

class NinjamTrackNode;

namespace ninjam { namespace client {
class ServerInfo;
class User;
class UserChannel;
}}

namespace audio {
    class MetronomeTrackNode;
    class MidiSyncTrackNode;
    class SamplesBuffer;
}

namespace persistence {
    class MetronomeSoundSettings;
}

namespace controller {

struct AudioChannelData;
class MainController;
class AudioEncoderController;

using ninjam::client::ServerInfo;
using ninjam::client::User;
using ninjam::client::UserChannel;
using audio::SamplesBuffer;
using audio::MetronomeTrackNode;
using audio::MidiSyncTrackNode;

class NinjamController : public QObject
{
    Q_OBJECT

public:
    explicit NinjamController(MainController *mainController);
    virtual ~NinjamController();
    virtual void process(const QSharedPointer<audio::SamplesBuffer>& in, const QSharedPointer<audio::SamplesBuffer>& out);
    void start(const ServerInfo &server);
    void stop(bool emitDisconnectedSignal);
    bool isRunning() const;

    void setEncodingQuality(float newEncodingQuality);

    const QSharedPointer<MetronomeTrackNode>& getMetronomeTrack() const;
    const QSharedPointer<MidiSyncTrackNode>& getMidiSyncTrack() const;

    void setMetronomeBeatsPerAccent(int beatsPerAccent);
    void setMetronomeAccentBeats(QList<int> accentBeats);

    int getCurrentBpi() const;
    int getCurrentBpm() const;

    void voteBpi(int newBpi);
    void voteBpm(int newBpm);

    void setBpi(int newBpi);
    void setBpm(int newBpm);
    void setBpmBpi(int initialBpm, int initalBpi);

    void setSyncEnabled(bool enabled);

    void sendChatMessage(const QString &msg);

    void scheduleXmitChange(int channelID, bool transmiting);     // schedule the change for the next interval

    void setSampleRate(int newSampleRate);

    void reset();     // discard downloaded intervals and reset intervalPosition

    bool isPreparedForTransmit() const;

    void updateMetronomeSound(const persistence::MetronomeSoundSettings& settings);

    bool userIsBot(const QString userName) const;

    User getUserByName(const QString &userName) const;

    uint getSamplesPerInterval() const;

    QSharedPointer<NinjamTrackNode> getTrackNode(int channelIndex) const;

    QList<QSharedPointer<NinjamTrackNode>> getTrackNodes() const;

signals:
    void currentBpiChanged(int newBpi);     // emitted when a scheduled bpi change is processed in interval start (first beat).
    void currentBpmChanged(int newBpm);

    void intervalBeatChanged(int intervalBeat);
    void startingNewInterval();
    void startProcessing(int intervalPosition);
    void channelAdded(const ninjam::client::User &user, const ninjam::client::UserChannel &channel, long channelID);
    void channelRemoved(const ninjam::client::User &user, const ninjam::client::UserChannel &channel, long channelID);
    void channelChanged(const ninjam::client::User &user, const ninjam::client::UserChannel &channel, long channelID); // emmited when channel name or flags (intervalic or voice chat channel) changes
    void channelAudioChunkDownloaded(long channelID);
    void channelAudioFullyDownloaded(long channelID);
    void userLeave(const QString &userFullName);
    void userEnter(const QString &userFullName);

    void publicChatMessageReceived(const ninjam::client::User &user, const QString &message);
    void privateChatMessageReceived(const ninjam::client::User &user, const QString &message);
    void topicMessageReceived(const QString &message);

    void encodedAudioAvailableToSend(const controller::AudioChannelData& channelData, const QByteArray& encodedData);

    void preparingTransmission();     // waiting for start transmission
    void preparedToTransmit();     // this signal is emmited one time, when Jamtaba is ready to transmit (after wait some complete itervals)

    void started();

protected:
    long intervalPosition;
    long samplesInInterval;

    mutable QMutex mutex;
    QMap<QString, QSharedPointer<NinjamTrackNode>> trackNodes;     // the other users channels

    MainController *mainController;

    QSharedPointer<MetronomeTrackNode> metronomeTrackNode;
    QSharedPointer<MidiSyncTrackNode> midiSyncTrackNode;

private:
    static QString getUniqueKeyForChannel(const UserChannel &channel, const QString &userFullName);

    void addTrack(const User &user, const UserChannel &channel);
    void removeTrack(const User &user, const UserChannel &channel);

    QScopedPointer<controller::AudioEncoderController, QScopedPointerDeleteLater> encoderController;

    bool running;
    int lastBeat;

    int currentBpi;
    int currentBpm;

    long computeTotalSamplesInInterval();
    long getSamplesPerBeat();

    void processScheduledChanges();
    bool hasScheduledChanges() const;

    void handleNewInterval();

    void setXmitStatus(int channelID, bool transmiting);

    // ++++++++++++++++++++ nested classes to handle scheduled events +++++++++++++++++

    class SchedulableEvent;     // the interface for all events
    class BpiChangeEvent;
    class BpmChangeEvent;
    QList<QSharedPointer<SchedulableEvent>> scheduledEvents;
    QMutex scheduledEventsMutex;

    bool preparedForTransmit;
    int waitingIntervals;
    static const int TOTAL_PREPARED_INTERVALS = 2;     // how many intervals Jamtaba will wait to start trasmiting?

    void scheduleEvent(QSharedPointer<SchedulableEvent>&& event);

private slots:
    // ninjam events
    void scheduleBpmChangeEvent(quint16 newBpm);
    void scheduleBpiChangeEvent(quint16 newBpi, quint16 oldBpi);
    void handleIntervalCompleted(const ninjam::client::User &user, quint8 channelIndex,
                                 const QSharedPointer<QByteArray>& encodedAudioData);
    void handleIntervalDownloading(const ninjam::client::User &user, quint8 channelIndex,
                                   const QSharedPointer<QByteArray>& encodedAudio, bool isFirstPart, bool isLastPart);
    void addNinjamRemoteChannel(const ninjam::client::User &user, const ninjam::client::UserChannel &channel);
    void removeNinjamRemoteChannel(const ninjam::client::User &user, const ninjam::client::UserChannel &channel);
    void updateNinjamRemoteChannel(const ninjam::client::User &user, const ninjam::client::UserChannel &channel);
    void handleNinjamUserExiting(const ninjam::client::User &user);
    void handleNinjamUserEntering(const ninjam::client::User &user);
    void handleReceivedPublicChatMessage(const ninjam::client::User &user, const QString &message);
    void handleReceivedPrivateChatMessage(const ninjam::client::User &user, const QString &message);
    void handleEncodedAudioAvailable(const controller::AudioChannelData& channelData, const QByteArray& encodedData);

};     // end of class

inline QList<QSharedPointer<NinjamTrackNode>> NinjamController::getTrackNodes() const
{
    return trackNodes.values();
}

inline bool NinjamController::isPreparedForTransmit() const
{
    return preparedForTransmit;
}

inline int NinjamController::getCurrentBpi() const
{
    return currentBpi;
}

inline int NinjamController::getCurrentBpm() const
{
    return currentBpm;
}

inline bool NinjamController::isRunning() const
{
    return running;
}

inline uint NinjamController::getSamplesPerInterval() const
{
    return samplesInInterval;
}
} // namespace controller

#endif // NINJAMJAMROOMCONTROLLER_H
