#ifndef AUDIOCONTROLLER_H
#define AUDIOCONTROLLER_H

#include <QObject>
#include <QThread>
#include <QThreadPool>
#include <QFutureInterface>
#include <QScopedPointer>
#include <QSharedPointer>

#include "audio/core/AudioMixer.h"
#include "audio/core/AudioPeak.h"
#include "audio/core/LocalInputNode.h"
#include "Helpers.h"

class AudioEncoder;

namespace audio
{
class SamplesBuffer;
}

namespace controller
{

struct AudioChannelData
{
    int channelID;
    QSharedPointer<audio::SamplesBuffer> samples;
    int sampleRate;
    bool isVoiceChat;
    bool isFirstPart;
    bool isLastPart;
};

template<class T>
using AudioNodeCallback = std::function<bool(const QSharedPointer<T>&)>;

class AudioController final : public TaskObject
{
    Q_OBJECT

public:
    AudioController();
    ~AudioController();
    void start();
    void stop();

    inline int getInputTracksCount() const
    {
        return inputTracks.size();     // return the track input count
    }

    float getMasterGain() const;

    const QSharedPointer<QThreadPool>& getPluginsThreadPool() const;

    QSharedPointer<audio::LocalInputNode> createInputNodeAsync(int groupIndex, const QSharedPointer<audio::Looper>& looper);

    void manageTrack(const QSharedPointer<audio::AudioNode>& trackNode);
    void addTrackAsync(const QSharedPointer<audio::AudioNode>& trackNode);
    void addMixerTrackAsync(const QSharedPointer<audio::AudioNode>& trackNode);

    QFuture<void> visitInputGroups(controller::AudioNodeCallback<audio::LocalInputGroup>&& callback);

    QFuture<QList<AudioChannelData>> mixInputSubchannels(int samples);
    QFuture<void> processAudio(const QSharedPointer<audio::SamplesBuffer>& in,
                               const QSharedPointer<audio::SamplesBuffer>& out,
                               const QVector<midi::MidiMessage>& midiMessages);

signals:
    void masterPeakChanged(audio::AudioPeak audioPeak);

    void postSetSampleRate(int newSampleRate);
    void postSetMasterGain(float newGain);
    void postAddTrack(QSharedPointer<audio::AudioNode> trackNode, QPrivateSignal);
    void postAddMixerTrack(QSharedPointer<audio::AudioNode> trackNode, QPrivateSignal);
    void postRemoveTrack(int trackID);
    void postRemoveAllInputTracks();
    void postSetAllLoopersStatus(bool activated);
    void postStopAllLoopers();
    void postStartNewLoopersCycle(uint samplesInCycle);
    void postSetAllTracksActivation(bool activated);
    void postSetVoiceChatStatus(int channelID, bool voiceChatActivated);
    void postSetTransmitingStatus(int channelID, bool transmiting);
    void postEnumTracks(QFutureInterface<void> futureInterface,
                        controller::AudioNodeCallback<audio::AudioNode> callback);
    void postEnumInputGroups(QFutureInterface<void> futureInterface,
                             controller::AudioNodeCallback<audio::LocalInputGroup> callback);
    void postEnumInputs(controller::AudioNodeCallback<audio::LocalInputNode> callback);
    void postEnumInputsOnPool(controller::AudioNodeCallback<audio::LocalInputNode> callback,
                              QSharedPointer<QThreadPool> threadPool);
    void postMixInputSubchannels(QFutureInterface<QList<controller::AudioChannelData>> futureInterface, int samples);
    void postReceiveProcess(QFutureInterface<void> futureInterface,
                            const QSharedPointer<audio::SamplesBuffer>& in,
                            const QSharedPointer<audio::SamplesBuffer>& out,
                            QVector<midi::MidiMessage> midiMessages);

private slots:
    void setSampleRate(int newSampleRate);
    void setMasterGain(float newGain);
    void addTrack(QSharedPointer<audio::AudioNode> trackNode);
    void addMixerTrack(QSharedPointer<audio::AudioNode> trackNode);
    void removeTrack(int trackID);
    void removeAllInputTracks();
    void removeInputTrackNode(int trackID);
    void setAllLoopersStatus(bool activated);
    void stopAllLoopers();
    void startNewLoopersCycle(uint samplesInCycle);
    void setAllTracksActivation(bool activated);
    void setVoiceChatStatus(int channelID, bool voiceChatActivated);
    void setTransmitingStatus(int channelID, bool transmiting);
    void enumTracks(QFutureInterface<void> futureInterface,
                    controller::AudioNodeCallback<audio::AudioNode> callback);
    void enumInputGroups(QFutureInterface<void> futureInterface,
                         controller::AudioNodeCallback<audio::LocalInputGroup> callback);
    void enumInputs(controller::AudioNodeCallback<audio::LocalInputNode> callback);
    void enumInputsOnPool(controller::AudioNodeCallback<audio::LocalInputNode> callback,
                          QSharedPointer<QThreadPool> threadPool);
    void mixInputSubchannelsAudio(QFutureInterface<QList<controller::AudioChannelData>> futureInterface, int samples);
    void receiveProcess(QFutureInterface<void> futureInterface,
                        const QSharedPointer<audio::SamplesBuffer>& in,
                        const QSharedPointer<audio::SamplesBuffer>& out,
                        QVector<midi::MidiMessage> midiMessages);
private:
    QScopedPointer<QThread, QScopedPointerDeleteLater> thread;
    QSharedPointer<QThreadPool> pluginsThreadPool;

    audio::AudioPeak masterPeak;
    audio::AudioMixer audioMixer;
    QMap<int, QSharedPointer<audio::AudioNode>> tracksNodes;
    QMap<int, QSharedPointer<audio::LocalInputNode>> inputTracks;
    QMap<int, QSharedPointer<audio::LocalInputGroup>> trackGroups;

    QSharedPointer<audio::LocalInputGroup> createInputTrackGroup(int groupIndex);
};

}

Q_DECLARE_METATYPE(controller::AudioChannelData)
Q_DECLARE_METATYPE(QFutureInterface<QList<controller::AudioChannelData>>)
Q_DECLARE_METATYPE(controller::AudioNodeCallback<audio::AudioNode>)
Q_DECLARE_METATYPE(controller::AudioNodeCallback<audio::LocalInputGroup>)
Q_DECLARE_METATYPE(controller::AudioNodeCallback<audio::LocalInputNode>)

#endif // AUDIOCONTROLLER_H
