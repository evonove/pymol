
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
#include"ObjectGadgetRamp.h"
#include"GadgetSet.h"
#include"Base.h"
#include"MemoryDebug.h"
#include"CGO.h"
#include"Scene.h"
#include"Setting.h"
#include"PConv.h"
#include"main.h"
#include"Color.h"
#include"VFont.h"
#include"ObjectMolecule.h"
#include"Executive.h"
#include"Util.h"

#include"P.h"

void ObjectGadgetRampFree(ObjectGadgetRamp * I)
{
  ColorForgetExt(I->Gadget.Obj.G, I->Gadget.Obj.Name);
  VLAFreeP(I->Level);
  VLAFreeP(I->Color);
  VLAFreeP(I->Special);
  VLAFreeP(I->Extreme);
  ObjectGadgetPurge(&I->Gadget);
  OOFreeP(I);
}

#ifdef _PYMOL_INLINE
__inline__
#endif
static void ObjectGadgetRampCalculate(ObjectGadgetRamp * I, float v, float *result)
{
  int i;
  const float _1 = 1.0F;
  const float _0 = 0.0F;
  /* from Filipe Maia */

  /* All of this functions are taken right of the gnuplot manual  */
  if(v > _1)
    v = _1;
  else if(v < _0)
    v = _0;

  switch (I->CalcMode) {
  case cRAMP_TRADITIONAL:
    result[0] = (float) sqrt(v);
    result[1] = v * v * v;
    result[2] = (float) sin(v * 2 * cPI);
    break;
  case cRAMP_SLUDGE:
    result[0] = v;
    result[1] = (float) fabs(v - 0.5F);
    result[2] = v * v * v * v;
    break;
  case cRAMP_OCEAN:
    result[0] = 3 * v - 2;
    result[1] = (float) fabs((3 * v - 1) / 2);
    result[2] = v;
    break;
  case cRAMP_HOT:
    result[0] = 3 * v;
    result[1] = 3 * v - 1;
    result[2] = 3 * v - 2;
    break;
  case cRAMP_GRAYABLE:
    result[0] = v / 0.32F - 0.78125F;
    result[1] = 2 * v - 0.84F;
    result[2] = v / 0.08F - 11.5F;      /* I'm not so sure about this one */
    break;
  case cRAMP_RAINBOW:
    result[0] = (float) fabs(2 * v - 0.5F);
    result[1] = (float) sin(v * cPI);
    result[2] = (float) cos(v * cPI / 2.0F);
    break;
  case cRAMP_AFMHOT:
    result[0] = 2 * v;
    result[1] = 2 * v - 0.5F;
    result[2] = 2 * v - 1.0F;
    break;
  case cRAMP_GRAYSCALE:
    result[0] = v;
    result[1] = v;
    result[2] = v;
    break;
  default:                     /* default is simply white */
    result[0] = 1.0F;
    result[1] = 1.0F;
    result[2] = 1.0F;
    break;
  }
  for(i = 0; i < 3; i++) {
    if(result[i] > 1.0F) {
      result[i] = 1.0F;
    } else if(result[i] < 0.0F) {
      result[i] = 0.0F;
    }
  }
}

#ifdef _PYMOL_INLINE
__inline__
#endif
static int _ObjectGadgetRampInterpolate(ObjectGadgetRamp * I, float level, float *color,
                                        float *table, float *extreme)
{
  register float *i_level = I->Level;
  register int n_level = I->NLevel;
  const float _0 = 0.0F;
  const float _1 = 1.0F;
  int ok = true;
  if(i_level && table) {
    register int level_is_ge = -1;
    register int level_is_le = n_level;
    register int i = 0;
    i = n_level - 1;
    while(i >= 0) {
      register float f = i_level[i];
      if(level >= f) {
        level_is_ge = i;
        break;
      }
      i--;
    }
    i = 0;
    while(i < n_level) {
      register float f = i_level[i];
      if(level <= f) {
        level_is_le = i;
        break;
      } else
        i++;
    }
    if(level_is_ge != level_is_le) {
      if(level_is_le == 0) {    /* lower extreme */
        register float *v;
        if(extreme) {
          v = extreme;
        } else {
          v = table;
        }
        copy3f(v, color);
      } else if(level_is_ge == (n_level - 1)) { /* upper extreme */
        register float *v;
        if(extreme) {
          v = extreme + 3;
        } else {
          v = table + 3 * (n_level - 1);
        }
        copy3f(v, color);
      } else {
        register float d, x0, x1;

        d = i_level[level_is_ge] - i_level[level_is_le];
        if(fabs(d) > R_SMALL8) {
          x0 = (level - i_level[level_is_le]) / d;
          x1 = 1.0F - x0;
          for(i = 0; i < 3; i++) {
            color[i] = x0 * table[3 * level_is_ge + i] + x1 * table[3 * level_is_le + i];
          }
          clamp3f(color);
        } else {
          register float *v = table + 3 * level_is_ge;
          copy3f(v, color);
        }
      }
    } else {                    /* dead on the specified level */
      copy3f(table + 3 * level_is_ge, color);
      clamp3f(color);
    }
  } else {
    float base, range;
    if(n_level && i_level) {
      base = i_level[0];
      range = i_level[n_level - 1] - base;
      if(fabs(range) < R_SMALL8)
        range = _1;
    } else {
      base = _0;
      range = _1;
    }
    level = (level - base) / range;
    ObjectGadgetRampCalculate(I, level, color);
  }
  return (ok);
}

