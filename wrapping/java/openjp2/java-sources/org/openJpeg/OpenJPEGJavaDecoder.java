/*
 * Copyright (C) 2018, El Mostafa IDRASSI
 * Copyright (c) 2002-2014, Universite catholique de Louvain (UCL), Belgium
 * Copyright (c) 2002-2014, Professor Benoit Macq
 * Copyright (c) 2002-2007, Patrick Piscaglia, Telemis s.a.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS `AS IS'
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */ 
package org.openJpeg;

import java.io.ByteArrayOutputStream;
import java.nio.ByteBuffer;
import java.util.Vector;

/** 
 * This class can be used in two different ways : 
 * 1 -	To decompress / decode one or multiple images in a manner that is similar
 *			to the opj_decompress utility. 
 *		This is achieved by passing arguments via a call to setDecoderArguments(args) 
 *			as it is similarily done via a command line.
 * 2 -	To decompress / decode one JPEG-2000 stream of bytes at a time and gather the result
 *			as .bmp output stream of bytes.
 *      In this case, no arguments are passed to the decoder.
 *		This is achieved by : 
 *          a) Passing the JPEG-2000 input stream bytes via a call to setInputSream(inputBytes).
 *             The input stream MUST BE a J2K, a JP2 or JPT / JPIP stream.
 *             In case the input stream is a JPT / JPIP stream, a call to setInputJPT() must also be made afterwards.
 *          b) Setting the wanted format of the output stream via a call to setOutputFormat(format).
 *             Only supported output format at the moment is : .bmp.
 *          c) Optionally, setting quiet mode to avoid getting logging messages from C Code via a call to setQuiet().
 *          d) Optionally, setting locking mode to avoid C internal buffers from being swapped. (not yet implemented)
 *             This is usually necessary when dealing with sensitive data, and is done via a call to setLocking().
 *          e) Calling decodeJ2KtoImage().
 *			f) Finally, retrieving the output stream as a byte array via a call to getOutputStreamBytes().
 *
 * Note that forcing output precision, upsampling and forcing RGB output are not supported when using the 2nd way.
 *
 * To be able to log messages, the caller must register a IJavaJ2KDecoderLogger object when 
 *	instantiating the class object by passing it as an argument, in addition to the absolute path of the shared library.
 */
public class OpenJPEGJavaDecoder
{
	public interface IJavaJ2KDecoderLogger
    {
		public void logDecoderMessage(String message);
		public void logDecoderError(String message);
	}
    
	/** Whether the object class has already been initialized */
    private static boolean isInitialized = false;
    
	/** <============ decompression parameters =============> */
	/** These value can be set via a call to setDecoderArguments() */
    private String[] decoder_arguments = null;

	/** Holds the J2K / JP2 / JPT bytecode to decode / decompress ; set by a call to setInputStream() */
    private byte[] inputStream = null;

    /** Holds the output format wanted from <.BMP> ; set by a call to setOutputFormat()  */
    private String outputFormat = null;

    /** Determines whether the input stream is known to be a JPT (JPIP) stream ; set by a call to setInputIsJPT() */
    private int isInputJPT = 0;

    /** Determines whether or not to print messages by the C code ; set by a call to setQuiest() */
    private int quiet = 0;

    /** Determines whether or not to use locking in the C Code ; set by a call to setLocking() ; not implemented yet */
    private int locking = 0;

	/** Holds the decoded / decompressed version of the input stream ; set by a call from the C Code to writeToOutputStream() */
	private ByteArrayOutputStream outputStream = null;
	private byte[] outputStreamBytes = null;

	/** Holds the size of the output stream */
	private int outputStreamSize = 0;

	/** Vector of loggers */
    private Vector<IJavaJ2KDecoderLogger> loggers = new Vector<IJavaJ2KDecoderLogger>();

	/**
	 * This parameter is never used in Java but is read by the C library to know the number of resolutions to skip when decoding,
	 * i.e. if there are 5 resolutions and skipped=1 ==> decode until resolution 4.
	 */
	private int skippedResolutions = 0;

	/* -------------- The following vars are deprecated / never used ----------------------- */

	/** Holds the compressed version of the index file, to be used by the decoder */
    private byte inputIndex[] = null;

	/** Number of resolutions decompositions */
	private int nbResolutions = -1;

	/** The quality layers */
	private int[] layers = null;

	/** Hold Width, Height and Depth of the resulting output stream */
    private int width = -1;
    private int height = -1;
    private int depth = -1;

	/* -------------------------------------------------------------------------------------- */

    public OpenJPEGJavaDecoder(String openJPEGlibraryFullPathAndName, IJavaJ2KDecoderLogger messagesAndErrorsLogger) throws ExceptionInInitializerError
    {
    	this(openJPEGlibraryFullPathAndName);
    	loggers.addElement(messagesAndErrorsLogger);
    }

