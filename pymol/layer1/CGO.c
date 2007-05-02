/* 
A* -------------------------------------------------------------------
B* This file contains source code for the PyMOL computer program
C* copyright 1998-2000 by Warren Lyford Delano of DeLano Scientific. 
D* -------------------------------------------------------------------
E* It is unlawful to modify or remove this copyright notice.
F* -------------------------------------------------------------------
G* Please see the accompanying LICENSE file for further information. 
H* --------------------------------------------------\-----------------
I* Additional authors of this source file include:
-* 
-* 
-*
Z* -------------------------------------------------------------------
*/

#include"os_predef.h"
#include"os_std.h"
#include"os_gl.h"

#include"Feedback.h"
#include"CGO.h"
#include"Base.h"
#include"OOMac.h"
#include"Setting.h"
#include"Sphere.h"
#include"PConv.h"
#include"GadgetSet.h"
#include"VFont.h"
#include"P.h"
#include"PyMOLGlobals.h"
#include"Ray.h"
#include"Util.h"
#include"Scene.h"

#define CGO_read_int(p) (*((int*)(p++)))
#define CGO_get_int(p) (*((int*)(p)))
#define CGO_write_int(p,i) ((*((int*)(p++)))=(i))

struct _CCGORenderer {
  PyMOLGlobals *G;
  float alpha;
};

int CGORendererInit(PyMOLGlobals *G)
{
  register CCGORenderer *I= NULL;
  
  I = (G->CGORenderer = Calloc(CCGORenderer,1));
  if(I) {
    I->G = G;
    I->alpha = 1.0F;
    return 1;
  } else 
    return 0;
}

void CGORendererFree(PyMOLGlobals *G)
{
  FreeP(G->CGORenderer);
}

int CGO_sz[] = {
  CGO_NULL_SZ,
  CGO_NULL_SZ,
  CGO_BEGIN_SZ,
  CGO_END_SZ,

  CGO_VERTEX_SZ,
  CGO_NORMAL_SZ,
  CGO_COLOR_SZ,
  CGO_SPHERE_SZ,

  CGO_TRIANGLE_SZ,
  CGO_CYLINDER_SZ,
  CGO_LINEWIDTH_SZ,
  CGO_WIDTHSCALE_SZ,

  CGO_ENABLE_SZ,
  CGO_DISABLE_SZ,
  CGO_SAUSAGE_SZ,
  CGO_CUSTOM_CYLINDER_SZ,

  CGO_DOTWIDTH_SZ,
  CGO_NULL_SZ,
  CGO_NULL_SZ,
  CGO_FONT_SZ,

  CGO_FONT_SCALE_SZ,
  CGO_FONT_VERTEX_SZ,
  CGO_FONT_AXES_SZ,
  CGO_CHAR_SZ,

  CGO_INDENT_SZ,
  CGO_ALPHA_SZ,
  CGO_NULL_SZ,
  CGO_NULL_SZ,

  CGO_NULL_SZ,
  CGO_NULL_SZ,
  CGO_RESET_NORMAL_SZ,
  CGO_PICK_COLOR_SZ,


};

typedef void CGO_op(CCGORenderer *I,float *);
typedef CGO_op *CGO_op_fn;

static float *CGO_add(CGO *I,int c);
static float *CGO_size(CGO *I,int sz);
static void subdivide( int n, float *x, float *y);
void CGOSimpleCylinder(CGO *I,float *v1,float *v2,float tube_size,float *c1,float *c2,int cap1,int cap2);
void CGOSimpleSphere(CGO *I,float *v,float vdw);

CGO *CGOProcessShape(CGO *I,struct GadgetSet *gs,CGO *result)
{
  register float *p,*pc = I->op;
  register float *n,*nc;
  register int op;
  int sz;

  if(!result)
    result = CGONew(I->G);
  CGOReset(result);
  VLACheck(result->op,float,I->c+32);
  
  while((op=(CGO_MASK&CGO_read_int(pc)))) {
    sz=CGO_sz[op];
    nc=CGO_add(result,sz+1);
    *(nc++)=*(pc-1);
    switch(op) {
    case CGO_NORMAL:
      GadgetSetFetchNormal(gs,pc,nc);
      break;
    case CGO_COLOR:
      GadgetSetFetchColor(gs,pc,nc);
      break;
    case CGO_VERTEX:
      GadgetSetFetch(gs,pc,nc);
      break;
    case CGO_FONT_VERTEX:
      GadgetSetFetch(gs,pc,nc);
      break;
    case CGO_SPHERE:
      GadgetSetFetch(gs,pc,nc);
      *(nc+3)=*(pc+3);
      break;
    case CGO_CUSTOM_CYLINDER:
      GadgetSetFetch(gs,pc,nc);
      GadgetSetFetch(gs,pc+3,nc+3);
      GadgetSetFetchColor(gs,pc+7,nc+7);
      GadgetSetFetchColor(gs,pc+10,nc+10);
      *(nc+6)=*(pc+6);
      *(nc+13)=*(pc+13);
      *(nc+14)=*(pc+14);
      break;
    case CGO_CYLINDER:
      GadgetSetFetch(gs,pc,nc);
      GadgetSetFetch(gs,pc+3,nc+3);
      GadgetSetFetchColor(gs,pc+7,nc+7);
      GadgetSetFetchColor(gs,pc+10,nc+10);
      *(nc+6)=*(pc+6);
      break;
    case CGO_SAUSAGE:
      GadgetSetFetch(gs,pc,nc);
      GadgetSetFetch(gs,pc+3,nc+3);
      GadgetSetFetchColor(gs,pc+7,nc+7);
      GadgetSetFetchColor(gs,pc+10,nc+10);
      *(nc+6)=*(pc+6);
      break;
    case CGO_TRIANGLE:
      GadgetSetFetch(gs,pc,nc);
      GadgetSetFetch(gs,pc+3,nc+3);
      GadgetSetFetch(gs,pc+6,nc+6);
      GadgetSetFetchNormal(gs,pc+9,nc+9);
      GadgetSetFetchNormal(gs,pc+12,nc+12);
      GadgetSetFetchNormal(gs,pc+15,nc+15);
      GadgetSetFetchColor(gs,pc+18,nc+18);
      GadgetSetFetchColor(gs,pc+21,nc+21);
      GadgetSetFetchColor(gs,pc+24,nc+24);
      break;
    default:
      p = pc;
      n = nc;
      while(sz--)
        *(n++)=*(p++);
      break;
    }
    pc+=CGO_sz[op];
    nc+=CGO_sz[op];
  }
  CGOStop(result);
  return(result);
}

