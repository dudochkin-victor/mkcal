### eqmake4 was here ###
CONFIG -= debug_and_release debug
CONFIG += release

DOXYGEN_BIN=doxygen

QMAKE_EXTRA_TARGETS += doc
doc.target = doc
isEmpty(DOXYGEN_BIN) {
    doc.commands = @echo "Unable to detect doxygen in PATH"
} else {
    doc.commands = @$${DOXYGEN_BIN} doc/libmkcal.cfg ;
    doc.commands+= ./doc/xmlize.pl ;
}
doc.depends = FORCE

# Install rules
htmldocs.files = ./doc/html/*
htmldocs.path = /usr/share/doc/libmkcal-doc
htmldocs.CONFIG += no_check_exist

INSTALLS += htmldocs    
