# 通用的下载并解压文件的 CMake 函数（兼容低版本 CMake）
# 参数说明:
#   PROJECT_NAME       - 项目名称 (必填)
#   PROJECT_VERSION    - 项目版本号 (必填)
#   DOWNLOAD_URL       - 文件下载地址 (必填)
#   ZIP_SAVE_PATH      - 压缩包保存路径 (可选，默认: ${CMAKE_BINARY_DIR}/download/${PROJECT_NAME}-${PROJECT_VERSION}.zip)
#   UNZIP_TARGET_DIR   - 解压目标目录 (可选，默认: ${CMAKE_BINARY_DIR})
#   DOWNLOAD_RETRY     - 下载重试次数 (可选，默认: 3)
#   DOWNLOAD_TIMEOUT   - 下载超时时间(秒) (可选，默认: 60)
#   RENAME_TO          - 解压后重命名的目标文件名 (可选，不填则不重命名)
function(download_and_unzip)
    # 解析函数参数（新增 RENAME_TO 参数）
    set(options "")
    set(oneValueArgs PROJECT_NAME PROJECT_VERSION DOWNLOAD_URL ZIP_SAVE_PATH UNZIP_TARGET_DIR DOWNLOAD_RETRY DOWNLOAD_TIMEOUT RENAME_TO)
    set(multiValueArgs "")
    cmake_parse_arguments(DU "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})

    # 检查必填参数
    if(NOT DU_PROJECT_NAME)
        message(FATAL_ERROR "PROJECT_NAME is a required parameter for download_and_unzip function!")
    endif()
    if(NOT DU_PROJECT_VERSION)
        message(FATAL_ERROR "PROJECT_VERSION is a required parameter for download_and_unzip function!")
    endif()
    if(NOT DU_DOWNLOAD_URL)
        message(FATAL_ERROR "DOWNLOAD_URL is a required parameter for download_and_unzip function!")
    endif()

    # 设置默认值
    if(NOT DU_ZIP_SAVE_PATH)
        set(DU_ZIP_SAVE_PATH "${CMAKE_BINARY_DIR}/download/${DU_PROJECT_NAME}-${DU_PROJECT_VERSION}.zip")
    endif()
    if(NOT DU_UNZIP_TARGET_DIR)
        set(DU_UNZIP_TARGET_DIR "${CMAKE_BINARY_DIR}/${DU_PROJECT_NAME}-${DU_PROJECT_VERSION}")
    endif()
    if(NOT DU_DOWNLOAD_RETRY)
        set(DU_DOWNLOAD_RETRY 3)
    endif()
    if(NOT DU_DOWNLOAD_TIMEOUT)
        set(DU_DOWNLOAD_TIMEOUT 60)
    endif()

    # 1. Create download directory
    get_filename_component(zip_dir "${DU_ZIP_SAVE_PATH}" DIRECTORY)
    file(MAKE_DIRECTORY "${zip_dir}")

    # 2. 下载压缩包（自定义重试逻辑，兼容低版本 CMake）
    #    增加下载后文件有效性校验，避免下载到损坏/不完整的文件
    set(need_download TRUE)
    if(EXISTS "${DU_ZIP_SAVE_PATH}")
        # 已存在的文件检查是否有效（大小 > 1KB，排除空文件/HTML 错误页）
        file(SIZE "${DU_ZIP_SAVE_PATH}" existing_size)
        if(existing_size GREATER 1024)
            message(STATUS "${DU_PROJECT_NAME} ${DU_PROJECT_VERSION} package already exists, skip downloading")
            set(need_download FALSE)
        else()
            message(WARNING "${DU_PROJECT_NAME} ${DU_PROJECT_VERSION} existing package is too small (${existing_size} bytes), re-downloading...")
            file(REMOVE "${DU_ZIP_SAVE_PATH}")
        endif()
    endif()

    if(need_download)
        set(download_success FALSE)
        set(retry_count 0)
        
        # 循环重试下载
        while(retry_count LESS ${DU_DOWNLOAD_RETRY} AND NOT download_success)
            math(EXPR retry_count_plus "${retry_count} + 1")
            message(STATUS "Downloading ${DU_PROJECT_NAME} ${DU_PROJECT_VERSION} (Attempt ${retry_count_plus}/${DU_DOWNLOAD_RETRY}) ...")
            
            file(DOWNLOAD
                ${DU_DOWNLOAD_URL}
                ${DU_ZIP_SAVE_PATH}
                SHOW_PROGRESS
                LOG download_log
                TIMEOUT ${DU_DOWNLOAD_TIMEOUT}  # Timeout in seconds
                STATUS download_status
            )

            # Check download status
            list(GET download_status 0 status_code)
            list(GET download_status 1 status_msg)
            
            if(status_code EQUAL 0)
                # 校验下载的文件大小，防止下载到错误页面
                file(SIZE "${DU_ZIP_SAVE_PATH}" file_size)
                if(file_size GREATER 1024)
                    set(download_success TRUE)
                    message(STATUS "${DU_PROJECT_NAME} ${DU_PROJECT_VERSION} downloaded successfully: ${DU_ZIP_SAVE_PATH} (${file_size} bytes)")
                else()
                    message(WARNING "Downloaded file is too small (${file_size} bytes), likely an error page or empty file. Retrying...")
                    math(EXPR retry_count "${retry_count} + 1")
                    file(REMOVE "${DU_ZIP_SAVE_PATH}")
                endif()
            else()
                math(EXPR retry_count "${retry_count} + 1")
                if(retry_count LESS ${DU_DOWNLOAD_RETRY})
                    message(WARNING "Download attempt ${retry_count_plus} failed: ${status_msg}. Retrying...")
                    file(REMOVE "${DU_ZIP_SAVE_PATH}")
                endif()
            endif()
        endwhile()

 
        if(NOT download_success)
            file(REMOVE "${DU_ZIP_SAVE_PATH}")
            message(FATAL_ERROR "Download failed after ${DU_DOWNLOAD_RETRY} attempts! Last error: ${status_code} - ${status_msg}")
        endif()
    endif()
    

    if(NOT EXISTS "${DU_UNZIP_TARGET_DIR}/${DU_RENAME_TO}")
        file(MAKE_DIRECTORY "${DU_UNZIP_TARGET_DIR}")
        message(STATUS "Extracting ${DU_PROJECT_NAME} ${DU_PROJECT_VERSION} ...")

        # 根据平台和文件类型选择解压工具
        # macOS/Linux 上使用系统 unzip 避免 cmake -E tar 的 UTF-8 locale 转换问题
        # 同时设置 LANG=en_US.UTF-8 确保能正确处理 zip 中的非 ASCII 文件名
        get_filename_component(zip_ext "${DU_ZIP_SAVE_PATH}" LAST_EXT)
        if(zip_ext STREQUAL ".zip" AND NOT WIN32)
            find_program(UNZIP_EXECUTABLE unzip)
            if(UNZIP_EXECUTABLE)
                if(APPLE)
                    # macOS 上 ditto 对多字节文件名支持最好
                    find_program(DITTO_EXECUTABLE ditto)
                    if(DITTO_EXECUTABLE)
                        set(tar_command ${DITTO_EXECUTABLE} -xk "${DU_ZIP_SAVE_PATH}" .)
                    else()
                        set(tar_command ${UNZIP_EXECUTABLE} -o "${DU_ZIP_SAVE_PATH}")
                    endif()
                else()
                    set(tar_command ${UNZIP_EXECUTABLE} -o "${DU_ZIP_SAVE_PATH}")
                endif()
            else()
                # 回退到 cmake -E tar
                set(tar_command ${CMAKE_COMMAND} -E tar xzf "${DU_ZIP_SAVE_PATH}")
            endif()
        elseif(WIN32)
            set(tar_command ${CMAKE_COMMAND} -E tar xf "${DU_ZIP_SAVE_PATH}")
        else()
            set(tar_command ${CMAKE_COMMAND} -E tar xzf "${DU_ZIP_SAVE_PATH}")
        endif()

        # 4. 执行解压并增强错误信息
        #    Unix/macOS 上用 env 设置 LANG/LC_ALL 为 UTF-8 以处理含非 ASCII 文件名的 zip
        #    Windows 上直接使用 tar 命令（env 在 Windows 上不可用）
        if(WIN32)
            execute_process(
                COMMAND ${tar_command}
                WORKING_DIRECTORY "${DU_UNZIP_TARGET_DIR}"
                RESULT_VARIABLE unzip_result
                OUTPUT_VARIABLE unzip_output
                ERROR_VARIABLE unzip_error
            )
        else()
            execute_process(
                COMMAND env LANG=en_US.UTF-8 LC_ALL=en_US.UTF-8 ${tar_command}
                WORKING_DIRECTORY "${DU_UNZIP_TARGET_DIR}"
                RESULT_VARIABLE unzip_result
                OUTPUT_VARIABLE unzip_output
                ERROR_VARIABLE unzip_error
            )
        endif()

        # Check unzip status
        if(NOT unzip_result EQUAL 0)
            file(REMOVE_RECURSE "${DU_UNZIP_TARGET_DIR}") # Clean up residual files on extraction failure
            file(REMOVE "${DU_ZIP_SAVE_PATH}") # Remove corrupted zip so it will be re-downloaded
            message(FATAL_ERROR "Failed to extract ${DU_PROJECT_NAME}! Error code: ${unzip_result}, Error log: ${unzip_error}\n  The corrupted zip file has been removed. Please re-run cmake to re-download.")
        endif()
        if(DU_RENAME_TO)
            file(RENAME "${DU_UNZIP_TARGET_DIR}/${DU_PROJECT_NAME}-${DU_PROJECT_VERSION}" "${DU_UNZIP_TARGET_DIR}/${DU_RENAME_TO}")
            message(STATUS "${DU_PROJECT_NAME} ${DU_PROJECT_VERSION} extracted to dir: ${DU_UNZIP_TARGET_DIR}/${DU_RENAME_TO}")
        else()
            message(STATUS "${DU_PROJECT_NAME} ${DU_PROJECT_VERSION} extracted to dir: ${DU_UNZIP_TARGET_DIR}/${DU_PROJECT_NAME}-${DU_PROJECT_VERSION}")
        endif()
    else()
        message(STATUS "${DU_PROJECT_NAME} ${DU_PROJECT_VERSION} extract dir already exists, skip extraction step")
    endif()


endfunction()