#ifdef _PYMOL_INLINE
__inline__
#endif
static int _ObjectGadgetRampBlend(ObjectGadgetRamp * I, float *color,
                                  float *table, float *extreme, int mode)
{
  /* this capability needs to be re-thought */

  register float *i_level = I->Level;
  register int n_level = I->NLevel;
  const float _1 = 1.0F;
  float avg[3];
  int ok = true;
  zero3f(avg);

  {
    int cnt = 0;
    switch (mode) {
    case 1:
    case 2:
      break;
    default:
      if(i_level && table) {
        int i;
        for(i = 0; i < n_level; i++) {
          add3f(table + 3 * i, avg, avg);
          cnt++;
        }
        if(extreme) {
          add3f(extreme, avg, avg);
          add3f(extreme + 3, avg, avg);
          cnt += 2;
        }
        if(cnt) {
          float fact = _1 / cnt;
          scale3f(avg, fact, avg);
        }
        clamp3f(avg);
      }
      copy3f(avg, color);
    }
  }

  switch (mode) {
  case 1:                      /* min components */
  case 3:
    ones3f(color);
    if(i_level && table) {
      int i, j;
      for(i = 0; i < n_level; i++) {
        for(j = 0; j < 3; j++) {
          color[j] = (color[j] < table[3 * i + j]) ? color[j] : table[3 * i + j];
        }
      }
      if(extreme) {
        for(j = 0; j < 3; j++) {
          color[j] = (color[j] < extreme[j]) ? color[j] : extreme[j];
          color[j] = (color[j] < extreme[j + 3]) ? color[j] : extreme[j + 3];
        }
      }
      clamp3f(color);
    }
    if(mode == 3) {             /* average serves as a minimum */
      int j;
      for(j = 0; j < 3; j++) {
        color[j] = (color[j] > avg[j]) ? color[j] : avg[j];
      }
    }
    break;
  case 2:                      /* max components */
    zero3f(color);
    if(i_level && table) {
      int i, j;
      for(i = 0; i < n_level; i++) {
        for(j = 0; j < 3; j++) {
          color[j] = (color[j] > table[3 * i + j]) ? color[j] : table[3 * i + j];
        }
      }
      if(extreme) {
        for(j = 0; j < 3; j++) {
          color[j] = (color[j] > extreme[j]) ? color[j] : extreme[j];
          color[j] = (color[j] > extreme[j + 3]) ? color[j] : extreme[j + 3];
        }
      }
      clamp3f(color);
    }
    break;
  default:                     /* simple average of all colors */
    copy3f(avg, color);
    break;
  }
  return (ok);
}

#define MAX_COLORS 64

#ifdef _PYMOL_INLINE
__inline__
#endif
static int ObjectGadgetRampInterpolateWithSpecial(ObjectGadgetRamp * I,
                                                  float level,
                                                  float *color,
                                                  float *atomic,
                                                  float *object,
                                                  float *vertex, int state, int blend_all)
{
  /* now thread-safe...via stack copy of colors */

  float stack_color[MAX_COLORS * 3];
  float stack_extreme[6];
  register float *i_level = I->Level;
  register float *i_color = I->Color;
  register int *i_special = I->Special;
  if(i_level && i_color && i_special) {
    register int i = 0;
    register int n_level = I->NLevel;
    register float *i_extreme = I->Extreme;
    register float *extreme = i_extreme;
    /* mix special coloring into the table */
    register float *src = i_color, *dst = stack_color;
    if((n_level + 2) > MAX_COLORS)
      n_level = MAX_COLORS - 2;
    while(i < n_level) {
      register int index = i_special[i];
      switch (index) {
      case 0:
        *(dst++) = *(src++);
        *(dst++) = *(src++);
        *(dst++) = *(src++);
        break;
      case cColorDefault:
      case cColorAtomic:
        copy3f(atomic, dst);
        src += 3;
        dst += 3;
        break;
      case cColorObject:
        copy3f(object, dst);
        dst += 3;
        src += 3;
        break;
      default:                 /* allow nested ramps */
        if(ColorCheckRamped(I->Gadget.Obj.G, index)) {
          ColorGetRamped(I->Gadget.Obj.G, index, vertex, dst, state);
        } else {
          copy3f(src, dst);
        }
        dst += 3;
        src += 3;
        break;
      }
      i++;
    }
    /* mix extreme colors in (if present) */
    if(i_extreme) {
      i = 0;
      extreme = stack_extreme;
      src = i_extreme;
      dst = stack_extreme;
      while(i < 2) {
        register int index = i_special[i + n_level];
        switch (index) {
        case 0:
          *(dst++) = *(src++);
          *(dst++) = *(src++);
          *(dst++) = *(src++);
          break;
        case cColorDefault:
        case cColorAtomic:
          copy3f(atomic, dst);
          dst += 3;
          src += 3;
          break;
        case cColorObject:
          copy3f(object, dst);
          dst += 3;
          src += 3;
          break;
        default:               /* allow nested ramps */
          if(ColorCheckRamped(I->Gadget.Obj.G, index)) {
            ColorGetRamped(I->Gadget.Obj.G, index, vertex, dst, state);
          } else {
            copy3f(src, dst);
          }
          dst += 3;
          src += 3;
          break;
        }
        i++;
      }
    }
    /* interpolate using stack tables */
    if(blend_all)
      return _ObjectGadgetRampBlend(I, color, stack_color, extreme, I->SrcState);
    else
      return _ObjectGadgetRampInterpolate(I, level, color, stack_color, extreme);
  } else {
    /* interpolate using static tables */
    if(blend_all)
      return _ObjectGadgetRampBlend(I, color, i_color, I->Extreme, I->SrcState);
    else
      return _ObjectGadgetRampInterpolate(I, level, color, i_color, I->Extreme);
  }
}

