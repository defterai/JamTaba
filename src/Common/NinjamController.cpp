#include "NinjamController.h"
#include "MainController.h"
#include "controller/AudioController.h"
#include "controller/AudioEncoderController.h"

#include "ninjam/client/Service.h"
#include "ninjam/client/User.h"
#include "ninjam/client/UserChannel.h"
#include "ninjam/client/ServerInfo.h"
#include "audio/core/AudioNode.h"
#include "audio/core/SamplesBuffer.h"
#include "audio/core/AudioDriver.h"
#include "audio/core/SamplesBuffer.h"
#include "audio/core/LocalInputGroup.h"
#include "file/FileReaderFactory.h"
#include "file/FileReader.h"
#include "audio/NinjamTrackNode.h"
#include "audio/MetronomeTrackNode.h"
#include "audio/MidiSyncTrackNode.h"
#include "audio/Resampler.h"
#include "audio/SamplesBufferRecorder.h"
#include "audio/vorbis/VorbisEncoder.h"
#include "audio/vorbis/Vorbis.h"
#include "gui/NinjamRoomWindow.h"
#include "log/Logging.h"
#include "MetronomeUtils.h"
#include "persistence/Settings.h"
#include "Utils.h"
#include "Helpers.h"

#include "gui/chat/NinjamChatMessageParser.h"

#include <QMutexLocker>
#include <QDebug>
#include <QThread>
#include <QFileInfo>
#include <QWaitCondition>

#include <cmath>
#include <cassert>
#include <vector>

using controller::NinjamController;
using ninjam::client::ServerInfo;

// +++++++++++++++++ Nested classes to handle schedulable events ++++++++++++++++

class NinjamController::SchedulableEvent // an event scheduled to be processed in next interval
{
public:
    explicit SchedulableEvent(NinjamController *controller) :
        controller(controller)
    {
        //
    }

    virtual void process() = 0;

    virtual ~SchedulableEvent()
    {
    }

protected:
    NinjamController *controller;
};

// ++++++++++++++++++++++++++++++++++++++++++++

class NinjamController::BpiChangeEvent : public SchedulableEvent
{
public:
    BpiChangeEvent(NinjamController *controller, quint16 newBpi) :
        SchedulableEvent(controller),
        newBpi(newBpi)
    {
    }

    void process() override
    {
        controller->setBpi(newBpi);
    }

private:
    quint16 newBpi;
};

// +++++++++++++++++++++++++++++++++++++++++

class NinjamController::BpmChangeEvent : public SchedulableEvent
{
public:
    BpmChangeEvent(NinjamController *controller, quint16 newBpm) :
        SchedulableEvent(controller),
        newBpm(newBpm)
    {
    }

    void process() override
    {
        controller->setBpm(newBpm);
    }

private:
    quint16 newBpm;
};

// +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

NinjamController::NinjamController(controller::MainController *mainController) :
    intervalPosition(0),
    samplesInInterval(0),
    mutex(QMutex::Recursive),
    mainController(mainController),
    metronomeTrackNode(createQSharedPointer<audio::MetronomeTrackNode>(mainController->getSettings().metronomeSettings, mainController->getSampleRate())),
    midiSyncTrackNode(createQSharedPointer<audio::MidiSyncTrackNode>(mainController->getSampleRate())),
    encoderController(new AudioEncoderController()),
    lastBeat(0),
    currentBpi(0),
    currentBpm(0),
    preparedForTransmit(false),
    waitingIntervals(0) // waiting for start transmit
{
    running = false;

    const auto& audioController = mainController->getAudioController();
    audioController->manageTrack(metronomeTrackNode);
    audioController->manageTrack(midiSyncTrackNode);

    connect(midiSyncTrackNode.data(), &MidiSyncTrackNode::midiClockStarted, mainController, &MainController::startMidiClock);
    connect(midiSyncTrackNode.data(), &MidiSyncTrackNode::midiClockStopped, mainController, &MainController::stopMidiClock);
    connect(midiSyncTrackNode.data(), &MidiSyncTrackNode::midiClockPulsed, mainController, &MainController::sendMidiClockPulse);

    connect(encoderController.data(), &AudioEncoderController::encodeCompleted, this, &NinjamController::handleEncodedAudioAvailable);
}

User NinjamController::getUserByName(const QString &userName) const
{
    auto server = mainController->getNinjamService()->getCurrentServer();
    auto users = server->getUsers();
    for (const auto &user : qAsConst(users)) {
        if (user.getName() == userName) {
            return user;
        }
    }
    return User();
}

