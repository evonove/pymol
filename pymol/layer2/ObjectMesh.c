
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
#include"os_python.h"

#include"os_predef.h"
#include"os_std.h"
#include"os_gl.h"

#include"OOMac.h"
#include"ObjectMesh.h"
#include"Base.h"
#include"MemoryDebug.h"
#include"Map.h"
#include"Debug.h"
#include"Parse.h"
#include"Isosurf.h"
#include"Vector.h"
#include"Color.h"
#include"main.h"
#include"Scene.h"
#include"Setting.h"
#include"Executive.h"
#include"PConv.h"
#include"P.h"
#include"Matrix.h"
#include"ShaderMgr.h"
#include"CGO.h"
#include"ObjectCGO.h"

ObjectMesh *ObjectMeshNew(PyMOLGlobals * G);

static void ObjectMeshFree(ObjectMesh * I);
static void ObjectMeshInvalidate(ObjectMesh * I, int rep, int level, int state);
void ObjectMeshStateInit(PyMOLGlobals * G, ObjectMeshState * ms);
void ObjectMeshRecomputeExtent(ObjectMesh * I);

#ifndef _PYMOL_NOPY
static PyObject *ObjectMeshStateAsPyList(ObjectMeshState * I)
{
  PyObject *result = NULL;

  result = PyList_New(17);

  PyList_SetItem(result, 0, PyInt_FromLong(I->Active));
  PyList_SetItem(result, 1, PyString_FromString(I->MapName));
  PyList_SetItem(result, 2, PyInt_FromLong(I->MapState));
  PyList_SetItem(result, 3, CrystalAsPyList(&I->Crystal));
  PyList_SetItem(result, 4, PyInt_FromLong(I->ExtentFlag));
  PyList_SetItem(result, 5, PConvFloatArrayToPyList(I->ExtentMin, 3));
  PyList_SetItem(result, 6, PConvFloatArrayToPyList(I->ExtentMax, 3));
  PyList_SetItem(result, 7, PConvIntArrayToPyList(I->Range, 6));
  PyList_SetItem(result, 8, PyFloat_FromDouble(I->Level));
  PyList_SetItem(result, 9, PyFloat_FromDouble(I->Radius));
  PyList_SetItem(result, 10, PyInt_FromLong(I->CarveFlag));
  PyList_SetItem(result, 11, PyFloat_FromDouble(I->CarveBuffer));
  if(I->CarveFlag && I->AtomVertex) {
    PyList_SetItem(result, 12, PConvFloatVLAToPyList(I->AtomVertex));
  } else {
    PyList_SetItem(result, 12, PConvAutoNone(NULL));
  }
  PyList_SetItem(result, 13, PyInt_FromLong(I->MeshMode));
  PyList_SetItem(result, 14, PyFloat_FromDouble(I->AltLevel));
  PyList_SetItem(result, 15, PyInt_FromLong(I->quiet));
  if(I->Field) {
    PyList_SetItem(result, 16, IsosurfAsPyList(I->Field));
  } else {
    PyList_SetItem(result, 16, PConvAutoNone(NULL));
  }
  return (PConvAutoNone(result));
}
#endif

#ifndef _PYMOL_NOPY
static int ObjectMeshStateMapExists(ObjectMesh *I, ObjectMeshState *ms){
  return ExecutiveFindObjectMapByName(I->Obj.G, ms->MapName) ? 1 : 0;
}

int ObjectMeshAllMapsInStatesExist(ObjectMesh * I)
{
  int a;
  for(a = 0; a < I->NState; a++) {
    if(I->State[a].Active) {
      if (!ObjectMeshStateMapExists(I, I->State + a)){
	return 0;
      }
    }
  }
  return 1;
}

static PyObject *ObjectMeshAllStatesAsPyList(ObjectMesh * I)
{

  PyObject *result = NULL;
  int a;
  result = PyList_New(I->NState);
  for(a = 0; a < I->NState; a++) {
    if(I->State[a].Active) {
      PyList_SetItem(result, a, ObjectMeshStateAsPyList(I->State + a));
    } else {
      PyList_SetItem(result, a, PConvAutoNone(NULL));
    }
  }
  return (PConvAutoNone(result));
}
#endif

#ifndef _PYMOL_NOPY
static int ObjectMeshStateFromPyList(PyMOLGlobals * G, ObjectMeshState * I,
                                     PyObject * list)
{
  int ok = true;
  int ll = 0;
  PyObject *tmp;
  if(ok)
    ok = (list != NULL);
  if(ok) {
    if(!PyList_Check(list))
      I->Active = false;
    else {
      ObjectMeshStateInit(G, I);
      if(ok)
        ok = (list != NULL);
      if(ok)
        ok = PyList_Check(list);
      if(ok)
        ll = PyList_Size(list);
      /* TO SUPPORT BACKWARDS COMPATIBILITY...
         Always check ll when adding new PyList_GetItem's */

      if(ok)
        ok = PConvPyIntToInt(PyList_GetItem(list, 0), &I->Active);
      if(ok)
        ok = PConvPyStrToStr(PyList_GetItem(list, 1), I->MapName, WordLength);
      if(ok)
        ok = PConvPyIntToInt(PyList_GetItem(list, 2), &I->MapState);
      if(ok)
        ok = CrystalFromPyList(&I->Crystal, PyList_GetItem(list, 3));
      if(ok)
        ok = PConvPyIntToInt(PyList_GetItem(list, 4), &I->ExtentFlag);
      if(ok)
        ok = PConvPyListToFloatArrayInPlace(PyList_GetItem(list, 5), I->ExtentMin, 3);
      if(ok)
        ok = PConvPyListToFloatArrayInPlace(PyList_GetItem(list, 6), I->ExtentMax, 3);
      if(ok)
        ok = PConvPyListToIntArrayInPlace(PyList_GetItem(list, 7), I->Range, 6);
      if(ok)
        ok = PConvPyFloatToFloat(PyList_GetItem(list, 8), &I->Level);
      if(ok)
        ok = PConvPyFloatToFloat(PyList_GetItem(list, 9), &I->Radius);
      if(ok)
        ok = PConvPyIntToInt(PyList_GetItem(list, 10), &I->CarveFlag);
      if(ok)
        ok = PConvPyFloatToFloat(PyList_GetItem(list, 11), &I->CarveBuffer);
      if(ok) {
        tmp = PyList_GetItem(list, 12);
        if(tmp == Py_None)
          I->AtomVertex = NULL;
        else
          ok = PConvPyListToFloatVLA(tmp, &I->AtomVertex);
      }
      if(ok)
        ok = PConvPyIntToInt(PyList_GetItem(list, 13), &I->MeshMode);
      if(ok) {
        I->RefreshFlag = true;
        I->ResurfaceFlag = true;
      }
      if(ok && (ll > 14)) {
        ok = PConvPyFloatToFloat(PyList_GetItem(list, 14), &I->AltLevel);
      } else {
        I->AltLevel = I->Level;
      }
      if(ok && (ll > 15)) {
        ok = PConvPyIntToInt(PyList_GetItem(list, 15), &I->quiet);
      } else {
        I->quiet = true;
      }
      if(ok && (ll > 16)) {
        tmp = PyList_GetItem(list, 16);
        if(tmp == Py_None)
          I->Field = NULL;
        else
          ok = ((I->Field = IsosurfNewFromPyList(G, tmp)) != NULL);
      }
    }
  }
  return (ok);
}
#endif

