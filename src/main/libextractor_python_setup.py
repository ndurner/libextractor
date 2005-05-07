from distutils.core import Extension, setup

cmod = Extension("extractor",["libextractor_python3.c"],
                 libraries=["extractor"],
                 include_dirs=["../include"],
                 library_dirs=["/home/heiko/usr/lib"])

setup(name="Extractor",
      version="0.5.0",
      ext_modules=[cmod],
      author="Christian Grothoff, Heiko Wundram",
      author_email="libextractor@gnu.org")