QSharedPointer<NinjamTrackNode> NinjamController::getTrackNode(int channelIndex) const
{
    QMutexLocker locker(&mutex);
    for (const auto& trackNode : trackNodes) {
        if (trackNode->getID() == channelIndex) {
            return trackNode;
        }
    }
    return nullptr;
}

void NinjamController::setBpm(int newBpm)
{
    currentBpm = newBpm;
    samplesInInterval = computeTotalSamplesInInterval();

    emit metronomeTrackNode->postBpm(currentBpm);
    emit midiSyncTrackNode->postBpm(currentBpm);
    emit currentBpmChanged(currentBpm);
}

void NinjamController::setBpi(int newBpi)
{
    currentBpi = newBpi;
    samplesInInterval = computeTotalSamplesInInterval();

    emit metronomeTrackNode->postBpi(currentBpi);
    emit midiSyncTrackNode->postBpi(currentBpi);
    emit currentBpiChanged(currentBpi);
}

void NinjamController::setBpmBpi(int initialBpm, int initialBpi)
{
    currentBpi = initialBpi;
    currentBpm = initialBpm;
    samplesInInterval = computeTotalSamplesInInterval();

    emit midiSyncTrackNode->postBpm(currentBpm);
    emit midiSyncTrackNode->postBpi(currentBpi);
    emit metronomeTrackNode->postBpm(currentBpm);
    emit metronomeTrackNode->postBpi(currentBpi);

    emit currentBpmChanged(currentBpm);
    emit currentBpiChanged(currentBpi);
}

void NinjamController::setSyncEnabled(bool enabled)
{
    if (enabled) {
        emit midiSyncTrackNode->postStart();
    } else {
        emit midiSyncTrackNode->postStop();
    }
}

// +++++++++++++++++++++++++ THE MAIN LOGIC IS HERE  ++++++++++++++++++++++++++++++++++++++++++++++++

void NinjamController::process(const QSharedPointer<audio::SamplesBuffer>& in, const QSharedPointer<audio::SamplesBuffer>& out)
{
    QMutexLocker locker(&mutex);

    if (currentBpi == 0 || currentBpm == 0)
        processScheduledChanges(); // check if we have the initial bpm and bpi change pending

    if (!running || samplesInInterval <= 0)
        return; // not initialized

    int totalSamplesToProcess = out->getFrameLenght();
    int samplesProcessed = 0;

    int offset = 0;

    do
    {
        emit startProcessing(intervalPosition); // vst host time line is updated with this event

        int samplesToProcessInThisStep
            = (std::min)((int)(samplesInInterval - intervalPosition),
                         totalSamplesToProcess - offset);

        assert(samplesToProcessInThisStep);

        static QSharedPointer<audio::SamplesBuffer> tempOutBuffer = createQSharedPointer<audio::SamplesBuffer>(out->getChannels(), samplesToProcessInThisStep);
        tempOutBuffer->setFrameLenght(samplesToProcessInThisStep);
        tempOutBuffer->zero();

        QSharedPointer<audio::SamplesBuffer> tempInBuffer = createQSharedPointer<audio::SamplesBuffer>(in->getChannels(), samplesToProcessInThisStep);
        tempInBuffer->set(*in, offset, samplesToProcessInThisStep, 0);

        bool newInterval = intervalPosition == 0;
        if (newInterval)   // starting new interval
            handleNewInterval();

        emit metronomeTrackNode->postSetIntervalPosition(this->intervalPosition);
        emit midiSyncTrackNode->postSetIntervalPosition(this->intervalPosition);
        int currentBeat = intervalPosition / getSamplesPerBeat();
        if (currentBeat != lastBeat)
        {
            lastBeat = currentBeat;
            emit intervalBeatChanged(currentBeat);
        }

        // +++++++++++ MAIN AUDIO OUTPUT PROCESS +++++++++++++++
        bool isLastPart = intervalPosition + samplesToProcessInThisStep >= samplesInInterval;
        //for (NinjamTrackNode *track : trackNodes)
        //    track->setProcessingLastPartOfInterval(isLastPart); // TODO resampler still need a flag indicating the last part?
        mainController->getAudioController()->processAudio(tempInBuffer, tempOutBuffer, mainController->pullMidiMessagesFromDevices()).waitForFinished();
        out->add(*tempOutBuffer, offset); // generate audio output
        // ++++++++++++++++++++++++++++++++++++++++++++++++++++++

        if (preparedForTransmit)
        {
            // 1) mix input subchannels, 2) encode and 3) send the encoded audio
            bool isFirstPart = intervalPosition == 0;
            auto mixedChannels = mainController->getAudioController()->mixInputSubchannels(samplesToProcessInThisStep).result();
            for (auto& mixedChannelData : mixedChannels) {
                mixedChannelData.isFirstPart = isFirstPart;
                mixedChannelData.isLastPart = isLastPart;
            }
            encoderController->scheduleEncode(mixedChannels);
        }

        samplesProcessed += samplesToProcessInThisStep;
        offset += samplesToProcessInThisStep;
        this->intervalPosition = (this->intervalPosition + samplesToProcessInThisStep)
                                 % samplesInInterval;
    }
    while (samplesProcessed < totalSamplesToProcess);
}

