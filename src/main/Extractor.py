"""Extractor.py

Modul docstring...
"""

import _extractor

__all__ = ["Extractor","Keyword"]
__author__ = "Christian Grothoff, Heiko Wundram"
__version__ = "0.5.0"
__license__ = "GPL"
__date__ = "5/5/2005"

class Extractor(object):
    """
    """
    
    def __init__(self):
        self.__plugins = _extractor.loadDefaultLibraries()
    def __del__(self):
        _extractor.removeAll(self.__plugins)
#    def load(plugs):
#        self.__plugins = _extractor.load(self.__plugins, plugs)
#        return None
#    def unload(plugs):
#        self.__plugins = _extractor.unload(self.__plugins, plugs)
#        return None
    def extract(self,filename):
        """Pass a filename to extract keywords.
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
