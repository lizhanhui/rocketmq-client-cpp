add_library(client_obj OBJECT)
target_sources(client_obj
        PRIVATE
            ClientConfigImpl.cpp
            ClientManagerImpl.cpp
            ClientManagerFactory.cpp
            LogInterceptorFactory.cpp
            LogInterceptor.cpp
            TopicAssignmentInfo.cpp
            RpcClientImpl.cpp
            Signature.cpp
            TlsHelper.cpp
            TopicPublishInfo.cpp
            TracingUtility.cpp)
target_include_directories(client_obj
        PUBLIC
            ${CMAKE_BINARY_DIR}/generated/third_party/opentelemetry-proto
            ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(client_obj
        PUBLIC
            admin_library
            crc_library
            crypto
            scheduler_library
            ssl
            ThreadPool
        PRIVATE
            gtest)
if (ENABLE_TRACING)
    target_link_libraries(client_obj
        PUBLIC
            opentelemetry_trace
            opentelemetry_exporter_otprotocol)
endif ()

add_library(client_library
        STATIC
            $<TARGET_OBJECTS:client_obj>)

target_include_directories(client_library
        PUBLIC
            ${CMAKE_BINARY_DIR}/generated/third_party/opentelemetry-proto
            ${CMAKE_CURRENT_SOURCE_DIR}/include)

target_link_libraries(client_library
        PUBLIC
            admin_library
            crc_library
            crypto
            mq_proto_library
            scheduler_library
            ssl
            ThreadPool)
if (ENABLE_TRACING)
    target_link_libraries(client_library
        PUBLIC
            opentelemetry_trace
            opentelemetry_exporter_otprotocol)
endif ()

if (BUILD_TESTS)
    add_subdirectory(mocks)
endif()