int ObjectGadgetRampInterpolate(ObjectGadgetRamp * I, float level, float *color)
{
  int result = _ObjectGadgetRampInterpolate(I, level, color, I->Color, I->Extreme);
  return result;
}

PyObject *ObjectGadgetRampAsPyList(ObjectGadgetRamp * I)
{
#ifdef _PYMOL_NOPY
  return NULL;
#else

  PyObject *result = NULL;

  result = PyList_New(11);

  PyList_SetItem(result, 0, ObjectGadgetPlainAsPyList(&I->Gadget));
  PyList_SetItem(result, 1, PyInt_FromLong(I->RampType));
  PyList_SetItem(result, 2, PyInt_FromLong(I->NLevel));
  if(I->Level && I->NLevel) {
    PyList_SetItem(result, 3, PConvFloatVLAToPyList(I->Level));
  } else {
    PyList_SetItem(result, 3, PConvAutoNone(NULL));
  }
  if(I->Color && I->NLevel) {
    PyList_SetItem(result, 4, PConvFloatVLAToPyList(I->Color));
  } else {
    PyList_SetItem(result, 4, PConvAutoNone(NULL));
  }
  PyList_SetItem(result, 5, PyInt_FromLong(I->var_index));
  PyList_SetItem(result, 6, PyString_FromString(I->SrcName));
  PyList_SetItem(result, 7, PyInt_FromLong(I->SrcState));
  PyList_SetItem(result, 8, PyInt_FromLong(I->CalcMode));
  if(I->Special && I->NLevel) {
    PyList_SetItem(result, 9, PConvIntVLAToPyList(I->Special));
  } else {
    PyList_SetItem(result, 9, PConvAutoNone(NULL));
  }
  if(I->Extreme && I->NLevel) {
    PyList_SetItem(result, 10, PConvFloatVLAToPyList(I->Extreme));
  } else {
    PyList_SetItem(result, 10, PConvAutoNone(NULL));
  }
  return (PConvAutoNone(result));
#endif
}

int ObjectGadgetRampNewFromPyList(PyMOLGlobals * G, PyObject * list,
                                  ObjectGadgetRamp ** result, int version)
{
#ifdef _PYMOL_NOPY
  return 0;
#else

  ObjectGadgetRamp *I = NULL;
  int ok = true;
  int ll = 0;

  if(ok)
    I = ObjectGadgetRampNew(G);
  if(ok)
    ok = (I != NULL);
  if(ok)
    ok = (list != NULL);
  if(ok)
    ok = PyList_Check(list);
  if(ok)
    ll = PyList_Size(list);
  /* TO SUPPORT BACKWARDS COMPATIBILITY...
     Always check ll when adding new PyList_GetItem's */

  if(ok)
    ok = ObjectGadgetInitFromPyList(G, PyList_GetItem(list, 0), &I->Gadget, version);
  if(ok)
    ok = PConvPyIntToInt(PyList_GetItem(list, 1), &I->RampType);
  if(ok)
    ok = PConvPyIntToInt(PyList_GetItem(list, 2), &I->NLevel);
  if(ok && I->NLevel)
    ok = PConvPyListToFloatVLA(PyList_GetItem(list, 3), &I->Level);
  if(ok && I->NLevel) {
    PyObject *item = PyList_GetItem(list, 4);
    if(item != Py_None) {
      ok = PConvPyListToFloatVLA(item, &I->Color);
    }
  }
  if(ok)
    ok = PConvPyIntToInt(PyList_GetItem(list, 5), &I->var_index);
  if(ok)
    ok = PConvPyStrToStr(PyList_GetItem(list, 6), I->SrcName, WordLength);
  if(ok)
    ok = PConvPyIntToInt(PyList_GetItem(list, 7), &I->SrcState);
  if(ok && (ll > 8))
    ok = PConvPyIntToInt(PyList_GetItem(list, 8), &I->CalcMode);
  if(ok && (ll > 9)) {
    PyObject *item = PyList_GetItem(list, 9);
    if(item != Py_None) {
      ok = PConvPyListToIntVLA(item, &I->Special);
    }
  } else {
    I->Special = NULL;
  }
  if(ok && I->NLevel && (ll > 10)) {
    PyObject *item = PyList_GetItem(list, 10);
    if(item != Py_None) {
      ok = PConvPyListToFloatVLA(item, &I->Extreme);
    }
  } else {
    I->Extreme = NULL;
  }
  if(ok)
    ObjectGadgetUpdateStates(&I->Gadget);
  if(ok)
    ObjectGadgetUpdateExtents(&I->Gadget);
  if(ok)
    (*result) = I;
  return (ok);
#endif
}

