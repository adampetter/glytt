set(SOURCES
    source/app_tailer.cpp)

idf_component_register(SRCS ${SOURCES}
                    INCLUDE_DIRS . include
                    REQUIRES api)