#ifndef _PYMOL_NOPY

static PyObject *CGOArrayAsPyList(CGO *I)
{

  register float *pc = I->op;
  register int op;
  int i;
  int cc;
  PyObject *result = NULL;

  result = PyList_New(I->c);
  
  i=0;
  if(I->c) {
    while((op=(CGO_MASK&CGO_read_int(pc)))) {
      PyList_SetItem(result,i++,PyFloat_FromDouble((float)op));
      cc = CGO_sz[op];
      switch(op) { /* now convert any instructions with int arguments */
      case CGO_BEGIN:
      case CGO_ENABLE:
      case CGO_DISABLE:
        PyList_SetItem(result,i++,PyFloat_FromDouble((float)CGO_read_int(pc))); 
        cc--;
        break;
      }
      if(cc>0) 
        while(cc--) {
          PyList_SetItem(result,i++,PyFloat_FromDouble(*(pc++)));
        }
    }
  }
  while(i<I->c) {
    PyList_SetItem(result,i++,PyFloat_FromDouble((float)CGO_STOP));
  }
  return(result);
}
#endif

#ifndef _PYMOL_NOPY

static int CGOArrayFromPyListInPlace(PyObject *list,CGO *I)
{
  int a;
  int c=I->c;
  int cc=0;
  int ok=true;
  float *pc;
  int sz,op;
  int l;
  if(!list) { 
    ok=false;
  } else if(!PyList_Check(list)) {
    ok=false;
  } else {
    l=PyList_Size(list);
    if (l!=I->c) 
      ok=false;
  }
  if(ok) {
    pc = I->op;
    
    while(c>0) {
      op = (int)PyFloat_AsDouble(PyList_GetItem(list,cc++));
      op = op&CGO_MASK;
      c--;
      sz = CGO_sz[op];
      CGO_write_int(pc,op);
      ok=true;
      
      switch(op) { /* now convert any instructions with int arguments */
      case CGO_BEGIN:
      case CGO_ENABLE:
      case CGO_DISABLE:
        CGO_write_int(pc,(int)PyFloat_AsDouble(PyList_GetItem(list,cc++)));
        c--;
        sz--;
        break;
      }
      
      for(a=0;a<sz;a++) {
        *(pc++)=(float)PyFloat_AsDouble(PyList_GetItem(list,cc++));
        c--;
      }
    }
  }
  return(ok);
}
#endif

PyObject *CGOAsPyList(CGO *I)
{
#ifdef _PYMOL_NOPY
  return NULL;
#else
  PyObject *result;
  result = PyList_New(2);
  PyList_SetItem(result,0,PyInt_FromLong(I->c));
  PyList_SetItem(result,1,CGOArrayAsPyList(I));
  return(result);
#endif
}

CGO *CGONewFromPyList(PyMOLGlobals *G,PyObject *list,int version)
{
#ifdef _PYMOL_NOPY
  return NULL;
#else
  int ok=true;
  int ll;
  OOAlloc(G,CGO);
  I->G=G;
  I->op=NULL;
  if(ok) ok=(list!=NULL);
  if(ok) ok=PyList_Check(list);
  if(ok) ll = PyList_Size(list);
  /* TO ENABLE BACKWARDS COMPATIBILITY...
   Always check ll when adding new PyList_GetItem's */
  if(ok) ok=PConvPyIntToInt(PyList_GetItem(list,0),&I->c);
  if(ok) ok=((I->op=VLAlloc(float,I->c+1))!=NULL);
  if((version>0)&&(version<=86)) {
    if(ok) ok=PConvPyListToFloatArrayInPlace(PyList_GetItem(list,1),I->op,I->c);    
  } else {
    if(ok) ok=CGOArrayFromPyListInPlace(PyList_GetItem(list,1),I);
  }
  
  if(!ok) {
    CGOFree(I);
    I=NULL;
  }
  return(I);
#endif
}

CGO *CGONew(PyMOLGlobals *G)
{
  OOAlloc(G,CGO);
  I->G=G;
  I->op=VLAlloc(float,33);
  I->c=0;
  return(I);
}

CGO *CGONewSized(PyMOLGlobals *G,int size)
{
  OOAlloc(G,CGO);
  I->G=G;
  I->op=VLAlloc(float,size+32);
  I->c=0;
  return(I);
}

void CGOReset(CGO *I) 
{
  I->c=0;
}

void CGOFree(CGO *I)
{
  if(I) {
    VLAFreeP(I->op);
  }
  OOFreeP(I);
}

static float *CGO_add(CGO *I,int c)
{
  float *at;
  VLACheck(I->op,float,I->c+c);
  at = I->op+I->c;
  I->c+=c;
  return(at);
}

static float *CGO_size(CGO *I,int sz)
{
  float *at;
  VLASize(I->op,float,sz);
  at=I->op+I->c;
  I->c=sz;
  return(at);
}

/*===== Object Creation Routines =======*/