#ifndef _PYMOL_NOPY
static int ObjectMeshAllStatesFromPyList(ObjectMesh * I, PyObject * list)
{

  int ok = true;
  int a;
  VLACheck(I->State, ObjectMeshState, I->NState);
  if(ok)
    ok = PyList_Check(list);
  if(ok) {
    for(a = 0; a < I->NState; a++) {
      ok = ObjectMeshStateFromPyList(I->Obj.G, I->State + a, PyList_GetItem(list, a));
      if(!ok)
        break;
    }
  }
  return (ok);
}
#endif

int ObjectMeshNewFromPyList(PyMOLGlobals * G, PyObject * list, ObjectMesh ** result)
{
#ifdef _PYMOL_NOPY
  return 0;
#else
  int ok = true;
  int ll;
  ObjectMesh *I = NULL;
  (*result) = NULL;

  if(ok)
    ok = (list != NULL);
  if(ok)
    ok = PyList_Check(list);
  if(ok)
    ll = PyList_Size(list);
  /* TO SUPPORT BACKWARDS COMPATIBILITY...
     Always check ll when adding new PyList_GetItem's */

  I = ObjectMeshNew(G);
  if(ok)
    ok = (I != NULL);

  if(ok)
    ok = ObjectFromPyList(G, PyList_GetItem(list, 0), &I->Obj);
  if(ok)
    ok = PConvPyIntToInt(PyList_GetItem(list, 1), &I->NState);
  if(ok)
    ok = ObjectMeshAllStatesFromPyList(I, PyList_GetItem(list, 2));
  if(ok) {
    (*result) = I;
    ObjectMeshRecomputeExtent(I);
  } else {
    ObjectMeshFree(I);
    (*result) = NULL;
  }
  return (ok);
#endif
}

static CGO *ObjectMeshRenderImpl(ObjectMesh * I, RenderInfo * info, int returnCGO, int stateArg);

PyObject *ObjectMeshAsPyList(ObjectMesh * I)
{
#ifdef _PYMOL_NOPY
  return NULL;
#else

  PyObject *result = NULL;

  int allMapsExist = ObjectMeshAllMapsInStatesExist(I);

  if (allMapsExist){
    result = PyList_New(3);
    PyList_SetItem(result, 0, ObjectAsPyList(&I->Obj));
    PyList_SetItem(result, 1, PyInt_FromLong(I->NState));
    PyList_SetItem(result, 2, ObjectMeshAllStatesAsPyList(I));
  } else {
    /* save ObjectMesh as ObjectCGO */
    ObjectCGO *retObjectCGO = NULL;
    int a;
    PRINTFB(I->Obj.G, FB_ObjectMesh, FB_Errors)
      "ObjectMesh-Warning: map has been deleted, saving as CGO.\n"
      ENDFB(I->Obj.G);
    for(a = 0; a < I->NState; a++) {
      CGO *cgo = ObjectMeshRenderImpl(I, 0, 1, a);
      retObjectCGO = ObjectCGOFromCGO(I->Obj.G, retObjectCGO, cgo, a);
      
    }
    ObjectCopyHeader(&retObjectCGO->Obj, &I->Obj);
    ObjectSetRepVis(&retObjectCGO->Obj, cRepMesh, 0);
    ObjectSetRepVis(&retObjectCGO->Obj, cRepCGO, 1);
    retObjectCGO->Obj.type = cObjectCGO;
    result = ObjectCGOAsPyList(retObjectCGO);
    ObjectCGOFree(retObjectCGO);
  }
  return (PConvAutoNone(result));
#endif
}

static void ObjectMeshStateFree(ObjectMeshState * ms)
{
  ObjectStatePurge(&ms->State);
  if(ms->State.G->HaveGUI) {
#ifdef _PYMOL_GL_CALLLISTS
    if(ms->displayList) {
      if(PIsGlutThread()) {
        if(ms->State.G->ValidContext) {
          glDeleteLists(ms->displayList, 1);
          ms->displayList = 0;
        }
      } else {
        char buffer[255];       /* pass this off to the main thread */
        sprintf(buffer, "_cmd.gl_delete_lists(cmd._COb,%d,%d)\n", ms->displayList, 1);
        PParse(ms->State.G, buffer);
        ms->displayList = 0;
      }
    }
#endif
  }
  if(ms->Field) {
    IsosurfFieldFree(ms->State.G, ms->Field);
    ms->Field = NULL;
  }
  VLAFreeP(ms->N);
  VLAFreeP(ms->V);
  FreeP(ms->VC);
  FreeP(ms->RC);
  VLAFreeP(ms->AtomVertex);
  if (ms->shaderCGO){
    CGOFree(ms->shaderCGO);
    ms->shaderCGO = NULL;
    ms->shaderUnitCellCGO = NULL;
  }
  if(ms->UnitCellCGO) {
    CGOFree(ms->UnitCellCGO);
    ms->UnitCellCGO = NULL;
  }
  ms->Active = false;
}

static void ObjectMeshFree(ObjectMesh * I)
{
  int a;
  for(a = 0; a < I->NState; a++) {
    if(I->State[a].Active)
      ObjectMeshStateFree(I->State + a);
  }
  VLAFreeP(I->State);
  ObjectPurge(&I->Obj);

  OOFreeP(I);
}

int ObjectMeshInvalidateMapName(ObjectMesh * I, char *name)
{
  int a;
  ObjectMeshState *ms;
  int result = false;
  for(a = 0; a < I->NState; a++) {
    ms = I->State + a;
    if(ms->Active) {
      if(strcmp(ms->MapName, name) == 0) {
        ObjectMeshInvalidate(I, cRepAll, cRepInvAll, a);
        result = true;
      }
    }
  }
  return result;
}

void ObjectMeshDump(ObjectMesh * I, char *fname, int state)
{
  float *v;
  int *n;
  int c;
  FILE *f;
  f = fopen(fname, "wb");
  if(!f)
    ErrMessage(I->Obj.G, "ObjectMeshDump", "can't open file for writing");
  else {
    if(state < I->NState) {
      n = I->State[state].N;
      v = I->State[state].V;
      if(n && v)
        while(*n) {
          c = *(n++);
          if(!I->State[state].MeshMode) {
            fprintf(f, "\n");
          }
          while(c--) {
            fprintf(f, "%10.4f%10.4f%10.4f\n", v[0], v[1], v[2]);
            v += 3;
          }
        }
    }
    fclose(f);
    PRINTFB(I->Obj.G, FB_ObjectMesh, FB_Actions)
      " ObjectMeshDump: %s written to %s\n", I->Obj.Name, fname ENDFB(I->Obj.G);
  }
}

