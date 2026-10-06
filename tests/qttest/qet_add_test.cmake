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


# qet_add_test(): how every test in this directory is built, included by
# its CMakeLists.txt.

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
