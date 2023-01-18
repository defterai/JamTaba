#include "LocalInputGroup.h"
#include "LocalInputNode.h"

using audio::LocalInputGroup;
using audio::LocalInputNode;
using audio::SamplesBuffer;

LocalInputGroup::LocalInputGroup(int groupIndex) :
    groupIndex(groupIndex),
    transmiting(true),
    voiceChatActivated(false)
{
}

LocalInputGroup::~LocalInputGroup()
{
    groupedInputs.clear();
}

void LocalInputGroup::addInputNode(QSharedPointer<LocalInputNode> input)
{
    groupedInputs.append(input);
}

QSharedPointer<LocalInputNode> LocalInputGroup::getInputNode(quint8 index) const
{
    if (index < groupedInputs.size()) {
        return groupedInputs.at(index);
    }

    return nullptr;
}

void LocalInputGroup::mixGroupedInputs(SamplesBuffer &out)
{
    for (auto inputTrack : groupedInputs) {
        auto lastBuffer = inputTrack->getLastBuffer();
        if (lastBuffer.getChannels() == out.getChannels()) {
            out.add(lastBuffer);
        }
        else {
            out.add(inputTrack->getLastBufferMixedToMono());
        }
    }
}

void LocalInputGroup::removeInput(QSharedPointer<LocalInputNode> input)
{
    if (!groupedInputs.removeOne(input))
        qCritical() << "the input track was not removed!";
}

int LocalInputGroup::getMaxInputChannelsForEncoding() const
{
    if (groupedInputs.size() > 1)
        return 2;    // stereo encoding

    if (!groupedInputs.isEmpty()) {
        const auto& inputNode = groupedInputs.first();
        if (inputNode->isMidi()) {
            return 2;    // just one midi track, use stereo encoding
        }
        if (inputNode->isAudio()) {
            return inputNode->getAudioInputRange().getChannels();
        }
        if (inputNode->isNoInput()) {
            return 2;    // allow channels using noInput but processing some vst looper in stereo
        }
    }
    return 0;    // no channels to encoding
}

void LocalInputGroup::setTransmitingStatus(bool transmiting)
{
    this->transmiting = transmiting;
}

void LocalInputGroup::setVoiceChatStatus(bool voiceChat)
{
    this->voiceChatActivated = voiceChat;
}

bool LocalInputGroup::isVoiceChatActivated() const
{
    return voiceChatActivated;
}
