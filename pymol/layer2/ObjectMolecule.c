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

#include"os_predef.h"
#include"os_std.h"
#include"os_gl.h"

#include"Base.h"
#include"Debug.h"
#include"Parse.h"
#include"OOMac.h"
#include"Vector.h"
#include"MemoryDebug.h"
#include"Err.h"
#include"Map.h"
#include"Selector.h"
#include"ObjectMolecule.h"
#include"Ortho.h"
#include"Util.h"
#include"Vector.h"
#include"Selector.h"
#include"Matrix.h"
#include"Scene.h"
#include"P.h"
#include"PConv.h"
#include"Executive.h"
#include"Setting.h"
#include"Sphere.h"
#include"main.h"
#include"CGO.h"
#include"Raw.h"
#include"Editor.h"
#include"Selector.h"
#include"Sculpt.h"
#include"OVContext.h"
#include"OVOneToOne.h"
#include"OVLexicon.h"

#define cMaxNegResi 100

#define ntrim ParseNTrim
#define nextline ParseNextLine
#define ncopy ParseNCopy
#define nskip ParseNSkip

void ObjectMoleculeCylinders(ObjectMolecule *I);
CoordSet *ObjectMoleculeMMDStr2CoordSet(PyMOLGlobals *G,char *buffer,AtomInfoType **atInfoPtr);

int ObjectMoleculeDoesAtomNeighborSele(ObjectMolecule *I, int index, int sele);


void ObjectMoleculeAppendAtoms(ObjectMolecule *I,AtomInfoType *atInfo,CoordSet *cset);

void ObjectMoleculeUpdate(ObjectMolecule *I);
int ObjectMoleculeGetNFrames(ObjectMolecule *I);

void ObjectMoleculeDescribeElement(ObjectMolecule *I,int index,char *buffer);

void ObjectMoleculeSeleOp(ObjectMolecule *I,int sele,ObjectMoleculeOpRec *op);

void ObjectMoleculeTransformTTTf(ObjectMolecule *I,float *ttt,int state);



int ObjectMoleculeGetAtomGeometry(ObjectMolecule *I,int state,int at);
void ObjectMoleculeBracketResidue(ObjectMolecule *I,AtomInfoType *ai,int *st,int *nd);

void ObjectMoleculeAddSeleHydrogens(ObjectMolecule *I,int sele,int state);


CSetting **ObjectMoleculeGetSettingHandle(ObjectMolecule *I,int state);
void ObjectMoleculeInferAmineGeomFromBonds(ObjectMolecule *I,int state);
CoordSet *ObjectMoleculeTOPStr2CoordSet(PyMOLGlobals *G,char *buffer,
                                        AtomInfoType **atInfoPtr);

ObjectMolecule *ObjectMoleculeReadTOPStr(PyMOLGlobals *G,ObjectMolecule *I,char *TOPStr,int frame,int discrete);

void ObjectMoleculeInferHBondFromChem(ObjectMolecule *I);

int ObjectMoleculeSetDiscrete(PyMOLGlobals *G,ObjectMolecule *I,int discrete) 
{
  /* currently only works for object with no coordinates that need to
     become discrete */
  int ok=true;
  if((discrete>0) && (!I->DiscreteFlag)) {
    I->DiscreteFlag=discrete;
    I->NDiscrete=0;
    if(I->DiscreteFlag) { 
      I->DiscreteAtmToIdx = VLAMalloc(10,sizeof(int),6,false);
      I->DiscreteCSet = VLAMalloc(10,sizeof(CoordSet*),5,false);
    } else {
      I->DiscreteAtmToIdx = NULL;
      I->DiscreteCSet = NULL;
    }    
  }
  return ok;
}

int ObjectMoleculeCheckFullStateSelection(ObjectMolecule *I,int sele, int state)
{
  register PyMOLGlobals *G = I->Obj.G;
  int result=false;
  if((state>=0)&&(state<I->NCSet)) {
    register AtomInfoType *ai = I->AtomInfo;
    register CoordSet *cs = I->CSet[state];
    if(cs) {
      register int a;
      register int at;
      result=true;
      for(a=0;a<cs->NIndex;a++) {
        at = cs->IdxToAtm[a];
        if(!SelectorIsMember(G,ai[at].selEntry,sele)) {
          result = false;
          break;
        }
      }
    }
  }
  return result;
}

static char *ObjectMoleculeGetCaption(ObjectMolecule *I)
{
  int state = ObjectGetCurrentState((CObject*)I,false);
  
  if((state>=0)&&(state<I->NCSet)) {
    CoordSet *cs = I->CSet[state];
    if(cs) {
      return cs->Name;
    }
  }
  return NULL;
}

int ObjectMoleculeGetMatrix(ObjectMolecule *I,int state,double **history)
{
  int ok = true;
  if((state<0)||(state>=I->NCSet)) {
    /* nonsensical -- or should get the TTT if state<0 */
    ok=false;
  } else {
    CoordSet *cs = I->CSet[state];
    if(!cs)
      ok=false;
    else {
      (*history) = cs->State.Matrix; /* note -- can be NULL */
    }
  }
  return ok;
}

int ObjectMoleculeSetMatrix(ObjectMolecule *I,int state,double *matrix)
{
  int ok = true;
  if((state<0)||(state>=I->NCSet)) {
    /* nonsensical  -- or should set the TTT if state<0*/
    ok=false;
  } else {
    CoordSet *cs = I->CSet[state];
    if(!cs)
      ok=false;
    else {
      ObjectStateSetMatrix(&cs->State,matrix);
    }
  }
  return ok;
}

#define MAX_BOND_DIST 50

#if 0
static void dump_jxn(char *lab,char *q)
{
  char cc[MAXLINELEN];  
  printf("\n%s %p\n",lab,q);
  q=q-150;
  q=nextline(q);
  ncopy(cc,q,100);
  printf("0[%s]\n",cc);
  q=nextline(q);
  ncopy(cc,q,100);
  printf("1[%s]\n",cc);
  q=nextline(q);
  ncopy(cc,q,100);
  printf("2[%s]\n",cc);
  q=nextline(q);
  ncopy(cc,q,100);
  printf("3[%s]\n",cc);

}
#endif

void ObjectMoleculeTransformState44f(ObjectMolecule *I,int state,float *matrix,
                                            int log_trans,int homogenous,int transformed)
{
  int a;
  int use_matrices = SettingGet_b(I->Obj.G,I->Obj.Setting,NULL,cSetting_matrix_mode);
  float tmp_matrix[16];
  CoordSet *cs;
  if(!use_matrices) {
    ObjectMoleculeTransformSelection(I,state,-1,matrix,log_trans,I->Obj.Name,homogenous,true);
  } else {
    double dbl_matrix[16];
    /* ensure homogenous matrix to preserve programmer sanity */
    if(!homogenous) {
      convertTTTfR44d(matrix,dbl_matrix);
      copy44d44f(dbl_matrix,tmp_matrix);
      matrix = tmp_matrix;
    } else {
      copy44f44d(matrix,dbl_matrix);
    }
    if(state<0) { /* all states */
      for(a=0;a<I->NCSet;a++) {
        cs = I->CSet[a];
        if(cs) ObjectStateLeftCombineMatrixR44d(&cs->State, dbl_matrix);
      }
    } else if(state<I->NCSet) { /* single state */
      cs = I->CSet[(I->CurCSet = state % I->NCSet )];
      if(cs) ObjectStateLeftCombineMatrixR44d(&cs->State,  dbl_matrix);
    } else if(I->NCSet==1) { /* static singleton state */
      cs = I->CSet[0];
      if(cs && SettingGet_b(I->Obj.G,I->Obj.Setting,NULL,cSetting_static_singletons)) {
        ObjectStateLeftCombineMatrixR44d(&cs->State, dbl_matrix);
      }
    }
  }
}
/*========================================================================*/
static void ObjectMoleculeFixSeleHydrogens(ObjectMolecule *I,int sele,int state)
{
  int a,b;
  int n;
  CoordSet *cs;
  int seleFlag=false;
  int h_idx;
  float fixed[3],v0[3],v1[3],sought[3];
  AtomInfoType *ai0,*ai1;

  ai0=I->AtomInfo;
  for(a=0;a<I->NAtom;a++) {
    if(SelectorIsMember(I->Obj.G,ai0->selEntry,sele)) {
      seleFlag=true;
      break;
    }
    ai0++;
  }
  if(seleFlag) {
    seleFlag=false;
    if(!ObjectMoleculeVerifyChemistry(I,state)) {
      ErrMessage(I->Obj.G," AddHydrogens","missing chemical geometry information.");
    } else {
      ObjectMoleculeUpdateNeighbors(I);
      ai0=I->AtomInfo;
      for(a=0;a<I->NAtom;a++) {
        if(!ai0->hydrogen) { /* only do heavies */
          if(SelectorIsMember(I->Obj.G,ai0->selEntry,sele)) {
            n = I->Neighbor[a]+1;
            while( (h_idx=I->Neighbor[n]) >=0) {
              ai1=I->AtomInfo + h_idx;
              if(ai1->hydrogen) {
                for(b=0;b<I->NCSet;b++) { /* iterate through each coordinate set */
                  if(ObjectMoleculeGetAtomVertex(I,b,a,v0) &&
                     ObjectMoleculeGetAtomVertex(I,b,h_idx,v1)) { /* current direction */
                    float l;
                    subtract3f(v1,v0,sought);
                    l = length3f(sought); /* use the current length */

                    if(ObjectMoleculeFindOpenValenceVector(I,b,a,fixed,
                                                           sought,h_idx)) {
                      scale3f(fixed,l,fixed);
                      add3f(fixed,v0,fixed);
                      ObjectMoleculeSetAtomVertex(I,b,h_idx,fixed);
                      seleFlag=true;
                    }
                  }
                  cs = I->CSet[b];
                }
              }
              n+=2;
            }
          }
        }
        ai0++;
      }
    }
    if(seleFlag)
      ObjectMoleculeInvalidate(I,cRepAll,cRepInvAll,-1);
  }
}

static char *skip_fortran(int num,int per_line,char *p)
{
  int a,b;
  b=0;
  for(a=0;a<num;a++) {
    if((++b)==per_line) {
      b=0;
      p=nextline(p);
    }
  }  
  if(b||(!num)) p=nextline(p);
  return(p);
}

void M4XAnnoInit(M4XAnnoType *m4x)
{
  UtilZeroMem((char*)m4x,sizeof(M4XAnnoType));
}

void M4XAnnoPurge(M4XAnnoType *m4x)
{
  int c;
  if(m4x) {
    for(c=0;c<m4x->n_context;c++) {
      VLAFreeP(m4x->context[c].hbond);
      VLAFreeP(m4x->context[c].nbond);
      VLAFreeP(m4x->context[c].site);
      VLAFreeP(m4x->context[c].ligand);
      VLAFreeP(m4x->context[c].water);
    }
    if(m4x->align) {
      M4XAlignPurge(m4x->align);
    }
    VLAFreeP(m4x->context);
  }
}

void M4XAlignInit(M4XAlignType *align)
{
  UtilZeroMem((char*)align,sizeof(M4XAlignType));
  align->id_at_point = VLACalloc(int,100);
  align->fitness = VLAlloc(float,100);
}

void M4XAlignPurge(M4XAlignType *align)
{
  VLAFreeP(align->id_at_point);
  VLAFreeP(align->fitness);
  FreeP(align);
}

void ObjectMoleculeOpRecInit(ObjectMoleculeOpRec *op)
{
  UtilZeroMem((char*)op,sizeof(ObjectMoleculeOpRec));
}

int ObjectMoleculeGetTopNeighbor(PyMOLGlobals *G,
                                 ObjectMolecule *I, 
                                 int start, int excluded)
{
  /* returns the most proton-rich element with the lowest priority value
   (OG1 before OG2, CG, HB1) */

  int n0, at;
  AtomInfoType *ai;
  int highest_at = -1, highest_prot=0, lowest_pri=9999;

  ObjectMoleculeUpdateNeighbors(I);    
  n0 = I->Neighbor[start]+1;
  while(I->Neighbor[n0]>=0) {
    ai = I->AtomInfo + (at = I->Neighbor[n0]);
    if((highest_at<0)&&(at!=excluded)) {
      highest_prot = ai->protons;
      lowest_pri = ai->priority;
      highest_at = at;
    } else if(((ai->protons>highest_prot)||
               ((ai->protons==highest_prot)&&(ai->priority<lowest_pri)))
              &&(at!=excluded)) {
      highest_prot = ai->protons;
      highest_at = at;
      lowest_pri = ai->priority;
    }
    n0+=2;
  }
  return highest_at;
}

/*========================================================================*/
ObjectMolecule *ObjectMoleculeLoadTRJFile(PyMOLGlobals *G,ObjectMolecule *I,char *fname,int frame,
                                          int interval,int average,int start,
                                          int stop,int max,char *sele,int image,
                                          float *shift,int quiet)
{
  int ok=true;
  FILE *f;
  char *buffer,*p;
  char cc[MAXLINELEN];  
  int n_read;
  int to_go;
  int skip_first_line = true;
  int periodic=false;
  int angles=true;
  float f0,f1,f2,*fp;
  float box[3],angle[3];
  float r_cent[3],r_trans[3];
  int r_act,r_val,r_cnt;
  float *r_fp_start=NULL,*r_fp_stop=NULL;
  int a,b,c,i;
  int *to,at_i;
  int zoom_flag=false;
  int cnt=0;
  int n_avg=0;
  int icnt;
  int ncnt=0;
  int sele0 = SelectorIndexByName(G,sele);
  int *xref = NULL;
  float zerovector[3]={0.0,0.0,0.0};
  CoordSet *cs = NULL;

  if(!shift)
    shift = zerovector;
  if(interval<1)
    interval=1;

  icnt=interval;
  #define BUFSIZE 4194304
  #define GETTING_LOW 10000

  f=fopen(fname,"rb");
  if(!f) {
	 ok=ErrMessage(G,"ObjectMoleculeLoadTRJFile","Unable to open file!");
  } else
	 {
      if(!I->CSTmpl) {
        PRINTFB(G,FB_Errors,FB_ObjectMolecule)
          " ObjMolLoadTRJFile: Missing topology"
          ENDFB(G);
        return(I);
      }
      cs=CoordSetCopy(I->CSTmpl);

      if(sele0>=0) { /* build array of cross-references */
        xref = Alloc(int,I->NAtom);
        c=0;
        for(a=0;a<I->NAtom;a++) {
          if(SelectorIsMember(G,I->AtomInfo[a].selEntry,sele0)) {
            xref[a]=c++;
          } else {
            xref[a]=-1;
          }
        }

        for(a=0;a<I->NAtom;a++) { /* now terminate the excluded references */
          if(xref[a]<0) {
            cs->AtmToIdx[a]=-1;
          } 
        }

        to=cs->IdxToAtm;
        c=0;
        for(a=0;a<cs->NIndex;a++) { /* now fix IdxToAtm, AtmToIdx,
                                       and remap xref to coordinate space */         
          at_i = cs->IdxToAtm[a];
          if(cs->AtmToIdx[at_i]>=0) {
            *(to++)=at_i;
            cs->AtmToIdx[at_i]=c;
            xref[a]=c;
            c++;
          } else {
            xref[a]=-1;
          }
        }

        cs->NIndex=c;
        cs->IdxToAtm = Realloc(cs->IdxToAtm,int,cs->NIndex+1);
        VLASize(cs->Coord,float,cs->NIndex*3);
      }
      PRINTFB(G,FB_ObjectMolecule,FB_Blather) 
        " ObjMolLoadTRJFile: Loading from \"%s\".\n",fname
        ENDFB(G);
      buffer = (char*)mmalloc(BUFSIZE+1); /* 1 MB read buffer */
      p = buffer;
      buffer[0]=0;
      n_read = 0;
      to_go=0;
      a = 0;
      b = 0;
      c = 0;
      f1=0.0;
      f2=0.0;
      while(1)
        {
          to_go = n_read-(p-buffer);
          if(to_go<GETTING_LOW) 
            if(!feof(f)) {
              if(to_go) 
                memcpy(buffer,p,to_go);
              n_read = fread(buffer+to_go,1,BUFSIZE-to_go,f);              
              n_read = to_go + n_read;
              buffer[n_read]=0;
              p = buffer;
              if(skip_first_line) {
                p=nextline(p);
                skip_first_line=false;
              }
              to_go = n_read-(p-buffer);
            }
          if(!to_go) break;
          p=ncopy(cc,p,8);
          if((++b)==10) {
            b=0;
            p=nextline(p);
          }
          f0 = f1;
          f1 = f2;
          if(sscanf(cc,"%f",&f2)==1) {
            if((++c)==3) {
              c=0;
              if((cnt+1)>=start) {
                if(icnt<=1) {
                  if(xref) { 
                    if(xref[a]>=0)
                      fp=cs->Coord+3*xref[a];
                    else 
                      fp=NULL;
                  } else {
                    fp=cs->Coord+3*a;
                  }
                  if(fp) {
                    if(n_avg) {
                      *(fp++)+=f0;
                      *(fp++)+=f1;
                      *(fp++)+=f2;
                    } else {
                      *(fp++)=f0;
                      *(fp++)=f1;
                      *(fp++)=f2;
                    }
                  }
                }
              }
              if((++a)==I->NAtom) {
                
                cnt++;
                a=0;
                if(b) p=nextline(p);
                b=0;


                if(cs->PeriodicBoxType!=cCSet_NoPeriodicity) {
                  /* read periodic box */

                  c=0;
                  periodic=true;
                  angles=true;
                  
                  p = ncopy(cc,p,8);
                  if(sscanf(cc,"%f",&box[0])!=1) 
                    periodic=false;
                  p = ncopy(cc,p,8);
                  if(sscanf(cc,"%f",&box[1])!=1)
                    periodic=false;
                  p = ncopy(cc,p,8);
                  if(sscanf(cc,"%f",&box[2])!=1)
                    periodic=false;
                  
                  p = ncopy(cc,p,8);
                  if(sscanf(cc,"%f",&angle[0])!=1)
                    angles=false;
                  
                  p = ncopy(cc,p,8);
                  if(sscanf(cc,"%f",&angle[1])!=1)
                    angles=false;
                  
                  p = ncopy(cc,p,8);
                  if(sscanf(cc,"%f",&angle[2])!=1)
                    angles=false;
                  if(periodic) {
                    if(!cs->PeriodicBox)
                      cs->PeriodicBox=CrystalNew(G);
                    cs->PeriodicBox->Dim[0] = box[0];
                    cs->PeriodicBox->Dim[1] = box[1];
                    cs->PeriodicBox->Dim[2] = box[2];
                    if(angles) {
                      cs->PeriodicBox->Angle[0] = angle[0];
                      cs->PeriodicBox->Angle[1] = angle[1];
                      cs->PeriodicBox->Angle[2] = angle[2];
                    } 
                    CrystalUpdate(cs->PeriodicBox);
                    /*                    CrystalDump(cs->PeriodicBox);*/
                    p=nextline(p);
                    b=0;
                  }

                  if(cs->PeriodicBoxType==cCSet_Octahedral)
                    periodic=false; /* can't handle this yet... */
                }
                
                if((stop>0)&&(cnt>=stop))
                  break;
                if(cnt>=start) {
                  icnt--;                      
                  if(icnt>0) {
                    PRINTFB(G,FB_Details,FB_ObjectMolecule)
                      " ObjectMolecule: skipping set %d...\n",cnt
                      ENDFB(G);
                  } else {
                    icnt=interval;
                    n_avg++;
                  }
                  
                  if(icnt==interval) {
                    if(n_avg<average) {
                      PRINTFB(G,FB_Details,FB_ObjectMolecule)
                        " ObjectMolecule: averaging set %d...\n",cnt
                        ENDFB(G);
                    } else {
                      
                      /* compute average */
                      if(n_avg>1) {
                        fp=cs->Coord;
                        for(i=0;i<cs->NIndex;i++) {
                          *(fp++)/=n_avg;
                          *(fp++)/=n_avg;
                          *(fp++)/=n_avg;
                        }
                      }
                      if(periodic&&image) { /* Perform residue-based period image transformation */
                        i = 0;
                        r_cnt = 0;
                        r_act = 0; /* 0 unspec, 1=load, 2=image, 3=leave*/
                        r_val = -1;
                        while(r_act!=3) {
                          if(i>=cs->NIndex) {
                            if(r_cnt)
                              r_act = 2; 
                            else
                              r_act = 3;
                          }
                          if(r_act==0) {
                            /* start new residue */
                            r_cnt = 0;
                            r_act = 1; /* now load */
                          }
                          if(r_act==1) {
                            if(i<cs->NIndex) {
                              
                              /* is there a coordinate for atom? */
                              if(xref) { 
                                if(xref[i]>=0)
                                  fp=cs->Coord+3*xref[i];
                                else 
                                  fp=NULL;
                              } else {
                                fp=cs->Coord+3*i;
                              }
                              if(fp) { /* yes there is... */
                                if(r_cnt) {
                                  if(r_val!=I->AtomInfo[cs->IdxToAtm[i]].resv) {
                                    r_act=2; /* end of residue-> time to image */
                                  } else {
                                    r_cnt++;
                                    r_cent[0]+=*(fp++);
                                    r_cent[1]+=*(fp++);
                                    r_cent[2]+=*(fp++);
                                    r_fp_stop = fp; /* stop here */
                                    i++;
                                  }
                                } else {
                                  r_val = I->AtomInfo[cs->IdxToAtm[i]].resv;
                                  r_cnt++;
                                  r_fp_start = fp; /* start here */
                                  r_cent[0]=*(fp++);
                                  r_cent[1]=*(fp++);
                                  r_cent[2]=*(fp++);
                                  r_fp_stop = fp; /* stop here */
                                  i++;
                                }
                              } else {
                                i++;
                              }
                            } else {
                              r_act=2; /* image */
                            }
                          }

                          if(r_act==2) { /* time to image */
                            if(r_cnt) {
                              r_cent[0]/=r_cnt;
                              r_cent[1]/=r_cnt;
                              r_cent[2]/=r_cnt;
                              transform33f3f(cs->PeriodicBox->RealToFrac,r_cent,r_cent);
                              r_trans[0]=(float)fmod(1000.0+shift[0]+r_cent[0],1.0F);
                              r_trans[1]=(float)fmod(1000.0+shift[1]+r_cent[1],1.0F);
                              r_trans[2]=(float)fmod(1000.0+shift[2]+r_cent[2],1.0F);
                              r_trans[0]-=r_cent[0];
                              r_trans[1]-=r_cent[1];
                              r_trans[2]-=r_cent[2];
                              transform33f3f(cs->PeriodicBox->FracToReal,r_trans,r_trans);
                              fp=r_fp_start;
                              while(fp<r_fp_stop) {
                                *(fp++)+=r_trans[0];
                                *(fp++)+=r_trans[1];
                                *(fp++)+=r_trans[2];
                              }
                            }
                            r_act=0; /* reset */ 
                            r_cnt=0;
                          }
                        }
                      }

                      /* add new coord set */
                      if(cs->fInvalidateRep)
                        cs->fInvalidateRep(cs,cRepAll,cRepInvRep);
                      if(frame<0) frame=I->NCSet;
                      if(!I->NCSet) {
                        zoom_flag=true;
                      }
                      
                      VLACheck(I->CSet,CoordSet*,frame);
                      if(I->NCSet<=frame) I->NCSet=frame+1;
                      if(I->CSet[frame]) I->CSet[frame]->fFree(I->CSet[frame]);
                      I->CSet[frame] = cs;
                      ncnt++;
                      
                      if(average<2) {
                        PRINTFB(G,FB_Details,FB_ObjectMolecule)
                          " ObjectMolecule: read set %d into state %d...\n",cnt,frame+1
                          ENDFB(G);
                      } else {
                        PRINTFB(G,FB_Details,FB_ObjectMolecule)
                          " ObjectMolecule: averaging set %d...\n",cnt
                          ENDFB(G);
                        PRINTFB(G,FB_Details,FB_ObjectMolecule)
                          " ObjectMolecule: average loaded into state %d...\n",frame+1
                          ENDFB(G);
                      }
                      frame++;
                      cs = CoordSetCopy(cs);
                      n_avg=0;
                      if((stop>0)&&(cnt>=stop))
                        break;
                      if((max>0)&&(ncnt>=max))
                        break;
                    }
                  }
                } else {
                  PRINTFB(G,FB_Details,FB_ObjectMolecule)
                    " ObjectMolecule: skipping set %d...\n",cnt
                    ENDFB(G);
                }
              }
            }
          } else {
            PRINTFB(G,FB_Errors,FB_ObjectMolecule)
              " ObjMolLoadTRJFile-Error: Failed to read an expected coordinate value.\n    This trajectory does not match the loaded parameter/topology file.\n    Likely cause: either the atom count or the periodic box settings\n    are inconsistent between the two files.\n"
              ENDFB(G);
            break;
          }
        }
      FreeP(xref);
		mfree(buffer);
	 }
  if(cs)
    cs->fFree(cs);
  SceneChanged(G);
  SceneCountFrames(G);
  if(zoom_flag) 
    if(SettingGet(G,cSetting_auto_zoom)) {
      ExecutiveWindowZoom(G,I->Obj.Name,0.0,-1,0,0,quiet); /* auto zoom (all states) */
    }
  
  return(I);
}
ObjectMolecule *ObjectMoleculeLoadRSTFile(PyMOLGlobals *G,ObjectMolecule *I,
                                          char *fname,int frame,int quiet)

{
  int ok=true;
  FILE *f;
  char *buffer,*p;
  char cc[MAXLINELEN];  
  float f0,f1,f2,*fp;
  int a,b,c;
  int zoom_flag=false;
  CoordSet *cs = NULL;
  int size;

  #define BUFSIZE 4194304
  #define GETTING_LOW 10000

  f=fopen(fname,"rb");
  if(!f)
	 ok=ErrMessage(G,"ObjectMoleculeLoadRSTFile","Unable to open file!");
  else
	 {
      if(!I->CSTmpl) {
        PRINTFB(G,FB_Errors,FB_ObjectMolecule)
          " ObjMolLoadRSTFile: Missing topology"
          ENDFB(G);
        return(I);
      }
      cs=CoordSetCopy(I->CSTmpl);
      PRINTFB(G,FB_ObjectMolecule,FB_Blather) 
        " ObjMolLoadRSTFile: Loading from \"%s\".\n",fname
        ENDFB(G);


		fseek(f,0,SEEK_END);
      size=ftell(f);
		fseek(f,0,SEEK_SET);

		buffer=(char*)mmalloc(size+255);
		ErrChkPtr(G,buffer);
		p=buffer;
		fseek(f,0,SEEK_SET);
		fread(p,size,1,f);
		p[size]=0;
		fclose(f);

      p=nextline(p);
      p=nextline(p);

      a = 0;
      b = 0;
      c = 0;
      f1=0.0;
      f2=0.0;
      while(*p)
        {
          p=ncopy(cc,p,12);
          if((++b)==6) {
            b=0;
            p=nextline(p);
          }
          f0 = f1;
          f1 = f2;
          if(sscanf(cc,"%f",&f2)==1) {
            if((++c)==3) {
              c=0;
              fp=cs->Coord+3*a;
              *(fp++)=f0;
              *(fp++)=f1;
              *(fp++)=f2;
              
              if((++a)==I->NAtom) {
                a=0;
                if(b) p=nextline(p);
                b=0;
                /* add new coord set */
                if(cs->fInvalidateRep)
                  cs->fInvalidateRep(cs,cRepAll,cRepInvRep);
                if(frame<0) frame=I->NCSet;
                if(!I->NCSet) {
                  zoom_flag=true;
                }
                
                VLACheck(I->CSet,CoordSet*,frame);
                if(I->NCSet<=frame) I->NCSet=frame+1;
                if(I->CSet[frame]) I->CSet[frame]->fFree(I->CSet[frame]);
                I->CSet[frame] = cs;
                
                PRINTFB(G,FB_Details,FB_ObjectMolecule)
                  " ObjectMolecule: read coordinates into state %d...\n",frame+1
                  ENDFB(G);
               
                cs = CoordSetCopy(cs);
                break;
              }
            }
          } else {
            PRINTFB(G,FB_Errors,FB_ObjectMolecule)
              " ObjMolLoadRSTFile: atom/coordinate mismatch.\n"
              ENDFB(G);
            break;
          }
        }
		mfree(buffer);
	 }
  if(cs)
    cs->fFree(cs);
  
  SceneChanged(G);
  SceneCountFrames(G);
  if(zoom_flag) 
    if(SettingGet(G,cSetting_auto_zoom)) {
      ExecutiveWindowZoom(G,I->Obj.Name,0.0,-1,0,0,quiet); /* auto zoom (all states) */
    }
  
  return(I);
}
static char *findflag(PyMOLGlobals *G,char *p,char *flag,char *format)
{

  char cc[MAXLINELEN];
  char pat[MAXLINELEN] = "%FLAG ";
  int l;

  PRINTFD(G,FB_ObjectMolecule)
    " findflag: flag %s format %s\n",flag,format
    ENDFD;

  strcat(pat,flag);
  l=strlen(pat);
  while(*p) {
    p=ncopy(cc,p,l);
    if(WordMatch(G,cc,pat,true)<0) {
      p=nextline(p);
      break;
    }
    p=nextline(p);
    if(!*p) {
      PRINTFB(G,FB_ObjectMolecule,FB_Errors)
        " ObjectMolecule-Error: Unrecognized file format (can't find \"%s\").\n",pat
        ENDFB(G);
    }
  }

  strcpy(pat,"%FORMAT(");
  strcat(pat,format);
  strcat(pat,")");
  l=strlen(pat);
  while(*p) {
    p=ncopy(cc,p,l);
    if(WordMatch(G,cc,pat,true)<0) {
      p=nextline(p);
      break; 
    }
    p=nextline(p);
    if(!*p) {
      PRINTFB(G,FB_ObjectMolecule,FB_Errors)
        " ObjectMolecule-Error: Unrecognized file format (can't find \"%s\").\n",pat
        ENDFB(G);
    }
      
  }
  return(p);
}

/*========================================================================*/
CoordSet *ObjectMoleculeTOPStr2CoordSet(PyMOLGlobals *G,char *buffer,
                                        AtomInfoType **atInfoPtr)
{
  char *p;
  int nAtom;
  int a,b,c,bi,last_i,at_i,aa,rc;
  float *coord = NULL;
  float *f;
  CoordSet *cset = NULL;
  AtomInfoType *atInfo = NULL,*ai;
  BondType *bond=NULL,*bd;
  int nBond=0;
  int auto_show_lines = (int)SettingGet(G,cSetting_auto_show_lines);
  int auto_show_spheres = (int)SettingGet(G,cSetting_auto_show_spheres);
  int auto_show_nonbonded = (int)SettingGet(G,cSetting_auto_show_nonbonded);
  int amber7 = false;

  WordType title;
  ResName *resn;

  char cc[MAXLINELEN];
  int ok=true;
  int i0,i1,i2;

  /* trajectory parameters */

  int NTYPES,NBONH,MBONA,NTHETH,MTHETA;
  int NPHIH,MPHIA,NHPARM,NPARM,NNB,NRES;
  int NBONA,NTHETA,NPHIA,NUMBND,NUMANG,NPTRA;
  int NATYP,NPHB,IFPERT,NBPER,NGPER,NDPER;
  int MBPER,MGPER,MDPER,IFBOX,NMXRS,IFCAP;
  int NEXTRA,IPOL=0;
  int wid,col;
  float BETA;
  float BOX1,BOX2,BOX3;

  cset = CoordSetNew(G);  

  p=buffer;
  nAtom=0;
  if(atInfoPtr)
	 atInfo = *atInfoPtr;
  if(!atInfo)
    ErrFatal(G,"TOPStr2CoordSet","need atom information record!");
  /* failsafe for old version..*/

  ncopy(cc,p,8);
  if(strcmp(cc,"%VERSION")==0) {
    amber7=true;
    PRINTFB(G,FB_ObjectMolecule,FB_Details)
      " ObjectMolecule: Attempting to read Amber7 topology file.\n"
      ENDFB(G);
  } else {
    PRINTFB(G,FB_ObjectMolecule,FB_Details)
      " ObjectMolecule: Assuming this is an Amber6 topology file.\n"
      ENDFB(G);
  }
  
  /* read title */
  if(amber7) {
    p = findflag(G,buffer,"TITLE","20a4");
  }

  p=ncopy(cc,p,20);
  title[0]=0;
  sscanf(cc,"%s",title);
  p=nextline(p);

  if(amber7) {

    p = findflag(G,buffer,"POINTERS","10I8");

    p=ncopy(cc,p,8); ok = ok && (sscanf(cc,"%d",&nAtom)==1);
    p=ncopy(cc,p,8); ok = ok && (sscanf(cc,"%d",&NTYPES)==1);
    p=ncopy(cc,p,8); ok = ok && (sscanf(cc,"%d",&NBONH)==1);
    p=ncopy(cc,p,8); ok = ok && (sscanf(cc,"%d",&MBONA)==1);
    p=ncopy(cc,p,8); ok = ok && (sscanf(cc,"%d",&NTHETH)==1);

    p=ncopy(cc,p,8); ok = ok && (sscanf(cc,"%d",&MTHETA)==1);
    p=ncopy(cc,p,8); ok = ok && (sscanf(cc,"%d",&NPHIH)==1);
    p=ncopy(cc,p,8); ok = ok && (sscanf(cc,"%d",&MPHIA)==1);
    p=ncopy(cc,p,8); ok = ok && (sscanf(cc,"%d",&NHPARM)==1);
    p=ncopy(cc,p,8); ok = ok && (sscanf(cc,"%d",&NPARM)==1);

    p=nextline(p);

    p=ncopy(cc,p,8); ok = ok && (sscanf(cc,"%d",&NNB)==1);
    p=ncopy(cc,p,8); ok = ok && (sscanf(cc,"%d",&NRES)==1);
    p=ncopy(cc,p,8); ok = ok && (sscanf(cc,"%d",&NBONA)==1);
    p=ncopy(cc,p,8); ok = ok && (sscanf(cc,"%d",&NTHETA)==1);
    p=ncopy(cc,p,8); ok = ok && (sscanf(cc,"%d",&NPHIA)==1);

    p=ncopy(cc,p,8); ok = ok && (sscanf(cc,"%d",&NUMBND)==1);
    p=ncopy(cc,p,8); ok = ok && (sscanf(cc,"%d",&NUMANG)==1);
    p=ncopy(cc,p,8); ok = ok && (sscanf(cc,"%d",&NPTRA)==1);
    p=ncopy(cc,p,8); ok = ok && (sscanf(cc,"%d",&NATYP)==1);
    p=ncopy(cc,p,8); ok = ok && (sscanf(cc,"%d",&NPHB)==1);

    p=nextline(p);

    p=ncopy(cc,p,8); ok = ok && (sscanf(cc,"%d",&IFPERT)==1);
    p=ncopy(cc,p,8); ok = ok && (sscanf(cc,"%d",&NBPER)==1);
    p=ncopy(cc,p,8); ok = ok && (sscanf(cc,"%d",&NGPER)==1);
    p=ncopy(cc,p,8); ok = ok && (sscanf(cc,"%d",&NDPER)==1);
    p=ncopy(cc,p,8); ok = ok && (sscanf(cc,"%d",&MBPER)==1);

    p=ncopy(cc,p,8); ok = ok && (sscanf(cc,"%d",&MGPER)==1);
    p=ncopy(cc,p,8); ok = ok && (sscanf(cc,"%d",&MDPER)==1);
    p=ncopy(cc,p,8); ok = ok && (sscanf(cc,"%d",&IFBOX)==1);
    p=ncopy(cc,p,8); ok = ok && (sscanf(cc,"%d",&NMXRS)==1);
    p=ncopy(cc,p,8); ok = ok && (sscanf(cc,"%d",&IFCAP)==1);
    
    p=nextline(p);
    p=ncopy(cc,p,8); ok = ok && (sscanf(cc,"%d",&NEXTRA)==1);

  } else {
    
    p=ncopy(cc,p,6); ok = ok && (sscanf(cc,"%d",&nAtom)==1);
    p=ncopy(cc,p,6); ok = ok && (sscanf(cc,"%d",&NTYPES)==1);
    p=ncopy(cc,p,6); ok = ok && (sscanf(cc,"%d",&NBONH)==1);
    p=ncopy(cc,p,6); ok = ok && (sscanf(cc,"%d",&MBONA)==1);
    p=ncopy(cc,p,6); ok = ok && (sscanf(cc,"%d",&NTHETH)==1);
    p=ncopy(cc,p,6); ok = ok && (sscanf(cc,"%d",&MTHETA)==1);
    
    p=ncopy(cc,p,6); ok = ok && (sscanf(cc,"%d",&NPHIH)==1);
    p=ncopy(cc,p,6); ok = ok && (sscanf(cc,"%d",&MPHIA)==1);
    p=ncopy(cc,p,6); ok = ok && (sscanf(cc,"%d",&NHPARM)==1);
    p=ncopy(cc,p,6); ok = ok && (sscanf(cc,"%d",&NPARM)==1);
    p=ncopy(cc,p,6); ok = ok && (sscanf(cc,"%d",&NNB)==1);
    p=ncopy(cc,p,6); ok = ok && (sscanf(cc,"%d",&NRES)==1);
    
    p=nextline(p);
    
    p=ncopy(cc,p,6); ok = ok && (sscanf(cc,"%d",&NBONA)==1);
    p=ncopy(cc,p,6); ok = ok && (sscanf(cc,"%d",&NTHETA)==1);
    p=ncopy(cc,p,6); ok = ok && (sscanf(cc,"%d",&NPHIA)==1);
    p=ncopy(cc,p,6); ok = ok && (sscanf(cc,"%d",&NUMBND)==1);
    p=ncopy(cc,p,6); ok = ok && (sscanf(cc,"%d",&NUMANG)==1);
    p=ncopy(cc,p,6); ok = ok && (sscanf(cc,"%d",&NPTRA)==1);
    
    p=ncopy(cc,p,6); ok = ok && (sscanf(cc,"%d",&NATYP)==1);
    p=ncopy(cc,p,6); ok = ok && (sscanf(cc,"%d",&NPHB)==1);
    p=ncopy(cc,p,6); ok = ok && (sscanf(cc,"%d",&IFPERT)==1);
    p=ncopy(cc,p,6); ok = ok && (sscanf(cc,"%d",&NBPER)==1);
    p=ncopy(cc,p,6); ok = ok && (sscanf(cc,"%d",&NGPER)==1);
    p=ncopy(cc,p,6); ok = ok && (sscanf(cc,"%d",&NDPER)==1);
    
    p=nextline(p);
    
    p=ncopy(cc,p,6); ok = ok && (sscanf(cc,"%d",&MBPER)==1);
    p=ncopy(cc,p,6); ok = ok && (sscanf(cc,"%d",&MGPER)==1);
    p=ncopy(cc,p,6); ok = ok && (sscanf(cc,"%d",&MDPER)==1);
    p=ncopy(cc,p,6); ok = ok && (sscanf(cc,"%d",&IFBOX)==1);
    p=ncopy(cc,p,6); ok = ok && (sscanf(cc,"%d",&NMXRS)==1);
    p=ncopy(cc,p,6); ok = ok && (sscanf(cc,"%d",&IFCAP)==1);
    
    p=ncopy(cc,p,6); if(sscanf(cc,"%d",&NEXTRA)!=1) NEXTRA=0;

  }

  switch(IFBOX) {
  case 2:
    cset->PeriodicBoxType = cCSet_Octahedral;
    PRINTFB(G,FB_ObjectMolecule,FB_Details)
      " TOPStrToCoordSet: Warning: can't currently image a truncated octahedron...\n"
      ENDFB(G);
    break;
  case 1:
    cset->PeriodicBoxType = cCSet_Orthogonal;
    break;
  case 0:
  default:
    cset->PeriodicBoxType = cCSet_NoPeriodicity;
    break;
  }

  p=nextline(p);

  if(!ok) {
    ErrMessage(G,"TOPStrToCoordSet","Error reading counts lines");
  } else {
    PRINTFB(G,FB_ObjectMolecule,FB_Blather)
      " TOPStr2CoordSet: read counts line nAtom %d NBONA %d NBONH %d\n",
      nAtom,NBONA,NBONH
      ENDFB(G);
  }

  if(ok) {  
    VLACheck(atInfo,AtomInfoType,nAtom);

    if(amber7) {
      p = findflag(G,buffer,"ATOM_NAME","20a4");
    }
    /* read atoms */

    b=0;
    for(a=0;a<nAtom;a++) {
      p=ncopy(cc,p,4);
      ai=atInfo+a;
      if(!sscanf(cc,"%s",ai->name))
        ai->name[0]=0;
      if((++b)==20) {
        b=0;
        p=nextline(p);
      }
    }
  
    if(b) p=nextline(p);

    if(!ok) {
      ErrMessage(G,"TOPStrToCoordSet","Error reading atom names");
    } else {
      PRINTFB(G,FB_ObjectMolecule,FB_Blather)
        " TOPStr2CoordSet: read atom names.\n"
        ENDFB(G);
    }

    /* read charges */

    if(amber7) {
      p = findflag(G,buffer,"CHARGE","5E16.8");
    }

    b=0;
    for(a=0;a<nAtom;a++) {
      p=ncopy(cc,p,16);
      ai=atInfo+a;
      if(!sscanf(cc,"%f",&ai->partialCharge))
        ok=false;
      else {
        ai->partialCharge/=18.2223F; /* convert to electron charge */
      }
      if((++b)==5) {
        b=0;
        p=nextline(p);
      }
    }

    if(!ok) {
      ErrMessage(G,"TOPStrToCoordSet","Error reading charges");
    } else {
      PRINTFB(G,FB_ObjectMolecule,FB_Blather)
        " TOPStr2CoordSet: read charges.\n"
        ENDFB(G);
    }
    if(b) p=nextline(p);
  
    if(!amber7) {
      /* skip masses */
      
      p=skip_fortran(nAtom,5,p);
    }

    /* read LJ atom types */

    if(amber7) {
      p = findflag(G,buffer,"ATOM_TYPE_INDEX","10I8");
      col=10;
      wid=8;
    } else {
      col=12;
      wid=6;
    }

    b=0;
    for(a=0;a<nAtom;a++) {
      p=ncopy(cc,p,wid);
      ai=atInfo+a;
      if(!sscanf(cc,"%d",&ai->customType))
        ok=false;
      if((++b)==col) {
        b=0;
        p=nextline(p);
      }
    }
    if(b) p=nextline(p);

    if(!ok) {
      ErrMessage(G,"TOPStrToCoordSet","Error LJ atom types");
    } else {
      PRINTFB(G,FB_ObjectMolecule,FB_Blather)
        " TOPStr2CoordSet: read LJ atom types.\n"
        ENDFB(G);
    }

    if(!amber7) {
      /* skip excluded atom counts */
      
      p=skip_fortran(nAtom,12,p);
      
      /* skip NB param arrays */
      
      p=skip_fortran(NTYPES*NTYPES,12,p);
    }

    /* read residue labels */

    if(amber7) {
      p = findflag(G,buffer,"RESIDUE_LABEL","20a4");
    }

    resn = Alloc(ResName,NRES);

    b=0;
    for(a=0;a<NRES;a++) {
      p=ncopy(cc,p,4);
      if(!sscanf(cc,"%s",resn[a]))
        resn[a][0]=0;
      if((++b)==20) {
        b=0;
        p=nextline(p);
      }
    }
    if(b) p=nextline(p);

    if(!ok) {
      ErrMessage(G,"TOPStrToCoordSet","Error reading residue labels");
    } else {
      PRINTFB(G,FB_ObjectMolecule,FB_Blather)
        " TOPStr2CoordSet: read residue labels.\n"
        ENDFB(G);
    }

    /* read residue assignments */

    if(amber7) {
      p = findflag(G,buffer,"RESIDUE_POINTER","10I8");
      col=10;
      wid=8;
    } else {
      col=12;
      wid=6;
    }

    b=0;
    last_i=0;
    rc=0;
    for(a=0;a<NRES;a++) {
      p=ncopy(cc,p,wid);
      if(sscanf(cc,"%d",&at_i))
        {
          if(last_i)
            for(aa=(last_i-1);aa<(at_i-1);aa++) {
              ai = atInfo+aa;
              strcpy(ai->resn,resn[a-1]);
              ai->resv=rc;
              sprintf(ai->resi,"%d",rc);
            }
          rc++;
          last_i=at_i;
        }
      if((++b)==col) {
        b=0;
        p=nextline(p);
      }
    }
    if(b) p=nextline(p);
    if(last_i)
      for(aa=(last_i-1);aa<nAtom;aa++) {
        ai = atInfo+aa;
        strcpy(ai->resn,resn[NRES-1]);
        ai->resv=rc;
        sprintf(ai->resi,"%d",rc);
      }
    rc++;

    if(!ok) {
      ErrMessage(G,"TOPStrToCoordSet","Error reading residues");
    } else {
      PRINTFB(G,FB_ObjectMolecule,FB_Blather)
        " TOPStr2CoordSet: read residues.\n"
        ENDFB(G);
    }

    FreeP(resn);

    if(!amber7) {  
      /* skip bond force constants */

      p=skip_fortran(NUMBND,5,p);
      
      /* skip bond lengths */

      p=skip_fortran(NUMBND,5,p);
      
      /* skip angle force constant */
      
      p=skip_fortran(NUMANG,5,p);
      
      /* skip angle eq */
      
      p=skip_fortran(NUMANG,5,p);
      
      /* skip dihedral force constant */
      
      p=skip_fortran(NPTRA,5,p);
      
      /* skip dihedral periodicity */
      
      p=skip_fortran(NPTRA,5,p);
      
      /* skip dihedral phases */
      
      p=skip_fortran(NPTRA,5,p);
      
      /* skip SOLTYs */
      
      p=skip_fortran(NATYP,5,p);
      
      /* skip LJ terms r12 */
      
      p=skip_fortran((NTYPES*(NTYPES+1))/2,5,p);
      
      /* skip LJ terms r6 */
      
      p=skip_fortran((NTYPES*(NTYPES+1))/2,5,p);
      
    }

    /* read bonds */

    if(amber7) {
      p = findflag(G,buffer,"BONDS_INC_HYDROGEN","10I8");
      col=10;
      wid=8;
    } else {
      col=12;
      wid=6;
    }
    
    nBond = NBONH + NBONA;

    bond=VLACalloc(BondType,nBond);
  
    bi = 0;
  

    b=0;
    c=0;
    i0=0;
    i1=0;
    for(a=0;a<3*NBONH;a++) {
      p=ncopy(cc,p,wid);
      i2=i1;
      i1=i0;
      if(!sscanf(cc,"%d",&i0))
        ok=false;
      if((++c)==3) {
        c=0;
        bd=bond+bi;
        bd->index[0]=(abs(i2)/3);
        bd->index[1]=(abs(i1)/3);
        bd->order=1;
        bd->stereo=0;
        bd->id = bi+1;
        bi++;
      }
      if((++b)==col) {
        b=0;
        p=nextline(p);
      }
    }
    if(b) p=nextline(p);

    if(!ok) {
      ErrMessage(G,"TOPStrToCoordSet","Error hydrogen containing bonds");
    } else {
      PRINTFB(G,FB_ObjectMolecule,FB_Blather)
        " TOPStr2CoordSet: read %d hydrogen containing bonds.\n",NBONH
        ENDFB(G);
    }

    if(amber7) {
      p = findflag(G,buffer,"BONDS_WITHOUT_HYDROGEN","10I8");
      col=10;
      wid=8;
    } else {
      col=12;
      wid=6;
    }

    b=0;
    c=0;
    for(a=0;a<3*NBONA;a++) {
      p=ncopy(cc,p,wid);
      i2=i1;
      i1=i0;
      if(!sscanf(cc,"%d",&i0))
        ok=false;
      if((++c)==3) {
        c=0;
        bd=bond+bi;
        bd->index[0]=(abs(i2)/3);
        bd->index[1]=(abs(i1)/3);
        bd->order=0;
        bd->stereo=0;
        bd->id = bi+1;
        bi++;
      }
      if((++b)==col) {
        b=0;
        p=nextline(p);
      }
    }
    if(b) p=nextline(p);

    if(!ok) {
      ErrMessage(G,"TOPStrToCoordSet","Error hydrogen free bonds");
    } else {
      PRINTFB(G,FB_ObjectMolecule,FB_Blather)
        " TOPStr2CoordSet: read %d hydrogen free bonds.\n",NBONA
        ENDFB(G);
    }

    if(!amber7) {

      /* skip hydrogen angles */
      
      p=skip_fortran(4*NTHETH,12,p);
      
      /* skip non-hydrogen angles */
      
      p=skip_fortran(4*NTHETA,12,p);
      
      /* skip hydrogen dihedrals */
      
      p=skip_fortran(5*NPHIH,12,p);
      
      /* skip non hydrogen dihedrals */
      
      p=skip_fortran(5*NPHIA,12,p);
      
      /* skip nonbonded exclusions */
      
      p=skip_fortran(NNB,12,p);
      
      /* skip hydrogen bonds ASOL */
      
      p=skip_fortran(NPHB,5,p);
      
      /* skip hydrogen bonds BSOL */
      
      p=skip_fortran(NPHB,5,p);
      
      /* skip HBCUT */
      
      p=skip_fortran(NPHB,5,p);
      
    }
    /* read AMBER atom types */

    if(amber7) {
      p = findflag(G,buffer,"AMBER_ATOM_TYPE","20a4");
    }

    b=0;
    for(a=0;a<nAtom;a++) {
      OrthoLineType temp;
      p=ncopy(cc,p,4);
      ai=atInfo+a;
      if(sscanf(cc,"%s",temp)!=1)
        ok=false;
      else {
        OVreturn_word ret = OVLexicon_GetFromCString(G->Lexicon,temp);
        if(OVreturn_IS_OK(ret)) {
          ai->textType = ret.word;
        }
      }
      if((++b)==20) {
        b=0;
        p=nextline(p);
      }
    }
    if(b) p=nextline(p);

    if(!ok) {
      ErrMessage(G,"TOPStrToCoordSet","Error reading atom types");
    } else {
      PRINTFB(G,FB_ObjectMolecule,FB_Blather)
        " TOPStr2CoordSet: read atom types.\n"
        ENDFB(G);
    }

    if(!amber7) {
      /* skip TREE classification */
      
      p=skip_fortran(nAtom,20,p);
      
      /* skip tree joining information */
      
      p=skip_fortran(nAtom,12,p);
      
      /* skip last atom rotated blah blah blah */
      
      p=skip_fortran(nAtom,12,p);

    }

    if(IFBOX>0) {
      
      int IPTRES,NSPM,NSPSOL;
      
      if(amber7) {
        p = findflag(G,buffer,"SOLVENT_POINTERS","3I8");
        wid=8;
      } else {
        wid=6;
      }
      p=ncopy(cc,p,wid); ok = ok && (sscanf(cc,"%d",&IPTRES)==1);
      p=ncopy(cc,p,wid); ok = ok && (sscanf(cc,"%d",&NSPM)==1);
      p=ncopy(cc,p,wid); ok = ok && (sscanf(cc,"%d",&NSPSOL)==1);
      
      p=nextline(p);
      
      if(amber7) {
        p = findflag(G,buffer,"ATOMS_PER_MOLECULE","10I8");
        col=10;
      } else {
        col=12;
      }
      
      /* skip num atoms per box */
      
      p=skip_fortran(NSPM,col,p);
      
      if(amber7) {
        p = findflag(G,buffer,"BOX_DIMENSIONS","5E16.8");
      }
      wid=16;
      
      p=ncopy(cc,p,16); ok = ok && (sscanf(cc,"%f",&BETA)==1);
      p=ncopy(cc,p,16); ok = ok && (sscanf(cc,"%f",&BOX1)==1);
      p=ncopy(cc,p,16); ok = ok && (sscanf(cc,"%f",&BOX2)==1);
      p=ncopy(cc,p,16); ok = ok && (sscanf(cc,"%f",&BOX3)==1);
      
      if(ok) {
        if(!cset->PeriodicBox) 
          cset->PeriodicBox=CrystalNew(G);
        cset->PeriodicBox->Dim[0] = BOX1;
        cset->PeriodicBox->Dim[1] = BOX2;
        cset->PeriodicBox->Dim[2] = BOX3;
        if((BETA > 109.47) && (BETA < 109.48)) {
          cset->PeriodicBoxType=cCSet_Octahedral;
          cset->PeriodicBox->Angle[0]=(float)(2.0*acos(1.0/sqrt(3.0))*180.0/PI);
          cset->PeriodicBox->Angle[1]=(float)(2.0*acos(1.0/sqrt(3.0))*180.0/PI);
          cset->PeriodicBox->Angle[2]=(float)(2.0*acos(1.0/sqrt(3.0))*180.0/PI);
        } else if(BETA==60.0) {
          cset->PeriodicBox->Angle[0]=60.0; /* rhombic dodecahedron (from ptraj.c) */
          cset->PeriodicBox->Angle[1]=90.0;
          cset->PeriodicBox->Angle[2]=60.0;
        } else {
          cset->PeriodicBox->Angle[0]=90.0;
          cset->PeriodicBox->Angle[1]=BETA;
          cset->PeriodicBox->Angle[2]=90.0;
        }
        
        CrystalUpdate(cset->PeriodicBox);
        /*        CrystalDump(cset->PeriodicBox);*/
      }
      /* skip periodic box */
      
      p=nextline(p);
      
    }
    
    if(!amber7) {

      if(IFCAP>0) {
        p=nextline(p);
        p=nextline(p);
        p=nextline(p);
      }

      if(IFPERT>0) {

        /* skip perturbed bond atoms */

        p=skip_fortran(2*NBPER,12,p);    

        /* skip perturbed bond atom pointers */

        p=skip_fortran(2*NBPER,12,p);    

        /* skip perturbed angles */

        p=skip_fortran(3*NGPER,12,p);    

        /* skip perturbed angle pointers */

        p=skip_fortran(2*NGPER,12,p);    

        /* skip perturbed dihedrals */

        p=skip_fortran(4*NDPER,12,p);    

        /* skip perturbed dihedral pointers */

        p=skip_fortran(2*NDPER,12,p);    

        /* skip residue names */

        p=skip_fortran(NRES,20,p);    

        /* skip atom names */

        p=skip_fortran(nAtom,20,p);    

        /* skip atom symbols */

        p=skip_fortran(nAtom,20,p);    

        /* skip unused field */

        p=skip_fortran(nAtom,5,p);    

        /* skip perturbed flags */

        p=skip_fortran(nAtom,12,p);    

        /* skip LJ atom flags */

        p=skip_fortran(nAtom,12,p);    

        /* skip perturbed charges */

        p=skip_fortran(nAtom,5,p);    

      }

      if(IPOL>0) {

        /* skip atomic polarizabilities */

        p=skip_fortran(nAtom,5,p);    

      }

      if((IPOL>0) && (IFPERT>0)) {

        /* skip atomic polarizabilities */

        p=skip_fortran(nAtom,5,p);    
    
      }
    }
    /* for future reference 

%FLAG LES_NTYP
%FORMAT(10I8)
%FLAG LES_TYPE
%FORMAT(10I8)
%FLAG LES_FAC
%FORMAT(5E16.8)
%FLAG LES_CNUM
%FORMAT(10I8)
%FLAG LES_ID
%FORMAT(10I8)

Here is the additional information for LES topology formats:
First, if NPARM ==1, LES entries are in topology (NPARM is the 10th
entry in the initial list of control parameters); otherwise the standard
format applies.
So, with NPARM=1, you just need to read a few more things at the very
end of topology file:
LES_NTYP (format: I6) ... one number, number of LES types
and four arrays:
LES_TYPE (12I6) ... NATOM integer entries
LES_FAC (E16.8) ... LES_NTYPxLES_NTYP float entries
LES_CNUM (12I6) ... NATOM integer entries
LES_ID (12I6)   ... NATOM integer entries

and that's it. Your parser must have skipped this information because it
was at the end of the file. Maybe that's good enough.



     */

    coord=VLAlloc(float,3*nAtom);

    f=coord;
    for(a=0;a<nAtom;a++) {
      *(f++)=0.0;
      *(f++)=0.0;
      *(f++)=0.0;
      ai = atInfo + a;
      ai->id = a+1; /* assign 1-based identifiers */
      ai->rank = a; 
      AtomInfoAssignParameters(G,ai);
      AtomInfoAssignColors(G,ai);
      for(c=0;c<cRepCnt;c++) {
        ai->visRep[c] = false;
      }
      ai->visRep[cRepLine] = auto_show_lines; /* show lines by default */
      ai->visRep[cRepNonbonded] = auto_show_nonbonded; /* show lines by default */
      ai->visRep[cRepSphere] = auto_show_spheres; /* show lines by default */
    }
  }
  if(ok) {
    cset->NIndex=nAtom;
    cset->Coord=coord;
    cset->TmpBond=bond;
    cset->NTmpBond=nBond;
  } else {
    if(cset) 
      cset->fFree(cset);
  }
  if(atInfoPtr)
	 *atInfoPtr = atInfo;
  
  return(cset);
}

/*========================================================================*/
ObjectMolecule *ObjectMoleculeReadTOPStr(PyMOLGlobals *G,ObjectMolecule *I,char *TOPStr,int frame,int discrete)
{
  CoordSet *cset = NULL;
  AtomInfoType *atInfo;
  int ok=true;
  int isNew = true;
  unsigned int nAtom = 0;

  if(!I) 
	 isNew=true;
  else 
	 isNew=false;

  if(ok) {

	 if(isNew) {
		I=(ObjectMolecule*)ObjectMoleculeNew(G,discrete);
		atInfo = I->AtomInfo;
		isNew = true;
	 } else { /* never */
		atInfo=VLAMalloc(10,sizeof(AtomInfoType),2,true); /* autozero here is important */
		isNew = false;
	 }
    if(isNew) {
      I->Obj.Color = AtomInfoUpdateAutoColor(G);
    }

	 cset=ObjectMoleculeTOPStr2CoordSet(G,TOPStr,&atInfo);	 
	 nAtom=cset->NIndex;
  }

  /* include coordinate set */
  if(ok) {

      if(I->DiscreteFlag&&atInfo) {
        unsigned int a;
        int fp1 = frame+1;
        AtomInfoType *ai = atInfo;
        for(a=0;a<nAtom;a++) {
          (ai++)->discrete_state = fp1;
        }
      }

    cset->Obj = I;
    cset->fEnumIndices(cset);
    if(cset->fInvalidateRep)
      cset->fInvalidateRep(cset,cRepAll,cRepInvRep);
    if(isNew) {		
      I->AtomInfo=atInfo; /* IMPORTANT to reassign: this VLA may have moved! */
    } else {
      ObjectMoleculeMerge(I,atInfo,cset,false,cAIC_AllMask,true); /* NOTE: will release atInfo */
    }
    if(isNew) I->NAtom=nAtom;
    /* 
       if(frame<0) frame=I->NCSet;
       VLACheck(I->CSet,CoordSet*,frame);
       if(I->NCSet<=frame) I->NCSet=frame+1;
       if(I->CSet[frame]) I->CSet[frame]->fFree(I->CSet[frame]);
       I->CSet[frame] = cset;
    */

    if(isNew) I->NBond = ObjectMoleculeConnect(I,&I->Bond,I->AtomInfo,cset,false);
    if(cset->Symmetry&&(!I->Symmetry)) {
      I->Symmetry=SymmetryCopy(cset->Symmetry);
      SymmetryAttemptGeneration(I->Symmetry,false);
    }

    if(I->CSTmpl)
      if(I->CSTmpl->fFree)
        I->CSTmpl->fFree(I->CSTmpl);
    I->CSTmpl = cset; /* save template coordinate set */

    SceneCountFrames(G);
    ObjectMoleculeExtendIndices(I);
    ObjectMoleculeSort(I);
    ObjectMoleculeUpdateIDNumbers(I);
    ObjectMoleculeUpdateNonbonded(I);
  }
  return(I);
}

ObjectMolecule *ObjectMoleculeLoadTOPFile(PyMOLGlobals *G,ObjectMolecule *obj,char *fname,int frame,int discrete)
{
  ObjectMolecule *I=NULL;
  int ok=true;
  FILE *f;
  long size;
  char *buffer,*p;

  f=fopen(fname,"rb");
  if(!f)
	 ok=ErrMessage(G,"ObjectMoleculeLoadTOPFile","Unable to open file!");
  else
	 {
      PRINTFB(G,FB_ObjectMolecule,FB_Blather) 
        " ObjectMoleculeLoadTOPFile: Loading from %s.\n",fname
        ENDFB(G);
		
		fseek(f,0,SEEK_END);
      size=ftell(f);
		fseek(f,0,SEEK_SET);

		buffer=(char*)mmalloc(size+255);
		ErrChkPtr(G,buffer);
		p=buffer;
		fseek(f,0,SEEK_SET);
		fread(p,size,1,f);
		p[size]=0;
		fclose(f);

		I=ObjectMoleculeReadTOPStr(G,obj,buffer,frame,discrete);

		mfree(buffer);
	 }

  return(I);
}

void ObjectMoleculeSculptClear(ObjectMolecule *I)
{
  PRINTFD(I->Obj.G,FB_ObjectMolecule)
    " ObjectMoleculeSculptClear: entered.\n"
    ENDFD;

  if(I->Sculpt) SculptFree(I->Sculpt);
  I->Sculpt=NULL;
}

void ObjectMoleculeSculptImprint(ObjectMolecule *I,int state,int match_state,int match_by_segment)
{
  PRINTFD(I->Obj.G,FB_ObjectMolecule)
    " ObjectMoleculeUpdateSculpt: entered.\n"
    ENDFD;

  if(!I->Sculpt) I->Sculpt = SculptNew(I->Obj.G);
  SculptMeasureObject(I->Sculpt,I,state,match_state,match_by_segment);
}

float ObjectMoleculeSculptIterate(ObjectMolecule *I,int state,int n_cycle, float *center)
{
  PRINTFD(I->Obj.G,FB_ObjectMolecule)
    " ObjectMoleculeIterateSculpt: entered.\n"
    ENDFD;
  if(I->Sculpt) {
    return SculptIterateObject(I->Sculpt,I,state,n_cycle,center);
  } else
    return 0.0F;
}

void ObjectMoleculeUpdateIDNumbers(ObjectMolecule *I)
{
  int a;
  int max;
  AtomInfoType *ai;
  BondType *b;

  if(I->AtomCounter<0) {
    max=-1;
    ai=I->AtomInfo;
    for(a=0;a<I->NAtom;a++) {
      if(ai->id>max)
        max=ai->id;
      ai++;
    }
    I->AtomCounter=max+1;
  }
  ai=I->AtomInfo;
  for(a=0;a<I->NAtom;a++) {
    if(ai->id<0) 
      ai->id=I->AtomCounter++;
    ai++;
  }

  if(I->BondCounter<0) {
    max=-1;
    b=I->Bond;
    for(a=0;a<I->NBond;a++) {
      if(b->id>max) 
        max=b->id;
      b++;
    }
    I->BondCounter=max+1;
  }
  b=I->Bond;
  for(a=0;a<I->NBond;a++) {
    if(!b->id) 
      b->id=I->BondCounter++;
    b++;
  }
}

static CoordSet *ObjectMoleculePMO2CoordSet(PyMOLGlobals *G,CRaw *pmo,AtomInfoType **atInfoPtr,int *restart)
{
  int nAtom,nBond;
  int a;
  float *coord = NULL;
  CoordSet *cset = NULL;
  AtomInfoType *atInfo = NULL,*ai;
#if 0
  AtomInfoType068 *atInfo068 = NULL;
  AtomInfoType076 *atInfo076 = NULL;
  AtomInfoType083 *atInfo083 = NULL;
  BondType068 *bond068=NULL;
  BondType083 *bond083=NULL;
#endif
  BondType *bond=NULL;

  int ok=true;
  int type,size;
  float *spheroid=NULL;
  float *spheroid_normal=NULL;
  int sph_info[2];
  int version;

  *restart=false;
  nAtom=0;
  nBond=0;
  if(atInfoPtr)
	 atInfo = *atInfoPtr;
  
  type = RawGetNext(pmo,&size,&version);
  if(type!=cRaw_AtomInfo1) {
    ok=false;
  } else { /* read atoms */
    PRINTFD(G,FB_ObjectMolecule)
      " ObjectMolPMO2CoordSet: loading atom info %d bytes = %8.3f\n",size,((float)size)/sizeof(AtomInfoType)
      ENDFD;
    if(version<98) {
      PRINTFB(G,FB_ObjectMolecule,FB_Errors)
        " ObjectMolecule: unsupported binary file (version %d). aborting.\n",
        version
        ENDFB(G);
      ok=false;
#if 0
    } else if(version<69) { /* legacy atom format */
      nAtom = size/sizeof(AtomInfoType068);
      atInfo068 = Alloc(AtomInfoType068,nAtom);
      ok = RawReadInto(pmo,cRaw_AtomInfo1,size,(char*)atInfo068);
      VLACheck(atInfo,AtomInfoType,nAtom);
      UtilExpandArrayElements(atInfo068,atInfo,nAtom,
                              sizeof(AtomInfoType068),sizeof(AtomInfoType));
      FreeP(atInfo068);
    } else if(version<77) { /* legacy atom format */
      nAtom = size/sizeof(AtomInfoType076);
      atInfo076 = Alloc(AtomInfoType076,nAtom);
      ok = RawReadInto(pmo,cRaw_AtomInfo1,size,(char*)atInfo076);
      VLACheck(atInfo,AtomInfoType,nAtom);
      UtilExpandArrayElements(atInfo076,atInfo,nAtom,
                              sizeof(AtomInfoType076),sizeof(AtomInfoType));
      FreeP(atInfo076);
      
    } else if(version<84) { /* legacy atom format */
      nAtom = size/sizeof(AtomInfoType083);
      atInfo083 = Alloc(AtomInfoType083,nAtom);
      ok = RawReadInto(pmo,cRaw_AtomInfo1,size,(char*)atInfo083);
      VLACheck(atInfo,AtomInfoType,nAtom);
      UtilExpandArrayElements(atInfo083,atInfo,nAtom,
                              sizeof(AtomInfoType083),sizeof(AtomInfoType));
      FreeP(atInfo083);
#endif

    } else {
      nAtom = size/sizeof(AtomInfoType);
      VLACheck(atInfo,AtomInfoType,nAtom);
      ok = RawReadInto(pmo,cRaw_AtomInfo1,size,(char*)atInfo);
    }
  }
  if(ok) {
    PRINTFD(G,FB_ObjectMolecule)
      " ObjectMolPMO2CoordSet: loading coordinates\n"
      ENDFD;
    coord = (float*)RawReadVLA(pmo,cRaw_Coords1,sizeof(float),5,false);
    if(!coord)
      ok=false;
  }
  type = RawGetNext(pmo,&size,&version);
  if(type==cRaw_SpheroidInfo1) {

    PRINTFD(G,FB_ObjectMolecule)
      " ObjectMolPMO2CoordSet: loading spheroid\n"
      ENDFD;

    ok = RawReadInto(pmo,cRaw_SpheroidInfo1,sizeof(int)*2,(char*)sph_info);
    if(ok) {

    PRINTFD(G,FB_ObjectMolecule)
      " ObjectMolPMO2CoordSet: loading spheroid size %d nsph %d\n",sph_info[0],sph_info[1]
      ENDFD;

      spheroid = (float*)RawReadPtr(pmo,cRaw_Spheroid1,&size);
      if(!spheroid)
        ok=false;

      PRINTFD(G,FB_ObjectMolecule)
        " ObjectMolPMO2CoordSet: loaded spheroid %p size %d \n",
        (void*)spheroid,size
        ENDFD;

    }
    if(ok) {
      spheroid_normal = (float*)RawReadPtr(pmo,cRaw_SpheroidNormals1,&size);
      if(!spheroid_normal)
        ok=false;
      }
      PRINTFD(G,FB_ObjectMolecule)
        " ObjectMolPMO2CoordSet: loaded spheroid %p size %d \n",
        (void*)spheroid_normal,size
        ENDFD;

    } 
  if(ok) 
      type = RawGetNext(pmo,&size,&version);    
  if(ok) {
    
    PRINTFD(G,FB_ObjectMolecule)
      " ObjectMolPMO2CoordSet: loading bonds\n"
      ENDFD;

    if(type!=cRaw_Bonds1) {
      ok=false;
    } else {
      if(ok) {

        /* legacy bond format */
        if(version<98) {
          PRINTFB(G,FB_ObjectMolecule,FB_Errors)
            " ObjectMolecule: unsupported binary file (version %d). aborting.\n",
            version
            ENDFB(G);
          ok=false;
#if 0
        } else if(version<69) { /* legacy atom format */
          nBond = size/sizeof(BondType068);
          bond068 = Alloc(BondType068,nBond);
          ok = RawReadInto(pmo,cRaw_Bonds1,nBond*sizeof(BondType068),(char*)bond068);
          bond=VLACalloc(BondType,nBond);
          UtilExpandArrayElements(bond068,bond,nBond,
                                  sizeof(BondType068),sizeof(BondType));
          FreeP(bond068);
          for(a=0;a<nBond;a++) bond[a].id=-1; /* initialize identifiers */
        } else if(version<84) {
          nBond = size/sizeof(BondType083);
          bond083 = Alloc(BondType083,nBond);
          ok = RawReadInto(pmo,cRaw_Bonds1,nBond*sizeof(BondType083),(char*)bond083);

          bond=VLACalloc(BondType,nBond);
          UtilExpandArrayElements(bond083,bond,nBond,
                                  sizeof(BondType083),sizeof(BondType));
          FreeP(bond083);
#endif
        } else {
          /* unique_id handling? */

          bond=(BondType*)RawReadVLA(pmo,cRaw_Bonds1,sizeof(BondType),5,true);
          nBond = VLAGetSize(bond);
        }
        
        PRINTFD(G,FB_ObjectMolecule)
          " ObjectMolPMO2CoordSet: found %d bonds\n",nBond
          ENDFD;

        if(Feedback(G,FB_ObjectMolecule,FB_Debugging)) {
          for(a=0;a<nBond;a++)
            printf(" ObjectMoleculeConnect: bond %d ind0 %d ind1 %d order %d\n",
                   a,bond[a].index[0],bond[a].index[1],bond[a].order);
        }
        
      }
    }
  }

  if(ok) {
    ai=atInfo;
    for(a=0;a<nAtom;a++) {
      ai->selEntry=0;
      ai++;
    }
	 cset = CoordSetNew(G);
	 cset->NIndex=nAtom;
	 cset->Coord=coord;
	 cset->NTmpBond=nBond;
	 cset->TmpBond=bond;
    if(spheroid) {
      cset->Spheroid=spheroid;
      cset->SpheroidNormal=spheroid_normal;
      cset->SpheroidSphereSize=sph_info[0];
      cset->NSpheroid = sph_info[1];

    }
  } else {
	 VLAFreeP(bond);
	 VLAFreeP(coord);
    FreeP(spheroid);
    FreeP(spheroid_normal);
  }
  if(atInfoPtr)
	 *atInfoPtr = atInfo;
  if(ok) {
    type = RawGetNext(pmo,&size,&version);
    if(type==cRaw_AtomInfo1)
      *restart=true;
  }
  return(cset);
}
/*========================================================================*/
ObjectMolecule *ObjectMoleculeReadPMO(PyMOLGlobals *G,ObjectMolecule *I,CRaw *pmo,int frame,int discrete)
{

  CoordSet *cset = NULL;
  AtomInfoType *atInfo;
  int ok=true;
  int isNew = true;
  unsigned int nAtom = 0;
  int restart =false;
  int repeatFlag = true;
  int successCnt = 0;

  while(repeatFlag) {
    repeatFlag = false;
  
    if(!I) 
      isNew=true;
    else 
      isNew=false;
    
    if(ok) {
      
      if(isNew) {
        I=(ObjectMolecule*)ObjectMoleculeNew(G,discrete);
        atInfo = I->AtomInfo;
        isNew = true;
      } else {
        atInfo=VLAMalloc(10,sizeof(AtomInfoType),2,true); /* autozero here is important */
        isNew = false;
      }
      if(isNew) {
        I->Obj.Color = AtomInfoUpdateAutoColor(G);
      }

      cset = ObjectMoleculePMO2CoordSet(G,pmo,&atInfo,&restart);

      if(isNew) {		
        I->AtomInfo=atInfo; /* IMPORTANT to reassign: this VLA may have moved! */
      }
      if(cset) 
        nAtom=cset->NIndex;
      else
        ok=false;
    }
    
    /* include coordinate set */
    if(ok) {

      if(I->DiscreteFlag&&atInfo) {
        unsigned int a;
        int fp1 = frame+1;
        AtomInfoType *ai = atInfo;
        for(a=0;a<nAtom;a++) {
          (ai++)->discrete_state = fp1;
        }
      }

      cset->Obj = I;
      cset->fEnumIndices(cset);
      if(cset->fInvalidateRep)
        cset->fInvalidateRep(cset,cRepAll,cRepInvRep);
      if(!isNew) {		
        ObjectMoleculeMerge(I,atInfo,cset,true,cAIC_AllMask,true); /* NOTE: will release atInfo */
      }
      if(isNew) I->NAtom=nAtom;
      if(frame<0) frame=I->NCSet;
      VLACheck(I->CSet,CoordSet*,frame);
      if(I->NCSet<=frame) I->NCSet=frame+1;
      if(I->CSet[frame]) I->CSet[frame]->fFree(I->CSet[frame]);
      I->CSet[frame] = cset;
      if(isNew) I->NBond = ObjectMoleculeConnect(I,&I->Bond,I->AtomInfo,cset,false);

      if(cset->Symmetry&&(!I->Symmetry)) {
        I->Symmetry=SymmetryCopy(cset->Symmetry);
        SymmetryAttemptGeneration(I->Symmetry,false);
      }
      SceneCountFrames(G);
      ObjectMoleculeExtendIndices(I);
      ObjectMoleculeSort(I);
      ObjectMoleculeUpdateIDNumbers(I);
      ObjectMoleculeUpdateNonbonded(I);
      successCnt++;
      if(successCnt>1) {
        if(successCnt==2){
          PRINTFB(G,FB_ObjectMolecule,FB_Actions)
            " ObjectMolReadPMO: read model %d\n",1
            ENDFB(G);
            }
        PRINTFB(G,FB_ObjectMolecule,FB_Actions)
          " ObjectMolReadPMO: read model %d\n",successCnt
          ENDFB(G);
      }
    }
    if(restart) {
      repeatFlag=true;
      frame=frame+1;
      restart=false;
    }
  }
  return(I);
  
  }
/*========================================================================*/
ObjectMolecule *ObjectMoleculeLoadPMOFile(PyMOLGlobals *G,ObjectMolecule *obj,char *fname,int frame,int discrete)
{
  ObjectMolecule *I=NULL;
  int ok=true;
  CRaw *raw;
    
  raw = RawOpenRead(G,fname);
  if(!raw)
	 ok=ErrMessage(G,"ObjectMoleculeLoadPMOFile","Unable to open file!");
  else
	 {
      PRINTFB(G,FB_ObjectMolecule,FB_Blather)
        " ObjectMoleculeLoadPMOFile: Loading from %s.\n",fname
        ENDFB(G);
		
		I=ObjectMoleculeReadPMO(G,obj,raw,frame,discrete);
      RawFree(raw);
	 }
  
  return(I);
}


/*========================================================================*/
int ObjectMoleculeMultiSave(ObjectMolecule *I,char *fname,int state,int append)
{
  /* version 1 writes atominfo, coords, spheroid, bonds */
  CRaw *raw = NULL;
  int ok=true;
  int a,c,a1,a2,b1,b2;
  BondType *b;
  CoordSet *cs;
  BondType *bondVLA = NULL;
  AtomInfoType *aiVLA = NULL;
  int start,stop;
  int nBond;
  int sph_info[2];
  PRINTFD(I->Obj.G,FB_ObjectMolecule)
    " ObjectMoleculeMultiSave-Debug: entered \"%s\" state=%d\n",fname,state
    ENDFD;
    
  if(append) {
    raw = RawOpenWrite(I->Obj.G,fname);
  } else {
    raw = RawOpenAppend(I->Obj.G,fname);
  }
  if(raw) {
    aiVLA = VLAMalloc(1000,sizeof(AtomInfoType),5,true);
    bondVLA = VLACalloc(BondType,4000);
    if(state<0) {
      start=0;
      stop=I->NCSet;
    } else {
      start=state;
      if(start<0)
        start=0;
      stop=state+1;
      if(stop>I->NCSet)
        stop=I->NCSet;
    }
    for(a=start;a<stop;a++) {

      PRINTFD(I->Obj.G,FB_ObjectMolecule)
        " ObjectMMSave-Debug: state %d\n",a
        ENDFD;

      cs=I->CSet[a];
      if(cs) {
        VLACheck(aiVLA,AtomInfoType,cs->NIndex);
        nBond=0;

        /* write atoms */
        for(c=0;c<cs->NIndex;c++) {
          a1 = cs->IdxToAtm[c]; /* always valid */
          aiVLA[c]=I->AtomInfo[a1];
        }
        if(ok) ok = RawWrite(raw,cRaw_AtomInfo1,sizeof(AtomInfoType)*cs->NIndex,0,(char*)aiVLA);
        
        /* write coords */
        if(ok) ok = RawWrite(raw,cRaw_Coords1,sizeof(float)*3*cs->NIndex,0,(char*)cs->Coord);

        /* write spheroid (if one exists) */
        if(cs->Spheroid&&cs->SpheroidNormal) {
          sph_info[0]=cs->SpheroidSphereSize;
          sph_info[1]=cs->NSpheroid;
          if(ok) ok = RawWrite(raw,cRaw_SpheroidInfo1,sizeof(int)*2,0,(char*)sph_info);          
          if(ok) ok = RawWrite(raw,cRaw_Spheroid1,sizeof(float)*cs->NSpheroid,0,(char*)cs->Spheroid);          
          if(ok) ok = RawWrite(raw,cRaw_SpheroidNormals1,sizeof(float)*3*cs->NSpheroid,0,(char*)cs->SpheroidNormal); 
          PRINTFD(I->Obj.G,FB_ObjectMolecule)
            " ObjectMolPMO2CoorSet: saved spheroid size %d %d\n",cs->SpheroidSphereSize,cs->NSpheroid
            ENDFD;
         
        }
        
        /* write bonds */
        b=I->Bond;
        for(c=0;c<I->NBond;c++) {
          b1 = b->index[0];
          b2 = b->index[1];
          if(I->DiscreteFlag) {
            if((cs==I->DiscreteCSet[b1])&&(cs==I->DiscreteCSet[b2])) {
              a1=I->DiscreteAtmToIdx[b1];
              a2=I->DiscreteAtmToIdx[b2];
            } else {
              a1=-1;
              a2=-1;
            }
          } else {
            a1=cs->AtmToIdx[b1];
            a2=cs->AtmToIdx[b2];
          }
          if((a1>=0)&&(a2>=0)) { 
            nBond++;
            VLACheck(bondVLA,BondType,nBond);
            bondVLA[nBond-1]=*b;
            bondVLA[nBond-1].index[0] = a1;
            bondVLA[nBond-1].index[1] = a2;
          }
          b++;

        }
        /* unique_id handling? */
        if(ok) ok = RawWrite(raw,cRaw_Bonds1,sizeof(BondType)*nBond,0,(char*)bondVLA);
      }
    }
  }
  if(raw) RawFree(raw);
  VLAFreeP(aiVLA);
  VLAFreeP(bondVLA);
  return(ok);
}
/*========================================================================*/
int ObjectMoleculeGetPhiPsi(ObjectMolecule *I,int ca,float *phi,float *psi,int state)
{
  int np=-1;
  int cm=-1;
  int c=-1;
  int n=-1;
  int result = false;
  AtomInfoType *ai;
  int n0,at;
  float v_ca[3];
  float v_n[3];
  float v_c[3];
  float v_cm[3];
  float v_np[3];

  ai=I->AtomInfo;

  if((ai[ca].name[0]=='C')&&(ai[ca].name[1]=='A'))
    {
      ObjectMoleculeUpdateNeighbors(I);
      
      /* find C */
      n0 = I->Neighbor[ca]+1;
      while(I->Neighbor[n0]>=0) {
        at = I->Neighbor[n0];
        if((ai[at].name[0]=='C')&&(ai[at].name[1]==0)) {
          c=at;
          break;
        }
        n0+=2;
      }
      
      /* find N */
      n0 = I->Neighbor[ca]+1;
      while(I->Neighbor[n0]>=0) {
        at = I->Neighbor[n0];
        if((ai[at].name[0]=='N')&&(ai[at].name[1]==0)) {
          n=at;
          break;
        }
        n0+=2;
      }
      
      /* find NP */
      if(c>=0) {
        n0 = I->Neighbor[c]+1;
        while(I->Neighbor[n0]>=0) {
          at = I->Neighbor[n0];
          if((ai[at].name[0]=='N')&&(ai[at].name[1]==0)) {
            np=at;
            break;
          }
        n0+=2;
        }
      }
      
      /* find CM */
      if(n>=0) {
        n0 = I->Neighbor[n]+1;
        while(I->Neighbor[n0]>=0) {
          at = I->Neighbor[n0];
          if((ai[at].name[0]=='C')&&(ai[at].name[1]==0)) {
            cm=at;
            break;
          }
          n0+=2;
        }
      }
      if((ca>=0)&&(np>=0)&&(c>=0)&&(n>=0)&&(cm>=0)) {
        if(ObjectMoleculeGetAtomVertex(I,state,ca,v_ca)&&
           ObjectMoleculeGetAtomVertex(I,state,n,v_n)&&
           ObjectMoleculeGetAtomVertex(I,state,c,v_c)&&
           ObjectMoleculeGetAtomVertex(I,state,cm,v_cm)&&
           ObjectMoleculeGetAtomVertex(I,state,np,v_np)) {

          (*phi)=rad_to_deg(get_dihedral3f(v_c,v_ca,v_n,v_cm));
          (*psi)=rad_to_deg(get_dihedral3f(v_np,v_c,v_ca,v_n));
          result=true;
        }
      }
    }
  return(result);
}
/*========================================================================*/
int ObjectMoleculeCheckBondSep(ObjectMolecule *I,int a0,int a1,int dist)
{
  int result = false;
  int n0;
  int stack[MAX_BOND_DIST+1];
  int history[MAX_BOND_DIST+1];
  int depth=0;
  int distinct;
  int a;
  if(dist>MAX_BOND_DIST)
    return false;
  
  /* NOTE: undealtwith crash log: fix this!

0   com.delsci.macpymol      	0x00135590 ObjectMoleculeCheckBondSep + 304 (crt.c:355)
1   <<00000000>> 	0x00000002 0 + 2
2   com.delsci.macpymol      	0x001acb58 RepCartoonNew + 6936 (crt.c:355)
3   com.delsci.macpymol      	0x001009f4 CoordSetUpdate + 1108 (crt.c:355)
4   com.delsci.macpymol      	0x001f061c CmdCoordSetUpdateThread + 108 (crt.c:355)
  
   presumably a race condition with UpdateNeighbors, which need to be mutexed somehow.

*/

  ObjectMoleculeUpdateNeighbors(I);

  PRINTFD(I->Obj.G,FB_ObjectMolecule)
    " CBS-Debug: %s %d %d %d\n",I->Obj.Name,a0,a1,dist
    ENDFD;
  depth = 1;
  history[depth]=a0;
  stack[depth] = I->Neighbor[a0]+1; /* go to first neighbor */
  while(depth) { /* keep going until we've traversed tree */
    while(I->Neighbor[stack[depth]]>=0) /* end of branches? go back up one bond */
      {
        n0 = I->Neighbor[stack[depth]]; /* get current neighbor index */
        stack[depth]+=2; /* set up next neighbor */
        distinct=true; /* check to see if current candidate is distinct from ancestors */
        for(a=1;a<depth;a++) {
          if(history[a]==n0)
            distinct=false;
        }
        if(distinct) {
          if(depth<dist) { /* are not yet at the proper distance? */
            if(distinct) {
              depth++; 
              stack[depth] = I->Neighbor[n0]+1; /* then keep moving outward */
              history[depth] = n0;
            }
          } else if(n0==a1) /* otherwise, see if we have a match */
            result = true;
        }
      }
    depth--;
  }
  PRINTFD(I->Obj.G,FB_ObjectMolecule)
    " CBS-Debug: result %d\n",result
    ENDFD;
  return result;
}
/*========================================================================*/
void ObjectGotoState(ObjectMolecule *I,int state)
{
  if((I->NCSet>1)||(!SettingGet(I->Obj.G,cSetting_static_singletons))) {
    if(state>I->NCSet)
      state = I->NCSet-1;
    if(state<0)
      state = I->NCSet-1;
    SceneSetFrame(I->Obj.G,0,state);
  }
}
/*========================================================================*/
static CObjectState *ObjectMoleculeGetObjectState(ObjectMolecule *I,int state)
{
  CObjectState *result = NULL;
  if(state<0) {
    state = ObjectGetCurrentState(&I->Obj,true);
  }
  if(state>=0) {
    if(state<I->NCSet) {
      CoordSet *cs = I->CSet[state];    
      result = &cs->State;
    }
  }
  return result;
}
/*========================================================================*/
CSetting **ObjectMoleculeGetSettingHandle(ObjectMolecule *I,int state)
{
  
  if(state<0) {
    return(&I->Obj.Setting);
  } else if(state<I->NCSet) {
    if(I->CSet[state]) {
      return(&I->CSet[state]->Setting);
    } else {
      return(NULL);
    }
  } else {
    return(NULL);
  }
}
/*========================================================================*/
int ObjectMoleculeSetStateTitle(ObjectMolecule *I,int state,char *text)
{
  int result=false;
  if(state<0) state=I->NCSet-1;
  if(state>=I->NCSet) {
    PRINTFB(I->Obj.G,FB_ObjectMolecule,FB_Errors)
      "Error: invalid state %d\n",state +1
      ENDFB(I->Obj.G);
    
  } else if(!I->CSet[state]) {
    PRINTFB(I->Obj.G,FB_ObjectMolecule,FB_Errors)
      "Error: empty state %d\n",state +1
      ENDFB(I->Obj.G);
  } else {
    UtilNCopy(I->CSet[state]->Name,text,sizeof(WordType));
    result=true;
  }
  return(result);
}

/*========================================================================*/
char *ObjectMoleculeGetStateTitle(ObjectMolecule *I,int state)
{
  char *result=NULL;
  if(state<0) state=I->NCSet-1;
  if(state>=I->NCSet) {
    PRINTFB(I->Obj.G,FB_ObjectMolecule,FB_Errors)
      "Error: invalid state %d\n",state +1
      ENDFB(I->Obj.G);
  } else if(!I->CSet[state]) {
    PRINTFB(I->Obj.G,FB_ObjectMolecule,FB_Errors)
      "Error: empty state %d\n",state +1
      ENDFB(I->Obj.G);
  } else {
    result = I->CSet[state]->Name;
  }
  return(result);
}

/*========================================================================*/
void ObjectMoleculeRenderSele(ObjectMolecule *I,int curState,int sele,int vis_only)
{

  register PyMOLGlobals *G = I->Obj.G;
  register CoordSet *cs;
  register int a,*idx2atm,nIndex;
  register float *coord,*v;
  register int use_matrices = SettingGet_b(I->Obj.G,I->Obj.Setting,NULL,cSetting_matrix_mode);
  register int flag = true;
  register int all_vis = !vis_only;
  register signed char *visRep;
  
  float tmp_matrix[16],v_tmp[3],*matrix = NULL;

  if(G->HaveGUI && G->ValidContext) {
    register AtomInfoType *atInfo = I->AtomInfo,*ai;
    
    if(curState>=0) {
      if(curState<I->NCSet) {
        if( (cs=I->CSet[curState]) ) {
          idx2atm = cs->IdxToAtm;
          nIndex = cs->NIndex;
          coord = cs->Coord;
          if(use_matrices && cs->State.Matrix) {
            copy44d44f(cs->State.Matrix,tmp_matrix);
            matrix = tmp_matrix;
          } else 
            matrix = NULL;

          if(I->Obj.TTTFlag) {
            if(!matrix) {
              convertTTTfR44f(I->Obj.TTT, tmp_matrix);
            } else {
              float ttt[16];
              convertTTTfR44f(I->Obj.TTT, ttt);
              left_multiply44f44f(ttt,tmp_matrix);
            }
            matrix = tmp_matrix;
          }

          for(a=0;a<nIndex;a++) {
            if(SelectorIsMember(G,atInfo[*(idx2atm++)].selEntry,sele)) {
              if(all_vis)
                flag = true;
              else {
                visRep = atInfo[idx2atm[-1]].visRep;
                ai = atInfo+ idx2atm[-1];
                flag = false;
                if(visRep[cRepCyl] || 
                   visRep[cRepCyl] ||
                   visRep[cRepSphere] ||
                   visRep[cRepSurface] ||
                   visRep[cRepLabel] ||
                   visRep[cRepNonbondedSphere] ||
                   visRep[cRepCartoon] ||
                   visRep[cRepRibbon] ||
                   visRep[cRepLine] ||
                   visRep[cRepMesh] ||
                   visRep[cRepDot] ||
                   visRep[cRepNonbonded])
                  flag = true;
              }
              if(flag) {
                v = coord + a + a + a;
                if(matrix) {
                  transform44f3f(matrix,v,v_tmp);
                  glVertex3fv(v_tmp);
                } else 
                  glVertex3fv(v);
              }
            }
          }
        }
      } else if(SettingGet(I->Obj.G,cSetting_static_singletons)) {
        if(I->NCSet==1) {
          if( (cs=I->CSet[0])) {
            idx2atm = cs->IdxToAtm;
            nIndex = cs->NIndex;
            coord = cs->Coord;
            for(a=0;a<nIndex;a++) {
              if(SelectorIsMember(G,atInfo[*(idx2atm++)].selEntry,sele)) {
                if(all_vis)
                  flag = true;
                else {
                  flag = true;
                }
                 if(flag) {
                   v = coord + a + a + a;
                   if(matrix) {
                     transform44f3f(matrix,v,v_tmp);
                     glVertex3fv(v_tmp);
                   } else 
                     glVertex3fv(v);
                 }
              }
            }
          }
        }
      }
    } else { /* all states */
      for(curState=0;curState<I->NCSet;curState++) {
        if( (cs=I->CSet[curState]) ) {
          idx2atm = cs->IdxToAtm;
          nIndex = cs->NIndex;
          coord = cs->Coord;
          for(a=0;a<nIndex;a++) {
            if(SelectorIsMember(G,atInfo[*(idx2atm++)].selEntry,sele)) {
              if(all_vis)
                flag = true;
              else {
                flag = true;
              }
              if(flag) {
                v = coord + a + a + a;
                if(matrix) {
                  transform44f3f(matrix,v,v_tmp);
                  glVertex3fv(v_tmp);
                } else 
                  glVertex3fv(v);
              }
            }
          }
        }
      }
    }
  }
}

/*========================================================================*/
static CoordSet *ObjectMoleculeXYZStr2CoordSet(PyMOLGlobals *G,char *buffer,
                                               AtomInfoType **atInfoPtr, int *have_bonds)
{
  char *p,*p_store;
  int nAtom;
  int a,c;
  float *coord = NULL;
  CoordSet *cset = NULL;
  AtomInfoType *atInfo = NULL,*ai;
  char cc[MAXLINELEN];
  int atomCount;
  BondType *bond=NULL;
  int nBond=0;
  int b1,b2;
  WordType tmp_name;
  int auto_show_lines = (int)SettingGet(G,cSetting_auto_show_lines);
  int auto_show_spheres = (int)SettingGet(G,cSetting_auto_show_spheres);
  int auto_show_nonbonded = (int)SettingGet(G,cSetting_auto_show_nonbonded);
  int tinker_xyz = true;
  int valid_atom;
  int have_n_atom = false;
  BondType *ii;


  p=buffer;
  nAtom=0;
  atInfo = *atInfoPtr;
  
  p_store = p;
  p=ncopy(cc,p,6);  
  if(sscanf(cc,"%d",&nAtom)!=1) {
    nAtom=0;
    tinker_xyz = false;
    p = p_store;
  } else {
    have_n_atom = true;
    p=nskip(p,2);
    p=ncopy(tmp_name,p,sizeof(WordType)-1);
    p=nextline(p);
  }

  if(tinker_xyz&&nAtom) { /* test Tinker XYZ formatting assumption*/
    char *pp = p;
    int dummy_int;
    float dummy_float;
    AtomName dummy_name;

    pp=ncopy(cc,pp,6);
    if(!sscanf(cc,"%d",&dummy_int)) tinker_xyz = false; /* id */
    pp=nskip(pp,2);
    pp=ncopy(cc,pp,3); 
    if(sscanf(cc,"%s",dummy_name)!=1) tinker_xyz = false; /* name */
    pp=ncopy(cc,pp,12);
    if(sscanf(cc,"%f",&dummy_float)!=1) tinker_xyz = false; /* x */
    pp=ncopy(cc,pp,12);
    if(sscanf(cc,"%f",&dummy_float)!=1) tinker_xyz = false; /* y */
    pp=ncopy(cc,pp,12);
    if(sscanf(cc,"%f",&dummy_float)!=1) tinker_xyz = false; /* z */
    pp=ncopy(cc,pp,6);
    if(sscanf(cc,"%d",&dummy_int)!=1) tinker_xyz = false; /* numeric type */
  }

  if(!tinker_xyz) {
    char *pp = p;
    int have_atom_line = true;
    float dummy_float;
    AtomName dummy_name;

    pp=ParseWordCopy(cc,pp,sizeof(AtomName)-1);
    if(sscanf(cc,"%s",dummy_name)!=1) have_atom_line = false; /* name */
    pp=ParseWordCopy(cc,pp,MAXLINELEN-1);
    if(sscanf(cc,"%f",&dummy_float)!=1) have_atom_line = false; /* x */
    pp=ParseWordCopy(cc,pp,MAXLINELEN-1);
    if(sscanf(cc,"%f",&dummy_float)!=1) have_atom_line = false; /* y */
    pp=ParseWordCopy(cc,pp,MAXLINELEN-1);
    if(sscanf(cc,"%f",&dummy_float)!=1) have_atom_line = false; /* z */
    if(!have_atom_line) { /* copy the comment line into the title field */
      p=ncopy(tmp_name,p,sizeof(WordType)-1);
      p=nextline(p);
    }
  }

  if(nAtom) {
    coord=VLAlloc(float,3*nAtom);
    if(atInfo)
      VLACheck(atInfo,AtomInfoType,nAtom);
  } else {
    coord=VLAlloc(float,3);    
  }
  
  if(tinker_xyz) {
    nBond=0;
    bond=VLACalloc(BondType,6*nAtom);  /* is this a safe assumption? */
    ii=bond;
  }

  PRINTFB(G,FB_ObjectMolecule,FB_Blather)
	 " ObjectMoleculeReadXYZ: Found %i atoms...\n",nAtom
    ENDFB(G);

  if(tinker_xyz)
    *have_bonds = true;
  else
    *have_bonds = false;

  a=0;
  atomCount=0;
  while(*p) {
    VLACheck(atInfo,AtomInfoType,atomCount);
    ai=atInfo+atomCount;
    
    if(!tinker_xyz) {
      valid_atom = true;

      p = ParseWordCopy(cc,p,sizeof(AtomName)-1);
      if(!sscanf(cc,"%s",ai->name)) valid_atom = false;
      if(valid_atom) {
           
        ai->rank = atomCount;
        ai->id = atomCount+1;
           
        VLACheck(coord,float,a*3+2);
        p = ParseWordCopy(cc,p,MAXLINELEN-1);
        if(sscanf(cc,"%f",coord+a)!=1) valid_atom = false;
        p = ParseWordCopy(cc,p,MAXLINELEN-1);
        if(sscanf(cc,"%f",coord+a+1)!=1) valid_atom = false;
        p = ParseWordCopy(cc,p,MAXLINELEN-1);
        if(sscanf(cc,"%f",coord+a+2)!=1) valid_atom = false;
           
        strcpy(ai->resn,"UNK");
        ai->alt[0]=0;
        ai->chain[0]=0;
        ai->resv=atomCount+1;          

        ai->q=1.0;
        ai->b=0.0;
        
        ai->segi[0]=0;
        ai->elem[0]=0; /* let atom info guess/infer atom type */
        
        for(c=0;c<cRepCnt;c++) {
          ai->visRep[c] = false;
        }
        ai->visRep[cRepLine] = auto_show_lines; /* show lines by default */
        ai->visRep[cRepNonbonded] = auto_show_nonbonded;
        ai->visRep[cRepSphere] = auto_show_spheres;
        
        /* in the absense of external tinker information, assume hetatm */
          
        ai->hetatm=1;
          
        AtomInfoAssignParameters(G,ai);
        AtomInfoAssignColors(G,ai);
      }
    } else { /* tinker XYZ */
        
      valid_atom = true;

      p=ncopy(cc,p,6);
      if(!sscanf(cc,"%d",&ai->id)) break;
      ai->rank = atomCount;
        
      p=nskip(p,2);/* to 12 */
      p=ncopy(cc,p,3); 
      if(!sscanf(cc,"%s",ai->name)) ai->name[0]=0;
        
      ai->alt[0]=0;
      strcpy(ai->resn,"UNK");
      ai->chain[0] = 0;
        
      ai->resv=atomCount+1;
      sprintf(ai->resi,"%d",ai->resv);
        
      valid_atom = true;
        
      p=ncopy(cc,p,12);
      sscanf(cc,"%f",coord+a);
      p=ncopy(cc,p,12);
      sscanf(cc,"%f",coord+(a+1));
      p=ncopy(cc,p,12);
      sscanf(cc,"%f",coord+(a+2));
        
      ai->q=1.0;
      ai->b=0.0;
        
      ai->segi[0]=0;
      ai->elem[0]=0; /* let atom info guess/infer atom type */
        
      for(c=0;c<cRepCnt;c++) {
        ai->visRep[c] = false;
      }
        
      ai->visRep[cRepLine] = auto_show_lines; /* show lines by default */
      ai->visRep[cRepNonbonded] = auto_show_nonbonded;
      ai->visRep[cRepSphere] = auto_show_spheres;
        
      p=ncopy(cc,p,6);
      sscanf(cc,"%d",&ai->customType);
        
      /* in the absense of external tinker information, assume hetatm */
        
      ai->hetatm=1;
        
      AtomInfoAssignParameters(G,ai);
      AtomInfoAssignColors(G,ai);
        
      b1 = atomCount;
      for(c=0;c<6;c++) {
        p=ncopy(cc,p,6);
        if (!cc[0]) 
          break;
        if(!sscanf(cc,"%d",&b2))
          break;
        if(b1<(b2-1)) {
          VLACheck(bond,BondType,nBond);
          ii = bond+nBond;
          nBond++;
          ii->index[0] = b1;
          ii->index[1] = b2-1;
          ii->order = 1; /* missing bond order information */
          ii->stereo = 0;
          ii->id = -1; /* no serial number */
          ii++;
        }
      }
    }

    if(valid_atom) {
      PRINTFD(G,FB_ObjectMolecule) 
        " ObjectMolecule-DEBUG: %s %s %s %s %8.3f %8.3f %8.3f %6.2f %6.2f %s\n",
        ai->name,ai->resn,ai->resi,ai->chain,
        *(coord+a),*(coord+a+1),*(coord+a+2),ai->b,ai->q,
        ai->segi
        ENDFD;
        
      a+=3;
      atomCount++;

    }
    if(have_n_atom && (atomCount>=nAtom))
      break;
    p=nextline(p);
  }
  
  PRINTFB(G,FB_ObjectMolecule,FB_Blather) 
   " XYZStr2CoordSet: Read %d bonds.\n",nBond
    ENDFB(G);
  
  if(!tinker_xyz) nAtom = atomCount; /* use number of atoms actually read */

  cset = CoordSetNew(G);
  cset->NIndex=nAtom;
  cset->Coord=coord;
  cset->TmpBond=bond;
  cset->NTmpBond=nBond;
  strcpy(cset->Name,tmp_name);
  if(atInfoPtr)
	 *atInfoPtr = atInfo;
  return(cset);
}

/*========================================================================*/
ObjectMolecule *ObjectMoleculeReadXYZStr(PyMOLGlobals *G,ObjectMolecule *I,
                                         char *PDBStr,int frame,int discrete)
{
  CoordSet *cset = NULL;
  AtomInfoType *atInfo;
  int ok=true;
  int isNew = true;
  unsigned int nAtom = 0;
  int have_bonds;

  if(!I) 
	 isNew=true;
  else 
	 isNew=false;

  if(ok) {

	 if(isNew) {
		I=(ObjectMolecule*)ObjectMoleculeNew(G,discrete);
		atInfo = I->AtomInfo;
		isNew = true;
	 } else {
		atInfo=VLAMalloc(10,sizeof(AtomInfoType),2,true); /* autozero here is important */
		isNew = false;
	 }
    if(isNew) {
      I->Obj.Color = AtomInfoUpdateAutoColor(G);
    }
    
	 cset=ObjectMoleculeXYZStr2CoordSet(G,PDBStr,&atInfo,&have_bonds);	 
	 nAtom=cset->NIndex;
  }

  /* include coordinate set */
  if(ok) {

      if(I->DiscreteFlag&&atInfo) {
        unsigned int a;
        int fp1 = frame+1;
        AtomInfoType *ai = atInfo;
        for(a=0;a<nAtom;a++) {
          (ai++)->discrete_state = fp1;
        }
      }

    cset->Obj = I;
    if(cset->fEnumIndices)
      cset->fEnumIndices(cset);
    if(cset->fInvalidateRep)
      cset->fInvalidateRep(cset,cRepAll,cRepInvRep);
    if(isNew) {		
      I->AtomInfo=atInfo; /* IMPORTANT to reassign: this VLA may have moved! */
    } else {
      ObjectMoleculeMerge(I,atInfo,cset,false,cAIC_IDMask,true); /* NOTE: will release atInfo */
    }

    if(isNew) I->NAtom=nAtom;
    if(frame<0) frame=I->NCSet;
    VLACheck(I->CSet,CoordSet*,frame);
    if(I->NCSet<=frame) I->NCSet=frame+1;
    if(I->CSet[frame]) I->CSet[frame]->fFree(I->CSet[frame]);
    I->CSet[frame] = cset;
    if(isNew) I->NBond = ObjectMoleculeConnect(I,&I->Bond,I->AtomInfo,cset,!have_bonds);
    if(cset->Symmetry&&(!I->Symmetry)) {
      I->Symmetry=SymmetryCopy(cset->Symmetry);
      SymmetryAttemptGeneration(I->Symmetry,false);
    }

    SceneCountFrames(G);
    ObjectMoleculeExtendIndices(I);
    ObjectMoleculeSort(I);
    ObjectMoleculeUpdateIDNumbers(I);
    ObjectMoleculeUpdateNonbonded(I);
  }
  return(I);
}
/*========================================================================*/
ObjectMolecule *ObjectMoleculeLoadXYZFile(PyMOLGlobals *G,ObjectMolecule *obj,char *fname,int frame,int discrete)
{
  ObjectMolecule *I=NULL;
  int ok=true;
  FILE *f;
  long size;
  char *buffer,*p;

  f=fopen(fname,"rb");
  if(!f)
	 ok=ErrMessage(G,"ObjectMoleculeLoadXYZFile","Unable to open file!");
  else
	 {
      PRINTFB(G,FB_ObjectMolecule,FB_Blather) 
        " ObjectMoleculeLoadXYZFile: Loading from %s.\n",fname
        ENDFB(G);
		
		fseek(f,0,SEEK_END);
      size=ftell(f);
		fseek(f,0,SEEK_SET);

		buffer=(char*)mmalloc(size+255);
		ErrChkPtr(G,buffer);
		p=buffer;
		fseek(f,0,SEEK_SET);
		fread(p,size,1,f);
		p[size]=0;
		fclose(f);

		I=ObjectMoleculeReadXYZStr(G,obj,buffer,frame,discrete);

		mfree(buffer);
	 }

  return(I);
}


/*========================================================================*/
int ObjectMoleculeAreAtomsBonded(ObjectMolecule *I,int i0,int i1)
{
  int result=false;
  int a;
  BondType *b;
  b=I->Bond;
  for (a=0;a<I->NBond;a++) {
    if(i0==b->index[0]) {
      if(i1==b->index[1]) {
        result=true;
        break;
      }
    }
    if(i1==b->index[0]) {
      if(i0==b->index[1]) {
        result=true;
        break;
      }
    }
    b++;
  }
  return(result);
}
/*========================================================================*/
void ObjectMoleculeRenameAtoms(ObjectMolecule *I,int force)
{
  AtomInfoType *ai;
  int a;
  if(force) {
    ai=I->AtomInfo;
    for(a=0;a<I->NAtom;a++)
      (ai++)->name[0]=0;
  }
  AtomInfoUniquefyNames(I->Obj.G,NULL,0,I->AtomInfo,I->NAtom);  
}
/*========================================================================*/
void ObjectMoleculeAddSeleHydrogens(ObjectMolecule *I,int sele,int state)
{
  int a,b;
  int n,nn;
  CoordSet *cs;
  CoordSet *tcs;
  int seleFlag=false;
  AtomInfoType *ai,*nai,fakeH;
  int repeatFlag=false;
  int nH;
  int *index;
  float v[3],v0[3];
  float d;

  UtilZeroMem(&fakeH,sizeof(AtomInfoType));
  fakeH.protons=1;
  ai=I->AtomInfo;
  for(a=0;a<I->NAtom;a++) {
    if(SelectorIsMember(I->Obj.G,ai->selEntry,sele)) {
      seleFlag=true;
      break;
    }
    ai++;
  }
  if(seleFlag) {
    if(!ObjectMoleculeVerifyChemistry(I,state)) {
      ErrMessage(I->Obj.G," AddHydrogens","missing chemical geometry information.");
    } else if(I->DiscreteFlag) {
      ErrMessage(I->Obj.G," AddHydrogens","can't modify a discrete object.");
    } else {

      repeatFlag=true;
      while(repeatFlag) {
        repeatFlag=false;
        nH = 0;
        ObjectMoleculeUpdateNeighbors(I);
        nai = (AtomInfoType*)VLAMalloc(1000,sizeof(AtomInfoType),1,true);        
        ai=I->AtomInfo;
        for(a=0;a<I->NAtom;a++) {
          if(SelectorIsMember(I->Obj.G,ai->selEntry,sele)) {
            n = I->Neighbor[a];
            nn = I->Neighbor[n++];
            if(nn<ai->valence) {
              VLACheck(nai,AtomInfoType,nH);
              UtilNCopy((nai+nH)->elem,"H",2);
              (nai+nH)->geom=cAtomInfoSingle;
              (nai+nH)->valence=1;
              (nai+nH)->temp1 = a; /* borrowing this field temporarily */
              ObjectMoleculePrepareAtom(I,a,nai+nH);
              nH++;
            }
          }
          ai++;
        }

        if(nH) {

          repeatFlag=true;
          cs = CoordSetNew(I->Obj.G);
          cs->Coord = VLAlloc(float,nH*3);
          cs->NIndex=nH;

          index = Alloc(int,nH);
          for(a=0;a<nH;a++) {
            index[a] = (nai+a)->temp1;
          }
          
          if(cs->fEnumIndices) cs->fEnumIndices(cs);

          cs->TmpLinkBond = VLACalloc(BondType,nH);
          for(a=0;a<nH;a++) {
            cs->TmpLinkBond[a].index[0] = (nai+a)->temp1;
            cs->TmpLinkBond[a].index[1] = a;
            cs->TmpLinkBond[a].order = 1;
            cs->TmpLinkBond[a].stereo = 0;
            cs->TmpLinkBond[a].id = -1;
          }
          cs->NTmpLinkBond = nH;

          AtomInfoUniquefyNames(I->Obj.G,I->AtomInfo,I->NAtom,nai,nH);

          ObjectMoleculeMerge(I,nai,cs,false,cAIC_AllMask,true); /* will free nai and cs->TmpLinkBond  */
          ObjectMoleculeExtendIndices(I);
          ObjectMoleculeUpdateNeighbors(I);

          for(b=0;b<I->NCSet;b++) { /* add coordinate into the coordinate set */
            tcs = I->CSet[b];
            if(tcs) {
              for(a=0;a<nH;a++) {
                ObjectMoleculeGetAtomVertex(I,b,index[a],v0);
                ObjectMoleculeFindOpenValenceVector(I,b,index[a],v,NULL,-1);
                d = AtomInfoGetBondLength(I->Obj.G,I->AtomInfo+index[a],&fakeH);
                scale3f(v,d,v);
                add3f(v0,v,cs->Coord+3*a);
              }
              CoordSetMerge(tcs,cs);  
            }
          }
          FreeP(index);
          if(cs->fFree)
            cs->fFree(cs);
          ObjectMoleculeSort(I);
          ObjectMoleculeUpdateIDNumbers(I);
        } else
          VLAFreeP(nai);
      }
    }
  }
}


/*========================================================================*/
void ObjectMoleculeFuse(ObjectMolecule *I,int index0,ObjectMolecule *src,
                        int index1,int mode,int move_flag)
{
  PyMOLGlobals *G = I->Obj.G;
  int a,b;
  AtomInfoType *ai0,*ai1,*nai;
  int n,nn;
  int at0=-1;
  int at1=-1;
  int a0,a1;
  int hydr1=-1;
  int anch1=-1;
  int ca0,ch0;
  BondType *b0,*b1;
  float *backup = NULL;
  float d,*f0,*f1;
  float va0[3],vh0[3],va1[3],vh1[3];
  float x0[3],y0[3],z0[3];
  float x1[3],y1[3],z1[3];
  float x[3],y[3],z[3];
  float t[3],t2[3];
  CoordSet *cs=NULL,*scs=NULL;
  int state1 = 0;
  CoordSet *tcs;
  int edit=1;
  OrthoLineType sele1,sele2,s1,s2;

  ObjectMoleculeUpdateNeighbors(I);
  ObjectMoleculeUpdateNeighbors(src);

  /* make sure each link point has only one neighbor */

  ai0=I->AtomInfo;
  ai1=src->AtomInfo;
  switch(mode) {
  case 0: /* fusing by replacing hydrogens */
    
    n = I->Neighbor[index0];
    nn = I->Neighbor[n++];
    if(nn==1)
      at0 = I->Neighbor[n];
    
    n = src->Neighbor[index1];
    nn = src->Neighbor[n++];
    if(nn==1)
      at1 = src->Neighbor[n];
    
    if(src->NCSet) {
      scs = src->CSet[state1];
      anch1 = scs->AtmToIdx[at1];
      hydr1 = scs->AtmToIdx[index1];
    }
    break;
  case 1: /* fuse merely by drawing a bond */
    at0 = index0;
    at1 = index1;
    
    if(src->NCSet) {
      scs = src->CSet[state1];
      anch1 = scs->AtmToIdx[at1];
    }

    break;
  }
  
  if((at0>=0)&&(at1>=0)&&scs&&(anch1>=0)) { /* have anchors and source coordinate set */

    nai=(AtomInfoType*)VLAMalloc(src->NAtom,sizeof(AtomInfoType),1,true);
    
    /* copy atoms and atom info into a 1:1 direct mapping */

    cs = CoordSetNew(I->Obj.G);
    cs->Coord = VLAlloc(float,scs->NIndex*3);
    cs->NIndex = scs->NIndex;
    for(a=0;a<scs->NIndex;a++) {
      copy3f(scs->Coord+a*3,cs->Coord+a*3);
      a1 = scs->IdxToAtm[a];

      AtomInfoCopy(G,ai1+a1,nai+a);
      /*      *(nai+a) = *(ai1+a1);  ** unsafe */

      if(a1 == at1) {
        (nai+a)->temp1=2; /* clear marks */        
      } else {
        (nai+a)->temp1=0; /* clear marks */
      }
    }
    
    /* copy internal bond information*/

    cs->TmpBond = VLACalloc(BondType,src->NBond);
    b1 = src->Bond;
    b0 = cs->TmpBond;
    cs->NTmpBond=0;
    for(a=0;a<src->NBond;a++) {
      a0 = scs->AtmToIdx[b1->index[0]];
      a1 = scs->AtmToIdx[b1->index[1]];
      if((a0>=0)&&(a1>=0)) {
        *b0=*b1;
        b0->index[0] = a0;
        b0->index[1] = a1;
        b0++;
        cs->NTmpBond++;
      }
      b1++;
    }

    backup = Alloc(float,cs->NIndex*3); /* make untransformed copy of coordinate set */
    for(a=0;a<cs->NIndex;a++) {
      copy3f(cs->Coord+a*3,backup+a*3);
    }
    
    switch(mode) {
    case 0:
      nai[hydr1].deleteFlag=true;
      I->AtomInfo[index0].deleteFlag=true;
      copy3f(backup+3*anch1,va1);
      copy3f(backup+3*hydr1,vh1);
      subtract3f(va1,vh1,x1); /* note reverse dir from above */
      get_system1f3f(x1,y1,z1);
      break;
    case 1:
      copy3f(backup+3*anch1,va1);
      ObjectMoleculeFindOpenValenceVector(src,state1,at1,x1,NULL,-1);
      scale3f(x1,-1.0F,x1);
      get_system1f3f(x1,y1,z1);      
      break;
    }

    /* set up the linking bond */

    cs->TmpLinkBond = VLACalloc(BondType,1);
    cs->NTmpLinkBond = 1;
    cs->TmpLinkBond->index[0] = at0;
    cs->TmpLinkBond->index[1] = anch1;
    cs->TmpLinkBond->order = 1;
    cs->TmpLinkBond->stereo = 0;
    cs->TmpLinkBond->id = -1;
    
    if(cs->fEnumIndices) cs->fEnumIndices(cs);

    d = AtomInfoGetBondLength(I->Obj.G,ai0+at0,ai1+at1);

    AtomInfoUniquefyNames(I->Obj.G,I->AtomInfo,I->NAtom,nai,cs->NIndex);

    /* set up tags which will enable use to continue editing bond */

    if(edit) {
      for(a=0;a<I->NAtom;a++) {
        I->AtomInfo[a].temp1=0;
      }
      I->AtomInfo[at0].temp1=1;
    }

    ObjectMoleculeMerge(I,nai,cs,false,cAIC_AllMask,true); /* will free nai, cs->TmpBond and cs->TmpLinkBond  */

    ObjectMoleculeExtendIndices(I);
    ObjectMoleculeUpdateNeighbors(I);
    for(a=0;a<I->NCSet;a++) { /* add coordinate into the coordinate set */
      tcs = I->CSet[a];
      if(tcs) {
        switch(mode) {
        case 0:
          ca0 = tcs->AtmToIdx[at0]; /* anchor */
          ch0 = tcs->AtmToIdx[index0]; /* hydrogen */

          if((ca0>=0)&&(ch0>=0)) {
            copy3f(tcs->Coord+3*ca0,va0);
            copy3f(tcs->Coord+3*ch0,vh0);
            subtract3f(vh0,va0,x0);
            get_system1f3f(x0,y0,z0);

          }
          break;
        case 1:
          ca0 = tcs->AtmToIdx[at0]; /* anchor */

          if(ca0>=0) {
            ObjectMoleculeFindOpenValenceVector(I,a,at0,x0,NULL,-1);
            copy3f(tcs->Coord+3*ca0,va0);
            get_system1f3f(x0,y0,z0);
            
          }
          break;
        }
        scale3f(x0,d,t2);
        add3f(va0,t2,t2);
        
        f0=backup;
        f1=cs->Coord;
        for(b=0;b<cs->NIndex;b++) { /* brute force transformation */
          if(move_flag) {
            subtract3f(f0,va1,t);
            scale3f(x0,dot_product3f(t,x1),x);
            scale3f(y0,dot_product3f(t,y1),y);
            scale3f(z0,dot_product3f(t,z1),z);
            add3f(x,y,y);
            add3f(y,z,f1);
            add3f(t2,f1,f1);
          } else {
            copy3f(f0,f1);
          }
          f0+=3;
          f1+=3;
        }
        CoordSetMerge(tcs,cs); 
      }
    }
    switch(mode) {
    case 0:
      ObjectMoleculePurge(I);
      break;
    }
    ObjectMoleculeSort(I);
    ObjectMoleculeUpdateIDNumbers(I);
    if(edit) { /* edit the resulting bond */
      at0=-1;
      at1=-1;
      for(a=0;a<I->NAtom;a++) {
        if(I->AtomInfo[a].temp1==1)
          at0=a;
        if(I->AtomInfo[a].temp1==2)
          at1=a;
      }
      if((at0>=0)&&(at1>=0)) {
        sprintf(sele1,"%s`%d",I->Obj.Name,at1+1); /* points outward... */
        sprintf(sele2,"%s`%d",I->Obj.Name,at0+1);
        SelectorGetTmp(I->Obj.G,sele1,s1);
        SelectorGetTmp(I->Obj.G,sele2,s2);
        EditorSelect(I->Obj.G,s1,s2,NULL,NULL,false,true,true);
        SelectorFreeTmp(I->Obj.G,s1);
        SelectorFreeTmp(I->Obj.G,s2);
      }
    }
  }
  if(cs)
    if(cs->fFree)
      cs->fFree(cs);
  FreeP(backup);
}
/*========================================================================*/
int ObjectMoleculeVerifyChemistry(ObjectMolecule *I,int state)
{
  int result=false;
  AtomInfoType *ai;
  int a;
  int flag;
  
  if(state<0) {
    /* use the first defined state */
    for(a=0;a<I->NCSet;a++) {
      if(I->CSet[a]) {
        state = a;
        break;
      }
    }
  }
  ai=I->AtomInfo;
  flag=true;
  for(a=0;a<I->NAtom;a++) {
    if(!ai->chemFlag) {
      flag=false;
    }
    ai++;
  }
  if((!flag)&&(state>=0)&&(state<I->NCSet)) {
    if(I->CSet[state]) { 
      ObjectMoleculeInferChemFromBonds(I,state);
      ObjectMoleculeInferChemFromNeighGeom(I,state);
      ObjectMoleculeInferHBondFromChem(I);
      /*      ObjectMoleculeInferChemForProtein(I,0);*/
    }
    flag=true;
    ai=I->AtomInfo;
    for(a=0;a<I->NAtom;a++) {
      if(!ai->chemFlag) {
        flag=false;
        break;
      }
      ai++;
    }
  }
  if(flag)
    result=true;
  return(result);
}
/*========================================================================*/
void ObjectMoleculeAttach(ObjectMolecule *I,int index,AtomInfoType *nai)
{
  int a;
  AtomInfoType *ai;
  int n,nn;
  float v[3],v0[3],d;
  CoordSet *cs;

  ObjectMoleculeUpdateNeighbors(I);
  ai=I->AtomInfo+index;
  n = I->Neighbor[index];
  nn = I->Neighbor[n++];
  
  cs = CoordSetNew(I->Obj.G);
  cs->Coord = VLAlloc(float,3);
  cs->NIndex=1;
  cs->TmpLinkBond = VLACalloc(BondType,1);
  cs->NTmpLinkBond = 1;
  cs->TmpLinkBond->index[0]=index;
  cs->TmpLinkBond->index[1]=0;
  cs->TmpLinkBond->order=1;
  cs->TmpLinkBond->stereo=0;
  
  cs->TmpLinkBond->id = -1;
  if(cs->fEnumIndices) cs->fEnumIndices(cs);
  ObjectMoleculePrepareAtom(I,index,nai);
  d = AtomInfoGetBondLength(I->Obj.G,ai,nai);
  ObjectMoleculeMerge(I,nai,cs,false,cAIC_AllMask,true); /* will free nai and cs->TmpLinkBond  */
  ObjectMoleculeExtendIndices(I);
  ObjectMoleculeUpdateNeighbors(I);
  for(a=0;a<I->NCSet;a++) { /* add atom to each coordinate set */
    if(I->CSet[a]) {
      ObjectMoleculeGetAtomVertex(I,a,index,v0);
      ObjectMoleculeFindOpenValenceVector(I,a,index,v,NULL,-1);
      scale3f(v,d,v);
      add3f(v0,v,cs->Coord);
      CoordSetMerge(I->CSet[a],cs); 
    }
  }
  ObjectMoleculeSort(I);
  ObjectMoleculeUpdateIDNumbers(I);
  if(cs->fFree)
    cs->fFree(cs);
  
}
/*========================================================================*/
int ObjectMoleculeFillOpenValences(ObjectMolecule *I,int index)
{
  int a;
  AtomInfoType *ai,*nai;
  int n,nn;
  int result=0;
  int flag = true;
  float v[3],v0[3],d;
  CoordSet *cs;

  if((index>=0)&&(index<=I->NAtom)) {  
    while(1) {
      ObjectMoleculeUpdateNeighbors(I);
      ai=I->AtomInfo+index;
      n = I->Neighbor[index];
      nn = I->Neighbor[n++];
      
      if((nn>=ai->valence)||(!flag))
        break;
      flag=false;

      cs = CoordSetNew(I->Obj.G);
      cs->Coord = VLAlloc(float,3);
      cs->NIndex=1;
      cs->TmpLinkBond = VLACalloc(BondType,1);
      cs->NTmpLinkBond = 1;
      cs->TmpLinkBond->index[0]=index;
      cs->TmpLinkBond->index[1]=0;
      cs->TmpLinkBond->order=1;
      cs->TmpLinkBond->stereo=0;
      
      cs->TmpLinkBond->id = -1;
      if(cs->fEnumIndices) cs->fEnumIndices(cs);
      nai = (AtomInfoType*)VLAMalloc(1,sizeof(AtomInfoType),1,true);
      UtilNCopy(nai->elem,"H",2);
      nai->geom=cAtomInfoSingle;
      nai->valence=1;
      ObjectMoleculePrepareAtom(I,index,nai);
      d = AtomInfoGetBondLength(I->Obj.G,ai,nai);
      ObjectMoleculeMerge(I,nai,cs,false,cAIC_AllMask,true); /* will free nai and cs->TmpLinkBond  */
      ObjectMoleculeExtendIndices(I);
      ObjectMoleculeUpdateNeighbors(I);
      for(a=0;a<I->NCSet;a++) { /* add atom to each coordinate set */
        if(I->CSet[a]) {
          ObjectMoleculeGetAtomVertex(I,a,index,v0);
          ObjectMoleculeFindOpenValenceVector(I,a,index,v,NULL,-1);
          scale3f(v,d,v);
          add3f(v0,v,cs->Coord);
          CoordSetMerge(I->CSet[a],cs); 
        }
      }
      if(cs->fFree)
        cs->fFree(cs);
      result++;
      flag=true;
    }
  }
  ObjectMoleculeUpdateIDNumbers(I);
  return(result);
}

  #define MaxOcc 100

/*========================================================================*/
static int get_planer_normal(ObjectMolecule *I,int state,int index,float *normal)
{   /* NOTE assumes neighbors are defined */
  int found = false;
  int nOcc = 0;
  float occ[MaxOcc*3];
  AtomInfoType *ai = I->AtomInfo + index;
  int n,a1;
  float v0[3],v1[3],v2[3],n0[3];

  if(ObjectMoleculeGetAtomVertex(I,state,index,v0)) {        
    n = I->Neighbor[index];
    n++; /* skip count */
    while(1) { /* look for an attached non-hydrogen as a base */
      a1 = I->Neighbor[n];
      n+=2; 
      if(a1<0) break;
      if(ObjectMoleculeGetAtomVertex(I,state,a1,v1)) {        
        subtract3f(v1,v0,n0);
        normalize3f(n0); /* n0's point away from center atom */
        copy3f(n0,occ+3*nOcc);
        nOcc++; 
        if(nOcc==MaxOcc) /* safety valve */
          break;
      }
    }
    switch(ai->geom) {
    case cAtomInfoPlaner:
      if(nOcc>1) {
        cross_product3f(occ,occ+3,normal);
        if(nOcc>2) {
          cross_product3f(occ,occ+6,v2);
          if(dot_product3f(normal,v2)<0) {
            subtract3f(normal,v2,normal);
          } else {
            add3f(normal,v2,normal);
          }
          cross_product3f(occ+3,occ+6,v2);
          if(dot_product3f(normal,v2)<0) {
            subtract3f(normal,v2,normal);
          } else {
            add3f(normal,v2,normal);
          }
        }
        normalize3f(normal);
        found=true;
      }
      break;
    }
  }
  return found;
}

/*========================================================================*/
int ObjectMoleculeFindOpenValenceVector(ObjectMolecule *I,int state,
                                        int index,float *v,float *seek,
                                        int ignore_index)
{
  CoordSet *cs;
  int nOcc = 0;
  float occ[MaxOcc*3];
  int last_occ = -1;
  int n;
  int a1;
  float v0[3],v1[3],n0[3],t[3];
  int result = false;
  AtomInfoType *ai,*ai1;
  float y[3],z[3];

  /* default is +X */
  v[0]=1.0;
  v[1]=0.0;
  v[2]=0.0;
  
  if(state<0) state=0;
  if(I->NCSet==1) state=0;
  state = state % I->NCSet;
  cs = I->CSet[state];
  if(cs) {
    if((index>=0)&&(index<=I->NAtom)) {
      ai=I->AtomInfo+index;
      if(ObjectMoleculeGetAtomVertex(I,state,index,v0)) {              
        ObjectMoleculeUpdateNeighbors(I);
        n = I->Neighbor[index];
        n++; /* skip count */
        while(1) { /* look for an attached non-hydrogen as a base */
          a1 = I->Neighbor[n];
          n+=2; 
          if(a1<0) break;
          if(a1!=ignore_index) {
            ai1=I->AtomInfo+a1;
            if(ObjectMoleculeGetAtomVertex(I,state,a1,v1)) {        
              last_occ = a1;
              subtract3f(v1,v0,n0);
              normalize3f(n0); /* n0's point away from center atom */
              copy3f(n0,occ+3*nOcc);
              nOcc++; 
              if(nOcc==MaxOcc) /* safety valve */
                break;
            }
          }
        }
        if((!nOcc)||(nOcc>4)||(ai->geom==cAtomInfoNone)) {
          if(!seek) 
            get_random3f(v);
          else
            copy3f(seek,v);
          result = true;
        } else {
          switch(nOcc) {
          case 1:  /* only one current occupied position */
            switch(ai->geom) {
            case cAtomInfoTetrahedral: 
              if(!seek) {
                get_system1f3f(occ,y,z);
                scale3f(occ,-0.334F,v);
                scale3f(z,  0.943F,t);
                add3f(t,v,v);              
              } else { /* point hydrogen towards sought vector */
                copy3f(seek,z);
                get_system2f3f(occ,z,y);
                scale3f(occ,-0.334F,v);
                scale3f(z,  0.943F,t);
                add3f(t,v,v);              
              }
              result = true;
              break;
            case cAtomInfoPlaner:
              {
                if(!seek) {
                  if((last_occ>=0)&&get_planer_normal(I,state,last_occ,n0)) {
                    copy3f(n0,y);
                    get_system2f3f(occ,y,z);
                  } else {
                    get_system1f3f(occ,y,z);
                  }
                  scale3f(occ,-0.500F,v);
                  scale3f(z,      0.866F,t);
                  add3f(t,v,v);
                } else {
                  copy3f(seek,z);
                  get_system2f3f(occ,z,y);
                  scale3f(occ,-0.500F,v);
                  scale3f(z,      0.866F,t);
                  add3f(t,v,v);
                }
                result = true;
              }
              break;
            case cAtomInfoLinear:
              scale3f(occ,-1.0F,v);
              result = true;
              break;
            default:
              if(!seek) 
                get_random3f(v);
              else
                copy3f(seek,v);
              result = true;
              break;
            }
            break;
          case 2:  /* only two current occupied positions */
            switch(ai->geom) {
            case cAtomInfoTetrahedral:
              add3f(occ,occ+3,t);
              get_system2f3f(t,occ,z);
              scale3f(t,-1.0F,v);
              if(seek) {
                if(dot_product3f(z,seek)<0.0F) {
                  invert3f(z);
                }
              } 
              scale3f(z,1.41F,t);
              add3f(t,v,v);              
              result = true;
              break;
            case cAtomInfoPlaner:
              add3f(occ,occ+3,t);
              scale3f(t,-1.0F,v);
              result = true;
              break;
            default:
              if(!seek) 
                get_random3f(v);
              else
                copy3f(seek,v);
              /* hypervalent */
              result = true;
              break;
            }
            break;
          case 3:  /* only three current occupied positions */
            switch(ai->geom) {
            case cAtomInfoTetrahedral:
              add3f(occ,occ+3,t);
              add3f(occ+6,t,t);
              scale3f(t,-1.0F,v);
              result = true;
              break;
            default:
              if(!seek) 
                get_random3f(v);
              else
                copy3f(seek,v);
              /* hypervalent */
              result = true;
              break;
            }
            break;
          case 4:
            if(!seek) 
              get_random3f(v);
            else
              copy3f(seek,v);
            /* hypervalent */
            result = true;
            break;
          }
        }
      }
    }
  }
  normalize3f(v);
  return(result); 
#undef MaxOcc
  
}
/*========================================================================*/
void ObjectMoleculeCreateSpheroid(ObjectMolecule *I,int average)
{
  CoordSet *cs;
  float *spheroid = NULL;
  int a,b,c,a0;
  SphereRec *sp;
  float *spl;
  float *v,*v0,*s,*f,ang,min_dist,*max_sq;
  int *i;
  float *center = NULL;
  float d0[3],n0[3],d1[3],d2[3];
  float p0[3],p1[3],p2[3];
  int t0,t1,t2,bt0,bt1,bt2;
  float dp,l,*fsum = NULL;
  float *norm = NULL;
  float spheroid_smooth;
  float spheroid_fill;
  float spheroid_ratio=0.1F; /* minimum ratio of width over length */
  float spheroid_minimum = 0.02F; /* minimum size - to insure valid normals */
  int row,*count=NULL,base;
  int nRow;
  int first=0;
  int last=0;
  int current;
  int cscount;
  int n_state=0;
  sp = I->Obj.G->Sphere->Sphere[1];
  
  nRow = I->NAtom*sp->nDot;


  center=Alloc(float,I->NAtom*3);
  count=Alloc(int,I->NAtom);
  fsum=Alloc(float,nRow);
  max_sq = Alloc(float,I->NAtom);

  spl=spheroid;

  spheroid_smooth=SettingGet(I->Obj.G,cSetting_spheroid_smooth);
  spheroid_fill=SettingGet(I->Obj.G,cSetting_spheroid_fill);
  /* first compute average coordinate */

  if(average<1)
    average=I->NCSet;
  current=0;
  cscount=0;
  while(current<I->NCSet) {
    if(I->CSet[current]) {
      if(!cscount)
        first=current;
      cscount++;
      last=current+1;
    }
    
    if(cscount==average)
      {
        PRINTFB(I->Obj.G,FB_ObjectMolecule,FB_Details)
          " ObjectMolecule: computing spheroid from states %d to %d.\n",
                 first+1,last
          ENDFB(I->Obj.G);

        spheroid=Alloc(float,nRow);
        
        v=center;
        i = count;
        for(a=0;a<I->NAtom;a++) {
          *(v++)=0.0;
          *(v++)=0.0;
          *(v++)=0.0;
          *(i++)=0;
        }

        for(b=first;b<last;b++) {
          cs=I->CSet[b];
          if(cs) {
            v = cs->Coord;
            for(a=0;a<cs->NIndex;a++) {
              a0=cs->IdxToAtm[a];
              v0 = center+3*a0;
              add3f(v,v0,v0);
              (*(count+a0))++;
              v+=3;
            }
          }
        }

        i=count;
        v=center;
        for(a=0;a<I->NAtom;a++) 
          if(*i) {
            (*(v++))/=(*i);
            (*(v++))/=(*i);
            (*(v++))/=(*i++);
          } else {
            v+=3;
            i++;
          }

        /* now go through and compute radial distances */

        f = fsum;
        s = spheroid;
        for(a=0;a<nRow;a++) {
          *(f++)=0.0;
          *(s++)=0.0; 
        }

        v = max_sq;
        for(a=0;a<I->NAtom;a++)
          *(v++)=0.0;

        for(b=first;b<last;b++) {
          cs=I->CSet[b];
          if(cs) {
            v = cs->Coord;
            for(a=0;a<cs->NIndex;a++) {
              a0=cs->IdxToAtm[a];
              base = (a0*sp->nDot);
              v0 = center+(3*a0);
              subtract3f(v,v0,d0); /* subtract from average */
              l = lengthsq3f(d0);
              if(l>max_sq[a0])
                max_sq[a0]=l;
              if(l>0.0) {
				float isq = (float)(1.0/sqrt1d(l));
                scale3f(d0,isq,n0);
                for(c=0;c<sp->nDot;c++) { /* average over spokes */
                  dp=dot_product3f(sp->dot[c],n0);
                  row = base + c;
                  if(dp>=0.0) {
					ang = (float)((acos(dp)/spheroid_smooth)*(cPI/2.0)); 
                    if(ang>spheroid_fill)
                      ang=spheroid_fill;
                    /* take envelop to zero over that angle */
                    if(ang<=(cPI/2.0)) {
                      dp = (float)cos(ang);
                      fsum[row] += dp*dp;
                      spheroid[row] += l*dp*dp*dp;
                    }
                  }
                }
              }
              v+=3;
            }
          }
        }

        f=fsum;
        s=spheroid;
        for(a=0;a<I->NAtom;a++) {
          min_dist = (float)(spheroid_ratio*sqrt(max_sq[a]));
          if(min_dist<spheroid_minimum)
            min_dist=spheroid_minimum;
          for(b=0;b<sp->nDot;b++) {
            if(*f>R_SMALL4) {
              (*s)=(float)(sqrt1d((*s)/(*(f++)))); /* we put the "rm" in "rms" */
            } else {
              f++;
            }
            if(*s<min_dist)
              *s=min_dist;
            s++;
          }
        }

        /* set frame 0 coordinates to the average */

         cs=I->CSet[first];
         if(cs) {
           v = cs->Coord;
           for(a=0;a<cs->NIndex;a++) {
             a0=cs->IdxToAtm[a];
             v0 = center+3*a0;
             copy3f(v0,v);
             v+=3;
           }
         }

        /* now compute surface normals */

        norm = Alloc(float,nRow*3);
        for(a=0;a<nRow;a++) {
          zero3f(norm+a*3);
        }
        for(a=0;a<I->NAtom;a++) {
          base = a*sp->nDot;
          for(b=0;b<sp->NTri;b++) {
            t0 = sp->Tri[b*3  ];
            t1 = sp->Tri[b*3+1];
            t2 = sp->Tri[b*3+2];
            bt0 = base + t0;
            bt1 = base + t1;
            bt2 = base + t2;
            copy3f(sp->dot[t0],p0);
            copy3f(sp->dot[t1],p1);
            copy3f(sp->dot[t2],p2);
            /*      scale3f(sp->dot[t0].v,spheroid[bt0],p0);
                    scale3f(sp->dot[t1].v,spheroid[bt1],p1);
                    scale3f(sp->dot[t2].v,spheroid[bt2],p2);*/
            subtract3f(p1,p0,d1);
            subtract3f(p2,p0,d2);
            cross_product3f(d1,d2,n0);
            normalize3f(n0);
            v = norm+bt0*3;
            add3f(n0,v,v);
            v = norm+bt1*3;
            add3f(n0,v,v);
            v = norm+bt2*3;
            add3f(n0,v,v);
          }
        }

        f=norm;
        for(a=0;a<I->NAtom;a++) {
          base = a*sp->nDot;
          for(b=0;b<sp->nDot;b++) {
            normalize3f(f);
            f+=3;
          }
        }
  
        if(I->CSet[first]) {
          I->CSet[first]->Spheroid=spheroid;
          I->CSet[first]->SpheroidNormal=norm;
          I->CSet[first]->NSpheroid=nRow;
        } else {
          FreeP(spheroid);
          FreeP(norm);
        }

        for(b=first+1;b<last;b++) { 
          cs=I->CSet[b];
          if(cs) {
            if(cs->fFree)
              cs->fFree(cs);
          }
          I->CSet[b]=NULL;
        }
        
        if(n_state!=first) {
          I->CSet[n_state]=I->CSet[first];
          I->CSet[first]=NULL;
        }
        n_state++;

        cscount=0;
      }
    current++;
  }
  I->NCSet=n_state;
  FreeP(center);
  FreeP(count);
  FreeP(fsum);
  FreeP(max_sq);

  ObjectMoleculeInvalidate(I,cRepSphere,cRepInvProp,-1);
}
/*========================================================================*/
void ObjectMoleculeReplaceAtom(ObjectMolecule *I,int index,AtomInfoType *ai)
{
  if((index>=0)&&(index<=I->NAtom)) {
    memcpy(I->AtomInfo+index,ai,sizeof(AtomInfoType));
    ObjectMoleculeInvalidate(I,cRepAll,cRepInvAtoms,-1);
    /* could we put in a refinement step here? */
  }
}
/*========================================================================*/
void ObjectMoleculePrepareAtom(ObjectMolecule *I,int index,AtomInfoType *ai)
{
  /* match existing properties of the old atom */
  int a;
  AtomInfoType *ai0;

  if((index>=0)&&(index<=I->NAtom)) {
    ai0=I->AtomInfo + index;
    ai->resv=ai0->resv;
    ai->hetatm=ai0->hetatm;
    ai->flags=ai0->flags;
    ai->geom=ai0->geom; /* ?*/
    ai->q=ai0->q;
    ai->b=ai0->b;
    strcpy(ai->chain,ai0->chain);
    strcpy(ai->alt,ai0->alt);
    strcpy(ai->resi,ai0->resi);
    strcpy(ai->segi,ai0->segi);
    strcpy(ai->resn,ai0->resn);  
    AtomInfoAssignColors(I->Obj.G,ai);
    if((ai->elem[0]==ai0->elem[0])&&(ai->elem[1]==ai0->elem[1]))
      ai->color=ai0->color;
    else if((ai->elem[0]=='C')&&(ai->elem[1]==0)) 
      /* carbons are always colored according to the object color */
      ai->color=I->Obj.Color;
    for(a=0;a<cRepCnt;a++)
      ai->visRep[a]=ai0->visRep[a];
    ai->id=-1;
    ai->rank=-1;
    AtomInfoUniquefyNames(I->Obj.G,I->AtomInfo,I->NAtom,ai,1);
    AtomInfoAssignParameters(I->Obj.G,ai);
  }
}
/*========================================================================*/
void ObjectMoleculePreposReplAtom(ObjectMolecule *I,int index,
                                   AtomInfoType *ai)
{
  int n;
  int a1;
  AtomInfoType *ai1;
  float v0[3],v1[3],v[3];
  float d0[3],d,n0[3];
  int cnt;
  float t[3],sum[3];
  int a;
  int ncycle;
  ObjectMoleculeUpdateNeighbors(I);
  for(a=0;a<I->NCSet;a++) {
    if(I->CSet[a]) {
      if(ObjectMoleculeGetAtomVertex(I,a,index,v0)) {
        copy3f(v0,v); /* default is direct superposition */
        ncycle=-1;
        while(ncycle) {
          cnt = 0;
          n = I->Neighbor[index];
          n++; /* skip count */
          zero3f(sum);
          while(1) { /* look for an attached non-hydrogen as a base */
            a1 = I->Neighbor[n];
            n+=2;
            if(a1<0) break;
            ai1=I->AtomInfo+a1;
            if(ai1->protons!=1) 
              if(ObjectMoleculeGetAtomVertex(I,a,a1,v1)) {        
                d = AtomInfoGetBondLength(I->Obj.G,ai,ai1);
                subtract3f(v0,v1,n0);
                normalize3f(n0);
                scale3f(n0,d,d0);
                add3f(d0,v1,t);
                add3f(t,sum,sum);
                cnt++;
              }
          }
          if(cnt) {
            scale3f(sum,1.0F/cnt,sum);
            copy3f(sum,v0);
            if((cnt>1)&&(ncycle<0))
              ncycle=5;
          }
          ncycle=abs(ncycle)-1;
        }
        if(cnt) copy3f(sum,v);
        ObjectMoleculeSetAtomVertex(I,a,index,v);            
      }
    }
  }
}
/*========================================================================*/
void ObjectMoleculeSaveUndo(ObjectMolecule *I,int state,int log)
{
  CoordSet *cs;
  PyMOLGlobals *G = I->Obj.G;
  FreeP(I->UndoCoord[I->UndoIter]);
  I->UndoState[I->UndoIter]=-1;
  if(state<0) state=0;
  if(I->NCSet==1) state=0;
  state = state % I->NCSet;
  cs = I->CSet[state];
  if(cs) {
    I->UndoCoord[I->UndoIter] = Alloc(float,cs->NIndex*3);
    memcpy(I->UndoCoord[I->UndoIter],cs->Coord,sizeof(float)*cs->NIndex*3);
    I->UndoState[I->UndoIter]=state;
    I->UndoNIndex[I->UndoIter] = cs->NIndex;
  }
  I->UndoIter=cUndoMask&(I->UndoIter+1);
  ExecutiveSetLastObjectEdited(G,(CObject*)I);
  if(log) {
    OrthoLineType line;
    if(SettingGet(I->Obj.G,cSetting_logging)) {
      sprintf(line,"cmd.push_undo(\"%s\",%d)\n",I->Obj.Name,state+1);
      PLog(G,line,cPLog_no_flush);
    }
  }

}
/*========================================================================*/
void ObjectMoleculeUndo(ObjectMolecule *I,int dir)
{
  CoordSet *cs;
  int state;

  FreeP(I->UndoCoord[I->UndoIter]);
  I->UndoState[I->UndoIter]=-1;
  state=SceneGetState(I->Obj.G);
  if(state<0) state=0;
  if(I->NCSet==1) state=0;
  state = state % I->NCSet;
  cs = I->CSet[state];
  if(cs) {
    I->UndoCoord[I->UndoIter] = Alloc(float,cs->NIndex*3);
    memcpy(I->UndoCoord[I->UndoIter],cs->Coord,sizeof(float)*cs->NIndex*3);
    I->UndoState[I->UndoIter]=state;
    I->UndoNIndex[I->UndoIter] = cs->NIndex;
  }

  I->UndoIter=cUndoMask&(I->UndoIter+dir);
  if(!I->UndoCoord[I->UndoIter])
    I->UndoIter=cUndoMask&(I->UndoIter-dir);

  if(I->UndoState[I->UndoIter]>=0) {
    state=I->UndoState[I->UndoIter];
    if(state<0) state=0;
    
    if(I->NCSet==1) state=0;
    state = state % I->NCSet;
    cs = I->CSet[state];
    if(cs) {
      if(cs->NIndex==I->UndoNIndex[I->UndoIter]) {
        memcpy(cs->Coord,I->UndoCoord[I->UndoIter],sizeof(float)*cs->NIndex*3);
        I->UndoState[I->UndoIter]=-1;
        FreeP(I->UndoCoord[I->UndoIter]);
        if(cs->fInvalidateRep)
          cs->fInvalidateRep(cs,cRepAll,cRepInvCoord);
        SceneChanged(I->Obj.G);
      }
    }
  }
}
/*========================================================================*/
int ObjectMoleculeAddBond(ObjectMolecule *I,int sele0,int sele1,int order)
{
  int a1,a2;
  AtomInfoType *ai1,*ai2;
  int s1,s2;
  int c = 0;
  BondType *bnd;

  ai1=I->AtomInfo;
  for(a1=0;a1<I->NAtom;a1++) {
    s1=ai1->selEntry;
    if(SelectorIsMember(I->Obj.G,s1,sele0)) {
      ai2=I->AtomInfo;
      for(a2=0;a2<I->NAtom;a2++) {
        s2=ai2->selEntry;
        if(SelectorIsMember(I->Obj.G,s2,sele1)) {
          if(!I->Bond)
            I->Bond=VLACalloc(BondType,1); 
          if(I->Bond) {
            VLACheck(I->Bond,BondType,I->NBond);
            bnd = I->Bond+(I->NBond);
            bnd->index[0]=a1;
            bnd->index[1]=a2;                      
            bnd->order=order;
            bnd->stereo = 0;
            bnd->id=-1;
            I->NBond++;
            c++;
            I->AtomInfo[a1].chemFlag=false;
            I->AtomInfo[a2].chemFlag=false;
          }
        }
        ai2++;
      }
    }
    ai1++;
  }
  if(c) {
    ObjectMoleculeInvalidate(I,cRepLine,cRepInvBonds,-1);
    ObjectMoleculeInvalidate(I,cRepCyl,cRepInvBonds,-1);
    ObjectMoleculeInvalidate(I,cRepNonbonded,cRepInvBonds,-1);
    ObjectMoleculeInvalidate(I,cRepNonbondedSphere,cRepInvBonds,-1);
    ObjectMoleculeInvalidate(I,cRepRibbon,cRepInvBonds,-1);
    ObjectMoleculeInvalidate(I,cRepCartoon,cRepInvBonds,-1);
    ObjectMoleculeUpdateIDNumbers(I);
  }
  return(c);    
}
/*========================================================================*/
int ObjectMoleculeAdjustBonds(ObjectMolecule *I,int sele0,int sele1,int mode,int order)
{
  int a0,a1;
  int offset=0;
  BondType *b0;
  int both;
  int s;
  int a;

  offset=0;
  if(I->Bond) {
    b0=I->Bond;
    for(a=0;a<I->NBond;a++) {
      a0=b0->index[0];
      a1=b0->index[1];
      
      both=0;
      s=I->AtomInfo[a0].selEntry;
      if(SelectorIsMember(I->Obj.G,s,sele0))
        both++;
      s=I->AtomInfo[a1].selEntry;
      if(SelectorIsMember(I->Obj.G,s,sele1))
        both++;
      if(both<2) { /* reverse combo */
        both=0;
        s=I->AtomInfo[a1].selEntry;
        if(SelectorIsMember(I->Obj.G,s,sele0))
          both++;
        s=I->AtomInfo[a0].selEntry;
        if(SelectorIsMember(I->Obj.G,s,sele1))
          both++;
      }
      
      if(both==2) {
        switch(mode) {
        case 0: /* cycle */
          b0->order++;
          if(b0->order>3)
            b0->order=1;
          I->AtomInfo[a0].chemFlag=false;
          I->AtomInfo[a1].chemFlag=false;
          break;
        case 1: /* set */
          b0->order=order;
          I->AtomInfo[a0].chemFlag=false;
          I->AtomInfo[a1].chemFlag=false;
          break;
        }
        ObjectMoleculeInvalidate(I,cRepLine,cRepInvBonds,-1);
        ObjectMoleculeInvalidate(I,cRepCyl,cRepInvBonds,-1);
        ObjectMoleculeInvalidate(I,cRepNonbonded,cRepInvBonds,-1);
        ObjectMoleculeInvalidate(I,cRepNonbondedSphere,cRepInvBonds,-1);
        ObjectMoleculeInvalidate(I,cRepRibbon,cRepInvBonds,-1);
        ObjectMoleculeInvalidate(I,cRepCartoon,cRepInvBonds,-1);
      }
      b0++;
    }
  }
  return(-offset);
}
/*========================================================================*/
int ObjectMoleculeRemoveBonds(ObjectMolecule *I,int sele0,int sele1)
{
  int a0,a1;
  int offset=0;
  BondType *b0,*b1;
  int both;
  int s;
  int a;

  if(I->Bond) {
    offset=0;
    b0=I->Bond;
    b1=I->Bond;
    for(a=0;a<I->NBond;a++) {
      a0=b0->index[0];
      a1=b0->index[1];
      
      both=0;
      s=I->AtomInfo[a0].selEntry;
      if(SelectorIsMember(I->Obj.G,s,sele0))
        both++;
      s=I->AtomInfo[a1].selEntry;
      if(SelectorIsMember(I->Obj.G,s,sele1))
        both++;
      if(both<2) { /* reverse combo */
        both=0;
        s=I->AtomInfo[a1].selEntry;
        if(SelectorIsMember(I->Obj.G,s,sele0))
          both++;
        s=I->AtomInfo[a0].selEntry;
        if(SelectorIsMember(I->Obj.G,s,sele1))
          both++;
      }
      
      if(both==2) {
        AtomInfoPurgeBond(I->Obj.G,b0);
        offset--;
        b0++;
        I->AtomInfo[a0].chemFlag=false;
        I->AtomInfo[a1].chemFlag=false;
      } else if(offset) {
        *(b1++)=*(b0++); /* copy bond info */
      } else {
        *(b1++)=*(b0++); /* copy bond info */
      }
    }
    if(offset) {
      I->NBond += offset;
      VLASize(I->Bond,BondType,I->NBond);
      ObjectMoleculeInvalidate(I,cRepLine,cRepInvBonds,-1);
      ObjectMoleculeInvalidate(I,cRepCyl,cRepInvBonds,-1);
      ObjectMoleculeInvalidate(I,cRepNonbonded,cRepInvBonds,-1);
      ObjectMoleculeInvalidate(I,cRepNonbondedSphere,cRepInvBonds,-1);
      ObjectMoleculeInvalidate(I,cRepRibbon,cRepInvBonds,-1);
      ObjectMoleculeInvalidate(I,cRepCartoon,cRepInvBonds,-1);
    }
  }

  return(-offset);
}
/*========================================================================*/
void ObjectMoleculePurge(ObjectMolecule *I)
{
  PyMOLGlobals *G = I->Obj.G;
  int a,a0,a1;
  int *oldToNew = NULL;
  int offset=0;
  BondType *b0,*b1;
  AtomInfoType *ai0,*ai1;
  
  PRINTFD(I->Obj.G,FB_ObjectMolecule)
    " ObjMolPurge-Debug: step 1, delete object selection\n"
    ENDFD;

  SelectorDelete(G,I->Obj.Name); /* remove the object selection and free up any selection entries*/
  /* note that we don't delete atom selection members -- those may be needed in the new object */

  PRINTFD(G,FB_ObjectMolecule)
    " ObjMolPurge-Debug: step 2, purge coordinate sets\n"
    ENDFD;

  for(a=0;a<I->NCSet;a++)
	 if(I->CSet[a]) 
      CoordSetPurge(I->CSet[a]);
  if(I->CSTmpl) {
    CoordSetPurge(I->CSTmpl);
  }
  PRINTFD(I->Obj.G,FB_ObjectMolecule)
    " ObjMolPurge-Debug: step 3, old-to-new mapping\n"
    ENDFD;

  oldToNew = Alloc(int,I->NAtom);
  ai0=I->AtomInfo;
  ai1=I->AtomInfo;
  for(a=0;a<I->NAtom;a++) {
    if(ai0->deleteFlag) {
      AtomInfoPurge(G,ai0);
      offset--;
      ai0++;
      oldToNew[a]=-1;
    } else if(offset) {
      *(ai1++)=*(ai0++);
      oldToNew[a]=a+offset;
    } else {
      oldToNew[a]=a;
      ai0++;
      ai1++;
    }
  }

  if(offset) {
    I->NAtom += offset;
    VLASize(I->AtomInfo,AtomInfoType,I->NAtom);
    for(a=0;a<I->NCSet;a++)
      if(I->CSet[a])
        CoordSetAdjustAtmIdx(I->CSet[a],oldToNew,I->NAtom);
  }

  PRINTFD(I->Obj.G,FB_ObjectMolecule)
    " ObjMolPurge-Debug: step 4, bonds\n"
    ENDFD;
  
  offset=0;
  b0=I->Bond;
  b1=I->Bond;
  for(a=0;a<I->NBond;a++) {
    a0=b0->index[0];
    a1=b0->index[1];
    if((oldToNew[a0]<0)||(oldToNew[a1]<0)) { 
      /* deleting bond */
      AtomInfoPurgeBond(I->Obj.G,b0);
      offset--;
      b0++;
    } else if(offset) {
      *b1=*b0;
      b1->index[0]=oldToNew[a0]; /* copy bond info */
      b1->index[1]=oldToNew[a1];
      b0++;
      b1++;
    } else {
      *b1=*b0;
      b1->index[0]=oldToNew[a0]; /* copy bond info */
      b1->index[1]=oldToNew[a1];
      b0++;
      b1++; /* TODO check reasoning agaist above */
    }
  }
  if(offset) {
    I->NBond += offset;
    VLASize(I->Bond,BondType,I->NBond);
  }
  FreeP(oldToNew);

  PRINTFD(I->Obj.G,FB_ObjectMolecule)
    " ObjMolPurge-Debug: step 5, invalidate...\n"
    ENDFD;

  ObjectMoleculeInvalidate(I,cRepAll,cRepInvAtoms,-1);

  PRINTFD(I->Obj.G,FB_ObjectMolecule)
    " ObjMolPurge-Debug: leaving...\n"
    ENDFD;

}
/*========================================================================*/
int ObjectMoleculeGetAtomGeometry(ObjectMolecule *I,int state,int at)
{
  /* this determines hybridization from coordinates in those few cases
   * where it is unambiguous */

  int result = -1;
  int n,nn;
  float v0[3],v1[3],v2[3],v3[3];
  float d1[3],d2[3],d3[3];
  float cp1[3],cp2[3],cp3[3];
  float avg;
  float dp;
  n  = I->Neighbor[at];
  nn = I->Neighbor[n++]; /* get count */
  if(nn==4) 
    result = cAtomInfoTetrahedral; 
  else if(nn==3) {
    /* check cross products */
    ObjectMoleculeGetAtomVertex(I,state,at,v0);    
    ObjectMoleculeGetAtomVertex(I,state,I->Neighbor[n],v1);
    ObjectMoleculeGetAtomVertex(I,state,I->Neighbor[n+2],v2);
    ObjectMoleculeGetAtomVertex(I,state,I->Neighbor[n+4],v3);
    subtract3f(v1,v0,d1);
    subtract3f(v2,v0,d2);
    subtract3f(v3,v0,d3);
    cross_product3f(d1,d2,cp1);
    cross_product3f(d2,d3,cp2);
    cross_product3f(d3,d1,cp3);
    normalize3f(cp1);
    normalize3f(cp2);
    normalize3f(cp3);
    avg=(dot_product3f(cp1,cp2)+
         dot_product3f(cp2,cp3)+
         dot_product3f(cp3,cp1))/3.0F;
    if(avg>0.75)
      result=cAtomInfoPlaner;
    else
      result=cAtomInfoTetrahedral;
  } else if(nn==2) {
    ObjectMoleculeGetAtomVertex(I,state,at,v0);    
    ObjectMoleculeGetAtomVertex(I,state,I->Neighbor[n],v1);
    ObjectMoleculeGetAtomVertex(I,state,I->Neighbor[n+2],v2);
    subtract3f(v1,v0,d1);
    subtract3f(v2,v0,d2);
    normalize3f(d1);
    normalize3f(d2);
    dp = dot_product3f(d1,d2);
    if(dp<-0.75)
      result=cAtomInfoLinear;
  }
  return(result);
}
/*========================================================================*/
void ObjectMoleculeInferChemForProtein(ObjectMolecule *I,int state)
{
  /* Infers chemical relations for a molecules under protein assumptions.
   * 
   * NOTE: this routine needs an all-atom model (with hydrogens!)
   * and it will make mistakes on non-protein atoms (if they haven't
   * already been assigned)
  */

  int a,n,a0,a1,nn;
  int changedFlag = true;
  
  AtomInfoType *ai,*ai0,*ai1=NULL;
  
  ObjectMoleculeUpdateNeighbors(I);

  /* first, try to find all amids and acids */
  while(changedFlag) {
    changedFlag=false;
    for(a=0;a<I->NAtom;a++) {
      ai=I->AtomInfo+a;
      if(ai->chemFlag) {
        if(ai->geom==cAtomInfoPlaner)
          if(ai->protons == cAN_C) {
            n = I->Neighbor[a];
            nn = I->Neighbor[n++];
            if(nn>1) {
              a1 = -1;
              while(1) {
                a0 = I->Neighbor[n];
                n+=2;
                if(a0<0) break;
                ai0 = I->AtomInfo+a0;
                if((ai0->protons==cAN_O)&&(!ai0->chemFlag)) {
                  a1=a0;
                  ai1=ai0; /* found candidate carbonyl */
                  break;
                }
              }
              if(a1>0) {
                n = I->Neighbor[a]+1;
                while(1) {
                  a0 = I->Neighbor[n];
                  if(a0<0) break;
                  n+=2;
                  if(a0!=a1) {
                    ai0 = I->AtomInfo+a0;
                    if(ai0->protons==cAN_O) {
                      if(!ai0->chemFlag) {
                        ai0->chemFlag=true; /* acid */
                        ai0->geom=cAtomInfoPlaner;
                        ai0->valence=1;
                        ai1->chemFlag=true;
                        ai1->geom=cAtomInfoPlaner;
                        ai1->valence=1;
                        changedFlag=true;
                        break;
                      }
                    } else if(ai0->protons==cAN_N) {
                      if(!ai0->chemFlag) { 
                        ai0->chemFlag=true; /* amide N */ 
                        ai0->geom=cAtomInfoPlaner;                            
                        ai0->valence=3;
                        ai1->chemFlag=true; /* amide =O */ 
                        ai1->geom=cAtomInfoPlaner;
                        ai1->valence=1;
                        changedFlag=true;
                        break;
                      } else if(ai0->geom==cAtomInfoPlaner) {
                        ai1->chemFlag=true; /* amide =O */
                        ai1->geom=cAtomInfoPlaner;
                        ai1->valence=1;
                        changedFlag=true;
                        break;
                      }
                    }
                  }
                }
              }
            }
          }
      }
    }
  }
  /* then handle aldehydes and amines (partial amides - both missing a valence) */
  
  changedFlag=true;
  while(changedFlag) {
    changedFlag=false;
    for(a=0;a<I->NAtom;a++) {
      ai=I->AtomInfo+a;
      if(!ai->chemFlag) {
        if(ai->protons==cAN_C) {
          n = I->Neighbor[a];
          nn = I->Neighbor[n++];
          if(nn>1) {
            a1 = -1;
            while(1) {
              a0 = I->Neighbor[n];
              n+=2;
              if(a0<0) break;
              ai0 = I->AtomInfo+a0;
              if((ai0->protons==cAN_O)&&(!ai0->chemFlag)) { /* =O */
                ai->chemFlag=true; 
                ai->geom=cAtomInfoPlaner;
                ai->valence=1;
                ai0->chemFlag=true;
                ai0->geom=cAtomInfoPlaner;
                ai0->valence=3;
                changedFlag=true;
                break;
              }
            }
          }
        }
        else if(ai->protons==cAN_N)
          {
            if((!ai->chemFlag)||ai->geom!=cAtomInfoLinear) {
              if(ai->formalCharge==0.0) {
                ai->chemFlag=true; 
                ai->geom=cAtomInfoPlaner;
                ai->valence=3;
              }
            }
          }
      }
    }
  }

}
/*========================================================================*/
void ObjectMoleculeInferChemFromNeighGeom(ObjectMolecule *I,int state)
{
  /* infers chemical relations from neighbors and geometry 
  * NOTE: very limited in scope */

  int a,n,a0,nn;
  int changedFlag=true;
  int geom;
  int carbonVal[10];
  
  AtomInfoType *ai,*ai2;

  carbonVal[cAtomInfoTetrahedral] = 4;
  carbonVal[cAtomInfoPlaner] = 3;
  carbonVal[cAtomInfoLinear] = 2;
  
  ObjectMoleculeUpdateNeighbors(I);
  while(changedFlag) {
    changedFlag=false;
    for(a=0;a<I->NAtom;a++) {
      ai=I->AtomInfo+a;
      if(!ai->chemFlag) {
        geom=ObjectMoleculeGetAtomGeometry(I,state,a);
        switch(ai->protons) {
        case cAN_K:
          ai->chemFlag=1;
          ai->geom=cAtomInfoNone;
          ai->valence=0;
          break;
        case cAN_H:
        case cAN_F:
        case cAN_I:
        case cAN_Br:
          ai->chemFlag=1;
          ai->geom=cAtomInfoSingle;
          ai->valence=1;
          break;
        case cAN_O:
          n = I->Neighbor[a];
          nn = I->Neighbor[n++];
          if(nn!=1) { /* water, hydroxy, ether */
            ai->chemFlag=1;
            ai->geom=cAtomInfoTetrahedral;
            ai->valence=2;
          } else { /* hydroxy or carbonyl? check carbon geometry */
            a0 = I->Neighbor[n+2];
            ai2=I->AtomInfo+a0;
            if(ai2->chemFlag) {
              if((ai2->geom==cAtomInfoTetrahedral)||
                 (ai2->geom==cAtomInfoLinear)) {
                ai->chemFlag=1; /* hydroxy */
                ai->geom=cAtomInfoTetrahedral;
                ai->valence=2;
              }
            }
          }
          break;
        case cAN_C:
          if(geom>=0) {
            ai->geom = geom;
            ai->valence = carbonVal[geom];
            ai->chemFlag=true;
          } else {
            n = I->Neighbor[a];
            nn = I->Neighbor[n++];
            if(nn==1) { /* only one neighbor */
              ai2=I->AtomInfo+I->Neighbor[n];
              if(ai2->chemFlag&&(ai2->geom==cAtomInfoTetrahedral)) {
                ai->chemFlag=true; /* singleton carbon bonded to tetC must be tetC */
                ai->geom=cAtomInfoTetrahedral;
                ai->valence=4;
              }
            }
          }
          break;
        case cAN_N:
          if(geom==cAtomInfoPlaner) {
            ai->chemFlag=true;
            ai->geom=cAtomInfoPlaner;
            ai->valence=3;
          } else if(geom==cAtomInfoTetrahedral) {
            ai->chemFlag=true;
            ai->geom=cAtomInfoTetrahedral;
            ai->valence=4;
          }
          break;
        case cAN_S:
          n = I->Neighbor[a];
          nn = I->Neighbor[n++];
          if(nn==4) { /* sulfone */
            ai->chemFlag=true;
            ai->geom=cAtomInfoTetrahedral;
            ai->valence=4;
          } else if(nn==3) { /* suloxide */
            ai->chemFlag=true;
            ai->geom=cAtomInfoTetrahedral;
            ai->valence=3;
          } else if(nn==2) { /* thioether */
            ai->chemFlag=true;
            ai->geom=cAtomInfoTetrahedral;
            ai->valence=2;
          }
          break;
        case cAN_Cl:
          ai->chemFlag=1;
          if(ai->formalCharge==0.0) {
            ai->geom=cAtomInfoSingle;
            ai->valence=1;
          } else {
            ai->geom=cAtomInfoNone;
            ai->valence=0;
          }
          break;
        }
        if(ai->chemFlag)
          changedFlag=true;
      }
    }
  }
}

/*========================================================================*/
void ObjectMoleculeInferHBondFromChem(ObjectMolecule *I)
{
  int a;
  AtomInfoType *ai;
  int a1;
  int n,nn;
  int has_hydro,may_have_lone_pair;
  /* initialize accumulators on uncategorized atoms */

  ObjectMoleculeUpdateNeighbors(I);
  ai=I->AtomInfo;
  for(a=0;a<I->NAtom;a++) {
    n = I->Neighbor[a];
    nn = I->Neighbor[n++];
    ai->hb_donor = false;
    ai->hb_acceptor = false;

    has_hydro = (nn < ai->valence); /* implicit hydrogens? */

    if(!has_hydro) {
      /* explicit hydrogens? */
      has_hydro = false;
      switch(ai->protons) {
      case cAN_N:
      case cAN_O:
        while((a1 = I->Neighbor[n])>=0) {
          n+=2; 
          if(I->AtomInfo[a1].protons==1) {
            has_hydro=true;
            break;
          }
        }
        break;
      }
    }
    
    switch(ai->protons) {
/* cat-ions, lewis acids etc. */
    case cAN_Fe:
    case cAN_Ca: 
    case cAN_Cu:
    case cAN_K:
    case cAN_Na:
    case cAN_Mg:
    case cAN_Zn:
    case cAN_Hg:
    case cAN_Sr:
    case cAN_Ba:
      ai->hb_donor=true;
      break;
    case cAN_N:
      if(has_hydro)
        ai->hb_donor=true;
      else {
        may_have_lone_pair = false;
        
        if((!has_hydro)&&(ai->protons==cAN_N)) {
          n = I->Neighbor[a] + 1;
          while(I->Neighbor[n]>=0) {
            if(I->Neighbor[n+1]>1) { /* any double/triple/delocalized bonds? */
              may_have_lone_pair = true;
            }
            n+=2; 
          }
        }
        if((ai->formalCharge<=0)&&may_have_lone_pair) {
          ai->hb_acceptor = true;
        }
      }
      break;
    case cAN_O:
      if(has_hydro)
        ai->hb_donor = true;
      if(ai->formalCharge<=0)
        ai->hb_acceptor = true;
      break;
    }
    ai++;
  }
  
}

/*========================================================================*/
void ObjectMoleculeInferChemFromBonds(ObjectMolecule *I,int state)
{

  int a,b;
  BondType *b0;
  AtomInfoType *ai,*ai0,*ai1=NULL;
  int a0,a1;
  int expect,order;
  int n,nn;
  int changedFlag;
  /* initialize accumulators on uncategorized atoms */

  ObjectMoleculeUpdateNeighbors(I);
  ai=I->AtomInfo;
  for(a=0;a<I->NAtom;a++) {
    if(!ai->chemFlag) {
      ai->geom=0;
      ai->valence=0;
    }
    ai++;
  }
  
  /* find maximum bond order for each atom */

  b0=I->Bond;
  for(b=0;b<I->NBond;b++) {
    a0 = b0->index[0];
    a1 = b0->index[1];
    ai0=I->AtomInfo + a0;
    ai1=I->AtomInfo + a1;
    order = b0->order;
    b0++;
    if(!ai0->chemFlag) {
      if(order>ai0->geom)
        ai0->geom=order;
      ai0->valence+=order;
    }
    if(!ai1->chemFlag) {
      if(order>ai1->geom)
        ai1->geom=order;
      ai1->valence+=order;
    }
    if(order==3) { 
      /* override existing chemistry * this is a temp fix to a pressing problem...
         we need to rethink the chemisty assignment ordering (should bond
         information come first? */
      ai0->geom = cAtomInfoLinear;
      ai1->geom = cAtomInfoLinear;
      switch(ai0->protons) {
      case cAN_C:
        ai0->valence=2;
        break;
      default:
        ai0->valence=1;
      }
      switch(ai1->protons) {
      case cAN_C:
        ai1->valence=2;
        break;
      default:
        ai1->valence=1;
      }
      ai0->chemFlag=true;
      ai1->chemFlag=true;
    } else if(order==4) {
      ai0->geom = cAtomInfoPlaner;
      ai1->geom = cAtomInfoPlaner;
      switch(ai0->protons) {
      case cAN_O:
        ai0->valence=1;
        break;
      case cAN_N:
        ai0->valence=2;
        break;
      case cAN_C:
        ai0->valence=3;
        break;
      default:
        ai0->valence=4;
      }
      switch(ai1->protons) {
      case cAN_O:
        ai1->valence=1;
        break;
      case cAN_N:
        ai1->valence=2;
        break;
      case cAN_C:
        ai1->valence=3;
        break;
      default:
        ai1->valence=1;
      }
      ai0->chemFlag=true;
      ai1->chemFlag=true;
    }
  }

  /* now set up valences and geometries */

  ai=I->AtomInfo;
  for(a=0;a<I->NAtom;a++) {
    if(!ai->chemFlag) {
      expect = AtomInfoGetExpectedValence(I->Obj.G,ai);
      n = I->Neighbor[a];
      nn = I->Neighbor[n++];
      if(ai->geom==3) {
        ai->geom = cAtomInfoLinear;
        switch(ai->protons) {
        case cAN_C:
          ai->valence=2;
          break;
        default:
          ai->valence=1;
        }
        ai->chemFlag=true;
      } else {
      if(expect<0) 
        expect = -expect; /* for now, just ignore this issue */
      /*      printf("%d %d %d %d\n",ai->geom,ai->valence,nn,expect);*/
      if(ai->valence==expect) { /* sum of bond orders equals valence */
        ai->chemFlag=true;
        ai->valence=nn;
        switch(ai->geom) { /* max bond order observed */
        case 0: ai->geom = cAtomInfoNone; break;
        case 2: ai->geom = cAtomInfoPlaner; break;
        case 3: ai->geom = cAtomInfoLinear; break;
        default: 
          if(expect==1) 
            ai->geom = cAtomInfoSingle;
          else
            ai->geom = cAtomInfoTetrahedral; 
          break;            
        }
      } else if(ai->valence<expect) { /* missing a bond */
        ai->chemFlag=true;
        ai->valence=nn+(expect-ai->valence); 
        switch(ai->geom) { 
        case 2: ai->geom = cAtomInfoPlaner; break;
        case 3: ai->geom = cAtomInfoLinear; break;
        default: 
          if(expect==1) 
            ai->geom = cAtomInfoSingle;
          else
            ai->geom = cAtomInfoTetrahedral; 
          break;
        }
      } else if(ai->valence>expect) {
        ai->chemFlag=true;
        ai->valence=nn;
        switch(ai->geom) { 
        case 2: ai->geom = cAtomInfoPlaner; break;
        case 3: ai->geom = cAtomInfoLinear; break;
        default: 
          if(expect==1) 
            ai->geom = cAtomInfoSingle;
          else
            ai->geom = cAtomInfoTetrahedral; 
          break;
        }
        if(nn>3)
          ai->geom = cAtomInfoTetrahedral;
      }
      }
    }
    ai++;
  }

  /* now go through and make sure conjugated amines are planer */
  changedFlag=true;
  while(changedFlag) {
    changedFlag=false;
    ai=I->AtomInfo;
    for(a=0;a<I->NAtom;a++) {
      if(ai->chemFlag) {
        if(ai->protons==cAN_N) 
          if(ai->formalCharge==0) 
            if(ai->geom==cAtomInfoTetrahedral) { 
              /* search for uncharged tetrahedral nitrogen */
              n = I->Neighbor[a]+1;
              while(1) {
                a0 = I->Neighbor[n];
                n+=2;
                if(a0<0) break;
                ai0 = I->AtomInfo+a0;
                if((ai0->chemFlag)&&(ai0->geom==cAtomInfoPlaner)&&
                   ((ai0->protons==cAN_C)||(ai0->protons==cAN_N))) {
                  ai->geom=cAtomInfoPlaner; /* found probable delocalization */
                  ai->valence=3; /* just in case...*/
                  changedFlag=true;
                  break;
                }
              }
            }
      }
      ai++; 
    }
  }

  /* now go through and make sure conjugated anions are planer */
  changedFlag=true;
  while(changedFlag) {
    changedFlag=false;
    ai=I->AtomInfo;
    for(a=0;a<I->NAtom;a++) {
      if(ai->chemFlag) {
        if(ai->protons==cAN_O) 
          if(ai->formalCharge==-1) 
            if((ai->geom==cAtomInfoTetrahedral)||
               (ai->geom==cAtomInfoSingle)) { /* search for anionic tetrahedral oxygen */
              n = I->Neighbor[a]+1;
              while(1) {
                a0 = I->Neighbor[n];
                n+=2;
                if(a0<0) break;
                ai0 = I->AtomInfo+a0;
                if((ai0->chemFlag)&&(ai0->geom==cAtomInfoPlaner)&&
                   ((ai0->protons==cAN_C)||(ai0->protons==cAN_N))) {
                  ai->geom=cAtomInfoPlaner; /* found probable delocalization */
                  changedFlag=true;
                  break;
                }
              }
            }
      }
      ai++;
    }
  }


}
/*========================================================================*/
int ObjectMoleculeTransformSelection(ObjectMolecule *I,int state,
                                     int sele,float *matrix,int log,
                                     char *sname,int homogenous,int global) 
{
  /* if sele == -1, then the whole object state is transformed */
  PyMOLGlobals *G = I->Obj.G;
  int a,s;
  int flag=false;
  CoordSet *cs;
  AtomInfoType *ai;
  int logging;
  int all_states=false,inp_state;
  int ok=true;
  float homo_matrix[16],tmp_matrix[16],*input_matrix = matrix;

  inp_state=state;
  if(state==-1) 
    state=ObjectGetCurrentState(&I->Obj,false);
  if(state<0) {
    all_states=true;
    state=-1;
  }
  PRINTFD(I->Obj.G,FB_ObjectMolecule)
    "ObjMolTransSele-Debug: state %d\n",state
    ENDFD;
  while(1) {
    if(all_states) {
      state++;
      if(state>=I->NCSet)
        break;
    }
    if(state<I->NCSet) {
      cs = I->CSet[state];
      if(cs) {
        int use_matrices = SettingGet_b(I->Obj.G,I->Obj.Setting,
                                        NULL,cSetting_matrix_mode);

        if(global && !homogenous) { /* convert matrix to homogenous */
          convertTTTfR44f(matrix,homo_matrix);
          matrix = homo_matrix;
          input_matrix = homo_matrix;
          homogenous = true;
        }

        if(global && ((use_matrices&&cs->State.Matrix) ||
                      I->Obj.TTTFlag)) { 
          /* if input coordinates are in the global system,
             they may need to be converted to local coordinates */
          
          matrix = input_matrix;
          
          /* global to object */

          if(I->Obj.TTTFlag) {
            float ttt[16];
            if(matrix!=tmp_matrix) {
              copy44f(matrix,tmp_matrix);
            }
            convertTTTfR44f(I->Obj.TTT, ttt);
            {
              float ttt_inv[16];
              invert_special44f44f(ttt,ttt_inv);
              left_multiply44f44f(ttt_inv,tmp_matrix);
              right_multiply44f44f(tmp_matrix,ttt);
            }
            matrix = tmp_matrix;
          }

          /* object to state */

          if(use_matrices) {
            if(cs->State.Matrix) {
              double tmp_double[16], tmp_inv[16];
              copy44f44d(matrix, tmp_double);
              invert_special44d44d(cs->State.Matrix, tmp_inv);
              left_multiply44d44d(tmp_inv,tmp_double);
              right_multiply44d44d(tmp_double,cs->State.Matrix);
              copy44d44f(tmp_double,tmp_matrix);
              matrix=tmp_matrix;
            }
          }
        }

        if(sele>=0) { /* transforming select atoms */
          ai=I->AtomInfo;
          for(a=0;a<I->NAtom;a++) {
            s=ai->selEntry;
            if(!(ai->protekted==1))
              if(SelectorIsMember(I->Obj.G,s,sele))
                {
                  if(homogenous) 
                    CoordSetTransformAtomR44f(cs,a,matrix);
                  else
                    CoordSetTransformAtomTTTf(cs,a,matrix);                    
                  flag=true;
                }
            ai++;
          }
        } else { /* transforming whole coordinate set */
          if(!use_matrices) {
            ai=I->AtomInfo;
            for(a=0;a<I->NAtom;a++) {
              if(!(ai->protekted==1)) {
                if(homogenous) 
                  CoordSetTransformAtomR44f(cs,a,matrix);
                else
                  CoordSetTransformAtomTTTf(cs,a,matrix);                
              }
              ai++;
            }
            flag=true;
            CoordSetRecordTxfApplied(cs,matrix,homogenous);
          } else {
            ObjectMoleculeTransformState44f(I,state,matrix,false,homogenous,false);
          }
        }
        if(flag) {
          cs->fInvalidateRep(cs,cRepAll,cRepInvCoord);
          ExecutiveUpdateCoordDepends(I->Obj.G,I);
        }
      }
    }
    if(!all_states)
      break;
  }

  
  if(log) {
    OrthoLineType line;
    WordType sele_str = ",'";
    logging = (int)SettingGet(I->Obj.G,cSetting_logging);
    if(sele>=0) {
      strcat(sele_str,sname);
      strcat(sele_str,"'");
    }
    else
      sele_str[0]=0;
    switch(logging) {
    case cPLog_pml:
      sprintf(line,
              "_ cmd.transform_object('%s',[\\\n_ %15.9f,%15.9f,%15.9f,%15.9f,\\\n_ %15.9f,%15.9f,%15.9f,%15.9f,\\\n_ %15.9f,%15.9f,%15.9f,%15.9f,\\\n_ %15.9f,%15.9f,%15.9f,%15.9f\\\n_     ],%d,%d%s,%d)\n",
              I->Obj.Name,
              matrix[ 0],matrix[ 1],matrix[ 2],matrix[ 3],
              matrix[ 4],matrix[ 5],matrix[ 6],matrix[ 7],
              matrix[ 8],matrix[ 9],matrix[10],matrix[11],
              matrix[12],matrix[13],matrix[14],matrix[15],
              inp_state+1,log,sele_str,homogenous);
      PLog(G,line,cPLog_no_flush);
      break;
    case cPLog_pym:
      
      sprintf(line,
              "cmd.transform_object('%s',[\n%15.9f,%15.9f,%15.9f,%15.9f,\n%15.9f,%15.9f,%15.9f,%15.9f,\n%15.9f,%15.9f,%15.9f,%15.9f,\n%15.9f,%15.9f,%15.9f,%15.9f\n],%d,%d%s,%d)\n",
              I->Obj.Name,
              matrix[ 0],matrix[ 1],matrix[ 2],matrix[ 3],
              matrix[ 4],matrix[ 5],matrix[ 6],matrix[ 7],
              matrix[ 8],matrix[ 9],matrix[10],matrix[11],
              matrix[12],matrix[13],matrix[14],matrix[15],
              inp_state+1,log,sele_str,homogenous);
      PLog(G,line,cPLog_no_flush);
      break;
    default:
      break;
    }
  }
  return(ok);
}
/*========================================================================*/
int ObjectMoleculeGetAtomIndex(ObjectMolecule *I,int sele)
{
  int a,s;
  if(sele<0)
    return(-1);
  for(a=0;a<I->NAtom;a++) {
    s=I->AtomInfo[a].selEntry;
    if(SelectorIsMember(I->Obj.G,s,sele))
      return(a);
  }
  return(-1);
}
/*========================================================================*/
void ObjectMoleculeUpdateNonbonded(ObjectMolecule *I)
{
  register int a;
  register BondType *b;
  register AtomInfoType *ai;
  register int nAtom = I->NAtom;
  register int nBond = I->NBond;


  ai=I->AtomInfo;
  
  for(a=0;a<nAtom;a++)
    (ai++)->bonded = false;
  
  b=I->Bond;
  ai=I->AtomInfo;
  for(a=0;a<nBond;a++)
    {
      ai[b->index[0]].bonded=true;
      ai[b->index[1]].bonded=true;
      b++;
    }
}
/*========================================================================*/
int ObjectMoleculeGetTotalAtomValence(ObjectMolecule *I,int atom)
{
  int result = 0;
  int n0;
  ObjectMoleculeUpdateNeighbors(I);
  if(atom<I->NAtom) {
    n0 = I->Neighbor[atom]+1;
    while(I->Neighbor[n0]>=0) {
      result+=I->Neighbor[n0+1];
    }
    n0+=2;
  } else {
    result = -1; /* error */
  }
  return result;
}
/*========================================================================*/
void ObjectMoleculeUpdateNeighbors(ObjectMolecule *I)
{
  /* neighbor storage structure: VERY COMPLICATED...
     
     0       list offset for atom 0 = n
     1       list offset for atom 1 = n + m + 1
     ...
     n-1     list offset for atom n-1

     n       count for atom 0 
     n+1     neighbor of atom 0
     n+2     bond index
     n+3     neighbor of atom 0
     n+4     bond index
     ...
     n+m     -1 terminator for atom 0

     n+m+1   count for atom 1
     n+m+2   neighbor of atom 1
     n+m+3   bond index
     n+m+4   neighbor of atom 1
     n+m+5   bond index
     etc.

     NOTE: all atoms have an offset and a terminator whether or not they have any bonds 
 */

  int size;
  int a,b,c,d,l0,l1,*l;
  BondType *bnd;
  if(!I->Neighbor) {
    size = (I->NAtom*3)+(I->NBond*4); 
    if(I->Neighbor) {
      VLACheck(I->Neighbor,int,size);
    } else {
      I->Neighbor=VLAlloc(int,size);
    }
    
    /* initialize */
    l = I->Neighbor;
    for(a=0;a<I->NAtom;a++)
      (*l++)=0;
    
    /* count neighbors for each atom */
    bnd = I->Bond;
    for(b=0;b<I->NBond;b++) {
      I->Neighbor[bnd->index[0]]++;
      I->Neighbor[bnd->index[1]]++;
      bnd++;
    }
    
    /* set up offsets and list terminators */
    c = I-> NAtom;
    for(a=0;a<I->NAtom;a++) {
      d = I->Neighbor[a]; /* get number of neighbors */
      I->Neighbor[c]=d; /* store neighbor count */
      I->Neighbor[a]=c+d+d+1; /* set initial position to end of list, we'll fill backwards */
      I->Neighbor[I->Neighbor[a]]=-1; /* store terminator */
      c += d + d + 2;
    }
    
    /* now load neighbors in a sequential list for each atom (reverse order) */
    bnd = I->Bond;
    for(b=0;b<I->NBond;b++) {
      l0 = bnd->index[0];
      l1 = bnd->index[1];
      bnd++;

      I->Neighbor[l0]--; 
      I->Neighbor[I->Neighbor[l0]]=b; /* store bond indices (for I->Bond) */
      I->Neighbor[l0]--;
      I->Neighbor[I->Neighbor[l0]]=l1; /* store neighbor references (I->AtomInfo, etc.)*/

      I->Neighbor[l1]--;
      I->Neighbor[I->Neighbor[l1]]=b; /* store bond indices (for I->Bond) */
      I->Neighbor[l1]--;
      I->Neighbor[I->Neighbor[l1]]=l0; /* store neighbor references (I->AtomInfo, etc.)*/
    }
    for(a=0;a<I->NAtom;a++) { /* adjust down to point to the count, not the first entry */
      if(I->Neighbor[a]>=0)
        I->Neighbor[a]--;
    }
    l=I->Neighbor;
  }
}
/*========================================================================*/
#ifndef _PYMOL_NOPY
static CoordSet *ObjectMoleculeChemPyModel2CoordSet(PyMOLGlobals *G,
                                                    PyObject *model,
                                                    AtomInfoType **atInfoPtr)
{
  int nAtom,nBond;
  int a,c;
  float *coord = NULL;
  CoordSet *cset = NULL;
  AtomInfoType *atInfo = NULL,*ai;
  float *f;
  BondType *ii,*bond=NULL;
  int ok=true;
  int auto_show_lines;
  int auto_show_spheres = (int)SettingGet(G,cSetting_auto_show_spheres);
  int auto_show_nonbonded;
  int hetatm;
  int ignore_ids;
  PyObject *atomList = NULL;
  PyObject *bondList = NULL;
  PyObject *atom = NULL;
  PyObject *bnd = NULL;
  PyObject *index = NULL;
  PyObject *crd = NULL;
  PyObject *tmp = NULL;
  auto_show_lines = (int)SettingGet(G,cSetting_auto_show_lines);
  auto_show_nonbonded = (int)SettingGet(G,cSetting_auto_show_nonbonded);

  ignore_ids=!(int)SettingGet(G,cSetting_preserve_chempy_ids);

  nAtom=0;
  nBond=0;
  if(atInfoPtr)
	 atInfo = *atInfoPtr;

  atomList = PyObject_GetAttrString(model,"atom");
  if(atomList) 
    nAtom = PyList_Size(atomList);
  else 
    ok=ErrMessage(G,"ObjectMoleculeChemPyModel2CoordSet","can't get atom list");


  if(ok) {
    coord=VLAlloc(float,3*nAtom);
    if(atInfo)
      VLACheck(atInfo,AtomInfoType,nAtom);	 
  }

  if(ok) { 
    
    f=coord;
    for(a=0;a<nAtom;a++) {

      atom = PyList_GetItem(atomList,a);
      if(!atom) 
        ok=ErrMessage(G,"ObjectMoleculeChemPyModel2CoordSet","can't get atom");
      crd = PyObject_GetAttrString(atom,"coord");
      if(!crd) 
        ok=ErrMessage(G,"ObjectMoleculeChemPyModel2CoordSet","can't get coordinates");
      else {
        for(c=0;c<3;c++) {
          tmp = PyList_GetItem(crd,c);
          if (tmp) 
            ok = PConvPyObjectToFloat(tmp,f++);
          if(!ok) {
            ErrMessage(G,"ObjectMoleculeChemPyModel2CoordSet","can't read coordinates");
            break;
          }
        }
      }
      Py_XDECREF(crd);
        
      ai = atInfo+a;
      ai->id = a; /* chempy models are zero-based */
      if(!ignore_ids) { 
        if(ok) { /* get chempy atom id if extant */
          if(PTruthCallStr(atom,"has","id")) { 
            tmp = PyObject_GetAttrString(atom,"id");
            if (tmp)
              ok = PConvPyObjectToInt(tmp,&ai->id);
            if(!ok) 
              ErrMessage(G,"ObjectMoleculeChemPyModel2CoordSet","can't read atom identifier");
            Py_XDECREF(tmp);
          } else {
            ai->id=-1;
          }
        }
      }
      ai->rank = a; /* ranks are always zero-based */

      if(ok) {
        tmp = PyObject_GetAttrString(atom,"name");
        if (tmp)
          ok = PConvPyObjectToStrMaxClean(tmp,ai->name,sizeof(AtomName)-1);
        if(!ok) 
          ErrMessage(G,"ObjectMoleculeChemPyModel2CoordSet","can't read name");
        Py_XDECREF(tmp);
      }

      if(ok) {
        ai->textType = 0;
        if(PTruthCallStr(atom,"has","text_type")) { 
          tmp = PyObject_GetAttrString(atom,"text_type");
          if (tmp) {
            OrthoLineType temp;
            OVreturn_word ret;
            ok = PConvPyObjectToStrMaxClean(tmp,temp,sizeof(OrthoLineType)-1);              
            ret = OVLexicon_GetFromCString(G->Lexicon,temp);
            if(OVreturn_IS_OK(ret)) {
              ai->textType = ret.word;
            }
          }
          if(!ok) 
            ErrMessage(G,"ObjectMoleculeChemPyModel2CoordSet","can't read text_type");
          Py_XDECREF(tmp);
        }
      }

      if(ok) {
        if(PTruthCallStr(atom,"has","vdw")) { 
          tmp = PyObject_GetAttrString(atom,"vdw");
          if (tmp)
            ok = PConvPyObjectToFloat(tmp,&ai->vdw);
          if(!ok) 
            ErrMessage(G,"ObjectMoleculeChemPyModel2CoordSet","can't read vdw radius");
          Py_XDECREF(tmp);
        } else {
          ai->vdw=0.0f;
        }
      }
      if(ok) {
        if(PTruthCallStr(atom,"has","bohr")) { 
          tmp = PyObject_GetAttrString(atom,"bohr");
          if (tmp)
            ok = PConvPyObjectToFloat(tmp,&ai->elec_radius);
          if(!ok) 
            ErrMessage(G,"ObjectMoleculeChemPyModel2CoordSet","can't read elec. radius");
          Py_XDECREF(tmp);
        } else {
          ai->elec_radius=0.0F;
        }
      }

      if(ok) {
        if(PTruthCallStr(atom,"has","stereo")) { 
          tmp = PyObject_GetAttrString(atom,"stereo");
          if (tmp)
            ok = PConvPyObjectToChar(tmp,(char*)&ai->stereo);
          if(!ok) 
            ErrMessage(G,"ObjectMoleculeChemPyModel2CoordSet","can't read stereo");
          Py_XDECREF(tmp);
        } else {
          ai->stereo = 0;
        }
      }

      if(ok) {
        if(PTruthCallStr(atom,"has","numeric_type")) { 
          tmp = PyObject_GetAttrString(atom,"numeric_type");
          if (tmp)
            ok = PConvPyObjectToInt(tmp,&ai->customType);
          if(!ok) 
            ErrMessage(G,"ObjectMoleculeChemPyModel2CoordSet","can't read numeric_type");
          Py_XDECREF(tmp);
        } else {
          ai->customType = cAtomInfoNoType;
        }
      }

      if(ok) {
        if(PTruthCallStr(atom,"has","formal_charge")) { 
          tmp = PyObject_GetAttrString(atom,"formal_charge");
          if (tmp)
            ok = PConvPyObjectToInt(tmp,&ai->formalCharge);
          if(!ok) 
            ErrMessage(G,"ObjectMoleculeChemPyModel2CoordSet","can't read formal_charge");
          Py_XDECREF(tmp);
        } else {
          ai->formalCharge = 0;
        }
      }

      if(ok) {
        if(PTruthCallStr(atom,"has","partial_charge")) { 
          tmp = PyObject_GetAttrString(atom,"partial_charge");
          if (tmp)
            ok = PConvPyObjectToFloat(tmp,&ai->partialCharge);
          if(!ok) 
            ErrMessage(G,"ObjectMoleculeChemPyModel2CoordSet","can't read partial_charge");
          Py_XDECREF(tmp);
        } else {
          ai->partialCharge = 0.0;
        }
      }

      if(ok) {
        if(PTruthCallStr(atom,"has","flags")) {         
          tmp = PyObject_GetAttrString(atom,"flags");
          if (tmp)
            ok = PConvPyObjectToInt(tmp,(int*)&ai->flags);
          if(!ok) 
            ErrMessage(G,"ObjectMoleculeChemPyModel2CoordSet","can't read flags");
          Py_XDECREF(tmp);
        } else {
          ai->flags = 0;
        }
      }

      if(ok) {
        tmp = PyObject_GetAttrString(atom,"resn");
        if (tmp)
          ok = PConvPyObjectToStrMaxClean(tmp,ai->resn,sizeof(ResName)-1);
        if(!ok) 
          ErrMessage(G,"ObjectMoleculeChemPyModel2CoordSet","can't read resn");
        Py_XDECREF(tmp);
      }
        
      if(ok) {
        tmp = PyObject_GetAttrString(atom,"resi");
        if (tmp)
          ok = PConvPyObjectToStrMaxClean(tmp,ai->resi,sizeof(ResIdent)-1);
        if(!ok) 
          ErrMessage(G,"ObjectMoleculeChemPyModel2CoordSet","can't read resi");
        else
          ai->resv=AtomResvFromResi(ai->resi);
        Py_XDECREF(tmp);
      }

      if(ok) {
        if(PTruthCallStr(atom,"has","resi_number")) {         
          tmp = PyObject_GetAttrString(atom,"resi_number");
          if (tmp)
            ok = PConvPyObjectToInt(tmp,&ai->resv);
          if(!ok) 
            ErrMessage(G,"ObjectMoleculeChemPyModel2CoordSet","can't read resi_number");
          Py_XDECREF(tmp);
        }
      }
        
      if(ok) {
        tmp = PyObject_GetAttrString(atom,"segi");
        if (tmp)
          ok = PConvPyObjectToStrMaxClean(tmp,ai->segi,sizeof(SegIdent)-1);
        if(!ok) 
          ErrMessage(G,"ObjectMoleculeChemPyModel2CoordSet","can't read segi");
        Py_XDECREF(tmp);
      }

      if(ok) {
        tmp = PyObject_GetAttrString(atom,"b");
        if (tmp)
          ok = PConvPyObjectToFloat(tmp,&ai->b);
        if(!ok) 
          ErrMessage(G,"ObjectMoleculeChemPyModel2CoordSet","can't read b value");
        Py_XDECREF(tmp);
      }

      if(ok) {
        tmp = PyObject_GetAttrString(atom,"q");
        if (tmp)
          ok = PConvPyObjectToFloat(tmp,&ai->q);
        if(!ok) 
          ErrMessage(G,"ObjectMoleculeChemPyModel2CoordSet","can't read occupancy");
        Py_XDECREF(tmp);
      }

        
      if(ok) {
        tmp = PyObject_GetAttrString(atom,"chain");
        if (tmp)
          ok = PConvPyObjectToStrMaxClean(tmp,ai->chain,sizeof(Chain)-1);
        if(!ok) 
          ErrMessage(G,"ObjectMoleculeChemPyModel2CoordSet","can't read chain");
        Py_XDECREF(tmp);
      }
        
      if(ok) {
        tmp = PyObject_GetAttrString(atom,"hetatm");
        if (tmp)
          ok = PConvPyObjectToInt(tmp,&hetatm);
        if(!ok) 
          ErrMessage(G,"ObjectMoleculeChemPyModel2CoordSet","can't read hetatm");
        else
          ai->hetatm = hetatm;
        Py_XDECREF(tmp);
      }
        
      if(ok) {
        tmp = PyObject_GetAttrString(atom,"alt");
        if (tmp)
          ok = PConvPyObjectToStrMaxClean(tmp,ai->alt,sizeof(Chain)-1);
        if(!ok) 
          ErrMessage(G,"ObjectMoleculeChemPyModel2CoordSet","can't read alternate conformation");
        Py_XDECREF(tmp);
      }

      if(ok) {
        tmp = PyObject_GetAttrString(atom,"symbol");
        if (tmp)
          ok = PConvPyObjectToStrMaxClean(tmp,ai->elem,sizeof(AtomName)-1);
        if(!ok) 
          ErrMessage(G,"ObjectMoleculeChemPyModel2CoordSet","can't read symbol");
        Py_XDECREF(tmp);
      }

      if(ok) {
        tmp = PyObject_GetAttrString(atom,"ss");
        if (tmp)
          ok = PConvPyObjectToStrMaxClean(tmp,ai->ssType,sizeof(SSType)-1);
        if(!ok) 
          ErrMessage(G,"ObjectMoleculeChemPyModel2CoordSet","can't read secondary structure");
        Py_XDECREF(tmp);
      }

      if(ok&&PyObject_HasAttrString(atom,"label")) {
        tmp = PyObject_GetAttrString(atom,"label");
        if(tmp) {
          if(ai->label) {
            OVLexicon_DecRef(G->Lexicon,ai->label);
          }
          if(!PConvPyStrToLexRef(tmp,G->Lexicon,&ai->label))
            ai->label = 0;
        }
        Py_XDECREF(tmp);
      }

      if(ok&&PyObject_HasAttrString(atom,"sphere_scale")) {
        tmp = PyObject_GetAttrString(atom,"sphere_scale");
        if(tmp) {
          float value;
          if(PConvPyFloatToFloat(tmp,&value)) {
            int uid = AtomInfoCheckUniqueID(G,ai);
            ai->has_setting = true;
            SettingUniqueSet_f(G,uid,cSetting_sphere_scale,value);
          }
        }
        Py_XDECREF(tmp);
      }
      if(ok&&PyObject_HasAttrString(atom,"cartoon_color")) {
        tmp = PyObject_GetAttrString(atom,"cartoon_color");
        if(tmp) {
          int color_index;
          ok = PConvPyObjectToInt(tmp,&color_index);
          if(!ok) 
            ErrMessage(G,"ObjectMoleculeChemPyModel2CoordSet","bad cartoon color info");
          else {
            int uid = AtomInfoCheckUniqueID(G,ai);
            ai->has_setting = true;
            SettingUniqueSet_color(G,uid,cSetting_cartoon_color,color_index);
          }
        }
        Py_XDECREF(tmp);
      }
      if(ok&&PyObject_HasAttrString(atom,"cartoon_trgb")) {
        tmp = PyObject_GetAttrString(atom,"cartoon_trgb");
        if(tmp) {
          unsigned int trgb;
          ok = PConvPyObjectToInt(tmp,(signed int*)&trgb);
          if(!ok) 
            ErrMessage(G,"ObjectMoleculeChemPyModel2CoordSet","bad cartoon color info");
          else {
            char color_name[24];
            int uid = AtomInfoCheckUniqueID(G,ai);
            ai->has_setting = true;
            sprintf(color_name,"0x%08x",trgb);
            SettingUniqueSet_color(G,uid,cSetting_cartoon_color,ColorGetIndex(G,color_name));
          }
        }
        Py_XDECREF(tmp);
      }
      if(ok&&PyObject_HasAttrString(atom,"ribbon_color")) {
        tmp = PyObject_GetAttrString(atom,"ribbon_color");
        if(tmp) {
          int color_index;
          ok = PConvPyObjectToInt(tmp,&color_index);
          if(!ok) 
            ErrMessage(G,"ObjectMoleculeChemPyModel2CoordSet","bad ribbon color info");
          else {
            int uid = AtomInfoCheckUniqueID(G,ai);
            ai->has_setting = true;
            SettingUniqueSet_color(G,uid,cSetting_ribbon_color,color_index);
          }
        }
        Py_XDECREF(tmp);
      }

      if(ok&&PyObject_HasAttrString(atom,"ribbon_trgb")) {
        tmp = PyObject_GetAttrString(atom,"ribbon_trgb");
        if(tmp) {
          unsigned int trgb;
          ok = PConvPyObjectToInt(tmp,(signed int*)&trgb);
          if(!ok) 
            ErrMessage(G,"ObjectMoleculeChemPyModel2CoordSet","bad cartoon color info");
          else {
            char color_name[24];
            int uid = AtomInfoCheckUniqueID(G,ai);
            ai->has_setting = true;
            sprintf(color_name,"0x%08x",trgb);
            SettingUniqueSet_color(G,uid,cSetting_ribbon_color,ColorGetIndex(G,color_name));
          }
        }
        Py_XDECREF(tmp);
      }

     
      for(c=0;c<cRepCnt;c++) {
        atInfo[a].visRep[c] = false;
      }
      atInfo[a].visRep[cRepLine] = auto_show_lines; /* show lines by default */
      atInfo[a].visRep[cRepNonbonded] = auto_show_nonbonded;
      atInfo[a].visRep[cRepSphere] = auto_show_spheres;

      if(ok&&PyObject_HasAttrString(atom,"visible")) {
        unsigned int vis;
        tmp = PyObject_GetAttrString(atom,"visible");
        if(tmp) {
          ok = PConvPyObjectToInt(tmp,(signed int*)&vis);
          if(!ok) 
            ErrMessage(G,"ObjectMoleculeChemPyModel2CoordSet","bad visibility info");
          else {
            for(c=0;c<cRepCnt;c++) {
              atInfo[a].visRep[c] = vis&0x1;
              vis = (vis>>1);
            }
          }
        }
        Py_XDECREF(tmp);
      }

      if(ok&&atInfo) {
        AtomInfoAssignParameters(G,ai);
        AtomInfoAssignColors(G,ai);
      }

      if(ok&&PyObject_HasAttrString(atom,"trgb")) { /* were we given a Transparency-Red-Blue-Green value? */
        unsigned int trgb;
        tmp = PyObject_GetAttrString(atom,"trgb");
        if(tmp) {
          ok = PConvPyObjectToInt(tmp,(signed int*)&trgb);
          if(!ok) 
            ErrMessage(G,"ObjectMoleculeChemPyModel2CoordSet","bad color info");
          else {
            char color_name[24];
            sprintf(color_name,"0x%08x",trgb);
            atInfo[a].color = ColorGetIndex(G,color_name);
          }
        }
      }


      if(!ok)
        break;
    }
  }

  bondList = PyObject_GetAttrString(model,"bond");
  if(bondList) 
    nBond = PyList_Size(bondList);
  else
    ok=ErrMessage(G,"ObjectMoleculeChemPyModel2CoordSet","can't get bond list");

  if(ok) {
    bond=VLACalloc(BondType,nBond);
    ii=bond;
	 for(a=0;a<nBond;a++)
		{
        bnd = PyList_GetItem(bondList,a);
        if(!bnd) 
          ok=ErrMessage(G,"ObjectMoleculeChemPyModel2CoordSet","can't get bond");
        index = PyObject_GetAttrString(bnd,"index");
        if(!index) 
          ok=ErrMessage(G,"ObjectMoleculeChemPyModel2CoordSet","can't get bond indices");
        else {
          for(c=0;c<2;c++) {
            tmp = PyList_GetItem(index,c);
            if (tmp) 
              ok = PConvPyObjectToInt(tmp,&ii->index[c]);
            if(!ok) {
              ErrMessage(G,"ObjectMoleculeChemPyModel2CoordSet","can't read coordinates");
              break;
            }
          }
        }
        if(ok) {
          tmp = PyObject_GetAttrString(bnd,"order");
          if (tmp)
            ok = PConvPyObjectToInt(tmp,&ii->order);
          if(!ok) 
            ErrMessage(G,"ObjectMoleculeChemPyModel2CoordSet","can't read bond order");
          Py_XDECREF(tmp);
        }

        if(ok) {
          int stereo;
          tmp = PyObject_GetAttrString(bnd,"stereo");
          if (tmp)
            ok = PConvPyObjectToInt(tmp,&stereo);
          else 
            ii->stereo = 0;
          if(!ok) 
            ii->stereo = 0;
          else
            ii->stereo = stereo;
          Py_XDECREF(tmp);
        }

        ii->id=a;
        if(!ignore_ids) { 
          if(ok) { /* get unique chempy bond id if present */
            if(PTruthCallStr(bnd,"has","id")) { 
              tmp = PyObject_GetAttrString(bnd,"id");
              if (tmp)
                ok = PConvPyObjectToInt(tmp,&ii->id);
              if(!ok) 
                ErrMessage(G,"ObjectMoleculeChemPyModel2CoordSet","can't read bond identifier");
              Py_XDECREF(tmp);
            } else {
              ii->id=-1;
            }
          }
        }
        Py_XDECREF(index);
        ii++;
      }
  }

  Py_XDECREF(atomList);
  Py_XDECREF(bondList);

  if(ok) {
	 cset = CoordSetNew(G);
	 cset->NIndex=nAtom;
	 cset->Coord=coord;
	 cset->NTmpBond=nBond;
	 cset->TmpBond=bond;
  } else {
	 VLAFreeP(bond);
	 VLAFreeP(coord);
  }
  if(atInfoPtr)
	 *atInfoPtr = atInfo;

  if(PyErr_Occurred())
    PyErr_Print();
  return(cset);
}
#endif

/*========================================================================*/
ObjectMolecule *ObjectMoleculeLoadChemPyModel(PyMOLGlobals *G,
                                              ObjectMolecule *I,
                                              PyObject *model,
                                              int frame,
                                              int discrete)
{
#ifdef _PYMOL_NOPY
  return NULL;
#else
  CoordSet *cset = NULL;
  AtomInfoType *atInfo;
  int ok=true;
  int isNew = true;
  unsigned int nAtom = 0;
  PyObject *tmp,*mol;

  if(!I) 
	 isNew=true;
  else 
	 isNew=false;

  if(ok) {

	 if(isNew) {
		I=(ObjectMolecule*)ObjectMoleculeNew(G,discrete);
		atInfo = I->AtomInfo;
		isNew = true;
	 } else {
		atInfo=VLAMalloc(10,sizeof(AtomInfoType),2,true); /* autozero here is important */
		isNew = false;
	 }

    if(isNew) {
      I->Obj.Color = AtomInfoUpdateAutoColor(G);
    }

	 cset=ObjectMoleculeChemPyModel2CoordSet(G,model,&atInfo);	 

    mol = PyObject_GetAttrString(model,"molecule");
    if(mol) {
      if(PyObject_HasAttrString(mol,"title")) {
        tmp = PyObject_GetAttrString(mol,"title");
        if(tmp) {
          UtilNCopy(cset->Name,PyString_AsString(tmp),sizeof(WordType));
          Py_DECREF(tmp);
          if(!strcmp(cset->Name,"untitled")) /* ignore untitled */
            cset->Name[0]=0;
        }
      }
      Py_DECREF(mol);
    }
    if(PyObject_HasAttrString(model,"spheroid")&&
       PyObject_HasAttrString(model,"spheroid_normals"))
      {
        tmp = PyObject_GetAttrString(model,"spheroid");
        if(tmp) {
          cset->NSpheroid = PConvPyListToFloatArray(tmp,&cset->Spheroid);
          if(cset->NSpheroid<0) cset->NSpheroid=0;
          Py_DECREF(tmp);
        }
        tmp = PyObject_GetAttrString(model,"spheroid_normals");
        if(tmp) {
          PConvPyListToFloatArray(tmp,&cset->SpheroidNormal);
          Py_DECREF(tmp);
        }
      }
    mol = PyObject_GetAttrString(model,"molecule");
    
	 nAtom=cset->NIndex;
  }
  /* include coordinate set */
  if(ok) {

      if(I->DiscreteFlag&&atInfo) {
        unsigned int a;
        int fp1 = frame+1;
        AtomInfoType *ai = atInfo;
        for(a=0;a<nAtom;a++) {
          (ai++)->discrete_state = fp1;
        }
      }

    cset->Obj = I;
    cset->fEnumIndices(cset);
    if(cset->fInvalidateRep)
      cset->fInvalidateRep(cset,cRepAll,cRepInvRep);
    if(isNew) {	
      I->AtomInfo=atInfo; /* IMPORTANT to reassign: this VLA may have moved! */
    } else {
      ObjectMoleculeMerge(I,atInfo,cset,false,cAIC_AllMask,true); /* NOTE: will release atInfo */
    }
    if(isNew) I->NAtom=nAtom;
    if(frame<0) frame=I->NCSet;
    VLACheck(I->CSet,CoordSet*,frame);
    if(I->NCSet<=frame) I->NCSet=frame+1;
    if(I->CSet[frame]) I->CSet[frame]->fFree(I->CSet[frame]);
    I->CSet[frame] = cset;
    if(isNew) I->NBond = ObjectMoleculeConnect(I,&I->Bond,I->AtomInfo,cset,false);
    if(cset->Symmetry&&(!I->Symmetry)) {
      I->Symmetry=SymmetryCopy(cset->Symmetry);
      SymmetryAttemptGeneration(I->Symmetry,false);
    }
    SceneCountFrames(G);
    ObjectMoleculeExtendIndices(I);
    ObjectMoleculeSort(I);
    ObjectMoleculeUpdateIDNumbers(I);
    ObjectMoleculeUpdateNonbonded(I);
  }
  return(I);
#endif
}


/*========================================================================*/
ObjectMolecule *ObjectMoleculeLoadCoords(PyMOLGlobals *G,ObjectMolecule *I,PyObject *coords,int frame)
{
#ifdef _PYMOL_NOPY
  return NULL;
#else
  CoordSet *cset = NULL;
  int ok=true;
  int a,l;
  PyObject *v;
  float *f;
  a=0;
  while(a<I->NCSet) {
    if(I->CSet[a]) {
      cset=I->CSet[a];
      break;
    }
    a++;
  }
  
  if(!PyList_Check(coords)) 
    ErrMessage(G,"LoadsCoords","passed argument is not a list");
  else {
    l = PyList_Size(coords);
    if (l==cset->NIndex) {
      cset=CoordSetCopy(cset);
      f=cset->Coord;
      for(a=0;a<l;a++) {
        v=PyList_GetItem(coords,a);
/* no error checking */
        *(f++)=(float)PyFloat_AsDouble(PyList_GetItem(v,0)); 
        *(f++)=(float)PyFloat_AsDouble(PyList_GetItem(v,1));
        *(f++)=(float)PyFloat_AsDouble(PyList_GetItem(v,2));
      }
    }
  }
  /* include coordinate set */
  if(ok) {
    if(cset->fInvalidateRep)
      cset->fInvalidateRep(cset,cRepAll,cRepInvRep);

    if(frame<0) frame=I->NCSet;
    VLACheck(I->CSet,CoordSet*,frame);
    if(I->NCSet<=frame) I->NCSet=frame+1;
    if(I->CSet[frame]) I->CSet[frame]->fFree(I->CSet[frame]);
    I->CSet[frame] = cset;
    SceneCountFrames(G);
  }
  return(I);
#endif
}

/*========================================================================*/
void ObjectMoleculeBlindSymMovie(ObjectMolecule *I)
{
  CoordSet *frac;
  int a,c;
  int x,y,z;
  float m[16];

  if(I->NCSet!=1) {
    ErrMessage(I->Obj.G,"ObjectMolecule:","SymMovie only works on objects with a single state.");
  } else if(!I->Symmetry) {
    ErrMessage(I->Obj.G,"ObjectMolecule:","No symmetry loaded!");
  } else if(!I->Symmetry->NSymMat) {
    ErrMessage(I->Obj.G,"ObjectMolecule:","No symmetry matrices!");    
  } else if(I->CSet[0]) {
    frac = CoordSetCopy(I->CSet[0]);
    CoordSetRealToFrac(frac,I->Symmetry->Crystal);
    for(x=-1;x<2;x++)
      for(y=-1;y<2;y++)
        for(z=-1;z<2;z++)
          for(a=0;a<I->Symmetry->NSymMat;a++) {
            if(!((!a)&&(!x)&&(!y)&&(!z))) {
              c = I->NCSet;
              VLACheck(I->CSet,CoordSet*,c);
              I->CSet[c] = CoordSetCopy(frac);
              CoordSetTransform44f(I->CSet[c],I->Symmetry->SymMatVLA+(a*16));
              identity44f(m);
              m[3] = (float)x;
              m[7] = (float)y;
              m[11] = (float)z;
              CoordSetTransform44f(I->CSet[c],m);
              CoordSetFracToReal(I->CSet[c],I->Symmetry->Crystal);
              I->NCSet++;
            }
          }
    frac->fFree(frac);
  }
  SceneChanged(I->Obj.G);
}

/*========================================================================*/
void ObjectMoleculeExtendIndices(ObjectMolecule *I)
{
  int a;
  CoordSet *cs;

  for(a=-1;a<I->NCSet;a++) {
    if(a<0) 
      cs=I->CSTmpl;
    else
      cs=I->CSet[a];
	 if(cs)
      if(cs->fExtendIndices)
        cs->fExtendIndices(cs,I->NAtom);
  }
}
/*========================================================================*/

static CoordSet *ObjectMoleculeMOLStr2CoordSet(PyMOLGlobals *G,char *buffer,
                                               AtomInfoType **atInfoPtr,
                                               char **restart)
{
  char *p;
  int nAtom,nBond;
  int a,c,cnt,atm,chg;
  float *coord = NULL;
  CoordSet *cset = NULL;
  AtomInfoType *atInfo = NULL;
  char cc[MAXLINELEN],cc1[MAXLINELEN],resn[MAXLINELEN] = "UNK";
  float *f;
  BondType *ii;
  BondType *bond=NULL;
  int ok=true;
  int auto_show_lines;
  int auto_show_spheres = (int)SettingGet(G,cSetting_auto_show_spheres);
  int auto_show_nonbonded;
  WordType nameTmp;
  auto_show_lines = (int)SettingGet(G,cSetting_auto_show_lines);
  auto_show_nonbonded = (int)SettingGet(G,cSetting_auto_show_nonbonded);

  p=buffer;
  nAtom=0;
  if(atInfoPtr)
	 atInfo = *atInfoPtr;

  /*  p=ParseWordCopy(nameTmp,p,sizeof(WordType)-1);*/
  p=ParseNCopy(nameTmp,p,sizeof(WordType)-1);
  PRINTFB(G,FB_ObjectMolecule,FB_Blather)
    " ObjMolMOLStr2CoordSet: title '%s'\n",nameTmp
    ENDFB(G)
  p=nextline(p); 
  p=nextline(p);
  p=nextline(p);

  if(ok) {
	 p=ncopy(cc,p,3);
	 if(sscanf(cc,"%d",&nAtom)!=1)
		ok=ErrMessage(G,"ReadMOLFile","bad atom count");
  }

  if(ok) {  
	 p=ncopy(cc,p,3);
	 if(sscanf(cc,"%d",&nBond)!=1)
		ok=ErrMessage(G,"ReadMOLFile","bad bond count");
  }

  if(ok) {
	 coord=VLAlloc(float,3*nAtom);
	 if(atInfo)
		VLACheck(atInfo,AtomInfoType,nAtom);	 
  }
  
  p=nextline(p);

  /* read coordinates and atom names */

  if(ok) { 
	 f=coord;
	 for(a=0;a<nAtom;a++)
		{
		  if(ok) {
			 p=ncopy(cc,p,10);
			 if(sscanf(cc,"%f",f++)!=1)
				ok=ErrMessage(G,"ReadMOLFile","bad coordinate");
		  }
		  if(ok) {
			 p=ncopy(cc,p,10);
			 if(sscanf(cc,"%f",f++)!=1)
				ok=ErrMessage(G,"ReadMOLFile","bad coordinate");
		  }
		  if(ok) {
			 p=ncopy(cc,p,10);
			 if(sscanf(cc,"%f",f++)!=1)
				ok=ErrMessage(G,"ReadMOLFile","bad coordinate");
		  }
		  if(ok) {
          p=nskip(p,1);
			 p=ncopy(atInfo[a].name,p,3);
			 UtilCleanStr(atInfo[a].name);
			 
          for(c=0;c<cRepCnt;c++) {
            atInfo[a].visRep[c] = false;
          }
          atInfo[a].visRep[cRepLine] = auto_show_lines; /* show lines by default */
          atInfo[a].visRep[cRepNonbonded] = auto_show_nonbonded; /* show lines by default */
          atInfo[a].visRep[cRepSphere] = auto_show_spheres; /* show lines by default */

		  }
        if(ok) {
          int tmp_int;
          p=nskip(p,2);
          p=ncopy(cc,p,3);
          if(sscanf(cc,"%d",&atInfo[a].formalCharge)==1) {
            if(atInfo[a].formalCharge) {
              atInfo[a].formalCharge = 4-atInfo[a].formalCharge;
            }
          }
          p=ncopy(cc,p,3);
          if(sscanf(cc,"%d",&tmp_int)!=1) 
            atInfo[a].stereo=0;
          else
            atInfo[a].stereo=tmp_int;
        }
        if(ok&&atInfo) {
          atInfo[a].id = a+1; 
          atInfo[a].rank = a;
          strcpy(atInfo[a].resn,resn);
          atInfo[a].hetatm=true;
          AtomInfoAssignParameters(G,atInfo+a);
          AtomInfoAssignColors(G,atInfo+a);
          atInfo[a].alt[0]=0;
          atInfo[a].segi[0]=0;
          atInfo[a].resi[0]=0;
        }
        p=nextline(p);
        if(!ok)
			 break;
		}
  }
  if(ok) {
	 bond=VLACalloc(BondType,nBond);
	 ii=bond;
	 for(a=0;a<nBond;a++)
		{
		  if(ok) {
			 p=ncopy(cc,p,3);
			 if(sscanf(cc,"%d",&ii->index[0])!=1)
				ok=ErrMessage(G,"ReadMOLFile","bad bond atom");
		  }
		  
		  if(ok) {  
			 p=ncopy(cc,p,3);
			 if(sscanf(cc,"%d",&ii->index[1])!=1)
				ok=ErrMessage(G,"ReadMOLFile","bad bond atom");
		  }

		  if(ok) {  
			 p=ncopy(cc,p,3);
			 if(sscanf(cc,"%d",&ii->order)!=1)
				ok=ErrMessage(G,"ReadMOLFile","bad bond order");
		  }
        if(ok) {
          int stereo;
          p=ncopy(cc,p,3);
          if(sscanf(cc,"%d",&stereo)!=1)
            ii->stereo=0;
          else
            ii->stereo=stereo;
        }
        ii++;
		  if(!ok)
			 break;
		  p=nextline(p);
		}
	 ii=bond;
	 for(a=0;a<nBond;a++) {
      ii->index[0]--;/* adjust bond indexs down one */
      ii->index[1]--;
      ii++;
	 }
  }
  while(*p) { /* read M  CHG records */
    p=ncopy(cc,p,6);
    if(cc[5]==' ') cc[5] = 0;
    if((!strcmp(cc,"M  END"))||(!strcmp(cc,"M END"))) {
      /* denotes end of MOL block */
      p=nextline(p);
      break;
    }
    if((!strcmp(cc,"M  CHG"))||(!strcmp(cc,"M CHG"))) {
      p=ncopy(cc,p,3);
      if(sscanf(cc,"%d",&cnt)==1) {
        while(cnt--) {
          p=ncopy(cc,p,4);
          p=ncopy(cc1,p,4);
          if(!((*cc)||(*cc1))) break;
          if((sscanf(cc,"%d",&atm)==1)&&
             (sscanf(cc1,"%d",&chg)==1)) {
            atm--;
            if((atm>=0)&&(atm<nAtom))
              atInfo[atm].formalCharge = chg;
          }
        }
      }
    }
    p=nextline(p);
  }
  if(ok) {
    (*restart) = p;
	 cset = CoordSetNew(G);
	 cset->NIndex=nAtom;
	 cset->Coord=coord;
	 cset->NTmpBond=nBond;
	 cset->TmpBond=bond;
    strcpy(cset->Name,nameTmp);
  } else {
	 VLAFreeP(bond);
	 VLAFreeP(coord);
    (*restart) = NULL;
  }
  if(atInfoPtr)
	 *atInfoPtr = atInfo;
  return(cset);
}

/*========================================================================*/

static CoordSet *ObjectMoleculeSDF2Str2CoordSet(PyMOLGlobals *G,char *buffer,
                                                AtomInfoType **atInfoPtr,char **next_mol)
{
  char cc[MAXLINELEN];
  char *p;
  CoordSet *result = NULL;
  result = ObjectMoleculeMOLStr2CoordSet(G,buffer,atInfoPtr,next_mol);
  p = *next_mol;
  if(p) {
    while(*p) { /* we simply need to skip until we've read past the end of the SDF record */
      p=ncopy(cc,p,4);
      p=nextline(p);
      if(!strcmp(cc,"$$$$")) { /* SDF record separator... */
        break;
      }
    }
    if(!*p) p = NULL;
  }
  *next_mol = p;
  return result;
}
/*========================================================================*/

static CoordSet *ObjectMoleculeMOL2Str2CoordSet(PyMOLGlobals *G,
                                                char *buffer,
                                                AtomInfoType **atInfoPtr,
                                                char **next_mol)
{
  char *p;
  int nAtom,nBond,nSubst,nFeat,nSet;
  int a,c;
  float *coord = NULL;
  CoordSet *cset = NULL;
  AtomInfoType *atInfo = NULL;
  char cc[MAXLINELEN];
  float *f;
  char *last_p;
  BondType *ii;
  BondType *bond=NULL;
  int ok=true;
  int auto_show_lines;
  int auto_show_spheres = (int)SettingGet(G,cSetting_auto_show_spheres);
  int auto_show_nonbonded;
  int have_molecule = false;
  WordType nameTmp;

  auto_show_lines = (int)SettingGet(G,cSetting_auto_show_lines);
  auto_show_nonbonded = (int)SettingGet(G,cSetting_auto_show_nonbonded);

  p=buffer;
  nAtom=0;
  if(atInfoPtr)
	 atInfo = *atInfoPtr;
  
  while((*p)&&ok) {
    
    last_p = p;
    /* top level -- looking for an RTI, assuming p points to the beginning of a line */
    p=ParseWordCopy(cc,p,MAXLINELEN);
    if(cc[0]=='@') {
      if(WordMatchExact(G,cc,"@<TRIPOS>MOLECULE",true)||WordMatchExact(G,cc,"@MOLECULE",true)) {
        if(have_molecule) {
          *next_mol = last_p;
          break; /* next record of multi-mol2 */
        }
        p=ParseNextLine(p);
        p=ParseNTrim(nameTmp,p,sizeof(WordType)-1); /* get mol name */         
        p=ParseNextLine(p);
        p=ParseWordCopy(cc,p,MAXLINELEN);
        if(sscanf(cc,"%d",&nAtom)!=1)
          ok=ErrMessage(G,"ReadMOL2File","bad atom count");
        else {
          coord=VLAlloc(float,3*nAtom);
          if(atInfo)
            VLACheck(atInfo,AtomInfoType,nAtom);	 

          p=ParseWordCopy(cc,p,MAXLINELEN);
          if(sscanf(cc,"%d",&nBond)!=1) {
            nBond = 0;
          }
          p=ParseWordCopy(cc,p,MAXLINELEN);
          if(sscanf(cc,"%d",&nSubst)!=1) {
            nSubst = 0;
          }
          p=ParseWordCopy(cc,p,MAXLINELEN);
          if(sscanf(cc,"%d",&nFeat)!=1) {
            nFeat = 0;
          }
          p=ParseWordCopy(cc,p,MAXLINELEN);
          if(sscanf(cc,"%d",&nSet)!=1) {
            nSet = 0;
          }

        }
        p=ParseNextLine(p);
        p=ParseNextLine(p);
        have_molecule=true;
      } else if(WordMatchExact(G,cc,"@<TRIPOS>ATOM",true)||WordMatchExact(G,cc,"@ATOM",true)) {
        if(!have_molecule) {
          ok=ErrMessage(G,"ReadMOL2File","@ATOM before @MOLECULE!");
          break;
        }
        p=ParseNextLine(p);
        f=coord;
        for(a=0;a<nAtom;a++) {
          AtomInfoType *ai = atInfo + a;

          if(ok) {
            p=ParseWordCopy(cc,p,MAXLINELEN);

            if(sscanf(cc,"%d",&ai->id)!=1)
              ok=ErrMessage(G,"ReadMOL2File","bad atom id");
          }
          if(ok) {
            p=ParseWordCopy(cc,p,MAXLINELEN);
            cc[cAtomNameLen] = 0; 
            if(sscanf(cc,"%s",ai->name)!=1)
              ok=ErrMessage(G,"ReadMOL2File","bad atom name");
          }
          if(ok) {
            p=ParseWordCopy(cc,p,MAXLINELEN);
            if(sscanf(cc,"%f",f++)!=1)
              ok=ErrMessage(G,"ReadMOL2File","bad x coordinate");
          }
          if(ok) {
            p=ParseWordCopy(cc,p,MAXLINELEN);
            if(sscanf(cc,"%f",f++)!=1)
              ok=ErrMessage(G,"ReadMOL2File","bad y coordinate");
          }
          if(ok) {
            p=ParseWordCopy(cc,p,MAXLINELEN);
            if(sscanf(cc,"%f",f++)!=1)
              ok=ErrMessage(G,"ReadMOL2File","bad z coordinate");
          }
          if(ok) {
            OrthoLineType temp;
            p=ParseWordCopy(cc,p,OrthoLineLength-1);
            if(sscanf(cc,"%s",temp)!=1)
              ok=ErrMessage(G,"ReadMOL2File","bad atom type");
            else { /* convert atom type to elem symbol */
              char *tt = temp;
              char *el = ai->elem;
              int elc = 0;
              while(*tt&&((*tt)!='.')) {
                *(el++) = *(tt++);
                elc++;
                if(elc>cAtomNameLen)
                  break;
              }
              *el=0;
              if(el[2])
                el[0] = 0;

              ai->textType = 0;
              if(temp[0]) {
                OVreturn_word result = OVLexicon_GetFromCString(G->Lexicon,temp);
                if(OVreturn_IS_OK(result)) {
                  ai->textType = result.word;
                }
              }
            }
          }
          if(ok) {
            p=ParseWordCopy(cc,p,MAXLINELEN);
            if(cc[0]) { /* subst_id is residue identifier */
              UtilNCopy(ai->resi,cc,cResiLen);
              ai->resv = AtomResvFromResi(cc);
              p=ParseWordCopy(cc,p,MAXLINELEN); 
              if(cc[0]) { 

                UtilNCopy(ai->resn,cc,cResnLen);

                p=ParseWordCopy(cc,p,MAXLINELEN);        
                if(cc[0]) {
                  if(sscanf(cc,"%f",&ai->partialCharge)!=1)                    
                    ok=ErrMessage(G,"ReadMOL2File","bad atom charge");
                }
              }
            }
          }
          p=ParseNextLine(p);

          for(c=0;c<cRepCnt;c++) {
            ai->visRep[c] = false;
          }
          ai->visRep[cRepLine] = auto_show_lines; /* show lines by default */
          ai->visRep[cRepNonbonded] = auto_show_nonbonded; 
          ai->visRep[cRepSphere] = auto_show_spheres;


          ai->id = a+1;
          ai->rank = a;
          ai->hetatm=true;
          AtomInfoAssignParameters(G,atInfo+a);
          AtomInfoAssignColors(G,atInfo+a);
          ai->alt[0]=0;
          ai->segi[0]=0;

        }
      } else if(WordMatchExact(G,cc,"@<TRIPOS>SET",true)||WordMatchExact(G,cc,"@SET",true)) {
        if(!have_molecule) {
          ok=ErrMessage(G,"ReadMOL2File","@SET before @MOLECULE!");
          break;
        }
        p=ParseNextLine(p);
        if(ok) {
          for(a=0;a<nSet;a++) {
            if(ok) {
              char ss = 0;
              p=ParseWordCopy(cc,p,50);
              cc[5]=0;
              if(!strcmp("HELIX",cc)) 
                ss='H';
              if(!strcmp("SHEET",cc)) 
                ss='S';
              p=ParseWordCopy(cc,p,50);
              if(!strcmp("STATIC",cc)) {
                int nMember;
              
                p=ParseNextLine(p);
                p=ParseWordCopy(cc,p,20);
                if(sscanf(cc,"%d",&nMember)!=1)
                  ok=ErrMessage(G,"ReadMOL2File","bad member count");
                else {
                  while(ok&&(nMember>0)) {
                    p=ParseWordCopy(cc,p,20);
                    if((!cc[0])&&(!*p)) {
                      ok=false;
                      break;
                    }
                    if(cc[0]!='\\') {
                      int atom_id;
                      if(sscanf(cc,"%d",&atom_id)!=1) {
                        ok=ErrMessage(G,"ReadMOL2File","bad member");
                      } else {
                        atom_id--;
                        if((atom_id>=0)&&(atom_id<nAtom)) {
                          atInfo[atom_id].ssType[0] = ss;
                          atInfo[atom_id].ssType[1] = 0;
                        }
                        nMember--;
                      }
                    } else {
                      p=ParseNextLine(p);
                    }
                  }
                }
                p=ParseNextLine(p);
              } else {
                p=ParseNextLine(p);
                p=ParseNextLine(p);                
              }
            }
          }
        }
      } else if(WordMatchExact(G,cc,"@<TRIPOS>BOND",true)||WordMatchExact(G,cc,"@BOND",true)) {
        if(!have_molecule) {
          ok=ErrMessage(G,"ReadMOL2File","@BOND before @MOLECULE!");
          break;
        }
        p=ParseNextLine(p);

        if(ok) {
          bond=VLACalloc(BondType,nBond);
          ii=bond;
          for(a=0;a<nBond;a++)
            {
              if(ok) {
                p=ParseWordCopy(cc,p,20);
                if(sscanf(cc,"%d",&ii->id)!=1)
                  ok=ErrMessage(G,"ReadMOL2File","bad atom id");
              }

              if(ok) {
                p=ParseWordCopy(cc,p,20);
                if(sscanf(cc,"%d",&ii->index[0])!=1)
                  ok=ErrMessage(G,"ReadMOL2File","bad source atom id");
              }

              if(ok) {
                p=ParseWordCopy(cc,p,20);
                if(sscanf(cc,"%d",&ii->index[1])!=1)
                  ok=ErrMessage(G,"ReadMOL2File","bad target atom id");
              }

              if(ok) {
                p=ParseWordCopy(cc,p,20);
                if(!cc[1]) {
                  switch(cc[0]) {
                  case '1':
                    ii->order = 1;
                    break;
                  case '2':
                    ii->order = 2;
                    break;
                  case '3':
                    ii->order = 3;
                    break;
                  }
                } else if(WordMatchExact(G,"ar",cc,true)) {
                  ii->order = 4;
                } else if(WordMatchExact(G,"am",cc,true)) {
                  ii->order = 1;
                } else if(WordMatchExact(G,"un",cc,true)) {
                  ii->order = 1;
                } else if(WordMatchExact(G,"nc",cc,true)) {
                  ii->order = 0; /* is this legal in PyMOL? */
                } else if(WordMatchExact(G,"du",cc,true)) { 
                  ii->order = 1;
                } else 
                  ok=ErrMessage(G,"ReadMOL2File","bad bond type");
              }
              ii->stereo = 0;
              ii++;
              if(!ok) break;
              p=ParseNextLine(p);
            }
          ii=bond;
          for(a=0;a<nBond;a++) {
            ii->index[0]--;/* adjust bond indexs down one */
            ii->index[1]--;
            ii++;
          }
        }
      } else if(WordMatchExact(G,cc,"@<TRIPOS>SUBSTRUCTURE",true)||WordMatchExact(G,cc,"@SUBSTRUCTURE",true)) {
        if(!have_molecule) {
          ok=ErrMessage(G,"ReadMOL2File","@SUBSTSRUCTURE before @MOLECULE!");
          break;
        }
        p=ParseNextLine(p);
        if(ok) {
          WordType subst_name;
          SegIdent segment; /* what MOL2 calls chain */
          Chain chain;
          WordType subst_type;
          ResIdent resi;
          ResName resn;
          int id, dict_type, root, resv;
          int end_line, seg_flag, subst_flag, resi_flag;
          int chain_flag, resn_flag, resv_flag;
          OVLexicon *lex = OVLexicon_New(G->Context->heap);
          OVOneToOne *o2o = OVOneToOne_New(G->Context->heap);

          {
            /* create linked list of atoms for each residue id */
            OVreturn_word result;
            int b;
            AtomInfoType *ai = atInfo;
            for(b=0;b<nAtom;b++) {
              ai->temp1=-1;
              ai++;
            }
            ai=atInfo;
            for(b=0;b<nAtom;b++) {
              if(OVreturn_IS_OK( (result=OVLexicon_BorrowFromCString(lex,ai->resi)) )) {
                if(OVreturn_IS_OK( (result=OVOneToOne_GetForward(o2o,result.word)) )) {
                  atInfo[b].temp1 = atInfo[result.word].temp1;
                  atInfo[result.word].temp1 = b;
                }
              } else {
                if(OVreturn_IS_OK( (result=OVLexicon_GetFromCString(lex,ai->resi)) ))
                  OVOneToOne_Set(o2o, result.word, b);
              }
              ai++;
            }
          }

          for(a=0;a<nSubst;a++) {
            segment[0]=0;
            subst_name[0]=0;
            chain[0]=0;
            resn[0]=0;
            resi[0]=0;
            end_line=false;
            seg_flag=false;
            subst_flag=false;
            
            resi_flag=false;
            chain_flag=false;
            resn_flag=false;
            resv_flag=false;
            
            if(ok) {
              char *save_p = p;
              p=ParseWordCopy(cc,p,20);
              if(sscanf(cc,"%d",&id)!=1) {
                if(cc[0]!='@')
                  ok=ErrMessage(G,"ReadMOL2File","bad substructure id");
                else {
                  p = save_p;
                  break; /* missing substructure... */
                }
              }
            }
            if(ok) {
              p=ParseWordCopy(cc,p,sizeof(WordType)-1);
              if(sscanf(cc,"%s",subst_name)!=1)
                ok=ErrMessage(G,"ReadMOL2File","bad substructure name");
            }
            if(ok) {
              p=ParseWordCopy(cc,p,20);
              if(sscanf(cc,"%d",&root)!=1)
                ok=ErrMessage(G,"ReadMOL2File","bad target root atom id");
              else
                root--; /* convert 1-based to 0-based */
            }

            /* optional data */
            if(ok&&(!end_line)) {
              p=ParseWordCopy(cc,p,sizeof(WordType)-1);
              if(sscanf(cc,"%s",subst_type)!=1) {
                end_line=true;
                subst_type[0]=0;
              } else {
                subst_flag=1;
              }
            }
            
            if(ok&&(!end_line)) {
              p=ParseWordCopy(cc,p,20);
              if(sscanf(cc,"%d",&dict_type)!=1)
                end_line=true;
            }
            
            if(ok&&(!end_line)) {
              p=ParseWordCopy(cc,p,sizeof(WordType)-1);
              cc[cSegiLen]=0;
              if(sscanf(cc,"%s",segment)!=1) {
                end_line=true;
                segment[0]=0;
              } else {
                seg_flag=true; 
                if(!segment[1]) { /* if segment is single letter, then also assign to chain field */
                  chain[0]=segment[0];
                  chain[1]=0;
                  chain_flag=true;
                }
              }
            }
            
            if(ok&&(!end_line)) {
              p=ParseWordCopy(cc,p,sizeof(WordType)-1);
              cc[cResnLen]=0;
              if(!strcmp(cc,"****")) { /* whoops -- no residue name... */
                char *pp;
                UtilNCopy(resn,subst_name,sizeof(ResName));
                pp = resn;
                /* any numbers? */
                while(*pp) {
                  if(((*pp)>='0')&&((*pp)<='9'))
                    break;
                  pp++;
                }
                /* trim them off */
                while(pp>=resn) {
                  if(((*pp)>='0')&&((*pp)<='9'))
                    *pp = 0;
                  else 
                    break;
                  pp--;
                }
                
                if(resn[0])
                  resn_flag = true;
              } else {
                if(sscanf(cc,"%s",resn)!=1) {
                  end_line=true;
                  resn[0]=0;
                } else {
                  resn_flag=true;
                }
              }
            }
            
            if(ok&&(root>=0)&&(root<nAtom)) {
              
              if((strlen(subst_name)==4) &&
                 ((subst_name[0]>='1') && (subst_name[0]<='9')) &&
                 ((((subst_name[1]>='A') && (subst_name[1]<='Z')) || 
                   ((subst_name[1]>='a') && (subst_name[1]<='z'))) ||
                  (((subst_name[2]>='A') && (subst_name[2]<='Z')) || 
                   ((subst_name[2]>='a') && (subst_name[2]<='z'))) ||
                  (((subst_name[3]>='A') && (subst_name[3]<='Z')) || 
                   ((subst_name[3]>='a') && (subst_name[3]<='z'))))) {
                
                /* if subst_name looks like a PDB code, then it isn't a residue identifier
                   so do nothing... */
              } else {
              
                if(!subst_flag) {
                  
                  /* Merck stuffs the chain ID and the residue ID in the
                     subst_name field, so handle that case first */
                  
                  /* get the residue value */
                  
                  ParseIntCopy(cc,subst_name,20);
                  if(sscanf(cc,"%d",&resv)==1) {
                    /* we have the resv, so now get the chain if there is one */
                    char *pp = subst_name;
                    if(!((pp[0]>='0')&&(pp[0]<='9'))) {
                      chain[0]=pp[0];
                      chain_flag=true;
                      pp++;
                    }
                    UtilNCopy(resi,pp,cResiLen); /* now get the textual residue identifier */
                    
                    if(resi[0]) { 
                      resi_flag=true;
                      resv_flag=true;
                    }
                  }
                } else { /* normal mode: assume that the substructure ID is a vanilla residue identifier */
                  
                  char *pp = subst_name;
                  
                  if(resn_flag) { /* if we have a resn, then remove it from the subst_name */
                    char *qq = resn;
                    while((*pp)&&(*qq)) {
                      if(*pp == *qq) {
                        pp++;
                        qq++;
                      } else
                        break;
                    }
                  }
                  
                  ParseIntCopy(cc,pp,20);
                  if(sscanf(cc,"%d",&resv)==1) {
                    UtilNCopy(resi,pp,cResiLen); /* now get the textual residue identifier */
                    if(resi[0]) { 
                      resi_flag=true;
                      resv_flag=true;
                    }
                  }
                }
              }
              
              /* for now, assume that atom ids are 1-based and sequential */
              
              if(ok) {
                if(resi_flag||chain_flag||resn_flag||seg_flag) {
#if 1
                  OVreturn_word result;
                  if(OVreturn_IS_OK( (result=OVLexicon_BorrowFromCString(lex,atInfo[root].resi)) )) {
                    if(OVreturn_IS_OK( (result=OVOneToOne_GetForward(o2o,result.word)) )) {
                      /* traverse linked list for resi */
                      int b = result.word;
                      while((b>=0)&&(b<nAtom)) {
                        AtomInfoType *ai = atInfo + b;
                        if(resi_flag)
                          UtilNCopy(ai->resi,resi,cResiLen);                        
                        if(chain_flag) {
                          ai->chain[0]=chain[0];
                        }
                        if(resn_flag)
                          UtilNCopy(ai->resn,resn,cResnLen);                                                
                        if(seg_flag)
                          UtilNCopy(ai->segi,segment,cSegiLen);
                        b = ai->temp1;
                      }
                    }
                  }
#else
                  int b,delta = -1;
                  ResIdent start_resi;
                  strcpy(start_resi,atInfo[root].resi);
                  b=root;
                  while(b<nAtom) {
                    AtomInfoType *ai = atInfo + b;
                    if(strcmp(start_resi, ai->resi)!=0) {
                      /* only act on atoms in this residue, assuming that
                         residues are grouped together */
                      if(delta>0)
                        break;
                      else {
                        delta = 1;
                        b = root;
                      }
                    } else {
                      if(resi_flag)
                        UtilNCopy(ai->resi,resi,cResiLen);                        
                      if(chain_flag) {
                        ai->chain[0]=chain[0];
                      }
                      if(resn_flag)
                        UtilNCopy(ai->resn,resn,cResnLen);                                                
                      if(seg_flag)
                        UtilNCopy(ai->segi,segment,cSegiLen);
                    }
                    b+=delta;
                    if(b<0) {
                      delta = 1;
                      b = root+1;
                    }
                  }
#endif
                }
              }
            }
            if(!ok)
              break;
            p=ParseNextLine(p);
            }
          OVLexicon_DEL_AUTO_NULL(lex);
          OVOneToOne_DEL_AUTO_NULL(o2o);
        }
      } else       
        p=ParseNextLine(p);
    } else
      p=ParseNextLine(p);
  }
  if(ok) {
	 cset = CoordSetNew(G);
	 cset->NIndex=nAtom;
	 cset->Coord=coord;
	 cset->NTmpBond=nBond;
	 cset->TmpBond=bond;
    strcpy(cset->Name,nameTmp);
  } else {
	 VLAFreeP(bond);
	 VLAFreeP(coord);
  }
  if(atInfoPtr)
	 *atInfoPtr = atInfo;
  return(cset);
}

ObjectMolecule *ObjectMoleculeReadStr(PyMOLGlobals *G,ObjectMolecule *I,
                                      char *st,int content_format, int frame,int discrete,
                                      int quiet,int multiplex, char *new_name,
                                      char **next_entry)
{
  int ok = true;
  CoordSet *cset=NULL;
  AtomInfoType *atInfo;
  int isNew;
  int nAtom;
  char *restart=NULL,*start;
  int repeatFlag=true;
  int successCnt = 0;
  char tmpName[WordLength];
  int deferred_tasks = false;
  int skip_out;
  *next_entry = NULL;

  start=st;
  while(repeatFlag) {
    repeatFlag=false;
    skip_out = false;

    if(!I) 
      isNew=true;
    else 
      isNew=false;

    if(isNew) {
      I=(ObjectMolecule*)ObjectMoleculeNew(G,(discrete>0));
      atInfo = I->AtomInfo;
      isNew = true;
    } else {
      atInfo=VLAMalloc(10,sizeof(AtomInfoType),2,true); /* autozero here is important */
      isNew = false;
    }

    if(isNew) {
      I->Obj.Color = AtomInfoUpdateAutoColor(G);
    }

    restart=NULL;
    switch(content_format) {
    case cLoadTypeMOL2:
    case cLoadTypeMOL2Str:
      cset=ObjectMoleculeMOL2Str2CoordSet(G,start,&atInfo,&restart);
      break;
    case cLoadTypeMOL:
    case cLoadTypeMOLStr:
      cset=ObjectMoleculeMOLStr2CoordSet(G,start,&atInfo,&restart);
      restart = NULL;
      break;
    case cLoadTypeSDF2:
    case cLoadTypeSDF2Str:
      cset=ObjectMoleculeSDF2Str2CoordSet(G,start,&atInfo,&restart);
      break;
    }
  
    if(!cset) {
      if(!successCnt) {
        ObjectMoleculeFree(I);
        I=NULL;
        ok=false;
      } else {
        skip_out = true;
      }
    }
    
    if(ok && ! skip_out) {
      
      if((discrete<0) && (restart) && isNew && (!multiplex)) { 
        /* if default discrete behavior and new object, with
           multi-coordinate set file, and not multiplex, then set
           discrete */
                                         
        ObjectMoleculeSetDiscrete(G,I,true);
      }

      if(frame<0)
        frame=I->NCSet;
      if(I->NCSet<=frame)
        I->NCSet=frame+1;
      VLACheck(I->CSet,CoordSet*,frame);
      
      nAtom=cset->NIndex;
      
      if(I->DiscreteFlag&&atInfo) {
        int a;
          int fp1 = frame+1;
          AtomInfoType *ai = atInfo;
          for(a=0;a<nAtom;a++) {
            (ai++)->discrete_state = fp1;
          }
      }
      
      if(multiplex>0) 
        UtilNCopy(tmpName,cset->Name,WordLength);
      
      cset->Obj = I;
      cset->fEnumIndices(cset);
      if(cset->fInvalidateRep)
        cset->fInvalidateRep(cset,cRepAll,cRepInvRep);
      if(isNew) {		
        I->AtomInfo=atInfo; /* IMPORTANT to reassign: this VLA may have moved! */
      } else {
        ObjectMoleculeMerge(I,atInfo,cset,false,cAIC_MOLMask,false); /* NOTE: will release atInfo */
      }
      
      if(isNew) I->NAtom=nAtom;
      if(frame<0) frame=I->NCSet;
      VLACheck(I->CSet,CoordSet*,frame);
      if(I->NCSet<=frame) I->NCSet=frame+1;
      if(I->CSet[frame]) I->CSet[frame]->fFree(I->CSet[frame]);
      I->CSet[frame] = cset;
      
      if(isNew) I->NBond = ObjectMoleculeConnect(I,&I->Bond,I->AtomInfo,cset,false);
      
      ObjectMoleculeExtendIndices(I);
      ObjectMoleculeSort(I);
      
      deferred_tasks = true;
      successCnt++;
      if(!quiet) {
        if(successCnt>1) {
          if(successCnt==2) {
            PRINTFB(G,FB_ObjectMolecule,FB_Actions)
              " ObjectMoleculeReadStr: read through molecule %d.\n",1
              ENDFB(G);
          }
          if(UtilShouldWePrintQuantity(successCnt)) {
            PRINTFB(G,FB_ObjectMolecule,FB_Actions)
              " ObjectMoleculeReadStr: read through molecule %d.\n",successCnt
              ENDFB(G);
          }
        }
      }
      
      if(multiplex>0) {
        UtilNCopy(new_name,tmpName,WordLength);
        if(restart) {
          *next_entry = restart;
        }
      } else if(restart) {
        repeatFlag=true;
        start=restart;
        frame=frame+1;
      }
    }
  }
  if(deferred_tasks&&I) { /* defer time-consuming tasks until all states have been loaded */
    SceneCountFrames(G);
    ObjectMoleculeInvalidate(I,cRepAll,cRepInvAll,-1);
    ObjectMoleculeUpdateIDNumbers(I);
    ObjectMoleculeUpdateNonbonded(I);
  }
  return(I);
}
 

/*========================================================================*/
typedef int CompareFn(PyMOLGlobals *,AtomInfoType *,AtomInfoType *);
void ObjectMoleculeMerge(ObjectMolecule *I,AtomInfoType *ai,
                         CoordSet *cs,int bondSearchFlag,int aic_mask,
                         int invalidate)
{
  PyMOLGlobals *G = I->Obj.G;
  int *index,*outdex,*a2i,*i2a;
  BondType *bond=NULL;
  register int a,b,lb=0,ac;
  int c,nb,a1,a2;
  register int found;
  int nAt,nBd,nBond;
  int expansionFlag = false;
  AtomInfoType *ai2;
  int oldNAtom,oldNBond;

  oldNAtom = I->NAtom;
  oldNBond = I->NBond;

  /* first, sort the coodinate set */
  
  index=AtomInfoGetSortedIndex(G,ai,cs->NIndex,&outdex);
  for(b=0;b<cs->NIndex;b++)
	 cs->IdxToAtm[b]=outdex[cs->IdxToAtm[b]];
  for(b=0;b<cs->NIndex;b++)
	 cs->AtmToIdx[b]=-1;
  for(b=0;b<cs->NIndex;b++)
	 cs->AtmToIdx[cs->IdxToAtm[b]]=b;
  ai2=(AtomInfoType*)VLAMalloc(cs->NIndex,sizeof(AtomInfoType),5,true); /* autozero here is important */
  for(a=0;a<cs->NIndex;a++) 
	 ai2[a]=ai[index[a]]; /* creates a sorted list of atom info records */
  VLAFreeP(ai);
  ai=ai2;

  /* now, match it up with the current object's atomic information */
	 
  for(a=0;a<cs->NIndex;a++) {
	 index[a]=-1;
	 outdex[a]=-1;
  }

  {
    register int n_index = cs->NIndex;
    register int n_atom = I->NAtom;
    register AtomInfoType *atInfo = I->AtomInfo,*ai_a;
    CompareFn *fCompare;

    if(SettingGetGlobal_b(G,cSetting_pdb_hetatm_sort)) {
      fCompare = AtomInfoCompareIgnoreRank;
    } else {
      fCompare = AtomInfoCompareIgnoreRankHet;
    }

    c=0;
    b=0;  
    lb = 0;

    for(a=0;a<n_index;a++) {
      register int reverse = false;
      ai_a = ai + a;
      found=false;
      if(!I->DiscreteFlag) { /* don't even try matching for discrete objects */
        while(b<n_atom) {
          ac=(fCompare(G,ai_a,atInfo+b));
          if(!ac) {
            found=true;
            break;
          } else if(ac<0) { /* atom is before current, so try going the other way */
            reverse = true;
            break;
          } else if(!b) {
            int idx;
            ac=(fCompare(G,ai_a,atInfo+n_atom-1));
            if(ac>0) { /* atom is after all atoms in list, so don't even bother searching */
              break;
            }
            /* next, try to jump to an appropriate position in the list */
            idx = ((a*n_atom)/n_index)-1;
            if((idx>0)&&(b!=idx)&&(idx<n_atom)) {
              ac=(fCompare(G,ai_a,atInfo+idx));
              if(ac>0) 
                b = idx;
            }
          }
          lb = b; /* last b before atom */
          b++;
        }
        if(reverse&&!found) { /* searching going backwards... */
          while(b>=0) {
            ac=(fCompare(G,ai_a,atInfo+b));
            if(!ac) {
              found=true;
              break;
            } else if(ac>0) { /* atom after current -- no good */
              break;
            }
            lb = b;
            b--;
          }
          if(b<0)
            b=0;
        }
      }
      if(found) {
        index[a] = b; /* store real atom index b for a in index[a] */
        b++;
      } else {
        index[a]=I->NAtom+c; /* otherwise, this is a new atom */
        c++;
        b=lb;
      }
    }
  }

  /* first, reassign atom info for matched atoms */

  /* allocate additional space */
  if(c) {
    expansionFlag=true;
    nAt=I->NAtom+c;
  } else {
    nAt=I->NAtom;
  }
  
  if(expansionFlag) {
	VLACheck(I->AtomInfo,AtomInfoType,nAt);
  }

  /* allocate our new x-ref tables */
  if(nAt<I->NAtom) nAt=I->NAtom;
  a2i = Alloc(int,nAt);
  i2a = Alloc(int,cs->NIndex);
  if(nAt) {
    ErrChkPtr(G,a2i);
  }
  if(cs->NIndex){
    ErrChkPtr(G,i2a);
  }
  
  for(a=0;a<cs->NIndex;a++) {/* a is in original file space */
    a1=cs->IdxToAtm[a]; /* a1 is in sorted atom info space */
    a2=index[a1];
    i2a[a]=a2; /* a2 is in object space */
    if(a2<oldNAtom)
      AtomInfoCombine(G,I->AtomInfo+a2,ai+a1,aic_mask);
    else
      *(I->AtomInfo+a2)=*(ai+a1);
  }
  
  if(I->DiscreteFlag) {
    if(I->NDiscrete<nAt) {
      VLACheck(I->DiscreteAtmToIdx,int,nAt);
      VLACheck(I->DiscreteCSet,CoordSet*,nAt);    
      for(a=I->NDiscrete;a<nAt;a++) {
        I->DiscreteAtmToIdx[a]=-1;
        I->DiscreteCSet[a]=NULL;
      }
    }
    I->NDiscrete = nAt;
  }
  
  cs->NAtIndex = nAt;
  I->NAtom = nAt;
  
  FreeP(cs->AtmToIdx);
  FreeP(cs->IdxToAtm);
  cs->AtmToIdx = a2i;
  cs->IdxToAtm = i2a;

  if(I->DiscreteFlag) {
    FreeP(cs->AtmToIdx);
    for(a=0;a<cs->NIndex;a++) {
      I->DiscreteAtmToIdx[cs->IdxToAtm[a]]=a;
      I->DiscreteCSet[cs->IdxToAtm[a]] = cs;
    }
  } else {
    for(a=0;a<cs->NAtIndex;a++)
      cs->AtmToIdx[a]=-1;
    for(a=0;a<cs->NIndex;a++)
      cs->AtmToIdx[cs->IdxToAtm[a]]=a;
  }
  
  VLAFreeP(ai); /* note that we're trusting AtomInfoCombine to have 
                   already purged all atom-dependent storage (such as strings) */

  AtomInfoFreeSortedIndexes(G,index,outdex);
  
  /* now find and integrate and any new bonds */
  if(expansionFlag) { /* expansion flag means we have introduced at least 1 new atom */
    nBond = ObjectMoleculeConnect(I,&bond,I->AtomInfo,cs,bondSearchFlag);
    if(nBond) {
      index=Alloc(int,nBond);
      
      c=0;
      b=0;  
      nb=0;
      for(a=0;a<nBond;a++) { /* iterate over new bonds */
        found=false;
        if(!I->DiscreteFlag) { /* don't even try matching for discrete objects */
          b=nb; /* pick up where we left off */
          while(b<I->NBond) { 
            ac=BondCompare(bond+a,I->Bond+b);
            if(!ac) { /* zero is a match */
              found=true;
              break;
            } else if(ac<0) { /* gone past position of this bond */
              break;
            } else if(!b) {
              int idx;
              ac=BondCompare(bond+a,I->Bond+I->NBond-1);
              
              if(ac>0) /* bond is after all bonds in list, so don't even bother searching */
                break;
              /* next, try to jump to an appropriate position in the list */
              idx = ((a*I->NBond)/nBond)-1;
              if((idx>0)&&(b!=idx)&&(idx<I->NBond)) {
                ac=BondCompare(bond+a,I->Bond+idx);
                if(ac>0) 
                  b = idx;
              }
            }
            
            b++; /* no match yet, keep looking */
          }
        }
        if(found) {
          index[a]=b; /* existing bond...*/
          nb=b+1;
        } else { /* this is a new bond, save index and increment */
          index[a]=I->NBond+c;
          c++; 
        }
      }
      /* first, reassign atom info for matched atoms */
      if(c) {
        /* allocate additional space */
        nBd=I->NBond+c;

        if(!I->Bond) 
          I->Bond=VLACalloc(BondType,1);          

        VLACheck(I->Bond,BondType,nBd);
        
        for(a=0;a<nBond;a++) { /* copy the new bonds */
          a2=index[a];
          if(a2 >= I->NBond) { 
            I->Bond[a2] = bond[a];
          }
        }
        I->NBond=nBd;
      }
      FreeP(index);
    }
    VLAFreeP(bond);
  }

  if(invalidate) {
    if(oldNAtom) {
      if(oldNAtom==I->NAtom) {
        if(oldNBond!=I->NBond) {
          ObjectMoleculeInvalidate(I,cRepAll,cRepInvBonds,-1);
        }
      } else {
        ObjectMoleculeInvalidate(I,cRepAll,cRepInvAtoms,-1);
      }
    }
  }

}
/*========================================================================*/
#if 0
ObjectMolecule *ObjectMoleculeLoadPDBFile(PyMOLGlobals *G,ObjectMolecule *obj,char *fname,
                                          int frame,int discrete,
                                          M4XAnnoType *m4x,PDBInfoRec *pdb_info)
{
  ObjectMolecule *I=NULL;
  int ok=true;
  FILE *f;
  long size;
  char *buffer,*p;

  f=fopen(fname,"rb");
  if(!f) {
    PRINTFB(G,FB_ObjectMolecule,FB_Errors)
      "ObjectMolecule-Error: Unable to open file '%s'\n",fname
      ENDFB(G);
    ok=false;
  } else
	 {
      PRINTFB(G,FB_ObjectMolecule,FB_Blather)
        " ObjectMoleculeLoadPDBFile: Loading from %s.\n",fname
        ENDFB(G);
		
		fseek(f,0,SEEK_END);
      size=ftell(f);
		fseek(f,0,SEEK_SET);

		buffer=(char*)mmalloc(size+255);
		ErrChkPtr(G,buffer);
		p=buffer;
		fseek(f,0,SEEK_SET);
		fread(p,size,1,f);
		p[size]=0;
		fclose(f);

		I=ObjectMoleculeReadPDBStr(G,obj,buffer,frame,discrete,m4x);

		mfree(buffer);
	 }

  return(I);
}
#endif
/*========================================================================*/
void ObjectMoleculeAppendAtoms(ObjectMolecule *I,AtomInfoType *atInfo,CoordSet *cs)
{
  int a;
  BondType *ii;
  BondType *si;
  AtomInfoType *src,*dest;
  int nAtom,nBond;

  if(I->NAtom) {
	 nAtom = I->NAtom+cs->NIndex;
	 VLACheck(I->AtomInfo,AtomInfoType,nAtom);	 
	 dest = I->AtomInfo+I->NAtom;
	 src = atInfo;
	 for(a=0;a<cs->NIndex;a++)
		*(dest++)=*(src++);
	 I->NAtom=nAtom;
	 VLAFreeP(atInfo);
  } else {
	 if(I->AtomInfo)
		VLAFreeP(I->AtomInfo);
	 I->AtomInfo = atInfo;
	 I->NAtom=cs->NIndex;
  }
  nBond=I->NBond+cs->NTmpBond;
  if(!I->Bond)
	 I->Bond=VLACalloc(BondType,nBond);
  VLACheck(I->Bond,BondType,nBond);
  ii=I->Bond+I->NBond;
  si=cs->TmpBond;
  for(a=0;a<cs->NTmpBond;a++) {
    ii->index[0]=cs->IdxToAtm[si->index[0]];
    ii->index[1]=cs->IdxToAtm[si->index[1]];
    ii->order=si->order;
    ii->stereo=si->stereo;
    ii->id=-1;
    ii++;
    si++;
  }
  I->NBond=nBond;
}
/*========================================================================*/
CoordSet *ObjectMoleculeGetCoordSet(ObjectMolecule *I,int setIndex)
{
  if((setIndex>=0)&&(setIndex<I->NCSet))
	 return(I->CSet[setIndex]);
  else
	 return(NULL);
}
/*========================================================================*/
void ObjectMoleculeTransformTTTf(ObjectMolecule *I,float *ttt,int frame) 
{
  int b;
  CoordSet *cs;
  for(b=0;b<I->NCSet;b++)
	{
     if((frame<0)||(frame==b)) {
       cs=I->CSet[b];
       if(cs) {
         if(cs->fInvalidateRep)
           cs->fInvalidateRep(I->CSet[b],cRepAll,cRepInvCoord);
         MatrixTransformTTTfN3f(cs->NIndex,cs->Coord,ttt,cs->Coord);
         CoordSetRecordTxfApplied(cs,ttt,false);
       }
     }
	}
}
/*========================================================================*/
void ObjectMoleculeSeleOp(ObjectMolecule *I,int sele,ObjectMoleculeOpRec *op)
{
  register float *coord;
  register int a,b,s;
  int c,d,t_i;
  int a1,ind;
  float r,rms;
  float v1[3],v2,*vv1,*vv2,*vt,*vt1,*vt2;
  int inv_flag;
  int hit_flag = false;
  int ok = true;
  int cnt;
  int skip_flag;
  int match_flag=false;
  int offset;
  int priority;
  register int use_matrices = false;
  CoordSet *cs;
  AtomInfoType *ai,*ai0,*ai_option;
  PyMOLGlobals *G=I->Obj.G;

  PRINTFD(G,FB_ObjectMolecule)
    " ObjectMoleculeSeleOp-DEBUG: sele %d op->code %d\n",sele,op->code
    ENDFD;
  if(sele>=0) {
    /* SelectorUpdateTableSingleObject(G,I,false,NULL,0);
    ** WLD 050808 -- why is the above statement present??? */

   /* always run on entry */
	switch(op->code) {
	case OMOP_ALTR: 
   case OMOP_AlterState:
     PBlock(G);
     /* PBlockAndUnlockAPI() is not safe.
      * what if "v" is invalidated by another thread? */
     break;
   }
   /* */
	switch(op->code) {
	case OMOP_AddHydrogens:
      ObjectMoleculeAddSeleHydrogens(I,sele,-1); /* state? */
      break;
	case OMOP_FixHydrogens:
      ObjectMoleculeFixSeleHydrogens(I,sele,-1); /* state? */
      break;
	case OMOP_PrepareFromTemplate:
      ai0=op->ai; /* template atom */
      for(a=0;a<I->NAtom;a++) {
         s=I->AtomInfo[a].selEntry;
         if(SelectorIsMember(G,s,sele)) {
           ai = I->AtomInfo + a;
           ai->hetatm=ai0->hetatm;
           ai->flags=ai0->flags;
           strcpy(ai->chain,ai0->chain);
           strcpy(ai->alt,ai0->alt);
           strcpy(ai->segi,ai0->segi);
           if(op->i1==1) { /* mode 1, merge residue information */
             strcpy(ai->resi,ai0->resi);
             ai->resv=ai0->resv;
             strcpy(ai->resn,ai0->resn);    
           }
           AtomInfoAssignColors(G,ai);
           if(op->i3) { 
             if((ai->elem[0]==ai0->elem[0])&&(ai->elem[1]==ai0->elem[1]))
               ai->color=ai0->color;
             else if(ai->protons == cAN_C) {
               ai->color=op->i4;
             }
           }
           for(b=0;b<cRepCnt;b++)
             ai->visRep[b]=ai0->visRep[b];
           ai->id=-1;
           ai->rank=-1;
           op->i2++;
         }
      }
      break;
    case OMOP_Sort:
      for(a=0;a<I->NAtom;a++) {
        s=I->AtomInfo[a].selEntry;
        if(SelectorIsMember(G,s,sele)) {
          ObjectMoleculeSort(I);
          break;
        }
      }
      break;
    case OMOP_SetAtomicSetting:
      ai = I->AtomInfo;
      for(a=0;a<I->NAtom;a++) {
        if(SelectorIsMember(G,ai->selEntry,sele)) {
          int uid = AtomInfoCheckUniqueID(G,ai);
          ai->has_setting = true;
          SettingUniqueSetTypedValue(G,uid,op->i1,op->i2,op->ii1);
          op->i4++;
        }
        ai++;
      }
      break;
   case OMOP_Pop:
     for(a=0;a<I->NAtom;a++)
       {
         s=I->AtomInfo[a].selEntry;
         if(SelectorIsMember(G,s,sele))
           {
             if(SelectorMoveMember(G,s,sele,op->i1)) {
               op->i2--;
               op->i3++;
             }
             if(!op->i2)
               break;
           }
       }
     break;
	case OMOP_PDB1:
	  for(b=0;b<I->NCSet;b++)
       if(I->CSet[b])
		  {
			if((b==op->i1)||(op->i1<0))
			  for(a=0;a<I->NAtom;a++)
				{
				  s=I->AtomInfo[a].selEntry;
              if(SelectorIsMember(G,s,sele))
                {
                  if(I->DiscreteFlag) {
                    if(I->CSet[b]==I->DiscreteCSet[a])
                      ind=I->DiscreteAtmToIdx[a];
                    else
                      ind=-1;
                  } else 
                    ind=I->CSet[b]->AtmToIdx[a];
                  if(ind>=0) 
                    CoordSetAtomToPDBStrVLA(G,&op->charVLA,&op->i2,I->AtomInfo+a,
                                            I->CSet[b]->Coord+(3*ind),op->i3,NULL);
                  op->i3++;
                }
				}
		  }
	  break;
	case OMOP_AVRT: /* average vertex coordinate */
      {
       register int op_i2 = op->i2;
       register int obj_TTTFlag = I->Obj.TTTFlag;
       if(op_i2) 
         use_matrices = SettingGet_b(I->Obj.G,I->Obj.Setting,NULL,cSetting_matrix_mode);
       for(a=0;a<I->NAtom;a++) {
         s=I->AtomInfo[a].selEntry;
         if((priority=SelectorIsMember(G,s,sele))) {
           cnt=0;
           for(b=0;b<I->NCSet;b++) {
             if( (cs=I->CSet[b]) ) {
               
               if(I->DiscreteFlag) {
                 if(cs==I->DiscreteCSet[a])
                   a1=I->DiscreteAtmToIdx[a];
                 else
                   a1=-1;
               } else 
                 a1=cs->AtmToIdx[a];
               if(a1>=0) {
                 if(!cnt) {
                   VLACheck(op->vv1,float,(op->nvv1*3)+2);
                   VLACheck(op->vc1,int,op->nvv1);
                 }
                 cnt++;
                 vv2=cs->Coord+(3*a1);
                 
                 if(op_i2) { /* do we want transformed coordinates? */
                   if(use_matrices) {
                     if(cs->State.Matrix) { /* state transformation */
                       transform44d3f(cs->State.Matrix,vv2,v1);
                       vv2=v1;
                     }
                   }
                   if(obj_TTTFlag) {
                     transformTTT44f3f(I->Obj.TTT,vv2,v1);
                     vv2=v1;
                   }
                 }
                 
                 vv1=op->vv1+(op->nvv1*3);
                 *(vv1++)+=*(vv2++);
                 *(vv1++)+=*(vv2++);
                 *(vv1++)+=*(vv2++);
               }
             }
           }
           op->vc1[op->nvv1]=cnt;
           if(cnt) {
             if(op->vp1) {
               VLACheck(op->vp1,int,op->nvv1);
               op->vp1[op->nvv1] = priority;
             }
             if(op->ai1VLA) {
               VLACheck(op->ai1VLA,AtomInfoType*,op->nvv1);
               op->ai1VLA[op->nvv1] = I->AtomInfo+a;
               I->AtomInfo[a].temp1 = a; /* KLUDGE ALERT!!! storing atom index in the temp1 field... */
             }
             op->nvv1++;
           }
         }
       }
      }
      break;
	case OMOP_StateVRT: /* state vertex coordinate */
      {
       register int op_i2 = op->i2;
       register int obj_TTTFlag = I->Obj.TTTFlag;
       if(op_i2) 
         use_matrices = SettingGet_b(I->Obj.G,I->Obj.Setting,NULL,cSetting_matrix_mode);

       for(a=0;a<I->NAtom;a++) {
         s=I->AtomInfo[a].selEntry;
         if((priority=SelectorIsMember(G,s,sele))) {
           cnt=0;
           b=op->i1;
           if(b<I->NCSet)
             if( (cs=I->CSet[b]) ) {
               if(I->DiscreteFlag) {
                 if(cs==I->DiscreteCSet[a])
                   a1=I->DiscreteAtmToIdx[a];
                 else
                   a1=-1;
               } else 
                 a1=cs->AtmToIdx[a];
               if(a1>=0) {
                 if(!cnt) {
                   VLACheck(op->vv1,float,(op->nvv1*3)+2);
                   VLACheck(op->vc1,int,op->nvv1);
                 }
                 cnt++;
                 vv2=cs->Coord+(3*a1);
                 
                 if(op_i2) { /* do we want transformed coordinates? */
                   if(use_matrices) {
                     if(cs->State.Matrix) { /* state transformation */
                       transform44d3f(cs->State.Matrix,vv2,v1);
                       vv2=v1;
                     }
                   }
                   if(obj_TTTFlag) {
                     transformTTT44f3f(I->Obj.TTT,vv2,v1);
                     vv2=v1;
                   }
                 }
                 
                 vv1=op->vv1+(op->nvv1*3);
                 *(vv1++)+=*(vv2++);
                 *(vv1++)+=*(vv2++);
                 *(vv1++)+=*(vv2++);
               }
             }
           op->vc1[op->nvv1]=cnt;
           if(cnt) {
             if(op->vp1) {
               VLACheck(op->vp1,int,op->nvv1);
               op->vp1[op->nvv1] = priority;
             }
             if(op->ai1VLA) {
               VLACheck(op->ai1VLA,AtomInfoType*,op->nvv1);
               op->ai1VLA[op->nvv1] = I->AtomInfo+a;
               I->AtomInfo[a].temp1 = a; /* KLUDGE ALERT!!! storing atom index in the temp1 field... */
             }
             op->nvv1++;
           }
         }
       }
      }
      break;
	case OMOP_SFIT: /* state fitting within a single object */
     vt = Alloc(float,3*op->nvv2); /* temporary (matching) target vertex pointers */
     cnt = 0;
     for(a=0;a<I->NAtom;a++)
       {
         s=I->AtomInfo[a].selEntry;
         if(SelectorIsMember(G,s,sele))
           {
             cnt++;
             break;
           }
       }
     if(cnt) { /* only perform action for selected object */
       
       for(b=0;b<I->NCSet;b++) {
         rms = -1.0;
         vt1 = vt; /* reset target vertex pointers */
         vt2 = op->vv2;
         t_i = 0; /* original target vertex index */
         if(I->CSet[b]&&(b!=op->i2))
           {
             op->nvv1=0;
             for(a=0;a<I->NAtom;a++)
               {
                 s=I->AtomInfo[a].selEntry;
                 if(SelectorIsMember(G,s,sele))
                   {
                     if(I->DiscreteFlag) {
                       if(I->CSet[b]==I->DiscreteCSet[a])
                         a1=I->DiscreteAtmToIdx[a];
                       else
                         a1=-1;
                     } else 
                       a1=I->CSet[b]->AtmToIdx[a];
                     if(a1>=0) {

                       match_flag=false;
                       while(t_i<op->nvv2) {
                         if(op->i1VLA[t_i]==a) {/* same atom? */
                           match_flag=true;
                           break;
                         }
                         if(op->i1VLA[t_i]<a) { /* catch up? */
                           t_i++;
                           vt2+=3;
                         } else 
                           break;
                       }
                       if(match_flag) {
                         VLACheck(op->vv1,float,(op->nvv1*3)+2);
                         vv2=I->CSet[b]->Coord+(3*a1);
                         vv1=op->vv1+(op->nvv1*3);
                         *(vv1++)=*(vv2++);
                         *(vv1++)=*(vv2++);
                         *(vv1++)=*(vv2++);
                         *(vt1++)=*(vt2);
                         *(vt1++)=*(vt2+1);
                         *(vt1++)=*(vt2+2);
                         op->nvv1++;
                       }
                     }
                   }
               }
             if(op->nvv1!=op->nvv2) {
               PRINTFB(G,FB_Executive,FB_Warnings)
                 "Executive-Warning: Missing atoms in state %d (%d instead of %d).\n",
                 b+1,op->nvv1,op->nvv2
                 ENDFB(G);
             }
             if(op->nvv1) {
               if(op->i1!=0) /* fitting flag */
                 rms = MatrixFitRMSTTTf(G,op->nvv1,op->vv1,vt,NULL,op->ttt);
               else 
                 rms = MatrixGetRMS(G,op->nvv1,op->vv1,vt,NULL);
               if((op->i1==2)) { 
                 ObjectMoleculeTransformTTTf(I,op->ttt,b);
                 
                 if(op->i3) {
                   const float divisor = (float)op->i3;
                   const float premult = (float)op->i3-1.0F;

                   /* mix flag is set, so average the prior target
                      coordinates with these coordinates */
                   
                   vt2 = op->vv2;
                   t_i = 0; /* original target vertex index */
                   for(a=0;a<I->NAtom;a++) {
                     s=I->AtomInfo[a].selEntry;
                     if(SelectorIsMember(G,s,sele))
                       {
                         if(I->DiscreteFlag) {
                           if(I->CSet[b]==I->DiscreteCSet[a])
                             a1=I->DiscreteAtmToIdx[a];
                           else
                             a1=-1;
                         } else 
                           a1=I->CSet[b]->AtmToIdx[a];
                         if(a1>=0) {
                           
                           match_flag=false;
                           while(t_i<op->nvv2) {
                             if(op->i1VLA[t_i]==a) {/* same atom? */
                               match_flag=true;
                               break;
                             }
                             if(op->i1VLA[t_i]<a) { /* catch up? */
                               t_i++;
                               vt2+=3;
                             } else 
                               break;
                           }
                           if(match_flag) {
                             vv2=I->CSet[b]->Coord+(3*a1);
                             *(vt2)   = ((premult * (* vt2   )) + *(vv2++))/divisor;
                             *(vt2+1) = ((premult * (*(vt2+1))) + *(vv2++))/divisor;
                             *(vt2+2) = ((premult * (*(vt2+2))) + *(vv2++))/divisor;
                           }
                         }
                       }
                   }
                 }
               }
             } else {
               PRINTFB(G,FB_Executive,FB_Warnings)
                 "Executive-Warning: No matches found for state %d.\n",b+1
                 ENDFB(G);
             }
           }
         VLACheck(op->f1VLA,float,b);
         op->f1VLA[b]=rms;
       }
       VLASize(op->f1VLA,float,I->NCSet);  /* NOTE this action is object-specific! */
     }
     FreeP(vt);
     break;
	case OMOP_SetGeometry: 
     for(a=0;a<I->NAtom;a++)
       {
         s=I->AtomInfo[a].selEntry;
         if(SelectorIsMember(G,s,sele))
           {
             ai = I->AtomInfo + a;
             ai->geom=op->i1;
             ai->valence=op->i2;
             ai->chemFlag = true;
             op->i3++;
             hit_flag=true;
             break;
           }
       }
     break;
	case OMOP_OnOff:
     for(a=0;a<I->NAtom;a++)
       {
         s=I->AtomInfo[a].selEntry;
         if(SelectorIsMember(G,s,sele))
           {
             hit_flag=true;
             break;
           }
       }
     break;
	case OMOP_SaveUndo: /* save undo */
     for(a=0;a<I->NAtom;a++)
       {
         s=I->AtomInfo[a].selEntry;
         if(SelectorIsMember(G,s,sele))
           {
             hit_flag=true;
             break;
           }
       }
     break;
	case OMOP_Identify: /* identify atoms */
     for(a=0;a<I->NAtom;a++)
       {
         s=I->AtomInfo[a].selEntry;
         if(SelectorIsMember(G,s,sele))
           {
             VLACheck(op->i1VLA,int,op->i1);
             op->i1VLA[op->i1++]=I->AtomInfo[a].id;
           }
       }
     break;
	case OMOP_GetBFactors: 
     ai=I->AtomInfo;
     for(a=0;a<I->NAtom;a++)
       {
         s=ai->selEntry;
         if(SelectorIsMember(G,s,sele))
           {
             op->ff1[op->i1]=ai->b;
             op->i1++;
           }
         ai++;
       }
     break;
	case OMOP_GetOccupancies: 
     ai=I->AtomInfo;
     for(a=0;a<I->NAtom;a++)
       {
         s=ai->selEntry;
         if(SelectorIsMember(G,s,sele))
           {
             op->ff1[op->i1]=ai->q;
             op->i1++;
           }
         ai++;
       }
     break;
	case OMOP_GetPartialCharges: 
     ai=I->AtomInfo;
     for(a=0;a<I->NAtom;a++)
       {
         s=ai->selEntry;
         if(SelectorIsMember(G,s,sele))
           {
             op->ff1[op->i1]=ai->partialCharge;
             op->i1++;
           }
         ai++;
       }
     break;
	case OMOP_IdentifyObjects: /* identify atoms */
     for(a=0;a<I->NAtom;a++)
       {
         s=I->AtomInfo[a].selEntry;
         if(SelectorIsMember(G,s,sele))
           {
             VLACheck(op->i1VLA,int,op->i1);
             op->i1VLA[op->i1]=I->AtomInfo[a].id; 
             VLACheck(op->obj1VLA,ObjectMolecule*,op->i1);
             op->obj1VLA[op->i1]=I;
             op->i1++;
           }
       }
     break;
	case OMOP_Index: /* identify atoms */
     for(a=0;a<I->NAtom;a++)
       {
         s=I->AtomInfo[a].selEntry;
         if(SelectorIsMember(G,s,sele))
           {
             VLACheck(op->i1VLA,int,op->i1);
             op->i1VLA[op->i1]=a; /* NOTE: need to incr by 1 before python */
             VLACheck(op->obj1VLA,ObjectMolecule*,op->i1);
             op->obj1VLA[op->i1]=I;
             op->i1++;
           }
       }
     break;
	case OMOP_GetObjects: /* identify atoms */
     for(a=0;a<I->NAtom;a++)
       {
         s=I->AtomInfo[a].selEntry;
         if(SelectorIsMember(G,s,sele))
           {
             VLACheck(op->obj1VLA,ObjectMolecule*,op->i1);
             op->obj1VLA[op->i1]=I;
             op->i1++;
             break;
           }
       }
     break;
	case OMOP_CountAtoms: /* count atoms in object, in selection */
     ai=I->AtomInfo;
     for(a=0;a<I->NAtom;a++)
       {
         s=ai->selEntry;
         if(SelectorIsMember(G,s,sele))
           op->i1++;
         ai++;
       }
     break;
   case OMOP_PhiPsi:
     ai=I->AtomInfo;
     for(a=0;a<I->NAtom;a++)
       {
         s=ai->selEntry;
         if(SelectorIsMember(G,s,sele)) {
           VLACheck(op->i1VLA,int,op->i1);
           op->i1VLA[op->i1]=a;
           VLACheck(op->obj1VLA,ObjectMolecule*,op->i1);
           op->obj1VLA[op->i1]=I;
           VLACheck(op->f1VLA,float,op->i1);
           VLACheck(op->f2VLA,float,op->i1);
           if(ObjectMoleculeGetPhiPsi(I,a,op->f1VLA+op->i1,op->f2VLA+op->i1,op->i2))
             op->i1++;
         }
         ai++; 
       }
     break;
	case OMOP_Cartoon: /* adjust cartoon type */
     ai=I->AtomInfo;
     for(a=0;a<I->NAtom;a++)
       {
         s=ai->selEntry;
         if(SelectorIsMember(G,s,sele)) {
           ai->cartoon = op->i1;
           op->i2++;
         }
         ai++; 
       }
     break;
	case OMOP_Protect: /* protect atoms from movement */
     ai=I->AtomInfo;
     for(a=0;a<I->NAtom;a++)
       {
         s=ai->selEntry;
         if(SelectorIsMember(G,s,sele))
           {
             ai->protekted = op->i1;
             op->i2++;
           }
         ai++;
       }
     break;
	case OMOP_Mask: /* protect atoms from selection */
     ai=I->AtomInfo;
     for(a=0;a<I->NAtom;a++)
       {
         s=ai->selEntry;
         if(SelectorIsMember(G,s,sele))
           {
             ai->masked = op->i1;
             op->i2++;
           }
         ai++;
       }
     break;
	case OMOP_SetB: /* set B-value */
     ai=I->AtomInfo;
     for(a=0;a<I->NAtom;a++)
       {
         s=ai->selEntry;
         if(SelectorIsMember(G,s,sele))
           {
             ai->b = op->f1;
             op->i2++;
           }
         ai++;
       }
     break;
	case OMOP_Remove: /* flag atoms for deletion */
     ai=I->AtomInfo;
     if(I->DiscreteFlag) /* for now, can't remove atoms from discrete objects */
       for(a=0;a<I->NAtom;a++)
         {         
           ai->deleteFlag=false;
           s=ai->selEntry;
           if(SelectorIsMember(G,s,sele))
             {
               ErrMessage(G,"Remove","Can't remove atoms from discrete objects.");
               break;
             }
           ai++;
         }
     else
       for(a=0;a<I->NAtom;a++)
         {         
           ai->deleteFlag=false;
           s=ai->selEntry;
           if(SelectorIsMember(G,s,sele))
             {
               ai->deleteFlag=true;
               op->i1++;
             }
           ai++;
         }
     break;

   case OMOP_GetChains:
     ai=I->AtomInfo;
     for(a=0;a<I->NAtom;a++)
       {         
         s=ai->selEntry;
         if(SelectorIsMember(G,s,sele))
           {
             op->ii1[(int)ai->chain[0]]++;
             op->i1++;
           }
         ai++;
       }
     break;

   case OMOP_Spectrum:
     ai=I->AtomInfo;
     ai0=NULL;
     for(a=0;a<I->NAtom;a++)
       {         
         s=ai->selEntry;
         if(SelectorIsMember(G,s,sele))
           {
             skip_flag=false;
             if(op->i4&&ai0) /* byres and we've done a residue */
               if(AtomInfoSameResidue(G,ai,ai0))
                 skip_flag=true;
             if(!skip_flag) {
               c = (int)(0.49999+op->i1*(op->ff1[op->i3] - op->f1)/op->f2);
               if(c<0) c=0;
               if(c>=op->i1) c=op->i1-1;
               ai->color=op->ii1[c];

               /*               printf("%8.3 %8.3\n",ai->partial_charge,*/
               if(op->i4) { /* byres */
                 offset = -1;
                 while((a+offset)>=0) {
                   ai0 = I->AtomInfo + a + offset;
                   if(AtomInfoSameResidue(G,ai,ai0)) {
                     ai0->color = op->ii1[c];
                     hit_flag=true;
                   } else 
                     break;
                   offset--;
                 }
                 offset = 1;
                 while((a+offset)<I->NAtom) {
                   ai0 = I->AtomInfo + a + offset;
                   if(AtomInfoSameResidue(G,ai,ai0)) {
                     ai0->color = op->ii1[c];
                     hit_flag=true;
                   } else 
                     break;
                   offset++;
                 }
               }
               ai0=ai;
                      }
             op->i3++;

           }
         ai++;
       }
     break;

   case OMOP_SingleStateVertices: /* same as OMOP_VERT for a single state */
     ai=I->AtomInfo;
     if(op->cs1<I->NCSet) {
       if(I->CSet[op->cs1]) {
         b = op->cs1;
         for(a=0;a<I->NAtom;a++)
           {         
             s=ai->selEntry;
             if(SelectorIsMember(G,s,sele))
               {
                 op->i1++;

                 if(I->DiscreteFlag) {
                   if(I->CSet[b]==I->DiscreteCSet[a])
                     a1=I->DiscreteAtmToIdx[a];
                   else
                     a1=-1;
                 } else 
                   a1=I->CSet[b]->AtmToIdx[a];
                 if(a1>=0) {
                   VLACheck(op->vv1,float,(op->nvv1*3)+2);
                   vv2=I->CSet[b]->Coord+(3*a1);
                   vv1=op->vv1+(op->nvv1*3);
                   *(vv1++)=*(vv2++);
                   *(vv1++)=*(vv2++);
                   *(vv1++)=*(vv2++);
                   op->nvv1++;
                 }
               }
             ai++;
           }
       }
     }
     break;
   case OMOP_CSetIdxGetAndFlag:
     ai=I->AtomInfo;
     for(a=0;a<I->NAtom;a++)
       {         
         s=ai->selEntry;
         if(SelectorIsMember(G,s,sele))
           {
             for(b=op->cs1;b<=op->cs2;b++) {
               offset = b-op->cs1;
               if(b<I->NCSet) {
                 if(I->CSet[b]) {
                   if(I->DiscreteFlag) {
                     if(I->CSet[b]==I->DiscreteCSet[a])
                       a1=I->DiscreteAtmToIdx[a];
                     else
                       a1=-1;
                   } else 
                     a1=I->CSet[b]->AtmToIdx[a];
                   if(a1>=0) {
                     op->ii1[op->i1*offset+op->i2] = 1; /* presence flag */
                     vv1=op->vv1+3*(op->i1*offset+op->i2); /* atom-based offset */
                     vv2=I->CSet[b]->Coord+(3*a1);
                     *(vv1++)=*(vv2++);
                     *(vv1++)=*(vv2++);
                     *(vv1++)=*(vv2++);
                     op->nvv1++;
                   }
                 }
               }
             }
             op->i2++; /* atom index field for atoms within selection...*/
           }
         ai++;
       }
     break;
   case OMOP_CSetIdxSetFlagged:
     ai=I->AtomInfo;
     hit_flag=false;
     for(a=0;a<I->NAtom;a++)
       {         
         s=ai->selEntry;
         if(SelectorIsMember(G,s,sele))
           {
             for(b=op->cs1;b<=op->cs2;b++) {
               offset = b-op->cs1;
               if(b<I->NCSet) {
                 if(I->CSet[b]) {
                   if(I->DiscreteFlag) {
                     if(I->CSet[b]==I->DiscreteCSet[a])
                       a1=I->DiscreteAtmToIdx[a];
                     else
                       a1=-1;
                   } else 
                     a1=I->CSet[b]->AtmToIdx[a];
                   if(a1>=0) {
                     if(op->ii1[op->i1*offset+op->i2]) { /* copy flag */
                       vv1=op->vv1+3*(op->i1*offset+op->i2); /* atom-based offset */
                       vv2=I->CSet[b]->Coord+(3*a1);
                       *(vv2++)=*(vv1++);
                       *(vv2++)=*(vv1++);
                       *(vv2++)=*(vv1++);
                       op->nvv1++;
                       hit_flag=true;
                     }
                   }
                 }
               }
             }
             op->i2++; /* atom index field for atoms within selection...*/
           }
         ai++;
       }
     break;
   case OMOP_SUMC: /* performance optimized to speed center & zoom actions */
     {
       register float *op_v1 = op->v1;
       register int op_i1 = op->i1;
       register int op_i2 = op->i2;
       register int obj_TTTFlag = I->Obj.TTTFlag;
       register int i_NCSet = I->NCSet;
       register int i_NAtom = I->NAtom;
       register int i_DiscreteFlag = I->DiscreteFlag;
       register CoordSet **i_CSet = I->CSet;
       if(op_i2) 
         use_matrices = SettingGet_b(I->Obj.G,I->Obj.Setting,NULL,cSetting_matrix_mode);
       ai = I->AtomInfo;
       for(a=0;a<i_NAtom;a++) {
         s=ai->selEntry;
         if(SelectorIsMember(G,s,sele)) {
           for(b=0;b<i_NCSet;b++) {
             if(i_DiscreteFlag) {
               if( (cs=I->DiscreteCSet[a]) )
                 a1=I->DiscreteAtmToIdx[a];
             } else {
               if( (cs=i_CSet[b]) )
                 a1=cs->AtmToIdx[a];
             }
             if(cs && (a1>=0) ) {
               coord = cs->Coord+3*a1;
               if(op_i2) { /* do we want transformed coordinates? */
                 if(use_matrices) {
                   if(cs->State.Matrix) { /* state transformation */
                     transform44d3f(cs->State.Matrix,coord,v1);
                     coord = v1;
                   }
                 }
                 if(obj_TTTFlag) {
                   transformTTT44f3f(I->Obj.TTT,coord,v1);
                   coord=v1;
                 }
               }
               add3f(op_v1,coord,op_v1);
               op_i1++;
             }
           }
         }
         ai++;
       }
       op->i1 = op_i1;
     }
     break;
   case OMOP_MNMX: /* performance optimized to speed center & zoom actions */
     {
       register float *op_v1 = op->v1;
       register float *op_v2 = op->v2;
       register int op_i1 = op->i1;
       register int op_i2 = op->i2;
       register int obj_TTTFlag = I->Obj.TTTFlag;
       register int i_NCSet = I->NCSet;
       register int i_NAtom = I->NAtom;
       register int i_DiscreteFlag = I->DiscreteFlag;
       register CoordSet **i_CSet = I->CSet;
       if(op_i2) 
         use_matrices = SettingGet_b(I->Obj.G,I->Obj.Setting,NULL,cSetting_matrix_mode);

       ai = I->AtomInfo;
       for(a=0;a<i_NAtom;a++) {
         s=ai->selEntry;
         if(SelectorIsMember(G,s,sele)) {
           for(b=0;b<i_NCSet;b++) {
             if(i_DiscreteFlag) {
               if( (cs=I->DiscreteCSet[a]) )
                 a1=I->DiscreteAtmToIdx[a];
             } else {
               if( (cs=i_CSet[b]) )
                 a1=cs->AtmToIdx[a];
             }
             if(cs && (a1>=0) ) {
               coord = cs->Coord+3*a1;
               if(op_i2) { /* do we want transformed coordinates? */
                 if(use_matrices) {
                   if(cs->State.Matrix) { /* state transformation */
                     transform44d3f(cs->State.Matrix,coord,v1);
                     coord = v1;
                   }
                 }
                 if(obj_TTTFlag) {
                   transformTTT44f3f(I->Obj.TTT,coord,v1);
                   coord=v1;
                 }
               }
               if(op_i1) {
                 if(op_v1[0]>coord[0]) op_v1[0]=coord[0];
                 if(op_v1[1]>coord[1]) op_v1[1]=coord[1];
                 if(op_v1[2]>coord[2]) op_v1[2]=coord[2];
                 if(op_v2[0]<coord[0]) op_v2[0]=coord[0];
                 if(op_v2[1]<coord[1]) op_v2[1]=coord[1];
                 if(op_v2[2]<coord[2]) op_v2[2]=coord[2];
               } else {
                 op_v1[0] = coord[0];
                 op_v1[1] = coord[1];
                 op_v1[2] = coord[2];
                 op_v2[0] = coord[0];
                 op_v2[1] = coord[1];
                 op_v2[2] = coord[2];
               }
               op_i1++;
             }
           }
         }
         ai++;
       }
       op->i1 = op_i1;
     }
     break;
   default:
     use_matrices = SettingGet_b(I->Obj.G,I->Obj.Setting,NULL,cSetting_matrix_mode);
     ai = I->AtomInfo;
     for(a=0;a<I->NAtom;a++)
		 {
		   switch(op->code) { 
         case OMOP_Flag: 
           ai->flags &= op->i2; /* clear flag using mask */
           op->i4++;
           /* no break here is intentional!  */
         case OMOP_FlagSet:
         case OMOP_FlagClear:
		   case OMOP_COLR: /* normal atom based loops */
		   case OMOP_VISI:
         case OMOP_CheckVis:
		   case OMOP_TTTF:
         case OMOP_ALTR:
         case OMOP_LABL:
         case OMOP_AlterState:
			 s=ai->selEntry;
          if(SelectorIsMember(G,s,sele))
            {
              switch(op->code) {
              case OMOP_Flag:
                ai->flags |= op->i1; /* set flag */
                op->i3++;
                break;
              case OMOP_FlagSet:
                ai->flags |= op->i1; /* set flag */
                op->i3++;
                break;
              case OMOP_FlagClear:
                ai->flags &= op->i2; /* clear flag */
                op->i3++;
                break;
              case OMOP_VISI:
                if(op->i1<0)
                  for(d=0;d<cRepCnt;d++) 
                    ai->visRep[d]=op->i2;                      
                else {
                  ai->visRep[op->i1]=op->i2;
                  if(op->i1==cRepCell) I->Obj.RepVis[cRepCell]=op->i2;
                }
                break;
                break;
              case OMOP_CheckVis:
                if(ai->visRep[op->i1]) {
                  op->i2 = true;
                }
                break;
              case OMOP_COLR:
                if(op->i1==cColorAtomic)
                  ai->color=ai->atomic_color;
                else
                  ai->color=op->i1;
                hit_flag=true;
                op->i2++;
                break;
              case OMOP_TTTF:
                hit_flag=true;
                break;
              case OMOP_LABL:
                if (ok) {
                  if(!op->s1[0]) {
                    if(ai->label) {
                      OVLexicon_DecRef(I->Obj.G->Lexicon,ai->label);
                      ai->label=0;
                    }
                    op->i1++;
                    ai->visRep[cRepLabel]=false;
                    hit_flag=true;
                  } else {
                    if(op->i2) {
                      /* python label expression evaluation */
                      if(PLabelAtom(I->Obj.G,&I->AtomInfo[a],I->Obj.Name,op->s1,a)) {
                        op->i1++;
                        ai->visRep[cRepLabel]=true;
                        hit_flag=true;
                      } else
                        ok=false;
                    } else {
                      /* simple string label text */
                      AtomInfoType *ai = I->AtomInfo + a;
                      char *label = op->s1;
                      if(ai->label) {
                        OVLexicon_DecRef(I->Obj.G->Lexicon,ai->label);
                      }
                      ai->label = 0;
                      if(label && label[0]) {
                        OVreturn_word ret = OVLexicon_GetFromCString(
                                              G->Lexicon,label);
                        if(OVreturn_IS_OK(ret)) {
                          ai->label = ret.word;
                        }
                      }
                    }
                  }
                }
                break;
              case OMOP_ALTR:
                if (ok) {
                  if(PAlterAtom(I->Obj.G,&I->AtomInfo[a],op->s1,op->i2,I->Obj.Name,a,op->py_ob1))
                    op->i1++;
                  else
                    ok=false;
                }
                break;
              case OMOP_AlterState:
                if (ok) {
                  if(op->i2<I->NCSet) {
                    cs = I->CSet[op->i2];
                    if(cs) {
                      if(I->DiscreteFlag) {
                        if(cs==I->DiscreteCSet[a])
                          a1=I->DiscreteAtmToIdx[a];
                        else
                          a1=-1;
                      } else 
                        a1=cs->AtmToIdx[a];
                      if(a1>=0) {
                        if(op->i4) 
                          ai_option = I->AtomInfo + a;
                        else
                          ai_option = NULL;
                        if(PAlterAtomState(I->Obj.G,cs->Coord+(a1*3),op->s1,op->i3,
                                           ai_option,I->Obj.Name,a,op->py_ob1)) {
                          op->i1++;
                          hit_flag=true;
                        } else
                          ok=false;
                      }
                    }
                  }
                }
                break;
              }
              break;
            }
          break;

          /* coord-set based properties, iterating only a single coordinate set */
         case OMOP_CSetMinMax:          
         case OMOP_CSetCameraMinMax:          
         case OMOP_CSetMaxDistToPt:
         case OMOP_CSetSumSqDistToPt:
         case OMOP_CSetSumVertices:
         case OMOP_CSetMoment: 
           cs = NULL;
           if((op->cs1>=0)&&(op->cs1<I->NCSet)) {
             cs=I->CSet[op->cs1];
           } else if(op->include_static_singletons) {
             if((I->NCSet==1)&&(SettingGet_b(G,NULL,I->Obj.Setting,cSetting_static_singletons))) {
               cs=I->CSet[0]; /*treat static singletons as present in each state */
             }
           }
           
           if(cs) {
             s=ai->selEntry;
             if(SelectorIsMember(G,s,sele))
               {
                 switch(op->code) {
                 case OMOP_CSetSumVertices:
                   if(I->DiscreteFlag) {
                     if(cs==I->DiscreteCSet[a])
                       a1=I->DiscreteAtmToIdx[a];
                     else
                       a1=-1;
                   } else 
                     a1=cs->AtmToIdx[a];
                   if(a1>=0) {
                     coord = cs->Coord+3*a1;
                     if(op->i2) { /* do we want transformed coordinates? */
                       if(use_matrices) {
                         if(cs->State.Matrix) { /* state transformation */
                           transform44d3f(cs->State.Matrix,coord,v1);
                           coord = v1;
                         }
                       }
                       if(I->Obj.TTTFlag) {
                         transformTTT44f3f(I->Obj.TTT,coord,v1);
                         coord=v1;
                       }
                     }
                     add3f(op->v1,coord,op->v1);
                     op->i1++;
                   }
                   break;
                 case OMOP_CSetMinMax:
                   if(I->DiscreteFlag) {
                     if(cs==I->DiscreteCSet[a])
                       a1=I->DiscreteAtmToIdx[a];
                     else
                       a1=-1;
                   } else 
                     a1=cs->AtmToIdx[a];
                   if(a1>=0)
                     {
                       coord = cs->Coord+3*a1;
                       if(op->i2) { /* do we want transformed coordinates? */
                         if(use_matrices) {
                           if(cs->State.Matrix) { /* state transformation */
                             transform44d3f(cs->State.Matrix,coord,v1);
                             coord = v1;
                           }
                         }
                         if(I->Obj.TTTFlag) {
                           transformTTT44f3f(I->Obj.TTT,coord,v1);
                           coord=v1;
                         }
                       }
                       if(op->i1) {
                         for(c=0;c<3;c++) {
                           if(*(op->v1+c)>*(coord+c)) *(op->v1+c)=*(coord+c);
                           if(*(op->v2+c)<*(coord+c)) *(op->v2+c)=*(coord+c);
                         }
                       } else {
                         for(c=0;c<3;c++) {
                           *(op->v1+c)=*(coord+c);
                           *(op->v2+c)=*(coord+c);
                         }
                       }
                       op->i1++;
                     }
                   break;
                 case OMOP_CSetCameraMinMax:
                   if(I->DiscreteFlag) {
                     if(cs==I->DiscreteCSet[a])
                       a1=I->DiscreteAtmToIdx[a];
                     else
                       a1=-1;
                   } else 
                     a1=cs->AtmToIdx[a];
                   if(a1>=0)
                     {
                       coord = cs->Coord+3*a1;
                       if(op->i2) { /* do we want transformed coordinates? */
                         if(use_matrices) {
                           if(cs->State.Matrix) { /* state transformation */
                             transform44d3f(cs->State.Matrix,coord,v1);
                             coord = v1;
                           }
                         }
                         if(I->Obj.TTTFlag) {
                           transformTTT44f3f(I->Obj.TTT,coord,v1);
                           coord=v1;
                         }
                       }
                       MatrixTransformC44fAs33f3f(op->mat1,coord,v1); /* convert to view-space */
                       coord=v1;
                       if(op->i1) {
                         for(c=0;c<3;c++) {
                           if(*(op->v1+c)>*(coord+c)) *(op->v1+c)=*(coord+c);
                           if(*(op->v2+c)<*(coord+c)) *(op->v2+c)=*(coord+c);
                         }
                       } else {
                         for(c=0;c<3;c++) {
                           *(op->v1+c)=*(coord+c);
                           *(op->v2+c)=*(coord+c);
                         }
                       }
                       op->i1++;
                     }
                   break;
                 case OMOP_CSetSumSqDistToPt:
                   if(I->DiscreteFlag) {
                     if(cs==I->DiscreteCSet[a])
                       a1=I->DiscreteAtmToIdx[a];
                     else
                       a1=-1;
                   } else 
                     a1=cs->AtmToIdx[a];
                   if(a1>=0) {
                     float dist;
                     coord = cs->Coord+3*a1;
                     if(op->i2) { /* do we want transformed coordinates? */
                       if(use_matrices) {
                         if(cs->State.Matrix) { /* state transformation */
                           transform44d3f(cs->State.Matrix,coord,v1);
                           coord = v1;
                         }
                       }
                       if(I->Obj.TTTFlag) {
                         transformTTT44f3f(I->Obj.TTT,coord,v1);
                         coord=v1;
                       }
                     }
                     dist = (float)diff3f(op->v1,coord);
                     op->d1 += dist * dist;
                     op->i1++;
                   }
                   break;
                 case OMOP_CSetMaxDistToPt:
                   if(I->DiscreteFlag) {
                     if(cs==I->DiscreteCSet[a])
                       a1=I->DiscreteAtmToIdx[a];
                     else
                       a1=-1;
                   } else 
                     a1=cs->AtmToIdx[a];
                   if(a1>=0)
                     {
                       float dist;
                       coord = cs->Coord+3*a1;
                       if(op->i2) { /* do we want transformed coordinates? */
                         if(use_matrices) {
                           if(cs->State.Matrix) { /* state transformation */
                             transform44d3f(cs->State.Matrix,coord,v1);
                             coord = v1;
                           }
                         }
                         if(I->Obj.TTTFlag) {
                           transformTTT44f3f(I->Obj.TTT,coord,v1);
                           coord=v1;
                         }
                       }
                       dist = (float)diff3f(op->v1,coord);
                       if(dist>op->f1)
                         op->f1=dist;
                       op->i1++;
                     }
                   break;
                 case OMOP_CSetMoment: 
                   if(I->DiscreteFlag) {
                     if(cs==I->DiscreteCSet[a])
                       a1=I->DiscreteAtmToIdx[a];
                     else
                       a1=-1;
                   } else 
                     a1=cs->AtmToIdx[a];
                   if(a1>=0) {
                     subtract3f(cs->Coord+(3*a1),op->v1,v1);
                     v2=v1[0]*v1[0]+v1[1]*v1[1]+v1[2]*v1[2]; 
                     op->d[0][0] += v2 - v1[0] * v1[0];
                     op->d[0][1] +=    - v1[0] * v1[1];
                     op->d[0][2] +=    - v1[0] * v1[2];
                     op->d[1][0] +=    - v1[1] * v1[0];
                     op->d[1][1] += v2 - v1[1] * v1[1];
                     op->d[1][2] +=    - v1[1] * v1[2];
                     op->d[2][0] +=    - v1[2] * v1[0];
                     op->d[2][1] +=    - v1[2] * v1[1];
                     op->d[2][2] += v2 - v1[2] * v1[2];
                   }
                   break;
                     
                 }
               }
           }
           break;
		   default: /* coord-set based properties, iterating as all coordsets within atoms */
           for(b=0;b<I->NCSet;b++) {
             if(I->DiscreteFlag) {
               cs=I->DiscreteCSet[a];
             } else {
               cs=I->CSet[b];
             }
             if(cs)
               {
                 s=ai->selEntry;
                 inv_flag=false;
                 if(SelectorIsMember(G,s,sele))
                   {
                     switch(op->code) {
#if 0
                /* pulled out for performance reasons */
                     case OMOP_SUMC:
                       if(I->DiscreteFlag) {
                         if(cs==I->DiscreteCSet[a])
                           a1=I->DiscreteAtmToIdx[a];
                         else
                           a1=-1;
                       } else 
                         a1=cs->AtmToIdx[a];
                       if(a1>=0)
                         {
                           coord = cs->Coord+3*a1;
                           if(op->i2) { /* do we want transformed coordinates? */
                             if(use_matrices) {
                               if(cs->State.Matrix) { /* state transformation */
                                 transform44d3f(cs->State.Matrix,coord,v1);
                                 coord = v1;
                               }
                             }
                             if(I->Obj.TTTFlag) {
                               transformTTT44f3f(I->Obj.TTT,coord,v1);
                               coord=v1;
                             }
                           }
                           add3f(op->v1,coord,op->v1);
                           op->i1++;
                         }
                       break;
                     case OMOP_MNMX:
                       if(I->DiscreteFlag) {
                         if(cs==I->DiscreteCSet[a])
                           a1=I->DiscreteAtmToIdx[a];
                         else
                           a1=-1;
                       } else 
                         a1=cs->AtmToIdx[a];
                       if(a1>=0)
                         {
                           coord = cs->Coord+3*a1;
                           if(op->i2) { /* do we want transformed coordinates? */
                             if(use_matrices) {
                               if(cs->State.Matrix) { /* state transformation */
                                 transform44d3f(cs->State.Matrix,coord,v1);
                                 coord = v1;
                               }
                             }
                             if(I->Obj.TTTFlag) {
                               transformTTT44f3f(I->Obj.TTT,coord,v1);
                               coord=v1;
                             }
                           }
                           if(op->i1) {
                             for(c=0;c<3;c++) {
                               if(*(op->v1+c)>*(coord+c)) *(op->v1+c)=*(coord+c);
                               if(*(op->v2+c)<*(coord+c)) *(op->v2+c)=*(coord+c);
                             }
                           } else {
                             for(c=0;c<3;c++) {
                               *(op->v1+c)=*(coord+c);
                               *(op->v2+c)=*(coord+c);
                             }
                           }
                           op->i1++;
                         }
                       break;
#endif

                     case OMOP_CameraMinMax:
                       if(I->DiscreteFlag) {
                         if(cs==I->DiscreteCSet[a])
                           a1=I->DiscreteAtmToIdx[a];
                         else
                           a1=-1;
                       } else 
                         a1=cs->AtmToIdx[a];
                       if(a1>=0)
                         {
                           coord = cs->Coord+3*a1;
                           if(op->i2) { /* do we want transformed coordinates? */
                             if(use_matrices) {
                               if(cs->State.Matrix) { /* state transformation */
                                 transform44d3f(cs->State.Matrix,coord,v1);
                                 coord = v1;
                               }
                             }
                             if(I->Obj.TTTFlag) {
                               transformTTT44f3f(I->Obj.TTT,coord,v1);
                               coord=v1;
                             }
                           }
                           MatrixTransformC44fAs33f3f(op->mat1,coord,v1); /* convert to view-space */
                           coord=v1;
                           if(op->i1) {
                             for(c=0;c<3;c++) {
                               if(*(op->v1+c)>*(coord+c)) *(op->v1+c)=*(coord+c);
                               if(*(op->v2+c)<*(coord+c)) *(op->v2+c)=*(coord+c);
                             }
                           } else {
                             for(c=0;c<3;c++) {
                               *(op->v1+c)=*(coord+c);
                               *(op->v2+c)=*(coord+c);
                             }
                           }
                           op->i1++;
                         }
                       break;
                     case OMOP_MaxDistToPt:
                       if(I->DiscreteFlag) {
                         if(cs==I->DiscreteCSet[a])
                           a1=I->DiscreteAtmToIdx[a];
                         else
                           a1=-1;
                       } else 
                         a1=cs->AtmToIdx[a];
                       if(a1>=0)
                         {
                           float dist;
                           coord = cs->Coord+3*a1;
                           if(op->i2) { /* do we want transformed coordinates? */
                             if(use_matrices) {
                               if(cs->State.Matrix) { /* state transformation */
                                 transform44d3f(cs->State.Matrix,coord,v1);
                                 coord = v1;
                               }
                             }
                             if(I->Obj.TTTFlag) {
                               transformTTT44f3f(I->Obj.TTT,coord,v1);
                               coord=v1;
                             }
                           }
                           dist = (float)diff3f(coord,op->v1);
                           if(dist>op->f1)
                             op->f1=dist;
                           op->i1++;
                         }
                       break;
                     case OMOP_MDST: 
                       if(I->DiscreteFlag) {
                         if(cs==I->DiscreteCSet[a])
                           a1=I->DiscreteAtmToIdx[a];
                         else
                           a1=-1;
                       } else 
                         a1=cs->AtmToIdx[a];
                       if(a1>=0)
                         {
                           r=(float)diff3f(op->v1,cs->Coord+(3*a1));
                           if(r>op->f1)
                             op->f1=r;
                         }
                       break;
                     case OMOP_INVA:
                       if(I->DiscreteFlag) {
                         if(cs==I->DiscreteCSet[a])
                           a1=I->DiscreteAtmToIdx[a];
                         else
                           a1=-1;
                       } else 
                         a1=cs->AtmToIdx[a]; 
                       if(a1>=0)                     /* selection touches this coordinate set */ 
                         inv_flag=true;              /* so set the invalidation flag */
                       break;
                     case OMOP_VERT: 
                       if(I->DiscreteFlag) {
                         if(cs==I->DiscreteCSet[a])
                           a1=I->DiscreteAtmToIdx[a];
                         else
                           a1=-1;
                       } else 
                         a1=cs->AtmToIdx[a];
                       if(a1>=0) {
                         VLACheck(op->vv1,float,(op->nvv1*3)+2);
                         vv2=cs->Coord+(3*a1);
                         vv1=op->vv1+(op->nvv1*3);
                         *(vv1++)=*(vv2++);
                         *(vv1++)=*(vv2++);
                         *(vv1++)=*(vv2++);
                         op->nvv1++;
                       }
                       break;	
                     case OMOP_SVRT:  /* gives us only vertices for a specific coordinate set */
                       if(b==op->i1) {
                         if(I->DiscreteFlag) {
                           if(cs==I->DiscreteCSet[a])
                             a1=I->DiscreteAtmToIdx[a];
                           else
                             a1=-1;
                         } else 
                           a1=cs->AtmToIdx[a];
                         if(a1>=0) {
                           VLACheck(op->vv1,float,(op->nvv1*3)+2);
                           VLACheck(op->i1VLA,int,op->nvv1);
                           op->i1VLA[op->nvv1]=a; /* save atom index for later comparisons */
                           vv2=cs->Coord+(3*a1);
                           vv1=op->vv1+(op->nvv1*3);
                           *(vv1++)=*(vv2++);
                           *(vv1++)=*(vv2++);
                           *(vv1++)=*(vv2++);
                           op->nvv1++;
                         }
                       }
                       break;	
                       /* Moment of inertia tensor - unweighted - assumes v1 is center of molecule */
                     case OMOP_MOME: 
                       if(I->DiscreteFlag) {
                         if(cs==I->DiscreteCSet[a])
                           a1=I->DiscreteAtmToIdx[a];
                         else
                           a1=-1;
                       } else 
                         a1=cs->AtmToIdx[a];
                       if(a1>=0) {
                         subtract3f(cs->Coord+(3*a1),op->v1,v1);
                         v2=v1[0]*v1[0]+v1[1]*v1[1]+v1[2]*v1[2]; 
                         op->d[0][0] += v2 - v1[0] * v1[0];
                         op->d[0][1] +=    - v1[0] * v1[1];
                         op->d[0][2] +=    - v1[0] * v1[2];
                         op->d[1][0] +=    - v1[1] * v1[0];
                         op->d[1][1] += v2 - v1[1] * v1[1];
                         op->d[1][2] +=    - v1[1] * v1[2];
                         op->d[2][0] +=    - v1[2] * v1[0];
                         op->d[2][1] +=    - v1[2] * v1[1];
                         op->d[2][2] += v2 - v1[2] * v1[2];
                       }
                       break;
                     }
                   }
                 switch(op->code) { /* full coord-set based */
                 case OMOP_INVA:
                   if(inv_flag) {
                     if(op->i1<0) {
                       /* invalidate all representations */
                       for(d=0;d<cRepCnt;d++) {
                         if(cs->fInvalidateRep)
                           cs->fInvalidateRep(cs,d,op->i2);
                       }
                     } else if(cs->fInvalidateRep) 
                       /* invalidate only that particular representation */
                       cs->fInvalidateRep(cs,op->i1,op->i2);
                   }
                   break;
                 }
                 if(I->DiscreteFlag) /* don't iterate every coordinate set for discrete objects! */
                   break;
               }
           } /* end coordset section */
           break;
         }
         ai++;
       }
     break;
	}
	if(hit_flag) {
	  switch(op->code) {
      case OMOP_COLR:
        ExecutiveUpdateColorDepends(I->Obj.G,I);
        break;
	  case OMOP_TTTF:
       ObjectMoleculeTransformTTTf(I,op->ttt,-1);
       break;
	  case OMOP_LABL:
       ObjectMoleculeInvalidate(I,cRepLabel,cRepInvText,-1);
       break;
     case OMOP_AlterState: /* overly coarse - doing all states, could do just 1 */
       if(!op->i3) { /* not read_only? */
         ObjectMoleculeInvalidate(I,-1,cRepInvRep,-1);
         SceneChanged(G);
       }
       break;
     case OMOP_CSetIdxSetFlagged:
       ObjectMoleculeInvalidate(I,-1,cRepInvRep,-1);
       SceneChanged(G);
       break;
     case OMOP_SaveUndo:
       op->i2=true;
       ObjectMoleculeSaveUndo(I,op->i1,false);
       break;
     case OMOP_OnOff:
       ExecutiveSetObjVisib(G,I->Obj.Name,op->i1);
       break;
	  }
	}

   /* always run on exit...*/
	switch(op->code) {
	case OMOP_ALTR:
   case OMOP_AlterState:
     PUnblock(G);
     break;
   }
   /* */
  }
}
/*========================================================================*/
void ObjectMoleculeDescribeElement(ObjectMolecule *I,int index, char *buffer) 
{
  AtomInfoType *ai;
  ai=I->AtomInfo+index;
  if(ai->alt[0])
    sprintf(buffer,"/%s/%s/%s/%s`%s/%s`%s",I->Obj.Name,ai->segi,ai->chain,ai->resn,ai->resi,ai->name,ai->alt);
    else
  sprintf(buffer,"/%s/%s/%s/%s`%s/%s",I->Obj.Name,ai->segi,ai->chain,ai->resn,ai->resi,ai->name);
}

/*========================================================================*/
void ObjectMoleculeGetAtomSele(ObjectMolecule *I,int index, char *buffer) 
{
  AtomInfoType *ai;
  ai=I->AtomInfo+index;
  if(ai->alt[0]) 
    sprintf(buffer,"/%s/%s/%s/%s`%s/%s`%s",I->Obj.Name,ai->segi,ai->chain,ai->resn,ai->resi,
            ai->name,ai->alt);
  else
    sprintf(buffer,"/%s/%s/%s/%s`%s/%s`",I->Obj.Name,ai->segi,ai->chain,ai->resn,ai->resi,
            ai->name);   
}
/*========================================================================*/
void ObjectMoleculeGetAtomSeleLog(ObjectMolecule *I,int index, char *buffer,int quote) 
{
  AtomInfoType *ai;
  char quo[5] = "";
  if(quote) {
    quo[0]='"';
    quo[1]=0;
  }
  if(SettingGet(I->Obj.G,cSetting_robust_logs)) {
    ai=I->AtomInfo+index;
    if(ai->alt[0]) 
      sprintf(buffer,"%s/%s/%s/%s/%s`%s/%s`%s%s",quo,I->Obj.Name,ai->segi,ai->chain,ai->resn,ai->resi,
              ai->name,ai->alt,quo);
    else
      sprintf(buffer,"%s/%s/%s/%s/%s`%s/%s`%s",quo,I->Obj.Name,ai->segi,ai->chain,ai->resn,ai->resi,
              ai->name,quo);   
  } else {
    sprintf(buffer,"%s(%s`%d)%s",quo,I->Obj.Name,index+1,quo);
  }
}

void ObjectMoleculeGetAtomSeleFast(ObjectMolecule *I,int index, char *buffer) 
{
  AtomInfoType *ai;
  WordType segi,chain,resi,name,alt;
  ai=I->AtomInfo+index;
  
  if(ai->segi[0]) {
    strcpy(segi,"s;");
    strcat(segi,ai->segi);
  } else {
    strcpy(segi,"s;''");
  }
  if(ai->chain[0]) {
    strcpy(chain,"c;");
    strcat(chain,ai->chain);
  } else {
    strcpy(chain,"c;''");
  }
  if(ai->resi[0]) {
    strcpy(resi,"i;");
    strcat(resi,ai->resi);
  } else {
    strcpy(resi,"i;''");
  }
  if(ai->name[0]) {
    strcpy(name,"n;");
    strcat(name,ai->name);
  } else {
    strcpy(name,"n;''");
  }
  if(ai->alt[0]) {
    strcpy(alt,"alt ");
    strcat(alt,ai->alt);
  } else {
    strcpy(alt,"alt ''");
  }
  sprintf(buffer,"(%s&%s&%s&%s&%s&%s)",I->Obj.Name,segi,chain,resi,name,alt);
}

/*========================================================================*/
int ObjectMoleculeGetNFrames(ObjectMolecule *I)
{
  return I->NCSet;
}

struct _CCoordSetUpdateThreadInfo {
  CoordSet *cs;
  int a;
};

void CoordSetUpdateThread(CCoordSetUpdateThreadInfo *T)
{
  if(T->cs && T->cs->fUpdate) {
    T->cs->fUpdate(T->cs,T->a);
  }
}

#ifndef _PYMOL_NOPY
static void ObjMolCoordSetUpdateSpawn(PyMOLGlobals *G,CCoordSetUpdateThreadInfo *Thread,int n_thread,int n_total)
{
  if(n_total==1) {
    CoordSetUpdateThread(Thread);
  } else if(n_total){
    int blocked;
    PyObject *info_list;
    int a,n=0;
    blocked = PAutoBlock(G);
    
    PRINTFB(G,FB_Scene,FB_Blather)
      " Scene: updating coordinate sets with %d threads...\n",n_thread
      ENDFB(G);
    info_list = PyList_New(n_total);
    for(a=0;a<n_total;a++) {
      PyList_SetItem(info_list,a,PyCObject_FromVoidPtr(Thread+a,NULL));
      n++;
    }
    PXDecRef(PyObject_CallMethod(G->P_inst->cmd,"_coordset_update_spawn","Oi",info_list,n_thread));
    Py_DECREF(info_list);
    PAutoUnblock(G,blocked);
  }
}
#endif

/*========================================================================*/
void ObjectMoleculeUpdate(ObjectMolecule *I)
{
  int a;
  OrthoBusyPrime(I->Obj.G);
  { /* note which representations are active */
    int b;
    AtomInfoType *ai = I->AtomInfo;    
    signed char *rv = I->RepVisCache;
    if(I->NCSet>1) {
      for(b=0;b<cRepCnt;b++)
        I->RepVisCache[b]=0;
      for(a=0;a<I->NAtom;a++) {
        for(b=0;b<cRepCnt;b++) {
          rv[b] = rv[b] || ai->visRep[b];
        }
        ai++;
      }
    } else {
      for(b=0;b<cRepCnt;b++) /* if only one coordinate set, then
                              * there's no benefit to pre-filtering
                              * the representations... */
        I->RepVisCache[b]=1;
    }
  }
  
  {
    int start = 0;
    int stop = I->NCSet;
       
    ObjectAdjustStateRebuildRange(&I->Obj,&start,&stop);
    if((I->NCSet==1)&&(SettingGet_b(I->Obj.G,I->Obj.Setting,NULL,cSetting_static_singletons))) {
      start=0;
      stop=1;
    }
    {
#ifndef _PYMOL_NOPY
      int n_thread  = SettingGetGlobal_i(I->Obj.G,cSetting_max_threads);
      int multithread = SettingGetGlobal_i(I->Obj.G,cSetting_async_builds);
      
      if(multithread&&(n_thread)&&(stop-start)>1) {
        int cnt = 0;

        ObjectMoleculeUpdateNeighbors(I); /* must precalculate to
         avoid race-condition since this isn't mutexed yet and
         neighbors are needed by cartoons */
 
        for(a=start;a<stop;a++)
          if(I->CSet[a]) cnt++;
        {
          CCoordSetUpdateThreadInfo *thread_info = Alloc(CCoordSetUpdateThreadInfo, cnt);
          if(thread_info) {
            cnt = 0;
            for(a=start;a<stop;a++) {
              if(I->CSet[a]) {
                thread_info[cnt].cs = I->CSet[a];
                thread_info[cnt].a = a;
                cnt++;
              }
            }
            ObjMolCoordSetUpdateSpawn(I->Obj.G,thread_info,n_thread,cnt);
            FreeP(thread_info);
          }
          
        }
        
      } else
#endif
        { /* single thread */
          for(a=start;a<stop;a++)
            if(I->CSet[a]) {	
              OrthoBusySlow(I->Obj.G,a,I->NCSet);
              PRINTFB(I->Obj.G,FB_ObjectMolecule,FB_Blather)
                " ObjectMolecule-DEBUG: updating representations for state %d of \"%s\".\n" 
                , a+1, I->Obj.Name
                ENDFB(I->Obj.G);
              if(I->CSet[a]->fUpdate) {
                I->CSet[a]->fUpdate(I->CSet[a],a);
              }
            }
        }
    }
    if(I->Obj.RepVis[cRepCell]) {
      if(I->Symmetry) {
        if(I->Symmetry->Crystal) {
          if(I->UnitCellCGO)
            CGOFree(I->UnitCellCGO);
          I->UnitCellCGO = CrystalGetUnitCellCGO(I->Symmetry->Crystal);
        }
      }
    }
  } 

  PRINTFD(I->Obj.G,FB_ObjectMolecule)
    " ObjectMolecule: updates complete for object %s.\n",I->Obj.Name
    ENDFD;
}
/*========================================================================*/
void ObjectMoleculeInvalidate(ObjectMolecule *I,int rep,int level,int state)
{
  int a;
  PRINTFD(I->Obj.G,FB_ObjectMolecule)
    " ObjectMoleculeInvalidate: entered. rep: %d level: %d\n",rep,level
    ENDFD;

  if(level>=cRepInvBonds) {
    VLAFreeP(I->Neighbor); /* set I->Neighbor to NULL */
    if(I->Sculpt) {
      SculptFree(I->Sculpt);
      I->Sculpt = NULL;
    }
    ObjectMoleculeUpdateNonbonded(I);
    if(level>=cRepInvAtoms) {
      SelectorUpdateObjectSele(I->Obj.G,I);
    }
  }
  PRINTFD(I->Obj.G,FB_ObjectMolecule)
    " ObjectMoleculeInvalidate: invalidating representations...\n"
    ENDFD;

  {
    int start = 0;
    int stop = I->NCSet;
    
    if(state>=0) {
      start = state;
      stop = state+1;
    } 
    if(stop>I->NCSet)
      stop = I->NCSet;
    for(a=start;a<stop;a++) {
      if(I->CSet[a]) {	 
        if(I->CSet[a]->fInvalidateRep) {
          I->CSet[a]->fInvalidateRep(I->CSet[a],rep,level);
        }
      }
    }
  }

  PRINTFD(I->Obj.G,FB_ObjectMolecule)
    " ObjectMoleculeInvalidate: leaving...\n"
    ENDFD;

}
/*========================================================================*/
int ObjectMoleculeMoveAtom(ObjectMolecule *I,int state,int index,float *v,int mode,int log)
{
  PyMOLGlobals *G=I->Obj.G;
  int result = 0;
  CoordSet *cs;
  if(!(I->AtomInfo[index].protekted==1)) {
    if(state<0) state=0;
    if(I->NCSet==1) state=0;
    state = state % I->NCSet;
    if((!I->CSet[state])&&(SettingGet_b(I->Obj.G,I->Obj.Setting,NULL,cSetting_all_states)))
      state=0;
    cs = I->CSet[state];
    if(cs) {
      result = CoordSetMoveAtom(I->CSet[state],index,v,mode);
      cs->fInvalidateRep(cs,cRepAll,cRepInvCoord);
      ExecutiveUpdateCoordDepends(I->Obj.G,I);
    }
  }
  if(log) {
    OrthoLineType line,buffer;
    if(SettingGet(I->Obj.G,cSetting_logging)) {
      ObjectMoleculeGetAtomSele(I,index,buffer);
      sprintf(line,"cmd.translate_atom(\"%s\",%15.9f,%15.9f,%15.9f,%d,%d,%d)\n",
              buffer,v[0],v[1],v[2],state+1,mode,0);
      PLog(G,line,cPLog_no_flush);
    }
  }
  return(result);
}
/*========================================================================*/
int ObjectMoleculeMoveAtomLabel(ObjectMolecule *I,int state,int index,float *v,int mode,int log)
{
  int result = 0;
  CoordSet *cs;
  if(!(I->AtomInfo[index].protekted==1)) {
    if(state<0) state=0;
    if(I->NCSet==1) state=0;
    state = state % I->NCSet;
    if((!I->CSet[state])&&(SettingGet_b(I->Obj.G,I->Obj.Setting,NULL,cSetting_all_states)))
      state=0;
    cs = I->CSet[state];
    if(cs) {
      result = CoordSetMoveAtomLabel(I->CSet[state],index,v,mode);
      cs->fInvalidateRep(cs,cRepLabel,cRepInvCoord);
    }
  }
#if 0
  if(log) {
    OrthoLineType line,buffer;
    if(SettingGet(I->Obj.G,cSetting_logging)) {
      ObjectMoleculeGetAtomSele(I,index,buffer);
      sprintf(line,"cmd.translate_atom(\"%s\",%15.9f,%15.9f,%15.9f,%d,%d,%d)\n",
              buffer,v[0],v[1],v[2],state+1,mode,0);
      PLog(G,line,cPLog_no_flush);
    }
  }
#endif

  return(result);
}
/*========================================================================*/
int ObjectMoleculeInitBondPath(ObjectMolecule *I,ObjectMoleculeBPRec *bp )
{
  int a;
  bp->dist = Alloc(int,I->NAtom);
  bp->list = Alloc(int,I->NAtom);
  for(a=0;a<I->NAtom;a++)
    bp->dist[a]=-1;
  bp->n_atom = 0;
  return 1;
}
/*========================================================================*/
int ObjectMoleculePurgeBondPath(ObjectMolecule *I,ObjectMoleculeBPRec *bp )
{
  FreeP(bp->dist);
  FreeP(bp->list);
  return 1;
}
/*========================================================================*/
int ObjectMoleculeGetBondPaths(ObjectMolecule *I,int atom,
                               int max,ObjectMoleculeBPRec *bp)
{
  /* returns list of bond counts from atom to all others 
     dist and list must be vla array pointers or NULL */

  int a,a1,a2,n;
  int cur;
  int n_cur;
  int b_cnt = 0;

  ObjectMoleculeUpdateNeighbors(I);
  
  /* reinitialize dist array (if we've done at least one pass) */

  for(a=0;a<bp->n_atom;a++)
    bp->dist[bp->list[a]]=-1;

  bp->n_atom = 0;
  bp->dist[atom] = 0;
  bp->list[bp->n_atom] = atom;
  bp->n_atom++;
  
  cur = 0;
  while(1) {
    b_cnt++;
    if(b_cnt>max) break;

    n_cur = bp->n_atom-cur;

    /* iterate through all current atoms */

    if(!n_cur) break;
    while(n_cur--) {
      a1 = bp->list[cur++];
      n=I->Neighbor[a1]; 
      n++; /* skip cnt */
      while(1) {
        a2=I->Neighbor[n];
        n+=2;
        if(a2<0) break;
        if(bp->dist[a2]<0) { /* for each atom not yet sampled... */
          bp->dist[a2]=b_cnt;
          bp->list[bp->n_atom]=a2;
          bp->n_atom++;
        }
      }
    }
  }
  return(bp->n_atom);
}
/*========================================================================*/
int ***ObjectMoleculeGetBondPrint(ObjectMolecule *I,int max_bond,int max_type,int *dim)
{
  int a,b,i,c;
  int at1,at2;
  int ***result=NULL;
  ObjectMoleculeBPRec bp;

  dim[0]=max_type+1;
  dim[1]=max_type+1;
  dim[2]=max_bond+1;
  
  result=(int***)UtilArrayMalloc((unsigned int*)dim,3,sizeof(int));
  UtilZeroMem(**result,dim[0]*dim[1]*dim[2]*sizeof(int));
  
  ObjectMoleculeInitBondPath(I,&bp);
  for(a=0;a<I->NAtom;a++) {
    at1 = I->AtomInfo[a].customType;
    if((at1>=0)&&(at1<=max_type)) {
      ObjectMoleculeGetBondPaths(I,a,max_bond,&bp);    
      for(b=0;b<bp.n_atom;b++)
        {
          i = bp.list[b];
          at2 = I->AtomInfo[i].customType;
          if((at2>=0)&&(at2<=max_type)) {
            c=bp.dist[i];
            result[at1][at2][c]++;
          }
        }
    }
  }
  ObjectMoleculePurgeBondPath(I,&bp);
  return(result);
}
/*========================================================================*/
float ObjectMoleculeGetAvgHBondVector(ObjectMolecule *I,int atom,
                                      int state,float *v,float *incoming)
     /* computes average / optima hydrogen bonding vector for an atom */
{
  float result = 0.0;
  int a1,a2,n;
  int vec_cnt = 0;
  float v_atom[3],v_neigh[3],v_diff[3],v_acc[3] = {0.0,0.0,0.0};
  int sp2_flag = false;
  int order;

  CoordSet *cs;

  ObjectMoleculeUpdateNeighbors(I);

  a1 = atom;
  if(state<0) state=0;
  if(I->NCSet==1) state=0; 
  state = state % I->NCSet;
  cs = I->CSet[state];
  if(cs) {
    if(CoordSetGetAtomVertex(cs,a1,v_atom)) { /* atom exists in this C-set */
      n=I->Neighbor[atom];
      n++;
      while(1) {
        a2=I->Neighbor[n];
        if(a2<0) break;
        order = I->Bond[I->Neighbor[n+1]].order;
        if((order==2)||(order==4)) {
          sp2_flag = true;
        }
        n+=2;
        
        if(I->AtomInfo[a2].protons!=1) { /* ignore hydrogens */
          if(CoordSetGetAtomVertex(cs,a2,v_neigh)) { 
            subtract3f(v_atom,v_neigh,v_diff);
            normalize3f(v_diff);
            add3f(v_diff,v_acc,v_acc);
            vec_cnt++;
          }
        }
      }
      if(vec_cnt) {
        result = (float)length3f(v_acc);
        result = result/vec_cnt;
        normalize23f(v_acc,v);
      } else {
        copy3f(v_acc,v);
      }
      
      if(incoming && (vec_cnt==1) && 
         (fabs(dot_product3f(v,incoming))<0.99F)) { 
        /* if we know where the donor is, and the acceptor can
           potentially rotate the lone pair, then we should optimally
           orient the acceptor, if possible */
        AtomInfoType *ai = I->AtomInfo + atom;
        float v_perp[3];
        float v_tmp1[3],v_tmp2[3];
        if( ((ai->protons==cAN_O)&&(!sp2_flag)) || /* C-O-H */
            ((ai->protons==cAN_N)&&(sp2_flag))){ /* C=N-H */
        
            remove_component3f(incoming,v,v_perp);
            normalize3f(v_perp);
            scale3f(v, 0.333644F, v_tmp1);
            scale3f(v_perp, 0.942699F, v_tmp2);
            add3f(v_tmp1,v_tmp2,v_tmp2);
            subtract3f(v,v_tmp2,v);
            normalize3f(v);
        }
      }
    }
  }
  return(result);
}
/*========================================================================*/
int ObjectMoleculeGetAtomVertex(ObjectMolecule *I,int state,int index,float *v)
{
  int result = 0;
  if(state<0) state=SettingGet_i(I->Obj.G,NULL,I->Obj.Setting,cSetting_state)-1;
  if(state<0) state=SceneGetState(I->Obj.G); 
  if(I->NCSet==1) state=0; /* static singletons always active here it seems */
  state = state % I->NCSet;
  if((!I->CSet[state])&&(SettingGet_b(I->Obj.G,I->Obj.Setting,NULL,cSetting_all_states)))
    state=0;
  if(I->CSet[state]) 
    result = CoordSetGetAtomVertex(I->CSet[state],index,v);

  return(result);
}
/*========================================================================*/
int ObjectMoleculeGetAtomTxfVertex(ObjectMolecule *I,int state,int index,float *v)
{
  int result = 0;
  if(state<0) state=SettingGet_i(I->Obj.G,NULL,I->Obj.Setting,cSetting_state)-1;
  if(state<0) state=SceneGetState(I->Obj.G); 
  if(I->NCSet==1) state=0; /* static singletons always active here it seems */
  state = state % I->NCSet;
  { 
    CoordSet *cs = I->CSet[state];
    if((!cs)&&(SettingGet_b(I->Obj.G,I->Obj.Setting,NULL,cSetting_all_states))) {
      state=0;
      cs = I->CSet[state];
    }
    if(cs) {
      result = CoordSetGetAtomTxfVertex(cs,index,v);
    }
  }
  return(result);
}
/*========================================================================*/
int ObjectMoleculeSetAtomVertex(ObjectMolecule *I,int state,int index,float *v)
{
  int result = 0;
  if(state<0) state=SettingGet_i(I->Obj.G,NULL,I->Obj.Setting,cSetting_state)-1;
  if(state<0) state=SceneGetState(I->Obj.G);
  if(I->NCSet==1) state=0;
  state = state % I->NCSet;
  if((!I->CSet[state])&&(SettingGet_b(I->Obj.G,I->Obj.Setting,NULL,cSetting_all_states)))
    state=0;
  if(I->CSet[state]) 
    result = CoordSetSetAtomVertex(I->CSet[state],index,v);
  return(result);
}
/*========================================================================*/
static void ObjectMoleculeRender(ObjectMolecule *I,RenderInfo *info)
{
  PyMOLGlobals *G = I->Obj.G;
  int state = info->state;
  CRay *ray = info->ray;
  Picking **pick = info->pick;
  int pass = info->pass;
  int a;
  int use_matrices = SettingGet_b(I->Obj.G,I->Obj.Setting,NULL,cSetting_matrix_mode);
  CoordSet *cs;
  int pop_matrix = false;

  PRINTFD(I->Obj.G,FB_ObjectMolecule)
    " ObjectMolecule: rendering %s pass %d...\n",I->Obj.Name,pass
    ENDFD;

  ObjectPrepareContext(&I->Obj,ray);

  if(I->UnitCellCGO&&(I->Obj.RepVis[cRepCell])) {
    if(ray) {
      /* need to apply object state matrix here */      
      CGORenderRay(I->UnitCellCGO,ray,ColorGet(I->Obj.G,I->Obj.Color),
                         I->Obj.Setting,NULL);
    } else if(G->HaveGUI && G->ValidContext) {
      if(pick) {
      } else {
        /* need to apply object state matrix here */
        ObjectUseColor(&I->Obj);
        CGORenderGL(I->UnitCellCGO,ColorGet(I->Obj.G,I->Obj.Color),
                    I->Obj.Setting,NULL,info);
      }
    }
  }

  PRINTFD(I->Obj.G,FB_ObjectMolecule)
    " ObjectMolecule: CGO's complete...\n"
    ENDFD;
  if(state<0) {
    for(a=0;a<I->NCSet;a++) {
      cs = I->CSet[a];
      if(cs && cs->fRender) {
        if(use_matrices) pop_matrix = ObjectStatePushAndApplyMatrix(&cs->State,info);
        /* need to apply object state matrix here */
        cs->fRender(cs,info);
        if(pop_matrix) ObjectStatePopMatrix(&cs->State,info);
    
      }
    }
  } else if(state<I->NCSet) {
    cs = I->CSet[(I->CurCSet = state % I->NCSet )];
    if(cs && cs->fRender) {
      if(use_matrices) pop_matrix = ObjectStatePushAndApplyMatrix(&cs->State,info);      
      cs->fRender(cs,info);
      if(pop_matrix) ObjectStatePopMatrix(&cs->State,info);
    }
  } else if(I->NCSet==1) { /* if only one coordinate set, assume static */
    cs = I->CSet[0];
    if(SettingGet_b(I->Obj.G,I->Obj.Setting,NULL,cSetting_static_singletons))
      if(cs && cs->fRender) {
        if(use_matrices) pop_matrix = ObjectStatePushAndApplyMatrix(&cs->State,info);      
        cs->fRender(cs,info);
        if(pop_matrix) ObjectStatePopMatrix(&cs->State,info);
      }
  }
  PRINTFD(I->Obj.G,FB_ObjectMolecule)
    " ObjectMolecule: rendering complete for object %s.\n",I->Obj.Name
    ENDFD;
}
/*========================================================================*/
void ObjectMoleculeDummyUpdate(ObjectMolecule *I,int mode)
{
  switch(mode) {
  case cObjectMoleculeDummyOrigin:
    SceneOriginGet(I->Obj.G,I->CSet[0]->Coord);
    break;
  case cObjectMoleculeDummyCenter:
    SceneGetPos(I->Obj.G,I->CSet[0]->Coord);
    break;
  }
}
/*========================================================================*/
ObjectMolecule *ObjectMoleculeDummyNew(PyMOLGlobals *G,int type)
{
  ObjectMolecule *I= NULL;
  
  int nAtom;
  float *coord = NULL;
  CoordSet *cset = NULL;
  AtomInfoType *atInfo = NULL;
  int frame=-1;

  I=ObjectMoleculeNew(G,false);

  nAtom=1;
  coord=VLAlloc(float,3*nAtom);
  zero3f(coord);
  
  atInfo=VLAMalloc(10,sizeof(AtomInfoType),2,true); /* autozero here is important */
  
  cset = CoordSetNew(G);
  cset->NIndex=nAtom;
  cset->Coord=coord;
  cset->TmpBond=NULL;
  cset->NTmpBond=0;
  strcpy(cset->Name,"_origin");
  
  cset->Obj = I;
  cset->fEnumIndices(cset);
  
  ObjectMoleculeMerge(I,atInfo,cset,false,cAIC_IDMask,true); /* NOTE: will release atInfo */
  
  if(frame<0) frame=I->NCSet;
  VLACheck(I->CSet,CoordSet*,frame);
  if(I->NCSet<=frame) I->NCSet=frame+1;
  if(I->CSet[frame]) I->CSet[frame]->fFree(I->CSet[frame]);
  I->CSet[frame] = cset;

  I->NBond = 0;
  I->Bond = VLACalloc(BondType,0);
  
  ObjectMoleculeExtendIndices(I);
  ObjectMoleculeSort(I);
  ObjectMoleculeUpdateIDNumbers(I);
  ObjectMoleculeUpdateNonbonded(I);

  return(I);
}

/*========================================================================*/

ObjectMolecule *ObjectMoleculeNew(PyMOLGlobals *G,int discreteFlag)
{
  int a;
  OOAlloc(G,ObjectMolecule);
  ObjectInit(G,(CObject*)I);
  I->Obj.type=cObjectMolecule;
  I->NAtom=0;
  I->NBond=0;
  I->CSet=VLAMalloc(10,sizeof(CoordSet*),5,true); /* auto-zero */
  I->NCSet=0;
  I->Bond=NULL;
  I->AtomCounter=-1;
  I->BondCounter=-1;
  I->DiscreteFlag=discreteFlag;
  I->NDiscrete=0;
  I->UnitCellCGO=NULL;
  I->Sculpt=NULL;
  I->CSTmpl=NULL;
  if(I->DiscreteFlag) { /* discrete objects don't share atoms between states */
    I->DiscreteAtmToIdx = VLAMalloc(10,sizeof(int),6,false);
    I->DiscreteCSet = VLAMalloc(10,sizeof(CoordSet*),5,false);
  } else {
    I->DiscreteAtmToIdx = NULL;
    I->DiscreteCSet = NULL;
  }    
  I->Obj.fRender=(void (*)(struct CObject *, RenderInfo *info))ObjectMoleculeRender;
  I->Obj.fFree= (void (*)(struct CObject *))ObjectMoleculeFree;
  I->Obj.fUpdate=  (void (*)(struct CObject *)) ObjectMoleculeUpdate;
  I->Obj.fGetNFrame = (int (*)(struct CObject *)) ObjectMoleculeGetNFrames;
  I->Obj.fInvalidate = (void (*)(struct CObject *,int rep, int level, int state))
    ObjectMoleculeInvalidate;
  I->Obj.fDescribeElement = (void (*)(struct CObject *,int index,char *buffer))
    ObjectMoleculeDescribeElement;
  I->Obj.fGetSettingHandle = (CSetting **(*)(struct CObject *,int state))
    ObjectMoleculeGetSettingHandle;
  I->Obj.fGetObjectState = (CObjectState *(*)(struct CObject *,int state))
    ObjectMoleculeGetObjectState;

  I->Obj.fGetCaption = (char *(*)(struct CObject *))ObjectMoleculeGetCaption;
  I->AtomInfo=VLAMalloc(10,sizeof(AtomInfoType),2,true); /* autozero here is important */
  I->CurCSet=0;
  I->Symmetry=NULL;
  I->Neighbor=NULL;
  for(a=0;a<=cUndoMask;a++) {
    I->UndoCoord[a]=NULL;
    I->UndoState[a]=-1;
  }
  I->UndoIter=0;
  return(I);
}
/*========================================================================*/
ObjectMolecule *ObjectMoleculeCopy(ObjectMolecule *obj)
{
  int a;
  BondType *i0,*i1;
  AtomInfoType *a0,*a1;
  OOAlloc(obj->Obj.G,ObjectMolecule);
  (*I)=(*obj);
  I->Symmetry=SymmetryCopy(I->Symmetry); /* null-safe */
  I->UnitCellCGO=NULL;
  I->Neighbor=NULL;
  I->Sculpt=NULL;
  I->Obj.Setting=NULL; /* TODO - make a copy */
  for(a=0;a<=cUndoMask;a++)
    I->UndoCoord[a]=NULL;
  I->CSet=VLAMalloc(I->NCSet,sizeof(CoordSet*),5,true); /* auto-zero */
  for(a=0;a<I->NCSet;a++) {
    I->CSet[a]=CoordSetCopy(obj->CSet[a]);
    I->CSet[a]->Obj=I;
  }
  if(obj->CSTmpl)
    I->CSTmpl = CoordSetCopy(obj->CSTmpl);
  else
    I->CSTmpl=NULL;
  I->Bond=VLACalloc(BondType,I->NBond);
  i0=I->Bond;
  i1=obj->Bond;
  for(a=0;a<I->NBond;a++) {
    *(i0++)=*(i1++); /* copy structure */
  }
  i0=I->Bond;
  for(a=0;a<I->NBond;a++) {
    (i0++)->unique_id = 0; /* clear unique_id */
  }
  
  I->AtomInfo=VLAlloc(AtomInfoType,I->NAtom);
  a0=I->AtomInfo;
  a1=obj->AtomInfo;
  for(a=0;a<I->NAtom;a++)
    *(a0++)=*(a1++);

  a0 = I->AtomInfo;
  for(a=0;a<I->NAtom;a++) {
    a0->selEntry = 0;
    a0->unique_id = 0;
    a0++;
  }
  
  return(I);

}

/*========================================================================*/
void ObjectMoleculeFree(ObjectMolecule *I)
{
  int a;

  SceneObjectDel(I->Obj.G,(CObject*)I);
  SelectorPurgeObjectMembers(I->Obj.G,I);
  for(a=0;a<I->NCSet;a++)
	 if(I->CSet[a]) {
      if(I->CSet[a]->fFree)
        I->CSet[a]->fFree(I->CSet[a]);
		I->CSet[a]=NULL;
	 }
  if(I->Symmetry) SymmetryFree(I->Symmetry);
  VLAFreeP(I->Neighbor);
  VLAFreeP(I->DiscreteAtmToIdx);
  VLAFreeP(I->DiscreteCSet);
  VLAFreeP(I->CSet);
  {
    int nAtom = I->NAtom;
    AtomInfoType *ai = I->AtomInfo;
    
    for(a=0;a<nAtom;a++) {
      AtomInfoPurge(I->Obj.G,ai);
      ai++;
    }
    VLAFreeP(I->AtomInfo);
  }
  {
    int nBond = I->NBond;
    BondType *bi = I->Bond;
    
    for(a=0;a<nBond;a++) {
      AtomInfoPurgeBond(I->Obj.G,bi);
      bi++;
    }
    VLAFreeP(I->Bond);
  }
  if(I->UnitCellCGO) 
    CGOFree(I->UnitCellCGO);
  for(a=0;a<=cUndoMask;a++)
    FreeP(I->UndoCoord[a]);
  if(I->Sculpt)
    SculptFree(I->Sculpt);
  if(I->CSTmpl)
    if(I->CSTmpl->fFree)
      I->CSTmpl->fFree(I->CSTmpl);
  ObjectPurge(&I->Obj);
  OOFreeP(I);
}

/*========================================================================*/
ObjectMolecule *ObjectMoleculeReadMMDStr(PyMOLGlobals *G,ObjectMolecule *I,char *MMDStr,int frame,int discrete)
{
  int ok = true;
  CoordSet *cset=NULL;
  AtomInfoType *atInfo;
  int isNew;
  int nAtom;

  if(!I) 
	 isNew=true;
  else 
	 isNew=false;

  if(isNew) {
    I=(ObjectMolecule*)ObjectMoleculeNew(G,discrete);
    atInfo = I->AtomInfo;
  } else {
    atInfo=VLAMalloc(10,sizeof(AtomInfoType),2,true); /* autozero here is important */
  }
  
  if(isNew) {
      I->Obj.Color = AtomInfoUpdateAutoColor(G);
  }

  cset=ObjectMoleculeMMDStr2CoordSet(G,MMDStr,&atInfo);  

  if(!cset) 
	 {
		VLAFreeP(atInfo);
		ok=false;
	 }
  
  if(ok)
	 {
		if(!I) 
		  I=(ObjectMolecule*)ObjectMoleculeNew(G,discrete);
		if(frame<0)
		  frame=I->NCSet;
		if(I->NCSet<=frame)
		  I->NCSet=frame+1;
		VLACheck(I->CSet,CoordSet*,frame);
      nAtom=cset->NIndex;

      if(I->DiscreteFlag&&atInfo) {
        int a;
        int fp1 = frame+1;
        AtomInfoType *ai = atInfo;
        for(a=0;a<nAtom;a++) {
          (ai++)->discrete_state = fp1;
        }
      }

      cset->Obj = I;
      if(cset->fEnumIndices)
        cset->fEnumIndices(cset);
      if(cset->fInvalidateRep)
        cset->fInvalidateRep(cset,cRepAll,cRepInvRep);
      if(isNew) {		
        I->AtomInfo=atInfo; /* IMPORTANT to reassign: this VLA may have moved! */
        I->NAtom=nAtom;
      } else {
        ObjectMoleculeMerge(I,atInfo,cset,false,cAIC_MMDMask,true); /* NOTE: will release atInfo */
      }
      if(frame<0) frame=I->NCSet;
      VLACheck(I->CSet,CoordSet*,frame);
      if(I->NCSet<=frame) I->NCSet=frame+1;
      I->CSet[frame] = cset;
      if(isNew) I->NBond = ObjectMoleculeConnect(I,&I->Bond,I->AtomInfo,cset,false);
      SceneCountFrames(G);
      ObjectMoleculeExtendIndices(I);
      ObjectMoleculeSort(I);
      ObjectMoleculeUpdateIDNumbers(I);
      ObjectMoleculeUpdateNonbonded(I);
	 }
  return(I);
}
/*========================================================================*/
ObjectMolecule *ObjectMoleculeLoadMMDFile(PyMOLGlobals *G,ObjectMolecule *obj,char *fname,
                                          int frame,char *sepPrefix,int discrete)
{
  ObjectMolecule* I=NULL;
  int ok=true;
  FILE *f;
  int oCnt=0;
  long size;
  char *buffer,*p;
  char cc[MAXLINELEN],oName[WordLength];
  int nLines;
  f=fopen(fname,"rb");
  if(!f)
	 ok=ErrMessage(G,"ObjectMoleculeLoadMMDFile","Unable to open file!");
  else
	 {
      PRINTFB(G,FB_ObjectMolecule,FB_Blather)
        " ObjectMoleculeLoadMMDFile: Loading from %s.\n",fname
        ENDFB(G);
		fseek(f,0,SEEK_END);
      size=ftell(f);
		fseek(f,0,SEEK_SET);
		buffer=(char*)mmalloc(size+255);
		ErrChkPtr(G,buffer);
		p=buffer;
		fseek(f,0,SEEK_SET);
		fread(p,size,1,f);
		p[size]=0;
		fclose(f);
      p=buffer;
      while(ok) {
        ncopy(cc,p,6);
        if(sscanf(cc,"%d",&nLines)!=1)
          break;
        if(ok) {
          if(sepPrefix) {
            I=ObjectMoleculeReadMMDStr(G,NULL,p,frame,discrete);
            oCnt++;
            sprintf(oName,"%s-%02d",sepPrefix,oCnt);
            ObjectSetName((CObject*)I,oName);
            ExecutiveManageObject(G,(CObject*)I,true,false);
          } else {
            I=ObjectMoleculeReadMMDStr(G,obj,p,frame,discrete);
            obj=I;
          }
          p=nextline(p);
          while(nLines--)
            p=nextline(p);
        }
      }
		mfree(buffer);
	 }

  return(I);
}

/*========================================================================*/
ObjectMolecule *ObjectMoleculeReadPDBStr(PyMOLGlobals *G,ObjectMolecule *I,char *PDBStr,int frame,
                                         int discrete,M4XAnnoType *m4x,char *pdb_name,
                                         char **next_pdb,PDBInfoRec *pdb_info,int quiet,int *model_number)
{
  CoordSet *cset = NULL;
  AtomInfoType *atInfo;
  int ok=true;
  int isNew = true;
  unsigned int nAtom = 0;
  char *start,*restart=NULL;
  int repeatFlag = true;
  int successCnt = 0;
  unsigned int aic_mask = cAIC_PDBMask;
  const float _1 = 1.0F;

  SegIdent segi_override=""; /* saved segi for corrupted NMR pdb files */

  start=PDBStr;
  while(repeatFlag) {
    repeatFlag = false;
    
    if(!I) 
      isNew=true;
    else 
      isNew=false;
    
    if(ok) {
      
      if(isNew) {
        I=(ObjectMolecule*)ObjectMoleculeNew(G,discrete);
        atInfo = I->AtomInfo;
        isNew = true;
      } else {
        atInfo=VLAMalloc(10,sizeof(AtomInfoType),2,true); /* autozero here is important */
        isNew = false;
      }
      if(isNew) {
        I->Obj.Color = AtomInfoUpdateAutoColor(G);
      }

      cset=ObjectMoleculePDBStr2CoordSet(G,start,&atInfo,&restart,
                                         segi_override,m4x,pdb_name,
                                         next_pdb,pdb_info,quiet,model_number);	
      if(m4x) /* preserve original atom IDs for annotated Metaphorics files */
        if(m4x->annotated_flag)
          aic_mask = (cAIC_b|cAIC_q);
      nAtom=cset->NIndex;
    }
    if(pdb_name&&(*next_pdb)) {
      /* problematic scenario? */
    }

    /* include coordinate set */
    if(ok) {
      
      if(I->DiscreteFlag&&atInfo) {
        unsigned int a;
        int fp1 = frame+1;
        AtomInfoType *ai = atInfo;
        for(a=0;a<nAtom;a++) {
          (ai++)->discrete_state = fp1;
        }
      }

      cset->Obj = I;
      cset->fEnumIndices(cset);
      if(cset->fInvalidateRep)
        cset->fInvalidateRep(cset,cRepAll,cRepInvRep);
      if(isNew) {		
        I->AtomInfo=atInfo; /* IMPORTANT to reassign: this VLA may have moved! */
      } else {
        ObjectMoleculeMerge(I,atInfo,cset,true,aic_mask,true); /* NOTE: will release atInfo */
      }
      if(isNew) I->NAtom=nAtom;
      if(frame<0) frame=I->NCSet;
      if(*model_number > 0) {
        if(SettingGetGlobal_b(G,cSetting_pdb_honor_model_number))
          frame = *model_number - 1;
      }
      VLACheck(I->CSet,CoordSet*,frame);
      if(I->NCSet<=frame) I->NCSet=frame+1;
      if(I->CSet[frame]) I->CSet[frame]->fFree(I->CSet[frame]);
      I->CSet[frame] = cset;
      if(isNew) I->NBond = ObjectMoleculeConnect(I,&I->Bond,I->AtomInfo,cset,true);
      if(cset->Symmetry&&(!I->Symmetry)) {
        I->Symmetry=SymmetryCopy(cset->Symmetry);
        if(SymmetryAttemptGeneration(I->Symmetry,quiet)) {
          /* check scale records */
          if(pdb_info &&
             SettingGetGlobal_b(G,cSetting_pdb_insure_orthogonal) &&
             pdb_info->scale.flag[0] &&
             pdb_info->scale.flag[1] &&
             pdb_info->scale.flag[2] ) {

            int skipit=true;
            float threshold = 0.001F;
            float *r2f = I->Symmetry->Crystal->RealToFrac, *sca = pdb_info->scale.matrix;

            /* are the matrices sufficiently close to be the same? */
            if(     fabs(r2f[0]-sca[0])>threshold) skipit=false;
            else if(fabs(r2f[1]-sca[1])>threshold) skipit=false;
            else if(fabs(r2f[2]-sca[2])>threshold) skipit=false;
            else if(fabs(r2f[3]-sca[4])>threshold) skipit=false;
            else if(fabs(r2f[4]-sca[5])>threshold) skipit=false;
            else if(fabs(r2f[5]-sca[6])>threshold) skipit=false;
            else if(fabs(r2f[6]-sca[8])>threshold) skipit=false;
            else if(fabs(r2f[7]-sca[9])>threshold) skipit=false;
            else if(fabs(r2f[8]-sca[10])>threshold) skipit=false;
            else if(fabs(sca[3])>threshold) skipit=false;
            else if(fabs(sca[7])>threshold) skipit=false;
            else if(fabs(sca[11])>threshold) skipit=false;

            /* is SCALEn the identity matrix?  If so, then it
               should probably be ignored... */
            {
              int is_identity = true;
              if(     fabs(sca[0]-_1)>threshold) is_identity=false;
              else if(fabs(sca[1])>threshold) is_identity=false;
              else if(fabs(sca[2])>threshold) is_identity=false;
              else if(fabs(sca[4])>threshold) is_identity=false;
              else if(fabs(sca[5]-_1)>threshold) is_identity=false;
              else if(fabs(sca[6])>threshold) is_identity=false;
              else if(fabs(sca[8])>threshold) is_identity=false;
              else if(fabs(sca[9])>threshold) is_identity=false;
              else if(fabs(sca[10]-_1)>threshold) is_identity=false;
              else if(fabs(sca[3])>threshold) is_identity=false;
              else if(fabs(sca[7])>threshold) is_identity=false;
              else if(fabs(sca[11])>threshold) is_identity=false;
              if(is_identity) {
                skipit=true;
                if(!quiet) {
                  PRINTFB(G,FB_ObjectMolecule,FB_Blather)
                    " ObjectMolReadPDBStr: ignoring SCALEn (identity matrix).\n"
                    ENDFB(G);
                }
              }
            }
            /* is SCALEn invalid?  If so, then it
               should definitely be ignored... */
            {
              int is_valid = true;
              if(length3f(sca)<R_SMALL8) is_valid = false;
              if(length3f(sca+4)<R_SMALL8) is_valid = false;
              if(length3f(sca+8)<R_SMALL8) is_valid = false;
              if(!is_valid) {
                skipit=true;
                if(!quiet) {
                  PRINTFB(G,FB_ObjectMolecule,FB_Blather)
                    " ObjectMolReadPDBStr: ignoring SCALEn (invalid matrix).\n"
                    ENDFB(G);
                }
              }
            }
            if(!skipit) {
              if(!quiet) {
                PRINTFB(G,FB_ObjectMolecule,FB_Actions)
                  " ObjectMolReadPDBStr: using SCALEn to compute orthogonal coordinates.\n"
                  ENDFB(G);
              }
              CoordSetTransform44f(cset, pdb_info->scale.matrix);
              CoordSetTransform33f(cset, I->Symmetry->Crystal->FracToReal); 
            }
          }
        }
      }
      SceneCountFrames(G);
      ObjectMoleculeExtendIndices(I);
      ObjectMoleculeSort(I);
      ObjectMoleculeUpdateIDNumbers(I);
      ObjectMoleculeUpdateNonbonded(I);
      ObjectMoleculeAutoDisableAtomNameWildcard(I);

      successCnt++;
      if(!quiet) {
        if(successCnt>1) {
          if(successCnt==2){
            PRINTFB(G,FB_ObjectMolecule,FB_Actions)
              " ObjectMolReadPDBStr: read MODEL %d\n",1
              ENDFB(G);
          }
          PRINTFB(G,FB_ObjectMolecule,FB_Actions)
            " ObjectMolReadPDBStr: read MODEL %d\n",successCnt
            ENDFB(G);
        }
      }
    }
    if(restart) {
      repeatFlag=true;
      start=restart;
      frame=frame+1;
    }
  }
  return(I);
}
/*========================================================================*/
CoordSet *ObjectMoleculeMMDStr2CoordSet(PyMOLGlobals *G,char *buffer,AtomInfoType **atInfoPtr)
{
  char *p;
  int nAtom,nBond;
  int a,c,bPart,bOrder;
  float *coord = NULL;
  CoordSet *cset = NULL;
  AtomInfoType *atInfo = NULL,*ai;
  char cc[MAXLINELEN];
  float *f;
  BondType *ii,*bond=NULL;
  int ok=true;
  int auto_show_lines = (int)SettingGet(G,cSetting_auto_show_lines);
  int auto_show_spheres = (int)SettingGet(G,cSetting_auto_show_spheres);
  int auto_show_nonbonded = (int)SettingGet(G,cSetting_auto_show_nonbonded);

  p=buffer;
  nAtom=0;
  if(atInfoPtr)
	 atInfo = *atInfoPtr;


  if(ok) {
	 p=ncopy(cc,p,6);
	 if(sscanf(cc,"%d",&nAtom)!=1)
		ok=ErrMessage(G,"ReadMMDFile","bad atom count");
  }

  if(ok) {
	 coord=VLAlloc(float,3*nAtom);
	 if(atInfo)
		VLACheck(atInfo,AtomInfoType,nAtom);	 
  }

  if(!atInfo)
    ErrFatal(G,"MMDStr2CoordSet","need atom information record!"); /* failsafe for old version..*/

  nBond=0;
  if(ok) {
	 bond=VLACalloc(BondType,6*nAtom);  
  }
  p=nextline(p);

  /* read coordinates and atom names */

  if(ok) { 
	 f=coord;
	 ii=bond;
	 for(a=0;a<nAtom;a++)
		{
        ai=atInfo+a;

        ai->id=a+1;
        ai->rank=a;
        if(ok) {
          p=ncopy(cc,p,4);
          if(sscanf(cc,"%d",&ai->customType)!=1) 
            ok=ErrMessage(G,"ReadMMDFile","bad atom type");
        }
        if(ok) {
          if(ai->customType<=14) strcpy(ai->elem,"C");
          else if(ai->customType<=23) strcpy(ai->elem,"O");
          else if(ai->customType<=40) strcpy(ai->elem,"N");
          else if(ai->customType<=48) strcpy(ai->elem,"H");
          else if(ai->customType<=52) strcpy(ai->elem,"S");
          else if(ai->customType<=53) strcpy(ai->elem,"P");
          else if(ai->customType<=55) strcpy(ai->elem,"B");
          else if(ai->customType<=56) strcpy(ai->elem,"F");
          else if(ai->customType<=57) strcpy(ai->elem,"Cl");           
          else if(ai->customType<=58) strcpy(ai->elem,"Br");           
          else if(ai->customType<=59) strcpy(ai->elem,"I");           
          else if(ai->customType<=60) strcpy(ai->elem,"Si");           
          else if(ai->customType<=61) strcpy(ai->elem,"Du");           
          else if(ai->customType<=62) strcpy(ai->elem,"Z0");
          else if(ai->customType<=63) strcpy(ai->elem,"Lp");
          else strcpy(ai->elem,"?");
        }
        for(c=0;c<6;c++) {
          if(ok) {
            p=ncopy(cc,p,8);
            if(sscanf(cc,"%d%d",&bPart,&bOrder)!=2)
              ok=ErrMessage(G,"ReadMMDFile","bad bond record");
            else {
              if(bPart&&bOrder&&(a<(bPart-1))) {
                nBond++;
                ii->index[0]=a;
                ii->index[1]=bPart-1;
                ii->order=bOrder;
                ii->stereo=0;
                ii->id=-1;
                ii++;
              }
            }
          }
        }
        if(ok) {
          p=ncopy(cc,p,12);
          if(sscanf(cc,"%f",f++)!=1)
            ok=ErrMessage(G,"ReadMMDFile","bad coordinate");
        }
        if(ok) {
          p=ncopy(cc,p,12);
          if(sscanf(cc,"%f",f++)!=1)
            ok=ErrMessage(G,"ReadMMDFile","bad coordinate");
        }
        if(ok) {
          p=ncopy(cc,p,12);
			 if(sscanf(cc,"%f",f++)!=1)
				ok=ErrMessage(G,"ReadMMDFile","bad coordinate");
		  }
        if(ok) {
          p=nskip(p,1);
          p=ncopy(cc,p,5);
          ai->resv = AtomResvFromResi(cc);
          sprintf(ai->resi,"%d",ai->resv);
        }
        if(ok) {
          p=nskip(p,6);
          p=ncopy(cc,p,9);
			 if(sscanf(cc,"%f",&ai->partialCharge)!=1)
				ok=ErrMessage(G,"ReadMMDFile","bad charge");
        }
        if(ok) {
          p=nskip(p,10);
          p=ncopy(cc,p,3);
          if(sscanf(cc,"%s",ai->resn)!=1)
            ai->resn[0]=0;
          ai->hetatm=true;
        }

        ai->segi[0]=0;
        ai->alt[0]=0;

        if(ok) {
          p=nskip(p,2);
          p=ncopy(ai->name,p,4);
          UtilCleanStr(ai->name);
          if(ai->name[0]==0) {
            strcpy(ai->name,ai->elem);
            sprintf(cc,"%02d",a+1);
            if((strlen(cc)+strlen(ai->name))>4)
              strcpy(ai->name,cc);
            else
              strcat(ai->name,cc);
          }

          for(c=0;c<cRepCnt;c++) {
            ai->visRep[c] = false;
          }
          ai->visRep[cRepLine] = auto_show_lines; /* show lines by default */
          ai->visRep[cRepNonbonded] = auto_show_nonbonded; /* show lines by default */
          ai->visRep[cRepSphere] = auto_show_spheres;
        }
        if(ok) {
          AtomInfoAssignParameters(G,ai);
          AtomInfoAssignColors(G,ai);
        }
        if(!ok)
          break;
        p=nextline(p);
  }
}
  if(ok)
    VLASize(bond,BondType,nBond);
  if(ok) {
	 cset = CoordSetNew(G);
	 cset->NIndex=nAtom;
	 cset->Coord=coord;
	 cset->NTmpBond=nBond;
	 cset->TmpBond=bond;
  } else {
	 VLAFreeP(bond);
	 VLAFreeP(coord);
  }
  if(atInfoPtr)
	 *atInfoPtr = atInfo;
  return(cset);
}


#if 0
ObjectMolecule *ObjectMoleculeReadMOL2Str(PyMOLGlobals *G,ObjectMolecule *I,
                                          char *MOLStr,int frame,int discrete,
                                          int quiet,int multiplex, char *new_name,
                                          char **next_entry)
{
  int ok = true;
  CoordSet *cset=NULL;
  AtomInfoType *atInfo;
  int isNew;
  int nAtom;
  char *restart=NULL,*start;
  int repeatFlag=true;
  int successCnt = 0;
  char tmpName[WordLength];
  int deferred_tasks = false;

  *next_entry = NULL;

  start=MOLStr;
  while(repeatFlag) {
    repeatFlag=false;

    if(!I) 
      isNew=true;
    else 
      isNew=false;

    if(isNew) {
      I=(ObjectMolecule*)ObjectMoleculeNew(G,(discrete>0));
      atInfo = I->AtomInfo;
      isNew = true;
    } else {
      atInfo=VLAMalloc(10,sizeof(AtomInfoType),2,true); /* autozero here is important */
      isNew = false;
    }

    if(isNew) {
      I->Obj.Color = AtomInfoUpdateAutoColor(G);
    }

    restart=NULL;
    cset=ObjectMoleculeMOL2Str2CoordSet(G,start,&atInfo,&restart);

    if(restart && (discrete<0)) { /* make object discrete if default behavior selected */
      ObjectMoleculeSetDiscrete(G,I,true);
      discrete = true;
    }

    if(!cset) 
      {
        ObjectMoleculeFree(I);
        I=NULL;
        ok=false;
      }
  
    if(ok)
      {
        if(frame<0)
          frame=I->NCSet;
        if(I->NCSet<=frame)
          I->NCSet=frame+1;
        VLACheck(I->CSet,CoordSet*,frame);
      
        nAtom=cset->NIndex;

        if(I->DiscreteFlag&&atInfo) {
          int a;
          int fp1 = frame+1;
          AtomInfoType *ai = atInfo;
          for(a=0;a<nAtom;a++) {
            (ai++)->discrete_state = fp1;
          }
        }

        if(multiplex>0) 
          UtilNCopy(tmpName,cset->Name,WordLength);

        cset->Obj = I;
        cset->fEnumIndices(cset);
        if(cset->fInvalidateRep)
          cset->fInvalidateRep(cset,cRepAll,cRepInvRep);
        if(isNew) {		
          I->AtomInfo=atInfo; /* IMPORTANT to reassign: this VLA may have moved! */
        } else {
          ObjectMoleculeMerge(I,atInfo,cset,false,cAIC_MOLMask,false); /* NOTE: will release atInfo */
        }

        if(isNew) I->NAtom=nAtom;
        if(frame<0) frame=I->NCSet;
        VLACheck(I->CSet,CoordSet*,frame);
        if(I->NCSet<=frame) I->NCSet=frame+1;
        if(I->CSet[frame]) I->CSet[frame]->fFree(I->CSet[frame]);
        I->CSet[frame] = cset;
      
        if(isNew) I->NBond = ObjectMoleculeConnect(I,&I->Bond,I->AtomInfo,cset,false);
        
        ObjectMoleculeExtendIndices(I);
        ObjectMoleculeSort(I);
            
        deferred_tasks = true;
        successCnt++;
        if(!quiet) {
          if(successCnt>1) {
            if(successCnt==2) {
              PRINTFB(G,FB_ObjectMolecule,FB_Actions)
                " ObjectMolReadMOL2Str: read molecule %d\n",1
                ENDFB(G);
            }
            PRINTFB(G,FB_ObjectMolecule,FB_Actions)
            " ObjectMolReadMOL2Str: read molecule %d\n",successCnt
              ENDFB(G);
          }
        }
      }
    if(multiplex>0) {
      UtilNCopy(new_name,tmpName,WordLength);
      if(restart) {
        *next_entry = restart;
      }
    } else if(restart) {
      repeatFlag=true;
      start=restart;
      frame=frame+1;
    }
  }
  if(deferred_tasks&&I) { /* defer time-consuming tasks until all states have been loaded */
    SceneCountFrames(G);
    ObjectMoleculeInvalidate(I,cRepAll,cRepInvAll,-1);
    ObjectMoleculeUpdateIDNumbers(I);
    ObjectMoleculeUpdateNonbonded(I);
  }
  return(I);
}
#endif

#if 0
ObjectMolecule *ObjectMoleculeReadStr(PyMOLGlobals *G,ObjectMolecule *I,
                                      char *st,int content_format, int frame,int discrete,
                                      int quiet,int multiplex, char *new_name,
                                      char **next_entry)

ObjectMolecule *ObjectMoleculeReadMOLStr(PyMOLGlobals *G,ObjectMolecule *I,
                                         char *MOLStr,int frame,
                                         int discrete,int finish)
{
  int ok = true;
  CoordSet *cset=NULL;
  AtomInfoType *atInfo;
  int isNew;
  int nAtom;
  char *resume;

  if(!I) 
	 isNew=true;
  else 
	 isNew=false;

  if(isNew) {
    I=(ObjectMolecule*)ObjectMoleculeNew(G,discrete);
    atInfo = I->AtomInfo;
    isNew = true;
  } else {
    atInfo=VLAMalloc(10,sizeof(AtomInfoType),2,true); /* autozero here is important */
    isNew = false;
  }

  if(isNew) {
      I->Obj.Color = AtomInfoUpdateAutoColor(G);
  }

  cset=ObjectMoleculeMOLStr2CoordSet(G,MOLStr,&atInfo,&resume);
  
  if(!cset) 
	 {
      ObjectMoleculeFree(I);
      I=NULL;
		ok=false;
	 }
  
  if(ok)
	 {
		if(frame<0)
		  frame=I->NCSet;
		if(I->NCSet<=frame)
		  I->NCSet=frame+1;
		VLACheck(I->CSet,CoordSet*,frame);
      
      nAtom=cset->NIndex;

      if(I->DiscreteFlag&&atInfo) {
        int a;
        int fp1 = frame+1;
        AtomInfoType *ai = atInfo;
        for(a=0;a<nAtom;a++) {
          (ai++)->discrete_state = fp1;
        }
      }

      cset->Obj = I;
      cset->fEnumIndices(cset);
      if(cset->fInvalidateRep)
        cset->fInvalidateRep(cset,cRepAll,cRepInvRep);
      if(isNew) {		
        I->AtomInfo=atInfo; /* IMPORTANT to reassign: this VLA may have moved! */
      } else {
        ObjectMoleculeMerge(I,atInfo,cset,false,cAIC_MOLMask,finish); /* NOTE: will release atInfo */
      }

      if(isNew) I->NAtom=nAtom;
      if(frame<0) frame=I->NCSet;
      VLACheck(I->CSet,CoordSet*,frame);
      if(I->NCSet<=frame) I->NCSet=frame+1;
      if(I->CSet[frame]) I->CSet[frame]->fFree(I->CSet[frame]);
      I->CSet[frame] = cset;
      
      if(isNew) I->NBond = ObjectMoleculeConnect(I,&I->Bond,I->AtomInfo,cset,false);
      
      SceneCountFrames(G);
      ObjectMoleculeExtendIndices(I);
      ObjectMoleculeSort(I);
      if(finish) {
        ObjectMoleculeUpdateIDNumbers(I);
        ObjectMoleculeUpdateNonbonded(I);
      }
	 }
  return(I);
}

ObjectMolecule *ObjectMoleculeLoadMOLFile(PyMOLGlobals *G,ObjectMolecule *obj,char *fname,int frame,int discrete)
{
  ObjectMolecule* I=NULL;
  int ok=true;
  FILE *f;
  long size;
  char *buffer,*p;

  f=fopen(fname,"rb");
  if(!f)
	 ok=ErrMessage(G,"ObjectMoleculeLoadMOLFile","Unable to open file!");
  else
	 {
      PRINTFB(G,FB_ObjectMolecule,FB_Blather)
        " ObjectMoleculeLoadMOLFile: Loading from %s.\n",fname
        ENDFB(G);
		
		fseek(f,0,SEEK_END);
      size=ftell(f);
		fseek(f,0,SEEK_SET);

		buffer=(char*)mmalloc(size+255);
		ErrChkPtr(G,buffer);
		p=buffer;
		fseek(f,0,SEEK_SET);
		fread(p,size,1,f);
		p[size]=0;
		fclose(f);
		I=ObjectMoleculeReadMOLStr(G,obj,buffer,frame,discrete,true);
		mfree(buffer);
	 }

  return(I);
}
#endif
