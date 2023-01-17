#include <QtGlobal>

#ifdef Q_OS_WIN
    #include "log/stackwalker/WindowsStackWalker.h"
#endif

#include "MainController.h"
#include "NinjamController.h"
#include "Utils.h"
#include "loginserver/natmap.h"
#include "audio/core/AudioNode.h"
#include "audio/core/LocalInputNode.h"
#include "audio/core/LocalInputGroup.h"
#include "audio/RoomStreamerNode.h"
#include "ninjam/client/Service.h"
#include "recorder/JamRecorder.h"
#include "recorder/ReaperProjectGenerator.h"
#include "recorder/ClipSortLogGenerator.h"
#include "gui/MainWindow.h"
#include "gui/ThemeLoader.h"
#include "log/Logging.h"
#include "ninjam/client/Types.h"

#include <QBuffer>
#include <QByteArray>
#include <QDateTime>
#include <QSize>

using ninjam::client::Service;
using ninjam::client::ServerInfo;
using persistence::Settings;
using persistence::LocalInputTrackSettings;
using controller::MainController;

const quint8 MainController::CAMERA_FPS = 10;
const QSize MainController::MAX_VIDEO_SIZE(320, 240); // max video resolution in pixels

const QString MainController::CRASH_FLAG_STRING = "JamTaba closed without crash :)";

// ++++++++++++++++++++++++++++++++++++++++++++++

MainController::MainController(const Settings &settings) :
    loginService(this),
    audioMixer(44100),
    ninjamService(new Service()),
    settings(settings),
    mainWindow(nullptr),
    mutex(QMutex::Recursive),
    videoEncoder(),
    currentStreamingRoomID(-1000),
    started(false),
    masterGain(1),
    usersDataCache(Configurator::getInstance()->getCacheDir()),
    lastFrameTimeStamp(0),
    emojiManager(":/emoji/emoji.json", ":/emoji/icons")
{
    QDir cacheDir = Configurator::getInstance()->getCacheDir();

    // Register known JamRecorders here:
    jamRecorders.append(new recorder::JamRecorder(new recorder::ReaperProjectGenerator()));
    jamRecorders.append(new recorder::JamRecorder(new recorder::ClipSortLogGenerator()));

    connect(&videoEncoder, &FFMpegMuxer::dataEncoded, this, &MainController::enqueueVideoDataToUpload);

    for (auto emojiCode: settings.getRecentEmojis())
        emojiManager.addRecent(emojiCode);

    connect(&loginService, &login::LoginService::roomsListAvailable, [=](const QList<login::RoomInfo> &publicRooms){
        for (const auto & room : publicRooms) {
            for (const auto & user : room.getUsers()) {
                auto maskedIp = ninjam::client::maskIP(user.getIp());
                if (!maskedIp.isEmpty() && !locationCache.contains(maskedIp)) {
                    locationCache.insert(maskedIp, user.getLocation());
                    emit ipResolved(maskedIp);
                }
            }
        }
    });
}

void MainController::setChannelReceiveStatus(const QString &userFullName, quint8 channelIndex, bool receiveChannel)
{
    if (isPlayingInNinjamRoom())
        ninjamService->setChannelReceiveStatus(userFullName, channelIndex, receiveChannel);
}

long MainController::getDownloadTransferRate(const QString userFullName, quint8 channelIndex) const
{
    if (!isPlayingInNinjamRoom())
        return 0;

    return ninjamService->getDownloadTransferRate(userFullName, channelIndex);
}

long MainController::getTotalDownloadTransferRate() const
{
    if (!isPlayingInNinjamRoom())
        return 0;

    return ninjamService->getTotalDownloadTransferRate();
}

long MainController::getTotalUploadTransferRate() const
{
    if (!isPlayingInNinjamRoom())
        return 0;

    return ninjamService->getTotalUploadTransferRate();
}

void MainController::setVideoProperties(const QSize &resolution)
{
    QSize bestResolution(resolution);
    if (resolution.width() > MainController::MAX_VIDEO_SIZE.width())
         bestResolution = MainController::MAX_VIDEO_SIZE;

    videoEncoder.setVideoResolution(bestResolution);
    videoEncoder.setVideoFrameRate(CAMERA_FPS);
}

QSize MainController::getVideoResolution() const
{
    return videoEncoder.getVideoResolution();
}

void MainController::blockUserInChat(const QString &userNameToBlock)
{
    if (!chatBlockedUsers.contains(userNameToBlock)) {
        chatBlockedUsers.insert(userNameToBlock);
        emit userBlockedInChat(userNameToBlock);
    }
}

void MainController::unblockUserInChat(const QString &userNameToUnblock)
{
    if (chatBlockedUsers.remove(userNameToUnblock))
        emit userUnblockedInChat(userNameToUnblock);
}

bool MainController::userIsBlockedInChat(const QString &userFullName) const
{
    return chatBlockedUsers.contains(userFullName);
}

