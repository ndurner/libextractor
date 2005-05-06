from distutils.core import Extension, setup

cmod = Extension(sources=["libextractor_python.c"],
                 module="_extractor")

setup(name="Extractor",
      version="0.1",
      extension=[cmod]
      sources=["Extractor.py"],
      author="Christian Grothoff, Heiko Wundram")

