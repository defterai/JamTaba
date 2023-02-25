#ifndef _AUDIO_LOOPER_STATE_
#define _AUDIO_LOOPER_STATE_

#include <QtGlobal>
#include <QMap>

#include "Looper.h"

namespace audio {

class SamplesBuffer;



class LooperState
{

public:
    enum class State {
        Stopped = 0,
        Playing,
        Waiting,
        Recording,
    };

    LooperState(State state);
    virtual ~LooperState() {}
    virtual void mixTo(Looper *looper, SamplesBuffer &samples, uint samplesToProcess) = 0;
    virtual void addBuffer(Looper *looper, const SamplesBuffer &samples, uint samplesToProcess) = 0;
    virtual void handleNewCycle(Looper *looper, uint samplesInCycle) = 0;

    inline bool isWaiting() const { return state == State::Waiting; }
    inline bool isStopped() const { return state == State::Stopped; }
    inline bool isRecording() const { return state == State::Recording; }
    inline bool isPlaying() const { return state == State::Playing; }

protected:
    State state;
};

// -------------------------------------------------------

class StoppedState : public LooperState
{
public:
    StoppedState();
    void handleNewCycle(Looper *looper, uint samplesInCycle) override;
    void mixTo(Looper *looper, SamplesBuffer &samples, uint samplesToProcess) override;
    void addBuffer(Looper *looper, const SamplesBuffer &samples, uint samplesToProcess) override;
};

// -------------------------------------------------------

class PlayingState : public LooperState
{
public:
    PlayingState();
    void handleNewCycle(Looper *looper, uint samplesInCycle) override;
    void mixTo(Looper *looper, SamplesBuffer &samples, uint samplesToProcess) override;
    void addBuffer(Looper *looper, const SamplesBuffer &samples, uint samplesToProcess) override;
};

// -------------------------------------------------------

class WaitingToRecordState : public LooperState
{
public:
    WaitingToRecordState();
    void handleNewCycle(Looper *looper, uint samplesInCycle) override;
    void addBuffer(Looper *looper, const SamplesBuffer &samples, uint samplesToProcess) override;
    void mixTo(Looper *looper, SamplesBuffer &samples, uint samplesToProcess) override;

private:
    SamplesBuffer lastInputBuffer;
};

// -------------------------------------------------------

class RecordingState : public LooperState
{
public:
    explicit RecordingState(quint8 recordingLayer);
    void handleNewCycle(Looper *looper, uint samplesInCycle) override;
    void addBuffer(Looper *looper, const SamplesBuffer &samples, uint samplesToProcess) override;
    void mixTo(Looper *looper, SamplesBuffer &samples, uint samplesToProcess) override;

private:
    const quint8 firstRecordingLayer; // used to watch when looper back to first rect layer and auto stop recording
};

// -------------------------------------------------------

} // namespace

#endif
