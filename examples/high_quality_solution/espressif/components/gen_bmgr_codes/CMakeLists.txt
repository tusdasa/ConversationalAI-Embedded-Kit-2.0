# Board information output
message(STATUS "Selected Board: esp_vocat_board_v1_2")
message(STATUS "Board Path: /Users/bytedance/Desktop/work/esp-adf/examples/conversation_ai/examples/high_quality_solution/espressif/managed_components/espressif__esp_board_manager/boards/esp_vocat_board_v1_2")

idf_component_register(
    SRC_DIRS "." "../../managed_components/espressif__esp_board_manager/boards/esp_vocat_board_v1_2"
    INCLUDE_DIRS "." "../../managed_components/espressif__esp_board_manager/boards/esp_vocat_board_v1_2"
    REQUIRES esp_board_manager
)

# This is equivalent to adding WHOLE_ARCHIVE option to the idf_component_register call above:
idf_component_set_property(${COMPONENT_NAME} WHOLE_ARCHIVE TRUE)
