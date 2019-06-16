The code in `wrapping/java/openjp2/JavaOpenJpegDecoder.c` and the `CMakeLists.txt` are outdated.
The code makes use of **old** and **deprecated** functions and structures, usually defined in files under `src/lib/openmj2 (e.g. openjpeg.h).`
Therefore, in order to make the JNI part of OpenJPEG work and to be able to build native JNI libraries for Windows, Linux and Android / ARM, I had to get rid of the old and deprecated code and replace it with the newest one from the latest release of OpenJPEG, v.2.3.1.

 - **1st approach : FAILURE**

	My first approach was to clean up the `CMakeLists.txt` as well as the `#include` list in `JavaOpenJpegDecoder.c` 
	then to add source files and include directories when they were needed, and tweak the code as needed.
	This looked promising, until I encountered a lot of code in `JavaOpenJpegDecoder.c` that was using **structures** defined in the source file `openjpeg.h under src/lib/openmj2`, **rather than the one under src/lib/openjp2.**

	My first instinct was to merge both files, which I first did. However, after running a couple of tests, I encountered numerous bugs related to some basic assert instructions.
	
	After some digging, I've found out that the newest code had already fixed those bugs, **fixes that didn't make it to the code / files under src/lib/openmj2.**

	Therefore, I had to find another approach.
	

 - **2nd approach : SUCCESS**

	By looking at the code in `src/bin/jp2/opj_decompress.c`, I couldn't help but notice that it looked a lot similar to the one under `JavaOpenJpegDecoder.c`.

	In short, I had the idea to copy the code from `src/bin/jp2/opj_decompress.c` into `JavaOpenJpegDecoder.c`, and tweak it in order to make it work.
	
	But, the `opj_decompress.c` code can only **deal with files / folders** which are passed to it as parameters in a command line	style **using -i, -o, -ImgDir** etc... The `JavaOpenJpegDecoder.c` code, on the other hand, enables the caller to either : 
	* use the command line style : the behaviour will be similar to the one of `opj_decompress.c`
		or 
	* directly set the **J2K /JP2 / JPT input stream** as a byte array from the Java code, pass it to the C code,  which will decode / decompress it and finally will return the resulting output stream in some format to the Java code.
	
	**The second option was the major change that I had to implement.**
	
	Side notes : (of when using a byte[] stream as input)
	
	- **Only the BMP output format** is supported for when the caller sets the input stream from Java code. 
	This is due to the fact that, to produce the other formats, the code makes use of functions (e.g. imagetoXXX() where XXX is the output format) which are difficult to port from the default "File philosophy" into the "Stream philosophy" due to a lack of generic code in these functions.
	The function **imagetobmp()** was the only generic one amongst the others and the easiest to reimplement.
	Feel free to implement the other functions if you can and make a pull request! :)
	
	- I made sure in my code to avoid storing the input stream in the C code heap, by passing it as **Java NIO ByteBuffer** and getting only a reference from the C code to the byte array in the Java code.
	
	- I made sure in my code to avoid storing the output stream as a large byte array in the C code and then pass it finally to the Java code.
	Instead, whenever there is a part of the JPEG byte stream that has been successfully decoded into a BMP image, I write the resulting output into the corresponding Java output byte array by calling the method **writeToOutputStream** from the C code.
	Sometimes, BMP images can be very big and we cannot afford failing the decoding because of a lack of memory to hold the entire output.
	
	- I will try to provide, in the future, the function setLocking() in Java code, which can be called only when using input streams. 
	When used, it sets locking in the C Code, meaning that the buffers used during the decoding and the output of the result will be 
	locked and prevented from being swapped, using VirtualLock / VirtualUnlock on Windows and mlock / munlock under Linux.
	This is a work in progress, as there are multiple challenges regarding this. It is thus not yet implemented.

	**N.B :**
	
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
