#include "AudioController.h"
#include "audio/core/AudioNode.h"
#include "audio/core/LocalInputGroup.h"
#include "audio/Encoder.h"
#include "audio/vorbis/Vorbis.h"
#include "audio/vorbis/VorbisEncoder.h"
#include "log/Logging.h"
#include "Helpers.h"
#include "Utils.h"
#include <QtConcurrent>

using namespace controller;

AudioController::AudioController() :
    thread(new QThread()),
    pluginsThreadPool(createQSharedPointer<QThreadPool>()),
    audioMixer(44100)
{
    static bool registered = false;
    if (!registered) {
        qRegisterMetaType<AudioChannelData>();
        qRegisterMetaType<QFutureInterface<QList<AudioChannelData>>>();
        qRegisterMetaType<QSharedPointer<audio::AudioNode>>();
        qRegisterMetaType<QVector<midi::MidiMessage>>();
        qRegisterMetaType<AudioNodeCallback<audio::AudioNode>>();
        qRegisterMetaType<AudioNodeCallback<audio::LocalInputGroup>>();
        qRegisterMetaType<AudioNodeCallback<audio::LocalInputNode>>();
        registered = true;
    }

    masterPeak.zero();
    thread->setObjectName("AudioThread");
    thread->setPriority(QThread::TimeCriticalPriority);
    pluginsThreadPool->setMaxThreadCount(1);
    pluginsThreadPool->setObjectName("PluginsThreadPool");
    pluginsThreadPool->setExpiryTimeout(-1);
}

AudioController::~AudioController()
{
    stop();
}

void AudioController::start() {
    moveToThread(thread.get());
    connect(this, &AudioController::postSetSampleRate, this, &AudioController::setSampleRate);
    connect(this, &AudioController::postSetMasterGain, this, &AudioController::setMasterGain);
    connect(this, &AudioController::postAddTrack, this, &AudioController::addTrack);
    connect(this, &AudioController::postAddMixerTrack, this, &AudioController::addMixerTrack);
    connect(this, &AudioController::postRemoveTrack, this, &AudioController::removeTrack);
    connect(this, &AudioController::postRemoveAllInputTracks, this, &AudioController::removeAllInputTracks);
    connect(this, &AudioController::postSetAllLoopersStatus, this, &AudioController::setAllLoopersStatus);
    connect(this, &AudioController::postStopAllLoopers, this, &AudioController::stopAllLoopers);
    connect(this, &AudioController::postStartNewLoopersCycle, this, &AudioController::startNewLoopersCycle);
    connect(this, &AudioController::postSetAllTracksActivation, this, &AudioController::setAllTracksActivation);
    connect(this, &AudioController::postSetVoiceChatStatus, this, &AudioController::setVoiceChatStatus);
    connect(this, &AudioController::postEnumTracks, this, &AudioController::enumTracks);
    connect(this, &AudioController::postEnumInputGroups, this, &AudioController::enumInputGroups);
    connect(this, &AudioController::postEnumInputs, this, &AudioController::enumInputs);
    connect(this, &AudioController::postEnumInputsOnPool, this, &AudioController::enumInputsOnPool);
    connect(this, &AudioController::postSetTransmitingStatus, this, &AudioController::setTransmitingStatus);
    connect(this, &AudioController::postMixInputSubchannels, this, &AudioController::mixInputSubchannelsAudio);
    connect(this, &AudioController::postReceiveProcess, this, &AudioController::receiveProcess);
    thread->start();
}

void AudioController::stop()
{
    if (thread->isRunning()) {
        qCDebug(jtAudio()) << "stopping autio thread...";
        thread->quit();
        thread->wait();
        qCDebug(jtAudio()) << "stopping autio thread done!";
    }
    qCDebug(jtAudio()) << "cleaning tracksNodes...";
    tracksNodes.clear();
    inputTracks.clear();
    trackGroups.clear();
    audioMixer.removeAllNodes();
    qCDebug(jtAudio()) << "cleaning tracksNodes done!";
    masterPeak.zero();
}

void AudioController::setSampleRate(int newSampleRate)
{
    audioMixer.setSampleRate(newSampleRate);
}

float AudioController::getMasterGain() const
{
    return Utils::poweredGainToLinear(audioMixer.getMasterGain());
}

void AudioController::setMasterGain(float newGain)
{
    audioMixer.setMasterGain(Utils::linearGainToPower(newGain));
}

