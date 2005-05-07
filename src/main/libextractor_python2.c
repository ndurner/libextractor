/* libextractor_python.c

libextractor-Binding to Python. */

#include <Python.h>
#include "extractor.h"

/* Typedefs. */

typedef struct {
  PyObject_HEAD
  PyObject *moduleList;
} Extractor;

typedef struct {
  PyObject_HEAD
  EXTRACTOR_ExtractorList *module;
} Module;

typedef struct {
  PyObject_HEAD
  PyObject *keywordList;
} KeywordList;

typedef struct {
  PyObject_HEAD
  EXTRACTOR_KeywordList *keyword;
} Keyword;

/* Types. */

static PyTypeObject ExtractorType;
static PyTypeObject ModuleType;
static PyTypeObject KeywordListType;
static PyTypeObject KeywordType;

/* Extractor type declarations. */

static PyObject *Extractor_new(PyTypeObject *type, PyObject *args,
			       PyObject *kwargs)
{
  Extractor *self = NULL;

  if( !( self = (Extractor*)type->tp_alloc(type,0) ) )
    goto error;
  if( !( self->moduleList = PyList_New(0) ) )
    goto error;

  goto finish;

 error:
  Py_XDECREF(self);
  self = NULL;

 finish:
  return (PyObject*)self;
}

static int Extractor_init(Extractor *self, PyObject *args, PyObject *kwargs)
{
  PyObject *conf = NULL, *conf_iter = NULL, *conf_item = NULL;
  EXTRACTOR_ExtractorList *elist = NULL, *ecur = NULL;
  Module *cur_mod = NULL;
  char *conf_str = NULL;
  char *kwargs_list[] = {"config",NULL};
  int rv = 0;

  if( !PyArg_ParseTupleAndKeywords(args,kwargs,"|s:__init__",kwargs_list,
				   &conf_str) ) {
    PyErr_Clear();
    if( !PyArg_ParseTupleAndKeywords(args,kwargs,"O:__init__",kwargs_list,
				     &conf) )
      goto error;
  }

  if( !conf ) {
    if( conf_str )
      elist = EXTRACTOR_loadConfigLibraries(NULL,conf_str);
    else
      elist = EXTRACTOR_loadDefaultLibraries();

    ecur = elist;
    while( ecur ) {
      elist = ecur->next;

      if( !( cur_mod = PyObject_NEW(Module,&ModuleType) ) )
	goto error;
      cur_mod->module = ecur;

      if( PyList_Append(self->moduleList,(PyObject*)cur_mod) )
	goto error;

      ecur->next = NULL;
      ecur = elist;

      Py_DECREF(cur_mod);
      cur_mod = NULL;
    }
  } else {
    if( !( conf_iter = PyObject_GetIter(conf) ) ) {
      PyErr_Clear();

      if( !PyObject_IsInstance(conf,(PyObject*)&ModuleType) )
	goto error;

      if( PyList_Append(self->moduleList,conf) )
	goto error;
    } else {
      while( ( conf_item = PyIter_Next(conf_iter) ) ) {
	if( !( conf_str = PyString_AsString(conf_item) ) ) {
	  if( !PyObject_IsInstance(conf_item,(PyObject*)&ModuleType) )
	    goto error;

	  if( PyList_Append(self->moduleList,conf_item) )
	    goto error;
	} else {
	  elist = EXTRACTOR_addLibrary(NULL,conf_str);
	  if( elist ) {
	    if( !( cur_mod = PyObject_NEW(Module,&ModuleType) ) )
	      goto error;
	    cur_mod->module = elist;

	    if( PyList_Append(self->moduleList,(PyObject*)cur_mod) )
	      goto error;

	    Py_DECREF(cur_mod);
	    cur_mod = NULL;
	  }
	}

	Py_DECREF(conf_item);
	conf_item = NULL;
      }

      Py_DECREF(conf_iter);
      conf_iter = NULL;
    }
  }

  goto finish;

 error:
  if( ecur )
    EXTRACTOR_removeAll(ecur);
  Py_XDECREF(cur_mod);
  Py_XDECREF(conf_item);
  rv = -1;

 finish:
  return rv;
}

static PyObject *Extractor_iter(Extractor *self)
{
  return PyObject_GetIter(self->moduleList);
}

static void Extractor_dealloc(Extractor *self)
{
  Py_DECREF(self->moduleList);
  self->ob_type->tp_free((PyObject*)self);
}

