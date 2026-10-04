# Copyright 2006-2026 The QElectroTech Team
# This file is part of QElectroTech.
#
# QElectroTech is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 2 of the License, or
# (at your option) any later version.
#
# QElectroTech is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with QElectroTech. If not, see <http://www.gnu.org/licenses/>.

cmake_minimum_required(VERSION 3.5)

message("..___________________________________________________________________")

project(qt_unittests LANGUAGES CXX)

set(CMAKE_INCLUDE_CURRENT_DIR ON)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

SET(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

message(".. PROJECT_NAME              :" ${PROJECT_NAME})
message(".. PROJECT_SOURCE_DIR        :" ${PROJECT_SOURCE_DIR})
if(NOT DEFINED QET_DIR)
  set(QET_DIR "../..")
  message(".. QET_DIR is not set, assuming QET is ../..")
endif()
message(".. QET_DIR                   :" ${QET_DIR})
if(NOT DEFINED QET_COMPONENTS)
  message(".. QET_COMPONENTS is not set !!! I set them up !!!")
  include(../../cmake/qet_compilation_vars.cmake)
endif()

find_package(
 Qt6
 COMPONENTS
 ${QET_COMPONENTS}
 Test
 REQUIRED)

# tst_crashhandler compiles crashhandler.cpp, which on a non-Windows build
# may pull in backtrace(). Resolved the same way as in the top-level
# CMakeLists so this directory also configures standalone -- on FreeBSD the
# symbol lives in libexecinfo, not libc.
if(NOT DEFINED Backtrace_FOUND)
  find_package(Backtrace QUIET)
  if(Backtrace_FOUND)
    include_directories(${Backtrace_INCLUDE_DIRS})
    add_compile_definitions(QET_CRASH_BACKTRACE)
  endif()
endif()

include(../../cmake/fetch_kdeaddons.cmake)
include(../../cmake/fetch_singleapplication.cmake)
include(../../cmake/fetch_pugixml.cmake)

enable_testing()

# A source path given relative to QET_DIR (standalone configure) must not be
# read relative to sources/ by qet_add_test() below.
get_filename_component(QET_DIR "${QET_DIR}" ABSOLUTE)

# qet_add_test(<name> [OFFSCREEN] [BINARY] [EXAMPLES] [ELEMENTS]
#              [SOURCES <file>...] [LIBS <lib>...] [DEFINES <def>...]
#              [INCLUDES <dir>...] [DEPENDS <target>...])
#
# Builds the test <name> from <name>.cpp, registers it with ctest and gives
# it QET's sources/ as an include directory.
#   SOURCES    files compiled into the test; a relative path is taken from
#              sources/. Keep this to what the test needs: tests here compile
#              the files they exercise, not the whole application.
#   LIBS       linked besides Qt::Test
#   DEFINES    extra compile definitions
#   INCLUDES   extra include directories
#   DEPENDS    targets built before the test
#   OFFSCREEN  runs it on Qt's offscreen platform, so it needs no display
#   BINARY     it runs the qelectrotech binary: builds that first and
#              defines QET_TEST_BINARY_PATH
#   EXAMPLES   defines QET_EXAMPLES_DIR (the projects in examples/)
#   ELEMENTS   defines QET_ELEMENTS_DIR (the collection in elements/)
function(qet_add_test name)
  cmake_parse_arguments(PARSE_ARGV 1 arg
    "OFFSCREEN;BINARY;EXAMPLES;ELEMENTS"
    ""
    "SOURCES;LIBS;DEFINES;INCLUDES;DEPENDS")

  set(sources ${name}.cpp)
  foreach(src IN LISTS arg_SOURCES)
    if(IS_ABSOLUTE "${src}")
      list(APPEND sources "${src}")
    else()
      list(APPEND sources "${QET_DIR}/sources/${src}")
    endif()
  endforeach()

  add_executable(${name} ${sources})
  add_test(NAME ${name} COMMAND ${name})
  target_include_directories(${name} PRIVATE ${QET_DIR}/sources ${arg_INCLUDES})
  target_link_libraries(${name} PRIVATE Qt::Test ${arg_LIBS})
  if(arg_DEFINES)
    target_compile_definitions(${name} PRIVATE ${arg_DEFINES})
  endif()
  if(arg_DEPENDS)
    add_dependencies(${name} ${arg_DEPENDS})
  endif()
  if(arg_OFFSCREEN)
    set_property(TEST ${name} APPEND PROPERTY ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
  endif()
  if(arg_BINARY)
    add_dependencies(${name} qelectrotech)
    target_compile_definitions(${name} PRIVATE
      "QET_TEST_BINARY_PATH=\"$<TARGET_FILE:qelectrotech>\"")
  endif()
  if(arg_EXAMPLES)
    target_compile_definitions(${name} PRIVATE
      "QET_EXAMPLES_DIR=\"${QET_DIR}/examples\"")
  endif()
  if(arg_ELEMENTS)
    target_compile_definitions(${name} PRIVATE
      "QET_ELEMENTS_DIR=\"${QET_DIR}/elements\"")
  endif()
endfunction()

# qet.cpp carries much of what the files below need, and pulls in qeticons
# and shortcutmanager; these are compiled alongside rather than linking the
# whole application. The application symbols they need are supplied by
# each test itself.
set(QET_TEST_QET_SOURCES qet.cpp qeticons.cpp shortcutmanager.cpp)
set(QET_TEST_QET_LIBS Qt::Widgets Qt::Xml pugixml::pugixml)

# BorderProperties: the default folio border, and comparing two borders.
qet_add_test(tst_borderproperties
  SOURCES borderproperties.cpp
  LIBS Qt::Xml)

# The Qt-only KAutoSaveFile (sources/ui/nokde) finds the backups a crashed
# process left. It needs nothing but Qt, so it is built and run whether or
# not this build uses KDE Frameworks -- CI builds with them.
qet_add_test(tst_kautosavefile
  SOURCES ui/nokde/kautosavefile.cpp
  INCLUDES ${QET_DIR}/sources/ui/nokde)

# diagramsortkeys.h is a header-only helper (no QET link deps needed
# beyond Qt itself), so this test builds independently of the rest of
# the QET sources.
qet_add_test(tst_diagramsortkeys)

# bordercelllabels.h is header-only too.
qet_add_test(tst_bordercelllabels)

# contactusage.h is a header-only helper holding the contact counting
# rules, so this test builds independently of the rest of the QET sources.
qet_add_test(tst_contactusage)

# textgrid.h is a header-only helper holding the text snap rules, so this
# test builds independently of the rest of the QET sources.
qet_add_test(tst_textgrid)

# elementviewgrid.h is header-only: the symbol editor's snap step at each
# zoom (bugtracker #112), tested without an editor window.
qet_add_test(tst_elementviewgrid)

# alignment.h is header-only too (it builds on textgrid.h): the geometry
# behind the align commands, tested without a scene.
qet_add_test(tst_alignment)

# terminalnamecheck.h is header-only: the terminal name rule applied by the
# element editor on save and by --check-elements.
qet_add_test(tst_terminalnamecheck)

# configprofile.h is header-only: which settings a saved configuration
# profile carries, and how loading one replaces the live settings.
qet_add_test(tst_configprofile LIBS Qt::Gui)

# itemgroups.cpp keeps group membership and the selection rule apart from
# Diagram, so it is tested here on a plain scene of rectangles.
qet_add_test(tst_itemgroups
  SOURCES itemgroups.cpp
  LIBS Qt::Widgets Qt::Xml)

# shownkinds.cpp keeps View > Show (bugtracker #301) apart from the item
# classes, so it is tested here on a plain scene of rectangles.
qet_add_test(tst_shownkinds OFFSCREEN
  SOURCES shownkinds.cpp
  LIBS Qt::Widgets)

# textanchor.h is header-only: the point of a text chosen by its alignment,
# shown as Position X/Y in the text properties (#1155).
qet_add_test(tst_textanchor OFFSCREEN LIBS Qt::Widgets)

# legacyelementtypes.h is header-only: the rewrite of element type names in
# nomenclature queries saved before June 2022.
qet_add_test(tst_legacyelementtypes)

qet_add_test(tst_qetpalette
  SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/inkcontrast.h
    palettegraphicsview.cpp
    palettegraphicsview.h
    qetpalette.cpp
    qetpalette.h
    ElementsCollection/elementpreviewdelegate.cpp
    ElementsCollection/elementpreviewdelegate.h
  LIBS Qt::Widgets)

# qeticons.cpp is compiled in so the test can check the table entries
# themselves (icon sizes in the element panel), not only the theme files.
qet_add_test(tst_qeticons
  SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/inkcontrast.h
    qeticons.cpp
    qeticons.h
    qetpalette.cpp
    qetstyle.cpp
    qetpalette.h
    ${QET_DIR}/ico/icon-themes.qrc
  LIBS Qt::Widgets Qt::Svg)

qet_add_test(tst_smart_device
  SOURCES bomexport.cpp bomexport.h
  LIBS Qt::Sql)

# qet.cpp carries the SingleApplication argument wire format
# (joinWithSpaces/splitWithSpaces) and QET::stringToFileName().
qet_add_test(tst_qetstrings
  SOURCES ${QET_TEST_QET_SOURCES}
  LIBS ${QET_TEST_QET_LIBS})

# CommandSearchPopup: accent-blind matching, ranking, and that Enter runs
# only the owning window's highlighted, enabled command.
qet_add_test(tst_commandsearch OFFSCREEN
  SOURCES commandsearchpopup.cpp commandsearchpopup.h shortcutmanager.cpp
  LIBS Qt::Widgets)

# scriptheader.h is header-only: the // ==QETScript== block that turns a
# stored script into a button; and ShortcutManager::unregisterAction(), which
# takes a deleted script's command out of the lists again.
qet_add_test(tst_scriptheader OFFSCREEN
  SOURCES shortcutmanager.cpp
  LIBS Qt::Widgets)

# QETSql::execReadOnly() -- read-only enforcement for every project-database
# query, including the ones a .qet file carries. Compiles sqlreadonly.cpp
# alone against its own in-memory QSQLITE connection, so the security
# property is checked without standing up a QETProject.
qet_add_test(tst_sqlreadonly
  SOURCES dataBase/sqlreadonly.cpp
  LIBS Qt::Sql)

# QetSettings::scriptingEnabled() -- whether QElectroTech may run a script.
# Compiles qetsettings.cpp alone: the setting is deliberately a plain
# QSettings read, so the test needs nothing else of QElectroTech.
qet_add_test(tst_scriptingsetting
  SOURCES utils/qetsettings.cpp
  LIBS Qt::Gui)

# AiAssistantSetup: finding the MCP server beside an installation, and the
# configuration text for each AI assistant. Compiles the one file alone.
qet_add_test(tst_aiassistantsetup
  SOURCES ui/aiassistantsetup.cpp
  LIBS Qt::Core)

# QetSettings::sheetBackground() -- the sheet (folio) background the diagram
# editor remembers between runs. Compiles qetsettings.cpp alone for the same
# reason as the test above; Qt::Gui because the pair carries a QColor.
qet_add_test(tst_sheetbackgroundsetting
  SOURCES utils/qetsettings.cpp
  LIBS Qt::Gui)

# SpaceMouseMotion::map() -- how a 3D mouse sample pans and zooms a view.
# Pure code with no device or widget behind it, so it is built and run
# whether or not QET_ENABLE_SPACEMOUSE is on.
qet_add_test(tst_spacemousemotion
  SOURCES spacemouse/spacemousemotion.cpp
  LIBS Qt::Core)

# SpaceMouseHid -- decoding a 3D mouse's raw USB reports. Pure code, built
# whether or not QET_ENABLE_SPACEMOUSE is on; recordings from real devices
# go in fixtures/spacemouse/.
qet_add_test(tst_spacemousehid
  SOURCES spacemouse/spacemousehid.cpp
  LIBS Qt::Core)

# ConnexionBackend -- the macOS 3DxWare backend. Built on every platform and
# run against fakeconnexion, a stand-in for 3DxWare's client library that
# exports the same calls and answers from a thread of its own.
add_library(fakeconnexion MODULE fakeconnexion.cpp)
target_link_libraries(fakeconnexion PRIVATE Qt::Core)
qet_add_test(tst_spacemouseconnexion
  SOURCES spacemouse/connexionbackend.cpp spacemouse/spacemousebackend.h
  LIBS Qt::Core
  DEFINES FAKE_CONNEXION="$<TARGET_FILE:fakeconnexion>"
  DEPENDS fakeconnexion)

# CrashHandler::formatInt() -- the async-signal-safe decimal formatter the
# signal handler uses for the "Signal: N" line of a crash dump. Compiles
# crashhandler.cpp and logring.cpp alongside; the handler deliberately
# depends on nothing heavier than QByteArray. Qt::Xml because
# crashhandler.cpp includes qetversion.h for the header it builds at
# install() time, and that pulls in QDomElement.
qet_add_test(tst_crashhandler
  SOURCES logging/crashhandler.cpp logging/logring.cpp qetversion.cpp
  LIBS Qt::Xml ${Backtrace_LIBRARIES})

# The crash-dump bookkeeping from #905: which dumps get listed, offered and
# deleted, and what redact() masks. qetlogger.cpp needs exactly one symbol
# from the application (QETApp::dataDir()), which the test supplies itself,
# so this links the logging sources rather than the whole program.
qet_add_test(tst_crashdumps
  SOURCES
    logging/qetlogger.cpp
    logging/crashhandler.cpp
    logging/logring.cpp
    qetversion.cpp
  LIBS Qt::Widgets Qt::Xml pugixml::pugixml ${Backtrace_LIBRARIES}
  INCLUDES ${QET_DIR} ${QET_DIR}/sources/NameList)

qet_add_test(tst_menubarkeyboard LIBS Qt::Widgets)

# Bugtracker #734 regression: exports fixtures/qet_bug_repro_resaved.qet
# (the report's own canonical reproduction) via the built qelectrotech
# binary's --export-svg and checks no conductor path is self-retracing.
# Runs the real binary rather than linking conductor.cpp directly, since
# Conductor::generateConductorPath() pulls in the full graphics-item
# dependency graph (Diagram, Element, Terminal...) that the other,
# deliberately header-only tests above avoid.
qet_add_test(tst_conductorselfretrace BINARY)

# The wiring list export (--export-cables) keeps every wire drawn to a folio
# report when a report has several, joins only one-to-one report pairs, and
# prints the folio number rather than its template. fixtures/wiring_list_arrows.qet
# has one of each report pair; the examples check the page column.
qet_add_test(tst_wiringlistexport BINARY EXAMPLES)

# --export-pdf puts an A3 folio on an A3 page whichever way it is turned:
# QPageSize matches standard sheets upright only, so a wide folio is matched
# upright and turned. fixtures/pdf_page_a3.qet has one folio each way.
qet_add_test(tst_pdfpagesize BINARY LIBS Qt::Gui)

# --export-pdf writes the cross-reference links in the same order in every
# run, rather than in Element pointer order. Exports examples/industrial.qet
# several times, in separate processes.
qet_add_test(tst_pdflinkorder BINARY EXAMPLES)

# One text per potential: the number goes on the same conductor in every
# run when two conductors tie for longest. Exports
# fixtures/one_text_per_potential_tie.qet through the real binary, several
# times, since the order it used to depend on changes only between runs.
qet_add_test(tst_potentialtextcarrier BINARY)

# Junction dots: a wire's corner that lies on another wire of the same
# potential gets a dot, even when the two share no terminal (issue #1280).
# Exports fixtures/junction_dot_shared_potential.qet through the real binary.
qet_add_test(tst_junctiondots BINARY)

# With SOURCE_DATE_EPOCH set, --export-pdf writes the same bytes for the
# same project in every run. Exports examples/741.qet several times, in
# separate processes.
qet_add_test(tst_pdfreproducible BINARY EXAMPLES)

# Hops where wires cross (issue #436): the geometry in wirehops.cpp, and
# through the real binary, that --resave keeps the project setting and
# adds nothing to a project without it.
qet_add_test(tst_wirehops BINARY
  SOURCES wirehops.cpp
  LIBS Qt::Gui)

# Wires per terminal (discussion #1158): the rules in wiringrules.cpp, and
# through the real binary, that --resave keeps the project setting, adds
# none to a project without it, and that a wire past the limit is refused
# unless the master switch is off.
qet_add_test(tst_wiringrules BINARY
  SOURCES wiringrules.cpp
  LIBS Qt::Xml)

# The routing behind qet.routeConductor(): conductorrouter.cpp needs only
# Qt Core, so the geometry is tested without a scene.
qet_add_test(tst_conductorrouter SOURCES conductorrouter.cpp)

# The tests in this block run a script through the real binary's --run, so
# they exist only where --run does: a build without the Qt Qml module has
# no scripting (see QET_HAS_SCRIPTING in the top-level CMakeLists.txt).
if(QET_HAS_SCRIPTING)
  # qet.conductorUuids() / qet.conductorEnds(): a script lists a folio's
  # conductors by uuid and finds each one's two ends, on
  # fixtures/qet_bug_repro_resaved.qet.
  qet_add_test(tst_scriptconductoruuid BINARY)

  # qet.terminalPosition() and qet.conductorPath(), checked against each
  # other on a fixture's conductors.
  qet_add_test(tst_scriptlayoutreads BINARY)

  # QET_SETTINGS_DIR moves the settings into an INI file there (#1178): a
  # script places a symbol only the folder's settings file can resolve.
  qet_add_test(tst_settingsdir BINARY ELEMENTS)

  # qet.addElement() on a symbol that exists but cannot be read says so,
  # rather than reporting a collision with the copy already embedded.
  qet_add_test(tst_unreadableelement BINARY ELEMENTS)

  # The project database filled from the file as it is read holds exactly
  # what it holds filled from the built folios (QET_DATABASE_FROM_FOLIOS=1),
  # on every example saved once; an older file falls back, saying why.
  qet_add_test(tst_databasefromdocument BINARY EXAMPLES LIBS Qt::Xml)

  # The project database keeps a folio's date with its century: 741.qet's
  # 2010-09-21, read through project_summary_view under LC_ALL=en_US.UTF-8.
  qet_add_test(tst_dbfoliodate BINARY EXAMPLES)

  # qet.elementTerminals() lists each terminal's uuid and qet.terminalIndex()
  # turns element uuid + terminal uuid back into the index the conductor
  # calls take, on examples/perceuse.qet (two terminals at one point).
  qet_add_test(tst_scriptterminaluuid BINARY EXAMPLES LIBS Qt::Xml)

  # The same symbol placed twice in a project whose collection does not
  # start with its "import" category (examples/lmdg.qet).
  qet_add_test(tst_importcategory BINARY EXAMPLES ELEMENTS)

  # qet.currentFolio() through --run: the first folio, or -1 with none; and
  # --run keeps one undo step per call (only an editor run is grouped).
  qet_add_test(tst_scriptcurrentfolio BINARY EXAMPLES)

  # Element numbering schemes carry an id and elements follow them by it:
  # a legacy file gets stable ids on --resave; through --run, rename, edit,
  # refused removal and undo/redo keep the elements following the scheme.
  qet_add_test(tst_elementautonumids BINARY EXAMPLES LIBS Qt::Xml)
endif()

# A symbol saved without a uuid gets the same one on every load, and
# inserting a folio does not change it. Runs the real binary's --resave on
# fixtures/qet_bug_repro_resaved.qet with the symbols' uuids stripped.
qet_add_test(tst_derivedsymboluuid BINARY LIBS Qt::Xml)

# A wire saved without a uuid gets one derived from its two ends: the same
# on every load, saved, and unchanged by inserting a folio, reordering the
# wires or drawing a wire the other way. Runs the real binary's --resave on
# a fixture and on examples/tremie_vibrante.qet (ends by terminal number).
qet_add_test(tst_derivedwireuuid BINARY EXAMPLES LIBS Qt::Xml)

# Saving a project that was just saved changes nothing: runs the real
# binary's --resave twice on every project in examples/, and on one whose
# title block holds a single-space value (#973).
qet_add_test(tst_resaveunchanged BINARY EXAMPLES)

# Replacing a symbol's definition in a project keeps its terminal uuids, so
# the wires saved against them still load: TerminalUuids::keep() on its own,
# then the real binary's --info on an example whose symbols were replaced.
qet_add_test(tst_terminaluuids BINARY EXAMPLES
  SOURCES ElementsCollection/terminaluuids.cpp
  LIBS Qt::Xml)

# ConductorProperties::applyChanges() -- the Selection properties panel
# writes only the fields the user edited.
qet_add_test(tst_conductorapplychanges
  SOURCES conductorproperties.cpp ${QET_TEST_QET_SOURCES}
  LIBS ${QET_TEST_QET_LIBS})

# ConductorMultiEdit -- editing several wires at once in the Selection
# properties panel: order, which wires an edit reaches, one undo step for
# all of them, and fields shown blank because the wires disagree.
qet_add_test(tst_conductormultiedit
  SOURCES
    conductorproperties.cpp
    QPropertyUndoCommand/qpropertyundocommand.cpp
    ${QET_TEST_QET_SOURCES}
  LIBS ${QET_TEST_QET_LIBS})

# DiagramContext::fromXml() -- the two readers (QDom for projects, pugixml
# for element definitions in the collection) give the same values: stray
# spaces trimmed, accents kept.
qet_add_test(tst_diagramcontext
  SOURCES diagramcontext.cpp ${QET_TEST_QET_SOURCES}
  LIBS ${QET_TEST_QET_LIBS})

# XRefProperties: an empty or unknown cross-reference position, as older
# versions saved it, reads as the default instead of drawing the
# cross-reference over the element's label (issue #1238).
qet_add_test(tst_xrefpos
  SOURCES properties/xrefproperties.cpp properties/propertiesinterface.cpp
  LIBS Qt::Widgets Qt::Xml pugixml::pugixml)

# SearchAndReplaceWorker::applyChange() -- what the Search and replace dialog
# writes into each field found: empty keeps, the erase marker clears, a
# wire's out-of-range values keep. The worker's header and replace*()
# functions reach into the whole scene, so the test compiles the worker with
# its headers and stubs the dozen scene symbols those functions call; only
# the static helpers run. OFFSCREEN for setupLineEdit()'s widgets.
qet_add_test(tst_searchandreplace OFFSCREEN
  SOURCES
    SearchAndReplace/searchandreplaceworker.cpp
    conductorproperties.cpp
    titleblockproperties.cpp
    diagramcontext.cpp
    QPropertyUndoCommand/qpropertyundocommand.cpp
    ${QET_TEST_QET_SOURCES}
  LIBS ${QET_TEST_QET_LIBS} Qt::Sql
  INCLUDES ${QET_DIR}/sources/ui ${QET_DIR}/sources/ui/nokde
  DEFINES BUILD_WITHOUT_KF)

# Qt 6.4.2 writes the attributes of an element in an order that changes from
# one process to the next, so a project saved twice by two runs of the binary
# differs in bytes only (seen on projects without any numbering, too). A fixed
# hash seed makes the order stable; it changes nothing for a newer Qt.
if(Qt6_VERSION VERSION_LESS 6.5.0)
  foreach(qet_test tst_resaveunchanged tst_elementautonumids)
    if(TEST ${qet_test})
      set_property(TEST ${qet_test} APPEND PROPERTY ENVIRONMENT "QT_HASH_SEED=0")
    endif()
  endforeach()
endif()
