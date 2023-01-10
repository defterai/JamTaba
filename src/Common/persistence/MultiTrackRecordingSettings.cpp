#include "MultiTrackRecordingSettings.h"
#include "log/Logging.h"
#include <QDir>
#include <QStandardPaths>

using namespace persistence;

const QString MultiTrackRecordingSettings::DATE_FORMAT_ISO = "Qt::ISODate";
const QString MultiTrackRecordingSettings::DATE_FORMAT_TEXT = "Qt::TextDate";

MultiTrackRecordingSettings::MultiTrackRecordingSettings() :
    SettingsObject("recording"),
    jamRecorderActivated(QMap<QString, bool>()),
    recordingPath(""),
    dirNameDateFormat(DATE_FORMAT_TEXT),
    saveMultiTracksActivated(false)
{
    qCDebug(jtSettings) << "MultiTrackRecordingSettings ctor";
    // TODO: populate jamRecorderActivated with {jamRecorderId, false} pairs for each known jamRecorder
}

void MultiTrackRecordingSettings::write(QJsonObject &out) const
{
    qCDebug(jtSettings) << "MultiTrackRecordingSettings write";
    out["recordingPath"] = QDir::toNativeSeparators(recordingPath);
    out["dirNameDateFormat"] = dirNameDateFormat;
    out["recordActivated"] = saveMultiTracksActivated;
    QJsonObject jamRecorders = QJsonObject();
    for (auto it = jamRecorderActivated.begin(); it != jamRecorderActivated.end(); ++it) {
        QJsonObject jamRecorder = QJsonObject();
        jamRecorder["activated"] = it.value();
        jamRecorders[it.key()] = jamRecorder;
    }
    out["jamRecorders"] = jamRecorders;
}

void MultiTrackRecordingSettings::read(const QJsonObject &in)
{
    bool useDefaultRecordingPath = false;
    if (in.contains("recordingPath")) {
        recordingPath = in["recordingPath"].toString();
        if (!recordingPath.isEmpty()) {
            QDir dir(QDir::fromNativeSeparators(recordingPath));
            if (!dir.exists() && !dir.mkpath(".")) {
                qWarning() << "Dir " << dir << " not exists, using the application directory to save multitracks!";
                useDefaultRecordingPath = true;
            }
        } else {
            useDefaultRecordingPath = true;
        }
    } else {
        useDefaultRecordingPath = true;
    }

    if (useDefaultRecordingPath) {
        recordingPath = MultiTrackRecordingSettings::getDefaultRecordingPath();
    }

    if (in.contains("dirNameDateFormat")) {
        setDirNameDateFormat(in["dirNameDateFormat"].toString());
    }

    saveMultiTracksActivated = getValueFromJson(in, "recordActivated", false);

    QJsonObject jamRecorders = getValueFromJson(in, "jamRecorders", QJsonObject());
    for (auto it = jamRecorders.begin(); it != jamRecorders.end(); ++it) {
        QJsonObject jamRecorder = it.value().toObject();
        jamRecorderActivated[it.key()] = getValueFromJson(jamRecorder, "activated", false);
    }

    qCDebug(jtSettings) << "MultiTrackRecordingSettings: recordingPath " << recordingPath
                        << " (useDefaultRecordingPath " << useDefaultRecordingPath << ")"
                        << "; dirNameDateFormat " << dirNameDateFormat
                        << "; saveMultiTracksActivated " << saveMultiTracksActivated
                        << "; jamRecorderActivated " << jamRecorderActivated;
}

QString MultiTrackRecordingSettings::getDefaultRecordingPath()
{
    qCDebug(jtSettings) << "MultiTrackRecordingSettings getDefaultRecordingPath";
    QString userDocuments = QStandardPaths::displayName(QStandardPaths::DocumentsLocation);
    QDir pathDir(QDir::homePath());
    QDir documentsDir(pathDir.absoluteFilePath(userDocuments));
    QDir jamTabaDir = QDir(documentsDir).absoluteFilePath("JamTaba");

    return QDir(jamTabaDir).absoluteFilePath("Jams"); // using 'Jams' as default recording folder (issue #891)
}

void MultiTrackRecordingSettings::setDirNameDateFormat(const QString &newDateFormat)
{
    if (newDateFormat == DATE_FORMAT_ISO || newDateFormat == DATE_FORMAT_TEXT) {
        dirNameDateFormat = newDateFormat;
    }
}

void MultiTrackRecordingSettings::setDirNameDateFormat(const Qt::DateFormat dateFormat)
{
    switch (dateFormat) {
    case Qt::ISODate:
        dirNameDateFormat = DATE_FORMAT_ISO;
        break;
    default:
        dirNameDateFormat = DATE_FORMAT_TEXT;
        break;
    }
}