void MainController::setSampleRate(int newSampleRate)
{
    {
        QMutexLocker locker(&mutex);
        int rmsWindowSize = audio::SamplesBuffer::computeRmsWindowSize(newSampleRate);
        for (auto node : tracksNodes.values()) {
            node->setRmsWindowSize(rmsWindowSize);
        }
    }

    audioMixer.setSampleRate(newSampleRate);

    if (settings.recordingSettings.isSaveMultiTrackActivated()) {
        for (auto jamRecorder : jamRecorders)
            jamRecorder->setSampleRate(newSampleRate);
    }

    if (isPlayingInNinjamRoom()) {
        ninjamController->setSampleRate(newSampleRate);

        for (auto inputTrack : inputTracks.values())
            inputTrack->getLooper()->stop(); // looper is stopped when sample rate is changed because the recorded material will sound funny :)
    }

    settings.audioSettings.setSampleRate(newSampleRate);
}

void MainController::setEncodingQuality(float newEncodingQuality)
{
    settings.audioSettings.setEncodingQuality(newEncodingQuality);

    if (isPlayingInNinjamRoom())
        ninjamController->recreateEncoders();
}

void MainController::finishUploads()
{
    for (int channelIndex : audioIntervalsToUpload.keys()) {
        auto &audioInterval = audioIntervalsToUpload[channelIndex];
        ninjamService->sendIntervalPart(audioInterval.getGUID(), QByteArray(), true);
    }

    if (videoIntervalToUpload)
        ninjamService->sendIntervalPart(videoIntervalToUpload->getGUID(), QByteArray(), true);
}

void MainController::quitFromNinjamServer(const QString &error)
{
    qCWarning(jtCore) << error;

    stopNinjamController();

    if (mainWindow)
        mainWindow->exitFromRoom(false, error);
}

void MainController::disconnectFromNinjamServer(const ServerInfo &server)
{
    Q_UNUSED(server);

    stopNinjamController();

    if (mainWindow)
        mainWindow->exitFromRoom(true);

    if (settings.recordingSettings.isSaveMultiTrackActivated()) {
        for (auto jamRecorder : jamRecorders)
            jamRecorder->stopRecording();
    }
}

void MainController::setupNinjamControllerSignals()
{
    auto controller = ninjamController.data();

    Q_ASSERT(controller);

    connect(controller, &NinjamController::encodedAudioAvailableToSend, this, &MainController::enqueueAudioDataToUpload);
    connect(controller, &NinjamController::startingNewInterval, this, &MainController::handleNewNinjamInterval);
    connect(controller, &NinjamController::currentBpiChanged, this, &MainController::updateBpi);
    connect(controller, &NinjamController::currentBpmChanged, this, &MainController::updateBpm);
    connect(controller, &NinjamController::startProcessing, this, &MainController::requestCameraFrame);
}

void MainController::clearNinjamControllerSignals()
{
    auto controller = ninjamController.data();

    Q_ASSERT(controller);

    disconnect(controller, &NinjamController::encodedAudioAvailableToSend, this, &MainController::enqueueAudioDataToUpload);
    disconnect(controller, &NinjamController::startingNewInterval, this, &MainController::handleNewNinjamInterval);
    disconnect(controller, &NinjamController::currentBpiChanged, this, &MainController::updateBpi);
    disconnect(controller, &NinjamController::currentBpmChanged, this, &MainController::updateBpm);
    disconnect(controller, &NinjamController::startProcessing, this, &MainController::requestCameraFrame);
}

void MainController::connectInNinjamServer(const ServerInfo &server)
{
    qCDebug(jtCore) << "connected in ninjam server";

    stopNinjamController();

    auto newNinjamController = createNinjamController();
    if (ninjamController) {
        clearNinjamControllerSignals();
    }
    ninjamController.reset(newNinjamController);

    setupNinjamControllerSignals();

    if (mainWindow)
        mainWindow->enterInRoom(login::RoomInfo(server.getHostName(), server.getPort(), server.getMaxUsers(),
                                                server.getMaxChannels()));
    else
        qCritical() << "mainWindow is null!";

    qCDebug(jtCore) << "starting ninjamController...";

    newNinjamController->start(server);

    if (settings.recordingSettings.isSaveMultiTrackActivated()) {
        QString userName = getUserName();
        QDir recordBasePath = QDir(settings.recordingSettings.getRecordingPath());
        quint16 bpm = server.getBpm();
        quint16 bpi = server.getBpi();
        float sampleRate = getSampleRate();

        for (auto jamRecorder : getActiveRecorders())
            jamRecorder->startRecording(userName, recordBasePath, bpm, bpi, sampleRate);
    }
}

QMap<int, bool> MainController::getXmitChannelsFlags() const
{
    QMap<int, bool> xmitFlags;

    for (auto inputGroup : trackGroups)
        xmitFlags.insert(inputGroup->getIndex(), inputGroup->isTransmiting());

    return xmitFlags;
}

void MainController::handleNewNinjamInterval()
{
    // TODO move the jamRecorder to NinjamController?
    if (settings.recordingSettings.isSaveMultiTrackActivated()) {
        for (auto jamRecorder : jamRecorders)
            jamRecorder->newInterval();
    }

    if (mainWindow->cameraIsActivated())
        videoEncoder.startNewInterval();
}

