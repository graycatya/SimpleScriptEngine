set(PROJECT_NAMES "luajit-rocks" 
                "angelscript"  
                "quickjs"
                "ChaiScript"
                "miniz"
)

set(luajit-rocks_VERSION_TAG "master")
set(luajit-rocks_URL "https://github.com/torch/luajit-rocks/archive/refs/heads/${luajit-rocks_VERSION_TAG}.zip")

set(angelscript_VERSION_TAG "2.38.0")
set(angelscript_URL "https://github.com/anjo76/angelscript/archive/refs/tags/v${angelscript_VERSION_TAG}.zip")

set(quickjs_VERSION_TAG "0.15.1")
set(quickjs_URL "https://github.com/quickjs-ng/quickjs/archive/refs/tags/v${quickjs_VERSION_TAG}.zip")

set(ChaiScript_VERSION_TAG "6.1.0")
set(ChaiScript_URL "https://github.com/ChaiScript/ChaiScript/archive/refs/tags/v${ChaiScript_VERSION_TAG}.zip")

set(miniz_VERSION_TAG "3.1.2")
set(miniz_URL "https://github.com/richgel999/miniz/archive/refs/tags/${miniz_VERSION_TAG}.zip")



foreach(PROJ_NAME IN LISTS PROJECT_NAMES)
    set(URL_VAR "${PROJ_NAME}_URL")
    set(VERSION_VAR "${PROJ_NAME}_VERSION_TAG")
    

    if (NOT DEFINED ${URL_VAR} OR NOT DEFINED ${VERSION_VAR})
        message(WARNING "Project ${PROJ_NAME} has no URL or version configured, skipping!")
        continue()
    endif()


    set(PROJ_URL "${${URL_VAR}}")
    set(PROJ_VERSION "${${VERSION_VAR}}")


    message(STATUS "========================")
    message(STATUS "Project Name: ${PROJ_NAME}")
    message(STATUS "Download URL: ${PROJ_URL}")
    message(STATUS "Version: ${PROJ_VERSION}")
    download_and_unzip(
        PROJECT_NAME        "${PROJ_NAME}"
        PROJECT_VERSION     "${PROJ_VERSION}"
        DOWNLOAD_URL        "${PROJ_URL}"
        ZIP_SAVE_PATH      "${3RDPARTY_ZIP_PATH}/${PROJ_NAME}-${PROJ_VERSION}.zip"
        UNZIP_TARGET_DIR   "${3RDPARTY_PATH}"
        RENAME_TO           "${PROJ_NAME}" 
        DOWNLOAD_RETRY      3
        DOWNLOAD_TIMEOUT    600
    )
    message(STATUS "========================")
endforeach()