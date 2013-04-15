
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
#include"RepNonbonded.h"
#include"Color.h"
#include"Scene.h"
#include"main.h"
#include"Setting.h"
#include"ShaderMgr.h"

typedef struct RepNonbonded {
  Rep R;
  float *V, *VP;
  Pickable *P;
  int N, NP;
  float Width;
  float Radius;
  CGO *shaderCGO;
} RepNonbonded;

#include"ObjectMolecule.h"

void RepNonbondedFree(RepNonbonded * I);

void RepNonbondedFree(RepNonbonded * I)
{
  if (I->shaderCGO){
    CGOFree(I->shaderCGO);
    I->shaderCGO = 0;
  }
  FreeP(I->VP);
  FreeP(I->V);
  RepPurge(&I->R);
  OOFreeP(I);
}

void RepNonbondedRenderImmediate(CoordSet * cs, RenderInfo * info)
{
  PyMOLGlobals *G = cs->State.G;
  if(info->ray || info->pick || (!(G->HaveGUI && G->ValidContext)))
    return;
  else {
    int active = false;
    ObjectMolecule *obj = cs->Obj;
    float line_width =
      SettingGet_f(G, cs->Setting, obj->Obj.Setting, cSetting_line_width);
    float nonbonded_size =
      SettingGet_f(G, cs->Setting, obj->Obj.Setting, cSetting_nonbonded_size);

    if(info->width_scale_flag)
      glLineWidth(line_width * info->width_scale);
    else
      glLineWidth(line_width);

    SceneResetNormal(G, true);

    if(!info->line_lighting)
      glDisable(GL_LIGHTING);
#ifdef _PYMOL_GL_DRAWARRAYS
    {
      int nverts = 0;
      {
	int a;
	int nIndex = cs->NIndex;
	AtomInfoType *atomInfo = obj->AtomInfo;
	int *i2a = cs->IdxToAtm;
	for(a = 0; a < nIndex; a++) {
	  AtomInfoType *ai = atomInfo + *(i2a++);
	  if((!ai->bonded) && ai->visRep[cRepNonbonded]) {
	    nverts += 6;
	  }
	}
      }
      if (nverts>0) {
	ALLOCATE_ARRAY(GLfloat,vertVals,nverts*3)
	ALLOCATE_ARRAY(GLfloat,colorVals,nverts*4)
	int pl, plc, plca = 0;
	int a;
	int nIndex = cs->NIndex;
	AtomInfoType *atomInfo = obj->AtomInfo;
	int *i2a = cs->IdxToAtm;
	int last_color = -1;
	float *v = cs->Coord, *colv;
	pl = 0;
	for(a = 0; a < nIndex; a++) {
	  AtomInfoType *ai = atomInfo + *(i2a++);
	  if((!ai->bonded) && ai->visRep[cRepNonbonded]) {
	    int c = ai->color;
	    float v0 = v[0];
	    float v1 = v[1];
	    float v2 = v[2];
	    active = true;
	    if(c != last_color) {
	      last_color = c;
	    }
	    colv = ColorGet(G, c);
	    for (plc=pl; plc < (pl+18); ){
	      colorVals[plca++] = colv[0];
	      colorVals[plca++] = colv[1];
	      colorVals[plca++] = colv[2];
	      colorVals[plca++] = 1.f;
	    }
	    vertVals[pl++] = v0 - nonbonded_size; vertVals[pl++] = v1; vertVals[pl++] = v2;
	    vertVals[pl++] = v0 + nonbonded_size; vertVals[pl++] = v1; vertVals[pl++] = v2;

	    vertVals[pl++] = v0; vertVals[pl++] = v1 - nonbonded_size; vertVals[pl++] = v2;
	    vertVals[pl++] = v0; vertVals[pl++] = v1 + nonbonded_size; vertVals[pl++] = v2;

	    vertVals[pl++] = v0; vertVals[pl++] = v1; vertVals[pl++] = v2 - nonbonded_size;
	    vertVals[pl++] = v0; vertVals[pl++] = v1; vertVals[pl++] = v2 + nonbonded_size;
	  }
	  v += 3;
	}
	glEnableClientState(GL_VERTEX_ARRAY);
	glEnableClientState(GL_COLOR_ARRAY);
	glVertexPointer(3, GL_FLOAT, 0, vertVals);
	glColorPointer(4, GL_FLOAT, 0, colorVals);
	glDrawArrays(GL_LINES, 0, nverts);
	glDisableClientState(GL_VERTEX_ARRAY);
	glDisableClientState(GL_COLOR_ARRAY);
	DEALLOCATE_ARRAY(vertVals)
	DEALLOCATE_ARRAY(colorVals)
      }
    }
#else
    glBegin(GL_LINES);
    {
      int a;
      int nIndex = cs->NIndex;
      AtomInfoType *atomInfo = obj->AtomInfo;
      int *i2a = cs->IdxToAtm;
      int last_color = -1;
      float *v = cs->Coord;

      for(a = 0; a < nIndex; a++) {
        AtomInfoType *ai = atomInfo + *(i2a++);
        if((!ai->bonded) && ai->visRep[cRepNonbonded]) {
          int c = ai->color;
          float v0 = v[0];
          float v1 = v[1];
          float v2 = v[2];
          active = true;
          if(c != last_color) {
            last_color = c;
            glColor3fv(ColorGet(G, c));
          }

          glVertex3f(v0 - nonbonded_size, v1, v2);
          glVertex3f(v0 + nonbonded_size, v1, v2);

          glVertex3f(v0, v1 - nonbonded_size, v2);
          glVertex3f(v0, v1 + nonbonded_size, v2);

          glVertex3f(v0, v1, v2 - nonbonded_size);
          glVertex3f(v0, v1, v2 + nonbonded_size);
        }
        v += 3;
      }
    }
    glEnd();
#endif
    glEnable(GL_LIGHTING);
    if(!active)
      cs->Active[cRepNonbonded] = true;
  }
}