int ObjectGadgetRampInterVertex(ObjectGadgetRamp * I, float *pos, float *color, int state)
{
  float level;
  int ok = true;
  switch (I->RampType) {
  case cRampMap:
    if(!I->Map)
      I->Map = ExecutiveFindObjectMapByName(I->Gadget.Obj.G, I->SrcName);
    if(!ExecutiveValidateObjectPtr(I->Gadget.Obj.G, (CObject *) I->Map, cObjectMap))
      ok = false;
    else {
      int src_state;
      if(I->SrcState >= 0)
        src_state = I->SrcState;
      else
        src_state = state;
      if(src_state < 0)
        src_state = SceneGetState(I->Gadget.Obj.G);
      if(ok)
        ok = (I->Map != NULL);
      if(ok)
        ok = ObjectMapInterpolate(I->Map, src_state, pos, &level, NULL, 1);
      if(ok)
        ok = ObjectGadgetRampInterpolate(I, level, color);
    }
    break;
  case cRampMol:
    if(!I->Mol)
      I->Mol = ExecutiveFindObjectMoleculeByName(I->Gadget.Obj.G, I->SrcName);
    if(!ExecutiveValidateObjectPtr(I->Gadget.Obj.G, (CObject *) I->Mol, cObjectMolecule))
      ok = false;
    else {
      float cutoff = 1.0F;
      float dist;
      int sub_vdw = false;
      if(state < 0)
        state = SceneGetState(I->Gadget.Obj.G);
      if(I->Level && I->NLevel) {
        cutoff = I->Level[I->NLevel - 1];
        if(I->Level[0] < 0.0F) {
          sub_vdw = true;
          cutoff += MAX_VDW;
        }
      }
      if(ok)
        ok = (I->Mol != NULL);
      if(ok) {
        if(SettingGet_b
           (I->Gadget.Obj.G, I->Gadget.Obj.Setting, NULL,
            cSetting_ramp_blend_nearby_colors)) {
          float atomic[3];
          int index =
            ObjectMoleculeGetNearestBlendedColor(I->Mol, pos, cutoff, state, &dist,
                                                 atomic, sub_vdw);
          if(index >= 0) {
            float *object = ColorGetRaw(I->Gadget.Obj.G, I->Mol->Obj.Color);

            if(!ObjectGadgetRampInterpolateWithSpecial(I, dist, color, atomic,
                                                       object, pos, state, false)) {
              copy3f(I->Color, color);
            }
          } else {
            float white[3] = { 1.0F, 1.0F, 1.0F };
            if(!ObjectGadgetRampInterpolateWithSpecial(I, cutoff + 1.0F, color, white,
                                                       white, pos, state, false)) {
              copy3f(I->Color, color);
            }
          }
        } else {
          int index =
            ObjectMoleculeGetNearestAtomIndex(I->Mol, pos, cutoff, state, &dist);
          if(index >= 0) {
            float *atomic = ColorGetRaw(I->Gadget.Obj.G, I->Mol->AtomInfo[index].color);
            float *object = ColorGetRaw(I->Gadget.Obj.G, I->Mol->Obj.Color);

            if(sub_vdw) {
              dist -= I->Mol->AtomInfo[index].vdw;
              if(dist < 0.0F)
                dist = 0.0F;
            }

            if(!ObjectGadgetRampInterpolateWithSpecial(I, dist, color, atomic,
                                                       object, pos, state, false)) {
              copy3f(I->Color, color);
            }
          } else {
            float white[3] = { 1.0F, 1.0F, 1.0F };
            if(!ObjectGadgetRampInterpolateWithSpecial(I, cutoff + 1.0F, color, white,
                                                       white, pos, state, false)) {
              copy3f(I->Color, color);
            }
          }
        }
      }
    }
    break;
  case cRampNone:
    {
      float white[3] = { 1.0F, 1.0F, 1.0F };
      if(!ObjectGadgetRampInterpolateWithSpecial(I, 0.0F, color, white, white, pos, state, true)) {     /* simple blend */
        copy3f(I->Color, color);
      }
    }
    break;
  default:
    ok = false;
    break;
  }
  return (ok);
}

