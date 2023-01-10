TARGET = Jamtaba2
TEMPLATE = app

include(../Jamtaba-common.pri)
include(../Jamtaba-translations.pri)

JAMTABA_VERSION = $$(JAMTABA_VERSION)
JAMTABA_BUILDER = $$(JAMTABA_BUILDER)

DEFINES += APP_VERSION=\"\\\"$${JAMTABA_VERSION}\\\"\"

VPATH += $$SOURCE_PATH
VPATH += $$SOURCE_PATH/Standalone

INCLUDEPATH += $$SOURCE_PATH/Libs
INCLUDEPATH += $$SOURCE_PATH/Libs/RtMidi

INCLUDEPATH += $$ROOT_PATH/libs/includes/portaudio

INCLUDEPATH += $$SOURCE_PATH/Standalone

INCLUDEPATH += $$SOURCE_PATH/Standalone/gui

INCLUDEPATH += $$VST_SDK_PATH/VST2_SDK/pluginterfaces/vst2.x

HEADERS += MainControllerStandalone.h \
HEADERS += gui/MainWindowStandalone.h
HEADERS += gui/PreferencesDialogStandalone.h
HEADERS += gui/LocalTrackViewStandalone.h
HEADERS += gui/LocalTrackGroupViewStandalone.h
HEADERS += gui/ScanFolderPanel.h
HEADERS += gui/FxPanel.h
HEADERS += gui/FxPanelItem.h
HEADERS += gui/MidiToolsDialog.h
HEADERS += gui/CrashReportDialog.h

HEADERS += audio/PortAudioDriver.h
HEADERS += audio/Host.h
HEADERS += midi/RtMidiDriver.h
HEADERS += vst/VstPlugin.h
HEADERS += vst/VstHost.h
HEADERS += vst/VstLoader.h
HEADERS += PluginFinder.h
HEADERS += vst/VstPluginFinder.h
HEADERS += vst/Utils.h
HEADERS += Libs/SingleApplication/singleapplication.h
HEADERS += Libs/RtMidi/RtMidi.h

mac:HEADERS += AU/AudioUnitHost.h
mac:HEADERS += AU/AudioUnitPlugin.h

SOURCES += main.cpp
SOURCES += MainControllerStandalone.cpp
SOURCES += ConfiguratorStandalone.cpp
SOURCES += gui/MainWindowStandalone.cpp
SOURCES += gui/PreferencesDialogStandalone.cpp
SOURCES += gui/LocalTrackViewStandalone.cpp
SOURCES += gui/LocalTrackGroupViewStandalone.cpp
SOURCES += gui/ScanFolderPanel.cpp
SOURCES += gui/FxPanel.cpp
SOURCES += gui/FxPanelItem.cpp
SOURCES += gui/MidiToolsDialog.cpp
SOURCES += midi/RtMidiDriver.cpp
SOURCES += vst/VstPlugin.cpp
SOURCES += vst/VstHost.cpp
SOURCES += PluginFinder.cpp
SOURCES += vst/VstPluginFinder.cpp
SOURCES += vst/Utils.cpp
SOURCES += vst/VstLoader.cpp
SOURCES += Libs/SingleApplication/singleapplication.cpp
SOURCES += Libs/RtMidi/RtMidi.cpp
SOURCES += audio/PortAudioDriver.cpp
SOURCES += gui/CrashReportDialog.cpp

mac:SOURCES += AU/AudioUnitHost.cpp
mac:SOURCES += AU/AudioUnitPluginFinder.cpp


FORMS += gui/MidiToolsDialog.ui
FORMS += gui/CrashReportDialog.ui

#conditional sources to different platforms
win32{
    SOURCES += audio/WindowsPortAudioDriver.cpp
    SOURCES += vst/WindowsVstPluginChecker.cpp
}
macx{

    AU_SDK_PATH = "$$PWD/../../AU_SDK"
    VPATH += $$AU_SDK_PATH

    SOURCES += audio/MacPortAudioDriver.cpp
    OBJECTIVE_SOURCES += vst/MacVstPluginChecker.mm

    HEADERS += AU/AudioUnitPlugin.h
    OBJECTIVE_SOURCES += AU/AudioUnitPlugin.mm

    INCLUDEPATH += $$AU_SDK_PATH
}
linux{
    SOURCES += audio/LinuxPortAudioDriver.cpp
    SOURCES += vst/LinuxVstPluginChecker.cpp
}


