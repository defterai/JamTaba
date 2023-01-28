#include "LocalInputNode.h"
#include "audio/core/AudioNodeProcessor.h"
#include "audio/core/LocalInputGroup.h"
#include "midi/MidiMessage.h"

#include <QDateTime>

using audio::LocalInputNode;
using audio::Looper;
using audio::SamplesBuffer;
using audio::LocalAudioInputProps;
using audio::MidiInputProps;

static const quint8 FIRST_SUBCHANNEL = 0;
static const quint8 SECOND_SUBCHANNEL = 1;
static const int MAX_SUB_CHANNELS = 2;

LocalAudioInputProps::LocalAudioInputProps() :
    stereoInverted(false)
{
    qRegisterMetaType<LocalAudioInputProps>();
}

LocalAudioInputProps::LocalAudioInputProps(int firstChannel, int channelsCount)
    : inputRange(firstChannel, channelsCount),
      stereoInverted(false)
{

}

bool LocalAudioInputProps::operator==(const LocalAudioInputProps& rhs) const
{
    return inputRange == rhs.inputRange && stereoInverted == rhs.stereoInverted;
}

void LocalAudioInputProps::reset()
{
    this->stereoInverted = false;
}

void LocalAudioInputProps::setChannelRange(const ChannelRange& channelRange)
{
    this->inputRange = channelRange;
}

void LocalAudioInputProps::setStereoInversion(bool inverted)
{
    this->stereoInverted = inverted;
}

MidiInputProps::MidiInputProps() :
    device(-1),
    channel(-1),
    lowerNote(0),
    higherNote(127),
    transpose(0),
    learning(false)
{
    qRegisterMetaType<MidiInputProps>();
}

bool MidiInputProps::operator==(const MidiInputProps& rhs) const
{
    return device == rhs.device &&
            channel == rhs.channel &&
            lowerNote == rhs.lowerNote &&
            higherNote == rhs.higherNote &&
            transpose == rhs.transpose &&
            learning == rhs.learning;
}

void MidiInputProps::disable()
{
    device = -1;
}

void MidiInputProps::setHigherNote(quint8 note)
{
    higherNote = qMin(note, (quint8)127);

    if (higherNote < lowerNote)
        lowerNote = higherNote;
}

void MidiInputProps::setLowerNote(quint8 note)
{
    lowerNote = qMin(note, (quint8)127);

    if (lowerNote > higherNote)
        higherNote = lowerNote;
}

bool MidiInputProps::isReceivingAllMidiChannels() const
{
    return channel < 0 || channel > 16;
}

void MidiInputProps::setTranspose(qint8 newTranspose)
{
    this->transpose = newTranspose;
}

