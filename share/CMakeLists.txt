# ==============================================================================
# Advanced Module System (AMS) CMake File
# ==============================================================================

# include subdirectories -------------------------------------------------------

INSTALL(DIRECTORY
            etc
        DESTINATION
            share/
        )

INSTALL(DIRECTORY
            systemd
        DESTINATION
            share/
        )


