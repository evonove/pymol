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

#include"os_std.h"
#include"os_gl.h"

#include"Base.h"
#include"MemoryDebug.h"
#include"OOMac.h"
#include"RepSurface.h"
#include"Map.h"
#include"Scene.h"
#include"Sphere.h"
#include"Setting.h"
#include"Color.h"
#include"ObjectMolecule.h"
#include"Triangle.h"
#include"Vector.h"
#include"main.h"

#ifdef NT
#undef NT
#endif

typedef struct RepSurface {
  Rep R;
  int N;
  int NT;
  float *V,*VN,*VC;
  int *Vis;
  int *T,*S; /* S=strips */
  int NDot;
  float *Dot;
  int solidFlag;
  int oneColorFlag,oneColor;
  int allVisibleFlag;
  Object *Obj;
  int *LastVisib;
  int *LastColor;
} RepSurface;


void RepSurfaceRender(RepSurface *I,CRay *ray,Pickable **pick);
void RepSurfaceFree(RepSurface *I);
int RepSurfaceSameVis(RepSurface *I,CoordSet *cs);

void RepSurfaceColor(RepSurface *I,CoordSet *cs);

void RepSurfaceFree(RepSurface *I)
{
  FreeP(I->V);
  FreeP(I->VN);
  FreeP(I->VC);
  FreeP(I->Vis);
  FreeP(I->LastColor);
  FreeP(I->LastVisib);
  VLAFreeP(I->T);
  VLAFreeP(I->S);
  /*  VLAFreeP(I->N);*/
  OOFreeP(I);
}

void RepSurfaceGetSolventDots(RepSurface *I,CoordSet *cs,float probe_radius,SphereRec *sp,float *extent);

void RepSurfaceRender(RepSurface *I,CRay *ray,Pickable **pick)
{
  float *v=I->V;
  float *vn=I->VN;
  float *vc=I->VC;
  int *t=I->T;
  int *s=I->S;
  int c=I->N;
  int *vi=I->Vis;

  if(ray) {
	 c=I->NT;
      while(c--)
        {
          if((*(vi+(*t)))&&(*(vi+(*(t+1))))&&(*(vi+(*(t+2)))))
            ray->fTriangle3fv(ray,v+(*t)*3,v+(*(t+1))*3,v+(*(t+2))*3,
                              vn+(*t)*3,vn+(*(t+1))*3,vn+(*(t+2))*3,
                              vc+(*t)*3,vc+(*(t+1))*3,vc+(*(t+2))*3);
          t+=3;
        }
  } else if(pick&&PMGUI) {
  } else if(PMGUI) {

	 if(I->S) {
      if(I->allVisibleFlag) {
        if(I->oneColorFlag) {
          glColor3fv(ColorGet(I->oneColor));
          c=*(s++);
          while(c) {
            glBegin(GL_TRIANGLE_STRIP);
            glNormal3fv(vn+(*s)*3);
            glVertex3fv(v+(*s)*3);
            s++;
            glNormal3fv(vn+(*s)*3);
            glVertex3fv(v+(*s)*3);
            s++;
            while(c--)
              {
                glNormal3fv(vn+(*s)*3);
                glVertex3fv(v+(*s)*3);
                s++;
              }
            glEnd();
            c=*(s++);
          }
        } else {
          c=*(s++);
          while(c) {
            glBegin(GL_TRIANGLE_STRIP);
            glColor3fv(vc+(*s)*3);
            glNormal3fv(vn+(*s)*3);
            glVertex3fv(v+(*s)*3);
            s++;
            glColor3fv(vc+(*s)*3);
            glNormal3fv(vn+(*s)*3);
            glVertex3fv(v+(*s)*3);
            s++;
            while(c--)
              {
                glColor3fv(vc+(*s)*3);
                glNormal3fv(vn+(*s)*3);
                glVertex3fv(v+(*s)*3);
                s++;
              }
            glEnd();
            c=*(s++);
          }
        }
        
		} else {
        /* subsets: not-optimized */
        c=I->NT;
        if(c) {
          glBegin(GL_TRIANGLES);
          while(c--) {
            if((*(vi+(*t)))&&(*(vi+(*(t+1))))&&(*(vi+(*(t+2))))) {
              glColor3fv(vc+(*t)*3);
              glNormal3fv(vn+(*t)*3);
              glVertex3fv(v+(*t)*3);
              t++;
              glColor3fv(vc+(*t)*3);
              glNormal3fv(vn+(*t)*3);
              glVertex3fv(v+(*t)*3);
              t++;
              glColor3fv(vc+(*t)*3);
              glNormal3fv(vn+(*t)*3);
              glVertex3fv(v+(*t)*3);
              t++;
            } else
              t+=3;
          }
          glEnd();
        }

      }
	 }
		/*

	c=I->NT;
		if(c) {
		glBegin(GL_TRIANGLES);
		while(c--) {
		glNormal3fv(vn+(*t)*3);
		glVertex3fv(v+(*t)*3);
		t++;
		glNormal3fv(vn+(*t)*3);
		glVertex3fv(v+(*t)*3);
		t++;
		glNormal3fv(vn+(*t)*3);
		glVertex3fv(v+(*t)*3);
		t++;
		}
		glEnd();
		}

		  t=I->T;
		  c=I->NT;
  if(c) {
	 glColor3f(0.0,1.0,0.0);
	 
	 while(c--)
		{
		  glBegin(GL_LINE_STRIP);
		  
		  glNormal3fv(vn+(*t)*3);
		  glVertex3fv(v+(*t)*3);
		  t++;
		  glNormal3fv(vn+(*t)*3);
		  glVertex3fv(v+(*t)*3);
		  t++;
		  glNormal3fv(vn+(*t)*3);
		  glVertex3fv(v+(*t)*3);
		  t++;
		  glEnd();
		}
  }
  c=I->N;
  if(c) {
	 glColor3f(1.0,0.0,0.0);
	 glBegin(GL_LINES);
	 SceneResetNormal(true);
	 while(c--)
		{
		  glVertex3fv(v);
		  glVertex3f(v[0]+vn[0]/2,v[1]+vn[1]/2,v[2]+vn[2]/2);
		  v+=3;
		  vn+=3;
		}
	 glEnd();
  }
  glColor3f(0.3,0.3,1.0);
  v=TestLine;
  c=NTestLine;
  glBegin(GL_LINES);
  while(c--) {
  glVertex3fv(v);
  v+=3;
  glVertex3fv(v);
  v+=3;
  }
  glEnd();
*/
  }
}