void MainController::processCapturedFrame(int frameID, const QImage &frame)
{
    Q_UNUSED(frameID);

    if (ninjamController && ninjamController->isPreparedForTransmit())
        videoEncoder.encodeImage(frame); // video encoder will emit a signal when video frame is encoded
}

void MainController::requestCameraFrame(int intervalPosition)
{
    if (isPlayingInNinjamRoom() && mainWindow->cameraIsActivated()) {
        bool isFirstPart = intervalPosition == 0;
        if (isFirstPart || canGrabNewFrameFromCamera()) {
            static int frameID = 0;
            processCapturedFrame(frameID++, mainWindow->pickCameraFrame());
            lastFrameTimeStamp = QDateTime::currentMSecsSinceEpoch();
        }
    }
}

uint MainController::getFramesPerInterval() const
{
    auto intervalTimeInSeconds = ninjamController->getSamplesPerInterval()/getSampleRate();

    return intervalTimeInSeconds * CAMERA_FPS;
}

void MainController::updateBpi(int newBpi)
{
    if (settings.recordingSettings.isSaveMultiTrackActivated()) {
        for (auto jamRecorder : jamRecorders)
            jamRecorder->setBpi(newBpi);
    }
}

void MainController::updateBpm(int newBpm)
{
    if (settings.recordingSettings.isSaveMultiTrackActivated()) {
        for (auto jamRecorder : jamRecorders)
            jamRecorder->setBpm(newBpm);
    }

    if (isPlayingInNinjamRoom()) {
        for (auto inputTrack: inputTracks.values())
            inputTrack->getLooper()->stop(); // looper is stopped when BPM is changed because the recorded material will be out of sync
    }
}

void MainController::enqueueAudioDataToUpload(const QByteArray &encodedData, quint8 channelIndex, bool isFirstPart)
{
    Q_ASSERT(encodedData.left(4) == "OggS");

    if (isFirstPart) {
        if (audioIntervalsToUpload.contains(channelIndex)) {
            auto &audioInterval = audioIntervalsToUpload[channelIndex];

            // flush the end of previous interval
            ninjamService->sendIntervalPart(audioInterval.getGUID(), audioInterval.getData(), true); // is the last part of interval
        }

        UploadIntervalData newInterval; // generate a new GUID
        audioIntervalsToUpload.insert(channelIndex, newInterval);

        ninjamService->sendIntervalBegin(newInterval.getGUID(), channelIndex, true); // starting a new audio interval
    }

    if (audioIntervalsToUpload.contains(channelIndex)) {
        auto &interval = audioIntervalsToUpload[channelIndex];

        interval.appendData(encodedData);

        auto sendThreshold = isVoiceChatActivated(channelIndex) ? 1 : 4096; // when voice chat is activated jamtaba will send all small packets
        //qDebug() << "Sending threshdold: " << sendThreshold;
        bool canSend = interval.getTotalBytes() >= sendThreshold;
        if (canSend) {
            ninjamService->sendIntervalPart(interval.getGUID(), interval.getData(), false); // is not the last part of interval
            interval.clear();
        }
    }

    if (settings.recordingSettings.isSaveMultiTrackActivated() && isPlayingInNinjamRoom()) {
        for (auto jamRecorder : getActiveRecorders())
            jamRecorder->appendLocalUserAudio(encodedData, channelIndex, isFirstPart);
    }
}

void MainController::enqueueVideoDataToUpload(const QByteArray &encodedData, bool isFirstPart)
{
    if (!ninjamService)
        return;

    if (isFirstPart) {
        if (videoIntervalToUpload) {
            // flush the end of previous interval
            ninjamService->sendIntervalPart(videoIntervalToUpload->getGUID(), videoIntervalToUpload->getData(), true); // is the last part of interval
            videoIntervalToUpload->clear();
        }

        videoIntervalToUpload.reset(new UploadIntervalData()); // generate a new GUID

        static const auto videoChannelIndex = 1; // always sending video in 2nd channel to avoid drop intervals in first channel
        ninjamService->sendIntervalBegin(videoIntervalToUpload->getGUID(), videoChannelIndex, false); // starting a new video interval
    }

    if (!videoIntervalToUpload || !ninjamService)
        return;

    videoIntervalToUpload->appendData(encodedData);

    bool canSend = videoIntervalToUpload->getTotalBytes() >= 4096;
    if (canSend) {
        ninjamService->sendIntervalPart(videoIntervalToUpload->getGUID(), videoIntervalToUpload->getData(), false); // is not the last part of interval
        videoIntervalToUpload->clear();
    }

    // disabling the video recording
// if (settings.isSaveMultiTrackActivated() && isPlayingInNinjamRoom()) {
// for (auto jamRecorder : getActiveRecorders()) {
// jamRecorder->appendLocalUserVideo(encodedData, isFirstPart);
// }
// }
}

int MainController::getMaxAudioChannelsForEncoding(uint trackGroupIndex) const
{
    auto it = trackGroups.find(trackGroupIndex);
    if (it != trackGroups.end()) {
        return it.value()->getMaxInputChannelsForEncoding();
    }
    return 0;
}