void NinjamController::updateMetronomeSound(const persistence::MetronomeSoundSettings& settings)
{
    emit metronomeTrackNode->postChangeSound(settings);
}

void NinjamController::stop(bool emitDisconnectedSignal)
{
    auto ninjamService = mainController->getNinjamService();

    if (this->running)
    {
        disconnect(ninjamService, &Service::serverBpmChanged, this,
                   &NinjamController::scheduleBpmChangeEvent);
        disconnect(ninjamService, &Service::serverBpiChanged, this,
                   &NinjamController::scheduleBpiChangeEvent);
        disconnect(ninjamService, &Service::audioIntervalCompleted, this,
                   &NinjamController::handleIntervalCompleted);

        disconnect(ninjamService, &Service::userChannelCreated, this,
                   &NinjamController::addNinjamRemoteChannel);
        disconnect(ninjamService, &Service::userChannelRemoved, this,
                   &NinjamController::removeNinjamRemoteChannel);
        disconnect(ninjamService, &Service::userChannelUpdated, this,
                   &NinjamController::updateNinjamRemoteChannel);
        disconnect(ninjamService, &Service::audioIntervalDownloading, this,
                   &NinjamController::handleIntervalDownloading);

        disconnect(ninjamService, &Service::publicChatMessageReceived, this,
                   &NinjamController::publicChatMessageReceived);
        disconnect(ninjamService, &Service::privateChatMessageReceived, this,
                   &NinjamController::privateChatMessageReceived);
        disconnect(ninjamService, &Service::serverTopicMessageReceived, this,
                   &NinjamController::topicMessageReceived);

        this->running = false;

        // stop midi sync track
        emit midiSyncTrackNode->postStop();
        emit mainController->getAudioController()->postRemoveTrack(midiSyncTrackNode->getID());

        // store metronome settings
        float metronomeGain;
        float metronomePan;
        bool metronomeIsMuted;
        metronomeTrackNode->postTask([&]() {
            metronomeGain = metronomeTrackNode->getGain();
            metronomePan = metronomeTrackNode->getPan();
            metronomeIsMuted = metronomeTrackNode->isMuted();
        }).waitForFinished();
        mainController->storeMetronomeSettings(Utils::poweredGainToLinear(metronomeGain), metronomePan, metronomeIsMuted);

        emit mainController->getAudioController()->postRemoveTrack(metronomeTrackNode->getID());// remove metronome

        QVector<QSharedPointer<NinjamTrackNode>> trackNodesList;
        {
            QMutexLocker locker(&mutex);
            trackNodesList.reserve(trackNodes.size());
            for (auto iterator = trackNodes.begin(); iterator != trackNodes.end(); ++iterator) {
                trackNodesList.append(iterator.value());
            }
            trackNodes.clear();
        }

        // clear all tracks
        for (const auto& trackNode : trackNodesList) {
            emit mainController->getAudioController()->postRemoveTrack(trackNode->getID());
        }
    }

    encoderController->stop();

    {
        QMutexLocker locker(&scheduledEventsMutex);
        scheduledEvents.clear();
    }

    qCDebug(jtNinjamCore) << "NinjamController destructor - disconnecting...";

    ninjamService->disconnectFromServer(emitDisconnectedSignal);
}

NinjamController::~NinjamController()
{
    // QMutexLocker locker(&mutex);
    qCDebug(jtNinjamCore) << "NinjamController destructor";
    if (isRunning())
        stop(false);
}

