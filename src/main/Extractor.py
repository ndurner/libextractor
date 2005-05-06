import _extractor

class Extractor(object):
    def __init__(self):
        self.plugins = _extractor.EXTRACTOR_PY_loadDefaultLibraries()
    def __del__(self):
        extractor.EXTRACTOR_PY_removeAll(self.plugins)
#    def load(plugs):
#        self.plugins = _extractor.EXTRACTOR_PY_load(self.plugins, plugs)
#        return None
#    def unload(plugs):
#        self.plugins = _extractor.EXTRACTOR_PY_unload(self.plugins, plugs)
#        return None
    def extract(self,filename):
        return _extractor.EXTRACTOR_PY_extract(self.plugins, filename, Keyword)

class Keyword(object):
    def __init__(self,type,value):
        self.type = type
        self.value = value.decode("utf-8")
    def __repr__(self):
        return u"%s(%i,%s)" % (self.__class__.__name__,self.type,self.value)
    def __str__(self):
        return u"%s: %s" % (self.getType(), self.getValue())
    def getType(self):
        return _extractor.EXTRACTOR_PY_getKeywordTypeAsStringType(self.type).decode("utf-8")
    def getValue(self):
        return self.value
