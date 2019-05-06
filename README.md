# OPENJPEG Library and Applications

## What is OpenJPEG ? 

OpenJPEG is an open-source JPEG 2000 codec written in C language. It has been developed in order to promote the use of [JPEG 2000](http://www.jpeg.org/jpeg2000), a still-image compression standard from the Joint Photographic Experts Group ([JPEG](http://www.jpeg.org)).  Since April 2015, it is officially recognized by ISO/IEC and ITU-T as a [JPEG 2000 Reference Software](http://www.itu.int/rec/T-REC-T.804-201504-I!Amd2).

## What is OpenJPEGJNI and what does it bring ? 

OpenJPEGJNI is a fork of the original OpenJPEG open-source project (as found on github : https://github.com/uclouvain/openjpeg/) that **fixes and improves the broken JNI code and bindings for only the decoder.**

It also **provides binaries** of the **native libraries (the JNI and the OpenJP2 libraries)** for `Windows x86 and x86_64 architectures, for Linux x86 and x86_64 architectures, and for Android armeabi-v7a, arm64-v8a, x86 and x86_64 ABIs.`

**ALL** the code, **with the exception of code under wrapping folder**, is the same as in **the version 2.3.1** of the original OpenJPEG repository, which is the latest release so far.

All the code that I had to modify / add is under the **wrapping** folder and the **root CMakeLists.txt**. In the following, I explain how I managed to make the JNI part of the OpenJPEG decoder build and work.

Please note that only the **JNI decoder** has been **implemented** and **tested** under **Java 1.8**, on Windows, Linux and Android. 

## How I managed to make the JNI part of OpenJPEG build and work ?

The code in `wrapping/java/openjp2/JavaOpenJpegDecoder.c` and the `CMakeLists.txt` are outdated.
It makes use of **old** and **deprecated** functions and structures, usually defined in files under `src/lib/openmj2 (e.g. openjpeg.h).`
Therefore, in order to make the JNI part of OpenJPEG work and to be able to build native JNI libraries for Windows, Linux and Android / ARM, 
I had to get rid of the old and deprecated code and replace it with the newest one from the latest release of OpenJPEG, v.2.3.1.

    cd wrapping/java/openjp2

 - **1st approach : FAILURE**

	My first approach was to clean up the `CMakeLists.txt` as well as the `#include` list in `JavaOpenJpegDecoder.c` 
	then to add source files and include directories when they were needed, and tweak the code as needed.
	This looked promising, until I encountered a lot of code in `JavaOpenJpegDecoder.c` that was using **structures** defined in the source file `openjpeg.h under src/lib/openmj2`, **rather than the one under src/lib/openjp2.**

	My first instinct was to merge both files, which I first did. However, after running a couple of tests, I encountered numerous bugs related to some basic assert instructions.
	
	After some digging, I've found out that the newest code had already fixed those bugs, **fixes that didn't make it to the code / files under src/lib/openmj2.**

	Therefore, I had to find another approach.
	

 - **2nd approach : SUCCESS**

	By looking at the code in `src/bin/jp2/opj_decompress.c`, I couldn't help but notice that it looked a lot similar to	the one under `JavaOpenJpegDecoder.c`.

	In short, I had the idea to copy the code from `src/bin/jp2/opj_decompress.c` into `JavaOpenJpegDecoder.c`, and tweak it in order to make it work.
	
	But, the `opj_decompress.c` code can only **deal with files / folders** which are passed to it as parameters in a command line	style **using -i, -o, -ImgDir** etc... The `JavaOpenJpegDecoder.c` code, on the other hand, enables the caller to either : 
	* use the command line style : the behaviour will be similar to the one of `opj_decompress.c`
		or 
	* directly set the **J2K /JP2 / JPT input stream** as a byte array from the Java code, pass it to the C code,  which will decode / decompress it and finally will return the resulting output stream in some format to the Java code.
	
	**The second option was the major change that I had to implement.**
	
	Side notes : (of when using an input stream)
	
	- **Only the BMP output format** is supported for when the caller sets the input stream from Java code. 
	This is due to the fact that, to produce the other formats, the code makes use of functions (e.g. imagetoXXX() where XXX is the output format) which are difficult to port from the default "File philosophy" into the "Stream philosophy" due to a lack of generic code in these functions.
	The function **imagetobmp()** was the only generic one amongst the others and the easiest to reimplement.
	Feel free to implement the other functions if you can and make a pull request! :)
	
	- I made sure in my code to avoid storing the input stream in the C code heap, by passing it as **Java NIO ByteBuffer** and getting only a reference from the C code to the byte array in the Java code.
	
	- I made sure in my code to avoid storing the output stream as a large byte array in the C code and then pass it finally to the Java code.
	Sometimes, bmp images can be very big and we cannot afford failing the decoding because of a lack of memory to hold the entire output.
	
	- I will try to provide, in the future, the function setLocking() in Java code, which can be called only when using input streams. 
	When used, it sets locking in the C Code, meaning that the buffers used during the decoding and the output of the result will be 
	locked and prevented from being swapped, using VirtualLock / VirtualUnlock on Windows and mlock / munlock under Linux.
	This is a work in progress, as there are multiple challenges regarding this. It is thus not yet implemented.

	**N.B : **
	
	*	In `opj_malloc.h under src/lib/openjp2`, there is a block of code that **poisons "malloc calloc realloc free".** 
	
			#if defined(__GNUC__) && !defined(OPJ_SKIP_POISON)
			#pragma GCC poison malloc calloc realloc free
			#endif
	
		This means that whoever wrote that code wanted to make sure that such calls are treated as hard errors.
		This forces the devs to use opj_malloc, opj_calloc, opj_realloc and opj_free.
		
		However, when I used opj_realloc in my method `opj_write_to_stream()`, I encountered the following error : 
		*"Invalid address specified to RtlValidateHeap."*
		This was fixed by just using `realloc().`
		
		Also there is original code in `opj_decompress.c` and `JavaOpenJPEGDecoder.c` that makes use of free, realloc and company.
		
		To avoid errors due to this poisoning pragma, **I defined** **OPJ_SKIP_POISON** in the beginning of `JavaOpenJPEGDecoder.c`.
		
	*	When **building** for **Linux**, I faced an error when linking against third party library libpng.a 
	
			libpng.a(pngrutil.c.o): relocation R_X86_64_PC32 against symbol `png_crc_finish' can not be used when making a shared object; recompile with -fPIC
		
		This was solved by adding "POSITION_INDEPENDENT_CODE TRUE" as a target property in the CMakeLists.txt of all the 
		thirdparty libraries under **thirdparty**, that is : liblcms2, libpng, libtiff and libz.
		
	*	When **building** for **Android** **under Windows using NDK**, **WIN32** is actually set.
		I modified the following #ifdef in `JavaOpenJPEGDecoder.c` from : 
		
			#if defined (_WIN32)
			#include <windows.h>
			#define strcasecmp _stricmp
			#define strncasecmp _strnicmp
			#else
			#include <strings.h>
			#include <sys/time.h>
			#include <sys/resource.h>
			#include <sys/times.h>
			#endif /* _WIN32 */
			
			to 
			
			#if defined (_WIN32) && (_MSC_VER) && (!__INTEL_COMPILER)
			#include <windows.h>
			#define strcasecmp _stricmp
			#define strncasecmp _strnicmp
			#else
			#include <strings.h>
			#include <sys/time.h>
			#include <sys/resource.h>
			#include <sys/times.h>
			#endif /* _WIN32 */
			
		This way, when building for Android, `strcasecmp` is not set to `_stricmp` but rather points to the default linux function found in `strings.h` header.
		
	*	When **building** for **Android under Windows using NDK** (especially for **arm64-v8a**), I hit the following error : 
		
		*thirdparty/lib/libpng.a(pngrutil.c.o): In function png_init_filter_functions':
		undefined reference to `png_init_filter_functions_neon'
		clang.exe: error: linker command failed with exit code 1 (use -v to see invocation)*
		
		It is due to this block in *pngpriv.h*
		
			#ifndef PNG_ARM_NEON_OPT
			#if (defined(__ARM_NEON__) || defined(__ARM_NEON)) && \
			   defined(PNG_ALIGNED_MEMORY_SUPPORTED)
			#define PNG_ARM_NEON_OPT 2
			#else
			#define PNG_ARM_NEON_OPT 0
			#endif
			#endif

			#if PNG_ARM_NEON_OPT > 0
			#define PNG_FILTER_OPTIMIZATIONS png_init_filter_functions_neon
			#endif /* PNG_ARM_NEON_OPT > 0 */
		
		Gcc on 64bit ARM always has macro __ARM_NEON defined, since NEON in part of ARMv8.
		Thus, on 64bit ARM, even without ENEBLE_NEON=ON being passed to cmake, png_init_filter_functions_neon which doesn't exist, will be called.
		Usually, many Linux distributions have their own libpng package, so the issue itself doesn't have a big impact.
		However, on Android / ARM, this issue exists, and to avoid it, I added the following to **CMakeLists.txt in root folder :** 
		
			if(CMAKE_SYSTEM_PROCESSOR MATCHES "^arm" OR CMAKE_SYSTEM_PROCESSOR MATCHES "^aarch64")
			add_definitions(-DPNG_ARM_NEON_OPT=0)
			endif()
				
		Or, when building for Android / ARM, we can append :
			
			DCMAKE_C_FLAGS:STRING="-DPNG_ARM_NEON_OPT=0"

			
## How to build ?


	Building under Windows using MSVC Visual Studio 15 2017 :

		N.B : We need to use static runtime libraries (/MT) instead of dlls (/MD)
			  Projects to configure : lcms2, openjp2, openjpegjni, opj_decompress, opj_compress, opj_dump, png, tiff, z

		For x86
			*	cd build/x86
			*	cmake ..\.. -G "Visual Studio 15 2017" -DBUILD_THIRDPARTY:BOOL=ON -DBUILD_JAVA:BOOL=ON
			*	For each project
					--	Properties / C/C++ / Code Generation 
							from /MDd to /MTd (for debug)
							from /MD to /MT (for release)
					Optional (to enable more warnings)
						--	Properties / C/C++ / General 
								Warning Level = /W4
								Treat warnings as errors : No
						--	Properties / C/C++ / Language / 
								Disable language extensions : No 
								Conformance Mode : Yes (/permissive-)
			*	Build the solution from VS (Chose the configuration beforehand)
			*	We find the .dll files in bin/Release or bin/Debug etc... depending on the configuration selected

		For x86_64
			*	cd build/x86_64
			*	cmake ..\.. -G "Visual Studio 15 2017 Win64" -DBUILD_THIRDPARTY:BOOL=ON -DBUILD_JAVA:BOOL=ON
			*	For each project
					--	Properties / C/C++ / Code Generation 
							from /MDd to /MTd (for debug)
							from /MD to /MT (for release)
					Optional (to enable more warnings)
						--	Properties / C/C++ / General 
								Warning Level = /W4
								Treat warnings as errors : No
						--	Properties / C/C++ / Language / 
								Disable language extensions : No 
								Conformance Mode : Yes (/permissive-)
			*	Build the solution from VS (Chose the configuration beforehand)
			*	We find the .dll files in bin/Release or bin/Debug etc... depending on the configuration selected

****************************************************

	Building for Linux under Ubuntu 18.04
	
		*	Optional : build using -pedantic (disables language extensions)

		*	sudo apt-get install liblcms2-dev libtiff-dev libpng-dev zlib1g-dev	(for Ubuntu built-in third party libraries)
			sudo apt install openjdk-8-jdk 										(for JNI library)

			$ gcc --version
				gcc (Ubuntu 7.3.0-27ubuntu1~18.04) 7.3.0

			$ java -version
				openjdk version "1.8.0_191"
				OpenJDK Runtime Environment (build 1.8.0_191-8u191-b12-2ubuntu0.18.04.1-b12)
				OpenJDK 64-Bit Server VM (build 25.191-b12, mixed mode)

			$ javac -version
				javac 1.8.0_191

			*	cd build
			*	cmake ../.. -G "Unix Makefiles" -DBUILD_THIRDPARTY:BOOL=ON -DBUILD_JAVA:BOOL=ON -DCMAKE_BUILD_TYPE="Release" -DCMAKE_VERBOSE_MAKEFILE:BOOL=ON
			*	make

				We get in bin 
				├── libopenjp2.a
				├── libopenjp2.so -> libopenjp2.so.7
				├── libopenjp2.so.2.3.1
				├── libopenjp2.so.7 -> libopenjp2.so.2.3.1
				├── libopenjpegjni.so
				├── openjpeg.jar
				├── opj_compress
				├── opj_decompress
				└── opj_dump

********************************************************

	Building for Android (ARM) under Windows
		https://boringssl.googlesource.com/boringssl/+/version_for_cocoapods_6.0/third_party/android-cmake/README.md
		https://developer.android.com/ndk/guides/abis
		https://developer.android.com/ndk/guides/cmake

		*	In Android Studio, install NDK and CMake (revision: 3.6.4111459)
			=>	armeabi removed since NDK17
				MIPS 32-bit and 64-bit removed since NDK17
				x86 and x86_64 under Android are very rare
				=>	We target : 
						armeabi-v7a
						arm64-v8a
						x86
						x86_64

		*	Download https://github.com/ninja-build/ninja/releases. 
			Create, for example, C://Ninja and put the exe there
			Finally, add it to the path

		*	Need ndk-builds/ platforms : 
				android-8 platform for ARM 
				android-9 platform for x86 and MIPS 
				android-21 platform for 64-bit ABIs
			Missing platforms in ndk-builds/platforms of NDK 18
			=>	Please note that recent NDKs (r14+) have dropped support for platforms below API 9 (GINGERBREAD).
				Download from https://android.googlesource.com/platform/development/+/a77c1bdd4abf3c7e82af1f3b4330143d14c84103/ndk/platforms

		*	Invalid Android NDK revision (should be 12): 19.2.5345600
			=>	Download NDK-12 from https://developer.android.com/ndk/downloads/older_releases
			=>	Extract in C:\Users\<user>\AppData\Local\Android\Sdk\ndk-12
			
		*	Only CMake version that works is : 3.6.4111459 and maybe below. Version 3.10 does not work as "Android Gradle - Ninja" generator cannot be created.
			See https://stackoverflow.com/questions/48294319/cmake-error-could-not-create-named-generator-android-gradle-ninja

		For armeabi-v7a :
			*	cd C:\Users\<user>\AppData\Local\Android\Sdk\cmake\3.6.4111459\bin
			*	cmake -G "Android Gradle - Ninja" -B"path/to/build/dir/armeabi-v7a" -H"path/to/source" -DANDROID_NDK="C:\Users\<user>\AppData\Local\Android\Sdk\ndk-12" -DBUILD_THIRDPARTY:BOOL=ON -DBUILD_JAVA:BOOL=ON -DCMAKE_BUILD_TYPE="Release" -DCMAKE_VERBOSE_MAKEFILE:BOOL=ON -DANDROID_ABI="armeabi-v7a" -DCMAKE_TOOLCHAIN_FILE="C:\Users\<user>\AppData\Local\Android\Sdk\cmake\3.6.4111459\android.toolchain.cmake"
			*	cmake --build "path/to/build/dir/armeabi-v7a"

		For arm64-v8a: 
			*	cd C:\Users\<user>\AppData\Local\Android\Sdk\cmake\3.6.4111459\bin
			*	cmake -G "Android Gradle - Ninja" -B"path/to/build/dir/arm64-v8a" -H"path/to/source" -DANDROID_NDK="C:\Users\<user>\AppData\Local\Android\Sdk\ndk-12" -DBUILD_THIRDPARTY:BOOL=ON -DBUILD_JAVA:BOOL=ON -DCMAKE_BUILD_TYPE="Release" -DCMAKE_VERBOSE_MAKEFILE:BOOL=ON -DANDROID_ABI="arm64-v8a" -DCMAKE_TOOLCHAIN_FILE="C:\Users\<user>\AppData\Local\Android\Sdk\cmake\3.6.4111459\android.toolchain.cmake" -DCMAKE_C_FLAGS:STRING="-DPNG_ARM_NEON_OPT=0"
			*	cmake --build "path/to/build/dir/arm64-v8a"
			
		For x86 :
			*	cd C:\Users\<user>\AppData\Local\Android\Sdk\cmake\3.6.4111459\bin
			*	cmake -G "Android Gradle - Ninja" -B"path/to/build/dir/x86" -H"path/to/source" -DANDROID_NDK="C:\Users\<user>\AppData\Local\Android\Sdk\ndk-12" -DBUILD_THIRDPARTY:BOOL=ON -DBUILD_JAVA:BOOL=ON -DCMAKE_BUILD_TYPE="Release" -DCMAKE_VERBOSE_MAKEFILE:BOOL=ON -DANDROID_ABI="x86" -DCMAKE_TOOLCHAIN_FILE="C:\Users\<user>\AppData\Local\Android\Sdk\cmake\3.6.4111459\android.toolchain.cmake"
			*	cmake --build "path/to/build/dir/x86"

		For x86_64: 
			*	cd C:\Users\<user>\AppData\Local\Android\Sdk\cmake\3.6.4111459\bin
			*	cmake -G "Android Gradle - Ninja" -B"path/to/build/dir/x86_64" -H"path/to/source" -DANDROID_NDK="C:\Users\<user>\AppData\Local\Android\Sdk\ndk-12" -DBUILD_THIRDPARTY:BOOL=ON -DBUILD_JAVA:BOOL=ON -DCMAKE_BUILD_TYPE="Release" -DCMAKE_VERBOSE_MAKEFILE:BOOL=ON -DANDROID_ABI="x86_64" -DCMAKE_TOOLCHAIN_FILE="C:\Users\<user>\AppData\Local\Android\Sdk\cmake\3.6.4111459\android.toolchain.cmake"
			*	cmake --build "path/to/build/dir/x86_64"


## How to use the decoder ?

- First, you have to load the JP2 native library. This is crucial because the JNI native library depends on it :
`System.load("absolute/path/to/openjpegLib")` 

- Then, instantiate an `OpenJPEGJavaDecoder ` object by passing it the absolute path to the JNI native library : `OpenJPEGJavaDecoder decoder = new OpenJPEGJavaDecoder("absolute/path/to/openjpegjniLib");` . **Optionally, you can pass it an implementation of the interface IJavaJ2KDecoderLogger to be able to get logs from the C code.**

- Here, you have 2 choices : 

   - either you pass parameters to the decoder similar to passing parameters to `opj_decompress`, via `decoder.setDecoderArguments(args)`
   - or you pass the input stream to be decoded directly, via `decoder.setInputStream(byte[])`. Note that if the input stream is a JPT / JPIP stream, you must also call `decoder.setInputIsJPT()`. Also, you should set the output format via a call to `decoder.setOutputFormat(String)`, knowing that the only supported format for the output is BMP as explained in the README.

- Finally, you call `decoder.decodeJ2KtoImage()`. In case you passed an input stream to the decoder, you shall get the output by calling `decoder.getOutputStreamBytes()` as a byte array.

## Who can use the code ?

Anyone. As the OpenJPEG code is released under the [BSD 2-clause "Simplified" License][link-license], anyone can use or modify the code, even for commercial applications. The only restriction is to retain the copyright in the sources or in the binaries documentation.

## Who are the developers ?

The JNI part has been solely modified by me.

The library is developed and maintained by the Image and Signal Processing Group ([ISPGroup](http://sites.uclouvain.be/ispgroup/)), in the Université catholique de Louvain ([UCL](http://www.uclouvain.be/en-index.html), with the support of the [CNES](https://cnes.fr/), the [CS](http://www.c-s.fr/) company and the [intoPIX](http://www.intopix.com) company. The JPWL module has been developed by the Digital Signal Processing Lab ([DSPLab](http://dsplab.diei.unipg.it/)) of the University of Perugia, Italy ([UNIPG](http://www.unipg.it/)).

## Details on folders hierarchy

* src
  * lib
    * openjp2: contains the sources of the openjp2 library (Part 1 & 2)
    * openjpwl: contains the additional sources if you want to build a JPWL-flavoured library.
    * openjpip: complete client-server architecture for remote browsing of jpeg 2000 images.
    * openjp3d: JP3D implementation
    * openmj2: MJ2 implementation
  * bin: contains all applications that use the openjpeg library
    * common: common files to all applications
    * jp2: a basic codec
    * mj2: motion jpeg 2000 executables
    * jpip: OpenJPIP applications (server and dec server)
      * java: a Java client viewer for JPIP
    * jp3d: JP3D applications
      * tcltk: a test tool for JP3D
    * wx
      * OPJViewer: gui for displaying j2k files (based on wxWidget)
* wrapping
  * java: java jni to use openjpeg in a java program
* thirdparty: thirdparty libraries used by some applications. These libraries will be built only if there are not found on the system. Note that libopenjpeg itself does not have any dependency.
* doc: doxygen documentation setup file and man pages
* tests: configuration files and utilities for the openjpeg test suite. All test images are located in [openjpeg-data](https://github.com/uclouvain/openjpeg-data) repository.
* cmake: cmake related files
* scripts: scripts for developers

See [LICENSE][link-license] for license and copyright information.

See [INSTALL](https://github.com/uclouvain/openjpeg/blob/master/INSTALL.md) for installation procedures.

See [NEWS](https://github.com/uclouvain/openjpeg/blob/master/NEWS.md) for user visible changes in successive releases.

## API/ABI

An API/ABI timeline is automatically updated [here][link-api-timeline].

OpenJPEG strives to provide a stable API/ABI for your applications. As such it
only exposes a limited subset of its functions.  It uses a mechanism of
exporting/hiding functions. If you are unsure which functions you can use in
your applications, you should compile OpenJPEG using something similar to gcc:
`-fvisibility=hidden` compilation flag.
See also: http://gcc.gnu.org/wiki/Visibility

On windows, MSVC directly supports export/hiding function and as such the only
API available is the one supported by OpenJPEG.

[comment-license]: https://img.shields.io/github/license/uclouvain/openjpeg.svg "https://img.shields.io/badge/license-BSD--2--Clause-blue.svg"
[badge-license]: https://img.shields.io/badge/license-BSD--2--Clause-blue.svg "BSD 2-clause \"Simplified\" License"
[link-license]: https://github.com/uclouvain/openjpeg/blob/master/LICENSE "BSD 2-clause \"Simplified\" License"
[badge-build]: https://travis-ci.org/uclouvain/openjpeg.svg?branch=master "Build Status"
[link-build]: https://travis-ci.org/uclouvain/openjpeg "Build Status"
[badge-msvc-build]: https://ci.appveyor.com/api/projects/status/github/uclouvain/openjpeg?branch=master&svg=true "Windows Build Status"
[link-msvc-build]: https://ci.appveyor.com/project/detonin/openjpeg/branch/master "Windows Build Status"
[badge-coverity]: https://scan.coverity.com/projects/6383/badge.svg "Coverity Scan Build Status"
[link-coverity]: https://scan.coverity.com/projects/uclouvain-openjpeg "Coverity Scan Build Status"
[link-api-timeline]: http://www.openjpeg.org/abi-check/timeline/openjpeg "OpenJPEG API/ABI timeline"