int CGOFromFloatArray(CGO *I,float *src,int len)
{
  int op,iarg;
  int c;
  int ok;
  int all_ok=true;
  int bad_entry=0;
  int sz;
  int a;
  int cc=0;
  float val;
  float *pc,*save_pc,*tf;
  VLACheck(I->op,float,I->c+len+32);
  save_pc=I->op+I->c;
  while(len-->0) {
    cc++;
    c=1;
    op = CGO_MASK&((int)(*(src++)));
    sz = CGO_sz[op];
    if(len<sz) 
      break; /* discard short instruction */
    len-=sz;
    pc=save_pc;
    CGO_write_int(pc,op);
    ok=true;
    for(a=0;a<sz;a++) {
      cc++;
      val=*(src++);
      if((FLT_MAX-val)>0.0F) { /* make sure we have a real float */
        *(pc++)=val;
      } else {
        *(pc++)=0.0;
        ok=false;
      }
    }
    if(ok) {
      switch(op) { /* now convert any instructions with int arguments */
      case CGO_BEGIN:
      case CGO_ENABLE:
      case CGO_DISABLE:
        tf=save_pc+1;
        iarg = (int)*(tf);
        CGO_write_int(tf,iarg);
        break;
      }
      save_pc=pc;
      I->c+=sz+1;
    } else {  /* discard illegal instructions */
      if(all_ok) 
        bad_entry = cc;
      all_ok=false;
    }
  }
  return(bad_entry);
}


void CGOBegin(CGO *I,int mode)
{
  float *pc = CGO_add(I,2);
  CGO_write_int(pc,CGO_BEGIN);
  CGO_write_int(pc,mode);
}

void CGOEnable(CGO *I,int mode)
{
  float *pc = CGO_add(I,2);
  CGO_write_int(pc,CGO_ENABLE);
  CGO_write_int(pc,mode);
}

void CGODisable(CGO *I,int mode)
{
  float *pc = CGO_add(I,2);
  CGO_write_int(pc,CGO_DISABLE);
  CGO_write_int(pc,mode);
}

void CGOLinewidth(CGO *I,float v)
{
  float *pc = CGO_add(I,2);
  CGO_write_int(pc,CGO_LINEWIDTH);
  *(pc++)=v;
}

void CGODotwidth(CGO *I,float v)
{
  float *pc = CGO_add(I,2);
  CGO_write_int(pc,CGO_DOTWIDTH);
  *(pc++)=v;
}

void CGOCylinderv(CGO *I,float *p1,float *p2,float r,float *c1,float *c2)
{
  float *pc = CGO_add(I,14);
  CGO_write_int(pc,CGO_CYLINDER);
  *(pc++)=*(p1++);
  *(pc++)=*(p1++);
  *(pc++)=*(p1++);
  *(pc++)=*(p2++);
  *(pc++)=*(p2++);
  *(pc++)=*(p2++);
  *(pc++)=r;
  *(pc++)=*(c1++);
  *(pc++)=*(c1++);
  *(pc++)=*(c1++);
  *(pc++)=*(c2++);
  *(pc++)=*(c2++);
  *(pc++)=*(c2++);
}


void CGOCustomCylinderv(CGO *I,float *p1,float *p2,float r,float *c1,float *c2,
                      float cap1,float cap2)
{
  float *pc = CGO_add(I,16);
  CGO_write_int(pc,CGO_CUSTOM_CYLINDER);
  *(pc++)=*(p1++);
  *(pc++)=*(p1++);
  *(pc++)=*(p1++);
  *(pc++)=*(p2++);
  *(pc++)=*(p2++);
  *(pc++)=*(p2++);
  *(pc++)=r;
  *(pc++)=*(c1++);
  *(pc++)=*(c1++);
  *(pc++)=*(c1++);
  *(pc++)=*(c2++);
  *(pc++)=*(c2++);
  *(pc++)=*(c2++);
  *(pc++)=cap1;
  *(pc++)=cap2;
}

void CGOPickColor(CGO *I,int index,int bond)
{
  float *pc = CGO_add(I,3);
  CGO_write_int(pc,CGO_PICK_COLOR);
  *(pc++)=(float)index;
  *(pc++)=(float)bond;
}

void CGOAlpha(CGO *I,float alpha)
{
  float *pc = CGO_add(I,2);
  CGO_write_int(pc,CGO_ALPHA);
  *(pc++)=alpha;
}

void CGOSphere(CGO *I,float *v1, float r)
{
  float *pc = CGO_add(I,5);
  CGO_write_int(pc,CGO_SPHERE);
  *(pc++) = *(v1++);
  *(pc++) = *(v1++);
  *(pc++) = *(v1++);
  *(pc++) = r;
}

void CGOSausage(CGO *I,float *v1,float *v2,float r,float *c1,float *c2)
{
  float *pc = CGO_add(I,14);
  CGO_write_int(pc,CGO_SAUSAGE);
  *(pc++) = *(v1++);
  *(pc++) = *(v1++);
  *(pc++) = *(v1++);
  *(pc++) = *(v2++);
  *(pc++) = *(v2++);
  *(pc++) = *(v2++);
  *(pc++) = r;
  *(pc++) = *(c1++);
  *(pc++) = *(c1++);
  *(pc++) = *(c1++);
  *(pc++) = *(c2++);
  *(pc++) = *(c2++);
  *(pc++) = *(c2++);
}

void CGOVertex(CGO *I,float v1,float v2,float v3)
{
  float *pc = CGO_add(I,4);
  CGO_write_int(pc,CGO_VERTEX);
  *(pc++)=v1;
  *(pc++)=v2;
  *(pc++)=v3;
}

void CGOVertexv(CGO *I,float *v)
{
  float *pc = CGO_add(I,4);
  CGO_write_int(pc,CGO_VERTEX);
  *(pc++)=*(v++);
  *(pc++)=*(v++);
  *(pc++)=*(v++);
}

void CGOColor(CGO *I,float v1,float v2,float v3)
{
  float *pc = CGO_add(I,4);
  CGO_write_int(pc,CGO_COLOR);
  *(pc++)=v1;
  *(pc++)=v2;
  *(pc++)=v3;
}

void CGOColorv(CGO *I,float *v)
{
  float *pc = CGO_add(I,4);
  CGO_write_int(pc,CGO_COLOR);
  *(pc++)=*(v++);
  *(pc++)=*(v++);
  *(pc++)=*(v++);
}