void AudioController::addTrack(QSharedPointer<audio::AudioNode> trackNode)
{
    auto inputTrackNode = trackNode.dynamicCast<audio::LocalInputNode>();
    if (inputTrackNode) {
        auto inputGroup = createInputTrackGroup(inputTrackNode->getChannelGroupIndex());
        inputGroup->addInputNode(inputTrackNode);
        inputTrackNode->attachChannelGroup(inputGroup);
        inputTracks.insert(inputTrackNode->getID(), inputTrackNode);
    }
    tracksNodes.insert(trackNode->getID(), trackNode);
    audioMixer.addNode(trackNode);
}

void AudioController::addMixerTrack(QSharedPointer<audio::AudioNode> trackNode)
{
    audioMixer.addNode(trackNode);
}

void AudioController::removeTrack(int trackID)
{
    QSharedPointer<audio::AudioNode> trackNode;
    auto iterator = tracksNodes.find(trackID);
    if (iterator != tracksNodes.end()) {
        trackNode = iterator.value();
        tracksNodes.erase(iterator);
    }
    if (trackNode) {
        audioMixer.removeNode(trackNode);
    }
    removeInputTrackNode(trackID);
}

void AudioController::removeAllInputTracks()
{
    auto trackIds = inputTracks.keys();
    for (int trackID : qAsConst(trackIds)) {
        removeTrack(trackID);
    }
}

void AudioController::removeInputTrackNode(int trackID)
{
    QSharedPointer<audio::LocalInputNode> inputTrack;
    {
        auto trackIt = inputTracks.find(trackID);
        if (trackIt != inputTracks.end()) {
            // remove from group
            inputTrack = trackIt.value();
            int trackGroupIndex = inputTrack->getChannelGroupIndex();
            auto it = trackGroups.find(trackGroupIndex);
            if (it != trackGroups.end()) {
                it.value()->removeInput(inputTrack);
                if (it.value()->isEmpty()) {
                    trackGroups.erase(it);
                }
            }

            inputTracks.erase(trackIt);
        }
    }
    if (inputTrack) {
        QtConcurrent::run(pluginsThreadPool.data(), [](QSharedPointer<audio::LocalInputNode> inputTrack) {
            inputTrack->suspendProcessors();
            return 0;
        }, inputTrack);
    }
}

void AudioController::setAllLoopersStatus(bool activated)
{
    for (const auto& inputTrack : qAsConst(inputTracks)) {
        inputTrack->getLooper()->setActivated(activated);
    }
}

void AudioController::stopAllLoopers()
{
    for (const auto& inputTrack : qAsConst(inputTracks)) {
        inputTrack->getLooper()->stop();
    }
}

void AudioController::startNewLoopersCycle(uint samplesInCycle)
{
    for (const auto& inputTrack : qAsConst(inputTracks)) {
        inputTrack->getLooper()->startNewCycle(samplesInCycle);
    }
}

void AudioController::setAllTracksActivation(bool activated)
{
    for (const auto& track : qAsConst(tracksNodes)) {
        track->setActivated(activated);
    }
}

void AudioController::setVoiceChatStatus(int channelID, bool voiceChatActivated)
{
    auto it = trackGroups.find(channelID);
    if (it != trackGroups.end()) {
        it.value()->setVoiceChatStatus(voiceChatActivated);
    }
}

void AudioController::setTransmitingStatus(int channelID, bool transmiting)
{
    auto it = trackGroups.find(channelID);
    if (it != trackGroups.end()) {
        auto trackGroup = it.value();
        if (trackGroup->isTransmiting() != transmiting) {
            trackGroup->setTransmitingStatus(transmiting);
        }
    }
}

const QSharedPointer<QThreadPool>& AudioController::getPluginsThreadPool() const
{
    return pluginsThreadPool;
}

void AudioController::enumTracks(QFutureInterface<void> futureInterface,
                                 AudioNodeCallback<audio::AudioNode> callback)
{
    try {
        for (const auto& trackNode : qAsConst(tracksNodes)) {
            if (!callback(trackNode)) {
                break;
            }
        }
    } catch (...) {
        qCritical(jtAudio()) << "Exception in AudioController::enumTracks";
    }
    futureInterface.reportFinished();
}

void AudioController::enumInputGroups(QFutureInterface<void> futureInterface,
                                      AudioNodeCallback<audio::LocalInputGroup> callback)
{
    try {
        for (const auto& trackGroup : qAsConst(trackGroups)) {
            if (!callback(trackGroup)) {
                break;
            }
        }
    } catch (...) {
        qCritical(jtAudio()) << "Exception in AudioController::enumInputGroups";
    }
    futureInterface.reportFinished();
}

