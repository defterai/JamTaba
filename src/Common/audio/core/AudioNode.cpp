#include "AudioNode.h"
#include "AudioDriver.h"
#include "SamplesBuffer.h"
#include "AudioNodeProcessor.h"
#include "AudioPeak.h"
#include <cmath>
#include <cassert>
#include <QDebug>
#include "midi/MidiDriver.h"
#include <QMutexLocker>

#include "audio/Resampler.h"

using audio::AudioNode;
using audio::SamplesBuffer;
using audio::AudioPeak;
using audio::AudioNodeProcessor;

const double AudioNode::ROOT_2_OVER_2 = 1.414213562373095 * 0.5;
const double AudioNode::PI_OVER_2 = 3.141592653589793238463 * 0.5;

QAtomicInt AudioNode::LAST_FREE_ID = 1;

void AudioNode::processReplacing(const SamplesBuffer &in, SamplesBuffer &out, int sampleRate, std::vector<midi::MidiMessage> &midiBuffer)
{
    Q_UNUSED(in);

    if (!isActivated())
        return;

    internalInputBuffer.setFrameLenght(out.getFrameLenght());
    internalOutputBuffer.setFrameLenght(out.getFrameLenght());

    {
        QMutexLocker locker(&mutex);
        for (auto node : qAsConst(connections)) { // ask connected nodes to generate audio
            node->processReplacing(internalInputBuffer, internalOutputBuffer, sampleRate, midiBuffer);
        }
    }

    internalOutputBuffer.set(internalInputBuffer); // if we have no plugins inserted the input samples are just copied  to output buffer.

    // process inserted plugins
    pluginsProcess(internalOutputBuffer, midiBuffer);

    preFaderProcess(internalOutputBuffer); //call overrided preFaderProcess in subclasses to allow some preFader process.

    internalOutputBuffer.applyGain(gain, leftGain, rightGain, boost);

    lastPeak.update(internalOutputBuffer.computePeak());

    postFaderProcess(internalOutputBuffer);

    out.add(internalOutputBuffer);

    emit audioPeakChanged(lastPeak);
}

void AudioNode::setRmsWindowSize(int samples)
{
    internalOutputBuffer.setRmsWindowSize(samples);
}

AudioNode::AudioNode() :
    internalInputBuffer(2),
    internalOutputBuffer(2),
    lastPeak(),
    pan(0),
    leftGain(1.0),
    rightGain(1.0),
    id(LAST_FREE_ID++),
    muted(false),
    soloed(false),
    activated(true),
    gain(1),
    boost(1),
    resamplingCorrection(0)
{
    QObject::connect(this, &AudioNode::postGain, this, &AudioNode::setGain);
    QObject::connect(this, &AudioNode::postPan, this, &AudioNode::setPan);
    QObject::connect(this, &AudioNode::postBoost, this, &AudioNode::setBoost);
    QObject::connect(this, &AudioNode::postMute, this, &AudioNode::setMute);
    QObject::connect(this, &AudioNode::postSolo, this, &AudioNode::setSolo);
}

AudioNode::~AudioNode()
{

}

int AudioNode::getInputResamplingLength(int sourceSampleRate, int targetSampleRate, int outFrameLenght)
{
    double doubleValue = static_cast<double>(sourceSampleRate) * static_cast<double>(outFrameLenght) / static_cast<double>(targetSampleRate);
    int intValue = static_cast<int>(doubleValue);
    resamplingCorrection += doubleValue - intValue;
    if (qAbs(resamplingCorrection) > 1) {
        intValue += resamplingCorrection;
        if (resamplingCorrection > 0)
            resamplingCorrection--;
        else
            resamplingCorrection++;
    }

    return intValue;
}

AudioPeak AudioNode::getLastPeak() const
{
    return this->lastPeak;
}

void AudioNode::resetLastPeak()
{
    lastPeak.zero();
    emit audioPeakChanged(lastPeak);
}

void AudioNode::setPan(float pan, void* sender)
{
    if (pan < -1) {
        pan = -1;
    } else if (pan > 1) {
        pan = 1;
    }
    if (!qFuzzyCompare(this->pan, pan)) {
        this->pan = pan;
        updateGains();
        emit panChanged(pan, sender);
    }
}

void AudioNode::setGain(float gainValue, void* sender)
{
    if (!qFuzzyCompare(this->gain, gainValue)) {
        this->gain = gainValue;
        emit gainChanged(gainValue, sender);
    }
}

void AudioNode::setBoost(float boostValue, void* sender)
{
    if (!qFuzzyCompare(this->boost, boostValue)) {
        this->boost = boostValue;
        emit boostChanged(boostValue, sender);
    }
}

void AudioNode::setMute(bool muteStatus, void* sender)
{
    if (this->muted != muteStatus) {
        this->muted = muteStatus;
        emit muteChanged(muteStatus, sender);
    }
}

void AudioNode::setSolo(bool soloed, void* sender)
{
    if (this->soloed != soloed) {
        this->soloed = soloed;
        emit soloChanged(soloed, sender);
    }
}

void AudioNode::reset()
{
    setGain(1.0);
    setPan(0);
    setBoost(1.0);
    setMute(false);
    setSolo(false);
}

void AudioNode::updateGains()
{
    double angle = pan * PI_OVER_2 * 0.5;
    leftGain = (float)(ROOT_2_OVER_2 * (cos(angle) - sin(angle)));
    rightGain = (float)(ROOT_2_OVER_2 * (cos(angle) + sin(angle)));
}

bool AudioNode::connect(AudioNode &other)
{
    if (&other != this) {
        QMutexLocker(&(other.mutex));
        other.connections.insert(this);
        return true;
    }
    return false;
}

bool AudioNode::disconnect(AudioNode &other)
{
    if (&other != this) {
        QMutexLocker(&(other.mutex));
        other.connections.remove(this);
        return true;
    }
    return false;
}
