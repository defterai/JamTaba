#include "NinjamTrackNode.h"

#include <QDataStream>
#include <QDebug>
#include <QList>
#include <QByteArray>
#include <QMutexLocker>
#include <QDateTime>
#include <QtConcurrent/QtConcurrent>

#include "audio/core/Filters.h"
#include "audio/core/AudioDriver.h"
#include "audio/vorbis/VorbisDecoder.h"


const double NinjamTrackNode::LOW_CUT_DRASTIC_FREQUENCY = 220.0; // in Hertz
const double NinjamTrackNode::LOW_CUT_NORMAL_FREQUENCY = 120.0; // in Hertz

using audio::Filter;

class NinjamTrackNode::LowCutFilter
{
public:
    explicit LowCutFilter(double sampleRate);
    void process(audio::SamplesBuffer &buffer);
    //inline bool isActivated() const { return activated; }
    inline LowCutState getState(){ return this->state; }
    void setState(LowCutState state);
private:
    LowCutState state;
    Filter leftFilter;
    Filter rightFilter;
};

NinjamTrackNode::LowCutFilter::LowCutFilter(double sampleRate) :
    state(LowCutState::Off),
    leftFilter(Filter::FilterType::HighPass, sampleRate, LOW_CUT_NORMAL_FREQUENCY, 1.0, 1.0),
    rightFilter(Filter::FilterType::HighPass, sampleRate, LOW_CUT_NORMAL_FREQUENCY, 1.0, 1.0)
{

}

void NinjamTrackNode::LowCutFilter::setState(LowCutState state)
{
    this->state = state;
    if (state != LowCutState::Off){
        double frequency = LOW_CUT_NORMAL_FREQUENCY;
        if (state == LowCutState::Drastic)
            frequency = LOW_CUT_DRASTIC_FREQUENCY;

        leftFilter.setFrequency(frequency);
        rightFilter.setFrequency(frequency);
    }
}

void NinjamTrackNode::LowCutFilter::process(audio::SamplesBuffer &buffer)
{
    if (state == LowCutState::Off)
        return;

    quint32 samples = buffer.getFrameLenght();
    leftFilter.process(buffer.getSamplesArray(0), samples);
    if (!buffer.isMono())
        rightFilter.process(buffer.getSamplesArray(1), samples);
}

//--------------------------------------------------------------------------

class NinjamTrackNode::IntervalDecoder
{
public:
    explicit IntervalDecoder(const QSharedPointer<QByteArray>& vorbisData = nullptr);
    void decode(quint32 maxSamplesToDecode);
    void addEncodedData(const QSharedPointer<QByteArray>& vorbisData);
    quint32 getDecodedSamples(audio::SamplesBuffer &outBuffer, uint samplesToDecode);
    inline int getSampleRate() const { return vorbisDecoder.getSampleRate(); }
    inline bool isStereo() const { return vorbisDecoder.isStereo(); }
    void stopDecoding();
    bool isFullyDecoded() const { return vorbisDecoder.isFinished(); }
    bool isValid() const { return vorbisDecoder.isValid(); }
private:
    vorbis::Decoder vorbisDecoder;
    audio::SamplesBuffer decodedBuffer;
    QMutex mutex;
};

NinjamTrackNode::IntervalDecoder::IntervalDecoder(const QSharedPointer<QByteArray>& vorbisData)
    :decodedBuffer(2)
{
    vorbisDecoder.setInputData(vorbisData);
}

void NinjamTrackNode::IntervalDecoder::addEncodedData(const QSharedPointer<QByteArray>& vorbisData)
{
    // this funcion is called from GUI thread
    QMutexLocker locker(&mutex);
    vorbisDecoder.addInputData(vorbisData);
}

void NinjamTrackNode::IntervalDecoder::decode(quint32 maxSamplesToDecode)
{
    QMutexLocker locker(&mutex);
    decodedBuffer.append(vorbisDecoder.decode(maxSamplesToDecode));
}

void NinjamTrackNode::IntervalDecoder::stopDecoding()
{
    // this funcion is called from GUI thread
    QMutexLocker locker(&mutex);
    vorbisDecoder.setInputData(nullptr); // empty data
}

quint32 NinjamTrackNode::IntervalDecoder::getDecodedSamples(audio::SamplesBuffer &outBuffer, uint samplesToDecode)
{
    QMutexLocker locker(&mutex);
    while (decodedBuffer.getFrameLenght() < samplesToDecode) { //need decode more samples to fill outBuffer?
        quint32 toDecode = samplesToDecode - decodedBuffer.getFrameLenght();
        const auto &decodedSamples = vorbisDecoder.decode(toDecode);
        decodedBuffer.append(decodedSamples);
        if (decodedSamples.isEmpty())
            break; //no more samples to decode
    }

    quint32 totalSamples = qMin(samplesToDecode, decodedBuffer.getFrameLenght());
    outBuffer.setFrameLenght(totalSamples);
    outBuffer.set(decodedBuffer);
    decodedBuffer.discardFirstSamples(totalSamples);

    return totalSamples;
}