win32{

    equals(JAMTABA_BUILDER, "docker") {
        LIBS += -L/usr/local/lib
        LIBS += -lvorbisenc
    } else {
        #supressing warning about missing .pdb files
        QMAKE_LFLAGS += /ignore:4099
        QMAKE_LFLAGS += "/NODEFAULTLIB:libcmt"

        !contains(QMAKE_TARGET.arch, x86_64) {
            #message("msvc x86 build") ## Windows x86 (32bit) specific build here
            LIBS_PATH = "static/win32-msvc"

            #after a lot or research Ezee found this userfull link explaining how compile to be compatible with Windows XP: http://www.tripleboot.org/?p=423
            QMAKE_LFLAGS_WINDOWS = /SUBSYSTEM:WINDOWS,5.01
        } else {
            #message("msvc x86_64 build") ## Windows x64 (64bit) specific build here
            LIBS_PATH = "static/win64-msvc"
        }
        LIBS += -L$$ROOT_PATH/libs/$$LIBS_PATH
        LIBS += -llegacy_stdio_definitions
    }

    CONFIG(release, debug|release) {
        LIBS += -lportaudio -lvorbisfile -lvorbis -logg -lx264 -lavcodec -lavutil -lavformat -lswscale -lswresample -lminiupnpc
    } else:CONFIG(debug, debug|release) {
        LIBS += -lportaudiod -lvorbisfiled -lvorbisd -loggd -lx264 -lavcodecd -lavutild -lavformatd -lswscaled -lswresample -lminiupnpcd
    }

    CONFIG(release, debug|release) {
        equals(JAMTABA_BUILDER, "docker") {
            QMAKE_CXXFLAGS_RELEASE += -flto
        } else {
            #ltcg - http://blogs.msdn.com/b/vcblog/archive/2009/02/24/quick-tips-on-using-whole-program-optimization.aspx
            QMAKE_CXXFLAGS_RELEASE +=  -GL -Gy -Gw
            QMAKE_LFLAGS_RELEASE += /LTCG
        }
    }

    LIBS += -lwinmm -lole32 -lws2_32 -ladvapi32 -luser32 -lpsapi
    LIBS += -liphlpapi  # used by miniupnp lib
    LIBS += -lsecur32   # used by libx264
    LIBS += -lbcrypt    # used by ffmpeg

    #performance monitor lib
    QMAKE_CXXFLAGS += -DPSAPI_VERSION=1

    RC_FILE = ../Jamtaba2.rc #windows icon
}

macx{
    #message("Mac x86_64 build")
    LIBS_PATH = "static/mac64"
    LIBS += -lz
    LIBS += -L$$PWD/../../libs/$$LIBS_PATH -lportaudio -lminimp3 -lvorbisfile -lvorbisenc -lvorbis -logg -lx264 -lavcodec -lavutil -lavformat -lswscale -lswresample -liconv -lminiupnpc
    LIBS += -framework IOKit
    LIBS += -framework CoreAudio
    LIBS += -framework CoreMidi
    LIBS += -framework AudioToolbox
    LIBS += -framework AudioUnit
    LIBS += -framework CoreServices
    LIBS += -framework Carbon
    LIBS += -framework Cocoa

    #mac osx doc icon
    ICON = ../Jamtaba.icns
}

linux{
    equals(JAMTABA_BUILDER, "docker") {
        message("Linux docker build")

        QMAKE_CXXFLAGS_RELEASE += -flto
    } else {
        message("Linux local build")

        contains(QMAKE_HOST.arch, x86_64) {
            LIBS_PATH = "static/linux64"
        } else {
            LIBS_PATH = "static/linux32"
        }
        LIBS += -L$$PWD/../../libs/$$LIBS_PATH
    }

    DEFINES += __LINUX_ALSA__

    LIBS += -lportaudio -lvorbisfile -lvorbisenc -lvorbis -logg -lavformat -lavcodec -lswscale -lavutil -lswresample -lminiupnpc -lx264
    LIBS += -lasound
    LIBS += -ldl
    LIBS += -lz
}