void CGONormal(CGO *I,float v1,float v2,float v3)
{
  float *pc = CGO_add(I,4);
  CGO_write_int(pc,CGO_NORMAL);
  *(pc++)=v1;
  *(pc++)=v2;
  *(pc++)=v3;
}

void CGOResetNormal(CGO *I,int mode)
{
  float *pc = CGO_add(I,2);
  CGO_write_int(pc,CGO_RESET_NORMAL);
  CGO_write_int(pc,mode);
}

void CGOFontVertexv(CGO *I,float *v)
{
  float *pc = CGO_add(I,4);
  CGO_write_int(pc,CGO_FONT_VERTEX);
  *(pc++)=*(v++);
  *(pc++)=*(v++);
  *(pc++)=*(v++);
}

void CGOFontVertex(CGO *I,float x,float y,float z)
{
  float *pc = CGO_add(I,4);
  CGO_write_int(pc,CGO_FONT_VERTEX);
  *(pc++)=x;
  *(pc++)=y;
  *(pc++)=z;
}

void CGOFontScale(CGO *I,float v1,float v2)
{
  float *pc = CGO_add(I,3);
  CGO_write_int(pc,CGO_FONT_SCALE);
  *(pc++)=v1;
  *(pc++)=v2;
}

void CGOChar(CGO *I,char c)
{
  float *pc = CGO_add(I,2);
  CGO_write_int(pc,CGO_CHAR);
  *(pc++)=(float)c;
}

void CGOIndent(CGO *I,char c,float dir)
{
  float *pc = CGO_add(I,3);
  CGO_write_int(pc,CGO_INDENT);
  *(pc++)=(float)c;
  *(pc++)=dir;
}

void CGOWrite(CGO *I,char *str)
{
  float *pc;

  while(*str) {
    pc = CGO_add(I,2);
    CGO_write_int(pc,CGO_CHAR);
    *(pc++)=(float)*(str++);
  }
}

void CGOWriteLeft(CGO *I,char *str)
{
  float *pc;
  char *s;
  s = str;
  while(*s) {
    pc = CGO_add(I,3);
    CGO_write_int(pc,CGO_INDENT);
    *(pc++)=(float)*(s++);
    *(pc++)=-1.0F;
  }
  s = str;
  while(*s) {
    pc = CGO_add(I,2);
    CGO_write_int(pc,CGO_CHAR);
    *(pc++)=(float)*(s++);
  }
}

void CGOWriteIndent(CGO *I,char *str,float indent)
{
  float *pc;
  char *s;
  s = str;
  while(*s) {
    pc = CGO_add(I,3);
    CGO_write_int(pc,CGO_INDENT);
    *(pc++)=(float)*(s++);
    *(pc++)=indent;
  }
  s = str;
  while(*s) {
    pc = CGO_add(I,2);
    CGO_write_int(pc,CGO_CHAR);
    *(pc++)=(float)*(s++);
  }
}


void CGONormalv(CGO *I,float *v)
{
  float *pc = CGO_add(I,4);
  CGO_write_int(pc,CGO_NORMAL);
  *(pc++)=*(v++);
  *(pc++)=*(v++);
  *(pc++)=*(v++);
}

void CGOEnd(CGO *I)
{
  float *pc = CGO_add(I,1);
  CGO_write_int(pc,CGO_END);
}

void CGOStop(CGO *I)
{
  /* add enough zeros to prevent overrun in the event of corruption
   * (include more zeros than the longest instruction in the compiler 

   * although this is wasteful, it does prevent crashes...
   */

  #define CGO_STOP_ZEROS 16

  float *pc = CGO_size(I,I->c+CGO_STOP_ZEROS); 

  UtilZeroMem(pc,sizeof(float)*CGO_STOP_ZEROS);
}

int CGOCheckComplex(CGO *I)
{
  register float *pc = I->op;
  int fc=0;
  int nEdge;
  int op;
  SphereRec *sp;
  
  sp = I->G->Sphere->Sphere[1];

  nEdge= (int) SettingGet(I->G,cSetting_stick_quality);

  while((op=(CGO_MASK&CGO_read_int(pc)))) {
    switch(op) {
    case CGO_CYLINDER:
    case CGO_SAUSAGE:
    case CGO_CUSTOM_CYLINDER:
      fc+=3*(3+(nEdge+1)*9)+9;
      break;
    case CGO_SPHERE:
      fc+=(sp->NVertTot*6)+(sp->NStrip*3)+3;
      break;
    }
    pc+=CGO_sz[op];
  }
  return(fc);
}

int CGOPreloadFonts(CGO *I)
{
  int ok=true;
  register float *pc = I->op;
  int op;
  int font_seen = false;
  int font_id;
  int blocked = false;

  blocked = PAutoBlock(I->G);
  while((op=(CGO_MASK&CGO_read_int(pc)))) {
    switch(op) {
    case CGO_FONT:
      ok = ok && (VFontLoad(I->G,1.0,1,1,true));
      font_seen = true;
      break;
    case CGO_CHAR:
      if(!font_seen) {
        font_id = VFontLoad(I->G,1.0,1,1,true);
        ok = ok && font_id;
        font_seen=true;
      }
      break;
    }
    pc+=CGO_sz[op];
  }
  if(blocked) PUnblock(I->G);

  return(ok);
}

int CGOCheckForText(CGO *I)
{
  register float *pc = I->op;
  int fc=0;
  int op;
  
  while((op=(CGO_MASK&CGO_read_int(pc)))) {
    switch(op) {
    case CGO_FONT:
    case CGO_FONT_AXES:
    case CGO_FONT_SCALE:
      fc++;
      break;
    case CGO_INDENT:
    case CGO_FONT_VERTEX:
      fc++;
      break;
    case CGO_CHAR:
      fc+=3+2*3*10; /* est 10 lines per char */
      break;
    }
    pc+=CGO_sz[op];
  }
  PRINTFD(I->G,FB_CGO)
   " CGOCheckForText-Debug: %d\n",fc
    ENDFD;

  return(fc);
}


