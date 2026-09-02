idf_component_register(
    SRCS "app_main.cpp" "ili9488_display.cpp"
    INCLUDE_DIRS "." "../../steamcore/include"
    REQUIRES driver
)