static void ObjectGadgetRampUpdateCGO(ObjectGadgetRamp * I, GadgetSet * gs)
{
  CGO *cgo;
  int n_extra;
  int a, c = 0;
  float *p;
  char buffer[255];
  float white[3] = { 1.0F, 1.0F, 1.0F };
  int blocked = false;
  int font_id = 0;

  blocked = PAutoBlock(I->Gadget.Obj.G);
  font_id = VFontLoad(I->Gadget.Obj.G, 1.0, 1, 1, true);
  if(blocked)
    PUnblock(I->Gadget.Obj.G);

  cgo = CGONewSized(I->Gadget.Obj.G, 100);

  /* behind text */
  CGOBegin(cgo, GL_TRIANGLE_STRIP);
  CGOColor(cgo, 0.05F, 0.05F, 0.05F);
  CGONormal(cgo, 0.f, 0.f, 1.f); // normal 2
  CGOVertex(cgo, I->border, -(I->border + I->bar_height), I->border); // 9
  CGOVertex(cgo, I->border, -(I->height + I->border), I->border); // 7
  CGOVertex(cgo, I->width + I->border, -(I->border + I->bar_height), I->border); // 10
  CGOVertex(cgo, I->width + I->border, -(I->height + I->border), I->border); // 8
  CGOEnd(cgo);


  CGOColor(cgo, 1.0F, 1.0F, 1.0F);
  if(I->Level && I->NLevel) {
    float pos[] = { I->border + I->text_border, I->text_border - (I->border + I->height),
		    I->border + I->text_raise };
    float scale[] = { I->text_scale_h, I->text_scale_v };
    float axes[] = { 1.0F, 0.0F, 0.0F,
		     0.0F, 1.0F, 0.0F,
		     0.0F, 0.0F, 1.0F };
    /* left text for ramp */
    sprintf(buffer, "%0.3f", I->Level[0]);
    VFontWriteToCGO(I->Gadget.Obj.G, font_id, cgo, buffer, pos, scale, axes);

    /* right text, right justified for ramp */
    pos[0] = I->width + I->border; pos[1] = I->text_border - (I->border + I->height);
    pos[2] = I->border + I->text_raise ;
    sprintf(buffer, "%0.3f", I->Level[I->NLevel - 1]);
    /* indent for right justification */
    VFontIndent(I->Gadget.Obj.G, font_id, buffer, pos, scale, axes, -1.f);
    VFontWriteToCGO(I->Gadget.Obj.G, font_id, cgo, buffer, pos, scale, axes);
  }

  /* center */
  CGOBegin(cgo, GL_TRIANGLE_STRIP);
  CGONormal(cgo, 0.f, 0.f, 1.f); // normal 2
  if(I->Color) {
    n_extra = 3 * I->NLevel;
    if(n_extra < 6)
      n_extra = 6;
    {
      VLACheck(gs->Coord, float, (I->var_index + n_extra) * 3);
      c = I->var_index;
      p = gs->Coord + 3 * c;
      if(I->NLevel > 1) {
        for(a = 0; a < I->NLevel; a++) {
          if(I->Special && (I->Special[a] < 0)) {
            CGOColorv(cgo, white);
          } else {
            float tmp[3], *src = I->Color + 3 * a;
            copy3f(src, tmp);
            ColorLookupColor(I->Gadget.Obj.G, tmp);
            CGOColorv(cgo, tmp);
          }
	  CGOVertex(cgo,  I->border + (I->width * a) / (I->NLevel - 1),
		    -I->border, I->border);
	  CGOVertex(cgo,  I->border + (I->width * a) / (I->NLevel - 1),
		    -(I->border + I->bar_height), I->border);
        }
      } else {
        for(a = 0; a < 2; a++) {
          if(I->Special && (I->Special[0] < 0)) {
            CGOColorv(cgo, white);
          } else {
            float tmp[3], *src = I->Color;
            copy3f(src, tmp);
            ColorLookupColor(I->Gadget.Obj.G, tmp);
            CGOColorv(cgo, tmp);
          }

	  CGOVertex(cgo, I->border + (I->width * a), -I->border, I->border);
	  CGOVertex(cgo, I->border + (I->width * a), -(I->border + I->bar_height), I->border);
        }
      }
    }
  } else {
    int samples = 20;
    float fxn;
    float color[3];
    n_extra = 3 * samples;
    VLACheck(gs->Coord, float, (I->var_index + n_extra) * 3);
    c = I->var_index;
    p = gs->Coord + 3 * c;

    for(a = 0; a < samples; a++) {
      fxn = a / (samples - 1.0F);

      ObjectGadgetRampCalculate(I, fxn, color);
      CGOColorv(cgo, color);

      CGOVertex(cgo, I->border + (I->width * fxn), -I->border, I->border);
      CGOVertex(cgo, I->border + (I->width * fxn), -(I->border + I->bar_height), I->border);
    }
  }
  gs->NCoord = c;
  /* center */
  CGOEnd(cgo);

  CGOColor(cgo, 1.F, 1.F, 1.F);

  /* top */
  CGOBegin(cgo, GL_TRIANGLE_STRIP);

  CGONormal(cgo, 0.f, 0.f, 1.f); // normal 2
  CGOVertex(cgo, I->border, -I->border, I->border); // 5
  CGOVertex(cgo, I->width + I->border, -I->border, I->border); // 6
  CGONormal(cgo, 0.f, 1.f, .1f); // normal 1
  CGOVertex(cgo, 0.0, 0.0, 0.0); // 1
  CGOVertex(cgo, I->width + I->border * 2, 0.0, 0.0); // 2
  CGOEnd(cgo);

  /* bottom */
  CGOBegin(cgo, GL_TRIANGLE_STRIP);
  CGONormal(cgo, 0.f, -1.f, .1f); // normal 4
  CGOVertex(cgo, 0.f, -(I->height + I->border * 2), 0.f); // 3
  CGOVertex(cgo, I->width + I->border * 2, -(I->height + I->border * 2), 0.f); // 4
  CGONormal(cgo, 0.f, 0.f, 1.f); // normal 2
  CGOVertex(cgo, I->border, -(I->height + I->border), I->border); // 7
  CGOVertex(cgo, I->width + I->border, -(I->height + I->border), I->border); // 8
  CGOEnd(cgo);

  /* left */
  CGOBegin(cgo, GL_TRIANGLE_STRIP);
  CGONormal(cgo, -1.f, 0.f, 0.1f); // normal 3
  CGOVertex(cgo, 0.f, 0.f, 0.f); // 1
  CGOVertex(cgo, 0.f, -(I->height + I->border * 2), 0.f); // 3
  CGONormal(cgo, 0.f, 0.f, 1.f); // normal 2
  CGOVertex(cgo, I->border, -I->border, I->border); // 5
  CGOVertex(cgo, I->border, -(I->height + I->border), I->border); // 7
  CGOEnd(cgo);

  /* right */
  CGOBegin(cgo, GL_TRIANGLE_STRIP);
  CGONormal(cgo, 0.f, 0.f, 1.f); // normal 2
  CGOVertex(cgo, I->width + I->border, -I->border, I->border); // 6
  CGOVertex(cgo, I->width + I->border, -(I->height + I->border), I->border); // 8
  CGONormal(cgo, 1.f, 0.f, .1f); // normal 0
  CGOVertex(cgo, I->width + I->border * 2, 0.f, 0.f); // 2
  CGOVertex(cgo, I->width + I->border * 2, -(I->height + I->border * 2), 0.f); // 4
  CGOEnd(cgo);
  CGOStop(cgo);

  CGOFree(gs->ShapeCGO);
  gs->ShapeCGO = cgo;

  //#ifndef _PYMOL_NOPY
  CGOPreloadFonts(gs->ShapeCGO);
  //#endif

  cgo = CGONewSized(I->Gadget.Obj.G, 100);
  CGODotwidth(cgo, 5);
  CGOPickColor(cgo, 0, cPickableGadget);

  /* top */
  CGOBegin(cgo, GL_TRIANGLE_STRIP);
  CGOVertex(cgo, 0.f, 0.f, 0.f); // 1
  CGOVertex(cgo, I->width + I->border * 2, 0.f, 0.f); // 2
  CGOVertex(cgo, I->border, -I->border, I->border); // 5
  CGOVertex(cgo, I->width + I->border, -I->border, I->border); // 6
  CGOEnd(cgo);

  /* bottom */
  CGOBegin(cgo, GL_TRIANGLE_STRIP);
  CGOVertex(cgo, 0.f, -(I->height + I->border * 2), 0.f); // 3
  CGOVertex(cgo, I->width + I->border * 2, -(I->height + I->border * 2), 0.f); // 4
  CGOVertex(cgo, I->border, -(I->height + I->border), I->border); // 7
  CGOVertex(cgo, I->width + I->border, -(I->height + I->border), I->border); // 8
  CGOEnd(cgo);

  /* left */
  CGOBegin(cgo, GL_TRIANGLE_STRIP);
  CGOVertex(cgo, 0.f, 0.f, 0.f); // 1
  CGOVertex(cgo, 0.f, -(I->height + I->border * 2), 0.f); // 3
  CGOVertex(cgo, I->border, -I->border, I->border); // 5
  CGOVertex(cgo, I->border, -(I->height + I->border), I->border); // 7
  CGOEnd(cgo);

  /* right */
  CGOBegin(cgo, GL_TRIANGLE_STRIP);
  CGOVertex(cgo, I->width + I->border, -I->border, I->border); // 6
  CGOVertex(cgo, I->width + I->border, -(I->height + I->border), I->border); // 8
  CGOVertex(cgo, I->width + I->border * 2, 0.f, 0.f); // 2
  CGOVertex(cgo, I->width + I->border * 2, -(I->height + I->border * 2), 0.f); // 4
  CGOEnd(cgo);

  /* band */
  CGOPickColor(cgo, 1, cPickableGadget);
  CGOBegin(cgo, GL_TRIANGLE_STRIP);
  CGOVertex(cgo, I->border, -I->border, I->border); // 5
  CGOVertex(cgo, I->width + I->border, -I->border, I->border); // 6
  CGOVertex(cgo, I->border, -(I->height + I->border), I->border); // 7
  CGOVertex(cgo, I->width + I->border, -(I->height + I->border), I->border); // 8
  CGOEnd(cgo);
  CGOStop(cgo);

  CGOFree(gs->PickShapeCGO);

  gs->PickShapeCGO = cgo;
}

