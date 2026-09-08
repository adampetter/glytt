set(SOURCES
    source/tailer.cpp
    source/app.cpp
    source/cli.cpp)

idf_component_register(SRCS ${SOURCES}
                    INCLUDE_DIRS . include
                    REQUIRES api)