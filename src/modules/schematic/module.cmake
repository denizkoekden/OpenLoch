# Schematic module ("Schaltplan"): own model and file format, nets, sPlan files, editor widget, a demo program and its tests.
# Included from the root CMakeLists.txt; paths are relative to this file, the repository root is CMAKE_CURRENT_SOURCE_DIR.
set(OPENLOCH_SCHEMATIC_DIR ${CMAKE_CURRENT_LIST_DIR})
set(OPENLOCH_SPLAN_DIR ${CMAKE_CURRENT_LIST_DIR}/../../formats/splan)
add_library(openloch_schematic STATIC
    ${OPENLOCH_SCHEMATIC_DIR}/model.cpp
    ${OPENLOCH_SCHEMATIC_DIR}/text.cpp
    ${OPENLOCH_SCHEMATIC_DIR}/dimension.cpp
    ${OPENLOCH_SCHEMATIC_DIR}/file.cpp
    ${OPENLOCH_SCHEMATIC_DIR}/nets.cpp
    ${OPENLOCH_SCHEMATIC_DIR}/numbering.cpp
    ${OPENLOCH_SCHEMATIC_DIR}/partslist.cpp
    ${OPENLOCH_SCHEMATIC_DIR}/print.cpp
    ${OPENLOCH_SCHEMATIC_DIR}/search.cpp
    ${OPENLOCH_SCHEMATIC_DIR}/images.cpp
    ${OPENLOCH_SCHEMATIC_DIR}/shapes.cpp
    ${OPENLOCH_SCHEMATIC_DIR}/render.cpp
    ${OPENLOCH_SCHEMATIC_DIR}/svg.cpp
    ${OPENLOCH_SCHEMATIC_DIR}/emf.cpp
    ${OPENLOCH_SCHEMATIC_DIR}/zip.cpp
    ${OPENLOCH_SCHEMATIC_DIR}/settings.cpp
    ${OPENLOCH_SCHEMATIC_DIR}/example.cpp
    ${OPENLOCH_SCHEMATIC_DIR}/library.cpp
    ${OPENLOCH_SCHEMATIC_DIR}/schematicicons.cpp
    ${OPENLOCH_SCHEMATIC_DIR}/view.cpp
    ${OPENLOCH_SCHEMATIC_DIR}/properties.cpp
    ${OPENLOCH_SCHEMATIC_DIR}/dialogs.cpp
    ${OPENLOCH_SCHEMATIC_DIR}/editor.cpp
    ${OPENLOCH_SPLAN_DIR}/inflate.cpp
    ${OPENLOCH_SPLAN_DIR}/records.cpp
    ${OPENLOCH_SPLAN_DIR}/splan.cpp)
target_link_libraries(openloch_schematic PUBLIC openloch_core Qt6::Gui Qt6::Widgets Qt6::PrintSupport)
# The library pages that come with the module (libraries/schematic, written by its generate.py).
file(GLOB OPENLOCH_SCHEMATIC_LIBRARY_FILES RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/libraries/schematic/*.json)
qt_add_resources(openloch_schematic "schematic_libraries" PREFIX "/" FILES ${OPENLOCH_SCHEMATIC_LIBRARY_FILES})
# The module's help pages (Hilfe → Hilfethemen…, F1), at qrc:/help/schematic/<language>.html.
qt_add_resources(openloch_schematic "schematic_help" PREFIX "/help/schematic" BASE ${OPENLOCH_SCHEMATIC_DIR}/help
    FILES ${OPENLOCH_SCHEMATIC_DIR}/help/de.html ${OPENLOCH_SCHEMATIC_DIR}/help/en.html ${OPENLOCH_SCHEMATIC_DIR}/help/fr.html)

# A small stand-alone program around the editor until the suite's start screen takes it over.
qt_add_executable(openloch_schematic_demo ${OPENLOCH_SCHEMATIC_DIR}/demo.cpp)
target_link_libraries(openloch_schematic_demo PRIVATE openloch_schematic)

add_executable(openloch_schematic_tests ${CMAKE_CURRENT_SOURCE_DIR}/tests/schematic_tests.cpp ${CMAKE_CURRENT_SOURCE_DIR}/tests/schematic_editor_tests.cpp ${CMAKE_CURRENT_SOURCE_DIR}/tests/schematic_splan_tests.cpp)
target_link_libraries(openloch_schematic_tests PRIVATE openloch_schematic)
target_compile_definitions(openloch_schematic_tests PRIVATE OPENLOCH_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}")
add_test(NAME schematic COMMAND openloch_schematic_tests)
set_tests_properties(schematic PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen" TIMEOUT 600)