static void ObjectGadgetRampBuild(ObjectGadgetRamp * I)
{
  GadgetSet *gs = NULL;
  ObjectGadget *og;

  OrthoBusyPrime(I->Gadget.Obj.G);

  og = &I->Gadget;
  gs = GadgetSetNew(I->Gadget.Obj.G);

  gs->NCoord = 2;
  I->var_index = gs->NCoord;
  gs->Coord = VLAlloc(float, gs->NCoord * 3);

  gs->Coord[0] = I->x;
  gs->Coord[1] = I->y;
  gs->Coord[2] = .3f;
  gs->Coord[3] = gs->Coord[4] = gs->Coord[5] = 0.f;
  gs->NNormal = 0;
  gs->Normal = NULL;

  og->GSet[0] = gs;
  og->NGSet = 1;
  og->Obj.Context = 1;          /* unit window */
  gs->Obj = (ObjectGadget *) I;
  gs->State = 0;

  ObjectGadgetRampUpdateCGO(I, gs);
  gs->fUpdate(gs);

}


/*========================================================================*/
void ObjectGadgetRampUpdate(ObjectGadgetRamp * I)
{
  float scale;

  if(I->Gadget.Changed) {
    scale = (1.0F + 5 * I->Gadget.GSet[0]->Coord[1 * 3]);
    I->Gadget.GSet[0]->Coord[1 * 3] = 0.0;
    switch (I->RampType) {
    case cRampMol:
      {
        int a;
        for(a = 0; a < I->NLevel; a++) {
          I->Level[a] = I->Level[a] * scale;
        }
        ExecutiveInvalidateRep(I->Gadget.Obj.G, cKeywordAll, cRepAll, cRepInvColor);
      }
      break;
    default:
      if(I->NLevel == 2) {
        {
          float mean = (I->Level[0] + I->Level[1]) / 2.0F;
          I->Level[0] = (I->Level[0] - mean) * scale + mean;
          I->Level[2] = (I->Level[1] - mean) * scale + mean;
          ExecutiveInvalidateRep(I->Gadget.Obj.G, cKeywordAll, cRepAll, cRepInvColor);
        }
        break;
      } else if(I->NLevel == 3) {
        I->Level[0] = (I->Level[0] - I->Level[1]) * scale + I->Level[1];
        I->Level[2] = (I->Level[2] - I->Level[1]) * scale + I->Level[1];
        ExecutiveInvalidateRep(I->Gadget.Obj.G, cKeywordAll, cRepAll, cRepInvColor);
      }
    }
    if(I->Gadget.NGSet)
      if(I->Gadget.GSet[0]) {
        ObjectGadgetRampUpdateCGO(I, I->Gadget.GSet[0]);
        ObjectGadgetUpdateStates(&I->Gadget);
      }
    ObjectGadgetUpdateExtents(&I->Gadget);
    I->Gadget.Changed = false;
    SceneChanged(I->Gadget.Obj.G);
  }
}

