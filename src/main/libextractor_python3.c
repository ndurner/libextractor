/* libextractor_python.c
   ---------------------

   Implements the Python wrapper for libextractor. The wrapper builds on the
   Python type module, which wraps a single module, over extractor, which
   implements the extractor from modules, up to keyword(list), which implements
   keyword handling. */

/* Includes. */

#include <Python.h>
#include "extractor.h"

/* Typedefs. */

typedef struct {
  PyObject_HEAD
  PyObject *mlist;
  int locks;
} ModuleList;

typedef struct {
  PyObject_HEAD
  EXTRACTOR_ExtractorList *module;
  ModuleList *mlist;
} Module;

/* Type objects. */

static PyTypeObject ModuleListType;
static PyTypeObject ModuleType;

/* Module list type. */

static inline int ModuleList_checkModule(Module *arg)
{
  if( !PyObject_IsInstance((PyObject*)arg,(PyObject*)&ModuleType) ) {
    PyErr_SetString(PyExc_TypeError,"append only accepts a Module.");
    return -1;
  }

  if( arg->mlist ) {
    PyErr_SetString(PyExc_TypeError,"cannot take ownership of module.");
    return -1;
  }

  return 0;
}

static PyObject *ModuleList_prepend(ModuleList *self, Module *arg)
{
  PyObject *rv = NULL;
  Module *first = NULL;
  int mlistlen = 0;

  if( ModuleList_checkModule(arg) )
    goto error;

  mlistlen = PyList_GET_SIZE(self->mlist);
  if( mlistlen ) {
    first = (Module*)PyList_GET_ITEM(self->mlist,0);
    arg->module->next = first->module;
  }

  if( PyList_Insert(self->mlist,0,(PyObject*)arg) )
    goto error;
  arg->mlist = self;
  Py_INCREF(self);

  rv = (PyObject*)arg;
  Py_INCREF(rv);

  goto finish;

 error:
  Py_XDECREF(rv);
  rv = NULL;

 finish:
  return (PyObject*)rv;
}

static PyObject *ModuleList_append(ModuleList *self, Module *arg)
{
  PyObject *rv = NULL;
  Module *last = NULL;
  int mlistlen = 0;

  if( ModuleList_checkModule(arg) )
    goto error;

  mlistlen = PyList_GET_SIZE(self->mlist);
  if( mlistlen ) {
    last = (Module*)PyList_GET_ITEM(self->mlist,mlistlen-1);
    last->module->next = arg->module;
  }

  if( PyList_Append(self->mlist,(PyObject*)arg) )
    goto error;
  arg->mlist = self;
  Py_INCREF(self);

  rv = (PyObject*)arg;
  Py_INCREF(rv);

  goto finish;

 error:
  Py_XDECREF(rv);
  rv = NULL;

 finish:
  return (PyObject*)rv;
}

static PyObject *ModuleList_new(PyTypeObject *type, PyObject *args,
				PyObject *kwargs)
{
  ModuleList *self = NULL;

  if( !( self = (ModuleList*)type->tp_alloc(type,0) ) )
    goto error;
  self->locks = 0;

  if( !( self->mlist = PyList_New(0) ) )
    goto error;

  goto finish;

 error:
  Py_XDECREF(self);
  self = NULL;

 finish:
  return (PyObject*)self;
}

static int ModuleList_init(ModuleList *self, PyObject *args, PyObject *kwargs)
{
  PyObject *mod = NULL, *mod_iter = NULL, *mod_item = NULL;
  EXTRACTOR_ExtractorList *elist = NULL, *ecur = NULL;
  char *kwargs_list[] = {"modules",NULL};
  int rv = 0;

  if( !PyArg_ParseTupleAndKeywords(args,kwargs,"|O:__init__",kwargs_list,
				   &mod) )
    goto error;

  if( !mod || mod == Py_None || PyString_Check(mod) ) {
    if( !mod || mod == Py_None )
      elist = EXTRACTOR_loadDefaultLibraries();
    else
      elist = EXTRACTOR_loadConfigLibraries(NULL,PyString_AsString(mod));

    ecur = elist;
    while( ecur ) {
      if( !( mod_item = (PyObject*)PyObject_GC_New(Module,&ModuleType) ) )
	goto error;

      elist = ecur;
      ecur = elist->next;
      elist->next = NULL;

      ((Module*)mod_item)->module = elist;
      ((Module*)mod_item)->mlist = NULL;

      if( !ModuleList_append(self,(Module*)mod_item) )
	goto error;
      Py_DECREF(mod_item);
      mod_item = NULL;
    }
  } else if( PyObject_IsInstance(mod,(PyObject*)&ModuleType) ) {
    if( !ModuleList_append(self,(Module*)mod) )
      goto error;
  } else {
    if( !( mod_iter = PyObject_GetIter(mod) ) )
      goto error;

    while( ( mod_item = PyIter_Next(mod_iter) ) ) {
      if( !ModuleList_append(self,(Module*)mod_item) )
	goto error;
      Py_DECREF(mod_item);
      mod_item = NULL;
    }
  }

  goto finish;

 error:
  EXTRACTOR_removeAll(ecur);
  Py_XDECREF(mod_item);
  rv = -1;

 finish:
  Py_XDECREF(mod_iter);
  return rv;
}

