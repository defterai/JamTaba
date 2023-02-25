#include "AudioDriver.h"
#include "SamplesBuffer.h"
#include <vector>
#include <QDebug>
#include <cmath>
#include <QMutexLocker>
#include "log/Logging.h"

using audio::ChannelRange;
using audio::AudioDriver;

ChannelRange::ChannelRange(int firstChannel, int channelsCount) :
    firstChannel(firstChannel),
    channelsCount(channelsCount)
{
    if (firstChannel < 0 || channelsCount < 0) {
        this->firstChannel = -1;
        this->channelsCount = 0;
    }
}

ChannelRange::ChannelRange() :
    firstChannel(-1),
    channelsCount(0)
{

}

bool ChannelRange::operator==(const ChannelRange& rhs) const
{
    return firstChannel == rhs.firstChannel && channelsCount == rhs.channelsCount;
}

void ChannelRange::setToStereo()
{
    this->channelsCount = 2;
}

void ChannelRange::setToMono()
{
    this->channelsCount = 1;
}

// +++++++++++++++++++++++++++++++++++++++++++
AudioDriver::AudioDriver() :
    globalInputRange(0, 0),
    globalOutputRange(0, 0),
    audioInputDeviceIndex(-1),
    audioOutputDeviceIndex(-1),
    sampleRate(44100),
    bufferSize(128),
    inputBuffer(QSharedPointer<SamplesBuffer>::create(2)),
    outputBuffer(QSharedPointer<SamplesBuffer>::create(2))
{

}

void AudioDriver::recreateBuffers()
{
    inputBuffer = QSharedPointer<SamplesBuffer>::create(globalInputRange.getChannels());
    outputBuffer = QSharedPointer<SamplesBuffer>::create(globalOutputRange.getChannels());
}

AudioDriver::~AudioDriver()
{
    qCDebug(jtAudio) << "AudioDriver destructor.";
}

void AudioDriver::setProperties(int firstIn, int lastIn, int firstOut, int lastOut)
{
    stop();
    this->globalInputRange = ChannelRange(firstIn, (lastIn - firstIn) + 1);
    this->globalOutputRange = ChannelRange(firstOut, (lastOut - firstOut) + 1);
}

void AudioDriver::setSampleRate(int newSampleRate)
{
    if (sampleRate != newSampleRate) {
        sampleRate = newSampleRate;
        emit sampleRateChanged(newSampleRate);
    }
}

void AudioDriver::setBufferSize(int newBufferSize)
{
    bufferSize = newBufferSize;
}
