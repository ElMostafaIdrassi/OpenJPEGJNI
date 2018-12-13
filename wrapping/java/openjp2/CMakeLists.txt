# wrapping/java/openjp2/CMakeLists.txt

# First thing define the source files :
set(openjpegjni_SRCS
	JavaOpenJPEGDecoder.c
	index.c
	index.h
	convert.c
	convert.h
	convertbmp.c
	${OPENJPEG_SOURCE_DIR}/src/lib/openjp2/opj_malloc.c
	${OPENJPEG_SOURCE_DIR}/src/bin/common/color.c
	${OPENJPEG_SOURCE_DIR}/src/bin/common/color.h
	${OPENJPEG_SOURCE_DIR}/src/bin/common/opj_getopt.c
	${OPENJPEG_SOURCE_DIR}/src/bin/common/opj_getopt.h
	${OPENJPEG_SOURCE_DIR}/src/bin/common/opj_string.h
)
if(OPJ_HAVE_LIBTIFF)
	list(APPEND openjpegjni_SRCS converttif.c)
endif()
if(OPJ_HAVE_LIBPNG)
	list(APPEND openjpegjni_SRCS convertpng.c)
endif()

# JNI binding :
if (ANDROID)
	# We are only interested in finding jni.h: we do not care about extended JVM
	# functionality or the AWT library.
	# Avoid error message : Could NOT find JNI (missing: JAVA_AWT_LIBRARY JAVA_JVM_LIBRARY
  	# JAVA_INCLUDE_PATH JAVA_INCLUDE_PATH2 JAVA_AWT_INCLUDE_PATH)
	set(JAVA_AWT_LIBRARY NotNeeded)
	set(JAVA_JVM_LIBRARY NotNeeded)
	set(JAVA_INCLUDE_PATH2 NotNeeded)
	set(JAVA_AWT_INCLUDE_PATH NotNeeded)
	# set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE BOTH)
endif()

find_package(JNI REQUIRED)
include_directories(${JNI_INCLUDE_DIRS})

# Required header files are located in :
include_directories(
	${OPENJPEG_BINARY_DIR}/src/lib/openjp2 	# opj_config.h, opj_config_private.h
	${OPENJPEG_BINARY_DIR}/src/bin/common 	# opj_apps_config.h
	${OPENJPEG_SOURCE_DIR}/src/lib/openjp2	# openjpeg.h, opj_includes.h
	${OPENJPEG_SOURCE_DIR}/src/bin/common	# opj_getopt.h
)
if(BUILD_THIRDPARTY)
	include_directories(
		${LCMS_INCLUDE_DIRNAME}
		${Z_INCLUDE_DIRNAME}
		${PNG_INCLUDE_DIRNAME}
		${TIFF_INCLUDE_DIRNAME}
	)
endif()

add_library(openjpegjni MODULE
	${openjpegjni_SRCS}
)

# Java module should not have a SONAME:
set_property(TARGET openjpegjni PROPERTY NO_SONAME 1)

# Link libraries
target_link_libraries(openjpegjni 
	${OPENJPEG_LIBRARY_NAME}
	${PNG_LIBNAME} 
	${TIFF_LIBNAME} 
	${LCMS_LIBNAME}
)

# To support universal exe:
if(ZLIB_FOUND AND APPLE)
	target_link_libraries(openjpegjni z)
else()
	target_link_libraries(openjpegjni ${Z_LIBNAME})
endif()

# On unix, we need to link to the math library:
if(UNIX)
  target_link_libraries(openjpegjni m)
endif()

install(TARGETS openjpegjni
  EXPORT OpenJPEGTargets
  LIBRARY DESTINATION ${OPENJPEG_INSTALL_JNI_DIR} COMPONENT Libraries
)

# Build jar :
find_package(Java 1.5 REQUIRED) # javac, jar

# Build dep list :
file(GLOB java_srcs "java-sources/org/openJpeg/*.java")

# Make sure target javac dir exists:
file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/classes)

# Build java
add_custom_command(
	OUTPUT ${LIBRARY_OUTPUT_PATH}/openjpeg.jar
	COMMAND ${Java_JAVAC_EXECUTABLE} -sourcepath  "${CMAKE_CURRENT_SOURCE_DIR}/java-sources"
	${java_srcs} -d ${CMAKE_CURRENT_BINARY_DIR}/classes
	COMMAND ${Java_JAR_EXECUTABLE} cvf ${LIBRARY_OUTPUT_PATH}/openjpeg.jar org
	WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/classes
	DEPENDS ${java_srcs}
	COMMENT "javac *.java; jar cvf -> openjpeg.jar"
)

# Name the target
add_custom_target(OpenJPEGJavaJar ALL
	DEPENDS ${LIBRARY_OUTPUT_PATH}/openjpeg.jar
	COMMENT "building openjpeg.jar"
)

install(FILES ${LIBRARY_OUTPUT_PATH}/openjpeg.jar
	DESTINATION ${OPENJPEG_INSTALL_SHARE_DIR} COMPONENT JavaModule
)
