#ifndef _LOCAL_INPUT_NODE_H_
#define _LOCAL_INPUT_NODE_H_

#include "AudioNode.h"
#include "looper/Looper.h"
#include <array>

namespace midi {
    class MidiMessage;
}

namespace audio {

class LocalInputGroup;

enum class LocalInputMode
{
    AUDIO,
    MIDI,
    DISABLED,
};

class LocalAudioInputProps final
{
public:
    LocalAudioInputProps();
    LocalAudioInputProps(int firstChannel, int channelsCount);
    bool operator==(const LocalAudioInputProps&) const;
    void reset();
    void setChannelRange(const ChannelRange& channelRange);
    inline const ChannelRange& getChannelRange() const { return inputRange; }
    inline int getChannels() const { return inputRange.getChannels(); }
    void setStereoInversion(bool stereoInverted);
    inline bool isStereoInverted() const { return stereoInverted; }
private:
    ChannelRange inputRange;
    // store user selected input range. For example, user can choose just the
    // right input channel (index 1), or use stereo input (indexes 0 and 1), or
    // use the channels 2 and 3 (the second input pair in a multichannel audio interface)
    bool stereoInverted;
};

class MidiInputProps final
{
public:
    MidiInputProps();
    bool operator==(const MidiInputProps&) const;
    void disable();
    inline void setDevice(int index) { device = index; }
    inline int getDevice() const { return device; }
    inline void setChannel(int index) { channel = index; }
    inline int getChannel() const { return channel; }
    inline quint8 getHigherNote() const { return higherNote; }
    void setHigherNote(quint8 higherNote);
    inline quint8 getLowerNote() const { return lowerNote; }
    void setLowerNote(quint8 lowerNote);
    bool isReceivingAllMidiChannels() const;
    //void updateActivity(const midi::MidiMessage &message);
    bool accept(const midi::MidiMessage &message) const;
    inline qint8 getTranspose() const { return transpose; }
    void setTranspose(qint8 newTranspose);
    inline bool isLearning() const { return learning; }
    void setLearning(bool learning) { this->learning = learning; }
private:
    int device; // setted when user choose MIDI as input method
    int channel;
    quint8 lowerNote;
    quint8 higherNote;
    qint8 transpose;
    bool learning; // is waiting to learn a midi note?
};

class LocalInputNode : public AudioNode
{
    Q_OBJECT

public:
    LocalInputNode(int inputGroupIndex, const QSharedPointer<Looper>& looper, int sampleRate);
    ~LocalInputNode();
    void attachChannelGroup(const QSharedPointer<audio::LocalInputGroup>& group);
    void processReplacing(const SamplesBuffer &in, SamplesBuffer &out, std::vector<midi::MidiMessage> &midiBuffer) override;
    bool setSampleRate(int newSampleRate) override;

    LocalInputMode getInputMode() const;

    int getChannelGroupIndex() const;

    const audio::SamplesBuffer &getLastBuffer() const;
    SamplesBuffer getLastBufferMixedToMono() const;

    const LocalAudioInputProps& getAudioInputProps() const;
    const MidiInputProps& getMidiInputProps() const;

    qint8 getTranspose() const;

    bool isReceivingRoutedMidiInput() const;
    void setReceivingRoutedMidiInput(bool receiveRoutedMidiInput);
    bool isRoutingMidiInput() const;
    void setRoutingMidiInput(bool routeMidiInput);

    const QSharedPointer<Looper>& getLooper() const;

    template<class T>
    QList<QSharedPointer<T>> getProcessors()
    {
        QList<QSharedPointer<T>> processors;
        for (const auto& processor : getProcessors()) {
            auto p = processor.dynamicCast<T>();
            if (p) {
                processors.append(p);
            }
        }
        return processors;
    }

    void addProcessor(const QSharedPointer<AudioNodeProcessor> &newProcessor, quint32 slotIndex);
    void swapProcessors(quint32 firstSlotIndex, quint32 secondSlotIndex);
    void removeProcessor(const QSharedPointer<AudioNodeProcessor> &processor);
    void suspendProcessors();
    void resumeProcessors();
    void updateProcessorsGui();
    void closeProcessorsWindows();

    static const quint8 MAX_PROCESSORS_PER_TRACK = 4;
signals:
    void midiNoteLearned(quint8 midiNote);
    void midiActivityDetected(quint8 midiActivityValue);
    void stereoInversionChanged(bool stereoInverted, void* sender);
    void inputModeChanged(audio::LocalInputMode inputMode, void* sender);
    void audioInputPropsChanged(audio::LocalAudioInputProps audioInput, void* sender);
    void midiInputPropsChanged(audio::MidiInputProps midiInput, void* sender);

    void postSetStereoInversion(bool stereoInverted, void* sender);
    void postSetInputMode(audio::LocalInputMode inputMode, void* sender);
    void postSetAudioInputProps(audio::LocalAudioInputProps audioInputProps, void* sender);
    void postSetMidiInputProps(audio::MidiInputProps midiInputProps, void* sender);

protected:
    void reset() override;
    void pluginsProcess(audio::SamplesBuffer &in, audio::SamplesBuffer &out, std::vector<midi::MidiMessage> &midiBuffer) override;
    void preFaderProcess(audio::SamplesBuffer &out) override;
    void postFaderProcess(audio::SamplesBuffer &out) override;

private:
    using ProcessorsArray = std::array<QSharedPointer<AudioNodeProcessor>, MAX_PROCESSORS_PER_TRACK>;

    mutable QMutex mutex; // used to protected processors manipulation because nodes can be added or removed by different threads
    ProcessorsArray processors;
    LocalAudioInputProps audioInputProps;
    MidiInputProps midiInputProps;

    QSharedPointer<Looper> looper;

    QSharedPointer<audio::LocalInputGroup> inputGroup;
    int inputGroupIndex;

    bool receivingRoutedMidiInput; // true when this is the first subchannel and is receiving midi input from second subchannel (rounted midi input)? issue #102
    bool routingMidiInput; // true when this is the second channel and is sending midi messages to the first channel

    LocalInputMode inputMode = LocalInputMode::DISABLED;

    ProcessorsArray getProcessors() const;

    bool canProcessMidiMessage(const midi::MidiMessage &msg) const;

    void processIncommingMidi(std::vector<midi::MidiMessage> &inBuffer, std::vector<midi::MidiMessage> &outBuffer);
    void updateMidiActivity(const midi::MidiMessage &message);

    void setInputMode(LocalInputMode inputMode, void* sender);

    void setStereoInversion(bool stereoInverted, void* sender);
    void setAudioInputProps(LocalAudioInputProps props, void* sender);
    void setMidiInputProps(MidiInputProps props, void* sender);

    void setProcessorsSampleRate(int newSampleRate);
};

inline const QSharedPointer<Looper>& LocalInputNode::getLooper() const
{
    return looper;
}

inline bool LocalInputNode::isRoutingMidiInput() const
{
    return (inputMode == LocalInputMode::MIDI) && routingMidiInput;
}

inline bool LocalInputNode::isReceivingRoutedMidiInput() const
{
    return receivingRoutedMidiInput;
}

inline const audio::SamplesBuffer &LocalInputNode::getLastBuffer() const
{
    return internalOutputBuffer;
}

} // namespace

Q_DECLARE_METATYPE(audio::LocalInputMode)
Q_DECLARE_METATYPE(audio::LocalAudioInputProps)
Q_DECLARE_METATYPE(audio::MidiInputProps)

#endif
