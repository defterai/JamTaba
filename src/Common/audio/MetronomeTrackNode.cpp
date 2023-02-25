#include "MetronomeTrackNode.h"
#include "MetronomeUtils.h"
#include "audio/core/AudioDriver.h"
#include "audio/core/SamplesBuffer.h"
#include "audio/IntervalUtils.h"
#include "Helpers.h"
#include "Utils.h"

#include <QtConcurrent>

using audio::MetronomeTrackNode;
using audio::SamplesBuffer;

MetronomeTrackNode::MetronomeTrackNode(const persistence::MetronomeSettings& settings, int sampleRate) :
    audio::AudioNode(sampleRate),
    soundSettings(settings),
    bpi(0),
    bpm(0),
    samplesPerBeat(0),
    intervalPosition(0),
    beatPosition(0),
    currentBeat(0),
    loading(false)
{
    setMute(settings.isMuted());
    setPan(settings.getPan());
    setGain(Utils::linearGainToPower(settings.getGain()));
    scheduleLoad();

    QObject::connect(this, &MetronomeTrackNode::postBpi, this, &MetronomeTrackNode::setBpi);
    QObject::connect(this, &MetronomeTrackNode::postBpm, this, &MetronomeTrackNode::setBpm);
    QObject::connect(this, &MetronomeTrackNode::postAccentBeats, this, &MetronomeTrackNode::setAccentBeats);
    QObject::connect(this, &MetronomeTrackNode::postBeatsPerAccent, this, &MetronomeTrackNode::setBeatsPerAccent);
    QObject::connect(this, &MetronomeTrackNode::postSetIntervalPosition, this, &MetronomeTrackNode::setIntervalPosition);
    QObject::connect(this, &MetronomeTrackNode::postChangeSound, this, &MetronomeTrackNode::changeSound);
}

MetronomeTrackNode::~MetronomeTrackNode()
{
    cancelLoad();
}

void MetronomeTrackNode::setBpi(int bpi)
{
    if (this->bpi != bpi) {
        this->bpi = bpi;
        updateSamplePerBeats();
    }
}

void MetronomeTrackNode::setBpm(int bpm)
{
    if (this->bpm != bpm) {
        this->bpm = bpm;
        updateSamplePerBeats();
    }
}

void MetronomeTrackNode::setAccentBeats(QList<int> accentBeats)
{
    this->accentBeats = accentBeats;
}

const QList<int>& MetronomeTrackNode::getAccentBeats() const
{
    return accentBeats;
}

void MetronomeTrackNode::resetInterval()
{
    beatPosition = intervalPosition = 0;
}

void MetronomeTrackNode::setBeatsPerAccent(int beatsPerAccent)
{
    setAccentBeats(metronomeUtils::getAccentBeats(beatsPerAccent, bpi));
}

void MetronomeTrackNode::setIntervalPosition(long intervalPosition)
{
    if (samplesPerBeat <= 0)
        return;

    this->intervalPosition = intervalPosition;
    this->beatPosition = intervalPosition % samplesPerBeat;
    this->currentBeat = (intervalPosition / samplesPerBeat);
}

void MetronomeTrackNode::changeSound(persistence::MetronomeSoundSettings settings)
{
    if (soundSettings.isSoundChanged(settings)) {
        soundSettings = settings;
        scheduleLoad();
    }
}

QSharedPointer<MetronomeTrackNode::AudioBuffers> MetronomeTrackNode::loadSound(const persistence::MetronomeSoundSettings& settings, int sampleRate)
{
    auto buffers = createQSharedPointer<AudioBuffers>();
    if (settings.isUsingCustomSounds())
    {
        audio::metronomeUtils::createCustomSounds(settings.getCustomPrimaryBeatFile(),
                                                  settings.getCustomOffBeatFile(),
                                                  settings.getCustomAccentBeatFile(),
                                                  buffers->firstBeat,
                                                  buffers->offBeat,
                                                  buffers->accentBeat,
                                                  sampleRate);
    }
    else
    {
        audio::metronomeUtils::createBuiltInSounds(settings.getBuiltInMetronomeAlias(),
                                                   buffers->firstBeat,
                                                   buffers->offBeat,
                                                   buffers->accentBeat,
                                                   sampleRate);
    }
    audio::metronomeUtils::removeSilenceInBufferStart(buffers->firstBeat);
    audio::metronomeUtils::removeSilenceInBufferStart(buffers->offBeat);
    audio::metronomeUtils::removeSilenceInBufferStart(buffers->accentBeat);
    return buffers;
}

void MetronomeTrackNode::cancelLoad()
{
    if (loading) {
        loadingFuture.cancel();
        loading = false;
    }
}

void MetronomeTrackNode::scheduleLoad()
{
    cancelLoad();
    loadingFuture = QtConcurrent::run(&MetronomeTrackNode::loadSound,
                                      soundSettings, getSampleRate());
    loading = true;
}

bool MetronomeTrackNode::completeLoad()
{
    if (loading && loadingFuture.isFinished()) {
        audioBuffers = loadingFuture.result();
        loading = false;
    }
    return !audioBuffers.isNull();
}

const SamplesBuffer& MetronomeTrackNode::getSamplesBuffer(int beat) const
{
    if (beat == 0) {
        return audioBuffers->firstBeat;
    }
    if (accentBeats.contains(beat)) {
        return audioBuffers->accentBeat;
    }
    return audioBuffers->offBeat;
}

void MetronomeTrackNode::updateSamplePerBeats()
{
    if (bpm <= 0 || bpi <= 0) return;
    long samplesPerBeat = intervalUtils::getSamplesPerBeat(bpm, bpi, getSampleRate());
    if (this->samplesPerBeat != samplesPerBeat) {
        this->samplesPerBeat = samplesPerBeat;
        resetInterval();
    }
}

void MetronomeTrackNode::processReplacing(const SamplesBuffer &in, SamplesBuffer &out,
                                          std::vector<midi::MidiMessage> &midiBuffer)
{
    if (samplesPerBeat <= 0 || !completeLoad())
        return;

    internalInputBuffer.setFrameLenght(out.getFrameLenght());
    internalInputBuffer.zero();

    auto samplesBuffer = getSamplesBuffer(currentBeat);
    uint samplesToCopy = qMin(static_cast<uint>(samplesBuffer.getFrameLenght() - beatPosition), out.getFrameLenght());
    int nextBeatSample = beatPosition + out.getFrameLenght();
    int internalOffset = 0;
    int samplesBufferOffset = beatPosition;
    if (nextBeatSample > samplesPerBeat) { // next beat starting in this audio buffer?
        samplesBuffer = getSamplesBuffer(currentBeat + 1);
        internalOffset = samplesPerBeat - beatPosition;
        samplesToCopy = std::min(nextBeatSample - samplesPerBeat,
                                 (long)samplesBuffer.getFrameLenght());
        samplesBufferOffset = 0;
    }

    if (samplesToCopy > 0)
        internalInputBuffer.set(samplesBuffer, samplesBufferOffset, samplesToCopy,
                                internalOffset);
    AudioNode::processReplacing(in, out, midiBuffer);
}

bool MetronomeTrackNode::setSampleRate(int sampleRate)
{
    if (AudioNode::setSampleRate(sampleRate)) {
        audioBuffers.reset(); // free buffers with old sample rate
        updateSamplePerBeats();
        scheduleLoad();
        return true;
    }
    return false;
}
