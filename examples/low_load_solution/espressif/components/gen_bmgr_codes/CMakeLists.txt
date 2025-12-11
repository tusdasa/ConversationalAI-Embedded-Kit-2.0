# Board information output
message(STATUS "Selected Board: echoear_core_board_v1_2")
message(STATUS "Board Path: managed_components/espressif__esp_board_manager/boards/echoear_core_board_v1_2")

idf_component_register(
    SRC_DIRS "." "../../managed_components/espressif__esp_board_manager//boards/echoear_core_board_v1_2"
    INCLUDE_DIRS "." "../../managed_components/espressif__esp_board_manager//boards/echoear_core_board_v1_2"
    REQUIRES esp_board_manager
)

# This is equivalent to adding WHOLE_ARCHIVE option to the idf_component_register call above:
idf_component_set_property(${COMPONENT_NAME} WHOLE_ARCHIVE TRUE)
