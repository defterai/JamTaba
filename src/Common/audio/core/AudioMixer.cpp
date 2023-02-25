#include "AudioMixer.h"
#include "AudioNode.h"
#include <QDebug>
#include "log/Logging.h"

using audio::AudioMixer;
using audio::AudioNode;
using audio::SamplesBuffer;

AudioMixer::AudioMixer(int sampleRate) :
    discardAudioBuffer(2),
    sampleRate(sampleRate),
    masterGain(1.0f),
    applyGain(false)
{

}

void AudioMixer::addNode(const QSharedPointer<AudioNode>& node)
{
    nodes.append(node);
    // make sure node sample rate match mixer
    node->setSampleRate(sampleRate);
}

void AudioMixer::removeNode(const QSharedPointer<AudioNode>& node)
{
    nodes.removeOne(node);
}

void AudioMixer::removeAllNodes()
{
    nodes.clear();
}

AudioMixer::~AudioMixer()
{
    qCDebug(jtAudio) << "Audio mixer destructor...";

    nodes.clear();

    qCDebug(jtAudio) << "Audio mixer destructor finished!";
}

bool AudioMixer::hasSoloedNode() const
{
    for (const auto& node : qAsConst(nodes)) {
        if (node->isSoloed()) {
            return true;
        }
    }
    return false;
}

void AudioMixer::setSampleRate(int sampleRate)
{
    if (this->sampleRate != sampleRate) {
        this->sampleRate = sampleRate;
        for (const auto& node : qAsConst(nodes)) {
            node->setSampleRate(sampleRate);
        }
    }
}

void AudioMixer::setMasterGain(float masterGain)
{
    this->masterGain = masterGain;
    this->applyGain = !qFuzzyCompare(masterGain, 1.0f);
}

void AudioMixer::process(const SamplesBuffer &in, SamplesBuffer &out, const QVector<midi::MidiMessage> &midiBuffer)
{
    bool hasSoloedBuffers = hasSoloedNode();
    for (const auto& node : qAsConst(nodes)) {
        bool canProcess = (!hasSoloedBuffers && !node->isMuted()) || (hasSoloedBuffers && node->isSoloed());
        if (canProcess) {
            // each channel (not subchannel) will receive a full copy of incomming midi messages
            std::vector<midi::MidiMessage> midiMessages(midiBuffer.size());
            midiMessages.insert(midiMessages.begin(), midiBuffer.begin(), midiBuffer.end());
            node->processReplacing(in, out, midiMessages);
        } else { // just discard the samples if node is muted, the internalBuffer is not copyed to out buffer
            discardAudioBuffer.setFrameLenght(out.getFrameLenght());
            node->processReplacing(in, discardAudioBuffer, discardMidiBuffer);
            discardMidiBuffer.clear();
        }
    }

    if (applyGain) { // optimization to skip apply near 1.0 gain values
        out.applyGain(masterGain, 1.0f); // using 1 as boost factor/multiplier (no boost)
    }
}
