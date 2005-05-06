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

#include <Python.h>
#include "extractor.h"

static PyObject * EXTRACTOR_PY_loadDefaultLibraries(PyObject * self,
						    PyObject * args) {
  return PyCObject_FromVoidPtr(EXTRACTOR_loadDefaultLibraries(), NULL);
}

static PyObject * EXTRACTOR_PY_removeAll(PyObject * self,
					 PyObject * args) {
  PyObject * py_exts;

  PyArg_ParseTuple(args, "O", &py_exts);

  EXTRACTOR_removeAll((EXTRACTOR_ExtractorList*) PyCObject_AsVoidPtr(py_exts));
  Py_INCREF(Py_None);
  return Py_None;
}

static PyObject * EXTRACTOR_PY_load(PyObject * self,
				    PyObject * args) {
  PyObject * py_exts;
  char * name;
  EXTRACTOR_ExtractorList * plugins;

  PyArg_ParseTuple(args, 
		   "Os", 
		   &py_exts,
		   &name);

  plugins = 
    EXTRACTOR_loadConfigLibraries((EXTRACTOR_ExtractorList*) PyCObject_AsVoidPtr(py_exts),
				  name);
  return PyCObject_FromVoidPtr(plugins, NULL);
}


static PyObject * EXTRACTOR_PY_unload(PyObject * self,
				      PyObject * args) {
  PyObject * py_exts;
  char * name;
  EXTRACTOR_ExtractorList * plugins;

  PyArg_ParseTuple(args, 
		   "Os", 
		   &py_exts,
		   &name);

  plugins = 
    EXTRACTOR_removeLibrary((EXTRACTOR_ExtractorList*) PyCObject_AsVoidPtr(py_exts),
			    name);
  return PyCObject_FromVoidPtr(plugins, NULL);
}


static PyObject * EXTRACTOR_PY_getKeywordTypeAsString(PyObject * self,
						      PyObject * args) {
  unsigned int type;

  PyArg_ParseTuple(args,
		   "i",
		   &type);
  return Py_BuildValue("s",
		       EXTRACTOR_getKeywordTypeAsString((EXTRACTOR_KeywordType)type));
}

static PyObject * EXTRACTOR_PY_extract(PyObject * self,
				       PyObject * args) {
  PyObject * py_exts;
  PyObject * py_clzz;
  PyObject * py_elem;
  char * filename;
  EXTRACTOR_ExtractorList * ex;
  EXTRACTOR_KeywordList * keys;
  EXTRACTOR_KeywordList * pos;
  PyObject * ret;

  PyArg_ParseTuple(args, 
		   "OsO", 
		   &py_exts, 
		   &filename,
		   &py_clzz);
  ex = PyCObject_AsVoidPtr(py_exts);

  Py_BEGIN_ALLOW_THREADS;
  keys = EXTRACTOR_getKeywords(ex,
			       filename);
  Py_END_ALLOW_THREADS;

  ret = PyList_New(0);
  pos = keys;
  while (pos != NULL) {
    py_elem = PyObject_Call(py_clzz, 
 		            Py_BuildValue("(OO)",
				          PyInt_FromLong((long)pos->keywordType),
			                  PyString_FromString(pos->keyword)),
                            NULL);
    PyList_Append(ret,
                  py_elem);
    Py_DECREF(py_elem);
    pos = pos->next;
  }
  EXTRACTOR_freeKeywords(keys);
  return ret;
}

static PyMethodDef ExtractorMethods[] = {
  { "getKeywordTypeAsString", 
    EXTRACTOR_PY_getKeywordTypeAsString,  
    METH_VARARGS,
    "convert a keyword type (int) to the string describing the type" },
  { "loadDefaultLibraries", 
    EXTRACTOR_PY_loadDefaultLibraries,  
    METH_VARARGS,
    "load the default set of libextractor plugins (returns the plugins)" },
  { "removeAll", 
    EXTRACTOR_PY_removeAll,  
    METH_VARARGS,
    "unload the given set of libextractor plugins (pass plugins as argument)" },
  { "load", 
    EXTRACTOR_PY_load,  
    METH_VARARGS,
    "load the given set of libextractor plugins (pass plugins names as argument)" },
  { "unload", 
    EXTRACTOR_PY_unload,  
    METH_VARARGS,
    "unload the given libextractor plugin (pass plugin name as argument)" },
  { "extract", 
    EXTRACTOR_PY_extract,  
    METH_VARARGS,
    "extract meta data from a file (pass plugins and filename as arguments, returns vector of meta-data)" },
  { NULL, NULL, 0, NULL }
};

PyMODINIT_FUNC
init_extractor() {
  Py_InitModule("_extractor", ExtractorMethods);
}

