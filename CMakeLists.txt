PROJECT(libcpu)

CMAKE_MINIMUM_REQUIRED(VERSION 2.8)

INCLUDE(FindThreads)

#
# 
#
SET(CMAKE_BUILD_TYPE "Debug")

#
# Add here the architecture you want to support.
# 
SET(GUEST_ARCHITECTURES 6502 m68k m88k mips arm x86 fapra)
SET(GUEST_EXTRA_TESTS multi)
IF(APPLE)
  SET(GUEST_EXTRA_TESTS ${GUEST_EXTRA_TESTS} next68k)
ENDIF(APPLE)

#
# Build Universal image.
#
IF(APPLE)
  OPTION(UNIVERSAL "Build Universal Executable" OFF)
ENDIF(APPLE)

IF(APPLE AND UNIVERSAL)
	INCLUDE(CMake/AppleMultiArch.cmake)
ENDIF()

# Add LLVM flags only to C++.
IF(MSVC)
SET(CMAKE_CXX_FLAGS "/std:c++17")
ELSE(MSVC)
SET(CMAKE_CXX_FLAGS "-std=c++17")
ENDIF(MSVC)

#
# Find LLVM, we need it.
#
SET(LLVM_FIND_REQUIRED TRUE)
INCLUDE(CMake/FindLLVM.cmake)

INCLUDE(CMake/BuildConfigH.cmake)
IF(MSVC)
	add_definitions(-D_CRT_SECURE_NO_WARNINGS -D_CRT_NONSTDC_NO_WARNINGS -D_SCL_SECURE_NO_WARNINGS -D__STDC_FORMAT_MACROS -D__STDC_CONSTANT_MACROS -D__STDC_LIMIT_MACROS)
	# disable warnings for LLVM
	add_definitions(-wd4146) # unary minus operator applied to unsigned type, result still unsigned
	add_definitions(-wd4800) # forcing value to bool 'true' or 'false' (performance warning)
	add_definitions(-wd4355) # 'this' : used in base member initializer list
	include_directories(libcpu/win32/msvc)
ENDIF()

#
# Find Python.
#
INCLUDE(FindPythonInterp)
IF(NOT PYTHONINTERP_FOUND)
	message(FATAL_ERROR "Could not find Python, but Python 2.6 or later is required")
ENDIF()


SET(LIBCPU_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/libcpu)

#
#
#
INCLUDE_DIRECTORIES(${LLVM_INCLUDE_DIR} ${PROJECT_SOURCE_DIR} ${PROJECT_SOURCE_DIR}/libcpu)
LINK_DIRECTORIES(${LLVM_LIB_DIR})

MACRO(TARGET_LINK_LLVM TARGET)
  TARGET_LINK_LIBRARIES(${TARGET} ${LLVM_LDFLAGS} ${LLVM_LIBS_CORE} ${LLVM_LIBS_JIT} ${LLVM_LIBS_JIT_OBJECTS})
ENDMACRO(TARGET_LINK_LLVM)

#
# Add extra warnings for gcc/g++.
#
IF(CMAKE_COMPILER_IS_GNUCXX)
  ADD_DEFINITIONS(-W -Wall -Wextra -Wno-unused -Wno-unused-parameter -D__STDC_FORMAT_MACROS=1 -fPIC)
ENDIF()

FOREACH(ARCHITECTURE ${GUEST_ARCHITECTURES})
	IF(EXISTS ${PROJECT_SOURCE_DIR}/arch/${ARCHITECTURE})
		MESSAGE("** Building Guest Architecture: ${ARCHITECTURE}")
		ADD_SUBDIRECTORY(${PROJECT_SOURCE_DIR}/arch/${ARCHITECTURE})
		INCLUDE_DIRECTORIES(${PROJECT_SOURCE_DIR}/arch/${ARCHITECTURE})
		IF(EXISTS ${PROJECT_SOURCE_DIR}/test/${ARCHITECTURE})
			MESSAGE("** Building Test: ${ARCHITECTURE}")
			ADD_SUBDIRECTORY(${PROJECT_SOURCE_DIR}/test/${ARCHITECTURE})
		ENDIF()
    SET(GUEST_ARCHITECTURES_ENABLED "${GUEST_ARCHITECTURES_ENABLED};${ARCHITECTURE}")
	ELSE()
		MESSAGE("** Disabling Guest Architecture: ${ARCHITECTURE}")
	ENDIF()
ENDFOREACH()

FOREACH(TEST ${GUEST_EXTRA_TESTS})
	IF(EXISTS ${PROJECT_SOURCE_DIR}/test/${TEST})
		MESSAGE("** Building Test: ${TEST}")
		ADD_SUBDIRECTORY(${PROJECT_SOURCE_DIR}/test/${TEST})
	ENDIF()
ENDFOREACH()

ADD_SUBDIRECTORY(libcpu)
# ADD_SUBDIRECTORY(upcl)
IF(NOT MSVC)
  ADD_SUBDIRECTORY(test/libloader)
  ADD_SUBDIRECTORY(test/libnix)
ENDIF()

