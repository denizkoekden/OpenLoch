# PCB module ("Leiterplatte"): own board model, editor widget, Sprint-Layout files, a demo program and its tests.
# Included from the root CMakeLists.txt, so paths are taken from this file's directory.
get_filename_component(OPENLOCH_PCB_SRC "${CMAKE_CURRENT_LIST_DIR}/../.." ABSOLUTE)
get_filename_component(OPENLOCH_PCB_ROOT "${OPENLOCH_PCB_SRC}/.." ABSOLUTE)
set(OPENLOCH_PCB_DIR "${OPENLOCH_PCB_SRC}/modules/pcb")
add_library(openloch_pcb STATIC
    ${OPENLOCH_PCB_DIR}/model.cpp
    ${OPENLOCH_PCB_DIR}/copper.cpp
    ${OPENLOCH_PCB_DIR}/shapes.cpp
    ${OPENLOCH_PCB_DIR}/font.cpp
    ${OPENLOCH_PCB_DIR}/footprints.cpp
    ${OPENLOCH_PCB_DIR}/example.cpp
    ${OPENLOCH_PCB_DIR}/pcbicons.cpp
    ${OPENLOCH_PCB_DIR}/draw.cpp ${OPENLOCH_PCB_DIR}/printing.cpp ${OPENLOCH_PCB_DIR}/fabrication.cpp ${OPENLOCH_PCB_DIR}/milling.cpp ${OPENLOCH_PCB_DIR}/gerberimport.cpp ${OPENLOCH_PCB_DIR}/outputs.cpp ${OPENLOCH_PCB_DIR}/view.cpp ${OPENLOCH_PCB_DIR}/overview.cpp ${OPENLOCH_PCB_DIR}/pictures.cpp ${OPENLOCH_PCB_DIR}/macropanel.cpp ${OPENLOCH_PCB_DIR}/netcheck.cpp
    ${OPENLOCH_PCB_DIR}/editor.cpp
    ${OPENLOCH_PCB_SRC}/formats/sprint/sprint.cpp ${OPENLOCH_PCB_SRC}/formats/sprint/textio.cpp)
target_link_libraries(openloch_pcb PUBLIC openloch_core Qt6::Widgets Qt6::PrintSupport)
add_executable(openloch_pcb_demo ${OPENLOCH_PCB_DIR}/demo.cpp)
target_link_libraries(openloch_pcb_demo PRIVATE openloch_pcb)
add_executable(openloch_pcb_tests ${OPENLOCH_PCB_ROOT}/tests/pcb_tests.cpp ${OPENLOCH_PCB_ROOT}/tests/pcb_editor_tests.cpp ${OPENLOCH_PCB_ROOT}/tests/pcb_editing_tests.cpp ${OPENLOCH_PCB_ROOT}/tests/pcb_preferences_tests.cpp ${OPENLOCH_PCB_ROOT}/tests/pcb_pictures_tests.cpp
    ${OPENLOCH_PCB_ROOT}/tests/pcb_fabrication_tests.cpp ${OPENLOCH_PCB_ROOT}/tests/pcb_schematic_tests.cpp)
target_link_libraries(openloch_pcb_tests PRIVATE openloch_pcb)
target_compile_definitions(openloch_pcb_tests PRIVATE OPENLOCH_SOURCE_DIR="${OPENLOCH_PCB_ROOT}")
add_test(NAME pcb COMMAND openloch_pcb_tests)
set_tests_properties(pcb PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen" TIMEOUT 600)
