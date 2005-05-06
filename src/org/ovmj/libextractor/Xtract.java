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

/**
 * Java version of extract.  This is just a tiny demo-application
 * to show how to use the Java API.  The C version of extract is
 * much better and should probably be used in practice.
 * 
 * @author Christian Grothoff
 */
public final class Xtract {

    public static void main(String[] args) {	
	Extractor ex = Extractor.getDefault();
	for (int i=0;i<args.length;i++) {
	    Vector keywords = ex.extract(args[i]);
	    System.out.println("Keywords for " + args[i] + ":\n");
	    for (int j=0;j<keywords.size();j++)
		System.out.println(keywords.elementAt(j));
	} 	
	// no need to unload, finalizer does the rest...
    }

} // end of Xtract