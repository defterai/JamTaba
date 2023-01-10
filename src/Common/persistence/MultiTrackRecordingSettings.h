#ifndef MULTITRACKRECORDINGSETTINGS_H
#define MULTITRACKRECORDINGSETTINGS_H

#include "SettingsObject.h"

namespace persistence {

class MultiTrackRecordingSettings final : public SettingsObject
{
public:
    static const QString DATE_FORMAT_ISO;
    static const QString DATE_FORMAT_TEXT;

    MultiTrackRecordingSettings();
    void write(QJsonObject &out) const override;
    void read(const QJsonObject &in) override;
    inline bool isJamRecorderActivated(const QString &key) const
    {
        auto it = jamRecorderActivated.find(key);
        if (it != jamRecorderActivated.end()) {
            return it.value();
        }
        return false;
    }
    inline void setJamRecorderActivated(const QString &key, bool value)
    {
        jamRecorderActivated[key] = value;
    }
    inline QString getRecordingPath() const
    {
        return recordingPath;
    }
    inline void setRecordingPath(const QString &newPath)
    {
        recordingPath = newPath;
    }
    inline Qt::DateFormat getDirNameDateFormat() const
    {
        return dirNameDateFormat == DATE_FORMAT_ISO ? Qt::ISODate : Qt::TextDate;
    }
    inline const QString& getDirNameDateFormatStr() const
    {
        return dirNameDateFormat;
    }
    void setDirNameDateFormat(const QString &newDateFormat);
    void setDirNameDateFormat(const Qt::DateFormat dateFormat);
    inline bool isSaveMultiTrackActivated() const
    {
        return saveMultiTracksActivated;
    }
    inline void setSaveMultiTrack(bool saveMultiTracks)
    {
        saveMultiTracksActivated = saveMultiTracks;
    }
private:
    static QString getDefaultRecordingPath();
    QMap<QString, bool> jamRecorderActivated;
    QString recordingPath;
    QString dirNameDateFormat;
    bool saveMultiTracksActivated;
};

}

#endif // MULTITRACKRECORDINGSETTINGS_H