static PyObject *ModuleList_repr(ModuleList *self)
{
  return PyString_FromFormat("<ModuleList: %i modules>",
			     PyList_GET_SIZE(self->mlist));
}

static int ModuleList_traverse(ModuleList *self, visitproc visit, void *arg)
{
  Py_VISIT(self->mlist);
  return 0;
}

static int ModuleList_clear(ModuleList *self)
{
  Py_CLEAR(self->mlist);
  return 0;
}

static void ModuleList_dealloc(ModuleList *self)
{
  ModuleList_clear(self);
  self->ob_type->tp_free((PyObject*)self);
}

static PyMethodDef ModuleList_methods[] = {
  {"prepend",(PyCFunction)ModuleList_prepend,METH_O,
   "Prepend a single module to the structure."},
  {"append",(PyCFunction)ModuleList_append,METH_O,
   "Append a single module to the structure."},
  {NULL}  /* Sentinel */
};

static PyTypeObject ModuleListType = {
    PyObject_HEAD_INIT(NULL)
    0,                         /*ob_size*/
    "extractor.ModuleList",    /*tp_name*/
    sizeof(ModuleList),        /*tp_basicsize*/
    0,                         /*tp_itemsize*/
    (destructor)ModuleList_dealloc, /*tp_dealloc*/
    0,                         /*tp_print*/
    0,                         /*tp_getattr*/
    0,                         /*tp_setattr*/
    0,                         /*tp_compare*/
    (reprfunc)ModuleList_repr, /*tp_repr*/
    0,                         /*tp_as_number*/
    0,                         /*tp_as_sequence*/
    0,                         /*tp_as_mapping*/
    0,                         /*tp_hash */
    0,                         /*tp_call*/
    0,                         /*tp_str*/
    0,                         /*tp_getattro*/
    0,                         /*tp_setattro*/
    0,                         /*tp_as_buffer*/
    Py_TPFLAGS_DEFAULT | Py_TPFLAGS_HAVE_GC, /*tp_flags*/
    "ModuleList objects",      /* tp_doc */
    (traverseproc)ModuleList_traverse, /* tp_traverse */
    (inquiry)ModuleList_clear, /* tp_clear */
    0,		               /* tp_richcompare */
    0,		               /* tp_weaklistoffset */
    0,                         /* tp_iter */
    0,		               /* tp_iternext */
    ModuleList_methods,        /* tp_methods */
    0,                         /* tp_members */
    0,                         /* tp_getset */
    0,                         /* tp_base */
    0,                         /* tp_dict */
    0,                         /* tp_descr_get */
    0,                         /* tp_descr_set */
    0,                         /* tp_dictoffset */
    (initproc)ModuleList_init, /* tp_init */
    0,                         /* tp_alloc */
    ModuleList_new,            /* tp_new */
};

/* Module type. */

static EXTRACTOR_KeywordList *Module_extractMethod(const char *filename,
						   char *data, size_t filesize,
						   EXTRACTOR_KeywordList *next,
						   const char *options)
{
  Module *self = NULL;

  self = (Module*)atoi(options); /* convert back from string repr of self. */

  printf("In the extractor with object %i.",(int)self);
  return next;
}

static PyObject *Module_new(PyTypeObject *type, PyObject *args,
			    PyObject *kwargs)
{
  Module *self = NULL;
  char *name = NULL, *options = NULL;
  char *kwargs_list[] = {"name","options",NULL};
  int namelen = 0, i;

  if( !PyArg_ParseTupleAndKeywords(args,kwargs,"s#|z:__new__",kwargs_list,
				   &name,&namelen,&options) )
    goto error;

  i = 0;
  while( name[i] )
    if( name[i++] == '(' ) {
      PyErr_SetString(PyExc_ValueError,"name may not contain (.");
      goto error;
    }

  if( !( self = (Module*)type->tp_alloc(type,0) ) )
    goto error;

  /* Somewhat a HACK, creates a module structure from scratch. */
  self->module = malloc(sizeof(EXTRACTOR_ExtractorList));
  self->module->libraryHandle = NULL;
  self->module->extractMethod = (ExtractMethod)&Module_extractMethod;
  self->module->libname = strdup(name);
  self->module->options = malloc(12); /* store self as string in options. */
  sprintf(self->module->options,"%i",(int)self);
  self->module->next = NULL;

  goto finish;

 error:
  Py_XDECREF(self);
  self = NULL;

 finish:
  return (PyObject*)self;
}

