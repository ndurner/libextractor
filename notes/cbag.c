/*
  cbag.c

  Christian Seberino
  chris@pythonsoft.com
  November 5, 2004

  The cbag module.
*/

#include <Python.h>
#include "Bag.h"

/* Create a Bag object and returns a corresponding PyCObject reference. */

static PyObject* init(PyObject *self, PyObject *args) {
	static struct Bag *my_bag;

	/* Create and set initial values. */

	my_bag = malloc(sizeof(struct Bag));
	my_bag->data[0] = 100;
	my_bag->data[0] = 100;
	my_bag->data[0] = 100;

	/* Returns a PyCObject reference corresponding to my_bag. */

	return PyCObject_FromVoidPtr((void*) my_bag, NULL);
}

/* Set an element of a Bag object array. */

static PyObject* set(PyObject* self, PyObject* args) {
	int        index, new_value;
	PyObject   *py_my_bag;
	struct Bag *my_bag;

	/* Extract C variables from arguments. */

	PyArg_ParseTuple(args, "Oii", &py_my_bag, &index, &new_value);

	/* Extract my_bag from py_my_bag. */

	my_bag = (struct Bag*) PyCObject_AsVoidPtr(py_my_bag);

	/* Set desired element of Bag object array. */

	my_bag->data[index] = new_value;

	/* Increase reference count to the None object. */

	Py_INCREF(Py_None);


	/* Returns a new reference to the None object. */

	return Py_None;
}

/* Get an element of a Bag object array. */

static PyObject* get(PyObject* self, PyObject* args) {
	int        index;
	PyObject   *py_my_bag;
	struct Bag *my_bag;

	/* Extract C variables from arguments. */

	PyArg_ParseTuple(args, "Oi", &py_my_bag, &index);

	/* Extract my_bag from py_my_bag. */

	my_bag = (struct Bag*) PyCObject_AsVoidPtr(py_my_bag);

	/* Return corresponding Python object to element with given index. */

	return Py_BuildValue("i", my_bag->data[index]);
}

/* Creating strings for Python documentation. */

static char cbag_doc[] =
	"Bag functions.\n";

static char init_doc[]  =
	"Create a Bag and return a corresponding object to it.\n";

static char set_doc[]  =
	"Sets an element of Bag object data.\n";

static char get_doc[]  =
	"Get an element of Bag object data.\n";

static PyMethodDef mapper[] = {
	{"init", (PyCFunction) init, METH_VARARGS, init_doc},
	{"set",  (PyCFunction) set,  METH_VARARGS, set_doc},
	{"get",  (PyCFunction) get,  METH_VARARGS, get_doc},
	{NULL}
};

/* Initialization function called when importing module. */

void initcbag() {
	Py_InitModule3("cbag", mapper, cbag_doc);
};
