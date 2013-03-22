

/* 
A* -------------------------------------------------------------------
B* This file contains source code for the PyMOL computer program
C* Copyright (c) Schrodinger, LLC. 
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
#include"Parse.h"

#ifndef _PYMOL_INLINE

char *ParseNextLine(char *p)
{
  register char ch;
  const char mask = -16;        /* 0xF0 */
  while((mask & p[0]) && (mask & p[1]) && (mask & p[2]) && (mask & p[3]))       /* trusting short-circuit to avoid overrun */
    p += 4;
  while((ch = *p)) {
    p++;
    if(ch == 0xD) {             /* Mac or PC */
      if((*p) == 0xA)           /* PC */
        return p + 1;
      return p;
    } else if(ch == 0xA) {      /* Unix */
      return p;
    }
  }
  return p;
}


/*========================================================================*/

char *ParseNCopy(char *q, char *p, int n)
{                               /* n character copy */
  register char ch;
  while((ch = *p)) {
    if((ch == 0xD) || (ch == 0xA))      /* don't copy end of lines */
      break;
    if(!n)
      break;
    n--;
    p++;
    *(q++) = ch;
  }
  *q = 0;
  return p;
}

#endif

char *ParseSkipEquals(char *p)
{
  while(*p) {
    if(*p != '=')
      p++;
    else
      break;
  }

  if(*p) {
    p++;
    while(*p) {
      if(*p < 33)               /* skip whitespace */
        p++;
      else
        break;
    }
  }
  return p;
}


/*========================================================================*/
char *ParseIntCopy(char *q, char *p, int n)
{                               /* integer copy */
  while(*p) {
    if((*p == 0xD) || (*p == 0xA))      /* don't skip end of lines */
      break;
    if(*p <= 32 || !((*p >= '0') && (*p <= '9')))
      p++;
    else
      break;
  }
  while(*p) {
    if(*p <= 32)
      break;
    if(!n)
      break;
    if((*p == 0xD) || (*p == 0xA))      /* don't copy end of lines */
      break;
    if(!((*p >= '0') && (*p <= '9')))
      break;
    *(q++) = *(p++);
    n--;
  }
  *q = 0;
  return p;
}


/*========================================================================*/
char *ParseAlphaCopy(char *q, char *p, int n)
{                               /* integer copy */
  while(*p) {
    if((*p == 0xD) || (*p == 0xA))      /* don't skip end of lines */
      break;
    if(*p <= 32 || !(((*p >= 'A') && (*p <= 'Z')) || ((*p >= 'a') && (*p <= 'z'))))
      p++;
    else
      break;
  }
  while(*p) {
    if(*p <= 32)
      break;
    if(!n)
      break;
    if((*p == 0xD) || (*p == 0xA))      /* don't copy end of lines */
      break;
    if(!(((*p >= 'A') && (*p <= 'Z')) || ((*p >= 'a') && (*p <= 'z'))))
      break;
    *(q++) = *(p++);
    n--;
  }
  *q = 0;
  return p;
}
#include <stdio.h>
#include <string.h>
#include <math.h>
/* ParseFloat3List: scan in Python-like list of 3 floats */
int ParseFloat3List(char *parg, float *vals){
  char buf[256], blen;
  int cont = 1;
  char *p = parg;
  int pl = 0, vpl = 0, i;
  char *npl;
  double dval;
  while (cont){
    switch(p[pl]){
    case ' ':
      pl++;
      break;
    default:
      cont = 0;
    }
  }
  if (p[pl] == '[') pl++;
  cont = 1;
  while (vpl<3){
    while (p[pl] == ' '){
      pl++;
    }
    if (!p[pl])
      break;
    npl = strchr(&p[pl], ',');
    if (npl){
      blen = (npl- &p[pl] );
      strncpy(buf, &p[pl], blen);
      buf[blen] = 0;
      for (i=0; i<blen; i++){
	if (!isdigit(buf[i]) && buf[i]!='.'){
	  cont = 0;
	}
      }
    } else {
      strcpy(buf, &p[pl]);
      blen = strlen(buf);
      if (buf[blen-1]==']')
	buf[blen-1] = 0;
    }
    cont &= sscanf(buf, "%lf", &dval);
    if (!cont) break;
    vals[vpl++] = (float)dval;
    if (npl){
      pl = npl-p + 1;
    } else {
      break;
    }
  }
  return vpl==3;
}