static PyObject *Extractor_extract(Extractor *self, PyObject *args,
				   PyObject *kwargs)
{
  PyObject *mlist = NULL;
  int i = 0, mlist_len = 0;
  Module *mlist_curitem = NULL;
  EXTRACTOR_ExtractorList *efirst = NULL, *elist = NULL;
  EXTRACTOR_KeywordList *kwlist = NULL;
  KeywordList *rv = NULL;
  Keyword *kw = NULL;
  char *filename = NULL;
  char *kwargs_list[] = {"filename",NULL};

  if( !PyArg_ParseTupleAndKeywords(args,kwargs,"s:extract",kwargs_list,
				   &filename) )
    goto error;

  mlist_len = PyList_Size(self->moduleList);
  if( !( mlist = PyList_New(mlist_len) ) )
    goto error;

  for( i = 0; i < mlist_len; i++ ) {
    mlist_curitem = (Module*)PyList_GET_ITEM(self->moduleList,i);
    Py_INCREF(mlist_curitem);
    PyList_SET_ITEM(mlist,i,(PyObject*)mlist_curitem);
    if( !efirst )
      efirst = elist = malloc(sizeof(EXTRACTOR_ExtractorList));
    else {
      elist->next = malloc(sizeof(EXTRACTOR_ExtractorList));
      elist = elist->next;
    }
    memcpy(elist,mlist_curitem->module,sizeof(EXTRACTOR_ExtractorList));
  }

  Py_BEGIN_ALLOW_THREADS;
  kwlist = EXTRACTOR_getKeywords(efirst,filename);
  Py_END_ALLOW_THREADS;

  if( !( rv = PyObject_NEW(KeywordList,&KeywordListType) ) )
    goto error;
  rv->keywordList = NULL;
  if( !( rv->keywordList = PyList_New(0) ) )
    goto error;

  while( kwlist ) {
    if( !( kw = PyObject_NEW(Keyword,&KeywordType) ) )
      goto error;
    kw->keyword = kwlist;
    kwlist = kwlist->next;
    kw->keyword->next = NULL;

    if( PyList_Append(rv->keywordList,(PyObject*)kw) )
      goto error;

    Py_DECREF(kw);
    kw = NULL;
  }

  goto finish;

 error:
  Py_XDECREF(kw);
  if( kwlist )
    EXTRACTOR_freeKeywords(kwlist);
  Py_XDECREF(rv);
  rv = NULL;

 finish:
  Py_XDECREF(mlist);
  return (PyObject*)rv;
}

static PyMethodDef Extractor_methods[] = {
  {"extract",(PyCFunction)Extractor_extract,METH_VARARGS|METH_KEYWORDS,
   "Extract data from file given as filename."},
  {NULL}  /* Sentinel */
};

static PyTypeObject ExtractorType = {
    PyObject_HEAD_INIT(NULL)
    0,                         /*ob_size*/
    "extractor.Extractor",     /*tp_name*/
    sizeof(Extractor),         /*tp_basicsize*/
    0,                         /*tp_itemsize*/
    (destructor)Extractor_dealloc, /*tp_dealloc*/
    0,                         /*tp_print*/
    0,                         /*tp_getattr*/
    0,                         /*tp_setattr*/
    0,                         /*tp_compare*/
    0,                         /*tp_repr*/
    0,                         /*tp_as_number*/
    0,                         /*tp_as_sequence*/
    0,                         /*tp_as_mapping*/
    0,                         /*tp_hash */
    0,                         /*tp_call*/
    0,                         /*tp_str*/
    0,                         /*tp_getattro*/
    0,                         /*tp_setattro*/
    0,                         /*tp_as_buffer*/
    Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE, /*tp_flags*/
    "Extractor objects",       /* tp_doc */
    0,		               /* tp_traverse */
    0,		               /* tp_clear */
    0,		               /* tp_richcompare */
    0,		               /* tp_weaklistoffset */
    (getiterfunc)Extractor_iter, /* tp_iter */
    0,		               /* tp_iternext */
    Extractor_methods,         /* tp_methods */
    0,                         /* tp_members */
    0,                         /* tp_getset */
    0,                         /* tp_base */
    0,                         /* tp_dict */
    0,                         /* tp_descr_get */
    0,                         /* tp_descr_set */
    0,                         /* tp_dictoffset */
    (initproc)Extractor_init,  /* tp_init */
    0,                         /* tp_alloc */
    Extractor_new,             /* tp_new */
};

/* Module type. */

/* KeywordList type. */

