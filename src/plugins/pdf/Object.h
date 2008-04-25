//========================================================================
//
// Object.h
//
// Copyright 1996-2003 Glyph & Cog, LLC
//
//========================================================================

#ifndef OBJECT_H
#define OBJECT_H

#include <aconf.h>

#ifdef USE_GCC_PRAGMAS
#pragma interface
#endif

#include <stdio.h>
#include <string.h>
#include "gtypes.h"
#include "gmem.h"
#include "GString.h"

class XRef;
class Array;
class Dict;
class Stream;

//------------------------------------------------------------------------
// Ref
//------------------------------------------------------------------------

struct Ref {
  int num;			// object number
  int gen;			// generation number
};

//------------------------------------------------------------------------
// object types
//------------------------------------------------------------------------

enum ObjType {
  // simple objects
  objBool,			// boolean
  objInt,			// integer
  objReal,			// real
  objString,			// string
  objName,			// name
  objNull,			// null

  // complex objects
  objArray,			// array
  objDict,			// dictionary
  objStream,			// stream
  objRef,			// indirect reference

  // special objects
  objCmd,			// command name
  objError,			// error return from Lexer
  objEOF,			// end of file return from Lexer
  objNone			// uninitialized object
};

#define numObjTypes 14		// total number of object types

//------------------------------------------------------------------------
// Object
//------------------------------------------------------------------------

#ifdef DEBUG_MEM
#define initObj(t) zeroUnion(); ++numAlloc[type = t]
#else
#define initObj(t) zeroUnion(); type = t
#endif

class Object {
public:
  // attempt to clear the anonymous union
  void zeroUnion() { this->name = NULL; }
  // Default constructor.
  Object():
    type(objNone) { zeroUnion(); }

  // Initialize an object.
  Object *initBool(GBool boolnA)
    { initObj(objBool); booln = boolnA; return this; }
  Object *initInt(int intgA)
    { initObj(objInt); intg = intgA; return this; }
  Object *initReal(double realA)
    { initObj(objReal); real = realA; return this; }
  Object *initString(GString *stringA)
    { initObj(objString); string = stringA; return this; }
  Object *initName(char *nameA)
    { initObj(objName); name = copyString(nameA); return this; }
  Object *initNull()
    { initObj(objNull); return this; }
  Object *initArray(XRef *xref);
  Object *initDict(XRef *xref);
  Object *initDict(Dict *dictA);
  Object *initStream(Stream *streamA);
  Object *initRef(int numA, int genA)
    { initObj(objRef); ref.num = numA; ref.gen = genA; return this; }
  Object *initCmd(char *cmdA)
    { initObj(objCmd); cmd = copyString(cmdA); return this; }
  Object *initError()
    { initObj(objError); return this; }
  Object *initEOF()
    { initObj(objEOF); return this; }

  // Copy an object.
  Object *copy(Object *obj);

  // If object is a Ref, fetch and return the referenced object.
  // Otherwise, return a copy of the object.
  Object *fetch(XRef *xref, Object *obj);

  // Free object contents.
  void free();

  // Type checking.
  ObjType getType() { return type; }
  GBool isBool() { return type == objBool; }
  GBool isInt() { return type == objInt; }
  GBool isReal() { return type == objReal; }
  GBool isNum() { return type == objInt || type == objReal; }
  GBool isString() { return type == objString; }
  GBool isName() { return type == objName; }
  GBool isNull() { return type == objNull; }
  GBool isArray() { return type == objArray; }
  GBool isDict() { return type == objDict; }
  GBool isStream() { return type == objStream; }
  GBool isRef() { return type == objRef; }
  GBool isCmd() { return type == objCmd; }
  GBool isError() { return type == objError; }
  GBool isEOF() { return type == objEOF; }
  GBool isNone() { return type == objNone; }

  // Special type checking.
  GBool isName(char *nameA)
    { return type == objName && !strcmp(name, nameA); }
  GBool isDict(char *dictType);
  GBool isStream(char *dictType);
  GBool isCmd(char *cmdA)
    { return type == objCmd && !strcmp(cmd, cmdA); }

  // Accessors.  NB: these assume object is of correct type.
  GBool getBool() { return booln; }
  int getInt() { return intg; }
  double getReal() { return real; }
  double getNum() { return type == objInt ? (double)intg : real; }
  GString *getString() { return string; }
  char *getName() { return name; }
  Array *getArray() { return array; }
  Dict *getDict() { return dict; }
  Stream *getStream() { return stream; }
  Ref getRef() { return ref; }
  int getRefNum() { return ref.num; }
  int getRefGen() { return ref.gen; }
  char *getCmd() { return cmd; }

