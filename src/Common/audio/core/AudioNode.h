#ifndef AUDIO_NODE_H
#define AUDIO_NODE_H

#include <QSet>
#include <QMutex>
#include "SamplesBuffer.h"
#include "AudioDriver.h"
#include "midi/MidiMessage.h"
#include <QDebug>
#include <QList>

namespace audio {

class AudioNodeProcessor;

class AudioNode : public QObject
{
    Q_OBJECT

public:
    AudioNode();
    virtual ~AudioNode();

    virtual void processReplacing(const SamplesBuffer &in, SamplesBuffer &out, int sampleRate, std::vector<midi::MidiMessage> &midiBuffer);

    virtual void setMute(bool muted, void* sender = nullptr);

    void setSolo(bool soloed, void* sender = nullptr);

    int getID() const;
    bool isMuted() const;
    bool isSoloed() const;

    virtual bool connect(AudioNode &other);
    virtual bool disconnect(AudioNode &other);

    void setGain(float gainValue, void* sender = nullptr);
    void setBoost(float boostValue, void* sender = nullptr);

    float getBoost() const;
    float getGain() const;

    void setPan(float pan, void* sender = nullptr);
    float getPan() const;

    AudioPeak getLastPeak() const;

    void resetLastPeak();

    void setRmsWindowSize(int samples);

    void deactivate();

    void activate();

    virtual bool isActivated() const;

    virtual void reset(); // reset pan, gain, boost, etc
protected:

    inline virtual void pluginsProcess(audio::SamplesBuffer &out, std::vector<midi::MidiMessage> &midiBuffer) { Q_UNUSED(out) Q_UNUSED(midiBuffer) }
    inline virtual void preFaderProcess(audio::SamplesBuffer &out){ Q_UNUSED(out) } // called after process all input and plugins, and just before compute gain, pan and boost.
    inline virtual void postFaderProcess(audio::SamplesBuffer &out){ Q_UNUSED(out) } // called after compute gain, pan and boost.

    int getInputResamplingLength(int sourceSampleRate, int targetSampleRate, int outFrameLenght);

    QSet<AudioNode *> connections;
    SamplesBuffer internalInputBuffer;
    SamplesBuffer internalOutputBuffer;

    mutable audio::AudioPeak lastPeak;
    mutable QMutex mutex; // used to protected connections manipulation because nodes can be added or removed by different threads

    // pan
    float pan;
    float leftGain;
    float rightGain;

private:
    AudioNode(const AudioNode &other);
    AudioNode &operator=(const AudioNode &other);

    int id;
    bool muted;
    bool soloed;

    bool activated; // used when room stream is played. All tracks are disabled, except the room streamer.

    float gain;
    float boost;

    static const double ROOT_2_OVER_2;
    static const double PI_OVER_2;
    static QAtomicInt LAST_FREE_ID;

    double resamplingCorrection;

    void updateGains();

signals:
    void gainChanged(float newGain, void* sender);
    void panChanged(float newPan, void* sender);
    void boostChanged(float newBoost, void* sender);
    void muteChanged(bool muteStatus, void* sender);
    void soloChanged(bool soloStatus, void* sender);
    void audioPeakChanged(audio::AudioPeak audioPeak);

    void postGain(float newGain, void* sender);
    void postPan(float newPan, void* sender);
    void postBoost(float newBoost, void* sender);
    void postMute(bool muteStatus, void* sender);
    void postSolo(bool soloStatus, void* sender);
};


inline void AudioNode::deactivate()
{
    activated = false;
}

inline void AudioNode::activate()
{
    activated = true;
}

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