void NinjamController::start(const ServerInfo &server)
{
    qCDebug(jtNinjamCore) << "starting ninjam controller...";
    QMutexLocker locker(&mutex);

    // schedule an update in internal attributes
    auto bpi = server.getBpi();
    if (bpi > 0) {
        scheduleBpiChangeEvent(bpi, 0);
    }
    auto bpm = server.getBpm();
    if (bpm > 0) {
        scheduleBpmChangeEvent(bpm);
    }
    preparedForTransmit = false; // the xmit start after the first interval is received
    emit preparingTransmission();

    processScheduledChanges();

    if (!running)
    {
        encoderController->start();

        // add a sine wave generator as input to test audio transmission
        // mainController->addInputTrackNode(new Audio::LocalInputTestStreamer(440, mainController->getAudioDriverSampleRate()));

        const auto& metronomeSettings = mainController->getSettings().metronomeSettings;

        emit this->metronomeTrackNode->postChangeSound(metronomeSettings);
        emit this->metronomeTrackNode->postMute(metronomeSettings.isMuted(), this);
        emit this->metronomeTrackNode->postGain(Utils::linearGainToPower(metronomeSettings.getGain()), this);
        emit this->metronomeTrackNode->postPan(metronomeSettings.getPan(), this);

        mainController->getAudioController()->addTrackAsync(this->metronomeTrackNode);

        mainController->getAudioController()->addTrackAsync(this->midiSyncTrackNode);

        this->intervalPosition = lastBeat = 0;

        auto ninjamService = mainController->getNinjamService();
        connect(ninjamService, &Service::serverBpmChanged, this,
                &NinjamController::scheduleBpmChangeEvent);
        connect(ninjamService, &Service::serverBpiChanged, this,
                &NinjamController::scheduleBpiChangeEvent);
        connect(ninjamService, &Service::audioIntervalCompleted, this,
                &NinjamController::handleIntervalCompleted);

        connect(ninjamService, &Service::serverInitialBpmBpiAvailable,
                this, &NinjamController::setBpmBpi);

        connect(ninjamService, &Service::userChannelCreated, this,
                &NinjamController::addNinjamRemoteChannel);
        connect(ninjamService, &Service::userChannelRemoved, this,
                &NinjamController::removeNinjamRemoteChannel);
        connect(ninjamService, &Service::userChannelUpdated, this,
                &NinjamController::updateNinjamRemoteChannel);
        connect(ninjamService, &Service::audioIntervalDownloading, this,
                &NinjamController::handleIntervalDownloading);
        connect(ninjamService, &Service::userExited, this,
                &NinjamController::handleNinjamUserExiting);
        connect(ninjamService, &Service::userEntered, this,
                &NinjamController::handleNinjamUserEntering);

        connect(ninjamService, &Service::publicChatMessageReceived, this,
                &NinjamController::handleReceivedPublicChatMessage);
        connect(ninjamService, &Service::privateChatMessageReceived, this,
                &NinjamController::handleReceivedPrivateChatMessage);
        connect(ninjamService, &Service::serverTopicMessageReceived, this,
                &NinjamController::topicMessageReceived);

        // add tracks for users connected in server
        auto users = server.getUsers();
        for (const auto &user : qAsConst(users))
        {
            for (const auto &channel : user.getChannels())
                addTrack(user, channel);
        }

        this->running = true;

        setEncodingQuality(mainController->getSettings().audioSettings.getEncodingQuality());

        emit started();
    }
    qCDebug(jtNinjamCore) << "ninjam controller started!";
}

void NinjamController::handleReceivedPublicChatMessage(const User &user, const QString &message)
{
    if (!mainController->userIsBlockedInChat(user.getFullName()))
        emit publicChatMessageReceived(user, message);
}

void NinjamController::handleReceivedPrivateChatMessage(const User &user, const QString &message)
{
    if (!mainController->userIsBlockedInChat(user.getFullName()))
        emit privateChatMessageReceived(user, message);
}

void NinjamController::handleEncodedAudioAvailable(const controller::AudioChannelData& channelData,
                                                   const QByteArray& encodedData)
{
    emit encodedAudioAvailableToSend(channelData, encodedData);
}

void NinjamController::sendChatMessage(const QString &msg)
{
    auto service = mainController->getNinjamService();

    if (gui::chat::isAdminCommand(msg))
    {
        service->sendAdminCommand(msg);
    }
    else if (gui::chat::isPrivateMessage(msg))
    {
        QString destUserName = gui::chat::extractDestinationUserNameFromPrivateMessage(msg);
        auto text = QString(msg).replace(destUserName + " ", ""); // remove the blank space after user name
        service->sendPrivateChatMessage(text, destUserName);
    }
    else
    {
        service->sendPublicChatMessage(msg);
    }
}

