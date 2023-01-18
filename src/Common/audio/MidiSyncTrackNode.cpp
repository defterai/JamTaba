#include "MidiSyncTrackNode.h"
#include "MetronomeUtils.h"
#include "audio/core/AudioDriver.h"
#include <algorithm>

using audio::MidiSyncTrackNode;
using audio::SamplesBuffer;

using namespace controller;

MidiSyncTrackNode::MidiSyncTrackNode() :
    pulsesPerInterval(0),
    samplesPerPulse(0),
    intervalPosition(0),
    currentPulse(0),
    lastPlayedPulse(-1),
    running(false),
    hasSentStart(false)
{
    resetInterval();
}

MidiSyncTrackNode::~MidiSyncTrackNode()
{
}

void MidiSyncTrackNode::setPulseTiming(long pulsesPerInterval, double samplesPerPulse)
{
    if (pulsesPerInterval <= 0 || samplesPerPulse <= 0)
        qCritical() << "invalid sync timing params";

    this->pulsesPerInterval = pulsesPerInterval;
    this->samplesPerPulse = samplesPerPulse;
    resetInterval();
}

void MidiSyncTrackNode::resetInterval()
{
    intervalPosition = 0;
    lastPlayedPulse = -1;
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
                                          int SampleRate, std::vector<midi::MidiMessage> &midiBuffer)
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
    AudioNode::processReplacing(in, out, SampleRate, midiBuffer);
}