void AudioController::enumInputs(AudioNodeCallback<audio::LocalInputNode> callback)
{
    try {
        for (const auto& inputTrack : qAsConst(inputTracks)) {
            if (!callback(inputTrack)) {
                break;
            }
        }
    } catch (...) {
        qCritical(jtAudio()) << "Exception in AudioController::enumInputs";
    }
}

void AudioController::enumInputsOnPool(AudioNodeCallback<audio::LocalInputNode> callback,
                                       QSharedPointer<QThreadPool> threadPool)
{
    enumInputs([pool = threadPool, callback](QSharedPointer<audio::LocalInputNode> inputTrack) {
        QtConcurrent::run(pool.data(), callback, inputTrack);
        return true;
    });
}

void AudioController::mixInputSubchannelsAudio(QFutureInterface<QList<AudioChannelData>> futureInterface, int samples)
{
    QList<AudioChannelData> result;
    for (auto it = trackGroups.begin(); it != trackGroups.end(); ++it) {
        const auto& inputGroup = it.value();
        if (/*encoders.contains(it.key()) && */inputGroup->isTransmiting()) {
            int channels = inputGroup->getMaxInputChannelsForEncoding();
            if (channels > 0) {
                auto mixedInputData = createQSharedPointer<audio::SamplesBuffer>(channels, samples);
                mixedInputData->zero();
                inputGroup->mixGroupedInputs(*mixedInputData);

                AudioChannelData mixedChannelData;
                mixedChannelData.channelID = it.key();
                mixedChannelData.samples = mixedInputData;
                mixedChannelData.sampleRate = audioMixer.getSampleRate();
                mixedChannelData.isVoiceChat = inputGroup->isVoiceChatActivated();
                result.append(mixedChannelData);
            }
        }
    }
    futureInterface.reportResult(result);
    futureInterface.reportFinished();
}

void AudioController::receiveProcess(QFutureInterface<void> futureInterface,
                                     const QSharedPointer<audio::SamplesBuffer>& in,
                                     const QSharedPointer<audio::SamplesBuffer>& out,
                                     QVector<midi::MidiMessage> midiMessages)
{
    audioMixer.process(*in, *out, midiMessages);
    masterPeak.update(out->computePeak());
    futureInterface.reportFinished();
    emit masterPeakChanged(masterPeak);
}

QSharedPointer<audio::LocalInputGroup> AudioController::createInputTrackGroup(int groupIndex)
{
    auto it = trackGroups.find(groupIndex);
    if (it != trackGroups.end()) {
        return it.value();
    }
    auto group = createQSharedPointer<audio::LocalInputGroup>(groupIndex);
    trackGroups.insert(groupIndex, group);
    return group;
}

QSharedPointer<audio::LocalInputNode> AudioController::createInputNodeAsync(int groupIndex, const QSharedPointer<audio::Looper>& looper)
{
    auto inputNode = createQSharedPointer<audio::LocalInputNode>(groupIndex, looper, 44100);
    inputNode->setBoost(0.0f);
    addTrackAsync(inputNode);
    return inputNode;
}

void AudioController::manageTrack(const QSharedPointer<audio::AudioNode>& trackNode)
{
    trackNode->moveToThread(thread.get());
}

void AudioController::addTrackAsync(const QSharedPointer<audio::AudioNode>& trackNode)
{
    trackNode->moveToThread(thread.get());
    emit postAddTrack(trackNode, {});
}

void AudioController::addMixerTrackAsync(const QSharedPointer<audio::AudioNode>& trackNode)
{
    trackNode->moveToThread(thread.get());
    emit postAddMixerTrack(trackNode, {});
}

QFuture<void> AudioController::visitInputGroups(controller::AudioNodeCallback<audio::LocalInputGroup>&& callback)
{
    QFutureInterface<void> futureInterface;
    futureInterface.reportStarted();
    emit postEnumInputGroups(futureInterface, callback);
    return futureInterface.future();
}

QFuture<QList<AudioChannelData>> AudioController::mixInputSubchannels(int samples)
{
    QFutureInterface<QList<AudioChannelData>> futureInterface;
    futureInterface.reportStarted();
    emit postMixInputSubchannels(futureInterface, samples);
    return futureInterface.future();
}

QFuture<void> AudioController::processAudio(const QSharedPointer<audio::SamplesBuffer>& in,
                                            const QSharedPointer<audio::SamplesBuffer>& out,
                                            const QVector<midi::MidiMessage>& midiMessages)
{
    QFutureInterface<void> futureInterface;
    futureInterface.reportStarted();
    emit postReceiveProcess(futureInterface, in, out, midiMessages);
    return futureInterface.future();
}