QString NinjamController::getUniqueKeyForChannel(const UserChannel &channel,
                                                 const QString &userFullName)
{
    return userFullName + QString::number(channel.getIndex());
}

bool NinjamController::userIsBot(const QString userName) const
{
    if (mainController)
        return mainController->getBotNames().contains(userName);
    return false;
}

void NinjamController::addTrack(const User &user, const UserChannel &channel)
{
    if (userIsBot(user.getName()))
        return;

    QString uniqueKey = getUniqueKeyForChannel(channel, user.getFullName());
    auto trackNode = createQSharedPointer<NinjamTrackNode>(44100);

    // checkThread("addTrack();");
    {
        QMutexLocker locker(&mutex);
        trackNodes.insert(uniqueKey, trackNode);
    } // release the mutex before emit the signal

    mainController->getAudioController()->addTrackAsync(trackNode);
    emit channelAdded(user, channel, trackNode->getID());
}

void NinjamController::removeTrack(const User &user, const UserChannel &channel)
{
    QString uniqueKey = getUniqueKeyForChannel(channel, user.getFullName());

    int trackNodeId = -1;
    {
        QMutexLocker locker(&mutex);
        // checkThread("removeTrack();");
        auto iterator = trackNodes.find(uniqueKey);
        if (iterator != trackNodes.end()) {
            trackNodeId = iterator.value()->getID();
            trackNodes.erase(iterator);
        }
    }

    if (trackNodeId != -1) {
        emit mainController->getAudioController()->postRemoveTrack(trackNodeId);
        emit channelRemoved(user, channel, trackNodeId);
    }
}

void NinjamController::setEncodingQuality(float newEncodingQuality)
{
    encoderController->setAudioEncodeQuality(newEncodingQuality);
}

const QSharedPointer<audio::MetronomeTrackNode>& NinjamController::getMetronomeTrack() const
{
    return metronomeTrackNode;
}

const QSharedPointer<audio::MidiSyncTrackNode>& NinjamController::getMidiSyncTrack() const
{
    return midiSyncTrackNode;
}

void NinjamController::voteBpi(int bpi)
{
    mainController->getNinjamService()->voteToChangeBPI(bpi);
}

void NinjamController::voteBpm(int bpm)
{
    mainController->getNinjamService()->voteToChangeBPM(bpm);
}

void NinjamController::setMetronomeBeatsPerAccent(int beatsPerAccent)
{
    emit metronomeTrackNode->postBeatsPerAccent(beatsPerAccent);
}

void NinjamController::setMetronomeAccentBeats(QList<int> accentBeats)
{
    emit metronomeTrackNode->postAccentBeats(accentBeats);
}

// void NinjamController::deleteDeactivatedTracks(){
////QMutexLocker locker(&mutex);
// checkThread("deleteDeactivatedTracks();");
// QList<QString> keys = trackNodes.keys();
// foreach (QString key, keys) {
// NinjamTrackNode* trackNode = trackNodes[key];
// if(!(trackNode->isActivated())){
// trackNodes.remove(key);
// mainController->removeTrack(trackNode->getID());
////delete trackNode; //BUG - sometimes Jamtaba crash when trackNode is deleted
// }
// }
// }

void NinjamController::handleNewInterval()
{
    // check if the transmiting can start
    if (!preparedForTransmit)
    {
        if (waitingIntervals >= TOTAL_PREPARED_INTERVALS)
        {
            preparedForTransmit = true;
            waitingIntervals = 0;
            emit preparedToTransmit();
        }
        else
        {
            waitingIntervals++;
        }
    }

    processScheduledChanges();

    {
        QMutexLocker locker(&mutex);
        for (const auto& track : qAsConst(trackNodes)) {
            emit track->postStartNewInterval();
        }
    }

    emit startingNewInterval(); // update the UI

    mainController->syncWithNinjamIntervalStart(samplesInInterval);
}

void NinjamController::processScheduledChanges()
{
    if (!scheduledEvents.isEmpty()) {
        QList<QSharedPointer<SchedulableEvent>> events;
        {
            QMutexLocker locker(&scheduledEventsMutex);
            std::swap(events, scheduledEvents);
        }
        for (const auto& event : qAsConst(events)) {
            event->process();
        }
    }
}

long NinjamController::getSamplesPerBeat()
{
    return computeTotalSamplesInInterval()/currentBpi;
}

long NinjamController::computeTotalSamplesInInterval()
{
    double intervalPeriod = 60000.0 / currentBpm * currentBpi;
    return (long)(mainController->getSampleRate() * intervalPeriod / 1000.0);
}