static void RepNonbondedRender(RepNonbonded * I, RenderInfo * info)
{
  CRay *ray = info->ray;
  Picking **pick = info->pick;
  PyMOLGlobals *G = I->R.G;
  float *v = I->V;
  int c = I->N;
  unsigned int i, j;
  Pickable *p;
  float alpha;
  int ok = true;

  alpha =
    SettingGet_f(G, I->R.cs->Setting, I->R.obj->Setting, cSetting_nonbonded_transparency);
  alpha = 1.0F - alpha;
  if(fabs(alpha - 1.0) < R_SMALL4)
    alpha = 1.0F;
  if(ray) {
    float radius;
    ray->fTransparentf(ray, 1.0F - alpha);

    if(I->Radius == 0.0F) {
      radius = ray->PixelRadius * I->Width / 2.0F;
    } else {
      radius = I->Radius;
    }

    v = I->V;
    c = I->N;

    while(ok && c--) {
      /*      printf("%8.3f %8.3f %8.3f   %8.3f %8.3f %8.3f \n",v[3],v[4],v[5],v[6],v[7],v[8]); */
      ok &= ray->fSausage3fv(ray, v + 3, v + 6, radius, v, v);
      if (ok)
	ok &= ray->fSausage3fv(ray, v + 9, v + 12, radius, v, v);
      if (ok)
	ok &= ray->fSausage3fv(ray, v + 15, v + 18, radius, v, v);
      v += 21;
    }
    ray->fTransparentf(ray, 0.0);
  } else if(G->HaveGUI && G->ValidContext) {
    if(pick) {

      i = (*pick)->src.index;

      v = I->VP;
      c = I->NP;
      p = I->R.P;
      SceneSetupGLPicking(G);
#ifdef _PYMOL_GL_DRAWARRAYS
      {
	int nverts = c * 6, pl, plc = 0;
	GLubyte *tmp_ptr;
	ALLOCATE_ARRAY(GLfloat,vertVals,nverts*3)
	ALLOCATE_ARRAY(GLubyte,colorVals,nverts*4)
	pl = 0;
	while(c--) {
	  i++;
	  if(!(*pick)[0].src.bond) {
	    /* pass 1 - low order bits */
	    colorVals[plc++] = (uchar) ((i & 0xF) << 4);
	    colorVals[plc++] = (uchar) ((i & 0xF0) | 0x8);
	    colorVals[plc++] = (uchar) ((i & 0xF00) >> 4);
	    colorVals[plc++] = (uchar) 255;
	    VLACheck((*pick), Picking, i);
	    p++;
	    (*pick)[i].src = *p;  /* copy object and atom info */
	    (*pick)[i].context = I->R.context;
	  } else {
	    /* pass 2 - high order bits */
	    j = i >> 12;
	    colorVals[plc++] = (uchar) ((j & 0xF) << 4);
	    colorVals[plc++] = (uchar) ((j & 0xF0) | 0x8);
	    colorVals[plc++] = (uchar) ((j & 0xF00) >> 4);
	    colorVals[plc++] = (uchar) 255;
	  }
	  memcpy(&vertVals[pl], v, 18*sizeof(GLfloat));
	  v += 18;
	  pl += 18;
	  tmp_ptr = &colorVals[plc-4];
	  colorVals[plc++] = tmp_ptr[0]; colorVals[plc++] = tmp_ptr[1]; colorVals[plc++] = tmp_ptr[2]; colorVals[plc++] = tmp_ptr[3];
	  colorVals[plc++] = tmp_ptr[0]; colorVals[plc++] = tmp_ptr[1]; colorVals[plc++] = tmp_ptr[2]; colorVals[plc++] = tmp_ptr[3];
	  colorVals[plc++] = tmp_ptr[0]; colorVals[plc++] = tmp_ptr[1]; colorVals[plc++] = tmp_ptr[2]; colorVals[plc++] = tmp_ptr[3];
	  colorVals[plc++] = tmp_ptr[0]; colorVals[plc++] = tmp_ptr[1]; colorVals[plc++] = tmp_ptr[2]; colorVals[plc++] = tmp_ptr[3];
	  colorVals[plc++] = tmp_ptr[0]; colorVals[plc++] = tmp_ptr[1]; colorVals[plc++] = tmp_ptr[2]; colorVals[plc++] = tmp_ptr[3];
	}
	glEnableClientState(GL_VERTEX_ARRAY);
	glEnableClientState(GL_COLOR_ARRAY);
	glVertexPointer(3, GL_FLOAT, 0, vertVals);
	glColorPointer(4, GL_UNSIGNED_BYTE, 0, colorVals);
	glDrawArrays(GL_LINES, 0, nverts);
	glDisableClientState(GL_VERTEX_ARRAY);
	glDisableClientState(GL_COLOR_ARRAY);
	DEALLOCATE_ARRAY(vertVals)
	DEALLOCATE_ARRAY(colorVals)
      }
#else
      glBegin(GL_LINES);
      while(c--) {
        i++;
        if(!(*pick)[0].src.bond) {
          /* pass 1 - low order bits */
          glColor3ub((uchar) ((i & 0xF) << 4), (uchar) ((i & 0xF0) | 0x8),
                     (uchar) ((i & 0xF00) >> 4));
          VLACheck((*pick), Picking, i);
          p++;
          (*pick)[i].src = *p;  /* copy object and atom info */
          (*pick)[i].context = I->R.context;
        } else {
          /* pass 2 - high order bits */
          j = i >> 12;
          glColor3ub((uchar) ((j & 0xF) << 4), (uchar) ((j & 0xF0) | 0x8),
                     (uchar) ((j & 0xF00) >> 4));
        }

        glVertex3fv(v);
        v += 3;
        glVertex3fv(v);
        v += 3;
        glVertex3fv(v);
        v += 3;
        glVertex3fv(v);
        v += 3;
        glVertex3fv(v);
        v += 3;
        glVertex3fv(v);
        v += 3;
      }
      glEnd();
#endif
      (*pick)[0].src.index = i;
    } else {
      /* not pick, but render */
      short use_shader, generate_shader_cgo = 0, use_display_lists = 0;
      short nonbonded_as_cylinders ;
      register int nvidia_bugs = SettingGetGlobal_i(G, cSetting_nvidia_bugs);
      use_shader = SettingGetGlobal_b(G, cSetting_nonbonded_use_shader) & 
                   SettingGetGlobal_b(G, cSetting_use_shaders);
      use_display_lists = SettingGetGlobal_i(G, cSetting_use_display_lists);

      nonbonded_as_cylinders = SettingGetGlobal_b(G, cSetting_render_as_cylinders) && SettingGetGlobal_b(G, cSetting_nonbonded_as_cylinders);
      if (!use_shader && I->shaderCGO){
	CGOFree(I->shaderCGO);
	I->shaderCGO = 0;
      }

      if (I->shaderCGO && (nonbonded_as_cylinders ^ I->shaderCGO->has_draw_cylinder_buffers)){
	CGOFree(I->shaderCGO);
	I->shaderCGO = 0;
      }

#ifdef _PYMOL_GL_CALLLISTS
        if(use_display_lists && I->R.displayList) {
          glCallList(I->R.displayList);
	  return;
	}
#endif

      if (use_shader){
	if (!I->shaderCGO){
	  I->shaderCGO = CGONew(G);
	  CHECKOK(ok, I->shaderCGO);
	  if (ok){
	    I->shaderCGO->use_shader = true;
	    generate_shader_cgo = 1;
	  }
	} else {
	  CShaderPrg *shaderPrg;
	  if (nonbonded_as_cylinders){
	    float pixel_scale_value = SettingGetGlobal_f(G, cSetting_ray_pixel_scale);
	    if(pixel_scale_value < 0)
	      pixel_scale_value = 1.0F;
	    shaderPrg = CShaderPrg_Enable_CylinderShader(G);
        if (!shaderPrg) return;
	    CShaderPrg_Set1f(shaderPrg, "uni_radius", info->vertex_scale * pixel_scale_value * I->Width/ 2.f);
	  } else {
	    shaderPrg = CShaderPrg_Enable_DefaultShader(G);
        if (!shaderPrg) return;
	    CShaderPrg_SetLightingEnabled(shaderPrg, 0);
	  }

	  CGORenderGL(I->shaderCGO, NULL, NULL, NULL, info, &I->R);

	  CShaderPrg_Disable(shaderPrg);
	  return;
	}
      }
#ifdef _PYMOL_GL_CALLLISTS
      if(use_display_lists) {
	if(!I->R.displayList) {
	  I->R.displayList = glGenLists(1);
	  if(I->R.displayList) {
	    glNewList(I->R.displayList, GL_COMPILE_AND_EXECUTE);
	  }
	}
      }
#else
      (void) use_display_lists; (void) nvidia_bugs;
#endif
      v = I->V;
      c = I->N;
      if (ok && generate_shader_cgo){
	ok &= CGOLinewidthSpecial(I->shaderCGO, LINEWIDTH_DYNAMIC_WITH_SCALE);
	if(ok && !info->line_lighting)
	  ok &= CGODisable(I->shaderCGO, GL_LIGHTING);
	if (nonbonded_as_cylinders){
	  if(ok && c) {
	    float *origin, axis[3];
	    ok &= CGOResetNormal(I->shaderCGO, true);
	    while(ok && c--) {
	      if(alpha == 1.0) {
		ok &= CGOColorv(I->shaderCGO, v);
	      } else {
		ok &= CGOAlpha(I->shaderCGO, alpha);
		if (ok)
		  ok &= CGOColorv(I->shaderCGO, v);
	      }
	      if (ok){
		v += 3;
		origin = v;
		v += 3;
		axis[0] = v[0] - origin[0];
		axis[1] = v[1] - origin[1];
		axis[2] = v[2] - origin[2];
		ok &= CGOShaderCylinder(I->shaderCGO, origin, axis, 1.f, 15);
	      }
	      if (ok){
		v += 3;
		origin = v;
		v += 3;
		axis[0] = v[0] - origin[0];
		axis[1] = v[1] - origin[1];
		axis[2] = v[2] - origin[2];
		ok &= CGOShaderCylinder(I->shaderCGO, origin, axis, 1.f, 15);
	      }
	      if (ok){
		v += 3;
		origin = v;
		v += 3;
		axis[0] = v[0] - origin[0];
		axis[1] = v[1] - origin[1];
		axis[2] = v[2] - origin[2];
		ok &= CGOShaderCylinder(I->shaderCGO, origin, axis, 1.f, 15);
		v += 3;
	      }
	    }
	  }
	} else {
	  if(ok && c) {
	    ok &= CGOBegin(I->shaderCGO, GL_LINES);
	    if (ok)
	      ok &= CGOResetNormal(I->shaderCGO, true);
	    while(ok && c--) {
	      if(alpha == 1.0) {
		ok &= CGOColorv(I->shaderCGO, v);
	      } else {
		ok &= CGOAlpha(I->shaderCGO, alpha);
		if (ok)
		  ok &= CGOColorv(I->shaderCGO, v);
	      }
	      v += 3;
	      if (ok){
		ok &= CGOVertexv(I->shaderCGO, v);
		v += 3;
	      }
	      if (ok){
		ok &= CGOVertexv(I->shaderCGO, v);
		v += 3;
	      }
	      if (ok){
		ok &= CGOVertexv(I->shaderCGO, v);
		v += 3;
	      }
	      if (ok){
		ok &= CGOVertexv(I->shaderCGO, v);
		v += 3;
	      }
	      if (ok){
		ok &= CGOVertexv(I->shaderCGO, v);
		v += 3;
	      }
	      if (ok){
		ok &= CGOVertexv(I->shaderCGO, v);
		v += 3;
	      }
	    }
	    if (ok)
	      ok &= CGOEnd(I->shaderCGO);
	  }
	}
	if(ok && !info->line_lighting)
	  ok &= CGOEnable(I->shaderCGO, GL_LIGHTING);
      } else {
	if(info->width_scale_flag)
	  glLineWidth(I->Width * info->width_scale);
	else
	  glLineWidth(I->Width);
	
        if(SettingGetGlobal_i(G, cSetting_ati_bugs)) {
          glFlush();            /* eliminate ATI artifacts under VISTA */
        }
        if(c) {
          if(!info->line_lighting)
            glDisable(GL_LIGHTING);
#ifdef _PYMOL_GL_DRAWARRAYS
          SceneResetNormal(G, true);
	  {
	    int nverts = c * 6;
	    ALLOCATE_ARRAY(GLfloat,vertVals,nverts*3)
	    ALLOCATE_ARRAY(GLfloat,colorVals,nverts*4)
	    int pl, plc;
	    pl = 0; plc = 0;
	    while(c--) {
	      colorVals[plc++] = v[0]; colorVals[plc++] = v[1]; colorVals[plc++] = v[2]; colorVals[plc++] = alpha;
	      colorVals[plc++] = v[0]; colorVals[plc++] = v[1]; colorVals[plc++] = v[2]; colorVals[plc++] = alpha;
	      colorVals[plc++] = v[0]; colorVals[plc++] = v[1]; colorVals[plc++] = v[2]; colorVals[plc++] = alpha;
	      colorVals[plc++] = v[0]; colorVals[plc++] = v[1]; colorVals[plc++] = v[2]; colorVals[plc++] = alpha;
	      colorVals[plc++] = v[0]; colorVals[plc++] = v[1]; colorVals[plc++] = v[2]; colorVals[plc++] = alpha;
	      colorVals[plc++] = v[0]; colorVals[plc++] = v[1]; colorVals[plc++] = v[2]; colorVals[plc++] = alpha;
	      v += 3;
	      memcpy(&vertVals[pl], v, 18*sizeof(GLfloat));
	      v += 18;
	      pl += 18;
	    }
	    glEnableClientState(GL_VERTEX_ARRAY);
	    glEnableClientState(GL_COLOR_ARRAY);
	    glVertexPointer(3, GL_FLOAT, 0, vertVals);
	    glColorPointer(4, GL_FLOAT, 0, colorVals);
	    glDrawArrays(GL_LINES, 0, nverts);
	    glDisableClientState(GL_VERTEX_ARRAY);
	    glDisableClientState(GL_COLOR_ARRAY);
	    DEALLOCATE_ARRAY(vertVals)
	    DEALLOCATE_ARRAY(colorVals)
	  }
#else
          glBegin(GL_LINES);
          SceneResetNormal(G, true);
          while(c--) {
            if(alpha == 1.0) {
              glColor3fv(v);
            } else {
              glColor4f(v[0], v[1], v[2], alpha);
            }
            v += 3;
            if(nvidia_bugs) {
              glFlush();
            }
            glVertex3fv(v);
            v += 3;
            glVertex3fv(v);
            v += 3;
            glVertex3fv(v);
            v += 3;
            glVertex3fv(v);
            v += 3;
            glVertex3fv(v);
            v += 3;
            glVertex3fv(v);
            v += 3;
          }
          glEnd();
#endif
          glEnable(GL_LIGHTING);
        }
      }

      if (use_shader) {
	if (generate_shader_cgo){
	  CGO *convertcgo = NULL;
	  if (ok)
	    ok &= CGOStop(I->shaderCGO);
#ifdef _PYMOL_CGO_DRAWARRAYS
	  if (ok && I->shaderCGO){
	    convertcgo = CGOCombineBeginEnd(I->shaderCGO, 0);
	    CGOFree(I->shaderCGO);
	    I->shaderCGO = convertcgo;
	    convertcgo = NULL;
	  }  
#else
	  (void)convertcgo;
#endif
#ifdef _PYMOL_CGO_DRAWBUFFERS
	  if (ok){
	    if (nonbonded_as_cylinders){
	      convertcgo = CGOOptimizeGLSLCylindersToVBOIndexed(I->shaderCGO, 0);
	    } else {
	      convertcgo = CGOOptimizeToVBONotIndexed(I->shaderCGO, 0);
	    }
	    CHECKOK(ok, convertcgo);
	  }
	  if (!ok || convertcgo){
	    CGOFree(I->shaderCGO);
	    I->shaderCGO = convertcgo;
	    convertcgo = NULL;
	  }
#else
	  (void)convertcgo;
#endif
	}
	
	if (ok){
	  CShaderPrg *shaderPrg;
	  if (nonbonded_as_cylinders){
	    float pixel_scale_value = SettingGetGlobal_f(G, cSetting_ray_pixel_scale);
	    if(pixel_scale_value < 0)
	      pixel_scale_value = 1.0F;
	    shaderPrg = CShaderPrg_Enable_CylinderShader(G);
	    if (!shaderPrg) return;
	    CShaderPrg_Set1f(shaderPrg, "uni_radius", info->vertex_scale * pixel_scale_value * I->Width/ 2.f);
	  } else {
	    shaderPrg = CShaderPrg_Enable_DefaultShader(G);
	    if (!shaderPrg) return;
	    CShaderPrg_SetLightingEnabled(shaderPrg, 0);
	  }	 
	  CGORenderGL(I->shaderCGO, NULL, NULL, NULL, info, &I->R);
	  CShaderPrg_Disable(shaderPrg);
	} else {
	  /* not OK, then destroy RepNonbonded object */
	  I->R.fInvalidate(&I->R, I->R.cs, cRepInvPurge);
	  I->R.cs->Active[cRepNonbonded] = false;
	}
      }
#ifdef _PYMOL_GL_CALLLISTS
      if (use_display_lists && I->R.displayList){
	glEndList();
	glCallList(I->R.displayList);      
      }
#endif
    }
  }
}

