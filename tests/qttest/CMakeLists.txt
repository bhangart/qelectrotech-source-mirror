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

include(qet_add_test.cmake)

# qet.cpp carries much of what the files below need, and pulls in qeticons
# and shortcutmanager; these are compiled alongside rather than linking the
# whole application. The application symbols they need are supplied by
# each test itself.
set(QET_TEST_QET_SOURCES qet.cpp qeticons.cpp shortcutmanager.cpp)
set(QET_TEST_QET_LIBS Qt::Widgets Qt::Xml pugixml::pugixml)

add_executable(${PROJECT_NAME} tst_mytest.cpp)
add_test(NAME ${PROJECT_NAME} COMMAND ${PROJECT_NAME})

target_link_libraries(
  ${PROJECT_NAME}
  PRIVATE
  Qt::Test
  ${KF_PRIVATE_LIBRARIES}
  ${QET_PRIVATE_LIBRARIES})

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
add_executable(
  tst_conductorselfretrace
  tst_conductorselfretrace.cpp)
add_test(NAME tst_conductorselfretrace COMMAND tst_conductorselfretrace)
add_dependencies(tst_conductorselfretrace qelectrotech)
target_include_directories(tst_conductorselfretrace PRIVATE ${QET_DIR}/sources)
target_link_libraries(tst_conductorselfretrace PRIVATE Qt::Test)
target_compile_definitions(tst_conductorselfretrace PRIVATE
  "QET_TEST_BINARY_PATH=\"$<TARGET_FILE:qelectrotech>\"")

# The wiring list export (--export-cables) keeps every wire drawn to a folio
# report when a report has several, joins only one-to-one report pairs, and
# prints the folio number rather than its template. fixtures/wiring_list_arrows.qet
# has one of each report pair; the examples check the page column.
add_executable(
  tst_wiringlistexport
  tst_wiringlistexport.cpp)
add_test(NAME tst_wiringlistexport COMMAND tst_wiringlistexport)
add_dependencies(tst_wiringlistexport qelectrotech)
target_link_libraries(tst_wiringlistexport PRIVATE Qt::Test)
target_compile_definitions(tst_wiringlistexport PRIVATE
  "QET_TEST_BINARY_PATH=\"$<TARGET_FILE:qelectrotech>\""
  "QET_EXAMPLES_DIR=\"${QET_DIR}/examples\"")

# --export-pdf puts an A3 folio on an A3 page whichever way it is turned:
# QPageSize matches standard sheets upright only, so a wide folio is matched
# upright and turned. fixtures/pdf_page_a3.qet has one folio each way.
add_executable(
  tst_pdfpagesize
  tst_pdfpagesize.cpp)
add_test(NAME tst_pdfpagesize COMMAND tst_pdfpagesize)
add_dependencies(tst_pdfpagesize qelectrotech)
target_link_libraries(tst_pdfpagesize PRIVATE Qt::Test Qt::Gui)
target_compile_definitions(tst_pdfpagesize PRIVATE
  "QET_TEST_BINARY_PATH=\"$<TARGET_FILE:qelectrotech>\"")

# --export-pdf writes the cross-reference links in the same order in every
# run, rather than in Element pointer order. Exports examples/industrial.qet
# several times, in separate processes.
add_executable(
  tst_pdflinkorder
  tst_pdflinkorder.cpp)
add_test(NAME tst_pdflinkorder COMMAND tst_pdflinkorder)
add_dependencies(tst_pdflinkorder qelectrotech)
target_link_libraries(tst_pdflinkorder PRIVATE Qt::Test)
target_compile_definitions(tst_pdflinkorder PRIVATE
  "QET_TEST_BINARY_PATH=\"$<TARGET_FILE:qelectrotech>\""
  "QET_EXAMPLES_DIR=\"${QET_DIR}/examples\"")

# One text per potential: the number goes on the same conductor in every
# run when two conductors tie for longest. Exports
# fixtures/one_text_per_potential_tie.qet through the real binary, several
# times, since the order it used to depend on changes only between runs.
add_executable(
  tst_potentialtextcarrier
  tst_potentialtextcarrier.cpp)
add_test(NAME tst_potentialtextcarrier COMMAND tst_potentialtextcarrier)
add_dependencies(tst_potentialtextcarrier qelectrotech)
target_link_libraries(tst_potentialtextcarrier PRIVATE Qt::Test)
target_compile_definitions(tst_potentialtextcarrier PRIVATE
  "QET_TEST_BINARY_PATH=\"$<TARGET_FILE:qelectrotech>\"")

# Junction dots: a wire's corner that lies on another wire of the same
# potential gets a dot, even when the two share no terminal (issue #1280).
# Exports fixtures/junction_dot_shared_potential.qet through the real binary.
add_executable(
  tst_junctiondots
  tst_junctiondots.cpp)
