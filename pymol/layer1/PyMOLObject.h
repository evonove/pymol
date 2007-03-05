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
#ifndef _H_PyMOLObject
#define _H_PyMOLObject

/* literally a 3-D object...also an object object */

#include"Base.h"
#include"Ray.h"
#include"Rep.h"
#include"Setting.h"
#include"PyMOLGlobals.h"
#include"View.h"
#include"Word.h"

typedef char ObjectNameType[WordLength];

#define cObjectMolecule     1
#define cObjectMap          2
#define cObjectMesh         3
#define cObjectDist         4
#define cObjectCallback     5
#define cObjectCGO          6
#define cObjectSurface      7
#define cObjectGadget       8
#define cObjectCalculator   9
#define cObjectSlice        10
#define cObjectAlignment    11
#define cObjectGroup        12

/* 
   the object base class is in the process of being converted to support
   states explicitly (an unfortunate early omission), which will allow
   for simplified implementation of future multi-state objects.
 */


typedef struct CObjectState {
  PyMOLGlobals *G;
  double *Matrix;
} CObjectState;

typedef struct CObject {
  PyMOLGlobals *G;
  void (*fUpdate)(struct CObject *I); /* update representations */
  void (*fRender)(struct CObject *I,RenderInfo *info);
  void (*fFree)(struct CObject *I);
  int  (*fGetNFrame)(struct CObject *I);
  void (*fDescribeElement)(struct CObject *I,int index,char *buffer);
  void (*fInvalidate)(struct CObject *I,int rep,int level,int state);
  CSetting **(*fGetSettingHandle)(struct CObject *I,int state);
  char *(*fGetCaption)(struct CObject *I);
  CObjectState *(*fGetObjectState)(struct CObject *I,int state);
  int type;
  ObjectNameType Name;
  int Color;
  int RepVis[cRepCnt]; 
  float ExtentMin[3],ExtentMax[3];
  int ExtentFlag,TTTFlag;
  float TTT[16]; /* translate, transform, translate matrix (to apply when rendering) */
  CSetting *Setting;
  int Enabled; /* read-only... maintained by Scene */
  int Context; /* 0 = Camera, 1 = Unit Window, 2 = Scaled Window */
  CViewElem *ViewElem; /* for animating objects via the TTT */
} CObject;

void ObjectInit(PyMOLGlobals *G,CObject *I);
int ObjectCopyHeader(CObject *I, CObject *src);
void ObjectPurge(CObject *I);
void ObjectSetName(CObject *I,char *name);
void ObjectMakeValidName(char *name);
void ObjectPurgeSettings(CObject *I);
void ObjectFree(CObject *I);
void ObjectUseColor(CObject *I);
void ObjectSetRepVis(CObject *I,int rep,int state);
void ObjectToggleRepVis(CObject *I,int rep);
void ObjectPrepareContext(CObject *I,CRay *ray);
void ObjectSetTTT(CObject *I,float *ttt,int state);
int ObjectGetTTT(CObject *I,float **ttt,int state);
int ObjectGetTotalMatrix(CObject *I, int state, int history, double *matrix);
void ObjectCombineTTT(CObject *I,float *ttt,int reverse_order);
void ObjectTranslateTTT(CObject *T, float *v);
void ObjectSetTTTOrigin(CObject *I,float *origin);
void ObjectResetTTT(CObject *I);
PyObject *ObjectAsPyList(CObject *I);
int ObjectFromPyList(PyMOLGlobals *G,PyObject *list,CObject *I);
int ObjectGetCurrentState(CObject *I,int ignore_all_states);
void ObjectAdjustStateRebuildRange(CObject *I,int *start, int *stop);
int ObjectView(CObject *I,int action,int first,
               int last,float power,float bias,
               int simple, float linear,int wrap,
               int hand,int window,int cycles,int quiet);


void ObjectStateInit(PyMOLGlobals *G,CObjectState *I);
void ObjectStateCopy(CObjectState *dst, CObjectState *src);
void ObjectStatePurge(CObjectState *I);
void ObjectStateSetMatrix(CObjectState *I, double *matrix);
double *ObjectStateGetMatrix(CObjectState *I);
void ObjectStateTransformMatrix(CObjectState *I, double *matrix);
void ObjectStateResetMatrix(CObjectState *I);
PyObject *ObjectStateAsPyList(CObjectState *I);
int ObjectStateFromPyList(PyMOLGlobals *G,PyObject *list,CObjectState *I);
int ObjectStatePushAndApplyMatrix(CObjectState *I,RenderInfo *info);
void ObjectStatePopMatrix(CObjectState *I,RenderInfo *info);
void ObjectStateRightCombineMatrixR44d(CObjectState *I, double *matrix);
void ObjectStateLeftCombineMatrixR44d(CObjectState *I, double *matrix);
void ObjectStateCombineMatrixTTT(CObjectState *I, float *matrix);

typedef struct _CObjectUpdateThreadInfo CObjectUpdateThreadInfo;

#endif



