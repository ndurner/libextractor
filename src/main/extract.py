import Extractor
import sys

xtract = Extractor.Extractor()
for arg in sys.argv:
    print "Keywords from " + arg
    keys = xtract.extract(arg);
    for i in keys:
        print i
