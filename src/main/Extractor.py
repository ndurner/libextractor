"""Extractor.py

     This file is part of libextractor.
     (C) 2002, 2003, 2004, 2005 Vidyut Samanta and Christian Grothoff

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

libextractor is a simple library for keyword extraction.  libextractor
does not support all formats but supports a simple plugging mechanism
such that you can quickly add extractors for additional formats, even
without recompiling libextractor. libextractor typically ships with a
dozen helper-libraries that can be used to obtain keywords from common
file-types.  

libextractor is a part of the GNU project (http://www.gnu.org/).     
"""

import _extractor

__all__ = ["Extractor","Keyword"]
__author__ = "Christian Grothoff, Heiko Wundram"
__version__ = "0.5.0"
__license__ = "GPL"
__date__ = "5/5/2005"

class Extractor(object):
    """
    Main class for extracting meta-data with GNU libextractor.

    You may create multiple instances of Extractor to use
    different sets of plugins.  Initially each Extractor
    will start with the default set of plugins.

    Use the extract method to obtain keywords from a file.

    Use the load and unload methods to change the list of
    plugins that should be used.
    """
    
    def __init__(self):
        self.__plugins = _extractor.loadDefaultLibraries()
    def __del__(self):
        _extractor.removeAll(self.__plugins)        
    def load(self,plugs):
        """
        Load certain plugins.  Invoke with a string with the names
        of the plugins that should be loaded.  For example,
        
        'libextractor_filename:-libextractor_split'

        will prepend the extractor that just adds the filename as a
        keyword and append (runs last) the extractor that splits
        keywords at whitespaces and punctuations.

        No errors are reported if any of the listed plugins are not
        found.
        """
        self.__plugins = _extractor.load(self.__plugins, plugs)
        return None
    def unload(self,plugs):
        """
        Unload a plugin.  Pass the name of the plugin that is to
        be unloaded.  Only one plugin can be unloaded at a time.
        For example,

        'libextractor_pdf'

        unloads the PDF extractor (if loaded).  No errors are
        reported if no matching plugin is found.
        """
        self.__plugins = _extractor.unload(self.__plugins, plugs)
        return None
    def extract(self,filename):
        """Pass a filename to extract keywords.

        This function returns a list of Keyword objects.
        If the file cannot be opened or cannot be found,
        the list will be empty.  The list can also be empty
        if no metadata was found for the file.
        """
        return _extractor.extract(self.__plugins, filename, Keyword)

class Keyword(object):
    def __init__(self,type,value):
        self.__type = type
        self.__value = value.decode("utf-8")
    def __repr__(self):
        return u"%s(%i,%r)" % (self.__class__.__name__,self.__type,self.__value)
    def __str__(self):
        return u"%s: %s" % (self.__getType(), self.__getValue())
    def __getType(self):
        return _extractor.getKeywordTypeAsString(self.__type).decode("utf-8")
    def __getValue(self):
        return self.__value
    def __hash__(self):
        return hash(self.__value)+self.__type

    type = property(__getType,None,None,"Type of the Keyword (i.e. author, title)")
    value = property(__getValue,None,None,"Value of the Keyword (i.e. 'The GNU GPL')")