static int ObjectGadgetRampHandleInputColors(ObjectGadgetRamp * I)
{
  int ok = true;
  if(I->NLevel < 1) {
    I->Level = VLASetSize(I->Level, 1);
    I->NLevel = 1;
    I->Level[0] = 0.0F;
  }
  if(ok && I->Color) {          /* handle cases where number of colors doesn't make expectation */
    int n_color = VLAGetSize(I->Color) / 3;

    if(!n_color) {
      I->Color = VLASetSize(I->Color, 3);
      I->Color[0] = I->Color[1] = I->Color[2] = 1.0F;
      n_color++;
    }

    if(n_color < I->NLevel) {
      I->Color = VLASetSize(I->Color, 3 * I->NLevel);
      while(n_color < I->NLevel) {
        float *v0 = I->Color + 3 * (n_color - 1);
        float *v1 = v0 + 3;
        copy3f(v0, v1);
        n_color++;
      }
    } else if(n_color > I->NLevel) {
      I->Extreme = VLAlloc(float, 6);   /* above/below extreme values */
      if(I->Extreme) {
        if(n_color == I->NLevel + 1) {  /* upper extreme only */
          copy3f(I->Color, I->Extreme);
          copy3f(I->Color + 3 * I->NLevel, I->Extreme + 3);
        } else {                /* both extremes */
          copy3f(I->Color + (3 * I->NLevel), I->Extreme);
          copy3f(I->Color + (3 * I->NLevel) + 3, I->Extreme + 3);
        }
      }
      I->Color = VLASetSize(I->Color, 3 * I->NLevel);
    }
  }
  if(ok && I->Color && I->NLevel) {     /* detect default coloring as negative R */
    I->Special = VLACalloc(int, I->NLevel + 2);
    if(I->Special && I->Color) {
      int a;
      for(a = 0; a < I->NLevel; a++) {
        if(I->Color[a * 3] < 0.0F) {
          I->Special[a] = (int) I->Color[a * 3];
        }
      }
      if(I->Extreme) {          /* Extremes */
        for(a = 0; a < 2; a++) {
          if(I->Extreme[a * 3] < 0.0F) {        /* is red negative? then color is special */
            I->Special[I->NLevel + a] = (int) I->Extreme[a * 3];
          }
        }
      }
    }
  }

  return ok;
}