CGO *CGODrawText(CGO *I,int est,float *camera)
{ /* assumes blocked intepreter */
  CGO *cgo;

  register float *pc = I->op;
  register float *nc;
  register int op;
  float *save_pc;
  int sz;
  int font_id = 0;
  char text[2] = " ";
  float pos[] = {0.0F,0.0F,0.0F};
  float axes[] = {1.0F,0.0F,0.0F,
                0.0F,1.0F,0.0F,
                0.0F,0.0F,1.0F};
  float scale[2] = { 1.0,1.0};

  cgo=CGONewSized(I->G,I->c+est);

  while((op=(CGO_MASK&CGO_read_int(pc)))) {
    save_pc=pc;
    switch(op) {
    case CGO_FONT:
      break;
    case CGO_FONT_AXES:
      break;
    case CGO_FONT_SCALE:
      scale[0] = pc[0];
      scale[1] = pc[1];
      break;
    case CGO_FONT_VERTEX:
      copy3f(pc,pos);
      break;
    case CGO_INDENT:
      text[0]=(unsigned char)*pc;
      VFontIndent(I->G,font_id,text,pos,scale,axes,pc[1]);
      break;
    case CGO_CHAR:
      if(!font_id) {
        font_id = VFontLoad(I->G,1.0,1,1,false);
      }
      text[0]=(unsigned char)*pc;
      VFontWriteToCGO(I->G,font_id,cgo,text,pos,scale,axes);
      break;
    default:
      sz=CGO_sz[op];
      nc=CGO_add(cgo,sz+1);
      *(nc++)=*(pc-1);
      while(sz--)
        *(nc++)=*(pc++);
    }
    pc=save_pc;
    pc+=CGO_sz[op];
  }
  CGOStop(cgo);
  return(cgo);
}

CGO *CGOSimplify(CGO *I,int est)
{
  CGO *cgo;

  register float *pc = I->op;
  register float *nc;
  register int op;
  float *save_pc;
  int sz;
  
  cgo=CGONewSized(I->G,I->c+est);

  while((op=(CGO_MASK&CGO_read_int(pc)))) {
    save_pc=pc;
    switch(op) {
    case CGO_CYLINDER:
      CGOSimpleCylinder(cgo,pc,pc+3,*(pc+6),pc+7,pc+10,1,1);
      break;
    case CGO_SAUSAGE:
      CGOSimpleCylinder(cgo,pc,pc+3,*(pc+6),pc+7,pc+10,2,2);
      break;
    case CGO_CUSTOM_CYLINDER:
      CGOSimpleCylinder(cgo,pc,pc+3,*(pc+6),pc+7,pc+10,(int)*(pc+13),(int)*(pc+14));
      break;
    case CGO_SPHERE:
      CGOSimpleSphere(cgo,pc,*(pc+3));
      break;
    default:
      sz=CGO_sz[op];
      nc=CGO_add(cgo,sz+1);
      *(nc++)=*(pc-1);
      while(sz--)
        *(nc++)=*(pc++);
    }
    pc=save_pc;
    pc+=CGO_sz[op];
  }
  CGOStop(cgo);
  return(cgo);
}

/* ======== Raytrace Renderer ======== */


int CGOGetExtent(CGO *I,float *mn,float *mx)
{
  register float *pc = I->op;
  register int op;
  int result = false;

  #define check_extent(v,r) {\
    if(!result) {\
      mn[0]=((*(v  ))-r); \
      mx[0]=((*(v  ))+r);  \
      mn[1]=((*(v+1))-r); \
      mx[1]=((*(v+1))+r); \
      mn[2]=((*(v+2))-r); \
      mx[2]=((*(v+2))+r); \
      result=true; \
  } else {\
       if(mn[0]>((*(v    ))-r)) mn[0]=((*(v    ))-r); \
       if(mx[0]<((*(v    ))+r)) mx[0]=((*(v    ))+r); \
       if(mn[1]>((*((v)+1))-r)) mn[1]=((*((v)+1))-r); \
       if(mx[1]<((*((v)+1))+r)) mx[1]=((*((v)+1))+r); \
       if(mn[2]>((*((v)+2))-r)) mn[2]=((*((v)+2))-r); \
       if(mx[2]<((*((v)+2))+r)) mx[2]=((*((v)+2))+r); }}

  while((op=(CGO_MASK&CGO_read_int(pc)))) {
    switch(op) {
    case CGO_VERTEX:
      check_extent(pc,0);
      break;
    case CGO_SPHERE:
      check_extent(pc,*(pc+3));
      break;
    case CGO_CYLINDER:
    case CGO_SAUSAGE:
    case CGO_CUSTOM_CYLINDER:
      check_extent(pc  ,*(pc+6));
      check_extent(pc+3,*(pc+6));
      break;
    case CGO_TRIANGLE:
      check_extent(pc  ,0);
      check_extent(pc+3,0);
      check_extent(pc+6,0);
      break;
    }
    pc+=CGO_sz[op];
  }
  return(result);
}

/* ======== Raytrace Renderer ======== */