static PyObject *KeywordList_new(PyTypeObject *type, PyObject *args,
				 PyObject *kwargs)
{
  KeywordList *self = NULL;

  if( !( self = (KeywordList*)type->tp_alloc(type,0) ) )
    goto error;
  if( !( self->keywordList = PyList_New(0) ) )
    goto error;

  goto finish;

 error:
  Py_XDECREF(self);
  self = NULL;

 finish:
  return (PyObject*)self;
}

static int KeywordList_init(KeywordList *self, PyObject *args,
			    PyObject *kwargs)
{
  PyObject *kw = NULL, *kw_iter = NULL, *kw_item = NULL;
  Keyword *cur_kw = NULL;
  int curtype = 0;
  char *curvalue = NULL;
  char *kwargs_list[] = {"keywords",NULL};
  int rv = 0;

  if( !PyArg_ParseTupleAndKeywords(args,kwargs,"|O:__init__",kwargs_list,
				   &kw) )
    goto error;

  if( kw )
    if( PyObject_IsInstance(kw,(PyObject*)&KeywordType) ) {
      if( PyList_Append(self->keywordList,kw) )
	goto error;
    } else if( PyArg_ParseTuple(kw,"(is)",&curtype,&curvalue) ) {
      if( !( cur_kw = PyObject_NEW(Keyword,&KeywordType) ) )
	goto error;
      cur_kw->keyword = NULL;

      cur_kw->keyword = malloc(sizeof(EXTRACTOR_KeywordList));
      cur_kw->keyword->keyword = strdup(curvalue);
      cur_kw->keyword->keywordType = curtype;
      cur_kw->keyword->next = NULL;

      if( PyList_Append(self->keywordList,(PyObject*)cur_kw) )
	goto error;

      Py_DECREF(cur_kw);
      cur_kw = NULL;
    } else {
      PyErr_Clear();
      if( !( kw_iter = PyObject_GetIter(kw) ) )
	goto error;

      while( ( kw_item = PyIter_Next(kw_iter) ) ) {
	if( PyObject_IsInstance(kw_item,(PyObject*)&KeywordType) ) {
	  if( PyList_Append(self->keywordList,kw_item) )
	    goto error;
	} else {
	  if( !PyArg_ParseTuple(kw_item,"(is)",&curtype,&curvalue) )
	    goto error;

	  if( !( cur_kw = PyObject_NEW(Keyword,&KeywordType) ) )
	    goto error;
	  cur_kw->keyword = NULL;

	  cur_kw->keyword = malloc(sizeof(EXTRACTOR_KeywordList));
	  cur_kw->keyword->keyword = strdup(curvalue);
	  cur_kw->keyword->keywordType = curtype;
	  cur_kw->keyword->next = NULL;

	  if( PyList_Append(self->keywordList,(PyObject*)cur_kw) )
	    goto error;

	  Py_DECREF(cur_kw);
	  cur_kw = NULL;
	}

	Py_DECREF(kw_item);
	kw_item = NULL;
      }

      Py_DECREF(kw_iter);
      kw_iter = NULL;
    }

  goto finish;

 error:
  Py_XDECREF(cur_kw);
  Py_XDECREF(kw_item);
  rv = -1;

 finish:
  return rv;
}

static PyObject *KeywordList_iter(KeywordList *self)
{
  return PyObject_GetIter(self->keywordList);
}

static void KeywordList_dealloc(KeywordList *self)
{
  Py_XDECREF(self->keywordList);
  self->ob_type->tp_free((PyObject*)self);
}

static PyMethodDef KeywordList_methods[] = {
  {NULL}  /* Sentinel */
};

static PyTypeObject KeywordListType = {
    PyObject_HEAD_INIT(NULL)
    0,                         /*ob_size*/
    "extractor.KeywordList",   /*tp_name*/
    sizeof(KeywordList),       /*tp_basicsize*/
    0,                         /*tp_itemsize*/
    (destructor)KeywordList_dealloc, /*tp_dealloc*/
    0,                         /*tp_print*/
    0,                         /*tp_getattr*/
    0,                         /*tp_setattr*/
    0,                         /*tp_compare*/
    0,                         /*tp_repr*/
    0,                         /*tp_as_number*/
    0,                         /*tp_as_sequence*/
    0,                         /*tp_as_mapping*/
    0,                         /*tp_hash */
    0,                         /*tp_call*/
    0,                         /*tp_str*/
    0,                         /*tp_getattro*/
    0,                         /*tp_setattro*/
    0,                         /*tp_as_buffer*/
    Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE, /*tp_flags*/
    "KeywordList objects",     /* tp_doc */
    0,		               /* tp_traverse */
    0,		               /* tp_clear */
    0,		               /* tp_richcompare */
    0,		               /* tp_weaklistoffset */
    (getiterfunc)KeywordList_iter, /* tp_iter */
    0,		               /* tp_iternext */
    KeywordList_methods,       /* tp_methods */
    0,                         /* tp_members */
    0,                         /* tp_getset */
    0,                         /* tp_base */
    0,                         /* tp_dict */
    0,                         /* tp_descr_get */
    0,                         /* tp_descr_set */
    0,                         /* tp_dictoffset */
    (initproc)KeywordList_init, /* tp_init */
    0,                         /* tp_alloc */
    KeywordList_new,           /* tp_new */
};