add_test(NAME tst_junctiondots COMMAND tst_junctiondots)
add_dependencies(tst_junctiondots qelectrotech)
target_link_libraries(tst_junctiondots PRIVATE Qt::Test)
target_compile_definitions(tst_junctiondots PRIVATE
  "QET_TEST_BINARY_PATH=\"$<TARGET_FILE:qelectrotech>\"")

# With SOURCE_DATE_EPOCH set, --export-pdf writes the same bytes for the
# same project in every run. Exports examples/741.qet several times, in
# separate processes.
add_executable(
  tst_pdfreproducible
  tst_pdfreproducible.cpp)
add_test(NAME tst_pdfreproducible COMMAND tst_pdfreproducible)
add_dependencies(tst_pdfreproducible qelectrotech)
target_link_libraries(tst_pdfreproducible PRIVATE Qt::Test)
target_compile_definitions(tst_pdfreproducible PRIVATE
  "QET_TEST_BINARY_PATH=\"$<TARGET_FILE:qelectrotech>\""
  "QET_EXAMPLES_DIR=\"${QET_DIR}/examples\"")

# Hops where wires cross (issue #436): the geometry in wirehops.cpp, and
# through the real binary, that --resave keeps the project setting and
# adds nothing to a project without it.
add_executable(
  tst_wirehops
  tst_wirehops.cpp
  ${QET_DIR}/sources/wirehops.cpp)
add_test(NAME tst_wirehops COMMAND tst_wirehops)
add_dependencies(tst_wirehops qelectrotech)
target_include_directories(tst_wirehops PRIVATE ${QET_DIR}/sources)
target_link_libraries(tst_wirehops PRIVATE Qt::Test Qt::Gui)
target_compile_definitions(tst_wirehops PRIVATE
  "QET_TEST_BINARY_PATH=\"$<TARGET_FILE:qelectrotech>\"")

# Wires per terminal (discussion #1158): the rules in wiringrules.cpp, and
# through the real binary, that --resave keeps the project setting, adds
# none to a project without it, and that a wire past the limit is refused
# unless the master switch is off.
add_executable(
  tst_wiringrules
  tst_wiringrules.cpp
  ${QET_DIR}/sources/wiringrules.cpp)
add_test(NAME tst_wiringrules COMMAND tst_wiringrules)
add_dependencies(tst_wiringrules qelectrotech)
target_include_directories(tst_wiringrules PRIVATE ${QET_DIR}/sources)
target_link_libraries(tst_wiringrules PRIVATE Qt::Test Qt::Xml)
target_compile_definitions(tst_wiringrules PRIVATE
  "QET_TEST_BINARY_PATH=\"$<TARGET_FILE:qelectrotech>\"")

# The routing behind qet.routeConductor(): conductorrouter.cpp needs only
# Qt Core, so the geometry is tested without a scene.
qet_add_test(tst_conductorrouter SOURCES conductorrouter.cpp)

