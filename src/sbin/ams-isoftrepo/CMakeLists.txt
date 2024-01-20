# ==============================================================================
# InfiCore CMake File
# ==============================================================================

# program objects --------------------------------------------------------------
SET(PROG_SRC
        ISoftRepoOptions.cpp
        ISoftRepoServer.cpp
        _ListCategories.cpp
        _Module.cpp
        _Version.cpp
        _Build.cpp
        _Error.cpp
        )

# final build ------------------------------------------------------------------
ADD_EXECUTABLE(ams-isoftrepo ${PROG_SRC})

TARGET_LINK_LIBRARIES(ams-isoftrepo ${AMS_FB_LIBS})

INSTALL(TARGETS
            ams-isoftrepo
        RUNTIME DESTINATION
            sbin
        )