void MainController::setUserName(const QString &newUserName)
{
    settings.storeUserName(newUserName);
}

QString MainController::getUserName() const
{
    return settings.getUserName();
}

QStringList MainController::getBotNames()
{
    return Service::getBotNamesList();
}


QString getFirstIpPart(const QString &ip){
    auto index = ip.indexOf(".");
    if (index)
        return ip.left(index);

    return ip;
}

login::Location MainController::getGeoLocation(const QString &ip)
{
    if (locationCache.isEmpty()) {
        return login::Location();
    }

    // try first level cache
    auto maskedIp = ninjam::client::maskIP(ip);
    {
        auto it = locationCache.find(maskedIp);
        if (it != locationCache.end()) {
            return it.value();
        }
    }

    // try second level cache
    auto halfIp = getFirstIpPart(ip);
    if (!halfIp.isEmpty()) {
        for (auto it = locationCache.keyValueBegin();
             it != locationCache.keyValueEnd(); ++it) {
            if (getFirstIpPart(it->first) == halfIp) {
                return it->second;
            }
        }
    }

    return login::Location();
}

void MainController::mixGroupedInputs(int groupIndex, audio::SamplesBuffer &out)
{
    auto it = trackGroups.find(groupIndex);
    if (it != trackGroups.end()) {
        it.value()->mixGroupedInputs(out);
    }
}

// this is called when a new ninjam interval is received and the 'record multi track' option is enabled
void MainController::saveEncodedAudio(const QString &userName, quint8 channelIndex, const QByteArray &encodedAudio)
{
    if (settings.recordingSettings.isSaveMultiTrackActivated()) { // just in case
        for (auto jamRecorder : getActiveRecorders())
            jamRecorder->addRemoteUserAudio(userName, encodedAudio, channelIndex);
    }
}

void MainController::removeAllInputTracks()
{
    for (int trackID : inputTracks.keys())
        removeInputTrackNode(trackID);
}

void MainController::removeInputTrackNode(int inputTrackIndex)
{
    QMutexLocker locker(&mutex);

    if (inputTracks.contains(inputTrackIndex)) {
        // remove from group
        auto inputTrack = inputTracks[inputTrackIndex];
        int trackGroupIndex = inputTrack->getChanneGroupIndex();
        auto it = trackGroups.find(trackGroupIndex);
        if (it != trackGroups.end()) {
            it.value()->removeInput(inputTrack);
            if (it.value()->isEmpty()) {
                trackGroups.erase(it);
            }
        }

        inputTracks.remove(inputTrackIndex);
        removeTrack(inputTrackIndex);
    }
}

int MainController::addInputTrackNode(QSharedPointer<audio::LocalInputNode> inputTrackNode)
{
    inputTracks.insert(inputTrackNode->getID(), inputTrackNode);
    addTrack(inputTrackNode);

    int trackGroupIndex = inputTrackNode->getChanneGroupIndex();
    auto it = trackGroups.find(trackGroupIndex);
    if (it != trackGroups.end()) {
        it.value()->addInputNode(inputTrackNode);
    } else {
        trackGroups.insert(trackGroupIndex, QSharedPointer<audio::LocalInputGroup>::create(trackGroupIndex, inputTrackNode));
    }

    return inputTrackNode->getID();
}

QSharedPointer<audio::LocalInputNode> MainController::getInputTrack(int localInputIndex)
{
    if (inputTracks.contains(localInputIndex))
        return inputTracks[localInputIndex];

    qCritical() << "wrong input track index " << localInputIndex;

    return nullptr;
}

QSharedPointer<audio::AudioNode> MainController::getTrackNode(long ID) const
{
    QMutexLocker locker(&mutex);
    auto it = tracksNodes.find(ID);
    if (it != tracksNodes.end()) {
        return it.value();
    }
    return nullptr;
}

bool MainController::addTrack(QSharedPointer<audio::AudioNode> trackNode)
{
    QMutexLocker locker(&mutex);

    tracksNodes.insert(trackNode->getID(), trackNode);
    audioMixer.addNode(trackNode);

    return true;
}

void MainController::storeChatFontSizeOffset(qint8 fontSizeOffset)
{
    settings.storeChatFontSizeOffset(fontSizeOffset);
}

void MainController::storeMultiTrackRecordingStatus(bool savingMultiTracks)
{
    if (settings.recordingSettings.isSaveMultiTrackActivated() && !savingMultiTracks) { // user is disabling recording multi tracks?
        for (auto jamRecorder : jamRecorders)
            jamRecorder->stopRecording();
    }

    settings.recordingSettings.setSaveMultiTrack(savingMultiTracks);
}

QMap<QString, QString> MainController::getJamRecoders() const
{
    QMap<QString, QString> jamRecoderMap;
    for (auto jamRecorder : jamRecorders)
        jamRecoderMap.insert(jamRecorder->getWriterId(), jamRecorder->getWriterName());

    return jamRecoderMap;
}

