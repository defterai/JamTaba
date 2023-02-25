#ifndef NINJAMTRACKNODE_H
#define NINJAMTRACKNODE_H

#include "core/AudioNode.h"
#include <QByteArray>
#include "SamplesBufferResampler.h"

namespace audio {
class SamplesBuffer;
class StreamBuffer;
}

enum class LowCutState : quint8
{
    Off = 0,
    Normal,
    Drastic,
};

class NinjamTrackNode final : public audio::AudioNode
{
    Q_OBJECT

public:
    enum class ChannelMode {
        Intervalic = 0,
        VoiceChat,
        Changing,   // used when waiting for the next interval do change the mode. Nothing is played in this 'transition state mode'
    };

    explicit NinjamTrackNode(int sampleRate);
    virtual ~NinjamTrackNode();
    void processReplacing(const audio::SamplesBuffer &in, audio::SamplesBuffer &out,
                          std::vector<midi::MidiMessage> &midiBuffer) override;
    int getDecoderSampleRate() const;

    LowCutState getLowCutState() const;

    bool isReceiveState() const;

    bool isPlaying();

    bool isStereo() const;

    bool isIntervalic() const { return mode == ChannelMode::Intervalic; }

    bool isVoiceChat() const { return mode == ChannelMode::VoiceChat; }

    void scheduleSetChannelMode(ChannelMode mode);

signals:
    void xmitStateChanged(bool transmiting);
    void lowCutStateChanged(LowCutState newState);

    void postVorbisEncodedInterval(const QSharedPointer<QByteArray>& fullIntervalBytes);
    void postVorbisEncodedChunk(const QSharedPointer<QByteArray>& chunkBytes, bool isFirstPart, bool isLastPart);
    void postStartNewInterval();
    void postDiscardDownloadedIntervals();
    void postNextLowCutState();
    void postReceiveState(bool enabled);
    void postChannelMode(NinjamTrackNode::ChannelMode newMode);

private slots:
    void addVorbisEncodedInterval(const QSharedPointer<QByteArray>& fullIntervalBytes);
    void addVorbisEncodedChunk(const QSharedPointer<QByteArray>& chunkBytes, bool isFirstPart, bool isLastPart);
    void startNewInterval();
    void discardDownloadedIntervals();  // Discard all downloaded (but not played yet) intervals
    void setLowCutToNextState();
    void setReceiveState(bool enabled);
    void setChannelMode(NinjamTrackNode::ChannelMode newMode);

private:
    const static double LOW_CUT_NORMAL_FREQUENCY;
    const static double LOW_CUT_DRASTIC_FREQUENCY;

    class LowCutFilter;
    class IntervalDecoder;

    SamplesBufferResampler resampler;
    QScopedPointer<LowCutFilter> lowCut;
    QList<std::shared_ptr<IntervalDecoder>> decoders;
    std::shared_ptr<IntervalDecoder> currentDecoder;
    ChannelMode mode;
    bool nodeDestroying;
    bool receiveState;

    void stopDecoding();
};

Q_DECLARE_METATYPE(LowCutState)
Q_DECLARE_METATYPE(NinjamTrackNode::ChannelMode)
Q_DECLARE_METATYPE(QSharedPointer<QByteArray>)

#endif // NINJAMTRACKNODE_H
