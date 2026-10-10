# Front panel module ("Frontplatte"): own model, native format, FrontDesigner FPL import/export, editor widget.
# Included from the root CMakeLists.txt; paths are relative to this file, the repository root is CMAKE_CURRENT_SOURCE_DIR.
set(OPENLOCH_FRONTPANEL_DIR ${CMAKE_CURRENT_LIST_DIR})
set(OPENLOCH_FRONTDESIGNER_DIR ${CMAKE_CURRENT_LIST_DIR}/../../formats/frontdesigner)
add_library(openloch_frontpanel
    ${OPENLOCH_FRONTPANEL_DIR}/frontpanel.cpp
    ${OPENLOCH_FRONTPANEL_DIR}/panelboards.cpp
    ${OPENLOCH_FRONTPANEL_DIR}/panelgeometry.cpp
    ${OPENLOCH_FRONTPANEL_DIR}/panelrender.cpp
    ${OPENLOCH_FRONTPANEL_DIR}/panelhistory.cpp
    ${OPENLOCH_FRONTPANEL_DIR}/emf.cpp
    ${OPENLOCH_FRONTPANEL_DIR}/strokefont.cpp
    ${OPENLOCH_FRONTPANEL_DIR}/strokefontown.cpp
    ${OPENLOCH_FRONTPANEL_DIR}/panelgenerators.cpp
    ${OPENLOCH_FRONTPANEL_DIR}/panelscale.cpp
    ${OPENLOCH_FRONTPANEL_DIR}/panelmachining.cpp
    ${OPENLOCH_FRONTPANEL_DIR}/panelicons.cpp
    ${OPENLOCH_FRONTPANEL_DIR}/paneldialogs.cpp
    ${OPENLOCH_FRONTPANEL_DIR}/panelwizards.cpp
    ${OPENLOCH_FRONTPANEL_DIR}/panelprint.cpp
    ${OPENLOCH_FRONTPANEL_DIR}/panelview.cpp
    ${OPENLOCH_FRONTPANEL_DIR}/panelsidebar.cpp
    ${OPENLOCH_FRONTPANEL_DIR}/paneleditor.cpp
    ${OPENLOCH_FRONTPANEL_DIR}/paneleditorboards.cpp
    ${OPENLOCH_FRONTDESIGNER_DIR}/delphistream.cpp
    ${OPENLOCH_FRONTDESIGNER_DIR}/inifile.cpp
    ${OPENLOCH_FRONTDESIGNER_DIR}/fpl.cpp
    ${OPENLOCH_FRONTDESIGNER_DIR}/x87.cpp
    ${OPENLOCH_FRONTDESIGNER_DIR}/fdplot.cpp
    ${OPENLOCH_FRONTDESIGNER_DIR}/fdshapes.cpp)
target_include_directories(openloch_frontpanel PUBLIC ${OPENLOCH_FRONTPANEL_DIR} ${OPENLOCH_FRONTDESIGNER_DIR})
target_link_libraries(openloch_frontpanel PUBLIC openloch_core Qt6::Gui Qt6::Widgets Qt6::PrintSupport)
# The module's help pages (Hilfe → Hilfethemen…, F1), at qrc:/help/frontpanel/<language>.html.
qt_add_resources(openloch_frontpanel "frontpanel_help" PREFIX "/" BASE ${CMAKE_CURRENT_SOURCE_DIR}
    FILES ${CMAKE_CURRENT_SOURCE_DIR}/help/frontpanel/de.html ${CMAKE_CURRENT_SOURCE_DIR}/help/frontpanel/en.html ${CMAKE_CURRENT_SOURCE_DIR}/help/frontpanel/fr.html)

# A small stand-alone program around the editor until the suite's start screen takes it over.
qt_add_executable(openloch_frontpanel_demo ${OPENLOCH_FRONTPANEL_DIR}/demo.cpp)
target_link_libraries(openloch_frontpanel_demo PRIVATE openloch_frontpanel)

add_executable(openloch_frontpanel_tests ${CMAKE_CURRENT_SOURCE_DIR}/tests/frontpanel_tests.cpp ${CMAKE_CURRENT_SOURCE_DIR}/tests/frontpanel_fpl.cpp ${CMAKE_CURRENT_SOURCE_DIR}/tests/frontpanel_editor.cpp ${CMAKE_CURRENT_SOURCE_DIR}/tests/frontpanel_strokefont.cpp ${CMAKE_CURRENT_SOURCE_DIR}/tests/frontpanel_scale.cpp ${CMAKE_CURRENT_SOURCE_DIR}/tests/frontpanel_emf.cpp ${CMAKE_CURRENT_SOURCE_DIR}/tests/frontpanel_hpgl.cpp)
target_link_libraries(openloch_frontpanel_tests PRIVATE openloch_frontpanel)
target_compile_definitions(openloch_frontpanel_tests PRIVATE OPENLOCH_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}")
add_test(NAME frontpanel COMMAND openloch_frontpanel_tests)
set_tests_properties(frontpanel PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen" TIMEOUT 600)