void MainController::storeJamRecorderStatus(const QString &writerId, bool status)
{
    if (settings.recordingSettings.isSaveMultiTrackActivated()) { // recording is active and changing the jamRecorder status
        for (auto jamRecorder : jamRecorders) {
            if (jamRecorder->getWriterId() == writerId) {
                if (status) {
                    if (isPlayingInNinjamRoom()) {
                        QDir recordingPath = QDir(settings.recordingSettings.getRecordingPath());
                        auto ninjamController = getNinjamController();
                        int bpi = ninjamController->getCurrentBpi();
                        int bpm = ninjamController->getCurrentBpm();
                        jamRecorder->startRecording(getUserName(), recordingPath, bpm, bpi, getSampleRate());
                    }
                } else {
                    jamRecorder->stopRecording();
                }
            }
        }
    }

    settings.recordingSettings.setJamRecorderActivated(writerId, status);
}

void MainController::storeMultiTrackRecordingPath(const QString &newPath)
{
    settings.recordingSettings.setRecordingPath(newPath);
    if (settings.recordingSettings.isSaveMultiTrackActivated()) {
        for (auto jamRecorder : jamRecorders)
            jamRecorder->setRecordPath(newPath);
    }
}

void MainController::storeDirNameDateFormat(const QString &newDateFormat)
{
    settings.recordingSettings.setDirNameDateFormat(newDateFormat);
    Qt::DateFormat dateFormat = settings.recordingSettings.getDirNameDateFormat();
    if (settings.recordingSettings.isSaveMultiTrackActivated()) {
        for (auto jamRecorder : jamRecorders)
            jamRecorder->setDirNameDateFormat(dateFormat);
    }
}

void MainController::storePrivateServerSettings(const QString &server, quint16 serverPort, const QString &password)
{
    settings.privateServerSettings.addPrivateServer(server, serverPort, password);
}

void MainController::storeMetronomeSettings(float metronomeGain, float metronomePan, bool metronomeMuted)
{
    settings.metronomeSettings.setGain(metronomeGain);
    settings.metronomeSettings.setPan(metronomePan);
    settings.metronomeSettings.setMuted(metronomeMuted);
}

void MainController::setBuiltInMetronome(const QString &metronomeAlias)
{
    settings.metronomeSettings.setBuiltInMetronome(metronomeAlias);
    recreateMetronome();
}

void MainController::setCustomMetronome(const QString &primaryBeatFile, const QString &offBeatFile, const QString &accentBeatFile)
{
    settings.metronomeSettings.setCustomMetronome(primaryBeatFile, offBeatFile, accentBeatFile);
    recreateMetronome();
}

void MainController::recreateMetronome()
{
    if (isPlayingInNinjamRoom())
        ninjamController->recreateMetronome(getSampleRate());
}

void MainController::storeIntervalProgressShape(int shape)
{
    settings.setIntervalProgressShape(shape);
}

void MainController::storeWindowSettings(bool maximized, const QPointF &location, const QSize &size)
{
    settings.windowSettings.setMaximized(maximized);
    settings.windowSettings.setLocation(location);
    settings.windowSettings.setSize(size);
}

void MainController::storeIOSettings(int firstIn, int lastIn, int firstOut, int lastOut, QString audioInputDevice, QString audioOutputDevice,
                                     const QList<bool> &midiInputsStatus, const QList<bool> &syncOutputsStatus)
{
    storeIOSettings(firstIn, lastIn, firstOut, lastOut, audioInputDevice, audioOutputDevice);
    settings.midiSettings.setInputDevicesStatus(midiInputsStatus);
    settings.syncSettings.setOutputDevicesStatus(syncOutputsStatus);
}

void MainController::storeIOSettings(int firstIn, int lastIn, int firstOut, int lastOut, QString audioInputDevice, QString audioOutputDevice)
{
    settings.audioSettings.setFirstInputIndex(firstIn);
    settings.audioSettings.setLastInputIndex(lastIn);
    settings.audioSettings.setFirstOutputIndex(firstOut);
    settings.audioSettings.setLastOutputIndex(lastOut);
    settings.audioSettings.setInputDevice(audioInputDevice);
    settings.audioSettings.setOutputDevice(audioOutputDevice);
}

void MainController::removeTrack(long trackID)
{
    QSharedPointer<audio::AudioNode> trackNode;
    {
        /** remove Track is called from ninjam service thread,
         *  and cause a crash if the process callback (audio Thread)
         *  is iterating over tracksNodes to render audio */
        QMutexLocker locker(&mutex);
        auto iterator = tracksNodes.find(trackID);
        if (iterator != tracksNodes.end()) {
            trackNode = iterator.value();
            tracksNodes.erase(iterator);
        }
    }
    if (trackNode) {
        audioMixer.removeNode(trackNode);
        trackNode->suspendProcessors();
    }
}

void MainController::setAllLoopersStatus(bool activated)
{
    for (auto inputTrack : inputTracks.values())
        inputTrack->getLooper()->setActivated(activated);
}

