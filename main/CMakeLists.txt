idf_component_register(SRCS
    "main.cxx"
    "display_driver.cxx"
    "adagfx.cxx"
    "font/FreeSerif24pt7b.cxx"
    "font/FreeSans24pt7b.cxx"
    "font/FreeSerif18pt7b.cxx"
    "font/FreeSans18pt7b.cxx"
    "font/FreeSerif12pt7b.cxx"
    "font/FreeSans12pt7b.cxx"
    "font/FreeSerif9pt7b.cxx"
    "font/FreeSans9pt7b.cxx"
    INCLUDE_DIRS ".")

target_compile_options(${COMPONENT_LIB} PRIVATE -Wno-missing-field-initializers)
