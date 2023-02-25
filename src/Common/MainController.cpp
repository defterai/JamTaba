#include <QtGlobal>

#ifdef Q_OS_WIN
    #include "log/stackwalker/WindowsStackWalker.h"
#endif

#include "MainController.h"
#include "NinjamController.h"
#include "Utils.h"
#include "Helpers.h"
#include "loginserver/natmap.h"
#include "audio/core/AudioNode.h"
#include "audio/core/LocalInputNode.h"
#include "audio/core/LocalInputGroup.h"
#include "audio/core/Plugins.h"
#include "audio/RoomStreamerNode.h"
#include "controller/AudioController.h"
#include "ninjam/client/Service.h"
#include "recorder/JamRecorder.h"
#include "recorder/ReaperProjectGenerator.h"
#include "recorder/ClipSortLogGenerator.h"
#include "gui/MainWindow.h"
#include "gui/ThemeLoader.h"
#include "log/Logging.h"
#include "ninjam/client/Types.h"

#include <QFuture>
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
    audioController(createQSharedPointer<controller::AudioController>()),
    loginService(this),
    ninjamService(new Service()),
    settings(settings),
    mainWindow(nullptr),
    mutex(QMutex::Recursive),
    videoEncoder(),
    currentStreamingRoomID(-1000),
    started(false),
    usersDataCache(createQSharedPointer<UsersDataCache>(Configurator::getInstance()->getCacheDir())),
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
    audioController->postSetSampleRate(newSampleRate);

    if (settings.recordingSettings.isSaveMultiTrackActivated()) {
        for (auto jamRecorder : jamRecorders)
            jamRecorder->setSampleRate(newSampleRate);
    }

    if (isPlayingInNinjamRoom()) {
        ninjamController->setSampleRate(newSampleRate);
        // looper is stopped when sample rate is changed because the recorded material will sound funny :)
        audioController->postStopAllLoopers();
    }

    settings.audioSettings.setSampleRate(newSampleRate);
}

void MainController::setEncodingQuality(float newEncodingQuality)
{
    settings.audioSettings.setEncodingQuality(newEncodingQuality);

    if (isPlayingInNinjamRoom())
        ninjamController->setEncodingQuality(newEncodingQuality);
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
        // looper is stopped when BPM is changed because the recorded material will be out of sync
        audioController->postStopAllLoopers();
    }
}

