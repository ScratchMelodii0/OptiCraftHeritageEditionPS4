# ps4_make_pkg.cmake — build <CONTENT_ID>.pkg from the staged package root.
#
# create-gp4 wants the explicit list of files, relative to the package root, so
# the tree is globbed here at build time rather than at configure time.
file(GLOB_RECURSE _files RELATIVE "${PKG_ROOT}" "${PKG_ROOT}/*")
list(FILTER _files EXCLUDE REGEX "\\.gp4$")
if(NOT "eboot.bin" IN_LIST _files)
    message(FATAL_ERROR "ps4-pkg: ${PKG_ROOT}/eboot.bin missing; build the OptiCraft target with create-fself available.")
endif()
string(JOIN " " _file_list ${_files})

execute_process(
    COMMAND "${CREATE_GP4}" -out pkg.gp4 "--content-id=${CONTENT_ID}" --files "${_file_list}"
    WORKING_DIRECTORY "${PKG_ROOT}"
    RESULT_VARIABLE _rc)
if(NOT _rc EQUAL 0)
    message(FATAL_ERROR "create-gp4 failed (${_rc})")
endif()

execute_process(
    COMMAND "${PKGTOOL}" pkg_build pkg.gp4 "${OUT_DIR}"
    WORKING_DIRECTORY "${PKG_ROOT}"
    RESULT_VARIABLE _rc)
if(NOT _rc EQUAL 0)
    message(FATAL_ERROR "PkgTool pkg_build failed (${_rc})")
endif()
message(STATUS "ps4-pkg: ${OUT_DIR}/${CONTENT_ID}.pkg")
