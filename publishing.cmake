set(POLICY_NAME "publishing")

string(REPLACE "_" "-" POLICY_NAME_HYPHENS ${POLICY_NAME})
set(IRODS_PACKAGE_COMPONENT_POLICY_NAME ${POLICY_NAME_HYPHENS})
set(TOUPPER IRODS_PACKAGE_COMPONENT_POLICY_NAME_UPPERCASE ${IRODS_PACKAGE_COMPONENT_POLICY_NAME})

set(TARGET_NAME "${IRODS_TARGET_NAME_PREFIX}-${POLICY_NAME}")

set(
  IRODS_PLUGIN_POLICY_COMPILE_DEFINITIONS
  IRODS_QUERY_ENABLE_SERVER_SIDE_API
  ENABLE_RE
  )

set(
  IRODS_PLUGIN_POLICY_LINK_LIBRARIES
  irods_server
  )

add_library(
    ${TARGET_NAME}
    MODULE
    ${CMAKE_SOURCE_DIR}/lib${TARGET_NAME}.cpp
    ${CMAKE_SOURCE_DIR}/configuration.cpp
    ${CMAKE_SOURCE_DIR}/plugin_specific_configuration.cpp
    ${CMAKE_SOURCE_DIR}/utilities.cpp
    ${CMAKE_SOURCE_DIR}/publishing_utilities.cpp
    )

target_include_directories(
    ${TARGET_NAME}
    PRIVATE
    ${IRODS_INCLUDE_DIRS}
    ${IRODS_EXTERNALS_FULLPATH_BOOST}/include
    ${IRODS_EXTERNALS_FULLPATH_FMT}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    )

target_link_libraries(
    ${TARGET_NAME}
    PRIVATE
    ${IRODS_PLUGIN_POLICY_LINK_LIBRARIES}
    ${IRODS_EXTERNALS_FULLPATH_BOOST}/lib/libboost_filesystem.so
    ${IRODS_EXTERNALS_FULLPATH_BOOST}/lib/libboost_system.so
    ${IRODS_EXTERNALS_FULLPATH_FMT}/lib/libfmt.so
    irods_common
    nlohmann_json::nlohmann_json
    )

target_compile_definitions(${TARGET_NAME} PRIVATE ${IRODS_PLUGIN_POLICY_COMPILE_DEFINITIONS} ${IRODS_COMPILE_DEFINITIONS} ${IRODS_COMPILE_DEFINITIONS_PRIVATE} BOOST_SYSTEM_NO_DEPRECATED)
target_compile_options(${TARGET_NAME} PRIVATE -Wno-write-strings)
set_property(TARGET ${TARGET_NAME} PROPERTY CXX_STANDARD ${IRODS_CXX_STANDARD})

install(
  TARGETS
  ${TARGET_NAME}
  LIBRARY
  DESTINATION ${IRODS_PLUGINS_DIRECTORY}/rule_engines
  COMPONENT ${IRODS_PACKAGE_COMPONENT_POLICY_NAME}
  )

install(
  FILES
  ${CMAKE_SOURCE_DIR}/packaging/test_plugin_publishing.py
  DESTINATION ${IRODS_HOME_DIRECTORY}/scripts/irods/test
  PERMISSIONS OWNER_READ OWNER_WRITE GROUP_READ WORLD_READ
  COMPONENT ${IRODS_PACKAGE_COMPONENT_POLICY_NAME}
  )

install(
  FILES
  ${CMAKE_SOURCE_DIR}/packaging/run_publishing_plugin_test.py
  DESTINATION ${IRODS_HOME_DIRECTORY}/scripts
  PERMISSIONS OWNER_READ OWNER_WRITE GROUP_READ WORLD_READ
  COMPONENT ${IRODS_PACKAGE_COMPONENT_POLICY_NAME}
  )

install(
  FILES
  ${CMAKE_SOURCE_DIR}/example_publishing_invocation.r
  DESTINATION ${IRODS_HOME_DIRECTORY}
  PERMISSIONS OWNER_READ OWNER_WRITE GROUP_READ WORLD_READ
  COMPONENT ${IRODS_PACKAGE_COMPONENT_POLICY_NAME}
  )

set(CPACK_DEBIAN_PACKAGE_CONTROL_EXTRA "${CMAKE_SOURCE_DIR}/packaging/postinst_tiering;")
set(CPACK_RPM_POST_INSTALL_SCRIPT_FILE "${CMAKE_SOURCE_DIR}/packaging/postinst_tiering")

set(CPACK_DEBIAN_${IRODS_PACKAGE_COMPONENT_POLICY_NAME_UPPERCASE}_PACKAGE_NAME ${TARGET_NAME})
set(CPACK_DEBIAN_${IRODS_PACKAGE_COMPONENT_POLICY_NAME_UPPERCASE}_PACKAGE_DEPENDS "${IRODS_PACKAGE_DEPENDENCIES_STRING}, irods-server (= ${IRODS_VERSION}), irods-runtime (= ${IRODS_VERSION}), libc6")

set(CPACK_RPM_${IRODS_PACKAGE_COMPONENT_POLICY_NAME}_PACKAGE_NAME ${TARGET_NAME})
if (IRODS_LINUX_DISTRIBUTION_NAME STREQUAL "centos" OR IRODS_LINUX_DISTRIBUTION_NAME STREQUAL "centos linux")
    set(CPACK_RPM_${IRODS_PACKAGE_COMPONENT_POLICY_NAME}_PACKAGE_REQUIRES "${IRODS_PACKAGE_DEPENDENCIES_STRING}, irods-server = ${IRODS_VERSION}, irods-runtime = ${IRODS_VERSION}")
elseif (IRODS_LINUX_DISTRIBUTION_NAME STREQUAL "opensuse")
    set(CPACK_RPM_${IRODS_PACKAGE_COMPONENT_POLICY_NAME}_PACKAGE_REQUIRES "${IRODS_PACKAGE_DEPENDENCIES_STRING}, irods-server = ${IRODS_VERSION}, irods-runtime = ${IRODS_VERSION}")
endif()

set(CPACK_DEBIAN_PACKAGE_BREAKS "irods-rule-engine-plugin-tiered-storage")
set(CPACK_DEBIAN_PACKAGE_REPLACES "irods-rule-engine-plugin-tiered-storage")
set(CPACK_RPM_PACKAGE_OBSOLETES "irods-rule-engine-plugin-tiered-storage")

