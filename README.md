# OPENJPEG Library and Applications

## What is OpenJPEG ? 

OpenJPEG is an open-source JPEG 2000 codec written in C language. It has been developed in order to promote the use of [JPEG 2000](http://www.jpeg.org/jpeg2000), a still-image compression standard from the Joint Photographic Experts Group ([JPEG](http://www.jpeg.org)).  Since April 2015, it is officially recognized by ISO/IEC and ITU-T as a [JPEG 2000 Reference Software](http://www.itu.int/rec/T-REC-T.804-201504-I!Amd2).

## What is OpenJPEGJNI and what does it bring ? 

OpenJPEGJNI is a fork of the original OpenJPEG open-source project (as found on github : https://github.com/uclouvain/openjpeg/) that **fixes and improves the broken JNI code and bindings for only the JPEG decoder.**

It also **provides binaries** of the **native libraries (the JNI and the OpenJP2 libraries)** for `Windows x86 and x86_64 architectures, for Linux x86 and x86_64 architectures, and for Android armeabi-v7a, arm64-v8a, x86 and x86_64 ABIs targeting API 16.`

**ALL** the code, **with the exception of the code under wrapping folder**, is the same as in **the version 2.3.1** of the original OpenJPEG repository, which is the latest release so far.

All the code that I had to modify / add is under the **wrapping** folder and the **root CMakeLists.txt**.

Please note that only the **JNI decoder** has been **implemented** and **tested** using **Java 1.8**, on Windows, Linux and Android. 

## How I fixed the broken JNI decoder ?

Details on how I managed to make the JNI part of OpenJPEG work can be found [here](https://github.com/ElMostafaIdrassi/OpenJPEGJNI/blob/master/HOW-I-FIXED-JNI.md).
			
## How to build ?

Details on how to build OpenJPEGJNI for Windows, Linux and Android can be found [here](https://github.com/ElMostafaIdrassi/OpenJPEGJNI/blob/master/BUILD-STEPS.md).

## How to use the decoder ?

Details on how to load and use the OpenJPEGJNI library can be found [here](https://github.com/ElMostafaIdrassi/OpenJPEGJNI/blob/master/USAGE-NOTES.md).

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