    public OpenJPEGJavaDecoder(String openJPEGlibraryFullPathAndName) throws ExceptionInInitializerError
    {
    	if (!isInitialized)
    	{
    		try {
    			System.load(openJPEGlibraryFullPathAndName);
    			isInitialized = true;
    		} catch (Throwable t)
            {
    			throw new ExceptionInInitializerError("OpenJPEG Java Decoder: probably impossible to find the C library");
    		}
    	}
    }
    
    public void addLogger(IJavaJ2KDecoderLogger messagesAndErrorsLogger)
    {
    	loggers.addElement(messagesAndErrorsLogger);
    }
    public void removeLogger(IJavaJ2KDecoderLogger messagesAndErrorsLogger)
    {
    	loggers.removeElement(messagesAndErrorsLogger);
    }

	/** Sets all the decoding arguments */
	public void setDecoderArguments(String[] argumentsForTheDecoder) 
	{
		decoder_arguments = argumentsForTheDecoder;
	}

	/** Sets the input stream to be decoded / decompressed */
	public void setInputStream(byte[] inputStream) throws IllegalArgumentException
	{
		if (inputStream != null && inputStream.length != 0)
			this.inputStream = inputStream;
		else
			throw new IllegalArgumentException("Input stream cannot be null or empty");
	}

	/** Sets the output format */
	public void setOutputFormat(String outputFormat) throws IllegalArgumentException
	{
		if (outputFormat.equalsIgnoreCase(".bmp"))
			this.outputFormat = outputFormat;
		else
			throw new IllegalArgumentException("Unsupported output format. Only supported output formats are : .bmp");
	}

    /** Called when the input stream is known to be of a JPT (JPIP) image */
	public void setInputIsJPT()
    {
        this.isInputJPT = 1;
    }

	/** Activates quiet mode */
	public void setQuiet()
	{
		this.quiet = 1;
	}

	/** Activated locking in C code (not implemented yet) */
	public void setLocking()
	{
		this.locking = 1;
	}

	/**
	 * Writes to outputStream
	 */
	public int writeToOutputStream(byte[] data, int len)
	{
		int lastSize = outputStream.size();
		int currentSize = lastSize;

		try
		{
			outputStream.write(data, 0, len);
		}
		catch(Exception e)
		{
			return -1;
		}

		currentSize = outputStream.size();
		return currentSize - lastSize;
	}

	/** Main method */
    public int decodeJ2KtoImage()
    {
		String[] arguments = new String[0 + (decoder_arguments != null ? decoder_arguments.length : 0)];
		int offset = 0;

		if (decoder_arguments != null)
		{
			for (int i=0; i<decoder_arguments.length; i++)
				arguments[i+offset] = decoder_arguments[i];
		}

		ByteBuffer inputByteBufer = ByteBuffer.allocateDirect(inputStream.length);
		inputByteBufer.put(inputStream);

		outputStream = new ByteArrayOutputStream();
		int res = internalDecodeJ2KtoImage(arguments, inputByteBufer);

		inputByteBufer.clear();

		if (res != 0)
		{
			logMessage("Decoding failed");
			outputStreamBytes = null;
			outputStreamSize = 0;
		}

		else
		{
			logMessage("Decoding succeeded");
			outputStreamBytes = outputStream.toByteArray();
			outputStreamSize = outputStreamBytes.length;
		}

		outputStream.reset();

		try
		{
			outputStream.close();
		}
		catch(Exception e)
		{
			logMessage("Closing stream failed");
		}

		return res;
    }
    
    /** 
     * Decode the j2k/jp2/jpt stream given in the ByteBuffer and fills the outputStream array with the byte representation of the .bmp result of the decoding.
     */
    private native int internalDecodeJ2KtoImage(String[] parameters, ByteBuffer inputStream);

	/**
	 * Returns the result
	 */
	public byte[] getOutputStreamBytes()
	{
		return outputStreamBytes;
	}
	
	/** This method is called either directly or by the C methods */
	public void logMessage(String message) 
	{
		for (IJavaJ2KDecoderLogger logger:loggers)
			logger.logDecoderMessage(message);
	}
	
	/** This method is called either directly or by the C methods */
	public void logError(String error) 
	{
		for (IJavaJ2KDecoderLogger logger:loggers)
			logger.logDecoderError(error);
	}

	public void reset() {
		nbResolutions = -1;
		layers = null;
		outputStream = null;
		inputStream = null;
		outputStreamBytes = null;
	    isInputJPT = 0;
	    outputFormat = "";
	    width = -1;
	    height = -1;
	    depth = -1;
	}
	
	/** 
	 * Sets the compressed version of the index file for this image.
	 * This index file is used by the decompressor
	 */
	public void setInputIndex(byte[] inputIndex) 
	{
		this.inputIndex = inputIndex;
	}
}
