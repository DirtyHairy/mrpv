idf_component_register(SRCS "main.cxx" "display_driver.cxx"
    INCLUDE_DIRS ".")

target_compile_options(${COMPONENT_LIB} PRIVATE -Wno-missing-field-initializers)