void MainController::doAudioProcess(const audio::SamplesBuffer &in, audio::SamplesBuffer &out, int sampleRate)
{
    auto incommingMidi = pullMidiMessagesFromDevices();
    audioMixer.process(in, out, sampleRate, incommingMidi);

    out.applyGain(masterGain, 1.0f); // using 1 as boost factor/multiplier (no boost)
    masterPeak.update(out.computePeak());
}

void MainController::process(const audio::SamplesBuffer &in, audio::SamplesBuffer &out, int sampleRate)
{
    QMutexLocker locker(&mutex);

    if (!started)
        return;

    try
    {
        if (!isPlayingInNinjamRoom()) {
            doAudioProcess(in, out, sampleRate);
        } else {
            if (ninjamController)
                ninjamController->process(in, out, sampleRate);
        }
    }
    catch (...)
    {
        qCritical() << "Exception in MainController::process";

        #ifdef Q_OS_WIN
        WindowsStackWalker stackWalker;
        stackWalker.ShowCallstack();
        #endif

        qFatal("Aborting in  MainController::process!");
    }
}

void MainController::syncWithNinjamIntervalStart(uint intervalLenght)
{
    for (auto inputTrack : inputTracks.values())
        inputTrack->startNewLoopCycle(intervalLenght);
}

audio::AudioPeak MainController::getTrackPeak(int trackID)
{
    auto trackNode = getTrackNode(trackID);

    if (trackNode && !trackNode->isMuted())
        return trackNode->getLastPeak();

    if (!trackNode)
        qWarning(jtGUI) << "trackNode not found! ID:" << trackID;

    return audio::AudioPeak();
}

audio::AudioPeak MainController::getRoomStreamPeak()
{
    return roomStreamer->getLastPeak();
}

void MainController::setVoiceChatStatus(int channelID, bool voiceChatActivated)
{
    auto it = trackGroups.find(channelID);
    if (it != trackGroups.end()) {
        it.value()->setVoiceChatStatus(voiceChatActivated);
    }
}

bool MainController::isVoiceChatActivated(int channelID) const
{
    auto it = trackGroups.find(channelID);
    if (it != trackGroups.end()) {
        return it.value()->isVoiceChatActivated();
    }
    return false;
}

void MainController::setTransmitingStatus(int channelID, bool transmiting)
{
    auto it = trackGroups.find(channelID);
    if (it != trackGroups.end()) {
        auto trackGroup = it.value();
        if (trackGroup->isTransmiting() != transmiting) {
            trackGroup->setTransmitingStatus(transmiting);
        }
    }
}

bool MainController::isTransmiting(int channelID) const
{
    auto it = trackGroups.find(channelID);
    if (it != trackGroups.end()) {
        return it.value()->isTransmiting();
    }
    return false;
}

void MainController::setTrackPan(int trackID, float pan, bool blockSignals)
{
    auto node = getTrackNode(trackID);
    if (node) {
        node->blockSignals(blockSignals);
        node->setPan(pan);
        node->blockSignals(false);
    }
}

void MainController::setTrackBoost(int trackID, float boostInDecibels)
{
    auto node = getTrackNode(trackID);
    if (node)
        node->setBoost(Utils::dbToLinear(boostInDecibels));
}

void MainController::resetTrack(int trackID)
{
    auto node = getTrackNode(trackID);
    if (node)
        node->reset();
}

void MainController::setTrackGain(int trackID, float gain, bool blockSignals)
{
    auto node = getTrackNode(trackID);
    if (node) {
        node->blockSignals(blockSignals);
        node->setGain(Utils::linearGainToPower(gain));
        node->blockSignals(false);
    }
}

void MainController::setMasterGain(float newGain)
{
    this->masterGain = Utils::linearGainToPower(newGain);
}

void MainController::setTrackMute(int trackID, bool muteStatus, bool blockSignals)
{
    auto node = getTrackNode(trackID);
    if (node) {
        node->blockSignals(blockSignals);
        node->setMute(muteStatus);
        node->blockSignals(false); // unblock signals by default
    }
}

void MainController::setTrackSolo(int trackID, bool soloStatus, bool blockSignals)
{
    auto node = getTrackNode(trackID);
    if (node) {
        node->blockSignals(blockSignals);
        node->setSolo(soloStatus);
        node->blockSignals(false);
    }
}

bool MainController::trackIsMuted(int trackID) const
{
    auto node = getTrackNode(trackID);

    if (node)
        return node->isMuted();

    return false;
}

bool MainController::trackIsSoloed(int trackID) const
{
    auto node = getTrackNode(trackID);

    if (node)
        return node->isSoloed();

    return false;
}

void MainController::setTrackStereoInversion(int trackID, bool stereoInverted)
{
    auto inputTrackNode = inputTracks[trackID];

    if (inputTrackNode)
        inputTrackNode->setStereoInversion(stereoInverted);
}

bool MainController::trackStereoIsInverted(int trackID) const
{
    auto inputTrackNode = inputTracks[trackID];

    if (inputTrackNode)
        return inputTrackNode->isStereoInverted();

    return false;
}

// +++++++++++++++++++++++++++++++++

