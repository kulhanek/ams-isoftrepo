# ==============================================================================
# Advanced Module System (AMS) CMake File
# ==============================================================================

# include subdirectories -------------------------------------------------------

INSTALL(DIRECTORY
            fcgi-bin
        DESTINATION
            var/html/isoftrepo
        )

INSTALL(DIRECTORY
            images
        DESTINATION
            var/html/isoftrepo
        FILES_MATCHING PATTERN
            "*.png"
        )

INSTALL(DIRECTORY
            scripts
        DESTINATION
            var/html/isoftrepo
        FILES_MATCHING PATTERN
            "*.js"
        )

INSTALL(DIRECTORY
            styles
        DESTINATION
            var/html/isoftrepo
        FILES_MATCHING PATTERN
            "*.css"
        )

INSTALL(DIRECTORY
            templates
        DESTINATION
            var/html/isoftrepo
        FILES_MATCHING PATTERN
            "*.html"
        )

