#include "LocalInputNode.h"
#include "audio/core/AudioNodeProcessor.h"
#include "audio/core/LocalInputGroup.h"
#include "midi/MidiMessage.h"

#include <QDateTime>

using audio::LocalInputNode;
using audio::Looper;
using audio::SamplesBuffer;

static const quint8 FIRST_SUBCHANNEL = 0;
static const quint8 SECOND_SUBCHANNEL = 1;

LocalInputNode::MidiInput::MidiInput() :
    device(-1),
    channel(-1),
    lastMidiActivity(0),
    lowerNote(0),
    higherNote(127),
    transpose(0),
    learning(false)
{
    //
}

void LocalInputNode::MidiInput::disable()
{
    device = -1;
}

void LocalInputNode::MidiInput::setHigherNote(quint8 note)
{
    higherNote = qMin(note, (quint8)127);

    if (higherNote < lowerNote)
        lowerNote = higherNote;
}

void LocalInputNode::MidiInput::setLowerNote(quint8 note)
{
    lowerNote = qMin(note, (quint8)127);

    if (lowerNote > higherNote)
        higherNote = lowerNote;
}

bool LocalInputNode::MidiInput::isReceivingAllMidiChannels() const
{
    return channel < 0 || channel > 16;
}

void LocalInputNode::MidiInput::updateActivity(const midi::MidiMessage &message)
{
    if (message.isNoteOn() || message.isControl()) {
        quint8 activityValue = message.getData2();
        if (activityValue > lastMidiActivity)
            lastMidiActivity = activityValue;
    }
}

void LocalInputNode::MidiInput::setTranspose(qint8 newTranspose)
{
    this->transpose = newTranspose;
}

bool LocalInputNode::MidiInput::accept(const midi::MidiMessage &message) const
{
    bool canAcceptDevice = message.getSourceDeviceIndex() == device;
    bool canAcceptChannel = isReceivingAllMidiChannels() || message.getChannel() == channel;
    bool canAcceptRange = true;

    if (message.isNote()) {
        int midiNote = message.getData1();
        canAcceptRange = midiNote >= lowerNote && midiNote <= higherNote;
    }

    return (canAcceptDevice && canAcceptChannel && canAcceptRange);
}

// ---------------------------------------------------------------------

LocalInputNode::LocalInputNode(const QSharedPointer<LocalInputGroup>& inputGroup, const QSharedPointer<Looper>& looper) :
    inputGroup(inputGroup),
    stereoInverted(false),
    receivingRoutedMidiInput(false),
    routingMidiInput(false),
    looper(looper)
{
    setToNoInput();
}

LocalInputNode::~LocalInputNode()
{

}

void LocalInputNode::stopLooper()
{
    looper->stop();
}

void LocalInputNode::startNewLoopCycle(uint intervalLenght)
{
    looper->startNewCycle(intervalLenght);
}

bool LocalInputNode::isMono() const
{
    return isAudio() && audioInputRange.isMono();
}

bool LocalInputNode::isStereo() const
{
    return isAudio() && audioInputRange.getChannels() == 2;
}

bool LocalInputNode::isNoInput() const
{
    return inputMode == DISABLED;
}

bool LocalInputNode::isMidi() const
{
    return inputMode == MIDI;
}

bool LocalInputNode::isAudio() const
{
    return inputMode == AUDIO;
}

void LocalInputNode::setProcessorsSampleRate(int newSampleRate)
{
    for (int i = 0; i < MAX_PROCESSORS_PER_TRACK; ++i) {
        if (processors[i])
            processors[i]->setSampleRate(newSampleRate);
    }
}

void LocalInputNode::closeProcessorsWindows()
{
    for (int i = 0; i < MAX_PROCESSORS_PER_TRACK; ++i) {
        if (processors[i])
            processors[i]->closeEditor();
    }
}

SamplesBuffer LocalInputNode::getLastBufferMixedToMono() const
{
    if (internalOutputBuffer.isMono())
        return internalOutputBuffer;

    const uint samples = internalOutputBuffer.getFrameLenght();
    SamplesBuffer lastBuffer(1, samples);
    float *samplesArray = lastBuffer.getSamplesArray(0);
    float *internalArrays[2] = {internalOutputBuffer.getSamplesArray(0), internalOutputBuffer.getSamplesArray(1)};
    for (uint s = 0; s < samples; ++s) {
        samplesArray[s] = internalArrays[0][s] * leftGain + internalArrays[1][s] * rightGain;
    }

    return lastBuffer;
}

void LocalInputNode::setAudioInputSelection(int firstChannelIndex, int channelCount)
{
    audioInputRange = ChannelRange(firstChannelIndex, channelCount);
    if (audioInputRange.isMono())
        internalInputBuffer.setToMono();
    else
        internalInputBuffer.setToStereo();

    midiInput.disable();
    inputMode = AUDIO;
}

void LocalInputNode::setToNoInput()
{
    audioInputRange = ChannelRange(-1, 0);// disable audio input
    midiInput.disable();
    inputMode = DISABLED;
}

void LocalInputNode::setMidiInputSelection(int midiDeviceIndex, int midiChannelIndex)
{
    audioInputRange   = ChannelRange(-1, 0);// disable audio input
    midiInput.device  = midiDeviceIndex;
    midiInput.channel = midiChannelIndex;
    inputMode = MIDI;
}

void LocalInputNode::setMidiHigherNote(quint8 newHigherNote)
{
    midiInput.setHigherNote(newHigherNote);
}

void LocalInputNode::setMidiLowerNote(quint8 newLowerNote)
{
    midiInput.setLowerNote(newLowerNote);
}

bool LocalInputNode::isReceivingAllMidiChannels() const
{
    if (inputMode == MIDI)
        return midiInput.isReceivingAllMidiChannels();

    return false;
}

