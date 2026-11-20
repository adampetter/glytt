set(SOURCES
    source/watcher.cpp
    source/app.cpp)

idf_component_register(SRCS ${SOURCES}
                    INCLUDE_DIRS . include
                    REQUIRES api)