  // Array accessors.
  int arrayGetLength();
  void arrayAdd(Object *elem);
  Object *arrayGet(int i, Object *obj);
  Object *arrayGetNF(int i, Object *obj);

  // Dict accessors.
  int dictGetLength();
  void dictAdd(char *key, Object *val);
  GBool dictIs(char *dictType);
  Object *dictLookup(char *key, Object *obj);
  Object *dictLookupNF(char *key, Object *obj);
  char *dictGetKey(int i);
  Object *dictGetVal(int i, Object *obj);
  Object *dictGetValNF(int i, Object *obj);

  // Stream accessors.
  GBool streamIs(char *dictType);
  void streamReset();
  void streamClose();
  int streamGetChar();
  int streamLookChar();
  char *streamGetLine(char *buf, int size);
  Guint streamGetPos();
  void streamSetPos(Guint pos, int dir = 0);
  Dict *streamGetDict();

  // Output.
  char *getTypeName();
  void print(FILE *f = stdout);

  // Memory testing.
  static void memCheck(FILE *f);

private:

  ObjType type;			// object type
  union {			// value for each type:
    GBool booln;		//   boolean
    int intg;			//   integer
    double real;		//   real
    GString *string;		//   string
    char *name;			//   name
    Array *array;		//   array
    Dict *dict;			//   dictionary
    Stream *stream;		//   stream
    Ref ref;			//   indirect reference
    char *cmd;			//   command
  };

#ifdef DEBUG_MEM
  static int			// number of each type of object
    numAlloc[numObjTypes];	//   currently allocated
#endif
};

//------------------------------------------------------------------------
// Array accessors.
//------------------------------------------------------------------------

#include "Array.h"

inline int Object::arrayGetLength()
  { if (type != objArray) return 0; return array->getLength(); }

inline void Object::arrayAdd(Object *elem)
  { if (type == objArray) array->add(elem); }

inline Object *Object::arrayGet(int i, Object *obj)
  { if (type != objArray) return obj->initNull(); return array->get(i, obj); }

inline Object *Object::arrayGetNF(int i, Object *obj)
  { if (type != objArray) return obj->initNull(); return array->getNF(i, obj); }

//------------------------------------------------------------------------
// Dict accessors.
//------------------------------------------------------------------------

#include "Dict.h"

inline int Object::dictGetLength()
  { if (type != objDict) return 0; return dict->getLength(); }

inline void Object::dictAdd(char *key, Object *val)
  { if (type == objDict) dict->add(key, val); }

inline GBool Object::dictIs(char *dictType)
  { return (type == objDict) && dict->is(dictType); }

inline GBool Object::isDict(char *dictType)
  { return (type == objDict) && dictIs(dictType); }

inline Object *Object::dictLookup(char *key, Object *obj)
  { if (type != objDict) return obj->initNull(); return dict->lookup(key, obj); }

inline Object *Object::dictLookupNF(char *key, Object *obj)
  { if (type != objDict) return obj->initNull(); return dict->lookupNF(key, obj); }

inline char *Object::dictGetKey(int i)
  { if (type != objDict) return NULL; return dict->getKey(i); }

inline Object *Object::dictGetVal(int i, Object *obj)
  { if (type != objDict) return obj->initNull(); return dict->getVal(i, obj); }

inline Object *Object::dictGetValNF(int i, Object *obj)
  { if (type != objDict) return obj->initNull(); return dict->getValNF(i, obj); }

//------------------------------------------------------------------------
// Stream accessors.
//------------------------------------------------------------------------

#include "Stream.h"

inline GBool Object::streamIs(char *dictType)
  { return (type == objStream) && stream->getDict()->is(dictType); }

inline GBool Object::isStream(char *dictType)
  { return (type == objStream) && streamIs(dictType); }

inline void Object::streamReset()
  { if (type == objStream) stream->reset(); }

inline void Object::streamClose()
  { if (type == objStream) stream->close(); }

inline int Object::streamGetChar()
  { if (type != objStream) return EOF; return stream->getChar(); }

inline int Object::streamLookChar()
  { if (type != objStream) return EOF; return stream->lookChar(); }

inline char *Object::streamGetLine(char *buf, int size)
  { if (type != objStream) return NULL; return stream->getLine(buf, size); }

inline Guint Object::streamGetPos()
  { if (type != objStream) return 0; return stream->getPos(); }

inline void Object::streamSetPos(Guint pos, int dir)
  { if (type == objStream) stream->setPos(pos, dir); }

inline Dict *Object::streamGetDict()
  { if (type != objStream) return NULL; return stream->getDict(); }

#endif
