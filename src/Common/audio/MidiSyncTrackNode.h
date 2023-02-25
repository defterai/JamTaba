#ifndef MIDISYNCTRACKNODE_H
#define MIDISYNCTRACKNODE_H

#include "core/AudioNode.h"

namespace audio {

class MidiSyncTrackNode : public audio::AudioNode
{
    Q_OBJECT

public:
    explicit MidiSyncTrackNode(int sampleRate);
    ~MidiSyncTrackNode();
    void processReplacing(const SamplesBuffer &in, SamplesBuffer &out, std::vector<midi::MidiMessage> &midiBuffer) override;
    bool setSampleRate(int sampleRate) override;

signals:
    void midiClockStarted();
    void midiClockStopped();
    void midiClockPulsed();

    void postBpi(int bpi);
    void postBpm(int bpm);
    void postSetIntervalPosition(long intervalPosition);
    void postStart();
    void postStop();

private slots:
    void setBpi(int bpi);
    void setBpm(int bpm);
    void setIntervalPosition(long intervalPosition);
    void start();
    void stop();

private:
    int bpi;
    int bpm;
    long pulsesPerInterval;
    double samplesPerPulse;
    long intervalPosition;
    int currentPulse;
    int lastPlayedPulse;
    bool running;
    bool hasSentStart;

    void resetInterval();
    void updateTimimgParams();
};

} // namespace

#endif // MIDISYNCTRACKNODE_H
