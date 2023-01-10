#include "AudioSettings.h"
#include "log/Logging.h"
#include "audio/vorbis/Vorbis.h"

using namespace persistence;

static_assert(AudioSettings::DEFAULT_SAMPLE_RATE >= AudioSettings::MIN_SAMPLE_RATE &&
    AudioSettings::DEFAULT_SAMPLE_RATE <= AudioSettings::MAX_SAMPLE_RATE, "Wrong AudioSettings::DEFAULT_SAMPLE_RATE constant value");
static_assert(AudioSettings::MIN_SAMPLE_RATE > 0, "Wrong AudioSettings::MIN_SAMPLE_RATE constant value");
static_assert(AudioSettings::MAX_SAMPLE_RATE >= AudioSettings::MIN_SAMPLE_RATE, "Wrong AudioSettings::MAX_SAMPLE_RATE constant value");

static_assert(AudioSettings::DEFAULT_BUFFER_SIZE >= AudioSettings::MIN_BUFFER_SIZE &&
    AudioSettings::DEFAULT_BUFFER_SIZE <= AudioSettings::MAX_BUFFER_SIZE, "Wrong AudioSettings::DEFAULT_BUFFER_SIZE constant value");
static_assert(AudioSettings::MIN_BUFFER_SIZE > 0, "Wrong AudioSettings::MIN_BUFFER_SIZE constant value");
static_assert(AudioSettings::MAX_BUFFER_SIZE >= AudioSettings::MIN_BUFFER_SIZE, "Wrong AudioSettings::MAX_BUFFER_SIZE constant value");

AudioSettings::AudioSettings() :
    SettingsObject("audio"),
    sampleRate(DEFAULT_SAMPLE_RATE),
    bufferSize(128),
    firstIn(-1),
    firstOut(-1),
    lastIn(-1),
    lastOut(-1),
    audioInputDevice(""),
    audioOutputDevice(""),
    encodingQuality(vorbis::EncoderQualityNormal)
{
    qCDebug(jtSettings) << "AudioSettings ctor";
}

void AudioSettings::read(const QJsonObject &in)
{
    setSampleRate(getValueFromJson(in, "sampleRate", DEFAULT_SAMPLE_RATE));
    setBufferSize(getValueFromJson(in, "bufferSize", 128));
    setFirstInputIndex(getValueFromJson(in, "firstIn", 0));
    setFirstOutputIndex(getValueFromJson(in, "firstOut", 0));
    setLastInputIndex(getValueFromJson(in, "lastIn", 0));
    setLastOutputIndex(getValueFromJson(in, "lastOut", 0));
    setInputDevice(getValueFromJson(in, "audioInputDevice", QString("")));
    setOutputDevice(getValueFromJson(in, "audioOutputDevice", QString("")));
    // using normal quality as fallback value.
    setEncodingQuality(getValueFromJson(in, "encodingQuality", vorbis::EncoderQualityNormal));

    qCDebug(jtSettings) << "AudioSettings: sampleRate " << sampleRate
                        << "; bufferSize " << bufferSize
                        << "; firstIn " << firstIn
                        << "; firstOut " << firstOut
                        << "; lastIn " << lastIn
                        << "; lastOut " << lastOut
                        << "; audioInputDevice " << audioInputDevice
                        << "; audioOutputDevice " << audioOutputDevice
                        << "; encodingQuality " << encodingQuality;
}

void AudioSettings::write(QJsonObject &out) const
{
    qCDebug(jtSettings) << "AudioSettings write";
    out["sampleRate"] = sampleRate;
    out["bufferSize"] = bufferSize;
    out["firstIn"] = firstIn;
    out["firstOut"] = firstOut;
    out["lastIn"] = lastIn;
    out["lastOut"] = lastOut;

    out["audioInputDevice"] = audioInputDevice;
    out["audioOutputDevice"] = audioOutputDevice;

    out["encodingQuality"] = encodingQuality;
}

void AudioSettings::setSampleRate(int value) {
    if (value < MIN_SAMPLE_RATE)
        sampleRate = MIN_SAMPLE_RATE;
    else if (value > MAX_SAMPLE_RATE)
        sampleRate = MAX_SAMPLE_RATE;
    else
        sampleRate = value;
}

void AudioSettings::setBufferSize(int value) {
    if (value < MIN_BUFFER_SIZE)
        bufferSize = MIN_BUFFER_SIZE;
    else if (value > MAX_BUFFER_SIZE)
        bufferSize = MAX_BUFFER_SIZE;
    else
        bufferSize = value;
}

void AudioSettings::setEncodingQuality(float value) {
    // ensure vorbis quality is in accepted range
    if (value < vorbis::EncoderQualityLow)
        encodingQuality = vorbis::EncoderQualityLow;
    else if (value > vorbis::EncoderQualityHigh)
        encodingQuality = vorbis::EncoderQualityHigh;
    else
        encodingQuality = value;
}
