idf_component_register(SRCS
    "main.cxx"
    "display/display_driver.cxx"
    "display/adagfx.cxx"
    "display/font/FreeSerif24pt7b.cxx"
    "display/font/FreeSans24pt7b.cxx"
    "display/font/FreeSerif18pt7b.cxx"
    "display/font/FreeSans18pt7b.cxx"
    "display/font/FreeSerif12pt7b.cxx"
    "display/font/FreeSans12pt7b.cxx"
    "display/font/FreeSerif9pt7b.cxx"
    "display/font/FreeSans9pt7b.cxx"
    "display/icon.cxx"
    "view.cxx"
    "display_task.cxx"
    "network.cxx"
    "http2/sh2lib.c"
    "http2/http2_request.cxx"
    "http2/http2_connection.cxx"
    "sha512.cxx"
    INCLUDE_DIRS ".")

target_compile_options(${COMPONENT_LIB} PRIVATE -Wno-missing-field-initializers)