void CGORenderRay(CGO *I,CRay *ray,float *color,CSetting *set1,CSetting *set2) 
{
  register float *pc = I->op;
  register int op;
  int vc=0;
  float linewidth=1.0F;
  float widthscale=0.15F;
  float lineradius,dotradius,dotwidth;
  float white[] = {1.0,1.0,1.0};
  float zee[] = {0.0,0.0,1.0};

  float *n0=NULL,*n1=NULL,*n2=NULL,*v0=NULL,*v1=NULL,*v2=NULL,*c0=NULL,*c1=NULL,*c2=NULL;
  int mode = -1;

  I->G->CGORenderer->alpha = 1.0F - SettingGet_f(I->G,set1,set2,cSetting_cgo_transparency);

  widthscale = SettingGet_f(I->G,set1,set2,cSetting_cgo_ray_width_scale);

  /*  printf("debug %8.9f\n",SceneGetScreenVertexScale(I->G,zee));*/
  linewidth = SettingGet_f(I->G,set1,set2,cSetting_cgo_line_width);
  if(linewidth<0.0F) linewidth = 1.0F;
  lineradius = SettingGet_f(I->G,set1,set2,cSetting_cgo_line_radius);
  dotwidth = SettingGet_f(I->G,set1,set2,cSetting_cgo_dot_width);
  dotradius = SettingGet_f(I->G,set1,set2,cSetting_cgo_dot_radius);
  if(lineradius<0.0F) 
    lineradius = linewidth * ray->PixelRadius/2.0F;
  if(dotradius<0.0F) 
    dotradius = dotwidth * ray->PixelRadius/2.0F;
  if(widthscale<0.0F)
    widthscale = ray->PixelRadius/2.0F;

  if(color)
    c0=color;
  else
    c0=white;
  ray->fTransparentf(ray,1.0F - I->G->CGORenderer->alpha);

  while((op=(CGO_MASK&CGO_read_int(pc)))) {
    switch(op) {
    case CGO_BEGIN:
      mode=CGO_get_int(pc);
      vc=0;
      n0=zee;
      break;
    case CGO_END:
      switch(mode) {
      case GL_LINE_LOOP:
        if(vc>1) ray->fSausage3fv(ray,v0,v2,lineradius,c0,c2);
        break;
      }
      mode=-1;
      break;
    case CGO_WIDTHSCALE:
      widthscale=*pc;
      lineradius=widthscale*linewidth;
      dotradius=widthscale*dotwidth;
      break;
    case CGO_DOTWIDTH:
      dotwidth=*pc;
      dotradius=widthscale*dotwidth;
      break;
    case CGO_LINEWIDTH:
      linewidth=*pc;
      lineradius=widthscale*linewidth;
      break;
    case CGO_NORMAL:
      n0=pc;
      break;
    case CGO_COLOR:
      c0=pc;
      ray->fColor3fv(ray,c0);
      break;
    case CGO_ALPHA:
      I->G->CGORenderer->alpha = *pc;
      ray->fTransparentf(ray,1.0F - *pc);
      break;
    case CGO_VERTEX:
      v0=pc;
      switch(mode) {
      case GL_POINTS:
        ray->fSphere3fv(ray,v0,dotradius);
        break;
      case GL_LINES:
        if(vc&0x1) ray->fSausage3fv(ray,v0,v1,lineradius,c0,c1);
        v1=v0;
        c1=c0;
        break;
      case GL_LINE_STRIP:
        if(vc) ray->fSausage3fv(ray,v0,v1,lineradius,c0,c1);
        v1=v0;
        c1=c0;
        break;
      case GL_LINE_LOOP:
        if(vc) 
          ray->fSausage3fv(ray,v0,v1,lineradius,c0,c1);
        else {
          v2=v0;
          c2=c0;
        }
        v1=v0;
        c1=c0;
        break;
      case GL_TRIANGLES:
        if(3*((vc+1)/3)==vc+1)
          ray->fTriangle3fv(ray,v0,v1,v2,n0,n1,n2,c0,c1,c2);
        v2=v1;
        c2=c1;
        n2=n1;
        v1=v0;
        c1=c0;
        n1=n0;
        break;
      case GL_TRIANGLE_STRIP:
        if(vc>1)
          ray->fTriangle3fv(ray,v0,v1,v2,n0,n1,n2,c0,c1,c2);
        v2=v1;
        c2=c1;
        n2=n1;
        v1=v0;
        c1=c0;
        n1=n0;
        break;
      case GL_TRIANGLE_FAN:
        if(vc>1)
          ray->fTriangle3fv(ray,v0,v1,v2,n0,n1,n2,c0,c1,c2);
        else if(!vc) {
          n2=n0;
          v2=v0;
          c2=c0;
        }
        v1=v0;
        c1=c0;
        n1=n0;
        break;
      }
      vc++;
      break;
    case CGO_SPHERE:
      ray->fColor3fv(ray,c0);
      ray->fSphere3fv(ray,pc,*(pc+3));
      break;
    case CGO_CUSTOM_CYLINDER:
      ray->fCustomCylinder3fv(ray,pc,pc+3,*(pc+6),pc+7,pc+10,(int)*(pc+13),(int)*(pc+14));
      break;
    case CGO_CYLINDER:
      ray->fCylinder3fv(ray,pc,pc+3,*(pc+6),pc+7,pc+10);
      break;
    case CGO_SAUSAGE:
      ray->fSausage3fv(ray,pc,pc+3,*(pc+6),pc+7,pc+10);
      break;
    case CGO_TRIANGLE:
      ray->fTriangle3fv(ray,pc,pc+3,pc+6,pc+9,pc+12,pc+15,pc+18,pc+21,pc+24);
      break;
    default:
      break;
    }
    pc+=CGO_sz[op];
  }

  ray->fTransparentf(ray,0.0F);
}

/* ======== GL Rendering ======== */

static void CGO_gl_begin(CCGORenderer *I,float *pc)
{
  glBegin(CGO_read_int(pc));
}

static void CGO_gl_end(CCGORenderer *I,float *pc)
{
  glEnd();
}

static void CGO_gl_linewidth(CCGORenderer *I,float *pc)
{
  glLineWidth(*pc);
}

static void CGO_gl_dotwidth(CCGORenderer *I,float *pc)
{
  glPointSize(*pc);
}

static void CGO_gl_enable(CCGORenderer *I,float *pc)
{
  glEnable(CGO_read_int(pc));
}

static void CGO_gl_disable(CCGORenderer *I,float *pc)
{
  glDisable(CGO_read_int(pc));
}

static void CGO_gl_alpha(CCGORenderer *I,float *pc)
{
  I->alpha = *pc;
}

static void CGO_gl_reset_normal(CCGORenderer *I,float *pc)
{
  SceneResetNormal(I->G,CGO_read_int(pc));
}

static void CGO_gl_null(CCGORenderer *I,float *pc) {
}

