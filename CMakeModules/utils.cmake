macro(copy_bin file)
   add_custom_command(TARGET ${file} POST_BUILD COMMAND mkdir -p ${CMAKE_BINARY_DIR}/bin)
   add_custom_command(TARGET ${file} POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_BINARY_DIR}/${file} ${CMAKE_BINARY_DIR}/bin/)
endmacro()

# Set variable ${out_semver} to a semantic version taken from a git tag.
# Set variable ${out_dirty} to a current (probably dirty) git tag.
#
# Semantic version is a 3-component string "X.Y.Z".
#
# Example:
#
#   get_version_from_git(semver dirty)
#   message(STATUS "semver = ${semver}, dirty git version = ${dirty}")
#
function(get_version_from_git out_semver out_dirty)
  execute_process(
    COMMAND         git describe --tags --dirty
    OUTPUT_VARIABLE git_tag_raw
    OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_QUIET
  )

  string(REGEX REPLACE "^v([0-9]+\\.[0-9]+\\.[0-9]+).*" "\\1" semver "${git_tag_raw}")

  # return
  set(${out_semver} "${semver}" PARENT_SCOPE)
  set(${out_dirty} "${git_tag_raw}" PARENT_SCOPE)
endfunction()

#
# get_version_component("11.22.33" 0 x) # => x = 11
# get_version_component("11.22.33" 1 x) # => x = 22
# get_version_component("11.22.33" 2 x) # => x = 33
#
function(get_version_component semantic_version component_index out)
  string(REPLACE "." ";" version_list "${semantic_version}")
  list(GET version_list "${component_index}" component)

  # return
  set(${out} "${component}" PARENT_SCOPE)
endfunction()

#
# Detect traits for the curent project depending on the root directory name.
#
# * ${out_prj_name}       = { "daobet" | "haya" | "EOSIO" }
# * ${out_core_sym_name}  = { "SYS" | "BET" }
# * ${out_core_sym_name}  = "https://github.com/..."
# * ...
#
function(get_current_project_conf
    out_prj_name        # project name
    out_core_sym_name   # core symbol name
    out_prj_url         # repository URL
    out_pkg_name        # OS package: application name
    out_pkg_vendor      # OS package: application vendor
    out_pkg_contact     # OS package: application contact (email, etc.)
)
  # cannon use PROJECT_SOURCE_DIR here as the file is sourced before calling project() command
  get_filename_component(root_dir "${CMAKE_CURRENT_SOURCE_DIR}" NAME)
  string(TOLOWER "${root_dir}" root_dir)
  message(STATUS "root_dir = ${root_dir}")
  if(root_dir MATCHES "^daobet")
    set(${out_prj_name}       "daobet"                              PARENT_SCOPE)
    set(${out_core_sym_name}  "BET"                                 PARENT_SCOPE)
    set(${out_prj_url}        "https://github.com/DaoCasino/DAObet" PARENT_SCOPE)
    set(${out_pkg_name}       "DAOBet"                              PARENT_SCOPE)
    set(${out_pkg_vendor}     "DAOBet"                              PARENT_SCOPE)
    set(${out_pkg_contact}    "info@dao.group"                      PARENT_SCOPE)
  elseif(root_dir MATCHES "^haya")
    set(${out_prj_name}       "haya"                                PARENT_SCOPE)
    set(${out_core_sym_name}  "SYS"                                 PARENT_SCOPE)
    set(${out_prj_url}        "https://github.com/mixbytes/haya"    PARENT_SCOPE)
    set(${out_pkg_name}       "haya"                                PARENT_SCOPE) # TODO: update
    set(${out_pkg_vendor}     "mixbytes"                            PARENT_SCOPE)
    set(${out_pkg_contact}    "hello@mixbytes.io"                   PARENT_SCOPE)
  elseif(root_dir MATCHES "^eos")
    set(${out_prj_name}       "EOSIO"                               PARENT_SCOPE)
    set(${out_core_sym_name}  "SYS"                                 PARENT_SCOPE)
    set(${out_prj_url}        "https://github.com/EOSIO/eos"        PARENT_SCOPE)
    set(${out_pkg_name}       "EOS.IO"                              PARENT_SCOPE)
    set(${out_pkg_vendor}     "block.one"                           PARENT_SCOPE)
    set(${out_pkg_contact}    "support@block.one"                   PARENT_SCOPE)
  else()
    message(FATAL_ERROR "Sorry, could not detect project by its root directory name: ${root_dir}")
  endif()
endfunction()
