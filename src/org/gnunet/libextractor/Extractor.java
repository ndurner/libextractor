/*
     This file is part of libextractor.
     (C) 2002, 2003, 2004 Vidyut Samanta and Christian Grothoff

     libextractor is free software; you can redistribute it and/or modify
     it under the terms of the GNU General Public License as published
     by the Free Software Foundation; either version 2, or (at your
     option) any later version.

     libextractor is distributed in the hope that it will be useful, but
     WITHOUT ANY WARRANTY; without even the implied warranty of
     MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
     General Public License for more details.

     You should have received a copy of the GNU General Public License
     along with libextractor; see the file COPYING.  If not, write to the
     Free Software Foundation, Inc., 59 Temple Place - Suite 330,
     Boston, MA 02111-1307, USA.
 */
package org.gnunet.libextractor;

import java.util.Vector;
import java.io.File;

/**
 * Java Binding for libextractor.
 *
 * @see Xtract
 * @author Christian Grothoff
 */ 
public final class Extractor {

    private static boolean warn_;

    /**
     * LE version.  0 if LE was compiled without JNI/Java support, in which
     * case we better not call any native methods...
     */
    private final static int version_;

    /**
     * Cached list of Strings describing keyword types.
     */
    private final static String[] typeCache_;

    static {	
	// first, initialize warn_
	boolean warn = false;
	try {
	    if (System.getProperty("libextractor.warn") != null)
		warn = true;
	} catch (SecurityException se) {
	    // ignore
	} finally {
	    warn_ = warn;
	}

	// next, load library and determine version_
	int ver = 0;
	try {
	    System.loadLibrary("extractor");
	} catch (UnsatisfiedLinkError ule) {
	    ver = -1;
	    warn("Did not find libextractor library: " + ule);
	}
	if (ver == 0) {
	    try {
		ver = getVersionInternal();
	    } catch (UnsatisfiedLinkError ule) {
		// warn: libextractor compiled without Java support
		warn("libextractor library compiled without Java support: " + ule);
	    }
	}
	version_ = ver;

	// finally, initialize typeCache_
	if (ver > 0) {
	    typeCache_ = new String[getMaxTypeInternal()];
	} else {
	    typeCache_ = null;
	}
    }    

    private static void warn(String warning) {
	if (warn_)
	    System.err.println("WARNING: " + warning);
    }

    /**
     * @return -1 if LE library was not found, 0 if LE library
     *  was found but compiled without JNI support, otherwise
     *  the LE version number
     */
    public static int getVersion() {
	return version_;
    }

    /**
     * Get the 'default' extractor, that is an extractor that loads
     * the default set of extractor plugins.
     */
    public static Extractor getDefault() {
	if (version_ > 0)
	    return new Extractor(loadDefaultInternal());
	else
	    return new Extractor(0);
    }

    /**
     * Get the 'empty' extractor, that is an extractor that does not
     * have any plugins loaded.  This is useful to manually construct
     * an Extractor from scratch.
     */
    public static Extractor getEmpty() {
	return new Extractor(0L);
    }

    /**
     * Note that the current implementation of function is quite
     * costly, we should probably build a cache (String[]) of all
     * keyword types on startup and keep that around instead of
     * doing a native call each time (initialize cache lazily!,
     * just determine the size in the static initializer!).
     *
     * @throws IllegalArgumentException if the type is not within range
     * @return an empty string if LE was not loaded
     */
    public static String getTypeAsString(int type) {
	if (version_ > 0) {
	    if ( (type >= 0) && (type <= typeCache_.length)) {
		if (typeCache_[type] == null)
		    typeCache_[type]
			= getTypeAsStringInternal(type);
		return typeCache_[type];
	    } else
		throw new IllegalArgumentException("Type out of range [0,"+typeCache_.length+")");
	} else
	    return "";
    }

    /**
     * Handle to the list of plugins (a C pointer, long to support
     * 64-bit architectures!).
     */
    private long pluginHandle_;

    /**
     * Creates an extractor.
     *
     * @param pluginHandle the internal handle (C pointer!) refering
     *   to the list of plugins.  0 means no plugins.
     */
    private Extractor(long pluginHandle) {
	this.pluginHandle_ = pluginHandle;
    }

    protected void finalize() {
	if (pluginHandle_ != 0)
	    unloadInternal(pluginHandle_);
    }

    public void unloadPlugin(String pluginName) {
	if (pluginHandle_ != 0) {
	    pluginHandle_ = unloadPlugin(pluginHandle_,
					 pluginName);
	}
    }

    /**
     * @param append if true, add the plugin at the end, otherwise at the
     *        beginning
     */
    public void loadPlugin(String pluginName,
			   boolean append) {
	if (version_ <= 0)
	    return; 
	pluginHandle_ = loadPlugin(pluginHandle_,
				   pluginName,
				   append);
    }

    /**
     * Extract keywords (meta-data) from the given file.
     *
     * @param f the file to extract meta-data from
     * @return a Vector of Extractor.Keywords
     */
    public Vector extract(File f) {
	return extract(f.getName());
    }
    
    /**
     * Extract keywords (meta-data) from the given file.
     *
     * @param file the name of the file
     * @return a Vector of Extractor.Keywords
     */
    public Vector extract(String filename) {
	if (pluginHandle_ == 0)
	    return new Vector(0); // fast way out
	long list
	    = extractInternal(pluginHandle_,
			      filename); // toChars?
	long pos 
	    = list;
	Vector res 
	    = new Vector();
	while (pos != 0) {
	    int type 
		= typeInternal(pos);
	    String keyword
		= keywordInternal(pos);
	    res.add(new Keyword(type, keyword));
	    pos = nextInternal(pos);
	}
	freeInternal(list);
	return res;
    }

    
    /* ********************* native calls ******************** */

    private static native long loadDefaultInternal();

    private static native void unloadInternal(long handle);
    
    private static native long extractInternal(long handle,
					       String filename);

    // free memory allocated by extractInternal
    private static native void freeInternal(long list);

    private static native int typeInternal(long pos);

    private static native String keywordInternal(long pos);

    private static native long nextInternal(long pos);

    private static native String getTypeAsStringInternal(int type);

    private static native int getVersionInternal();

    private static native int getMaxTypeInternal();

    private static native long unloadPlugin(long handle,
					    String pluginName);

    /**
     * @param append if true, add the plugin at the end, otherwise at the
     *        beginning
     */
    private static native long loadPlugin(long handle,
					  String pluginName,
					  boolean append);

    /**
     * Representation of a keyword.  Each keyword in libextractor
     * has a type (in Java modeled as an int) which describes what
     * the keyword is about.
     * 
     * @author Christian Grothoff
     */
    public static final class Keyword {

	private final int type_;

	private final String keyword_;

	Keyword(int type,
		String key) {
	    this.type_ = type;
	    this.keyword_ = key;
	}

	public String toString() {
	    return getTypeAsString(type_) + ": " + keyword_;
	}

	public int getType() {
	    return type_;
	}

	public String getKeyword() {
	    return keyword_;
	}

    } // end of Extractor.Keyword

} // end of Extractor