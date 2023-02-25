#ifndef UTILS_H
#define UTILS_H

#include <cmath>

class Utils // TODO - use a namespace
{

public:

    static inline float linearGainToPower(float linearGain)
    {
        return std::pow(linearGain, 3);
    }

    static inline float poweredGainToLinear(float poweredGain)
    {
        return std::pow(poweredGain, 1.0/3);
    }

    static inline float dbToLinear(float dbValue)
    {
        // db-to-linear(x) = 10^(x / 20)
        return std::pow(10, dbValue/20);
    }

    static inline float linearToDb(float linearValue)
    {
        // linear-to-db(x) = log(x) * 20
        return std::log10(linearValue) * 20.0;
    }

    static inline float clampPan(float value)
    {
        if (value < -1)
            return -1;
        if (value > 1)
            return 1;
        return value;
    }

    static inline float clampGain(float value)
    {
        if (value < 0)
            return 0;
        if (value > 2)
            return 2;
        return value;
    }

    static inline int clampBoost(int value)
    {
        if (value == 0)
            return 0;
        if (value < 0)
            return -1;
        return 1;
    }
};

#endif // UTILS_H