#define solv_tole 0.02

int RepSurfaceSameVis(RepSurface *I,CoordSet *cs)
{
  int same = true;
  int *lv,*lc,*cc;
  int a;
  AtomInfoType *ai;

  ai = cs->Obj->AtomInfo;
  lv = I->LastVisib;
  lc = I->LastColor;
  cc = cs->Color;

  for(a=0;a<cs->NIndex;a++)
    {
      if(*(lv++)!=(ai + cs->IdxToAtm[a])->visRep[cRepSurface] ) {
        same=false;
        break;
      }
      if(*(lc++)!=*(cc++)) {
        same=false;
        break;
      }
    }
  return(same);
}

void RepSurfaceColor(RepSurface *I,CoordSet *cs)
{
  MapType *map;
  int a,i0,i,j,h,k,l,c1;
  float *v0,*vc,*c0;
  int *vi,*lv,*lc,*cc;
  int first_color;
  ObjectMolecule *obj;
  float probe_radius;
  float dist,minDist;
  float proximity,cutoff;
  int inclH;
  int cullByFlag = false;
  AtomInfoType *ai2;

  /*  cullByFlag = SettingGet(cSetting_trim_dots);*/
  inclH = SettingGet(cSetting_dot_hydrogens);
  probe_radius = SettingGet(cSetting_solvent_radius);
  proximity = SettingGet(cSetting_surface_proximity);

  cutoff = MAX_VDW+probe_radius;
  if(cutoff<(MAX_VDW+proximity))
    cutoff=MAX_VDW+proximity;

  if(!I->LastVisib) I->LastVisib = Alloc(int,cs->NIndex);
  if(!I->LastColor) I->LastColor = Alloc(int,cs->NIndex);
  lv = I->LastVisib;
  lc = I->LastColor;
  cc = cs->Color;
  obj=cs->Obj;
  ai2=obj->AtomInfo;
  for(a=0;a<cs->NIndex;a++)
    {
      *(lv++) = (ai2 + cs->IdxToAtm[a])->visRep[cRepSurface];
      *(lc++) = *(cc++);
    }
  
  if(I->N) {
	 I->oneColorFlag=true;
	 first_color=-1;
	 if(!I->VC) I->VC = Alloc(float,3*I->N);
	 vc=I->VC;
    if(!I->Vis) I->Vis = Alloc(int,I->N);
    vi=I->Vis;
	 /* now, assign colors to each point */
	 map=MapNew(cutoff,cs->Coord,cs->NIndex,NULL);
	 if(map)
		{
		  MapSetupExpress(map);
		  for(a=0;a<I->N;a++)
			 {
				c1=1;
				minDist=MAXFLOAT;
				i0=-1;
				v0 = I->V+3*a;
				MapLocus(map,v0,&h,&k,&l);
				
				i=*(MapEStart(map,h,k,l));
				if(i) {
				  j=map->EList[i++];
				  while(j>=0) {
					 ai2 = obj->AtomInfo + cs->IdxToAtm[j];
					 if((inclH||(!ai2->hydrogen))&&
						 ((!cullByFlag)||(!(ai2->flags&0x2000000))))  
						/* ignore atom if flag 25 is set */
						{
						  dist = diff3f(v0,cs->Coord+j*3)-ai2->vdw;
						  if(dist<minDist)
							 {
								i0=j;
								minDist=dist;
							 }
						}
					 j=map->EList[i++];
				  }
				}
				if(i0>=0) {
				  c1=*(cs->Color+i0);
				  if(I->oneColorFlag) {
					 if(first_color>=0) {
						if(first_color!=c1)
						  I->oneColorFlag=false;
					 } else first_color=c1;
				  }
				}
				c0 = ColorGet(c1);
				*(vc++) = *(c0++);
				*(vc++) = *(c0++);
				*(vc++) = *(c0++);

            if(I->allVisibleFlag)
              *(vi++)=1;
            else {
              i0=-1;
              i=*(MapEStart(map,h,k,l));
              if(i) {
                j=map->EList[i++];
                while(j>=0) {
                  ai2 = obj->AtomInfo+cs->IdxToAtm[j];
                  if(ai2->visRep[cRepSurface])
                    if((inclH||(!ai2->hydrogen))&&
                       ((!cullByFlag)||(!(ai2->flags&0x2000000))))  
                      /* ignore if the "2" bit is set */
                      {
                        dist = diff3f(v0,cs->Coord+j*3);
                        if(dist<(ai2->vdw+proximity)) {
                          i0=j;
                          break;
                        }
                      }
                  j=map->EList[i++];
                }
              }
              if(i0>=0)
                *(vi++) = 1;
              else
                *(vi++) = 0;
            }

			 }
		  MapFree(map);
		}
    if(I->oneColorFlag) {
      I->oneColor=first_color;
    }
  } 
}

