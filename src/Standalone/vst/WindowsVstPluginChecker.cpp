#include "VstPluginChecker.h"
#include <QFile>
#include <QFileInfo>
#include <QLibrary>
#include <QDataStream>

class ExecutableFormatChecker
{
public:
    static bool is32Bits(const QString &dllPath);
    static bool is64Bits(const QString &dllPath);

private:
    static quint16 getMachineHeader(const QString &dllPath);
};


//implementation for the windows version of this function. Another version is implemented in the MacVstPluginChecker.cpp file.
bool Vst::PluginChecker::isValidPluginFile(const QString &pluginPath)
{
    QFileInfo pluginFileInfo(pluginPath);
    if(!pluginFileInfo.isFile())
        return false;

    if(!QLibrary::isLibrary(pluginPath))
        return false;

    if(pluginFileInfo.fileName().contains("Jamtaba"))//avoid Jamtaba standalone loading Jamtaba plugin. This is just a basic check, when the VSt plugin is loaded the real (compiled) name of the plugin is checked again.
        return false;

#ifdef _WIN64
    return ExecutableFormatChecker::is64Bits(pluginPath);
#else
    return ExecutableFormatChecker::is32Bits(pluginPath);
#endif

    return false;
}

//+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

const quint16 IMAGE_FILE_MACHINE_UNKNOWN = 0;
const quint16 IMAGE_FILE_MACHINE_I386 = 0x14c;
const quint16 IMAGE_FILE_MACHINE_AMD64 = 0x8664;

bool ExecutableFormatChecker::is32Bits(const QString &dllPath)
{
    return getMachineHeader(dllPath) == IMAGE_FILE_MACHINE_I386;
}

bool ExecutableFormatChecker::is64Bits(const QString &dllPath)
{
    return getMachineHeader(dllPath) == IMAGE_FILE_MACHINE_AMD64;
}

quint16 ExecutableFormatChecker::getMachineHeader(const QString &dllPath)
{
    // See http://www.microsoft.com/whdc/system/platform/firmware/PECOFF.mspx
    // Offset to PE header is always at 0x3C.
    // The PE header starts with "PE\0\0" =  0x50 0x45 0x00 0x00,
    // followed by a 2-byte machine type field (see the document above for the enum).

    QFile dllFile(dllPath);
    if (!dllFile.open(QFile::ReadOnly))
        return IMAGE_FILE_MACHINE_UNKNOWN;
    if (!dllFile.seek(0x3c))
        return IMAGE_FILE_MACHINE_UNKNOWN;
    QDataStream dataStream(&dllFile);
    dataStream.setByteOrder(QDataStream::LittleEndian);
    qint32 peOffset;
    dataStream >> peOffset;
    if (!dllFile.seek(peOffset))
        return IMAGE_FILE_MACHINE_UNKNOWN;
    quint32 peHead;
    dataStream >> peHead;
    if (peHead != 0x00004550) // "PE\0\0", little-endian
        return IMAGE_FILE_MACHINE_UNKNOWN; // "Can't find PE header"

    quint16 machineType;
    dataStream >> machineType;
    dllFile.close();
    return machineType;
}