MainController::~MainController()
{
    qCDebug(jtCore()) << "MainController destrutor!";

    if (mainWindow)
        mainWindow->detachMainController();

    stop();

    qCDebug(jtCore()) << "main controller stopped!";

    qCDebug(jtCore()) << "cleaning tracksNodes...";

    tracksNodes.clear();

    inputTracks.clear();

    trackGroups.clear();

    qCDebug(jtCore()) << "cleaning tracksNodes done!";

    qCDebug(jtCore()) << "cleaning jamRecorders...";

    for (auto jamRecorder : jamRecorders)
        delete jamRecorder;

    audioIntervalsToUpload.clear();

    qCDebug(jtCore()) << "cleaning jamRecorders done!";

    qCDebug(jtCore) << "MainController destructor finished!";

    qDebug() << MainController::CRASH_FLAG_STRING; // used to put a crash flag in the log file
}

void MainController::saveLastUserSettings(const persistence::LocalInputTrackSettings &inputsSettings)
{
    if (inputsSettings.isValid()) { // avoid save empty settings
        settings.setRecentEmojis(emojiManager.getRecents());
        settings.storeMasterGain(Utils::poweredGainToLinear(getMasterGain()));
        settings.save(inputsSettings);
    }
}

// -------------------------      PRESETS   ----------------------------

QStringList MainController::getPresetList()
{
    return Configurator::getInstance()->getPresetFilesNames(false);
}

void MainController::savePreset(const LocalInputTrackSettings &inputsSettings, const QString &name)
{
    settings.writePresetToFile(persistence::Preset(name, inputsSettings));
}

void MainController::deletePreset(const QString &name)
{
    return settings.deletePreset(name);
}

persistence::Preset MainController::loadPreset(const QString &name)
{
    return settings.readPresetFromFile(name); // allow multi subchannels by default
}

// +++++++++++++++++++++++++++++++++++++++

void MainController::setFullScreenView(bool fullScreen)
{
    settings.windowSettings.setFullScreenMode(fullScreen);
}

void MainController::setAllTracksActivation(bool activated)
{
    QMutexLocker locker(&mutex);
    for (auto track : tracksNodes) {
        if (activated)
            track->activate();
        else
            track->deactivate();
    }
}

void MainController::playRoomStream(const login::RoomInfo &roomInfo)
{
    if (roomInfo.hasStream()) {
        roomStreamer->setStreamPath(roomInfo.getStreamUrl());
        currentStreamingRoomID = roomInfo.getUniqueName();

        // mute all tracks and unmute the room Streamer
        setAllTracksActivation(false);
        roomStreamer->activate();
    }
}

void MainController::stopRoomStream()
{
    roomStreamer->stopCurrentStream();
    currentStreamingRoomID = "";

    setAllTracksActivation(true);
    // roomStreamer->setMuteStatus(true);
}

bool MainController::isPlayingRoomStream() const
{
    return roomStreamer && roomStreamer->isStreaming();
}

void MainController::enterInRoom(const login::RoomInfo &room, const QList<ninjam::client::ChannelMetadata> &channels, const QString &password)
{
    qCDebug(jtCore) << "EnterInRoom slot";

    if (isPlayingRoomStream())
        stopRoomStream();

    tryConnectInNinjamServer(room, channels, password);
}

void MainController::sendNewChannelsNames(const QList<ninjam::client::ChannelMetadata> &channels)
{
    if (isPlayingInNinjamRoom())
        ninjamService->sendNewChannelsListToServer(channels);
}

void MainController::sendRemovedChannelMessage(int removedChannelIndex)
{
    if (isPlayingInNinjamRoom())
        ninjamService->sendRemovedChannelIndex(removedChannelIndex);
}

void MainController::tryConnectInNinjamServer(const login::RoomInfo &ninjamRoom, const QList<ninjam::client::ChannelMetadata> &channels, const QString &password)
{
    qCDebug(jtCore) << "connecting...";

    if (userNameWasChoosed()) { // just in case :)
        QString serverIp = ninjamRoom.getName();
        int serverPort = ninjamRoom.getPort();
        QString userName = getUserName();
        QString userPass = (password.isNull() || password.isEmpty()) ? "" : password;

        if (ninjamRoom.hasPreferredUserCredentials()) {
            userName = ninjamRoom.getPreferredUserName();
            userPass = ninjamRoom.getPreferredUserPass();
        }

        this->ninjamService->startServerConnection(serverIp, serverPort, userName, channels, userPass);
    } else {
        qCritical() << "user name not choosed yet!";
    }
}

void MainController::start()
{
    if (!started) {
        qCInfo(jtCore) << "Creating roomStreamer ...";

        roomStreamer = QSharedPointer<audio::NinjamRoomStreamerNode>::create(); // new Audio::AudioFileStreamerNode(":/teste.mp3");
        this->audioMixer.addNode(roomStreamer);

        connect(ninjamService.data(), &Service::connectedInServer, this, &MainController::connectInNinjamServer);

        connect(ninjamService.data(), &Service::disconnectedFromServer, this, &MainController::disconnectFromNinjamServer);

        connect(ninjamService.data(), &Service::error, this, &MainController::quitFromNinjamServer);

        qInfo() << "Starting " + getUserEnvironmentString();

        started = true;
    }
}