static void ObjectMeshInvalidate(ObjectMesh * I, int rep, int level, int state)
{
  int a;
  int once_flag = true;
  if(level >= cRepInvExtents) {
    I->Obj.ExtentFlag = false;
  }
  if((rep == cRepMesh) || (rep == cRepAll) || (rep == cRepCell)) {
    if (state < 0){
      for(a = 0; a < I->NState; a++) {
	ObjectMeshState *ms = &I->State[a];
	if (ms && ms->shaderCGO){
	  CGOFree(ms->shaderCGO);
	  ms->shaderCGO = NULL;
	  ms->shaderUnitCellCGO = NULL;
	}
      }
    } else {
      ObjectMeshState *ms = &I->State[state];
      if (ms && ms->shaderCGO){
	CGOFree(ms->shaderCGO);
	ms->shaderCGO = NULL;
	ms->shaderUnitCellCGO = NULL;
      }
    }
    for(a = 0; a < I->NState; a++) {
      if(state < 0)
        once_flag = false;
      if(!once_flag)
        state = a;
      I->State[state].RefreshFlag = true;
      if(level >= cRepInvAll) {
        I->State[state].ResurfaceFlag = true;
        SceneChanged(I->Obj.G);
      } else if(level >= cRepInvColor) {
        I->State[state].RecolorFlag = true;
        SceneChanged(I->Obj.G);
      } else {
        SceneInvalidate(I->Obj.G);
      }
      if(once_flag)
        break;
    }
  }
}

int ObjectMeshGetLevel(ObjectMesh * I, int state, float *result)
{
  int ok = true;
  ObjectMeshState *ms;
  if(state >= I->NState) {
    ok = false;
  } else {
    if(state < 0) {
      state = 0;
    }
    ms = I->State + state;
    if(ms->Active && result) {
      *result = ms->Level;
    } else
      ok = false;
  }
  return (ok);
}

int ObjectMeshSetLevel(ObjectMesh * I, float level, int state, int quiet)
{
  int a;
  int ok = true;
  int once_flag = true;
  ObjectMeshState *ms;
  if(state >= I->NState) {
    ok = false;
  } else {
    for(a = 0; a < I->NState; a++) {
      if(state < 0) {
        once_flag = false;
      }
      if(!once_flag) {
        state = a;
      }
      ms = I->State + state;
      if(ms->Active) {
        ms->ResurfaceFlag = true;
        ms->RefreshFlag = true;
        ms->Level = level;
        ms->quiet = quiet;
      }
      if(once_flag) {
        break;
      }
    }
  }
  return (ok);
}

static void ObjectMeshStateUpdateColors(ObjectMesh * I, ObjectMeshState * ms)
{
  int one_color_flag = true;
  int cur_color = -1;

  if(ms->MeshMode == 0) {
    cur_color = SettingGet_color(I->Obj.G, I->Obj.Setting, NULL, cSetting_mesh_color);
  } else if(ms->MeshMode == 1) {
    cur_color = SettingGet_color(I->Obj.G, I->Obj.Setting, NULL, cSetting_dot_color);
  }

  if(cur_color == -1)
    cur_color = I->Obj.Color;

  if(ColorCheckRamped(I->Obj.G, cur_color))
    one_color_flag = false;

  ms->OneColor = cur_color;
  if(ms->V) {
    int ramped_flag = false;
    float *v = ms->V;
    float *vc;
    int *rc;
    int a;
    int state = ms - I->State;
    int n_vert = VLAGetSize(ms->V) / 3;
    int base_n_vert = ms->base_n_V / 3;

    if(ms->VC && (ms->VCsize < n_vert)) {
      FreeP(ms->VC);
      FreeP(ms->RC);
    }

    if(!ms->VC) {
      ms->VCsize = n_vert;
      ms->VC = Alloc(float, n_vert * 3);
    }
    if(!ms->RC) {
      ms->RC = Alloc(int, n_vert);
    }
    rc = ms->RC;
    vc = ms->VC;
    if(vc) {
      for(a = 0; a < n_vert; a++) {
        if(a == base_n_vert) {
          int new_color = SettingGet_color(I->Obj.G, I->Obj.Setting,
                                           NULL, cSetting_mesh_negative_color);
          if(new_color == -1)
            new_color = cur_color;
          if(new_color != cur_color) {
            one_color_flag = false;
            cur_color = new_color;
          }
        }
        if(ColorCheckRamped(I->Obj.G, cur_color)) {
          ColorGetRamped(I->Obj.G, cur_color, v, vc, state);
          *rc = cur_color;
          ramped_flag = true;
        } else {
          float *col = ColorGet(I->Obj.G, cur_color);
          copy3f(col, vc);
        }
        rc++;
        vc += 3;
        v += 3;
      }
    }

    if(one_color_flag && (!ramped_flag)) {
      FreeP(ms->VC);
      FreeP(ms->RC);
    } else if((!ramped_flag)
              ||
              (!SettingGet_b(I->Obj.G, NULL, I->Obj.Setting, cSetting_ray_color_ramps))) {
      FreeP(ms->RC);
    }
  }
}

