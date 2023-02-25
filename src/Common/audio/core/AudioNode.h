#ifndef AUDIO_NODE_H
#define AUDIO_NODE_H

#include <QSet>
#include <QMutex>
#include "SamplesBuffer.h"
#include "AudioDriver.h"
#include "midi/MidiMessage.h"
#include "Helpers.h"
#include <QDebug>
#include <QList>

namespace audio {

class AudioNodeProcessor;

class AudioNode : public TaskObject
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(AudioNode)

public:
    explicit AudioNode(int sampleRate);
    virtual ~AudioNode();
    virtual void processReplacing(const SamplesBuffer &in, SamplesBuffer &out, std::vector<midi::MidiMessage> &midiBuffer);
    virtual bool setSampleRate(int sampleRate);

    int getID() const;
    int getSampleRate() const;
    bool isActivated() const;
    bool isMuted() const;
    bool isSoloed() const;
    float getBoost() const;
    float getGain() const;
    float getPan() const;
    AudioPeak getLastPeak() const;

    void setMute(bool muted, void* sender = nullptr);
    void setSolo(bool soloed, void* sender = nullptr);
    void setGain(float gainValue, void* sender = nullptr);
    void setBoost(float boostValue, void* sender = nullptr);
    void setPan(float pan, void* sender = nullptr);

public slots:
    void setActivated(bool activated);

protected:
    inline virtual void pluginsProcess(audio::SamplesBuffer &in, audio::SamplesBuffer &out, std::vector<midi::MidiMessage> &midiBuffer) { Q_UNUSED(midiBuffer) out.set(in); }
    inline virtual void preFaderProcess(audio::SamplesBuffer &out){ Q_UNUSED(out) } // called after process all input and plugins, and just before compute gain, pan and boost.
    inline virtual void postFaderProcess(audio::SamplesBuffer &out){ Q_UNUSED(out) } // called after compute gain, pan and boost.

    int getInputResamplingLength(int sourceSampleRate, int targetSampleRate, int outFrameLenght);

    SamplesBuffer internalInputBuffer;
    SamplesBuffer internalOutputBuffer;

    mutable audio::AudioPeak lastPeak;

    // pan
    float pan;
    float leftGain;
    float rightGain;

signals:
    void gainChanged(float newGain, void* sender);
    void panChanged(float newPan, void* sender);
    void boostChanged(float newBoost, void* sender);
    void muteChanged(bool muteStatus, void* sender);
    void soloChanged(bool soloStatus, void* sender);
    void audioPeakChanged(const audio::AudioPeak& audioPeak);

    void postSetActivated(bool activated);
    void postReset();
    void postGain(float newGain, void* sender);
    void postPan(float newPan, void* sender);
    void postBoost(float newBoost, void* sender);
    void postMute(bool muteStatus, void* sender);
    void postSolo(bool soloStatus, void* sender);
    void postResetLastPeak();

protected slots:
    virtual void reset(); // reset pan, gain, boost, etc
    void resetLastPeak();

private:
    int id;
    int sampleRate;
    bool muted;
    bool soloed;

    bool activated; // used when room stream is played. All tracks are disabled, except the room streamer.

    float gain;
    float boost;

    static const double ROOT_2_OVER_2;
    static const double PI_OVER_2;
    static QAtomicInt LAST_FREE_ID;

    double resamplingCorrection;

    static int generateNodeId();

    void updateGains();
    void setRmsWindowSize(int samples);
};

inline bool AudioNode::isActivated() const
{
    return activated;
}

inline float AudioNode::getPan() const
{
    return pan;
}

inline float AudioNode::getBoost() const
{
    return boost;
}

inline float AudioNode::getGain() const
{
    return gain;
}

inline int AudioNode::getID() const
{
    return id;
}

inline int AudioNode::getSampleRate() const
{
    return sampleRate;
}

inline bool AudioNode::isMuted() const
{
    return muted;
}

inline bool AudioNode::isSoloed() const
{
    return soloed;
}


}//namespace

#endif
