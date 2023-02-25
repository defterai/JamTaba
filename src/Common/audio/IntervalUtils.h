#ifndef INTERVALUTILS_H
#define INTERVALUTILS_H

namespace audio {

namespace intervalUtils {

// calc interval period in milliseconds
inline double getIntervalPeriod(int bpm, int bpi) {
    return 60000.0 / bpm * bpi;
}

inline long getTotalSamplesInInterval(int bpm, int bpi, int sampleRate) {
    double intervalPeriod = getIntervalPeriod(bpm, bpi);
    return (long)(sampleRate * intervalPeriod / 1000.0);
}

inline long getSamplesPerBeat(int bpm, int bpi, int sampleRate) {
    return getTotalSamplesInInterval(bpm, bpi, sampleRate) / bpi;
}

}

}

#endif // INTERVALUTILS_H