# qet.conductorUuids() / qet.conductorEnds(): a script lists a folio's
# conductors by uuid and finds each one's two ends. Runs a script through
# the real binary's --run on fixtures/qet_bug_repro_resaved.qet, so only
# where --run exists: a build without the Qt Qml module has no scripting
# (see QET_HAS_SCRIPTING in the top-level CMakeLists.txt).
if(QET_HAS_SCRIPTING)
  add_executable(
    tst_scriptconductoruuid
    tst_scriptconductoruuid.cpp)
  add_test(NAME tst_scriptconductoruuid COMMAND tst_scriptconductoruuid)
  add_dependencies(tst_scriptconductoruuid qelectrotech)
  target_link_libraries(tst_scriptconductoruuid PRIVATE Qt::Test)
  target_compile_definitions(tst_scriptconductoruuid PRIVATE
    "QET_TEST_BINARY_PATH=\"$<TARGET_FILE:qelectrotech>\"")

  # qet.terminalPosition() and qet.conductorPath(), checked against each
  # other on a fixture's conductors.
  add_executable(
    tst_scriptlayoutreads
    tst_scriptlayoutreads.cpp)
  add_test(NAME tst_scriptlayoutreads COMMAND tst_scriptlayoutreads)
  add_dependencies(tst_scriptlayoutreads qelectrotech)
  target_link_libraries(tst_scriptlayoutreads PRIVATE Qt::Test)
  target_compile_definitions(tst_scriptlayoutreads PRIVATE
    "QET_TEST_BINARY_PATH=\"$<TARGET_FILE:qelectrotech>\"")

  # QET_SETTINGS_DIR moves the settings into an INI file there (#1178): a
  # script places a symbol only the folder's settings file can resolve.
  add_executable(
    tst_settingsdir
    tst_settingsdir.cpp)
  add_test(NAME tst_settingsdir COMMAND tst_settingsdir)
  add_dependencies(tst_settingsdir qelectrotech)
  target_link_libraries(tst_settingsdir PRIVATE Qt::Test)
  target_compile_definitions(tst_settingsdir PRIVATE
    "QET_TEST_BINARY_PATH=\"$<TARGET_FILE:qelectrotech>\""
    "QET_ELEMENTS_DIR=\"${QET_DIR}/elements\"")

  # qet.addElement() on a symbol that exists but cannot be read says so,
  # rather than reporting a collision with the copy already embedded.
  add_executable(
    tst_unreadableelement
    tst_unreadableelement.cpp)
  add_test(NAME tst_unreadableelement COMMAND tst_unreadableelement)
  add_dependencies(tst_unreadableelement qelectrotech)
  target_link_libraries(tst_unreadableelement PRIVATE Qt::Test)
  target_compile_definitions(tst_unreadableelement PRIVATE
    "QET_TEST_BINARY_PATH=\"$<TARGET_FILE:qelectrotech>\""
    "QET_ELEMENTS_DIR=\"${QET_DIR}/elements\"")

  # The project database filled from the file as it is read holds exactly
  # what it holds filled from the built folios (QET_DATABASE_FROM_FOLIOS=1),
  # on every example saved once; an older file falls back, saying why.
  add_executable(
    tst_databasefromdocument
    tst_databasefromdocument.cpp)
  add_test(NAME tst_databasefromdocument COMMAND tst_databasefromdocument)
  add_dependencies(tst_databasefromdocument qelectrotech)
  target_link_libraries(tst_databasefromdocument PRIVATE Qt::Test Qt::Xml)
  target_compile_definitions(tst_databasefromdocument PRIVATE
    "QET_TEST_BINARY_PATH=\"$<TARGET_FILE:qelectrotech>\""
    "QET_EXAMPLES_DIR=\"${QET_DIR}/examples\"")

  # The project database keeps a folio's date with its century: 741.qet's
  # 2010-09-21, read through project_summary_view under LC_ALL=en_US.UTF-8.
  add_executable(
    tst_dbfoliodate
    tst_dbfoliodate.cpp)
  add_test(NAME tst_dbfoliodate COMMAND tst_dbfoliodate)
  add_dependencies(tst_dbfoliodate qelectrotech)
  target_link_libraries(tst_dbfoliodate PRIVATE Qt::Test)
  target_compile_definitions(tst_dbfoliodate PRIVATE
    "QET_TEST_BINARY_PATH=\"$<TARGET_FILE:qelectrotech>\""
    "QET_EXAMPLES_DIR=\"${QET_DIR}/examples\"")

  # qet.elementTerminals() lists each terminal's uuid and qet.terminalIndex()
  # turns element uuid + terminal uuid back into the index the conductor
  # calls take, on examples/perceuse.qet (two terminals at one point).
  add_executable(
    tst_scriptterminaluuid
    tst_scriptterminaluuid.cpp)
  add_test(NAME tst_scriptterminaluuid COMMAND tst_scriptterminaluuid)
  add_dependencies(tst_scriptterminaluuid qelectrotech)
  target_link_libraries(tst_scriptterminaluuid PRIVATE Qt::Test Qt::Xml)
  target_compile_definitions(tst_scriptterminaluuid PRIVATE
    "QET_TEST_BINARY_PATH=\"$<TARGET_FILE:qelectrotech>\""
    "QET_EXAMPLES_DIR=\"${QET_DIR}/examples\"")

  # The same symbol placed twice in a project whose collection does not
  # start with its "import" category (examples/lmdg.qet).
  add_executable(
    tst_importcategory
    tst_importcategory.cpp)
  add_test(NAME tst_importcategory COMMAND tst_importcategory)
  add_dependencies(tst_importcategory qelectrotech)
  target_link_libraries(tst_importcategory PRIVATE Qt::Test)
  target_compile_definitions(tst_importcategory PRIVATE
    "QET_TEST_BINARY_PATH=\"$<TARGET_FILE:qelectrotech>\""
    "QET_EXAMPLES_DIR=\"${QET_DIR}/examples\""
    "QET_ELEMENTS_DIR=\"${QET_DIR}/elements\"")

  # qet.currentFolio() through --run: the first folio, or -1 with none; and
  # --run keeps one undo step per call (only an editor run is grouped).
  add_executable(
    tst_scriptcurrentfolio
    tst_scriptcurrentfolio.cpp)
  add_test(NAME tst_scriptcurrentfolio COMMAND tst_scriptcurrentfolio)
  add_dependencies(tst_scriptcurrentfolio qelectrotech)
  target_link_libraries(tst_scriptcurrentfolio PRIVATE Qt::Test)
  target_compile_definitions(tst_scriptcurrentfolio PRIVATE
    "QET_TEST_BINARY_PATH=\"$<TARGET_FILE:qelectrotech>\""
    "QET_EXAMPLES_DIR=\"${QET_DIR}/examples\"")