Rep *RepSurfaceNew(CoordSet *cs)
{
  ObjectMolecule *obj;
  int a,b,h,i,j,k,l,c,c1;
  MapType *map,*solv_map;
  float v1[3],*v0,*v,*vn,*vn0,*extent=NULL;
  float vdw;
  int SurfaceFlag = false;
  float probe_radius,probe_radius2;
  int inclH = true;
  int cullByFlag = false;
  int flag,*dot_flag,*p;
  int cnt;
  float minimum_sep;
  int visFlag;
  SphereRec *sp = Sphere0;
  AtomInfoType *ai1,*ai2;
  OOAlloc(RepSurface);

  obj = cs->Obj;
  visFlag=false;
  for(a=0;a<cs->NIndex;a++) {
	 if(obj->AtomInfo[cs->IdxToAtm[a]].visRep[cRepSurface])
		{
		  visFlag=true;
		  break;
		}
  }
  if(!visFlag) {
    OOFreeP(I);
    return(NULL); /* skip if no dots are visible */
  }

  RepInit(&I->R);

  cnt = SettingGet(cSetting_test2);

  if((int)SettingGet(cSetting_surface_quality)) {
    minimum_sep = SettingGet(cSetting_surface_best);
    sp=Sphere2;
  } else {
    minimum_sep = SettingGet(cSetting_surface_normal);
    sp=Sphere1;
  }

  cullByFlag = SettingGet(cSetting_trim_dots);
  inclH = SettingGet(cSetting_dot_hydrogens);

  probe_radius = SettingGet(cSetting_solvent_radius);
  probe_radius2 = probe_radius*probe_radius;

  I->N=0;
  I->NT=0;
  I->S=NULL;
  I->V=NULL;
  I->VC=NULL;
  I->Vis=NULL;
  I->VN=NULL;
  I->T=NULL;
  I->LastVisib=NULL;
  I->LastColor=NULL;
  I->R.fRender=(void (*)(struct Rep *, CRay *, Pickable **))RepSurfaceRender;
  I->R.fFree=(void (*)(struct Rep *))RepSurfaceFree;
  I->R.fRecolor=(void (*)(struct Rep*, struct CoordSet*))RepSurfaceColor;
  I->R.fSameVis=(int (*)(struct Rep*, struct CoordSet*))RepSurfaceSameVis;
  I->Obj = (Object*)(cs->Obj);
  I->Dot=NULL;
  I->allVisibleFlag=true;

  obj = cs->Obj;

  /* don't waist time computing a Surface unless we need it!! */
  for(a=0;a<cs->NIndex;a++) {
	 if(obj->AtomInfo[cs->IdxToAtm[a]].visRep[cRepSurface])
      SurfaceFlag=true;
    else
      I->allVisibleFlag=false;
  }
  if(SurfaceFlag) {
      
	 I->V=Alloc(float,cs->NIndex*3*sp->nDot*2);
    ErrChkPtr(I->V);
	 I->VN=Alloc(float,cs->NIndex*3*sp->nDot*2);
    ErrChkPtr(I->VN);
	 I->N=0;
    v=I->V;
    vn=I->VN;

	 OrthoBusyFast(0,1);
	 RepSurfaceGetSolventDots(I,cs,probe_radius,Sphere2,extent);
	 solv_map=MapNew(probe_radius,I->Dot,I->NDot,extent);
	 map=MapNew(MAX_VDW+probe_radius,cs->Coord,cs->NIndex,extent);

	 if(map&&solv_map)
		{
		  MapSetupExpress(solv_map);
		  MapSetupExpress(map);
        for(a=0;a<cs->NIndex;a++)
          {
            ai1 = obj->AtomInfo+cs->IdxToAtm[a];
            if((inclH||(!ai1->hydrogen))&&
               ((!cullByFlag)||
                (!(ai1->flags&1000000)))) {
              /* don't surface if flag 24 is set BROKEN */
              c1=*(cs->Color+a);
              v0 = cs->Coord+3*a;
              vdw = ai1->vdw;
              
              for(b=0;b<sp->nDot;b++)
                {
                  v1[0]=v0[0]+vdw*sp->dot[b].v[0];
                  v1[1]=v0[1]+vdw*sp->dot[b].v[1];
                  v1[2]=v0[2]+vdw*sp->dot[b].v[2];
                  
                  MapLocus(map,v1,&h,&k,&l);
                  
                  flag=true;
                  
                  i=*(MapEStart(map,h,k,l));
                  if(i) {
                    j=map->EList[i++];
                    while(j>=0) {
                      ai2 = obj->AtomInfo + cs->IdxToAtm[j];
                      if((inclH||(!ai2->hydrogen))&&
                           ((!cullByFlag)||(!(ai2->flags&0x2000000))))  
                        /* ignore atom if flag 25 if set BROKEN !!!*/
                        if(j!=a)
                          if(within3f(cs->Coord+3*j,v1,ai2->vdw))
                            {
                              flag=false;
                              break;
                            }
                      j=map->EList[i++];
                    }
                  }
                  if(flag) /* doesn't intersect any atom */
                    {
                      MapLocus(solv_map,v1,&h,&k,&l);
                      
                      flag=true;
                      i=*(MapEStart(solv_map,h,k,l));
                      if(i) {
                        j=solv_map->EList[i++];
                        while(j>=0) {
                          if(within3f(I->Dot+3*j,v1,probe_radius+solv_tole))
                            {
                              flag=false;
                              break;
                            }
                          j=solv_map->EList[i++];
                        }
                      }
                      if(!flag) /* appropriately close to a water center */
                        {
                          *(v++)=v1[0];
                          *(v++)=v1[1];
                          *(v++)=v1[2];
                          *(vn++)=sp->dot[b].v[0];
                          *(vn++)=sp->dot[b].v[1];
                          *(vn++)=sp->dot[b].v[2];
                          I->N++;
                        }
                    }
                }
            }
          }
		  OrthoBusyFast(3,10);
		  if(I->NDot) {
			 /* concave surfaces */
			 for(a=0;a<I->NDot;a++)
				{
				  v0 = I->Dot+3*a;
				  vdw = probe_radius;
				  for(b=0;b<sp->nDot;b++)
					 {
						v[0]=v0[0]+vdw*sp->dot[b].v[0];
						v[1]=v0[1]+vdw*sp->dot[b].v[1];
						v[2]=v0[2]+vdw*sp->dot[b].v[2];
						MapLocus(solv_map,v,&h,&k,&l);
						flag=true;
						i=*(MapEStart(solv_map,h,k,l));
						if(i) {
						  j=solv_map->EList[i++];
						  while(j>=0) {
							 if(j!=a) 
								{
								  if(within3f(I->Dot+3*j,v,probe_radius)) {
									 flag=false;
									 break;
								  }
								}
							 j=solv_map->EList[i++];
						  }
						}
						if(flag)
						  {
							 MapLocus(map,v,&h,&k,&l);
							 flag=true;
							 i=*(MapEStart(map,h,k,l));
							 if(i) {
								j=map->EList[i++];
								while(j>=0) {
								  ai2 = obj->AtomInfo + cs->IdxToAtm[j];
								  if((inclH||(!ai2->hydrogen))&&
									  ((!cullByFlag)||(!(ai2->flags&0x2000000))))  
										  /* ignore if flag 25 is set  - BROKEN */
									 if(j!=a)
										if(within3f(cs->Coord+3*j,v,ai2->vdw+probe_radius+solv_tole))
										  {
											 flag=false;
											 break;
										  }
								  j=map->EList[i++];
								}
							 }
							 if(!flag) {
								I->N++;
								v+=3;
								*(vn++)=-sp->dot[b].v[0];
								*(vn++)=-sp->dot[b].v[1];
								*(vn++)=-sp->dot[b].v[2];
							 }
						  }
					 }
				}
		  }
		  MapFree(solv_map);
		  MapFree(map);
		  FreeP(I->Dot);	 
		}
	 /* now, eliminate dots that are too close to each other*/

	 OrthoBusyFast(4,10);
    printf(" RepSurface: %i surface points.\n",I->N);

	 if(I->N) 
		{
		  dot_flag=Alloc(int,I->N);
		  for(a=0;a<I->N;a++) dot_flag[a]=1;
		  map=MapNew(minimum_sep,I->V,I->N,extent);
        MapSetupExpress(map);		  
        v=I->V;
        vn=I->VN;
        for(a=0;a<I->N;a++) {
          if(dot_flag[a]) {
            MapLocus(map,v,&h,&k,&l);
            i=*(MapEStart(map,h,k,l));
            if(i) {
              j=map->EList[i++];
              while(j>=0) {
                if(j!=a) 
                  {
                    if(dot_flag[j]) {
                      if(within3f(I->V+(3*j),v,minimum_sep)) {
                        dot_flag[j]=0;
                        add3f(vn,I->VN+(3*j),vn);
                        average3f(I->V+(3*j),v,v);
                      } 
                    }
                  }
                j=map->EList[i++];
              }
            }
          }
          v+=3;
          vn+=3;
        }
        MapFree(map);
        v0=I->V;
        v=I->V;
        vn0=I->VN;
        vn=I->VN;
        p=dot_flag;
		  c=I->N;
		  I->N=0;
		  for(a=0;a<c;a++)
			 {
				if(*(p++)) {
				  *(v0++)=*(v++);
				  *(v0++)=*(v++);
				  *(v0++)=*(v++);
              normalize3f(vn);
				  *(vn0++)=*(vn++);
				  *(vn0++)=*(vn++);
				  *(vn0++)=*(vn++);
				  I->N++;
				} else {
				  v+=3;
				  vn+=3;
				}
			 }
		  FreeP(dot_flag);
      }

    if(I->N) {	
      I->V = Realloc(I->V,float,(v0-I->V));
      I->VN = Realloc(I->VN,float,(vn0-I->VN));
    }

	 RepSurfaceColor(I,cs);
	 OrthoBusyFast(5,10);

    if(I->N) {
      I->T=TrianglePointsToSurface(I->V,I->VN,I->N,probe_radius,&I->NT,&I->S,extent);
		printf(" RepSurface: %i triangles.\n",I->NT);
    } else {
      I->V = Realloc(I->V,float,1);
      I->VN = Realloc(I->VN,float,1);
    }
    
  }
  OrthoBusyFast(4,4);
  return((void*)(struct Rep*)I);
}