QString MainController::getUserEnvironmentString() const
{
    QString systemName = QSysInfo::prettyProductName();
    QString userMachineArch = QSysInfo::currentCpuArchitecture();
    QString jamtabaArch = QSysInfo::buildCpuArchitecture();
    QString version = QApplication::applicationVersion();
    QString flavor = getJamtabaFlavor();
    return "Jamtaba " + version + " " + flavor + " (" + jamtabaArch + ") running on " + systemName
           + " (" + userMachineArch + ")";
}

void MainController::stop()
{
    if (started) {
        qCDebug(jtCore) << "Stopping MainController...";

        if (ninjamController)
            ninjamController->stop(false); // block disconnected signal

        started = false;
    }
}

// +++++++++++

bool MainController::setTheme(const QString &themeName)
{
    QString themeDir(Configurator::getInstance()->getThemesDir().absolutePath());
    QString themeCSS = theme::Loader::loadCSS(themeDir, themeName);

    if (!themeCSS.isEmpty()) {
        setCSS(themeCSS);
        settings.setTheme(themeName);
        emit themeChanged();
        return true;
    }

    return false;
}

login::LoginService *MainController::getLoginService() const
{
    return const_cast<login::LoginService *>(&loginService);
}

bool MainController::isPlayingInNinjamRoom() const
{
    if (ninjamController)
        return ninjamController->isRunning();

    return false;
}

// ++++++++++++= NINJAM ++++++++++++++++

void MainController::stopNinjamController()
{
    QMutexLocker locker(&mutex);

    if (ninjamController && ninjamController->isRunning())
        ninjamController->stop(true);

    audioIntervalsToUpload.clear();

    videoEncoder.finish(); // release memory used by video encoder
}

void MainController::setTranslationLanguage(const QString &languageCode)
{
    settings.setTranslation(languageCode);
}

QString MainController::getSuggestedUserName()
{
    // try get user name in home path. If is empty try the environment variables.

    QString userName = QDir::home().dirName();
    if (!userName.isEmpty())
        return userName;

    userName = qgetenv("USER"); // for MAc or Linux
    if (userName.isEmpty())
        userName = qgetenv("USERNAME"); // for windows

    if (!userName.isEmpty())
        return userName;

    return ""; // returning empty name as suggestion
}

void MainController::storeMeteringSettings(bool showingMaxPeaks, persistence::MeterMode meterOption)
{
    settings.meteringSettings.setOption(meterOption);
    settings.meteringSettings.setShowingMaxPeakMarkers(showingMaxPeaks);
}

QSharedPointer<audio::LocalInputNode> MainController::getInputTrackInGroup(quint8 groupIndex, quint8 trackIndex) const
{
    auto it = trackGroups.find(groupIndex);
    if (it != trackGroups.end()) {
        return it.value()->getInputNode(trackIndex);
    }
    return nullptr;
}

bool MainController::canGrabNewFrameFromCamera() const
{
    static const quint64 timeBetweenFrames = 1000/CAMERA_FPS;

    const quint64 now = QDateTime::currentMSecsSinceEpoch();

    return (now - lastFrameTimeStamp) >= timeBetweenFrames;
}

QList<recorder::JamRecorder *> MainController::getActiveRecorders() const
{
    QList<recorder::JamRecorder *> activeRecorders;

    for (auto jamRecorder : jamRecorders) {
        if (settings.recordingSettings.isJamRecorderActivated(jamRecorder->getWriterId()))
            activeRecorders.append(jamRecorder);
    }

    return activeRecorders;
}

QString MainController::getVersionFromLogContent()
{
    auto configurator = Configurator::getInstance();
    QStringList logContent = configurator->getPreviousLogContent();

    static const QString START_LINE("Starting Jamtaba ");
    for (const QString &logLine : logContent) {
        if (logLine.contains(START_LINE))
            return logLine.mid(logLine.indexOf(START_LINE) + START_LINE.length(), 6).trimmed();
    }

    return QString();
}

bool MainController::crashedInLastExecution()
{
    // crash in last execution is detected from version 2.1.1
    QString version = getVersionFromLogContent();
    QStringList versionParts = version.split(".");
    if (versionParts.size() != 3) {
        qWarning() << "Version string must have 3 elements " << version;
        return false;
    }

    if (!(versionParts.at(1).toInt() >= 1 && versionParts.at(2).toInt() >= 1)) {
        qWarning() << "Cant' detect crash in older versions " << version;
        return false;
    }

    auto configurator = Configurator::getInstance();
    QStringList logContent = configurator->getPreviousLogContent();
    if (!logContent.isEmpty()) {
        for (const QString &logLine : logContent) {
            if (logLine.contains(MainController::CRASH_FLAG_STRING))
                return false;
        }

        return true;
    }

    return false;
}

void MainController::setPublicChatActivated(bool activated)
{
    settings.setPublicChatActivated(activated);
}
