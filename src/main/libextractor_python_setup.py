from distutils.core import Extension, setup

cmod = Extension("_extractor",["libextractor_python.c"],
                 libraries=["extractor"],
                 include_dirs=["../include"])

setup(name="Extractor",
      version="0.5.0",
      ext_modules=[cmod],
      py_modules=["Extractor"],
      author="Christian Grothoff, Heiko Wundram",
      author_email="libextractor@gnu.org")