/*========================================================================*/
char *ParseWordCopy(char *q, char *p, int n)
{                               /* word copy */
  while(*p) {
    if((*p == 0xD) || (*p == 0xA))      /* don't skip end of lines */
      break;
    if(*p <= 32)
      p++;
    else
      break;
  }
  while(*p) {
    if(*p <= 32)
      break;
    if(!n) {
      while(*p > 32)            /* finish scanning word, but don't copy into field */
        p++;
      break;
    }
    if((*p == 0xD) || (*p == 0xA))      /* don't copy end of lines */
      break;
    *(q++) = *(p++);
    n--;
  }
  *q = 0;
  return p;
}


/*========================================================================*/
char *ParseWordNumberCopy(char *q, char *p, int n)
{                               /* word copy */
  int digit_seen_last = 0;
  while(*p) {
    if((*p == 0xD) || (*p == 0xA))      /* don't skip end of lines */
      break;
    if(*p <= 32)
      p++;
    else
      break;
  }
  while(*p) {
    if(*p <= 32)
      break;
    if(!n) {
      while(*p > 32)            /* finish scanning word, but don't copy into field */
        p++;
      break;
    }
    if((*p == 0xD) || (*p == 0xA))      /* don't copy end of lines */
      break;
    if(digit_seen_last && (*p == '-'))  /* parse 123.123-1234.465 as two separate words */
      break;
    digit_seen_last = (((*p) >= '0') && ((*p) <= '9')) || ((*p) == '.');
    *(q++) = *(p++);
    n--;
  }
  *q = 0;
  return p;
}


/*========================================================================
 * ParseWord
 *  Copy first word from p into q with fewer than n letters
 * PARAMS
 *  char* q
 *    destination; will contain the first word from p
 *  char* p
 *    source; comes in as a string of tokens
 *  RETURNS
 *    p modified so it points to one character past the end of the first
 *    word of p.  Eg.  "im a temp selection" => " a temp selection" and
 *    the return value *q = "im"
 */
char *ParseWord(char *q, char *p, int n)
{                               /* word copy, across lines */
  /* increment ptr past non character input, like spaces and line feeds, bells. */
  while(*p) {
    if(*p <= 32)
      p++;
    else
      break;
  }
  /* copy p to q stopping when we hit a space, or run out of 
   * space (memory) according to the limit n */
  while(*p) {
    if(*p <= 32)
      break;
    if(!n)
      break;
    *(q++) = *(p++);
    n--;
  }
  *q = 0;
  return p;
}


/*========================================================================*/
char *ParseNTrim(char *q, char *p, int n)
{                               /* n character trimmed copy */
  char *q_orig = q;
  while(*p) {
    if((*p == 0xD) || (*p == 0xA))      /* don't skip end of lines */
      break;
    if(*p <= 32) {
      p++;
      n--;
    } else
      break;
  }
  while(*p) {
    if(!n)
      break;
    if((*p == 0xD) || (*p == 0xA))      /* don't copy end of lines */
      break;
    *(q++) = *(p++);
    n--;
  }
  while(q > q_orig) {
    if(*(q - 1) <= 32)
      q--;
    else
      break;
  }
  *q = 0;
  return p;
}


/*========================================================================*/
char *ParseNTrimRight(char *q, char *p, int n)
{                               /* n character trimmed copy */
  char *q_orig = q;
  while(*p) {
    if(!n)
      break;
    if((*p == 0xD) || (*p == 0xA))      /* don't copy end of lines */
      break;
    *(q++) = *(p++);
    n--;
  }
  while(q > q_orig) {
    if(*(q - 1) <= 32)
      q--;
    else
      break;
  }
  *q = 0;
  return p;
}


/*========================================================================*/
char *ParseCommaCopy(char *q, char *p, int n)
{                               /* n character copy up to comma */
  while(*p) {
    if(!n)
      break;
    if((*p == 0xD) || (*p == 0xA))      /* don't copy end of lines */
      break;
    if(*p == ',')
      break;
    *(q++) = *(p++);
    n--;
  }
  *q = 0;
  return p;
}


/*========================================================================*/
char *ParseNSkip(char *p, int n)
{                               /* n character skip */
  while(*p) {
    if(!n)
      break;
    if((*p == 0xD) || (*p == 0xA))      /* stop at newlines */
      break;
    p++;
    n--;
  }
  return p;
}