static void ObjectMeshUpdate(ObjectMesh * I)
{
  PyMOLGlobals *G = I->Obj.G;
  int a;
  int c;
  ObjectMeshState *ms;
  ObjectMapState *oms = NULL;
  ObjectMap *map = NULL;

  int *n;
  float *v;
  float carve_buffer;
  int avoid_flag = false;
  int *old_n;
  float *old_v;
  int n_cur;
  int n_seg;
  int n_line;
  int flag;
  int last_flag = 0;
  int h, k, l;
  int i, j;
  int ok = true;
  int mesh_skip = SettingGet_i(G, I->Obj.Setting, NULL, cSetting_mesh_skip);

  MapType *voxelmap;            /* this has nothing to do with isosurfaces... */
  for(a = 0; a < I->NState; a++) {
    ms = I->State + a;
    if(ms->Active) {

      map = ExecutiveFindObjectMapByName(I->Obj.G, ms->MapName);
      if(!map) {
        ok = false;
        PRINTFB(I->Obj.G, FB_ObjectMesh, FB_Errors)
          "ObjectMeshUpdate-Error: map '%s' has been deleted.\n", ms->MapName
          ENDFB(I->Obj.G);
        ms->ResurfaceFlag = false;
      }
      if(map) {
        oms = ObjectMapGetState(map, ms->MapState);
        if(!oms)
          ok = false;
      }
      if(oms) {
        if(ms->RefreshFlag || ms->ResurfaceFlag) {
          if(!ms->Field) {
            ms->Crystal = *(oms->Symmetry->Crystal);
          }

          if(I->Obj.RepVis[cRepCell]) {
            if(ms->UnitCellCGO)
              CGOFree(ms->UnitCellCGO);
            ms->UnitCellCGO = CrystalGetUnitCellCGO(&ms->Crystal);
          }

          if(oms->State.Matrix) {
            ObjectStateSetMatrix(&ms->State, oms->State.Matrix);
          } else if(ms->State.Matrix) {
            ObjectStateResetMatrix(&ms->State);
          }
          ms->RefreshFlag = false;
          ms->displayListInvalid = true;
        }
      }

      if(map && oms && ms->N && ms->V && I->Obj.RepVis[cRepMesh]) {
        if(ms->ResurfaceFlag) {
          Isofield *field = NULL;
          ms->RecolorFlag = true;
          ms->ResurfaceFlag = false;
          if(!ms->quiet) {
            PRINTFB(G, FB_ObjectMesh, FB_Details)
              " ObjectMesh: updating \"%s\".\n", I->Obj.Name ENDFB(G);
          }
          if(ms->Field) {
            field = ms->Field;
          } else if(oms->Field) {
            field = oms->Field;
          }

          if(field) {
            {
              float *min_ext, *max_ext;
              float tmp_min[3], tmp_max[3];
              if(MatrixInvTransformExtentsR44d3f(ms->State.Matrix,
                                                 ms->ExtentMin, ms->ExtentMax,
                                                 tmp_min, tmp_max)) {
                min_ext = tmp_min;
                max_ext = tmp_max;
              } else {
                min_ext = ms->ExtentMin;
                max_ext = ms->ExtentMax;
              }

              IsosurfGetRange(I->Obj.G, field, oms->Symmetry->Crystal,
                              min_ext, max_ext, ms->Range, true);
            }
            /*                      printf("Mesh-DEBUG: %d %d %d %d %d %d\n",
               ms->Range[0],
               ms->Range[1],
               ms->Range[2],
               ms->Range[3],
               ms->Range[4],
               ms->Range[5]); */
            IsosurfVolume(I->Obj.G, I->Obj.Setting, NULL,
                          field,
                          ms->Level,
                          &ms->N, &ms->V,
                          ms->Range, ms->MeshMode, mesh_skip, ms->AltLevel);

            if(!SettingGet_b
               (I->Obj.G, I->Obj.Setting, NULL, cSetting_mesh_negative_visible)) {
              ms->base_n_V = VLAGetSize(ms->V);
            } else if(ms->MeshMode != 3) {
              /* do we want the negative surface too? */

              int *N2 = VLAlloc(int, 10000);
              float *V2 = VLAlloc(float, 10000);

              IsosurfVolume(I->Obj.G, I->Obj.Setting, NULL,
                            field,
                            -ms->Level,
                            &N2, &V2, ms->Range, ms->MeshMode, mesh_skip, ms->AltLevel);

              if(N2 && V2) {

                int base_n_N = VLAGetSize(ms->N);
                int base_n_V = VLAGetSize(ms->V);
                int addl_n_N = VLAGetSize(N2);
                int addl_n_V = VLAGetSize(V2);

                ms->base_n_V = base_n_V;

                /* make room */

                VLASize(ms->N, int, base_n_N + addl_n_N);
                VLASize(ms->V, float, base_n_V + addl_n_V);

                /* copy vertex data */

                memcpy(((char *) ms->V) + (sizeof(float) * base_n_V),
                       V2, sizeof(float) * addl_n_V);

                /* copy strip counts */

                memcpy(((char *) ms->N) + (sizeof(int) * (base_n_N - 1)),
                       N2, sizeof(int) * addl_n_N);
                ms->N[base_n_N + addl_n_N - 1] = 0;

                VLAFreeP(N2);
                VLAFreeP(V2);
              }

            }

            if(ms->State.Matrix && VLAGetSize(ms->N) && VLAGetSize(ms->V)) {
              int count;
              /* take map coordinates back to view coordinates if necessary */
              v = ms->V;
              count = VLAGetSize(ms->V) / 3;
              while(count--) {
                transform44d3f(ms->State.Matrix, v, v);
                v += 3;
              }
            }

          }
          if(ms->CarveFlag && ms->AtomVertex && VLAGetSize(ms->N) && VLAGetSize(ms->V)) {
            carve_buffer = ms->CarveBuffer;
            if(ms->CarveBuffer < 0.0F) {
              avoid_flag = true;
              carve_buffer = -carve_buffer;
            }

            /* cull my friend, cull */
            voxelmap = MapNew(I->Obj.G,
                              -carve_buffer, ms->AtomVertex,
                              VLAGetSize(ms->AtomVertex) / 3, NULL);
            if(voxelmap) {

              MapSetupExpress(voxelmap);

              old_n = ms->N;
              old_v = ms->V;
              ms->N = VLAlloc(int, VLAGetSize(old_n));
              ms->V = VLAlloc(float, VLAGetSize(old_v));

              n = old_n;
              v = old_v;
              n_cur = 0;
              n_seg = 0;
              n_line = 0;
              while(*n) {
                last_flag = false;
                c = *(n++);
                while(c--) {
                  flag = false;
                  MapLocus(voxelmap, v, &h, &k, &l);
                  i = *(MapEStart(voxelmap, h, k, l));
                  if(i) {
                    j = voxelmap->EList[i++];
                    while(j >= 0) {
                      if(within3f(ms->AtomVertex + 3 * j, v, carve_buffer)) {
                        flag = true;
                        break;
                      }
                      j = voxelmap->EList[i++];
                    }
                  }
                  if(avoid_flag)
                    flag = !flag;
                  if(flag && (!last_flag)) {
                    VLACheck(ms->V, float, 3 * (n_line + 1));
                    copy3f(v, ms->V + n_line * 3);
                    n_cur++;
                    n_line++;
                  }
                  if(flag && last_flag) {       /* continue segment */
                    VLACheck(ms->V, float, 3 * (n_line + 1));
                    copy3f(v, ms->V + n_line * 3);
                    n_cur++;
                    n_line++;
                  }
                  if((!flag) && last_flag) {    /* terminate segment */
                    VLACheck(ms->N, int, n_seg);
                    ms->N[n_seg] = n_cur;
                    n_seg++;
                    n_cur = 0;
                  }
                  last_flag = flag;
                  v += 3;
                }
                if(last_flag) { /* terminate segment */
                  VLACheck(ms->N, int, n_seg);
                  ms->N[n_seg] = n_cur;
                  n_seg++;
                  n_cur = 0;
                }
              }
              VLACheck(ms->N, int, n_seg);
              ms->N[n_seg] = 0;
              VLAFreeP(old_n);
              VLAFreeP(old_v);
              MapFree(voxelmap);
            }
          }
        }
        if(ms->RecolorFlag) {
          ObjectMeshStateUpdateColors(I, ms);
          ms->RecolorFlag = false;
        }
      }
      if (ms->shaderCGO){
	CGOFree(ms->shaderCGO);
	ms->shaderCGO = NULL;
	ms->shaderUnitCellCGO = NULL;
      }
    }
    SceneInvalidate(I->Obj.G);
  }
  if(!I->Obj.ExtentFlag) {
    ObjectMeshRecomputeExtent(I);
    if(I->Obj.ExtentFlag)
      SceneInvalidate(I->Obj.G);
  }
}