Rep *RepNonbondedNew(CoordSet * cs, int state)
{
  PyMOLGlobals *G = cs->State.G;
  ObjectMolecule *obj;
  int a, a1, c1;
  float nonbonded_size;
  float *v, *v0, *v1;
  int *active;
  AtomInfoType *ai;
  int nAtom = 0;
  float tmpColor[3];
  OOAlloc(G, RepNonbonded);
  obj = cs->Obj;

  active = Alloc(int, cs->NIndex);
  if(obj->RepVisCache[cRepNonbonded])
    for(a = 0; a < cs->NIndex; a++) {
      ai = obj->AtomInfo + cs->IdxToAtm[a];
      active[a] = (!ai->bonded) && (ai->visRep[cRepNonbonded]); /*&& (!ai->masked); */
      if(active[a]) {
        if(ai->masked)
          active[a] = -1;
        else
          active[a] = 1;
      }
      if(active[a])
        nAtom++;
    }
  if(!nAtom) {
    OOFreeP(I);
    FreeP(active);
    return (NULL);              /* skip if no dots are visible */
  }

  nonbonded_size =
    SettingGet_f(G, cs->Setting, obj->Obj.Setting, cSetting_nonbonded_size);
  RepInit(G, &I->R);

  I->R.fRender = (void (*)(struct Rep *, RenderInfo *)) RepNonbondedRender;
  I->R.fFree = (void (*)(struct Rep *)) RepNonbondedFree;

  I->shaderCGO = NULL;
  I->N = 0;
  I->NP = 0;
  I->V = NULL;
  I->VP = NULL;
  I->R.P = NULL;
  I->R.fRecolor = NULL;
  I->R.obj = (CObject *) (cs->Obj);
  I->R.cs = cs;

  I->Width = SettingGet_f(G, cs->Setting, obj->Obj.Setting, cSetting_line_width);
  I->Radius = SettingGet_f(G, cs->Setting, obj->Obj.Setting, cSetting_line_radius);
  I->V = (float *) mmalloc(sizeof(float) * nAtom * 21);
  ErrChkPtr(G, I->V);
  v = I->V;
  for(a = 0; a < cs->NIndex; a++)
    if(active[a]) {
      c1 = *(cs->Color + a);

      v1 = cs->Coord + 3 * a;

      if(ColorCheckRamped(G, c1)) {
        ColorGetRamped(G, c1, v1, tmpColor, state);
        v0 = tmpColor;
      } else {
        v0 = ColorGet(G, c1);
      }

      *(v++) = *(v0++);
      *(v++) = *(v0++);
      *(v++) = *(v0++);

      *(v++) = v1[0] - nonbonded_size;
      *(v++) = v1[1];
      *(v++) = v1[2];

      *(v++) = v1[0] + nonbonded_size;
      *(v++) = v1[1];
      *(v++) = v1[2];

      *(v++) = v1[0];
      *(v++) = v1[1] - nonbonded_size;
      *(v++) = v1[2];

      *(v++) = v1[0];
      *(v++) = v1[1] + nonbonded_size;
      *(v++) = v1[2];

      *(v++) = v1[0];
      *(v++) = v1[1];
      *(v++) = v1[2] - nonbonded_size;

      *(v++) = v1[0];
      *(v++) = v1[1];
      *(v++) = v1[2] + nonbonded_size;
      I->N++;
    }
  I->V = ReallocForSure(I->V, float, (v - I->V));

  /* now create pickable verson */

  if(SettingGet_f(G, cs->Setting, obj->Obj.Setting, cSetting_pickable)) {
    I->VP = (float *) mmalloc(sizeof(float) * nAtom * 21);
    ErrChkPtr(G, I->VP);

    I->R.P = Alloc(Pickable, cs->NIndex + 1);
    ErrChkPtr(G, I->R.P);

    v = I->VP;

    for(a = 0; a < cs->NIndex; a++)
      if(active[a] > 0) {

        a1 = cs->IdxToAtm[a];

        if(!obj->AtomInfo[a1].masked) {
          I->NP++;
          I->R.P[I->NP].index = a1;
          I->R.P[I->NP].bond = -1;
          v1 = cs->Coord + 3 * a;

          *(v++) = v1[0] - nonbonded_size;
          *(v++) = v1[1];
          *(v++) = v1[2];

          *(v++) = v1[0] + nonbonded_size;
          *(v++) = v1[1];
          *(v++) = v1[2];

          *(v++) = v1[0];
          *(v++) = v1[1] - nonbonded_size;
          *(v++) = v1[2];

          *(v++) = v1[0];
          *(v++) = v1[1] + nonbonded_size;
          *(v++) = v1[2];

          *(v++) = v1[0];
          *(v++) = v1[1];
          *(v++) = v1[2] - nonbonded_size;

          *(v++) = v1[0];
          *(v++) = v1[1];
          *(v++) = v1[2] + nonbonded_size;
        }
      }
    I->R.P = Realloc(I->R.P, Pickable, I->NP + 1);
    I->R.context.object = (void *) obj;
    I->R.context.state = state;
    I->R.P[0].index = I->NP;
    I->VP = ReallocForSure(I->VP, float, (v - I->VP));
  }
  FreeP(active);
  return ((void *) (struct Rep *) I);
}
