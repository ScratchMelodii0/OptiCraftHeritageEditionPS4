# ps4_stage_data.cmake — copy the game data tree into the PS4 package root.
#
# Invoked by the ps4-pkg target with -DSRC=<repo>/data -DDST=<pkg>/data. A
# missing data/ is not fatal: the package still installs and the game reports
# the missing assets at runtime (they can also be copied to /data/opticraft
# over FTP afterwards, see README.md).
if(NOT IS_DIRECTORY "${SRC}")
    message(WARNING "ps4-pkg: ${SRC} does not exist; packaging without game data.")
    return()
endif()
foreach(_sub assets resources)
    if(IS_DIRECTORY "${SRC}/${_sub}")
        file(COPY "${SRC}/${_sub}" DESTINATION "${DST}")
    endif()
endforeach()
