
/* 
A* -------------------------------------------------------------------
B* This file contains source code for the PyMOL computer program
C* copyright 1998-2000 by Warren Lyford Delano of DeLano Scientific. 
D* -------------------------------------------------------------------
E* It is unlawful to modify or remove this copyright notice.
F* -------------------------------------------------------------------
G* Please see the accompanying LICENSE file for further information. 
H* -------------------------------------------------------------------
I* Additional authors of this source file include:
-* 
-* 
-*
Z* -------------------------------------------------------------------
*/
#ifndef _H_P
#define _H_P

#include"os_python.h"
#include"AtomInfo.h"
#include"ObjectMolecule.h"
#include"CoordSet.h"
#include"PyMOLGlobals.h"

#define cLockAPI 1
#define cLockInbox 2
#define cLockOutbox 3

#define cPLog_pml_lf    0
#define cPLog_pml       1
#define cPLog_pym       2
#define cPLog_no_flush  3

#define cPType_string          1
#define cPType_int             2
#define cPType_int_as_string   3
#define cPType_float           4
#define cPType_stereo          5
#define cPType_char_as_type    6
#define cPType_model           7
#define cPType_index           8
#define cPType_int_custom_type 9
#define cPType_xyz_float      10
#define cPType_settings       11
#define cPType_properties     12
#define cPType_state          13

#define NUM_ATOM_PROPERTIES    38

#define cPRunType_alter          1
#define cPRunType_alter_state    2
#define cPRunType_label          3


int PLabelExprUsesVariable(PyMOLGlobals * G, char *expr, char *var);

int PLabelAtomAlt(PyMOLGlobals * G, AtomInfoType * at, char *model, char *expr,
                  int index);

#ifdef _PYMOL_NOPY

#define PRunStringInstance(G,x)
#define PRunStringModule(G,x)

#define PAutoBlock(G) 1
#define PAutoUnblock(G,a)

#define PBlock(G)
#define PUnblock(G)

#define PBlockLegacy()
#define PUnblockLegacy()

#define PLockAPIAsGlut(G,block_if_busy)
#define PUnlockAPIAsGlut(G)
#define PUnlockAPIAsGlutNoFlush(G)

#define PLockAPI(G)
#define PUnlockAPI(G)

#define PLockStatus(G)
#define PLockStatusAttempt()G 1
#define PUnlockStatusG()

#define PBlockAndUnlockAPI(G)
#define PBlockAndUnlockAPI(G)
#define PLockAPIAndUnblock(G)
#define PTryLockAPIAndUnblock(G)
#define PFlush(G)
#define PFlushFast(G)
#define PParse(G,s)
#define PDo(G,s)

#define PLog(G,a,b)
#define PLogFlush(G)

#define PIsGlutThread() 1
#define PComplete(G,a,b) 0

#define PSGIStereo(G,a)
#define PPovrayRender(a,b,c,d,e,f,g) 0

#define PTruthCallStr(a,b,c)

#define PSleep(G,a)
#define PSleepWhileBusy(G,a)
#define PSleepUnlocked(G,a)

#define PFree()
#define PInit(G,a)
#define PSetupEmbedded(G,a,b)
#define PConvertOptions(a,b)
#define PGetOptions(a)

#define PAlterAtom(G,a,b,c,d,e,f,g,h) 0
#define PLabelAtom(G,a,b,c,d,e,f) 0

#define PAlterAtomState(G,a,b,c,d,e,f,g,h,i,j,k) 0

#else

ov_status PCacheSet(PyMOLGlobals * G, PyObject * entry, PyObject * output);
ov_status PCacheGet(PyMOLGlobals * G,
                    PyObject ** result_output, PyObject ** result_entry,
                    PyObject * input);

void PInit(PyMOLGlobals * G, int global_instance);
void PSetupEmbedded(PyMOLGlobals * G, int argc, char **argv);

struct PyMOLOptionRec;

void PConvertOptions(CPyMOLOptions * rec, PyObject * options);
void PGetOptions(CPyMOLOptions * rec);

void PFree(void);
void PExit(PyMOLGlobals * G, int code);
void PParse(PyMOLGlobals * G, const char *str);       /* only accepts one command */
void PDo(PyMOLGlobals * G, const char *str);  /* accepts multple commands seperated by newlines */

int PAlterAtom(PyMOLGlobals * G, ObjectMolecule *obj, CoordSet *cs, AtomInfoType * at, PyCodeObject *expr_co,
               int read_only, char *model, int index, PyObject * space);
int PLabelAtom(PyMOLGlobals * G, ObjectMolecule *obj, CoordSet *cs, AtomInfoType * at, PyCodeObject *expr_co, char *model, int index);
int PAlterAtomState(PyMOLGlobals * G, float *v, PyCodeObject *expr_co, int read_only,
                    ObjectMolecule *obj, CoordSet *cs, AtomInfoType * at, char *model, int index, int csindex, int state, PyObject * space);

