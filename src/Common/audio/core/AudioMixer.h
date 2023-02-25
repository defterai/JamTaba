#ifndef AUDIO_MIXER_H
#define AUDIO_MIXER_H

#include <vector>
#include <QList>
#include <QVector>
#include <QSharedPointer>
#include "audio/core/SamplesBuffer.h"

namespace midi {
class MidiMessage;
}

namespace audio {

class AudioNode;
class LocalInputNode;

class AudioMixer final
{
    Q_DISABLE_COPY_MOVE(AudioMixer);

public:
    explicit AudioMixer(int sampleRate);
    ~AudioMixer();
    void process(const SamplesBuffer &in, SamplesBuffer &out, const QVector<midi::MidiMessage> &midiBuffer);
    void addNode(const QSharedPointer<AudioNode>& node);
    void removeNode(const QSharedPointer<AudioNode>& node);
    void removeAllNodes();
    int getSampleRate() const;
    float getMasterGain() const;
    void setSampleRate(int sampleRate);
    void setMasterGain(float masterGain);

private:
    QList<QSharedPointer<AudioNode>> nodes;
    audio::SamplesBuffer discardAudioBuffer;
    std::vector<midi::MidiMessage> discardMidiBuffer;
    int sampleRate;
    float masterGain;
    bool applyGain;

    bool hasSoloedNode() const;
};

inline int AudioMixer::getSampleRate() const
{
    return sampleRate;
}

inline float AudioMixer::getMasterGain() const
{
    return masterGain;
}

} // namespace

#endif
