add_library(bcas
  ${CMAKE_CURRENT_SOURCE_DIR}/client/cas_detail.cc
  ${CMAKE_CURRENT_SOURCE_DIR}/client/cas_client_task.cc
  ${CMAKE_CURRENT_SOURCE_DIR}/client/cas_proxy_task.cc
  ${CMAKE_CURRENT_SOURCE_DIR}/client/cas_proxy_callback.cc
)

if(BCAS_PUGIXML_SOURCE_ROOT)
  target_sources(bcas PRIVATE
    ${BCAS_PUGIXML_SOURCE_ROOT}/src/pugixml.cpp
  )
endif()

add_library(bcas::bcas ALIAS bcas)

target_compile_features(bcas PUBLIC cxx_std_20)
target_link_libraries(bcas
  PUBLIC
    $<BUILD_INTERFACE:bsrvcore::bsrvcore>
    $<INSTALL_INTERFACE:bsrvcore::bsrvcore>
)
target_include_directories(bcas
  PUBLIC
    $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
  PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
)

if(BCAS_PUGIXML_SOURCE_ROOT)
  target_include_directories(bcas PRIVATE
    ${BCAS_PUGIXML_SOURCE_ROOT}/src
  )
elseif(BUILD_SHARED_LIBS)
  target_link_libraries(bcas PRIVATE ${BCAS_PUGIXML_TARGET})
elseif(TARGET ${BCAS_PUGIXML_TARGET})
  message(FATAL_ERROR
    "Static bcas builds require pugixml source availability so pugixml can be "
    "embedded privately. Use BCAS_PUGIXML_SOURCE_DIR or BCAS_FETCH_PUGIXML."
  )
endif()

bcas_apply_project_warnings(bcas)

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