static void ObjectMeshRender(ObjectMesh * I, RenderInfo * info)
{
  ObjectMeshRenderImpl(I, info, 0, 0);
}

static short ObjectMeshStateRenderShader(ObjectMeshState *ms, ObjectMesh *I,
    RenderInfo *info, short mesh_as_cylinders, float mesh_width)
{
  PyMOLGlobals *G = I->Obj.G;
  CShaderPrg *shaderPrg;

  if (mesh_as_cylinders) {
    shaderPrg = CShaderPrg_Enable_CylinderShader(G);
    CShaderPrg_Set1f(shaderPrg, "uni_radius",
        SceneGetLineWidthForCylinders(G, info, mesh_width));
  } else {
    shaderPrg = CShaderPrg_Enable_DefaultShader(G);
    CShaderPrg_SetLightingEnabled(shaderPrg, 1);
    CShaderPrg_Set1i(shaderPrg, "two_sided_lighting_enabled",
		     SceneGetTwoSidedLighting(G));
  }

  if (!shaderPrg)
    return false;

  CGORenderGL(ms->shaderCGO, NULL, NULL, NULL, info, NULL);
  CShaderPrg_Disable(shaderPrg);

  if (ms->shaderUnitCellCGO){
    shaderPrg = CShaderPrg_Enable_DefaultShader(G);
    CShaderPrg_SetLightingEnabled(shaderPrg, 0);
    CGORenderGL(ms->shaderUnitCellCGO, NULL, NULL, NULL, info, NULL);
    CShaderPrg_Disable(shaderPrg);
  }

  return true;
}

