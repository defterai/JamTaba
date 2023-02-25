#include "MidiSyncTrackNode.h"
#include "MetronomeUtils.h"
#include "audio/core/AudioDriver.h"
#include "audio/IntervalUtils.h"
#include <algorithm>

using audio::MidiSyncTrackNode;
using audio::SamplesBuffer;

static const int PULSES_PER_INTERVAL = 24;

MidiSyncTrackNode::MidiSyncTrackNode(int sampleRate) :
    audio::AudioNode(sampleRate),
    pulsesPerInterval(0),
    samplesPerPulse(0),
    intervalPosition(0),
    currentPulse(0),
    lastPlayedPulse(-1),
    running(false),
    hasSentStart(false)
{
    connect(this, &MidiSyncTrackNode::postBpi, this, &MidiSyncTrackNode::setBpi);
    connect(this, &MidiSyncTrackNode::postBpm, this, &MidiSyncTrackNode::setBpm);
    connect(this, &MidiSyncTrackNode::postSetIntervalPosition, this, &MidiSyncTrackNode::setIntervalPosition);
    connect(this, &MidiSyncTrackNode::postStart, this, &MidiSyncTrackNode::start);
    connect(this, &MidiSyncTrackNode::postStop, this, &MidiSyncTrackNode::stop);
}

MidiSyncTrackNode::~MidiSyncTrackNode()
{
}

void MidiSyncTrackNode::resetInterval()
{
    intervalPosition = 0;
    lastPlayedPulse = -1;
}

void MidiSyncTrackNode::setBpi(int bpi)
{
    if (this->bpi != bpi) {
        this->bpi = bpi;
        updateTimimgParams();
    }
}

void MidiSyncTrackNode::setBpm(int bpm)
{
    if (this->bpm != bpm) {
        this->bpm = bpm;
        updateTimimgParams();
    }
}

void MidiSyncTrackNode::updateTimimgParams()
{
    if (bpi <= 0 || bpm <= 0)
        return;

    long pulsesPerInterval = bpi * PULSES_PER_INTERVAL;
    long samplesPerBeat = audio::intervalUtils::getSamplesPerBeat(bpm, bpi, getSampleRate());
    double samplesPerPulse = samplesPerBeat / static_cast<double>(PULSES_PER_INTERVAL);
    if (this->pulsesPerInterval != pulsesPerInterval ||
            this->samplesPerPulse != samplesPerPulse) {
        this->pulsesPerInterval = pulsesPerInterval;
        this->samplesPerPulse = samplesPerPulse;
        resetInterval();
    }
}

void MidiSyncTrackNode::setIntervalPosition(long intervalPosition)
{
    if (samplesPerPulse <= 0)
        return;

    this->intervalPosition = intervalPosition;
    this->currentPulse = ((double)intervalPosition / samplesPerPulse);
}

void MidiSyncTrackNode::start()
{
    running = true;
}

void MidiSyncTrackNode::stop()
{
    running = false;
    hasSentStart = false;
    emit midiClockStopped();
}

void MidiSyncTrackNode::processReplacing(const SamplesBuffer &in, SamplesBuffer &out,
                                         std::vector<midi::MidiMessage> &midiBuffer)
{
    if (pulsesPerInterval <= 0 || samplesPerPulse <= 0)
        return;

    if (currentPulse == 0 && currentPulse != lastPlayedPulse) {
        if (running && !hasSentStart) {
            emit midiClockStarted();
            hasSentStart = true;
        }
//        qDebug() << "Pulses played in interval: " << lastPlayedPulse;
        lastPlayedPulse = -1;
    }

    while (currentPulse < pulsesPerInterval && currentPulse - lastPlayedPulse >= 1) {
        emit midiClockPulsed();
        lastPlayedPulse++;
    }
    AudioNode::processReplacing(in, out, midiBuffer);
}

bool MidiSyncTrackNode::setSampleRate(int sampleRate)
{
    if (AudioNode::setSampleRate(sampleRate)) {
        updateTimimgParams();
        return true;
    }
    return false;
}
