
idf_component_register(
    INCLUDE_DIRS "include" "./"
    PRIV_INCLUDE_DIRS "private_include"
    SRC_DIRS "./" 
    REQUIRES esp_capture esp_video_codec
)