void PLog(PyMOLGlobals * G, const char *str, int lf);
void PLogFlush(PyMOLGlobals * G);

void PSleep(PyMOLGlobals * G, int usec);
void PSleepWhileBusy(PyMOLGlobals * G, int usec);
void PSleepUnlocked(PyMOLGlobals * G, int usec);

int PLockAPI(PyMOLGlobals * G, int block_if_busy);
void PUnlockAPI(PyMOLGlobals * G);

int PLockAPIAsGlut(PyMOLGlobals * G, int block_if_busy);
void PUnlockAPIAsGlut(PyMOLGlobals * G);
void PUnlockAPIAsGlutNoFlush(PyMOLGlobals * G);

void PLockStatus(PyMOLGlobals * G);
int PLockStatusAttempt(PyMOLGlobals * G);
void PUnlockStatus(PyMOLGlobals * G);

void PBlock(PyMOLGlobals * G);
void PUnblock(PyMOLGlobals * G);

void PBlockLegacy(void);
void PUnblockLegacy(void);

int PAutoBlock(PyMOLGlobals * G);
void PAutoUnblock(PyMOLGlobals * G, int flag);

void PBlockAndUnlockAPI(PyMOLGlobals * G);
void PLockAPIAndUnblock(PyMOLGlobals * G);
int PTryLockAPIAndUnblock(PyMOLGlobals * G);

int PFlush(PyMOLGlobals * G);
int PFlushFast(PyMOLGlobals * G);
void PXDecRef(PyObject * obj);
PyObject *PXIncRef(PyObject * obj);
void PSGIStereo(PyMOLGlobals * G, int flag);
void PDefineFloat(PyMOLGlobals * G, const char *name, float value);

void PRunStringModule(PyMOLGlobals * G, const char *str);
void PRunStringInstance(PyMOLGlobals * G, const char *str);

void PDumpTraceback(PyObject * err);
void PDumpException(void);

int PComplete(PyMOLGlobals * G, char *str, int buf_size);

int PTruthCallStr(PyObject * object, char *method, char *argument);
int PTruthCallStr0(PyObject * object, char *method);
int PTruthCallStr1i(PyObject * object, char *method, int argument);
int PTruthCallStr1s(PyObject * object, char *method, char *argument);
int PTruthCallStr4i(PyObject * object, char *method, int a1, int a2, int a3, int a4);
int PPovrayRender(PyMOLGlobals * G, char *header, char *inp, char *file, int width,
                  int height, int antialias);
int PIsGlutThread(void);

PyObject *PGetFontDict(PyMOLGlobals * G, float size, int face, int style);

typedef struct {
  int id;
  PyThreadState *state;
} SavedThreadRec;

typedef struct {
  PyObject_HEAD
  //  PyObject* dict;
  ObjectMolecule *obj;
  CoordSet *cs;
  AtomInfoType *atomInfo;
  char *model;
  int index;
  int csindex;
  float *v; // for PAlterAtomState x/y/z
  int state;
  short read_only; // set for PLabelAtom
  PyMOLGlobals * G;
  PyObject *dict;
} WrapperObject;

void WrapperObjectReset(WrapperObject *);

/* instance-specific Python object, containers, closures, and threads */

#define MAX_SAVED_THREAD ((PYMOL_MAX_THREADS)+3)

struct _CP_inst {
  /* instance-specific storage */

  PyObject *obj;
  PyObject *dict;
  PyObject *exec;
  PyObject *cmd;
  PyObject *parse;              /* parse closure */
  PyObject *complete;           /* complete partial command / TAB action */
  PyObject *cmd_do;

  PyObject *cache;

  /* locks and threads */

  PyObject *lock;               /* API locks */
  PyObject *lock_attempt;
  PyObject *unlock;

  PyObject *lock_c;             /* C locks */
  PyObject *unlock_c;

  PyObject *lock_status;        /* status locks */
  PyObject *lock_status_attempt;        /* status locks */
  PyObject *unlock_status;

  PyObject *lock_glut;          /* GLUT locks */
  PyObject *unlock_glut;

  int glut_thread_keep_out;
  SavedThreadRec savedThread[MAX_SAVED_THREAD];

  WrapperObject *wrapperObject;
};


/* PyObject *GetBondsDict(void); */


/* all of the following Python objects must be invariant global
   modules & module dictionaries for the application */

extern PyObject *P_menu;        /* used by Menu */
extern PyObject *P_xray;        /* used by Symmetry */
extern PyObject *P_chempy;      /* used by CoordSet and Selector for construction of models */
extern PyObject *P_models;      /* used by Selector for construction of models */
extern PyObject *P_setting;     /* used by Setting.c */
extern PyTypeObject *P_wrapper;     /* used by P.c for lazy-loading settings/properties/attributes */

extern unsigned int P_glut_thread_id;

#endif

#ifndef _PYMOL_NOPY
void PSetAtomPropertyInfo(PyMOLGlobals * G, int propid, short pt, int off);
#endif

char convertStereoToChar(int stereo);

#endif
