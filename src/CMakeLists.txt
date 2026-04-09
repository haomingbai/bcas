add_library(bcas
  ${CMAKE_CURRENT_SOURCE_DIR}/client/cas_detail.cc
  ${CMAKE_CURRENT_SOURCE_DIR}/client/cas_client_task.cc
  ${CMAKE_CURRENT_SOURCE_DIR}/client/cas_proxy_task.cc
  ${CMAKE_CURRENT_SOURCE_DIR}/client/cas_proxy_callback.cc
)
add_library(bcas::bcas ALIAS bcas)

target_compile_features(bcas PUBLIC cxx_std_20)
target_link_libraries(bcas
  PUBLIC
    $<BUILD_INTERFACE:bsrvcore::bsrvcore>
    $<INSTALL_INTERFACE:bsrvcore::bsrvcore>
  PRIVATE
    ${BCAS_PUGIXML_TARGET}
)
target_include_directories(bcas
  PUBLIC
    $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
  PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
)

set_target_properties(bcas PROPERTIES
  VERSION ${PROJECT_VERSION}
  SOVERSION ${PROJECT_VERSION_MAJOR}
  EXPORT_NAME bcas
  ARCHIVE_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}/lib"
  LIBRARY_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}/lib"
  RUNTIME_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}/bin"
)

if(NOT WIN32)
  set_target_properties(bcas PROPERTIES POSITION_INDEPENDENT_CODE ON)
endif()

install(TARGETS bcas
  EXPORT bcasTargets
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
  ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
  INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

install(EXPORT bcasTargets
  FILE bcasTargets.cmake
  NAMESPACE bcas::
  DESTINATION ${BCAS_INSTALL_CONFIGDIR}
)

if(BCAS_BSRVCORE_RESOLUTION_MODE MATCHES "^config:")
  export(EXPORT bcasTargets
    FILE "${PROJECT_BINARY_DIR}/bcasTargets.cmake"
    NAMESPACE bcas::
  )
else()
  message(STATUS
    "bcas skipped build-tree export because bsrvcore came from "
    "${BCAS_BSRVCORE_RESOLUTION_MODE}"
  )
endif()