void MainController::enqueueAudioDataToUpload(const controller::AudioChannelData& channelData, const QByteArray& encodedData)
{
    Q_ASSERT(encodedData.left(4) == "OggS");

    int channelIndex = channelData.channelID;
    if (channelData.isFirstPart) {
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

        auto sendThreshold = channelData.isVoiceChat ? 1 : 4096; // when voice chat is activated jamtaba will send all small packets
        //qDebug() << "Sending threshdold: " << sendThreshold;
        bool canSend = interval.getTotalBytes() >= sendThreshold;
        if (canSend) {
            ninjamService->sendIntervalPart(interval.getGUID(), interval.getData(), false); // is not the last part of interval
            interval.clear();
        }
    }

    if (settings.recordingSettings.isSaveMultiTrackActivated() && isPlayingInNinjamRoom()) {
        for (auto jamRecorder : getActiveRecorders())
            jamRecorder->appendLocalUserAudio(encodedData, channelIndex, channelData.isFirstPart);
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

// this is called when a new ninjam interval is received and the 'record multi track' option is enabled
void MainController::saveEncodedAudio(const QString &userName, quint8 channelIndex,
                                      const QSharedPointer<QByteArray> &encodedAudio)
{
    if (settings.recordingSettings.isSaveMultiTrackActivated()) { // just in case
        for (auto jamRecorder : getActiveRecorders())
            jamRecorder->addRemoteUserAudio(userName, encodedAudio, channelIndex);
    }
}

QSharedPointer<audio::LocalInputNode> MainController::createInputNode(int groupIndex)
{
    auto looper = createQSharedPointer<audio::Looper>(settings.looperSettings.getPreferredMode(),
                                                      settings.looperSettings.getPreferredLayersCount());
    return audioController->createInputNodeAsync(groupIndex, looper);
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

void MainController::storeChannelInstrumentIndex(int channelId, int instrumentIndex)
{
    channelInstrumentIndex[channelId] = instrumentIndex;
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
    updateMetronomeSound();
}

void MainController::setCustomMetronome(const QString &primaryBeatFile, const QString &offBeatFile, const QString &accentBeatFile)
{
    settings.metronomeSettings.setCustomMetronome(primaryBeatFile, offBeatFile, accentBeatFile);
    updateMetronomeSound();
}

void MainController::updateMetronomeSound()
{
    if (isPlayingInNinjamRoom())
        ninjamController->updateMetronomeSound(settings.metronomeSettings);
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

void MainController::process(const QSharedPointer<audio::SamplesBuffer>& in, const QSharedPointer<audio::SamplesBuffer>& out)
{
    if (!started)
        return;

    try
    {
        if (!isPlayingInNinjamRoom()) {
            audioController->processAudio(in, out, pullMidiMessagesFromDevices()).waitForFinished();
        } else {
            if (ninjamController)
                ninjamController->process(in, out);
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
    emit audioController->postStartNewLoopersCycle(intervalLenght);
}

audio::AudioPeak MainController::getRoomStreamPeak()
{
    return roomStreamer->getLastPeak();
}

// +++++++++++++++++++++++++++++++++

MainController::~MainController()
{
    qCDebug(jtCore()) << "MainController destrutor!";

    if (mainWindow)
        mainWindow->detachMainController();

    stop();

    qCDebug(jtCore()) << "main controller stopped!";

    qCDebug(jtCore()) << "cleaning jamRecorders...";

    for (auto jamRecorder : jamRecorders)
        delete jamRecorder;

    audioIntervalsToUpload.clear();

    qCDebug(jtCore()) << "cleaning jamRecorders done!";

    qCDebug(jtCore) << "MainController destructor finished!";

    qDebug() << MainController::CRASH_FLAG_STRING; // used to put a crash flag in the log file
}

QList<persistence::Plugin> buildPersistentPluginList(const QSharedPointer<audio::LocalInputNode>& inputNode)
{
    QList<persistence::Plugin> persistentPlugins;
    for (const auto& p : inputNode->getProcessors<audio::Plugin>()) {
        auto plugin = persistence::Plugin::Builder(p->getDescriptor())
                .setBypassed(p->isBypassed())
                .setData(p->getSerializedData())
                .build();
        persistentPlugins.append(plugin);
    }
    return persistentPlugins;
}

LocalInputTrackSettings MainController::getLastInputsSettings() const
{
    LocalInputTrackSettings::Builder settingsBuilder;
    audioController->visitInputGroups([&](QSharedPointer<audio::LocalInputGroup> inputGroup) {
        Channel::Builder channelBuilder;
        channelBuilder.setInstrumentIndex(channelInstrumentIndex[inputGroup->getIndex()]);
        int subchanelsCount = 0;
        for (auto inputNode = inputGroup->getInputNode(0);
             inputNode; inputNode = inputGroup->getInputNode(subchanelsCount)) {
            const auto& audioInputProps = inputNode->getAudioInputProps();
            const auto& midiInputProps = inputNode->getMidiInputProps();
            SubChannel subChannel = SubChannel::Builder()
                    .setFirstInput(audioInputProps.getChannelRange().getFirstChannel())
                    .setChannelsCount(audioInputProps.getChannelRange().getChannels())
                    .setMidiDevice(midiInputProps.getDevice())
                    .setMidiChannel(midiInputProps.getChannel())
                    .setGain(Utils::poweredGainToLinear(inputNode->getGain()))
                    .setBoost(Utils::linearToDb(inputNode->getBoost()))
                    .setPan(inputNode->getPan())
                    .setMuted(inputNode->isMuted())
                    .setStereoInverted(audioInputProps.isStereoInverted())
                    .setTranspose(midiInputProps.getTranspose())
                    .setLowerMidiNote(midiInputProps.getLowerNote())
                    .setHigherMidiNote(midiInputProps.getHigherNote())
                    .setRoutingMidiToFirstSubchannel(subchanelsCount > 0 && inputNode->isRoutingMidiInput()) // midi routing is not allowed in first subchannel
                    .build();
            subChannel.setPlugins(buildPersistentPluginList(inputNode));
            channelBuilder.addSubChannel(subChannel);
            subchanelsCount++;
        }
        settingsBuilder.addChannel(channelBuilder.build());
        return true; // continue next group
    }).waitForFinished();
    return settingsBuilder.build();
}

void MainController::saveLastUserSettings()
{
    auto inputsSettings = getLastInputsSettings();
    if (inputsSettings.isValid()) { // avoid save empty settings
        settings.setRecentEmojis(emojiManager.getRecents());
        audioController->postTask([&]() {
            settings.storeMasterGain(audioController->getMasterGain());
        }).waitForFinished();
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

void MainController::playRoomStream(const login::RoomInfo &roomInfo)
{
    if (roomInfo.hasStream()) {
        emit roomStreamer->postStartStream(roomInfo.getStreamUrl());
        currentStreamingRoomID = roomInfo.getUniqueName();

        // mute all tracks (except local input) and unmute the room Streamer
        emit audioController->postEnumTracks(QFutureInterface<void>::canceledResult(),
                                             [](const QSharedPointer<AudioNode>& trackNode) {
            if (trackNode.dynamicCast<LocalInputNode>().isNull()) {
                trackNode->setActivated(false);
            }
            return true;
        });
        emit roomStreamer->postSetActivated(true);
    }
}

void MainController::stopRoomStream()
{
    emit roomStreamer->postStopStream();
    currentStreamingRoomID = "";

    emit audioController->postSetAllTracksActivation(true);
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
        audioController->start();

        qCInfo(jtCore) << "Creating roomStreamer ...";
        roomStreamer = createQSharedPointer<audio::NinjamRoomStreamerNode>();
        audioController->addMixerTrackAsync(roomStreamer);

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

        audioController->stop();

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

bool MainController::isVoiceChatActivated(int channelID) const
{
    return channelChatActivated.contains(channelID);
}

void MainController::setVoiceChatActivated(int channelID, bool activated)
{
    emit audioController->postSetVoiceChatStatus(channelID, activated);
    if (activated) {
        channelChatActivated.insert(channelID);
    } else {
        channelChatActivated.remove(channelID);
    }
}