void LocalInputNode::processReplacing(const SamplesBuffer &in, SamplesBuffer &out,
                                           int sampleRate, std::vector<midi::MidiMessage> &midiBuffer)
{
    Q_UNUSED(sampleRate);

    /**
    *   The input buffer (in) is a multichannel buffer. So, this buffer contains
    * all channels grabbed from soundcard inputs. For example, if the user selected a range of 4
    * input channels in audio preferences this buffer will contain 4 channels.
    *
    *      A LocalInputNode instance will grab only your input range from 'in'. Other
    * LocalInputNode instances can read other channel range from 'in'.
    *
    */

    std::vector<midi::MidiMessage> filteredMidiBuffer(midiBuffer.size());
    internalInputBuffer.setFrameLenght(out.getFrameLenght());
    internalOutputBuffer.setFrameLenght(out.getFrameLenght());
    internalInputBuffer.zero();
    internalOutputBuffer.zero();

    if (!isNoInput()) {
        if (isAudio()) { // using audio input
            if (audioInputRange.isEmpty())
                return;

            internalInputBuffer.set(in, audioInputRange.getFirstChannel(), audioInputRange.getChannels());
        }
        else if (isMidi() && !midiBuffer.empty()) {
            processIncommingMidi(midiBuffer, filteredMidiBuffer);
        }
    }

    if (receivingRoutedMidiInput && !midiBuffer.empty()) { // vocoders, for example, can receive midi input from second subchannel
        auto secondSubchannel = inputGroup->getInputNode(SECOND_SUBCHANNEL);
        if (secondSubchannel && secondSubchannel->isMidi()) {
            secondSubchannel->processIncommingMidi(midiBuffer, filteredMidiBuffer);
        }
    }

    if (isRoutingMidiInput()) {
        lastPeak.update(AudioPeak()); // ensure the audio meters will be ZERO

        return; // when routing midi this track will not render midi data, this data will be rendered by first subchannel. But the midi data is processed above to update MIDI activity meter
    }

    AudioNode::processReplacing(in, out, sampleRate, filteredMidiBuffer); // only the filtered midi messages are sended to rendering code
}

void LocalInputNode::setRoutingMidiInput(bool routeMidiInput)
{
    auto firstSubchannel = inputGroup->getInputNode(FIRST_SUBCHANNEL);
    if (firstSubchannel == this)
        return; // midi routing is not allowed in first subchannel

    if (firstSubchannel) {
        routingMidiInput = isMidi() && routeMidiInput;

        if (routingMidiInput)
            receivingRoutedMidiInput = false;

        firstSubchannel->setReceivingRoutedMidiInput(routingMidiInput);
    }
    else {
        qCritical() << "First subchannel is null!";
    }
}

void LocalInputNode::setReceivingRoutedMidiInput(bool receiveRoutedMidiInput)
{
    receivingRoutedMidiInput = receiveRoutedMidiInput;

    if (receivingRoutedMidiInput)
        routingMidiInput = false;
}

void LocalInputNode::processIncommingMidi(std::vector<midi::MidiMessage> &inBuffer, std::vector<midi::MidiMessage> &outBuffer)
{
    for (auto iterator = inBuffer.begin(); iterator != inBuffer.end();) {
        auto message = *iterator;
        if (midiInput.isLearning()) {
            if (message.isNote() || message.isControl()) {
                quint8 midiNote = (quint8)message.getData1();
                emit midiNoteLearned(midiNote);
            }
            ++iterator; // when learning all messages are bypassed
        } else if (canProcessMidiMessage(message) &&
                   message.transpose(getTranspose())) {
            outBuffer.push_back(message);
            // save the midi activity peak value for notes or controls
            midiInput.updateActivity(message);
            iterator = inBuffer.erase(iterator);
        } else {
            ++iterator;
        }
    }
}

qint8 LocalInputNode::getTranspose() const
{
    if (!receivingRoutedMidiInput) {
        return midiInput.transpose;
    }
    auto secondSubchannel = inputGroup->getInputNode(SECOND_SUBCHANNEL);
    if (secondSubchannel && secondSubchannel->isMidi()) {
        return secondSubchannel->midiInput.transpose;
    }
    return 0;
}

void LocalInputNode::preFaderProcess(SamplesBuffer &out) // this function is called by the base class AudioNode when processing audio. It's the TemplateMethod design pattern idea.
{
    looper->addBuffer(out); // rec incoming samples before apply local track gain, pan and boost

    if (stereoInverted)
        out.invertStereo();
}

void LocalInputNode::postFaderProcess(SamplesBuffer &out)
{
    looper->mixToBuffer(out);
}

void LocalInputNode::setStereoInversion(bool inverted)
{
    if (isMono())
        return;

    if (this->stereoInverted != inverted)
    {
        this->stereoInverted = inverted;
        emit stereoInversionChanged(inverted);
    }
}

bool LocalInputNode::isStereoInverted() const
{
    return !isMono() && stereoInverted;
}

void LocalInputNode::setTranspose(qint8 transpose)
{
    midiInput.setTranspose(transpose);
}

bool LocalInputNode::canProcessMidiMessage(const midi::MidiMessage &message) const
{
    return midiInput.accept(message);
}

int LocalInputNode::getChannelGroupIndex() const
{
    return inputGroup->getIndex();
}

void LocalInputNode::startMidiNoteLearn()
{
    midiInput.learning = true;
}

void LocalInputNode::stopMidiNoteLearn()
{
    midiInput.learning = false;
}

void LocalInputNode::reset()
{
    AudioNode::reset();

    stereoInverted = false;

    emit stereoInversionChanged(stereoInverted);
}