static int Module_init(Module *self, PyObject *args, PyObject *kwargs)
{
  char *name = NULL, *options = NULL, *optstring = NULL;
  char *kwargs_list[] = {"name","options",NULL};
  int namelen = 0, optionslen = 0, i, rv = 0;

  if( !PyArg_ParseTupleAndKeywords(args,kwargs,"s#|z#:__init__",kwargs_list,
				   &name,&namelen,&options,&optionslen) )
    goto error;

  i = 0;
  while( options && options[i] )
    if( options[i++] == ')' ) {
      PyErr_SetString(PyExc_ValueError,"option may not contain ).");
      goto error;
    }

  EXTRACTOR_removeAll(self->module); /* slight crutch, was allocated in */
  self->module = NULL; /* __new__, so that programmer can create subtype. */

  optstring = malloc(namelen+optionslen+3);
  if( options )
    sprintf(optstring,"%s(%s)",name,options);
  else
    sprintf(optstring,"%s",name);
  if( !( self->module = EXTRACTOR_loadConfigLibraries(NULL,optstring) ) ) {
    PyErr_SetString(PyExc_ValueError,"could not load module.");
    goto error;
  }

  goto finish;

 error:
  rv = -1;

 finish:
  if( optstring )
    free(optstring);
  return rv;
}

static PyObject *Module_getattr(Module *self, char *name)
{
  if( !strcmp(name,"libname") )
    return PyString_FromString(self->module->libname);
  else if( !strcmp(name,"options") )
    return PyString_FromString(self->module->options);
  else if( !strcmp(name,"mlist") )
    return (PyObject*)self->mlist;
  PyErr_SetString(PyExc_AttributeError,name);
  return NULL;
}

static int Module_setattr(Module *self, char *name, PyObject *value)
{
  if( !strcmp(name,"libname") || !strcmp(name,"options") ||
      !strcmp(name,"mlist") )
    PyErr_Format(PyExc_AttributeError,"cannot set %s.",name);
  else
    PyErr_SetString(PyExc_AttributeError,name);
  return -1;
}

static PyObject *Module_repr(Module *self)
{
  if( self->module->options )
    return PyString_FromFormat("%s(\"%s\",\"%s\")",self->ob_type->tp_name,
			       self->module->libname,self->module->options);
  else
    return PyString_FromFormat("%s(\"%s\")",self->ob_type->tp_name,
			       self->module->libname);
}

static long Module_hash(Module *self)
{
  return (int)self->module;
}

static int Module_traverse(Module *self, visitproc visit, void *arg)
{
  Py_VISIT((PyObject*)self->mlist);
  return 0;
}

static int Module_clear(Module *self)
{
  printf("Removing module in clear: %s.\n",self->module->libname);
  Py_CLEAR(self->mlist);
  return 0;
}

static void Module_dealloc(Module *self)
{
  Module_clear(self);
  printf("Removing module: %s.\n",self->module->libname);
  self->module->next = NULL;
  EXTRACTOR_removeAll(self->module);
  self->ob_type->tp_free((PyObject*)self);
}

static PyMethodDef Module_methods[] = {
  {NULL}  /* Sentinel */
};

static PyTypeObject ModuleType = {
    PyObject_HEAD_INIT(NULL)
    0,                         /*ob_size*/
    "extractor.Module",        /*tp_name*/
    sizeof(Module),            /*tp_basicsize*/
    0,                         /*tp_itemsize*/
    (destructor)Module_dealloc, /*tp_dealloc*/
    0,                         /*tp_print*/
    (getattrfunc)Module_getattr, /*tp_getattr*/
    (setattrfunc)Module_setattr, /*tp_setattr*/
    0,                         /*tp_compare*/
    (reprfunc)Module_repr,     /*tp_repr*/
    0,                         /*tp_as_number*/
    0,                         /*tp_as_sequence*/
    0,                         /*tp_as_mapping*/
    (hashfunc)Module_hash,     /*tp_hash */
    0,                         /*tp_call*/
    0,                         /*tp_str*/
    0,                         /*tp_getattro*/
    0,                         /*tp_setattro*/
    0,                         /*tp_as_buffer*/
    Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE | Py_TPFLAGS_HAVE_GC, /*tp_flags*/
    "Module objects",          /* tp_doc */
    (traverseproc)Module_traverse, /* tp_traverse */
    (inquiry)Module_clear,     /* tp_clear */
    0,		               /* tp_richcompare */
    0,		               /* tp_weaklistoffset */
    0,                         /* tp_iter */
    0,		               /* tp_iternext */
    Module_methods,            /* tp_methods */
    0,                         /* tp_members */
    0,                         /* tp_getset */
    0,                         /* tp_base */
    0,                         /* tp_dict */
    0,                         /* tp_descr_get */
    0,                         /* tp_descr_set */
    0,                         /* tp_dictoffset */
    (initproc)Module_init,     /* tp_init */
    0,                         /* tp_alloc */
    Module_new,                /* tp_new */
};

/* Module level. */

static PyMethodDef Extractor_Module_methods[] = {
  {NULL}  /* Sentinel */
};

PyMODINIT_FUNC initextractor()
{
  PyObject *m;

  if( PyType_Ready(&ModuleListType) )
    return;
  if( PyType_Ready(&ModuleType) )
    return;

  m = Py_InitModule3("extractor",Extractor_Module_methods,"Extractor module.");
  if (m == NULL)
    return;

  Py_INCREF(&ModuleListType);
  Py_INCREF(&ModuleType);
  PyModule_AddObject(m,"ModuleList",(PyObject*)&ModuleListType);
  PyModule_AddObject(m,"Module",(PyObject*)&ModuleType);
}