static CGO *ObjectMeshRenderImpl(ObjectMesh * I, RenderInfo * info, int returnCGO, int stateArg)
{
  PyMOLGlobals *G = I->Obj.G;
  float *v = NULL;
  float *vc;
  int *rc;
  float radius;
  int state = 0;
  CRay *ray = 0;
  Picking **pick = 0;
  int pass = 0;
  int *n = NULL;
  int c;
  int a = 0;
  float line_width, mesh_width = SettingGet_f(I->Obj.G, I->Obj.Setting, NULL, cSetting_mesh_width);
  ObjectMeshState *ms = NULL;
  int ok = true;

  if (info){
    state = info->state;
    ray = info->ray;
    pick = info->pick;
    pass = info->pass;
  } else {
    state = stateArg;
  }

  line_width = SceneGetDynamicLineWidth(info, mesh_width);
  ObjectPrepareContext(&I->Obj, ray);

  for(a = 0; ok && a < I->NState; a++) {
    if(state < 0) {             /* all_states */
      ms = I->State + a;
    } else if(a > 0) {
      break;
    } else if(0 <= state && state < I->NState) {
      ms = I->State + state;
    } else if(I->NState == 1 && SettingGetGlobal_b(G, cSetting_static_singletons)) {
      ms = I->State;
    }

    if(!ms) {
      ok = false;
      break;
    }

    if(!ms->Active || !ms->V || !ms->N)
      continue;

    {
        v = ms->V;
        n = ms->N;
        if(ok && ray) {
          if(ms->UnitCellCGO && (I->Obj.RepVis[cRepCell])){
            ok &= CGORenderRay(ms->UnitCellCGO, ray, ColorGet(I->Obj.G, I->Obj.Color),
			       I->Obj.Setting, NULL);
	    if (!ok){
	      CGOFree(ms->UnitCellCGO);
	      ms->UnitCellCGO = NULL;
	      break;
	    }
	  }
          if(ms->MeshMode != 1) {
            radius = SettingGet_f(I->Obj.G, I->Obj.Setting, NULL, cSetting_mesh_radius);

            if(radius == 0.0F) {
              radius = ray->PixelRadius * line_width / 2.0F;
            }
          } else {
            radius = SettingGet_f(I->Obj.G, I->Obj.Setting, NULL, cSetting_dot_radius);
            if(radius == 0.0F) {
              radius =
                ray->PixelRadius * SettingGet_f(I->Obj.G, I->Obj.Setting, NULL,
                                                cSetting_dot_width) / 1.4142F;
            }
          }

          if(ok && n && v && I->Obj.RepVis[cRepMesh]) {
            float cc[3];
            float colA[3], colB[3];
            ColorGetEncoded(G, ms->OneColor, cc);
            vc = ms->VC;
            rc = ms->RC;
            if(ms->MeshMode == 1) {
              ray->fColor3fv(ray, cc);
              while(ok && *n) {
                c = *(n++);
                while(ok && c--) {
                  if(vc) {
                    register float *cA = vc;
                    if(rc) {
                      if(rc[0] < -1)
                        ColorGetEncoded(G, rc[0], (cA = colA));
                      rc++;
                    }
                    ray->fColor3fv(ray, cA);
                    ok &= ray->fSphere3fv(ray, v, radius);
                    vc += 3;
                  } else {
                    ok &= ray->fSphere3fv(ray, v, radius);
                  }
                  v += 3;
                }
              }
            } else {
              while(ok && *n) {
                c = *(n++);
                if(c--) {
                  v += 3;
                  if(vc) {
                    vc += 3;
                    if(rc)
                      rc++;
                  }
                  while(ok && c--) {
                    if(vc) {
                      register float *cA = vc - 3, *cB = vc;
                      if(rc) {
                        if(rc[-1] < -1)
                          ColorGetEncoded(G, rc[-1], (cA = colA));
                        if(rc[0] < -1)
                          ColorGetEncoded(G, rc[0], (cB = colB));
                        rc++;
                      }
                      ok &= ray->fSausage3fv(ray, v - 3, v, radius, cA, cB);
                      vc += 3;
                    } else {
                      ok &= ray->fSausage3fv(ray, v - 3, v, radius, cc, cc);
                    }
                    v += 3;
                  }
                }
              }
            }
          }
        } else if(G->HaveGUI && G->ValidContext) {
          if(!pick && !pass) {
	      short use_shader, use_display_lists;
	      short mesh_as_cylinders ;
	      CGO *shaderCGO = NULL;
	      use_shader = ( SettingGetGlobal_b(G, cSetting_mesh_use_shader) & SettingGetGlobal_b(G, cSetting_use_shaders)) | returnCGO;
	      use_display_lists = SettingGetGlobal_i(G, cSetting_use_display_lists);
	      mesh_as_cylinders = SettingGetGlobal_b(G, cSetting_render_as_cylinders) && SettingGetGlobal_b(G, cSetting_mesh_as_cylinders) && ms->MeshMode != 1;

	      if (ms->shaderCGO && (!use_shader || (mesh_as_cylinders ^ ms->shaderCGO->has_draw_cylinder_buffers))){
		CGOFree(ms->shaderCGO);
		ms->shaderCGO = NULL;
		ms->shaderUnitCellCGO = NULL;
	      }

	      if (ms->shaderCGO && !returnCGO) {
		ok &= ObjectMeshStateRenderShader(ms, I, info, mesh_as_cylinders, mesh_width);
		continue;
	      }

	      if (use_shader){
		shaderCGO = CGONew(G);
		if(!shaderCGO) {
		  ok = false;
		  break;
		}
		shaderCGO->use_shader = true;
	      }

	      if(ms->UnitCellCGO && I->Obj.RepVis[cRepCell]) {
		float *color = ColorGet(I->Obj.G, I->Obj.Color);
		if (!use_shader) {
		  CGORenderGL(ms->UnitCellCGO, color, I->Obj.Setting, NULL, info, NULL);
		} else if(!ms->shaderUnitCellCGO) {
		  CGO *newUnitCellCGO = CGONewSized(G, 0);
		  CGOColorv(newUnitCellCGO, color);
		  CGOAppend(newUnitCellCGO, ms->UnitCellCGO);
		  ms->shaderUnitCellCGO = CGOOptimizeToVBONotIndexed(newUnitCellCGO, 0);
		  CGOFree(newUnitCellCGO);
		  ms->shaderUnitCellCGO->use_shader = true;
		  ms->shaderUnitCellCGO->enable_shaders = false;
		}
	      }

#ifdef _PYMOL_GL_CALLLISTS
	      if (ms->displayList && ms->displayListInvalid) {
		glDeleteLists(ms->displayList, 1);
		ms->displayList = 0;
		ms->displayListInvalid = false;
	      }
	      if(use_display_lists){
		if (ms->displayList) {
		  glCallList(ms->displayList);
		  /* instead of returning, need to check if all states are being rendered */
		  if(state >= 0)
		    break;
		  a = a + 1;
		  if(a >= I->NState)
		    break;
		  continue;
		}
	      }
#endif

	      if(info && !info->line_lighting){
		if(!use_shader){
		  glDisable(GL_LIGHTING);
		} else if(!mesh_as_cylinders) {
		  ok &= CGODisable(shaderCGO, GL_LIGHTING);
		}
	      }
	      if(!ok) break;

	      if (use_shader){
		ok &= CGOResetNormal(shaderCGO, false);
	      } else {
		SceneResetNormal(I->Obj.G, false);
	      }
	      if(n && v && I->Obj.RepVis[cRepMesh]) {
		if(use_shader) {
		  vc = ms->VC;

		  if(!vc)
		    ok &= CGOColorv(shaderCGO, ColorGet(I->Obj.G, ms->OneColor));

		  if (!mesh_as_cylinders){
		    if(ms->MeshMode == 1){
		      ok &= CGODotwidth(shaderCGO, SettingGet_f
				  (I->Obj.G, I->Obj.Setting, NULL, cSetting_dot_width));
		    } else {
		      ok &= CGOLinewidthSpecial(shaderCGO, LINEWIDTH_DYNAMIC_MESH); 
		    }
		  } 

		  if(!ok) break;

		  if (mesh_as_cylinders){
		    if(returnCGO) {
		      ok &= CGOLinewidthSpecial(shaderCGO, CYLINDERWIDTH_DYNAMIC_MESH);
		    }
                    for(; ok && (c = *(n++)); v += 3, vc && (vc += 3)) {
                      for(; ok && (--c); v += 3) {
                        float axis[] = {
                          v[3] - v[0],
                          v[4] - v[1],
                          v[5] - v[2]
                        };
                        if(vc) {
                          ok &= CGOColorv(shaderCGO, vc);
			  vc += 3;
			}
                        if(vc && memcmp(vc - 3, vc, sizeof(float) * 3)) {
                          ok &= CGOShaderCylinder2ndColor(shaderCGO, v, axis, 1.f, 15, vc);
                        } else {
                          ok &= CGOShaderCylinder(shaderCGO, v, axis, 1.f, 15);
                        }
                      }
		    }
		  } else {
		    while(ok && *n) {
		      c = *(n++);
		      if(ms->MeshMode == 1)
			ok &= CGOBegin(shaderCGO, GL_POINTS);
		      else {
			if (c < 2){
			  while(c--) {
			    if(vc) {
			      vc += 3;
			    }
			    v += 3;
			  }
			  continue;
			}
			ok &= CGOBegin(shaderCGO, GL_LINE_STRIP);
		      }
		      while(ok && c--) {
			if(vc) {
			  ok &= CGOColorv(shaderCGO, vc);
			  vc += 3;
			}
			if (ok)
			  ok &= CGOVertexv(shaderCGO, v);
			v += 3;
		      }
		      if (ok)
			ok &= CGOEnd(shaderCGO);
		    }
		  }
		} else {
		  vc = ms->VC;

		  if(!vc)
		    glColor3fv(ColorGet(I->Obj.G, ms->OneColor));

		  if(ms->MeshMode == 1){
		    glPointSize(SettingGet_f
				(I->Obj.G, I->Obj.Setting, NULL, cSetting_dot_width));
		  } else {
		    glLineWidth(line_width);
		  }
		  while(*n) {
		    c = *(n++);
#ifdef _PYMOL_GL_DRAWARRAYS
		    {
		      int nverts = c;
		      ALLOCATE_ARRAY(GLfloat,ptsVals,nverts*3)
		      ALLOCATE_ARRAY(GLfloat,colorVals,nverts*4)
		      int pl = 0, plc = 0;
		      GLenum mode = GL_LINE_STRIP;
		      if(ms->MeshMode == 1){
			mode = GL_POINTS;
		      }
		      while(c--) {
			if(vc) {
			  colorVals[plc] = vc[0]; colorVals[plc+1] = vc[1]; colorVals[plc+2] = vc[2]; colorVals[plc+3] = 1.f;
			  vc += 3;
			}
			ptsVals[pl] = v[0]; ptsVals[pl+1] = v[1]; ptsVals[pl+2] = v[2];
			v += 3;
			pl += 3;
			plc += 4;
		      }
		      glEnableClientState(GL_VERTEX_ARRAY);
		      if (vc){
			glEnableClientState(GL_COLOR_ARRAY);
			glColorPointer(4, GL_FLOAT, 0, colorVals);
		      }
		      glVertexPointer(3, GL_FLOAT, 0, ptsVals);
		      glDrawArrays(mode, 0, nverts);
		      glDisableClientState(GL_VERTEX_ARRAY);
		      if (vc){
			glDisableClientState(GL_COLOR_ARRAY);
		      }
		      DEALLOCATE_ARRAY(ptsVals)
		      DEALLOCATE_ARRAY(colorVals)
		    }
#else
		    if(ms->MeshMode == 1)
		      glBegin(GL_POINTS);
		    else
		      glBegin(GL_LINE_STRIP);
		    while(c--) {
		      if(vc) {
			glColor3fv(vc);
			vc += 3;
		      }
		      glVertex3fv(v);
		      v += 3;
		    }
		    glEnd();
#endif
		  }
		}
	      }
	      if(info && !info->line_lighting){
		if(!use_shader){
		  glEnable(GL_LIGHTING);
		} else if (ok && !mesh_as_cylinders){
		  ok &= CGOEnable(shaderCGO, GL_LIGHTING);
		}
	      }

	      if (use_shader) {
		if (ok){
		  CGO *convertcgo = NULL;
		  if (ok)
		    ok &= CGOStop(shaderCGO);
#ifdef _PYMOL_CGO_DRAWARRAYS
		  if (ok)
		    convertcgo = CGOCombineBeginEnd(shaderCGO, 0);
		  CHECKOK(ok, convertcgo);
		  CGOFree(shaderCGO);    
		  shaderCGO = convertcgo;
#else
		  (void)convertcgo;
#endif
#ifdef _PYMOL_CGO_DRAWBUFFERS
		  if (returnCGO){
		    return (shaderCGO);
		  } else {
		    ms->shaderCGO = shaderCGO;
		  }
		  if (ok){
		    if (mesh_as_cylinders){
		      convertcgo = CGOOptimizeGLSLCylindersToVBOIndexed(ms->shaderCGO, 0);
		    } else {
		      convertcgo = CGOOptimizeToVBONotIndexed(ms->shaderCGO, 0);
		    }
		    CHECKOK(ok, convertcgo);
		  }
		  if (convertcgo){
		    CGOFree(ms->shaderCGO);
		    ms->shaderCGO = convertcgo;
		  }
#else
		  (void)convertcgo;
#endif
		}
		
		if(!ok) break;

                ok &= ObjectMeshStateRenderShader(ms, I, info, mesh_as_cylinders, mesh_width);
	      }
	      
#ifdef _PYMOL_GL_CALLLISTS
	      if (use_display_lists && ms->displayList){
		glEndList();
		glCallList(ms->displayList);      
	      }
#endif
      
	  }
	}
    }
  }
  if (!ok){
    I->Obj.fInvalidate(&I->Obj, cRepMesh, cRepInvPurge, -1);
    I->Obj.fInvalidate(&I->Obj, cRepCGO, cRepInvPurge, -1);
    ObjectSetRepVis(&I->Obj, cRepMesh, 0);
    ObjectSetRepVis(&I->Obj, cRepCGO, 0);
  }

  return NULL;
}