endif()

# A symbol saved without a uuid gets the same one on every load, and
# inserting a folio does not change it. Runs the real binary's --resave on
# fixtures/qet_bug_repro_resaved.qet with the symbols' uuids stripped.
add_executable(
  tst_derivedsymboluuid
  tst_derivedsymboluuid.cpp)
add_test(NAME tst_derivedsymboluuid COMMAND tst_derivedsymboluuid)
add_dependencies(tst_derivedsymboluuid qelectrotech)
target_link_libraries(tst_derivedsymboluuid PRIVATE Qt::Test Qt::Xml)
target_compile_definitions(tst_derivedsymboluuid PRIVATE
  "QET_TEST_BINARY_PATH=\"$<TARGET_FILE:qelectrotech>\"")

# A wire saved without a uuid gets one derived from its two ends: the same
# on every load, saved, and unchanged by inserting a folio, reordering the
# wires or drawing a wire the other way. Runs the real binary's --resave on
# a fixture and on examples/tremie_vibrante.qet (ends by terminal number).
add_executable(
  tst_derivedwireuuid
  tst_derivedwireuuid.cpp)
add_test(NAME tst_derivedwireuuid COMMAND tst_derivedwireuuid)
add_dependencies(tst_derivedwireuuid qelectrotech)
target_link_libraries(tst_derivedwireuuid PRIVATE Qt::Test Qt::Xml)
target_compile_definitions(tst_derivedwireuuid PRIVATE
  "QET_TEST_BINARY_PATH=\"$<TARGET_FILE:qelectrotech>\""
  "QET_EXAMPLES_DIR=\"${QET_DIR}/examples\"")

# Element numbering schemes carry an id and elements follow them by it:
# a legacy file gets stable ids on --resave; through --run, rename, edit,
# refused removal and undo/redo keep the elements following the scheme.
if(QET_HAS_SCRIPTING)
  add_executable(
    tst_elementautonumids
    tst_elementautonumids.cpp)
  add_test(NAME tst_elementautonumids COMMAND tst_elementautonumids)
  add_dependencies(tst_elementautonumids qelectrotech)
  target_link_libraries(tst_elementautonumids PRIVATE Qt::Test Qt::Xml)
  target_compile_definitions(tst_elementautonumids PRIVATE
    "QET_TEST_BINARY_PATH=\"$<TARGET_FILE:qelectrotech>\""
    "QET_EXAMPLES_DIR=\"${QET_DIR}/examples\"")
endif()

# Saving a project that was just saved changes nothing: runs the real
# binary's --resave twice on every project in examples/, and on one whose
# title block holds a single-space value (#973).

add_executable(
  tst_resaveunchanged
  tst_resaveunchanged.cpp)
add_test(NAME tst_resaveunchanged COMMAND tst_resaveunchanged)
add_dependencies(tst_resaveunchanged qelectrotech)
target_link_libraries(tst_resaveunchanged PRIVATE Qt::Test)
target_compile_definitions(tst_resaveunchanged PRIVATE
  "QET_TEST_BINARY_PATH=\"$<TARGET_FILE:qelectrotech>\""
  "QET_EXAMPLES_DIR=\"${QET_DIR}/examples\"")

# Replacing a symbol's definition in a project keeps its terminal uuids, so
# the wires saved against them still load: TerminalUuids::keep() on its own,
# then the real binary's --info on an example whose symbols were replaced.
add_executable(
  tst_terminaluuids
  tst_terminaluuids.cpp
  ${QET_DIR}/sources/ElementsCollection/terminaluuids.cpp)
add_test(NAME tst_terminaluuids COMMAND tst_terminaluuids)
add_dependencies(tst_terminaluuids qelectrotech)
target_link_libraries(tst_terminaluuids PRIVATE Qt::Test Qt::Xml)
target_compile_definitions(tst_terminaluuids PRIVATE
  "QET_TEST_BINARY_PATH=\"$<TARGET_FILE:qelectrotech>\""
  "QET_EXAMPLES_DIR=\"${QET_DIR}/examples\"")

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

# Qt 6.4.2 writes the attributes of an element in an order that changes from
# one process to the next, so a project saved twice by two runs of the binary
# differs in bytes only (seen on projects without any numbering, too). A fixed
# hash seed makes the order stable; it changes nothing for a newer Qt.
if(Qt6_VERSION VERSION_LESS 6.5.0)
  foreach(qet_test tst_resaveunchanged tst_elementautonumids)
    if(TEST ${qet_test})
      set_tests_properties(${qet_test} PROPERTIES ENVIRONMENT "QT_HASH_SEED=0")
    endif()
  endforeach()
endif()