void RepSurfaceGetSolventDots(RepSurface *I,CoordSet *cs,float probe_radius,SphereRec *sp,float *extent)
{
  ObjectMolecule *obj;
  int a,b,c,flag,i,h,k,l,j;
  float *v,*v0,vdw;
  MapType *map;
  int *p,*dot_flag;
  int cavity_cull;
  float probe_radius_plus;
  int dotCnt,maxCnt,maxDot=0;
  int cnt;
  cavity_cull = (int)SettingGet(cSetting_cavity_cull);

  obj = cs->Obj;
  I->Dot=(float*)mmalloc(sizeof(float)*cs->NIndex*3*sp->nDot);
  ErrChkPtr(I->Dot);

  probe_radius_plus = probe_radius * 1.5;

  I->NDot=0;
  map=MapNew(MAX_VDW+probe_radius,cs->Coord,cs->NIndex,extent);
  if(map)
	 {
		MapSetupExpress(map);
		maxCnt=0;
		v=I->Dot;
		for(a=0;a<cs->NIndex;a++)
		  {
			 OrthoBusyFast(a,cs->NIndex*5);
			 dotCnt=0;
			 v0 = cs->Coord+3*a;
			 vdw = cs->Obj->AtomInfo[cs->IdxToAtm[a]].vdw+probe_radius;
          for(b=0;b<sp->nDot;b++)
            {
              v[0]=v0[0]+vdw*sp->dot[b].v[0];
              v[1]=v0[1]+vdw*sp->dot[b].v[1];
              v[2]=v0[2]+vdw*sp->dot[b].v[2];
              MapLocus(map,v,&h,&k,&l);
              flag=true;
              i=*(MapEStart(map,h,k,l));
              if(i) {
                j=map->EList[i++];
                while(j>=0) {
                  if(j!=a) 
                    {
                      if(within3f(cs->Coord+3*j,v,obj->AtomInfo[cs->IdxToAtm[j]].vdw+probe_radius)) {
                        flag=false;
                        break;
                      }
                    }
                  j=map->EList[i++];
                }
              }
              if(flag)
                {
                  dotCnt++;
                  v+=3;
                  I->NDot++;
                }
            }
			 if(dotCnt>maxCnt)
				{
				  maxCnt=dotCnt;
				  maxDot=I->NDot-1;
				}
		  }
		MapFree(map);
	 }

  if(cavity_cull>0) {
	 dot_flag=Alloc(int,I->NDot);
	 ErrChkPtr(dot_flag);
	 for(a=0;a<I->NDot;a++) {
		dot_flag[a]=0;
	 }
	 dot_flag[maxDot]=1; /* this guarantees that we have a valid dot */

	 map=MapNew(probe_radius_plus,I->Dot,I->NDot,extent);
	 if(map)
		{
		  MapSetupExpress(map);		  
		  flag=true;
		  while(flag) {
			 p=dot_flag;
			 v=I->Dot;
		  
			 flag=false;
			 for(a=0;a<I->NDot;a++)
				{
				  if(!dot_flag[a]) {
					 cnt=0;
					 MapLocus(map,v,&h,&k,&l);
					 i=*(MapEStart(map,h,k,l));
					 if(i) {
						j=map->EList[i++];
						while(j>=0) {
						  if(j!=a) 
							 {
								if(within3f(I->Dot+(3*j),v,probe_radius_plus)) {
								  if(dot_flag[j]) {
									 *p=true;
									 flag=true;
									 break;
								  }
								  cnt++;
								  if(cnt>cavity_cull) 
									 {
										*p=true;
										flag=true;
										break;
									 }
								}
							 }
						  j=map->EList[i++];
						}
					 }
				  }
				  v+=3;
				  p++;
				}
		  }
		  MapFree(map);
		}
	 v0=I->Dot;
	 v=I->Dot;
	 p=dot_flag;
	 c=I->NDot;
	 I->NDot=0;
	 for(a=0;a<c;a++)
		{
		  if(*(p++)) {
			 *(v0++)=*(v++);
			 *(v0++)=*(v++);
			 *(v0++)=*(v++);
			 I->NDot++;
		  } else {
			 v+=3;
		  }
		}
	 FreeP(dot_flag);
  }
  /*  printf("NDot: %d->%d\n",c,I->NDot);*/
}


