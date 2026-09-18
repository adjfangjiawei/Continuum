cmake_minimum_required(VERSION 3.24)

project(
    Continuum
    VERSION 0.1.0
    DESCRIPTION "Continuum local project memory forensics desktop application"
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(CONTINUUM_WARNINGS_AS_ERRORS "Treat compiler warnings as errors" OFF)

find_package(wxWidgets REQUIRED COMPONENTS core base)
find_package(SQLite3 REQUIRED)
include(${wxWidgets_USE_FILE})

add_executable(
    Continuum
    WIN32
    src/main.cpp
    src/App.cpp
    src/MainFrame.cpp
    src/Pages.cpp
    src/EvidencePages.cpp
    src/LedgerPages.cpp
    src/InvestigationPages.cpp
    src/DeliveryPages.cpp
    src/OperationsPages.cpp
    src/GovernancePages.cpp
    src/SystemPages.cpp
    src/ApplicationDialogs.cpp
    src/AdvancedDialogs.cpp
    src/SystemDialogs.cpp
    src/Storage.cpp
    src/WorkspaceService.cpp
    src/FileScanner.cpp
    src/ContentParser.cpp
    src/SearchService.cpp
    src/BackgroundWorker.cpp
    src/BackupService.cpp
    src/ExternalDocumentService.cpp
    src/SecurityService.cpp
    src/UiDataService.cpp
    src/WxUiBinder.cpp
    src/WxRecordController.cpp
    src/UiEventBus.cpp
)

target_include_directories(
    Continuum
    PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(
    Continuum
    PRIVATE
    ${wxWidgets_LIBRARIES}
)

if(MSVC)
    target_compile_options(
        Continuum
        PRIVATE
        /W4
        /permissive-
        /utf-8
        /EHsc
    )

    if(CONTINUUM_WARNINGS_AS_ERRORS)
        target_compile_options(Continuum PRIVATE /WX)
    endif()
else()
    target_compile_options(
        Continuum
        PRIVATE
        -Wall
        -Wextra
        -Wpedantic
        -fno-char8_t
    )

    if(CONTINUUM_WARNINGS_AS_ERRORS)
        target_compile_options(Continuum PRIVATE -Werror)
    endif()
endif()

set_target_properties(
    Continuum
    PROPERTIES
    OUTPUT_NAME "Continuum"
)

install(
    TARGETS Continuum
    RUNTIME DESTINATION bin
)

target_link_libraries(Continuum PRIVATE SQLite::SQLite3)

# CONTINUUM_WINDOWS_CRYPTO_LIBRARIES
if(WIN32)
    target_link_libraries(Continuum PRIVATE Crypt32 Bcrypt)
endif()