/*========================================================================*/
ObjectGadgetRamp *ObjectGadgetRampMapNewAsDefined(PyMOLGlobals * G,
                                                  ObjectMap * map,
                                                  float *level_vla,
                                                  float *color_vla, int map_state,
                                                  float *vert_vla, float beyond,
                                                  float within, float sigma, int zero,
                                                  int calc_mode)
{

  ObjectGadgetRamp *I;
  int ok = true;
  I = ObjectGadgetRampNew(G);
  I->RampType = cRampMap;

  if(ok)
    I->Color = color_vla;
  if(ok)
    I->CalcMode = calc_mode;

  if(ok) {
    ObjectMapState *ms;
    float tmp_level[3];
    if(map_state < 0)
      map_state = 0;
    if(vert_vla && (ms = ObjectMapGetState(map, map_state))) {
      if(ObjectMapStateGetExcludedStats(G, ms, vert_vla, beyond, within, tmp_level)) {
        tmp_level[0] = tmp_level[1] + (tmp_level[0] - tmp_level[1]) * sigma;
        tmp_level[2] = tmp_level[1] + (tmp_level[2] - tmp_level[1]) * sigma;
        if(zero) {
          if(tmp_level[1] < 0.0F) {
            tmp_level[1] = 0.0F;
            tmp_level[2] = -tmp_level[0];
          } else if(tmp_level[1] > 0.0F) {
            tmp_level[1] = 0.0F;
            tmp_level[0] = -tmp_level[2];
          }
        }
      }
      I->Level = VLAlloc(float, 3);
      copy3f(tmp_level, I->Level);
      VLAFreeP(level_vla);
    } else {
      I->Level = level_vla;
    }
  }
  if(ok)
    I->NLevel = VLAGetSize(I->Level);
  if(ok)
    ok = ObjectGadgetRampHandleInputColors(I);
  ObjectGadgetRampBuild(I);
  UtilNCopy(I->SrcName, map->Obj.Name, WordLength);
  I->SrcState = map_state;

  return (I);

}


/*========================================================================*/
ObjectGadgetRamp *ObjectGadgetRampMolNewAsDefined(PyMOLGlobals * G, ObjectMolecule * mol,
                                                  float *level_vla,
                                                  float *color_vla,
                                                  int mol_state, int calc_mode)
{

  ObjectGadgetRamp *I;
  int ok = true;

  I = ObjectGadgetRampNew(G);
  if(mol)
    I->RampType = cRampMol;
  else
    I->RampType = cRampNone;
  if(ok)
    I->Color = color_vla;
  if(ok)
    I->CalcMode = calc_mode;
  if(ok)
    I->Level = level_vla;
  if(ok)
    I->NLevel = VLAGetSize(I->Level);
  if(ok)
    ok = ObjectGadgetRampHandleInputColors(I);

  if(ok && I->Level && I->NLevel) {
    int a;
    float last = I->Level[0];
    for(a = 1; a < I->NLevel; a++) {
      if(I->Level[a] < last)
        I->Level[a] = last;
      last = I->Level[a];
    }
  }

  ObjectGadgetRampBuild(I);
  if(mol) {
    UtilNCopy(I->SrcName, mol->Obj.Name, WordLength);
  } else {
    UtilNCopy(I->SrcName, "none", WordLength);
  }
  I->SrcState = mol_state;
  return (I);
}

static void ObjectGadgetRampInvalidate(ObjectGadgetRamp * I, int rep, int level,
                                       int state)
{
}


/*========================================================================*/
ObjectGadgetRamp *ObjectGadgetRampNew(PyMOLGlobals * G)
{
  OOAlloc(G, ObjectGadgetRamp);

  ObjectGadgetInit(G, &I->Gadget);
  I->Gadget.GadgetType = cGadgetRamp;
  I->RampType = 0;
  I->NLevel = 0;
  I->Level = NULL;
  I->Color = NULL;
  I->Special = NULL;
  I->Extreme = NULL;
  I->SrcName[0] = 0;

  I->Gadget.Obj.fUpdate = (void (*)(CObject *)) ObjectGadgetRampUpdate;
  I->Gadget.Obj.fFree = (void (*)(CObject *)) ObjectGadgetRampFree;
  I->Gadget.Obj.fInvalidate =
    (void (*)(CObject *, int, int, int)) ObjectGadgetRampInvalidate;

  I->Mol = NULL;
  I->Map = NULL;
  I->width = 0.9F;
  I->height = 0.06F;
  I->bar_height = 0.03F;
  I->text_raise = 0.003F;
  I->text_border = 0.004F;
  I->text_scale_h = 0.04F;
  I->text_scale_v = 0.02F;
  I->border = 0.018F;
  I->var_index = 0;
  I->x = (1.0F - (I->width + 2 * I->border)) / 2.0F;
  I->y = 0.12F;
  I->Map = NULL;
  I->CalcMode = 0;
  return (I);
}
