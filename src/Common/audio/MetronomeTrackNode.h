#ifndef METRONOMETRACKNODE_H
#define METRONOMETRACKNODE_H

#include "core/AudioNode.h"
#include "persistence/MetronomeSettings.h"

#include <QSharedPointer>
#include <QFuture>

namespace audio {

class SamplesBuffer;

class MetronomeTrackNode : public audio::AudioNode
{
    Q_OBJECT
public:
    MetronomeTrackNode(const persistence::MetronomeSettings& settings, int sampleRate);
    ~MetronomeTrackNode() override;
    void processReplacing(const SamplesBuffer &in, SamplesBuffer &out, std::vector<midi::MidiMessage> &midiBuffer) override;
    bool setSampleRate(int sampleRate) override;

    bool isPlayingAccents() const;
    int getBeatsPerAccent() const; // will return zero even if isPlayingAccents() when pattern is uneven
    const QList<int>& getAccentBeats() const; // returns the beats with accents

signals:
    void postBpi(int bpi);
    void postBpm(int bpm);
    void postAccentBeats(QList<int> accents);
    void postBeatsPerAccent(int beatsPerAccent);
    void postSetIntervalPosition(long intervalPosition);
    void postChangeSound(persistence::MetronomeSoundSettings settings);

private slots:
    void setBpi(int bpi);
    void setBpm(int bpm);
    void setAccentBeats(QList<int> accents);    // pass empty list to turn off accents
    void resetInterval();
    void setBeatsPerAccent(int beatsPerAccent); // pass zero to turn off accents
    void setIntervalPosition(long intervalPosition);
    void changeSound(persistence::MetronomeSoundSettings settings);

private:
    struct AudioBuffers
    {
        SamplesBuffer firstBeat;
        SamplesBuffer offBeat;
        SamplesBuffer accentBeat;

        AudioBuffers() :
            firstBeat(2),
            offBeat(2),
            accentBeat(2)
        {
        }
    };

    QSharedPointer<AudioBuffers> audioBuffers;
    persistence::MetronomeSoundSettings soundSettings;

    int bpi;
    int bpm;

    long samplesPerBeat;
    long intervalPosition;
    long beatPosition;
    int currentBeat;
    QList<int> accentBeats;

    bool loading;
    QFuture<QSharedPointer<AudioBuffers>> loadingFuture;

    static QSharedPointer<AudioBuffers> loadSound(const persistence::MetronomeSoundSettings& settings, int sampleRate);

    void cancelLoad();
    void scheduleLoad();
    bool completeLoad();
    const SamplesBuffer& getSamplesBuffer(int beat) const; // return the correct buffer to play in each beat
    void updateSamplePerBeats();
};

inline bool MetronomeTrackNode::isPlayingAccents() const
{
    return accentBeats.length() > 0;
}

} // namespace

#endif // METRONOMETRACKNODE_H