bool MidiInputProps::accept(const midi::MidiMessage &message) const
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
    lastMidiActivity(0),
    inputGroup(inputGroup),
    receivingRoutedMidiInput(false),
    routingMidiInput(false),
    inputMode(LocalInputMode::DISABLED),
    looper(looper)
{
    QObject::connect(this, &LocalInputNode::postSetInputMode, this, &LocalInputNode::setInputMode);
    QObject::connect(this, &LocalInputNode::postSetStereoInversion, this, &LocalInputNode::setStereoInversion);
    QObject::connect(this, &LocalInputNode::postSetAudioInputProps, this, &LocalInputNode::setAudioInputProps);
    QObject::connect(this, &LocalInputNode::postSetMidiInputProps, this, &LocalInputNode::setMidiInputProps);
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

audio::LocalInputMode LocalInputNode::getInputMode() const
{
    return inputMode;
}

void LocalInputNode::setProcessorsSampleRate(int newSampleRate)
{
    for (uint i = 0; i < processors.size(); ++i) {
        if (processors[i]) {
            processors[i]->setSampleRate(newSampleRate);
        }
    }
}

void LocalInputNode::closeProcessorsWindows()
{
    for (const auto& processor : getProcessors()) {
        if (processor) {
            processor->closeEditor();
        }
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

void LocalInputNode::setInputMode(LocalInputMode inputMode, void* sender)
{
    if (this->inputMode != inputMode) {
        this->inputMode = inputMode;
        emit inputModeChanged(inputMode, sender);
    }
}

void LocalInputNode::setStereoInversion(bool stereoInverted, void* sender)
{
    if (this->audioInputProps.isStereoInverted() != stereoInverted) {
        this->audioInputProps.setStereoInversion(stereoInverted);
        emit stereoInversionChanged(stereoInverted, sender);
        emit audioInputPropsChanged(this->audioInputProps, sender);
    }
}

void LocalInputNode::setAudioInputProps(LocalAudioInputProps props, void* sender)
{
    if (this->audioInputProps == props) return;
    bool prevStereoInversion = this->audioInputProps.isStereoInverted();
    this->audioInputProps = props;
    if (props.getChannelRange().isMono())
        internalInputBuffer.setToMono();
    else
        internalInputBuffer.setToStereo();
    if (prevStereoInversion != this->audioInputProps.isStereoInverted()) {
        emit stereoInversionChanged(this->audioInputProps.isStereoInverted(), sender);
    }
    emit audioInputPropsChanged(this->audioInputProps, sender);
}

void LocalInputNode::setMidiInputProps(MidiInputProps props, void* sender)
{
    if (this->midiInputProps == props) return;
    this->midiInputProps = props;
    emit midiInputPropsChanged(midiInputProps, sender);
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


    if (inputMode == audio::LocalInputMode::AUDIO) { // using audio input
        const auto& audioInputRange = audioInputProps.getChannelRange();
        if (audioInputRange.isEmpty()) {
            return;
        }
        internalInputBuffer.set(in, audioInputRange.getFirstChannel(), audioInputRange.getChannels());
    } else if (inputMode == LocalInputMode::MIDI && !midiBuffer.empty()) {
        processIncommingMidi(midiBuffer, filteredMidiBuffer);
    }

    if (receivingRoutedMidiInput && !midiBuffer.empty()) { // vocoders, for example, can receive midi input from second subchannel
        auto secondSubchannel = inputGroup->getInputNode(SECOND_SUBCHANNEL);
        if (secondSubchannel && secondSubchannel->getInputMode() == LocalInputMode::MIDI) {
            secondSubchannel->processIncommingMidi(midiBuffer, filteredMidiBuffer);
        }
    }

    if (isRoutingMidiInput()) {
        lastPeak.update(AudioPeak()); // ensure the audio meters will be ZERO

        return; // when routing midi this track will not render midi data, this data will be rendered by first subchannel. But the midi data is processed above to update MIDI activity meter
    }

    AudioNode::processReplacing(in, out, sampleRate, filteredMidiBuffer); // only the filtered midi messages are sended to rendering code
}

void LocalInputNode::updateMidiActivity(const midi::MidiMessage &message)
{
    if (message.isNoteOn() || message.isControl()) {
        quint8 activityValue = message.getData2();
        if (activityValue > lastMidiActivity)
            lastMidiActivity = activityValue;
    }
}

void LocalInputNode::setRoutingMidiInput(bool routeMidiInput)
{
    auto firstSubchannel = inputGroup->getInputNode(FIRST_SUBCHANNEL);
    if (firstSubchannel == this)
        return; // midi routing is not allowed in first subchannel

    if (firstSubchannel) {
        routingMidiInput = (inputMode == LocalInputMode::MIDI) && routeMidiInput;

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
        if (midiInputProps.isLearning()) {
            if (message.isNote() || message.isControl()) {
                quint8 midiNote = (quint8)message.getData1();
                emit midiNoteLearned(midiNote);
            }
            ++iterator; // when learning all messages are bypassed
        } else if (canProcessMidiMessage(message) &&
                   message.transpose(getTranspose())) {
            outBuffer.push_back(message);
            // save the midi activity peak value for notes or controls
            updateMidiActivity(message);
            iterator = inBuffer.erase(iterator);
        } else {
            ++iterator;
        }
    }
}

const LocalAudioInputProps& LocalInputNode::getAudioInputProps() const
{
    return audioInputProps;
}

const MidiInputProps& LocalInputNode::getMidiInputProps() const
{
    return midiInputProps;
}

qint8 LocalInputNode::getTranspose() const
{
    if (!receivingRoutedMidiInput) {
        return midiInputProps.getTranspose();
    }
    auto secondSubchannel = inputGroup->getInputNode(SECOND_SUBCHANNEL);
    if (secondSubchannel && secondSubchannel->getInputMode() == LocalInputMode::MIDI) {
        return secondSubchannel->midiInputProps.getTranspose();
    }
    return 0;
}

void LocalInputNode::pluginsProcess(audio::SamplesBuffer &out, std::vector<midi::MidiMessage> &midiBuffer)
{
    static SamplesBuffer tempInputBuffer(2);

    for (const auto& processor : getProcessors()) {
        if (processor && !processor->isBypassed()) {
            tempInputBuffer.setFrameLenght(internalOutputBuffer.getFrameLenght());
            tempInputBuffer.set(internalOutputBuffer); // the output from previous plugin is used as input to the next plugin in the chain

            processor->process(tempInputBuffer, internalOutputBuffer, midiBuffer);

            // some plugins are blocking the midi messages. If a VSTi can't generate messages the previous messages list will be sended for the next plugin in the chain. The messages list is cleared only when the plugin can generate midi messages.
            if (processor->isVirtualInstrument() && processor->canGenerateMidiMessages())
                midiBuffer.clear(); // only the fresh messages will be passed by the next plugin in the chain


            auto pulledMessages = processor->pullGeneratedMidiMessages();
            midiBuffer.insert(midiBuffer.end(), pulledMessages.begin(), pulledMessages.end());
        }
    }
}

void LocalInputNode::preFaderProcess(SamplesBuffer &out) // this function is called by the base class AudioNode when processing audio. It's the TemplateMethod design pattern idea.
{
    looper->addBuffer(out); // rec incoming samples before apply local track gain, pan and boost
    if (audioInputProps.isStereoInverted()) {
        out.invertStereo();
    }
}

void LocalInputNode::postFaderProcess(SamplesBuffer &out)
{
    looper->mixToBuffer(out);
}

bool LocalInputNode::canProcessMidiMessage(const midi::MidiMessage &message) const
{
    return midiInputProps.accept(message);
}

int LocalInputNode::getChannelGroupIndex() const
{
    return inputGroup->getIndex();
}

void LocalInputNode::reset()
{
    AudioNode::reset();
    audioInputProps.reset();
    emit audioInputPropsChanged(audioInputProps, nullptr);
}

LocalInputNode::ProcessorsArray LocalInputNode::getProcessors() const
{
    QMutexLocker locker(&mutex);
    return processors;
}

void LocalInputNode::addProcessor(const QSharedPointer<AudioNodeProcessor> &newProcessor, quint32 slotIndex)
{
    assert(newProcessor);
    assert(slotIndex < processors.size());
    QMutexLocker locker(&mutex);
    processors[slotIndex] = newProcessor;
}

void LocalInputNode::swapProcessors(quint32 firstSlotIndex, quint32 secondSlotIndex)
{
    assert(firstSlotIndex < processors.size());
    assert(secondSlotIndex < processors.size());
    if (firstSlotIndex != secondSlotIndex) {
        QMutexLocker locker(&mutex);
        qSwap(processors[firstSlotIndex], processors[secondSlotIndex]);
    }
}

void LocalInputNode::removeProcessor(const QSharedPointer<AudioNodeProcessor> &processor)
{
    assert(processor);
    processor->suspend();
    QMutexLocker locker(&mutex);
    for (uint i = 0; i < processors.size(); ++i) {
        if (processors[i] == processor){
            processors[i] = nullptr;
            break;
        }
    }
}

void LocalInputNode::suspendProcessors()
{
    for (const auto& processor : getProcessors()) {
        if (processor) {
            processor->suspend();
        }
    }
}

void LocalInputNode::updateProcessorsGui()
{
    for (const auto& processor : getProcessors()) {
        if (processor) {
            processor->updateGui();
        }
    }
}

void LocalInputNode::resumeProcessors()
{
    for (const auto& processor : getProcessors()) {
        if (processor) {
            processor->resume();
        }
    }
}