/*========================================================================*/

static int ObjectMeshGetNStates(ObjectMesh * I)
{
  return (I->NState);
}


/*========================================================================*/
ObjectMesh *ObjectMeshNew(PyMOLGlobals * G)
{
  int ok = true;
  OOAlloc(G, ObjectMesh);
  CHECKOK(ok, I);
  if (ok)
    ObjectInit(G, (CObject *) I);

  if (ok){
    I->NState = 0;
    I->State = VLAMalloc(10, sizeof(ObjectMeshState), 5, true);   /* autozero important */
    CHECKOK(ok, I->State);
  }
  if (ok){
    I->Obj.type = cObjectMesh;
    
    I->Obj.fFree = (void (*)(CObject *)) ObjectMeshFree;
    I->Obj.fUpdate = (void (*)(CObject *)) ObjectMeshUpdate;
    I->Obj.fRender = (void (*)(CObject *, RenderInfo *)) ObjectMeshRender;
    I->Obj.fInvalidate = (void (*)(CObject *, int, int, int)) ObjectMeshInvalidate;
    I->Obj.fGetNFrame = (int (*)(CObject *)) ObjectMeshGetNStates;
  }
  if (!ok){
    ObjectMeshFree(I);
    I = NULL;
  }
  /*  I->Obj.fGetCaption = (char *(*)(CObject *))ObjectMeshGetCaption; */
  return (I);
}


/*========================================================================*/
void ObjectMeshStateInit(PyMOLGlobals * G, ObjectMeshState * ms)
{
  if(ms->Active)
    ObjectStatePurge(&ms->State);
  if(ms->Field) {
    IsosurfFieldFree(ms->State.G, ms->Field);
    ms->Field = NULL;
  }
  ObjectStateInit(G, &ms->State);
  if(!ms->V) {
    ms->V = VLAlloc(float, 10000);
  }
  if(!ms->N) {
    ms->N = VLAlloc(int, 10000);
  }
  if(ms->AtomVertex) {
    VLAFreeP(ms->AtomVertex);
  }
  ms->N[0] = 0;
  ms->Active = true;
  ms->ResurfaceFlag = true;
  ms->RecolorFlag = false;
  ms->ExtentFlag = false;
  ms->CarveFlag = false;
  ms->quiet = true;
  ms->CarveBuffer = 0.0;
  ms->AtomVertex = NULL;
  ms->UnitCellCGO = NULL;
  ms->displayList = 0;
  ms->displayListInvalid = true;
  ms->caption[0] = 0;
  ms->Field = NULL;
  ms->shaderCGO = NULL;
  ms->shaderUnitCellCGO = NULL;
}


/*========================================================================*/
ObjectMesh *ObjectMeshFromXtalSym(PyMOLGlobals * G, ObjectMesh * obj, ObjectMap * map,
                                  CSymmetry * sym,
                                  int map_state,
                                  int state, float *mn, float *mx,
                                  float level, int meshMode,
                                  float carve, float *vert_vla,
                                  float alt_level, int quiet)
{
  int ok = true;
  ObjectMesh *I = NULL;
  ObjectMeshState *ms = NULL;
  ObjectMapState *oms = NULL;

  if(!obj) {
    I = ObjectMeshNew(G);
  } else {
    I = obj;
  }
  CHECKOK(ok, I);

  if (ok){
    if(state < 0)
      state = I->NState;
    if(I->NState <= state) {
      VLACheck(I->State, ObjectMeshState, state);
      CHECKOK(ok, I->State);
      if (ok)
	I->NState = state + 1;
    }
  }

  if (ok){
    ms = I->State + state;
    ObjectMeshStateInit(G, ms);
  }

  if (ok){
    strcpy(ms->MapName, map->Obj.Name);
    ms->MapState = map_state;
    oms = ObjectMapGetState(map, map_state);

    ms->Level = level;
    ms->AltLevel = alt_level;
    ms->MeshMode = meshMode;
    ms->quiet = quiet;
  }
  if(ok && oms) {
    if((meshMode == 3) && (ms->AltLevel < ms->Level)) {
      /* gradient object -- need to auto-set range */
      if(!ObjectMapStateGetDataRange(G, oms, &ms->Level, &ms->AltLevel)) {
        ms->Level = -1.0F;
        ms->AltLevel = 1.0F;
      }
    }

    copy3f(mn, ms->ExtentMin);  /* this is not exactly correct...should actually take vertex points from range */
    copy3f(mx, ms->ExtentMax);

    if(oms->State.Matrix) {
      ok &= ObjectStateSetMatrix(&ms->State, oms->State.Matrix);
    } else if(ms->State.Matrix) {
      ObjectStateResetMatrix(&ms->State);
    }

    if (ok) {
      float *min_ext, *max_ext;
      float tmp_min[3], tmp_max[3];
      if(MatrixInvTransformExtentsR44d3f(ms->State.Matrix,
                                         ms->ExtentMin, ms->ExtentMax,
                                         tmp_min, tmp_max)) {
        min_ext = tmp_min;
        max_ext = tmp_max;
      } else {
        min_ext = ms->ExtentMin;
        max_ext = ms->ExtentMax;
      }

      {
        int eff_range[6];

        if(IsosurfGetRange
           (G, oms->Field, oms->Symmetry->Crystal, min_ext, max_ext, eff_range, false)) {
          int fdim[3];
          int expand_result;
          /* need to generate symmetry-expanded temporary map */

          ms->Crystal = *(oms->Symmetry->Crystal);
          fdim[0] = eff_range[3] - eff_range[0];
          fdim[1] = eff_range[4] - eff_range[1];
          fdim[2] = eff_range[5] - eff_range[2];
          ms->Field = IsosurfFieldAlloc(I->Obj.G, fdim);

          expand_result =
            IsosurfExpand(oms->Field, ms->Field, oms->Symmetry->Crystal, sym, eff_range);

          if(expand_result == 0) {
            ok = false;
            if(!quiet) {
              PRINTFB(G, FB_ObjectMesh, FB_Warnings)
                " ObjectMesh-Warning: no symmetry expanded map points found.\n" ENDFB(G);
            }
          } else {
            if(!quiet) {
              PRINTFB(G, FB_ObjectMesh, FB_Warnings)
                " ObjectMesh-Warning: not all symmetry expanded points covered by map.\n"
                ENDFB(G);
            }
          }

          ms->Range[0] = 0;
          ms->Range[1] = 0;
          ms->Range[2] = 0;
          ms->Range[3] = fdim[0];
          ms->Range[4] = fdim[1];
          ms->Range[5] = fdim[2];

        } else {
          /* mesh entirely contained within bounds of current map */
          int a;
          for(a = 0; a < 6; a++) {
            ms->Range[a] = eff_range[a];
          }
        }
      }
    }
    ms->ExtentFlag = true;
  }
  if(ok) {
    if(carve != 0.0) {
      ms->CarveFlag = true;
      ms->CarveBuffer = carve;
      ms->AtomVertex = vert_vla;
    }
    if(I) {
      ObjectMeshRecomputeExtent(I);
    }
    I->Obj.ExtentFlag = true;
    /*  printf("Brick %d %d %d %d %d %d\n",I->Range[0],I->Range[1],I->Range[2],I->Range[3],I->Range[4],I->Range[5]); */
  }
  if(!ok) {
    ObjectMeshFree(I);
    I = NULL;
  }
  SceneChanged(G);
  SceneCountFrames(G);
  return (I);
}