// ninjam slots
void NinjamController::handleNinjamUserEntering(const User &user)
{
    emit userEnter(user.getFullName());
}

void NinjamController::handleNinjamUserExiting(const User &user)
{
    for (const auto &channel : user.getChannels()) {
        removeTrack(user, channel);
    }
    emit userLeave(user.getFullName());
}

void NinjamController::addNinjamRemoteChannel(const User &user, const UserChannel &channel)
{
    addTrack(user, channel);
}

void NinjamController::removeNinjamRemoteChannel(const User &user, const UserChannel &channel)
{
    removeTrack(user, channel);
}

void NinjamController::updateNinjamRemoteChannel(const User &user, const UserChannel &channel)
{
    auto uniqueKey = getUniqueKeyForChannel(channel, user.getFullName());

    int trackNodeId = -1;
    {
        QMutexLocker locker(&mutex);
        auto iterator = trackNodes.find(uniqueKey);
        if (iterator != trackNodes.end()) {
            trackNodeId = iterator.value()->getID();
        }
    }

    if (trackNodeId != -1) {
        emit channelChanged(user, channel, trackNodeId);
    }
}

void NinjamController::scheduleEvent(QSharedPointer<SchedulableEvent>&& event) {
    QMutexLocker locker(&scheduledEventsMutex);
    scheduledEvents.append(std::move(event));
}

void NinjamController::scheduleBpiChangeEvent(quint16 newBpi, quint16 oldBpi)
{
    Q_UNUSED(oldBpi);
    scheduleEvent(createQSharedPointer<BpiChangeEvent>(this, newBpi));
}

void NinjamController::scheduleBpmChangeEvent(quint16 newBpm)
{
    scheduleEvent(createQSharedPointer<BpmChangeEvent>(this, newBpm));
}

void NinjamController::handleIntervalCompleted(const User &user, quint8 channelIndex,
                                               const QSharedPointer<QByteArray>& encodedData)
{
    if (mainController->isMultiTrackRecordingActivated())
    {
        auto geoLocation = mainController->getGeoLocation(user.getIp());
        QString userName = user.getName() + " from " + geoLocation.countryName;
        mainController->saveEncodedAudio(userName, channelIndex, encodedData);
    }

    bool visited = user.visitChannel(channelIndex, [&](const UserChannel& channel) {
        QString channelKey = getUniqueKeyForChannel(channel, user.getFullName());
        QSharedPointer<NinjamTrackNode> trackNode;
        {
            QMutexLocker locker(&mutex);
            auto iterator = trackNodes.find(channelKey);
            if (iterator != trackNodes.end()) {
                trackNode = iterator.value();
            }
        }
        if (trackNode) {
            emit trackNode->postVorbisEncodedInterval(encodedData);
            emit channelAudioFullyDownloaded(trackNode->getID());
        } else {
            qWarning() << "The channel " << channelIndex << " of user " << user.getName()
                       << " not founded in tracks map!";
        }
    });
    if (!visited) {
        qWarning() << "The channel " << channelIndex << " of user " << user.getName()
                   << " not founded in map!";
    }
}

void NinjamController::reset()
{
    QMutexLocker locker(&mutex);
    for (const auto& trackNode : qAsConst(trackNodes)) {
        emit trackNode->postDiscardDownloadedIntervals();
    }
    trackNodes.clear();
    intervalPosition = lastBeat = 0;
}

void NinjamController::setSampleRate(int newSampleRate)
{
    if (!isRunning())
        return;

    reset(); // discard all downloaded intervals
    this->samplesInInterval = computeTotalSamplesInInterval();
}

void NinjamController::handleIntervalDownloading(const User &user, quint8 channelIndex, const QSharedPointer<QByteArray>& encodedAudio, bool isFirstPart, bool isLastPart)
{
    bool visited = user.visitChannel(channelIndex, [&](const UserChannel& channel) {
        QString channelKey = getUniqueKeyForChannel(channel, user.getFullName());
        QSharedPointer<NinjamTrackNode> track;
        {
            QMutexLocker locker(&mutex);
            track = trackNodes[channelKey];
        }
        if (track) {
            emit channelAudioChunkDownloaded(track->getID());
            emit track->postVorbisEncodedChunk(encodedAudio, isFirstPart, isLastPart);
        }
    });
    if (!visited) {
        qWarning() << "The channel " << channelIndex << " of user " << user.getName()
                   << " not founded in map!";
    }
}
