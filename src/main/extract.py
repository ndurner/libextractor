import extractor

def getKeywordTypeAsString(t):
    return extractor.EXTRACTOR_PY_getKeywordTypeAsString(t)

class Extractor:
    def __init__(self):
        self.plugins = extractor.EXTRACTOR_PY_loadDefaultLibraries(self)
    def __del__(self):
        extractor.EXTRACTOR_PY_removeAll(self, self.plugins)
    def extract(filename):
        extractor.EXTRACTOR_PY_extract(self, self.plugins, filename)