/* Keyword type. */

static PyObject *Keyword_new(PyTypeObject *type, PyObject *args,
			     PyObject *kwargs)
{
  Keyword *self = NULL;
  char *name = NULL;
  char *kwargs_list[] = {"name",NULL};

  if( !( self = (Keyword*)type->tp_alloc(type,0) ) )
    goto error;
  if( !( self->keyword = malloc(sizeof(EXTRACTOR_KeywordList)) ) )
    goto error;

  self->keyword->keyword = strdup("");
  self->keyword->keywordType = 0;
  self->keyword->next = NULL;

  goto finish;

 error:
  Py_XDECREF(self);
  self = NULL;

 finish:
  return (PyObject*)self;
}

static int Keyword_init(Keyword *self, PyObject *args, PyObject *kwargs)
{
  int type = 0;
  char *value = NULL;
  char *kwargs_list[] = {"type","value",NULL};
  int rv = 0;

  if( !PyArg_ParseTupleAndKeywords(args,kwargs,"is:__init__",kwargs_list,
				   &type,&value) ) {
    PyErr_Clear();
    if( !PyArg_ParseTupleAndKeywords(args,kwargs,":__init__",kwargs_list) )
      goto error;

    goto finish;
  }

  free(self->keyword->keyword);
  self->keyword->keyword = strdup(value);
  self->keyword->keywordType = type;

  goto finish;

 error:
  rv = -1;

 finish:
  return rv;
}

static void Keyword_dealloc(Keyword *self)
{
  EXTRACTOR_freeKeywords(self->keyword);
  self->ob_type->tp_free((PyObject*)self);
}

static PyObject *Keyword_getType(Keyword *self, PyObject *args)
{
  return PyInt_FromLong(self->keyword->keywordType);
}

static PyObject *Keyword_getValue(Keyword *self, PyObject *args)
{
  return PyString_FromString(self->keyword->keyword);
}

static PyMethodDef Keyword_methods[] = {
  {"getType",(PyCFunction)Keyword_getType,METH_NOARGS,
   "Retrieve type of keyword."},
  {"getValue",(PyCFunction)Keyword_getValue,METH_NOARGS,
   "Retrieve value of keyword."},
  {NULL}  /* Sentinel */
};

static PyTypeObject KeywordType = {
    PyObject_HEAD_INIT(NULL)
    0,                         /*ob_size*/
    "extractor.Keyword",        /*tp_name*/
    sizeof(Keyword),           /*tp_basicsize*/
    0,                         /*tp_itemsize*/
    (destructor)Keyword_dealloc, /*tp_dealloc*/
    0,                         /*tp_print*/
    0,                         /*tp_getattr*/
    0,                         /*tp_setattr*/
    0,                         /*tp_compare*/
    0,                         /*tp_repr*/
    0,                         /*tp_as_number*/
    0,                         /*tp_as_sequence*/
    0,                         /*tp_as_mapping*/
    0,                         /*tp_hash */
    0,                         /*tp_call*/
    0,                         /*tp_str*/
    0,                         /*tp_getattro*/
    0,                         /*tp_setattro*/
    0,                         /*tp_as_buffer*/
    Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE, /*tp_flags*/
    "Keyword objects",         /* tp_doc */
    0,		               /* tp_traverse */
    0,		               /* tp_clear */
    0,		               /* tp_richcompare */
    0,		               /* tp_weaklistoffset */
    0,                         /* tp_iter */
    0,		               /* tp_iternext */
    Keyword_methods,           /* tp_methods */
    0,                         /* tp_members */
    0,                         /* tp_getset */
    0,                         /* tp_base */
    0,                         /* tp_dict */
    0,                         /* tp_descr_get */
    0,                         /* tp_descr_set */
    0,                         /* tp_dictoffset */
    (initproc)Keyword_init,    /* tp_init */
    0,                         /* tp_alloc */
    Keyword_new,               /* tp_new */
};

/* Module. */