//-------------------------------------------------------------

NinjamTrackNode::NinjamTrackNode(int sampleRate) :
    AudioNode(sampleRate),
    lowCut(new NinjamTrackNode::LowCutFilter(44100)),
    currentDecoder(nullptr),
    mode(ChannelMode::Intervalic),
    nodeDestroying(false),
    receiveState(true)
{
    static bool registered = false;
    if (!registered) {
        qRegisterMetaType<LowCutState>();
        qRegisterMetaType<ChannelMode>();
        qRegisterMetaType<QSharedPointer<QByteArray>>();
        registered = true;
    }

    connect(this, &NinjamTrackNode::postVorbisEncodedInterval, this, &NinjamTrackNode::addVorbisEncodedInterval);
    connect(this, &NinjamTrackNode::postVorbisEncodedChunk, this, &NinjamTrackNode::addVorbisEncodedChunk);
    connect(this, &NinjamTrackNode::postStartNewInterval, this, &NinjamTrackNode::startNewInterval);
    connect(this, &NinjamTrackNode::postDiscardDownloadedIntervals, this, &NinjamTrackNode::discardDownloadedIntervals);
    connect(this, &NinjamTrackNode::postNextLowCutState, this, &NinjamTrackNode::setLowCutToNextState);
    connect(this, &NinjamTrackNode::postReceiveState, this, &NinjamTrackNode::setReceiveState);
    connect(this, &NinjamTrackNode::postChannelMode, this, &NinjamTrackNode::setChannelMode);
}

bool NinjamTrackNode::isStereo() const
{
    auto decoder = std::atomic_load(&currentDecoder);
    if (decoder) {
        return decoder->isStereo();
    }
    return true;
}

void NinjamTrackNode::stopDecoding()
{
    auto decoder = std::atomic_load(&currentDecoder);
    if (decoder) {
        decoder->stopDecoding();
    }
    discardDownloadedIntervals();
}

void NinjamTrackNode::setLowCutToNextState()
{
    LowCutState newState;
    switch (lowCut->getState()) {
    case LowCutState::Off:
        newState = LowCutState::Normal;
        break;
    case LowCutState::Normal:
        newState = LowCutState::Drastic;
        break;
    case LowCutState::Drastic:
    default:
        newState = LowCutState::Off;
        break;
    }
    lowCut->setState(newState);
    emit lowCutStateChanged(newState);
}

LowCutState NinjamTrackNode::getLowCutState() const
{
    return lowCut->getState();
}

int NinjamTrackNode::getDecoderSampleRate() const
{
    auto decoder = std::atomic_load(&currentDecoder);
    if (decoder) {
        return decoder->getSampleRate();
    }
    return -1;
}

NinjamTrackNode::~NinjamTrackNode()
{
    //qDebug() << "Destrutor NinjamTrackNode";
    nodeDestroying = true;
    discardDownloadedIntervals();
}

void NinjamTrackNode::discardDownloadedIntervals()
{
    decoders.clear();
    std::atomic_store(&currentDecoder, {});
}

bool NinjamTrackNode::isReceiveState() const {
    return receiveState;
}

void NinjamTrackNode::setReceiveState(bool state) {
    receiveState = state;
}

bool NinjamTrackNode::isPlaying()
{
    return std::atomic_load(&currentDecoder) || mode == ChannelMode::VoiceChat;
}

void NinjamTrackNode::startNewInterval()
{
    //qDebug() << "--------START INTERVAL------------";

    bool wasPlaying = isPlaying();
    if (mode == ChannelMode::Intervalic) {
        std::shared_ptr<IntervalDecoder> decoder; //discard the previous interval decoder
        if (!decoders.isEmpty()) {
            decoder = decoders.takeFirst(); //using the next buffered decoder (next interval)
        }
        std::atomic_store(&currentDecoder, decoder);
    }
    bool nowPlaying = isPlaying();
    if (wasPlaying != nowPlaying) {
        emit xmitStateChanged(nowPlaying);
    }
}

