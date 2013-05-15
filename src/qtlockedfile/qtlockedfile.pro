### eqmake4 was here ###
CONFIG -= debug_and_release debug
CONFIG += release

TEMPLATE=subdirs
CONFIG += ordered
include(common.pri)
qtlockedfile-uselib:SUBDIRS=buildlib
SUBDIRS+=example