static void CGO_gl_vertex(CCGORenderer *I,float *v) {
  glVertex3fv(v);
}
static void CGO_gl_normal(CCGORenderer *I,float *v) {
  glNormal3fv(v);
}
static void CGO_gl_color(CCGORenderer *I,float *v) {
  glColor4f(v[0],v[1],v[2],I->alpha);
}

/* dispatch table for OpenGL */

CGO_op_fn CGO_gl[] = {
  CGO_gl_null,             /* 0x00 */
  CGO_gl_null,             /* 0x01 */
  CGO_gl_begin,            /* 0x02 */
  CGO_gl_end,              /* 0x03 */
  CGO_gl_vertex,           /* 0x04 */
  CGO_gl_normal,           /* 0x05 */
  CGO_gl_color,            /* 0x06 */
  CGO_gl_null,             /* 0x07 */
  CGO_gl_null,             /* 0x08 */
  CGO_gl_null,             /* 0x09 */
  
  CGO_gl_linewidth,        /* 0x0A */
  CGO_gl_null,             /* 0x0B */
  CGO_gl_enable,           /* 0x0C */
  CGO_gl_disable,          /* 0x0D */
  CGO_gl_null,             /* 0x0E */
  CGO_gl_null,             /* 0x0F */

  CGO_gl_dotwidth,         /* 0X10 */
  CGO_gl_null,             /* 0x11 */
  CGO_gl_null,             /* 0x12 */
  CGO_gl_null,             /* 0X13 */

  CGO_gl_null,             /* 0X14 */
  CGO_gl_null,             /* 0x15 */
  CGO_gl_null,             /* 0x16 */
  CGO_gl_null,             /* 0X17 */

  CGO_gl_null,             /* 0X18 */
  CGO_gl_alpha,            /* 0x19 */
  CGO_gl_null,             /* 0x1A */
  CGO_gl_null,             /* 0X1B */

  CGO_gl_null,             /* 0X1C */
  CGO_gl_null,             /* 0x1D */
  CGO_gl_reset_normal,     /* 0x1E */
  CGO_gl_null, /* pick color  0X1F */
};

void CGORenderGLPicking(CGO *I,Picking **pick,PickContext *context,CSetting *set1,CSetting *set2)
{
  register PyMOLGlobals *G = I->G;
  if(G->ValidContext) {
    register float *pc = I->op;
    register int op;
    register CCGORenderer *R =G->CGORenderer;
    int i,j;
    Picking *p;
    
    if(I->c) {
      i=(*pick)->src.index;
      
      glLineWidth(SettingGet_f(G,set1,set2,cSetting_cgo_line_width));
      
      while((op=(CGO_MASK&CGO_read_int(pc)))) {
        if(op!=CGO_PICK_COLOR) {
          if(op!=CGO_COLOR) {
            CGO_gl[op](R,pc); /* ignore color changes */
          }
        } else {
          i++;
          if(!(*pick)[0].src.bond) {
            /* pass 1 - low order bits */
            
            glColor3ub((uchar)((i&0xF)<<4),(uchar)((i&0xF0)|0x8),(uchar)((i&0xF00)>>4)); /* we're encoding the index into the color */
            VLACheck((*pick),Picking,i);
            p=(*pick)+i;
            p->context = (*context);
            p->src.index = (int)*pc;
            p->src.bond = (int)*(pc+1); /* actually holds state information */
          } else { 
            /* pass 2 - high order bits */
            
            j=i>>12;
            
            glColor3ub((uchar)((j&0xF)<<4),(uchar)((j&0xF0)|0x8),(uchar)((j&0xF00)>>4)); 
            
          }			 
          
        }
        pc+=CGO_sz[op];
      }
      (*pick)[0].src.index = i; /* pass the count */
      
    }
  }
}


void CGORenderGL(CGO *I,float *color,CSetting *set1,CSetting *set2,RenderInfo *info)
     /* this should be as fast as you can make it...
      * the ASM loop is about 2X long as raw looped GL calls,
      * but hopefully superscaler processors won't care */
{
  register PyMOLGlobals *G = I->G;
  if(G->ValidContext) {
    register float *pc = I->op;
    register int op;
    register CCGORenderer *R = G->CGORenderer;
    
    if(I->c) {
      R->alpha = 1.0F - SettingGet_f(I->G,set1,set2,cSetting_cgo_transparency);
      if(color) 
        glColor4f(color[0],color[1],color[2],R->alpha);
      else
        glColor4f(1.0,1.0,1.0,R->alpha);
      if(info && info->width_scale_flag) {
        glLineWidth(SettingGet_f(I->G,set1,set2,cSetting_cgo_line_width)*info->width_scale);
        glPointSize(SettingGet_f(I->G,set1,set2,cSetting_cgo_dot_width)*info->width_scale);

      } else {
        glLineWidth(SettingGet_f(I->G,set1,set2,cSetting_cgo_line_width));
        glPointSize(SettingGet_f(I->G,set1,set2,cSetting_cgo_dot_width));
      }
      
      while((op=(CGO_MASK&CGO_read_int(pc)))) {
        CGO_gl[op](R,pc);
        pc+=CGO_sz[op];
      }
    }
  }
}

/* translation function which turns cylinders and spheres into triangles */

void CGOSimpleSphere(CGO *I,float *v,float vdw)
{
  SphereRec *sp;
  int *q,*s;
  int b,c;
  int ds;

  ds = SettingGet_i(I->G,NULL,NULL,cSetting_cgo_sphere_quality);
  if(ds<0) ds=0;
  if(ds>3) ds=3;
  sp = I->G->Sphere->Sphere[ds];
  
  q=sp->Sequence;

  s=sp->StripLen;

  for(b=0;b<sp->NStrip;b++)
    {
      CGOBegin(I,GL_TRIANGLE_STRIP);
      for(c=0;c<(*s);c++)
        {
          CGONormalv(I,sp->dot[*q]);
          CGOVertex(I,v[0]+vdw*sp->dot[*q][0],
                    v[1]+vdw*sp->dot[*q][1],
                    v[2]+vdw*sp->dot[*q][2]);
          q++;
        }
      CGOEnd(I);
      s++;
    }
}