// this function is used only for voice chat mode. The parameter is not a full Ogg Vorbis interval, it's just a chunk of data.
void NinjamTrackNode::addVorbisEncodedChunk(const QSharedPointer<QByteArray>& chunkBytes, bool isFirstPart, bool isLastPart)
{
    //qDebug() << "   Chunk received " << chunkBytes.left(4) << "\tFirst:" << isFirstPart << " Last:" << isLastPart << " Bytes received:" << chunkBytes.size();

    // track is not playing yet and receive the first interval bytes
    if (!isPlaying()) {
        emit xmitStateChanged(true);
    }

    if(mode != ChannelMode::VoiceChat)
        return;

    if (decoders.isEmpty()) {
        if (!isFirstPart) { // if decoders are empty and the chunk is not the first part we are receinving partial data of the previous interval, we must wait until receive a new interval
            //qDebug() << "Returning, not the first part of an interval";
            return;
        }
        // qDebug() << "First interval part received, creating new interval";
        decoders.push_back(std::make_shared<IntervalDecoder>());
    }

    decoders.last()->addEncodedData(chunkBytes);

    if (isLastPart) {
        //qDebug() << "Last part received, creating new IntervalDecoder";
        decoders.push_back(std::make_shared<IntervalDecoder>());
    }
}

 // this function is used only for Intervalic mode. The parameter is a full Ogg Vorbis Interval data
void NinjamTrackNode::addVorbisEncodedInterval(const QSharedPointer<QByteArray>& fullIntervalBytes)
{
    //qDebug() << "Full Interval received " << fullIntervalBytes.left(4);

    if (mode != ChannelMode::Intervalic)
        return;

    auto decoder = std::make_shared<IntervalDecoder>(fullIntervalBytes);
    decoders.append(decoder);

    //decoding the first samples in a separated thread to avoid slow down the audio thread in interval start (first beat)
    auto decoderWeakPtr = std::weak_ptr<IntervalDecoder>(decoder);
    QtConcurrent::run([](std::weak_ptr<IntervalDecoder> decoderWeakPtr) {
        auto pdecoder = decoderWeakPtr.lock();
        if (pdecoder) {
            pdecoder->decode(256);
        }
        return 0;
    }, decoderWeakPtr);
}

// ++++++++++++++

void NinjamTrackNode::setChannelMode(ChannelMode newMode)
{
    this->mode = newMode;
}

// ++++++++++++++++++++++++++++++++++++++

void NinjamTrackNode::scheduleSetChannelMode(ChannelMode mode)
{
    if (nodeDestroying) return;
    this->mode = ChannelMode::Changing; // nothing is played when is 'changing'

    emit postDiscardDownloadedIntervals();
    emit postChannelMode(mode);
}

void NinjamTrackNode::processReplacing(const audio::SamplesBuffer &in, audio::SamplesBuffer &out,
                                       std::vector<midi::MidiMessage> &midiBuffer)
{
    bool needResampling = false;
    if (isPlaying()) {
        auto decoder = std::atomic_load(&currentDecoder);
        if (!decoder) {
            if (mode == ChannelMode::VoiceChat && !decoders.isEmpty()) {
                decoder = decoders.first();
                std::atomic_store(&currentDecoder, decoder);
                //qDebug() << "USING FIRST DECODER";
            } else {
                //qDebug() << "Current decoder is null, not playing!";
                return;
            }
        }

        if (!decoder || !decoder->isValid()) {
            if (decoder && !decoder->isValid()){
                //qDebug() << "Current decoder is not valid, returning!";
                std::atomic_store(&currentDecoder, {}); // the current decoder is corrupted, setting to nullptr to force a new decoder usage
                decoders.clear();
                internalInputBuffer.zero();
            }
            return;
        }

        if (!receiveState) {
            std::atomic_store(&currentDecoder, {});
            decoder->stopDecoding();
            decoders.clear();
            internalInputBuffer.zero();
            return;
        }

        int outFrameLenght = out.getFrameLenght();
        needResampling = decoder && decoder->getSampleRate() != getSampleRate();
        auto framesToProcess = needResampling ? getInputResamplingLength(
             decoder ? decoder->getSampleRate() : 44100, getSampleRate(), outFrameLenght) : outFrameLenght;
        internalInputBuffer.setFrameLenght(framesToProcess);

        if (decoder) {
            decoder->getDecodedSamples(internalInputBuffer, framesToProcess);
            if (mode == ChannelMode::VoiceChat) { // in voice chat we will not wait until startInterval to use the next available downloaded decoder
                if (framesToProcess >= 0 && decoder->isFullyDecoded()) {
                    //qDebug() << "current decoder consumed, using the next decoder";
                    std::atomic_store(&currentDecoder, {});
                    needResampling = false;
                    if (!decoders.isEmpty()) {
                        decoders.takeFirst();
                    }
                }
            }
        }
    }

    if (!internalInputBuffer.isEmpty()) {
        if (!receiveState) {
            internalInputBuffer.zero();
            return;
        }
        if (needResampling) {
            const auto &resampledBuffer = resampler.resample(internalInputBuffer, out.getFrameLenght());
            internalInputBuffer.setFrameLenght(resampledBuffer.getFrameLenght());
            internalInputBuffer.set(resampledBuffer);
        }

        lowCut->process(internalInputBuffer);

        audio::AudioNode::processReplacing(in, out, midiBuffer); // process internal buffer pan, gain, etc
    }
}
