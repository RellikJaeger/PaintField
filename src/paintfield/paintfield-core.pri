
include(../src.pri)

PF_VERSION = 0.0.1

macx {
PF_PLATFORM = "mac"
}

unix:!macx {
PF_PLATFORM = "unix"
}

QT += core gui network xml svg plugin
QMAKE_CXXFLAGS += -std=c++11

LIBS += -lfreeimage -lqjson

INCLUDEPATH += $$PWD/.. $$PWD/../libs $$PWD/../libs/Malachite/include

# note that the head of a result is always /
defineReplace(relativePathFrom) {

message($$1)
message($$2)

path_to = $$1
path_from = $$2

path_to_from = $$replace(path_to, ^$${path_from}, )

message($$path_to_from)

contains(path_to_from, ^$${path_to}$) {
	message(from > to)
	
	path_to_from = $$replace(path_from, ^$$path_to, )
	path_to_from = $$replace(path_to_from, [^/]+, ..)
	
} else {
	message(to > from)
}

message($$path_to_from)

return($$path_to_from)

}

PF_OUT_PWD = $$OUT_PWD$$relativePathFrom($$PWD, $$_PRO_FILE_PWD_)

LIBS += -L$$PF_OUT_PWD/../libs/Malachite/src -L$$PF_OUT_PWD/../libs/Minizip
LIBS += -lmalachite -lminizip