static void subdivide( int n, float *x, float *y)
{
  int a;
  if(n<3) {n=3;}
  for(a=0;a<=n;a++)
	 {
		x[a]=(float)cos(a*2*PI/n);
		y[a]=(float)sin(a*2*PI/n);
	 }
}

void CGOSimpleCylinder(CGO *I,float *v1,float *v2,float tube_size,float *c1,float *c2,int cap1,int cap2)
{

#define MAX_EDGE 50

  float d[3],t[3],p0[3],p1[3],p2[3],vv1[3],vv2[3],v_buf[9],*v;
  float x[50],y[50];
  float overlap;
  float nub;
  int colorFlag;
  int nEdge;
  int c;

  v=v_buf;
  nEdge= (int)SettingGet(I->G,cSetting_stick_quality);
  overlap = tube_size*SettingGet(I->G,cSetting_stick_overlap);
  nub = tube_size*SettingGet(I->G,cSetting_stick_nub);

  if(nEdge>MAX_EDGE)
    nEdge=MAX_EDGE;
  subdivide(nEdge,x,y);

  colorFlag=(c1!=c2)&&c2;

  CGOColorv(I,c1);

  /* direction vector */
  
  p0[0] = (v2[0] - v1[0]);
  p0[1] = (v2[1] - v1[1]);
  p0[2] = (v2[2] - v1[2]);
  
  normalize3f(p0);
  
  if(cap1==cCylCapRound) {
    vv1[0]=v1[0]-p0[0]*overlap;
    vv1[1]=v1[1]-p0[1]*overlap;
    vv1[2]=v1[2]-p0[2]*overlap;
  } else {
    vv1[0]=v1[0];
    vv1[1]=v1[1];
    vv1[2]=v1[2];
  }
  if(cap2==cCylCapRound) {
    vv2[0]=v2[0]+p0[0]*overlap;
    vv2[1]=v2[1]+p0[1]*overlap;
    vv2[2]=v2[2]+p0[2]*overlap;
  } else {
    vv2[0]=v2[0];
    vv2[1]=v2[1];
    vv2[2]=v2[2];
  }
  
  d[0] = (vv2[0] - vv1[0]);
  d[1] = (vv2[1] - vv1[1]);
  d[2] = (vv2[2] - vv1[2]);
  
  get_divergent3f(d,t);

  cross_product3f(d,t,p1);
  
  normalize3f(p1);
  
  cross_product3f(d,p1,p2);
  
  normalize3f(p2);
  
  /* now we have a coordinate system*/
  
  CGOBegin(I,GL_TRIANGLE_STRIP);
  for(c=nEdge;c>=0;c--)
	 {
		v[0] = p1[0]*x[c] + p2[0]*y[c];
		v[1] = p1[1]*x[c] + p2[1]*y[c];
		v[2] = p1[2]*x[c] + p2[2]*y[c];
		
		v[3] = vv1[0] + v[0]*tube_size;
		v[4] = vv1[1] + v[1]*tube_size;
		v[5] = vv1[2] + v[2]*tube_size;
		
		v[6] = v[3] + d[0];
		v[7] = v[4] + d[1];
		v[8] = v[5] + d[2];
		
      CGONormalv(I,v);
      if(colorFlag) CGOColorv(I,c1);
      CGOVertexv(I,v+3);
      if(colorFlag) CGOColorv(I,c2);
      CGOVertexv(I,v+6);
	 }
  CGOEnd(I);

  if(cap1) {
  v[0] = -p0[0];
  v[1] = -p0[1];
  v[2] = -p0[2];

    if(cap1==cCylCapRound) {
      v[3] = vv1[0] - p0[0]*nub;
      v[4] = vv1[1] - p0[1]*nub;
      v[5] = vv1[2] - p0[2]*nub;
    } else {
      v[3] = vv1[0];
      v[4] = vv1[1];
      v[5] = vv1[2];
    }
    
    if(colorFlag) CGOColorv(I,c1);
    CGOBegin(I,GL_TRIANGLE_FAN);
    CGONormalv(I,v);
    CGOVertexv(I,v+3);
    
    for(c=nEdge;c>=0;c--)
      {
        v[0] = p1[0]*x[c] + p2[0]*y[c];
        v[1] = p1[1]*x[c] + p2[1]*y[c];
        v[2] = p1[2]*x[c] + p2[2]*y[c];
        
        v[3] = vv1[0] + v[0]*tube_size;
        v[4] = vv1[1] + v[1]*tube_size;
        v[5] = vv1[2] + v[2]*tube_size;
        
        if(cap1==cCylCapRound) CGONormalv(I,v);
        CGOVertexv(I,v+3);
      }
    CGOEnd(I);
  }

  if(cap2) {
    
    v[0] = p0[0];
    v[1] = p0[1];
    v[2] = p0[2];
    
    if(cap2==cCylCapRound) {
      v[3] = vv2[0] + p0[0]*nub;
      v[4] = vv2[1] + p0[1]*nub;
      v[5] = vv2[2] + p0[2]*nub;
    } else {
      v[3] = vv2[0];
      v[4] = vv2[1];
      v[5] = vv2[2];
    }
    
    if(colorFlag) CGOColorv(I,c2);
    CGOBegin(I,GL_TRIANGLE_FAN);
    CGONormalv(I,v);
    CGOVertexv(I,v+3);
    
    for(c=0;c<=nEdge;c++)
      {
        v[0] = p1[0]*x[c] + p2[0]*y[c];
        v[1] = p1[1]*x[c] + p2[1]*y[c];
        v[2] = p1[2]*x[c] + p2[2]*y[c];
        
        v[3] = vv2[0] + v[0]*tube_size;
        v[4] = vv2[1] + v[1]*tube_size;
        v[5] = vv2[2] + v[2]*tube_size;
        
        if(cap2==cCylCapRound) CGONormalv(I,v);
        CGOVertexv(I,v+3);
      }
    CGOEnd(I);
  }
}


