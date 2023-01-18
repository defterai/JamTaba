#ifndef MIDISYNCTRACKNODE_H
#define MIDISYNCTRACKNODE_H

#include "core/AudioNode.h"

namespace audio {

class MidiSyncTrackNode : public audio::AudioNode
{
    Q_OBJECT

public:
    MidiSyncTrackNode();
    ~MidiSyncTrackNode();
    void processReplacing(const SamplesBuffer &in, SamplesBuffer &out, int sampleRate, std::vector<midi::MidiMessage> &midiBuffer) override;
    void setPulseTiming(long pulsesPerInterval, double samplesPerPulse);
    void setIntervalPosition(long intervalPosition);
    void resetInterval();

    void start();
    void stop();

signals:
    void midiClockStarted();
    void midiClockStopped();
    void midiClockPulsed();

private:
    long pulsesPerInterval;
    double samplesPerPulse;
    long intervalPosition;
    int currentPulse;
    int lastPlayedPulse;
    bool running;
    bool hasSentStart;
};

} // namespace

#endif // MIDISYNCTRACKNODE_H
