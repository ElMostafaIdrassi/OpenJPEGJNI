
**Building under Windows using MSVC Visual Studio 15 2017 :**

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

**Building for Linux under Ubuntu 18.04**
	
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
			*	cmake .. -G "Unix Makefiles" -DBUILD_THIRDPARTY:BOOL=ON -DBUILD_JAVA:BOOL=ON -DCMAKE_BUILD_TYPE="Release" -DCMAKE_VERBOSE_MAKEFILE:BOOL=ON
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

**Building for Android (ARM) under Windows**

		https://boringssl.googlesource.com/boringssl/+/version_for_cocoapods_6.0/third_party/android-cmake/README.md
		https://developer.android.com/ndk/guides/abis
		https://developer.android.com/ndk/guides/cmake

		*	In Android Studio, install latest NDK (here, 19.2.5345600)
			This way, we are certain we are going to use Clang toolchain instead of GCC.
			=>	armeabi ABI is removed since NDK17
				MIPS 32-bit and 64-bit ABIs are removed since NDK17
				x86 and x86_64 ABIs under Android are very rare
				=>	We target : 
						armeabi-v7a
						arm64-v8a
						x86
						x86_64
						
		*	Download and install the latest version of CMake (here 3.14)
			We suppose that the path to the latest version of CMake executable has been added to PATH
			
		*	Make sure to have Java 1.8 in the PATH.
			To find out which version of Java we have in PATH, we can execute the following command : 
			
				java -XshowSettings:properties -version
				
			and look for "java.home" entry.

		*	NDK platforms : 
				-> android-8 platform for ARM 
				-> android-9 platform for x86 and MIPS 
				-> android-21 platform for 64-bit ABIs
			Missing platforms for NDK 18/19
			=>	Please note that recent NDKs (r14+) have dropped support for platforms below API 9 (GINGERBREAD).
				Download them if needed from https://android.googlesource.com/platform/development/+/a77c1bdd4abf3c7e82af1f3b4330143d14c84103/ndk/platforms

		*	Invalid Android NDK revision (should be 12): 19.2.5345600
				=>	Use android.toolchain.cmake file in /path/to/ndk/build/cmake/ instead of the one in /path/to/sdk/cmake/<version>/
				
		In the following, we are going to target API 16 as minimim targetted API, which means API 16 is the version of the API the project is compiled against.
		We are using NDK 19.2.5345600, CMake 3.14 and Java 1.8.0_192.
		We are going to instruct CMake to use the android.toolchain.cmake file in /path/to/ndk/build/cmake/ instead of the one in /path/to/sdk/cmake/<version>/.
		We are going to use "MinGW Makefiles" as the CMake generator instead of the deprecated "Android Gradle - Ninja" generator which is only available under the version 3.6 that ships with Android Studio (See https://stackoverflow.com/questions/48294319/cmake-error-could-not-create-named-generator-android-gradle-ninja)

		For armeabi-v7a :
			*	cd build/armeabi-v7a
			*	cmake ../.. -G "MinGW Makefiles"  -DANDROID_PLATFORM="android-16" -DANDROID_ABI="armeabi-v7a" -DCMAKE_MAKE_PROGRAM="/path/to/ndk/prebuilt/windows-x86_64/bin/make.exe" -DCMAKE_ANDROID_NDK="/path/to/ndk"  -DCMAKE_TOOLCHAIN_FILE="/path/to/ndk/build/cmake/android.toolchain.cmake"  -DCMAKE_BUILD_TYPE="Release" -DBUILD_THIRDPARTY:BOOL=ON  -DBUILD_JAVA:BOOL=ON  -DCMAKE_VERBOSE_MAKEFILE:BOOL=ON
			*	cmake --build .

		For arm64-v8a: 
			*	cd build/arm64-v8a: 
			*	cmake ../.. -G "MinGW Makefiles"  -DANDROID_PLATFORM="android-16" -DANDROID_ABI="arm64-v8a" -DCMAKE_MAKE_PROGRAM="/path/to/ndk/prebuilt/windows-x86_64/bin/make.exe" -DCMAKE_ANDROID_NDK="/path/to/ndk"  -DCMAKE_TOOLCHAIN_FILE="/path/to/ndk/build/cmake/android.toolchain.cmake"  -DCMAKE_BUILD_TYPE="Release" -DBUILD_THIRDPARTY:BOOL=ON  -DBUILD_JAVA:BOOL=ON  -DCMAKE_VERBOSE_MAKEFILE:BOOL=ON
			*	cmake --build .
			
		For x86 :
			*	cd build/x86
			*	cmake ../.. -G "MinGW Makefiles"  -DANDROID_PLATFORM="android-16" -DANDROID_ABI="x86" -DCMAKE_MAKE_PROGRAM="/path/to/ndk/prebuilt/windows-x86_64/bin/make.exe" -DCMAKE_ANDROID_NDK="/path/to/ndk"  -DCMAKE_TOOLCHAIN_FILE="/path/to/ndk/build/cmake/android.toolchain.cmake"  -DCMAKE_BUILD_TYPE="Release" -DBUILD_THIRDPARTY:BOOL=ON  -DBUILD_JAVA:BOOL=ON  -DCMAKE_VERBOSE_MAKEFILE:BOOL=ON
			*	cmake --build .

		For x86_64: 
			*	cd build/x86_64
			*	cmake ../.. -G "MinGW Makefiles"  -DANDROID_PLATFORM="android-16" -DANDROID_ABI="x86_64" -DCMAKE_MAKE_PROGRAM="/path/to/ndk/prebuilt/windows-x86_64/bin/make.exe" -DCMAKE_ANDROID_NDK="/path/to/ndk"  -DCMAKE_TOOLCHAIN_FILE="/path/to/ndk/build/cmake/android.toolchain.cmake"  -DCMAKE_BUILD_TYPE="Release" -DBUILD_THIRDPARTY:BOOL=ON  -DBUILD_JAVA:BOOL=ON  -DCMAKE_VERBOSE_MAKEFILE:BOOL=ON
			*	cmake --build .