/*========================================================================*/
ObjectMesh *ObjectMeshFromBox(PyMOLGlobals * G, ObjectMesh * obj, ObjectMap * map,
                              int map_state,
                              int state, float *mn, float *mx,
                              float level, int meshMode,
                              float carve, float *vert_vla, float alt_level, int quiet)
{
  ObjectMesh *I = NULL;
  ObjectMeshState *ms = NULL;
  ObjectMapState *oms = NULL;
  int ok = true;
  int created = !obj;

  if(created) {
    I = ObjectMeshNew(G);
  } else {
    I = obj;
  }
  CHECKOK(ok, I);
  if (ok){
    if(state < 0)
      state = I->NState;
    if(I->NState <= state) {
      VLACheck(I->State, ObjectMeshState, state);
      CHECKOK(ok, I->State);
      if (ok)
	I->NState = state + 1;
    }
  }

  if (ok){
    ms = I->State + state;
    ObjectMeshStateInit(G, ms);
    
    strcpy(ms->MapName, map->Obj.Name);
    ms->MapState = map_state;
    oms = ObjectMapGetState(map, map_state);
    
    ms->Level = level;
    ms->AltLevel = alt_level;
    ms->MeshMode = meshMode;
    ms->quiet = quiet;
  }
  if(ok && oms) {

    if((meshMode == 3) && (ms->AltLevel < ms->Level)) {
      /* gradient object -- need to auto-set range */
      if(!ObjectMapStateGetDataRange(G, oms, &ms->Level, &ms->AltLevel)) {
        ms->Level = -1.0F;
        ms->AltLevel = 1.0F;
      }
    }

    copy3f(mn, ms->ExtentMin);  /* this is not exactly correct...should actually take vertex points from range */
    copy3f(mx, ms->ExtentMax);

    if(oms->State.Matrix) {
      ok &= ObjectStateSetMatrix(&ms->State, oms->State.Matrix);
    } else if(ms->State.Matrix) {
      ObjectStateResetMatrix(&ms->State);
    }

    if (ok) {
      float *min_ext, *max_ext;
      float tmp_min[3], tmp_max[3];
      if(MatrixInvTransformExtentsR44d3f(ms->State.Matrix,
                                         ms->ExtentMin, ms->ExtentMax,
                                         tmp_min, tmp_max)) {
        min_ext = tmp_min;
        max_ext = tmp_max;
      } else {
        min_ext = ms->ExtentMin;
        max_ext = ms->ExtentMax;
      }

      IsosurfGetRange(G, oms->Field, oms->Symmetry->Crystal, min_ext, max_ext, ms->Range, true);
      ms->ExtentFlag = true;
    }
  }
  if(ok && carve != 0.0) {
    ms->CarveFlag = true;
    ms->CarveBuffer = carve;
    ms->AtomVertex = vert_vla;
  }
  if(ok && I) {
    ObjectMeshRecomputeExtent(I);
  }
  if (ok)
    I->Obj.ExtentFlag = true;
  /*  printf("Brick %d %d %d %d %d %d\n",I->Range[0],I->Range[1],I->Range[2],I->Range[3],I->Range[4],I->Range[5]); */
  if (ok){
    SceneChanged(G);
    SceneCountFrames(G);
  }
  if (!ok && created){
    ObjectMeshFree(I);
    I = NULL;
  }
  return (I);
}


/*========================================================================*/

void ObjectMeshRecomputeExtent(ObjectMesh * I)
{
  int extent_flag = false;
  int a;
  ObjectMeshState *ms;

  for(a = 0; a < I->NState; a++) {
    ms = I->State + a;
    if(ms->Active) {
      if(ms->ExtentFlag) {
        if(!extent_flag) {
          extent_flag = true;
          copy3f(ms->ExtentMax, I->Obj.ExtentMax);
          copy3f(ms->ExtentMin, I->Obj.ExtentMin);
        } else {
          max3f(ms->ExtentMax, I->Obj.ExtentMax, I->Obj.ExtentMax);
          min3f(ms->ExtentMin, I->Obj.ExtentMin, I->Obj.ExtentMin);
        }
      }
    }
  }

  I->Obj.ExtentFlag = extent_flag;

  if(I->Obj.TTTFlag && I->Obj.ExtentFlag) {
    float *ttt;
    double tttd[16];
    if(ObjectGetTTT(&I->Obj, &ttt, -1)) {
      convertTTTfR44d(ttt, tttd);
      MatrixTransformExtentsR44d3f(tttd,
                                   I->Obj.ExtentMin, I->Obj.ExtentMax,
                                   I->Obj.ExtentMin, I->Obj.ExtentMax);
    }
  }
}
