# ==============================================================================
# Advanced Module System (AMS) CMake File
# ==============================================================================

# include subdirectories -------------------------------------------------------

INSTALL(DIRECTORY
            etc
        DESTINATION
            share/etc
        )

INSTALL(DIRECTORY
            systemd
        DESTINATION
            share/systemd
        )


