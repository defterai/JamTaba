#include "MidiMessage.h"

#include <QDebug>

using midi::MidiMessage;

static void registerQtMetaType() {
    static bool registered = false;
    if (!registered) {
        qRegisterMetaType<MidiMessage>();
        registered = true;
    }
}

MidiMessage::MidiMessage(qint32 data, int sourceID) :
    data(data),
    sourceID(sourceID)
{
    registerQtMetaType();
}

MidiMessage::MidiMessage() :
    MidiMessage(-1, -1)
{
    registerQtMetaType();
}

MidiMessage MidiMessage::fromVector(std::vector<unsigned char> vector, qint32 deviceIndex)
{
    int msgData = 0;
    msgData |= vector.at(0);
    msgData |= vector.at(1) << 8;
    msgData |= vector.at(2) << 16;
    return MidiMessage(msgData, deviceIndex);
}

MidiMessage MidiMessage::fromArray(const char array[4], qint32 deviceIndex)
{
    std::vector<unsigned char> vector;
    vector.assign(array, array + 4);

    return fromVector(vector, deviceIndex);
}

bool MidiMessage::transpose(qint8 semitones)
{
    if (semitones == 0 || !isNote()) {
        return true;
    }
    int noteIndex = (data >> 8) & 0xFF;
    noteIndex += semitones;
    if (noteIndex >= 0 && noteIndex <= 127) {
        data = (data & ~qint32(0xFF00)) | (quint32(noteIndex) << 8);
        return true;
    }
    return false;
}

quint8 MidiMessage::getNoteVelocity() const
{
    if (!isNote())
        return 0;

    return getData2();
}

bool MidiMessage::isNote() const
{
    // 0x80 = Note Off
    // 0x90 = Note On
    // 0x80 to 0x9F where the low nibble is the MIDI channel.

    int status = getStatus();

    return status >= 0x80 && status <= 0x9F;
}

bool MidiMessage::isNoteOn() const
{
    int status = getStatus();
    return status >= 0x90 && status <= 0x9F;
}

bool MidiMessage::isNoteOff() const
{
    int status = getStatus();
    return status >= 0x80 && status <= 0x8F;
}
