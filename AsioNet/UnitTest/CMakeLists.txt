# CMakeList.txt: AsioNet 的 CMake 项目，在此处包括源代码并定义
# 项目特定的逻辑。
#
cmake_minimum_required (VERSION 2.8)

PROJECT(UnitTest)

SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")

SET(CMAKE_EXE_LINKER_FLAGS "-rdynamic -Wl,-Bstatic -Wl,-Bdynamic -lstdc++ -lpthread -ldl -lz -lrt")

INCLUDE_DIRECTORIES("${PROJECT_SOURCE_DIR}/../../asio/include")

INCLUDE_DIRECTORIES(${PROJECT_SOURCE_DIR})

INCLUDE_DIRECTORIES("${PROJECT_SOURCE_DIR}/../AsioNet")

IF(WIN32)
#zlib
SET(ZLIB_DIC "${PROJECT_SOURCE_DIR}/../../zlib_src/zlib_build")
INCLUDE_DIRECTORIES("${ZLIB_DIC}/include")
LINK_DIRECTORIES("${ZLIB_DIC}/lib")
ENDIF()

FILE(GLOB_RECURSE COMMON_LIST "${PROJECT_SOURCE_DIR}/../AsioNet/*.cpp")

FILE(GLOB_RECURSE SRC_LIST "${PROJECT_SOURCE_DIR}/*.cpp")

foreach(f ${COMMON_LIST})
	message("add cpp ${f}")
endforeach(f)

# 将源代码添加到此项目的可执行文件。
add_executable (
	UnitTest
	${COMMON_LIST}
	${SRC_LIST}
)


IF(WIN32)
	IF("${CMAKE_BUILD_TYPE}" STREQUAL "Debug")
		SET(ZLIB_NAME "zlibd")
	ELSE()
		SET(ZLIB_NAME "zlib")
	ENDIF()

	target_link_libraries(UnitTest ${ZLIB_NAME})
ENDIF()

# TODO: 如有需要，请添加测试并安装目标。
