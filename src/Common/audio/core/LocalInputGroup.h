#ifndef _LOCAL_INPUT_GROUP_H_
#define _LOCAL_INPUT_GROUP_H_

#include <QList>
#include <QSharedPointer>

namespace audio {

class LocalInputNode;
class SamplesBuffer;

class LocalInputGroup
{

public:
    LocalInputGroup(int groupIndex);
    ~LocalInputGroup();

    bool isEmpty() const;

    void addInputNode(QSharedPointer<audio::LocalInputNode> input);

    int getIndex() const;

    void mixGroupedInputs(audio::SamplesBuffer &out);

    void removeInput(QSharedPointer<audio::LocalInputNode> input);

    int getMaxInputChannelsForEncoding() const;

    bool isTransmiting() const;

    void setTransmitingStatus(bool transmiting);

    bool isVoiceChatActivated() const;

    void setVoiceChatStatus(bool voiceChat);

    QSharedPointer<audio::LocalInputNode> getInputNode(quint8 index) const;

private:
    int groupIndex;
    QList<QSharedPointer<audio::LocalInputNode>> groupedInputs;
    bool transmiting;
    bool voiceChatActivated;
};

inline bool LocalInputGroup::isTransmiting() const
{
    return transmiting;
}

inline int LocalInputGroup::getIndex() const
{
    return groupIndex;
}

inline bool LocalInputGroup::isEmpty() const
{
    return groupedInputs.empty();
}

} //namespace

#endif
