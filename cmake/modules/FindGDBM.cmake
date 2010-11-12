# find include directory
find_path(GDBM_INCLUDE_DIR gdbm/ndbm.h)
find_library(GDBM_LIBRARY NAMES libgdbm gdbm ndbm)
find_library(GDBM_LIBRARY_COMPAT NAMES libgdm_compat gdbm_compat ndbm_compat)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(GDBM DEFAULT_MSG GDBM_LIBRARY GDBM_INCLUDE_DIR)
