1 - First, we have to load the JP2 native library. This is crucial because the JNI native library depends on it :

    System.load("absolute/path/to/openjpegLib")

2 - Then, we instantiate an `OpenJPEGJavaDecoder ` object by passing it the absolute path to the JNI native library : 

    OpenJPEGJavaDecoder decoder = new OpenJPEGJavaDecoder("absolute/path/to/openjpegjniLib")` . 

**Optionally, we can pass it an implementation of the interface IJavaJ2KDecoderLogger to be able to get logs from the C code.**

3 - At this point, we have 2 choices : 

   - either we pass parameters to the decoder similar to passing parameters to `opj_decompress`, via `decoder.setDecoderArguments(args)`
   - or we pass the input stream to be decoded directly, via `decoder.setInputStream(byte[])`. 

Note that if the input stream is a JPT / JPIP stream, we must also call `decoder.setInputIsJPT()`. 
Also, we must set the output format via a call to `decoder.setOutputFormat(String)`, 
knowing that the only supported format for the output is BMP as explained in the README.

4 - Finally, we call `decoder.decodeJ2KtoImage()`. 
In case we passed an input stream to the decoder, we shall get the output as a byte array by calling `decoder.getOutputStreamBytes()`.
