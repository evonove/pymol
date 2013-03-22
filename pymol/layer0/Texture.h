

/* 
A* -------------------------------------------------------------------
B* This file contains source code for the PyMOL computer program
C* Copyright (c) Schrodinger, LLC. 
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
#ifndef _H_Texture
#define _H_Texture

#include "PyMOLGlobals.h"

int TextureInit(PyMOLGlobals * G);
void TextureFree(PyMOLGlobals * G);
int TextureGetFromChar(PyMOLGlobals * G, int char_id, float *extent);
void TextureInitTextTexture(PyMOLGlobals * G);
GLuint TextureGetTextTextureID(PyMOLGlobals * G);
void TextureGetPlacementForNewSubtexture(PyMOLGlobals * G, int new_texture_width, int new_texture_height, int *new_texture_posx, int *new_texture_posy);
int TextureGetTextTextureSize(PyMOLGlobals * G);

#endif
