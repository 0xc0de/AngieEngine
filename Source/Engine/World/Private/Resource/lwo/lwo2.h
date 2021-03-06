/*

Angie Engine Source Code

MIT License

Copyright (C) 2017-2021 Alexander Samusev.

This file is part of the Angie Engine Source Code.

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.

*/

#pragma once

#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct st_lwFile {
    size_t (*Read)( void * _Buffer, size_t _ElementSize, size_t _ElementCount, struct st_lwFile * _Stream );
    int (*Seek)( struct st_lwFile * _Stream, long _Offset, int _Origin );
    long (*Tell)( struct st_lwFile * _Stream );
    int (*Getc)( struct st_lwFile * _Stream );
    void *(*Alloc)( void * _Allocator, size_t _Size );
    void (*Free)( void * _Allocator, void * _Bytes );
    void * UserData;
    void * Allocator;
    int error;
} lwFile;

/*
======================================================================
lwo2.h

Definitions and typedefs for LWO2 files.

Ernie Wright  17 Sep 00
====================================================================== */

#ifndef LWO2_H
#define LWO2_H

/* chunk and subchunk IDs */

#define LWID_(a,b,c,d) (((a)<<24)|((b)<<16)|((c)<<8)|(d))

#define ID_FORM  LWID_('F','O','R','M')
#define ID_LWO2  LWID_('L','W','O','2')
#define ID_LWOB  LWID_('L','W','O','B')

/* top-level chunks */
#define ID_LAYR  LWID_('L','A','Y','R')
#define ID_TAGS  LWID_('T','A','G','S')
#define ID_PNTS  LWID_('P','N','T','S')
#define ID_BBOX  LWID_('B','B','O','X')
#define ID_VMAP  LWID_('V','M','A','P')
#define ID_VMAD  LWID_('V','M','A','D')
#define ID_POLS  LWID_('P','O','L','S')
#define ID_PTAG  LWID_('P','T','A','G')
#define ID_ENVL  LWID_('E','N','V','L')
#define ID_CLIP  LWID_('C','L','I','P')
#define ID_SURF  LWID_('S','U','R','F')
#define ID_DESC  LWID_('D','E','S','C')
#define ID_TEXT  LWID_('T','E','X','T')
#define ID_ICON  LWID_('I','C','O','N')

/* polygon types */
#define ID_FACE  LWID_('F','A','C','E')
#define ID_CURV  LWID_('C','U','R','V')
#define ID_PTCH  LWID_('P','T','C','H')
#define ID_MBAL  LWID_('M','B','A','L')
#define ID_BONE  LWID_('B','O','N','E')

/* polygon tags */
#define ID_SURF  LWID_('S','U','R','F')
#define ID_PART  LWID_('P','A','R','T')
#define ID_SMGP  LWID_('S','M','G','P')

/* envelopes */
#define ID_PRE   LWID_('P','R','E',' ')
#define ID_POST  LWID_('P','O','S','T')
#define ID_KEY   LWID_('K','E','Y',' ')
#define ID_SPAN  LWID_('S','P','A','N')
#define ID_TCB   LWID_('T','C','B',' ')
#define ID_HERM  LWID_('H','E','R','M')
#define ID_BEZI  LWID_('B','E','Z','I')
#define ID_BEZ2  LWID_('B','E','Z','2')
#define ID_LINE  LWID_('L','I','N','E')
#define ID_STEP  LWID_('S','T','E','P')

/* clips */
#define ID_STIL  LWID_('S','T','I','L')
#define ID_ISEQ  LWID_('I','S','E','Q')
#define ID_ANIM  LWID_('A','N','I','M')
#define ID_XREF  LWID_('X','R','E','F')
#define ID_STCC  LWID_('S','T','C','C')
#define ID_TIME  LWID_('T','I','M','E')
#define ID_CONT  LWID_('C','O','N','T')
#define ID_BRIT  LWID_('B','R','I','T')
#define ID_SATR  LWID_('S','A','T','R')
#define ID_HUE   LWID_('H','U','E',' ')
#define ID_GAMM  LWID_('G','A','M','M')
#define ID_NEGA  LWID_('N','E','G','A')
#define ID_IFLT  LWID_('I','F','L','T')
#define ID_PFLT  LWID_('P','F','L','T')

/* surfaces */
#define ID_COLR  LWID_('C','O','L','R')
#define ID_LUMI  LWID_('L','U','M','I')
#define ID_DIFF  LWID_('D','I','F','F')
#define ID_SPEC  LWID_('S','P','E','C')
#define ID_GLOS  LWID_('G','L','O','S')
#define ID_REFL  LWID_('R','E','F','L')
#define ID_RFOP  LWID_('R','F','O','P')
#define ID_RIMG  LWID_('R','I','M','G')
#define ID_RSAN  LWID_('R','S','A','N')
#define ID_TRAN  LWID_('T','R','A','N')
#define ID_TROP  LWID_('T','R','O','P')
#define ID_TIMG  LWID_('T','I','M','G')
#define ID_RIND  LWID_('R','I','N','D')
#define ID_TRNL  LWID_('T','R','N','L')
#define ID_BUMP  LWID_('B','U','M','P')
#define ID_SMAN  LWID_('S','M','A','N')
#define ID_SIDE  LWID_('S','I','D','E')
#define ID_CLRH  LWID_('C','L','R','H')
#define ID_CLRF  LWID_('C','L','R','F')
#define ID_ADTR  LWID_('A','D','T','R')
#define ID_SHRP  LWID_('S','H','R','P')
#define ID_LINE  LWID_('L','I','N','E')
#define ID_LSIZ  LWID_('L','S','I','Z')
#define ID_ALPH  LWID_('A','L','P','H')
#define ID_AVAL  LWID_('A','V','A','L')
#define ID_GVAL  LWID_('G','V','A','L')
#define ID_BLOK  LWID_('B','L','O','K')

/* texture layer */
#define ID_TYPE  LWID_('T','Y','P','E')
#define ID_CHAN  LWID_('C','H','A','N')
#define ID_NAME  LWID_('N','A','M','E')
#define ID_ENAB  LWID_('E','N','A','B')
#define ID_OPAC  LWID_('O','P','A','C')
#define ID_FLAG  LWID_('F','L','A','G')
#define ID_PROJ  LWID_('P','R','O','J')
#define ID_STCK  LWID_('S','T','C','K')
#define ID_TAMP  LWID_('T','A','M','P')

/* texture coordinates */
#define ID_TMAP  LWID_('T','M','A','P')
#define ID_AXIS  LWID_('A','X','I','S')
#define ID_CNTR  LWID_('C','N','T','R')
#define ID_SIZE  LWID_('S','I','Z','E')
#define ID_ROTA  LWID_('R','O','T','A')
#define ID_OREF  LWID_('O','R','E','F')
#define ID_FALL  LWID_('F','A','L','L')
#define ID_CSYS  LWID_('C','S','Y','S')

/* image map */
#define ID_IMAP  LWID_('I','M','A','P')
#define ID_IMAG  LWID_('I','M','A','G')
#define ID_WRAP  LWID_('W','R','A','P')
#define ID_WRPW  LWID_('W','R','P','W')
#define ID_WRPH  LWID_('W','R','P','H')
#define ID_VMAP  LWID_('V','M','A','P')
#define ID_AAST  LWID_('A','A','S','T')
#define ID_PIXB  LWID_('P','I','X','B')

/* procedural */
#define ID_PROC  LWID_('P','R','O','C')
#define ID_COLR  LWID_('C','O','L','R')
#define ID_VALU  LWID_('V','A','L','U')
#define ID_FUNC  LWID_('F','U','N','C')
#define ID_FTPS  LWID_('F','T','P','S')
#define ID_ITPS  LWID_('I','T','P','S')
#define ID_ETPS  LWID_('E','T','P','S')

/* gradient */
#define ID_GRAD  LWID_('G','R','A','D')
#define ID_GRST  LWID_('G','R','S','T')
#define ID_GREN  LWID_('G','R','E','N')
#define ID_PNAM  LWID_('P','N','A','M')
#define ID_INAM  LWID_('I','N','A','M')
#define ID_GRPT  LWID_('G','R','P','T')
#define ID_FKEY  LWID_('F','K','E','Y')
#define ID_IKEY  LWID_('I','K','E','Y')

/* shader */
#define ID_SHDR  LWID_('S','H','D','R')
#define ID_DATA  LWID_('D','A','T','A')


/* generic linked list */

typedef struct st_lwNode {
    struct st_lwNode *next, *prev;
    void *data;
} lwNode;


/* plug-in reference */

typedef struct st_lwPlugin {
    struct st_lwPlugin *next, *prev;
    char          *ord;
    char          *name;
    int            flags;
    void          *data;
} lwPlugin;


/* envelopes */

typedef struct st_lwKey {
    struct st_lwKey *next, *prev;
    float          value;
    float          time;
    unsigned int   shape;               /* ID_TCB, ID_BEZ2, etc. */
    float          tension;
    float          continuity;
    float          bias;
    float          param[4];
} lwKey;

typedef struct st_lwEnvelope {
    struct st_lwEnvelope *next, *prev;
    int            index;
    int            type;
    char          *name;
    lwKey         *key;                 /* linked list of keys */
    int            nkeys;
    int            behavior[2];       /* pre and post (extrapolation) */
    lwPlugin      *cfilter;             /* linked list of channel filters */
    int            ncfilters;
} lwEnvelope;

#define BEH_RESET      0
#define BEH_CONSTANT   1
#define BEH_REPEAT     2
#define BEH_OSCILLATE  3
#define BEH_OFFSET     4
#define BEH_LINEAR     5


/* values that can be enveloped */

typedef struct st_lwEParam {
    float          val;
    int            eindex;
} lwEParam;

typedef struct st_lwVParam {
    float          val[3];
    int            eindex;
} lwVParam;


/* clips */

typedef struct st_lwClipStill {
    char          *name;
} lwClipStill;

typedef struct st_lwClipSeq {
    char          *prefix;              /* filename before sequence digits */
    char          *suffix;              /* after digits, e.g. extensions */
    int            digits;
    int            flags;
    int            offset;
    int            start;
    int            end;
} lwClipSeq;

typedef struct st_lwClipAnim {
    char          *name;
    char          *server;              /* anim loader plug-in */
    void          *data;
} lwClipAnim;

typedef struct st_lwClipXRef {
    char          *string;
    int            index;
    struct st_lwClip *clip;
} lwClipXRef;

typedef struct st_lwClipCycle {
    char          *name;
    int            lo;
    int            hi;
} lwClipCycle;

typedef struct st_lwClip {
    struct st_lwClip *next, *prev;
    int            index;
    unsigned int   type;                /* ID_STIL, ID_ISEQ, etc. */
    union {
        lwClipStill    still;
        lwClipSeq      seq;
        lwClipAnim     anim;
        lwClipXRef     xref;
        lwClipCycle    cycle;
    }              source;
    float          start_time;
    float          duration;
    float          frame_rate;
    lwEParam       contrast;
    lwEParam       brightness;
    lwEParam       saturation;
    lwEParam       hue;
    lwEParam       gamma;
    int            negative;
    lwPlugin      *ifilter;             /* linked list of image filters */
    int            nifilters;
    lwPlugin      *pfilter;             /* linked list of pixel filters */
    int            npfilters;
} lwClip;


/* textures */

typedef struct st_lwTMap {
    lwVParam       size;
    lwVParam       center;
    lwVParam       rotate;
    lwVParam       falloff;
    int            fall_type;
    char          *ref_object;
    int            coord_sys;
} lwTMap;

typedef struct st_lwImageMap {
    int            cindex;
    int            projection;
    char          *vmap_name;
    int            axis;
    int            wrapw_type;
    int            wraph_type;
    lwEParam       wrapw;
    lwEParam       wraph;
    float          aa_strength;
    int            aas_flags;
    int            pblend;
    lwEParam       stck;
    lwEParam       amplitude;
} lwImageMap;

#define PROJ_PLANAR       0
#define PROJ_CYLINDRICAL  1
#define PROJ_SPHERICAL    2
#define PROJ_CUBIC        3
#define PROJ_FRONT        4

#define WRAP_NONE    0
#define WRAP_EDGE    1
#define WRAP_REPEAT  2
#define WRAP_MIRROR  3

typedef struct st_lwProcedural {
    int            axis;
    float          value[3];
    char          *name;
    void          *data;
} lwProcedural;

typedef struct st_lwGradKey {
    struct st_lwGradKey *next, *prev;
    float          value;
    float          rgba[4];
} lwGradKey;

typedef struct st_lwGradient {
    char          *paramname;
    char          *itemname;
    float          start;
    float          end;
    int            repeat;
    lwGradKey     *key;                 /* array of gradient keys */
    short         *ikey;                /* array of interpolation codes */
} lwGradient;

typedef struct st_lwTexture {
    struct st_lwTexture *next, *prev;
    char          *ord;
    unsigned int   type;
    unsigned int   chan;
    lwEParam       opacity;
    short          opac_type;
    short          enabled;
    short          negative;
    short          axis;
    union {
        lwImageMap     imap;
        lwProcedural   proc;
        lwGradient     grad;
    }              param;
    lwTMap         tmap;
} lwTexture;


/* values that can be textured */

typedef struct st_lwTParam {
    float          val;
    int            eindex;
    lwTexture     *tex;                 /* linked list of texture layers */
} lwTParam;

typedef struct st_lwCParam {
    float          rgb[3];
    int            eindex;
    lwTexture     *tex;                 /* linked list of texture layers */
} lwCParam;


/* surfaces */

typedef struct st_lwGlow {
    short          enabled;
    short          type;
    lwEParam       intensity;
    lwEParam       size;
} Glow;

typedef struct st_lwRMap {
    lwTParam       val;
    int            options;
    int            cindex;
    float          seam_angle;
} lwRMap;

typedef struct st_lwLine {
    short          enabled;
    unsigned short flags;
    lwEParam       size;
} lwLine;

typedef struct st_lwSurface {
    struct st_lwSurface *next, *prev;
    char          *name;
    char          *srcname;
    lwCParam       color;
    lwTParam       luminosity;
    lwTParam       diffuse;
    lwTParam       specularity;
    lwTParam       glossiness;
    lwRMap         reflection;
    lwRMap         transparency;
    lwTParam       eta;
    lwTParam       translucency;
    lwTParam       bump;
    float          smooth;
    int            sideflags;
    float          alpha;
    int            alpha_mode;
    lwEParam       color_hilite;
    lwEParam       color_filter;
    lwEParam       add_trans;
    lwEParam       dif_sharp;
    lwEParam       glow;
    lwLine         line;
    lwPlugin      *shader;              /* linked list of shaders */
    int            nshaders;
} lwSurface;


/* vertex maps */

typedef struct st_lwVMap {
    struct st_lwVMap *next, *prev;
    char          *name;
    unsigned int   type;
    int            dim;
    int            nverts;
    int            perpoly;
    int           *vindex;              /* array of point indexes */
    int           *pindex;              /* array of polygon indexes */
    float        **val;
    int            offset;              /* 0xc0de: added for user features */
} lwVMap;

typedef struct st_lwVMapPt {
    lwVMap        *vmap;
    int            index;               /* vindex or pindex element */
} lwVMapPt;


/* points and polygons */

typedef struct st_lwPoint {
    float          pos[3];
    int            npols;               /* number of polygons sharing the point */
    int           *pol;                 /* array of polygon indexes */
    int            nvmaps;
    lwVMapPt      *vm;                  /* array of vmap references */
} lwPoint;

typedef struct st_lwPolVert {
    int            index;               /* index into the point array */
    float          norm[3];
    int            nvmaps;
    lwVMapPt      *vm;                  /* array of vmap references */
} lwPolVert;

typedef struct st_lwPolygon {
    lwSurface     *surf;
    int            part;                /* part index */
    int            smoothgrp;           /* smoothing group */
    int            flags;
    unsigned int   type;
    float          norm[3];
    int            nverts;
    lwPolVert     *v;                   /* array of vertex records */
} lwPolygon;

typedef struct st_lwPointList {
    int            count;
    int            offset;              /* only used during reading */
    lwPoint       *pt;                  /* array of points */
} lwPointList;

typedef struct st_lwPolygonList {
    int            count;
    int            offset;              /* only used during reading */
    int            vcount;              /* total number of vertices */
    int            voffset;             /* only used during reading */
    lwPolygon     *pol;                 /* array of polygons */
} lwPolygonList;


/* geometry layers */

typedef struct st_lwLayer {
    struct st_lwLayer *next, *prev;
    char          *name;
    int            index;
    int            parent;
    int            flags;
    float          pivot[3];
    float          bbox[6];
    lwPointList    point;
    lwPolygonList  polygon;
    int            nvmaps;
    lwVMap        *vmap;                /* linked list of vmaps */
} lwLayer;


/* tag strings */

typedef struct st_lwTagList {
    int            count;
    int            offset;              /* only used during reading */
    char         **tag;                 /* array of strings */
} lwTagList;


/* an object */

typedef struct st_lwObject {
    lwLayer       *layer;               /* linked list of layers */
    lwEnvelope    *env;                 /* linked list of envelopes */
    lwClip        *clip;                /* linked list of clips */
    lwSurface     *surf;                /* linked list of surfaces */
    lwTagList      taglist;
    int            nlayers;
    int            nenvs;
    int            nclips;
    int            nsurfs;
} lwObject;


/* lwo2.c */

void lwFreeLayer( lwFile *fp, lwLayer *layer );
void lwFreeObject( lwFile *fp, lwObject *object );
lwObject *lwGetObject( lwFile *fp, unsigned int *failID, int *failpos );

/* pntspols.c */

void lwFreePoints( lwFile *fp, lwPointList *point );
void lwFreePolygons( lwFile *fp, lwPolygonList *plist );
int lwGetPoints( lwFile *fp, int cksize, lwPointList *point );
void lwGetBoundingBox( lwPointList *point, float bbox[] );
int lwAllocPolygons( lwFile * fp, lwPolygonList *plist, int npols, int nverts );
int lwGetPolygons( lwFile *fp, int cksize, lwPolygonList *plist, int ptoffset );
void lwGetPolyNormals( lwPointList *point, lwPolygonList *polygon );
int lwGetPointPolygons( lwFile * fp, lwPointList *point, lwPolygonList *polygon );
int lwResolvePolySurfaces( lwFile * fp, lwPolygonList *polygon, lwTagList *tlist,
                           lwSurface **surf, int *nsurfs );
void lwGetVertNormals( lwPointList *point, lwPolygonList *polygon );
void lwFreeTags( lwFile * fp, lwTagList *tlist );
int lwGetTags( lwFile *fp, int cksize, lwTagList *tlist );
int lwGetPolygonTags( lwFile *fp, int cksize, lwTagList *tlist,
                      lwPolygonList *plist );

/* vmap.c */

void lwFreeVMap( lwFile *fp, lwVMap *vmap );
lwVMap *lwGetVMap( lwFile *fp, int cksize, int ptoffset, int poloffset,
                   int perpoly );
int lwGetPointVMaps( lwFile * fp, lwPointList *point, lwVMap *vmap );
int lwGetPolyVMaps( lwFile * fp, lwPolygonList *polygon, lwVMap *vmap );

/* clip.c */

void lwFreeClip( lwFile *fp, lwClip *clip );
lwClip *lwGetClip( lwFile *fp, int cksize );
lwClip *lwFindClip( lwClip *list, int index );

/* envelope.c */

void lwFreeEnvelope( lwFile *fp, lwEnvelope *env );
lwEnvelope *lwGetEnvelope( lwFile *fp, int cksize );
lwEnvelope *lwFindEnvelope( lwEnvelope *list, int index );

/* surface.c */

void lwFreePlugin( lwFile *fp, lwPlugin *p );
void lwFreeTexture( lwFile *fp, lwTexture *t );
void lwFreeSurface( lwFile *fp, lwSurface *surf );
int lwGetTHeader( lwFile *fp, int hsz, lwTexture *tex );
int lwGetTMap( lwFile *fp, int tmapsz, lwTMap *tmap );
int lwGetImageMap( lwFile *fp, int rsz, lwTexture *tex );
int lwGetProcedural( lwFile *fp, int rsz, lwTexture *tex );
int lwGetGradient( lwFile *fp, int rsz, lwTexture *tex );
lwTexture *lwGetTexture( lwFile *fp, int bloksz, unsigned int type );
lwPlugin *lwGetShader( lwFile *fp, int bloksz );
lwSurface *lwGetSurface( lwFile *fp, int cksize );
lwSurface *lwDefaultSurface( lwFile * fp );

/* lwob.c */

lwSurface *lwGetSurface5( lwFile *fp, int cksize, lwObject *obj );
int lwGetPolygons5( lwFile *fp, int cksize, lwPolygonList *plist, int ptoffset );
lwObject *lwGetObject5( lwFile *fp, unsigned int *failID, int *failpos );

/* list.c */

void lwListFree( lwFile *fp, void *list, void ( *freeNode )(lwFile *fp, void *) );
void lwListAdd( void **list, void *node );
void lwListInsert( void **vlist, void *vitem,
                   int ( *compare )(void *, void *) );

/* vecmath.c */

float dot( float a[], float b[] );
void cross( float a[], float b[], float c[] );
void normalize( float v[] );
#define vecangle( a, b ) ( float ) acos( dot( a, b ))

/* lwio.c */

void  set_flen( int i );
int   get_flen( void );
void *getbytes( lwFile *fp, int size );
void  skipbytes( lwFile *fp, int n );
int   getI1( lwFile *fp );
short getI2( lwFile *fp );
int   getI4( lwFile *fp );
unsigned char  getU1( lwFile *fp );
unsigned short getU2( lwFile *fp );
unsigned int   getU4( lwFile *fp );
int   getVX( lwFile *fp );
float getF4( lwFile *fp );
char *getS0( lwFile *fp );
int   sgetI1( unsigned char **bp );
short sgetI2( unsigned char **bp );
int   sgetI4( unsigned char **bp );
unsigned char  sgetU1( unsigned char **bp );
unsigned short sgetU2( unsigned char **bp );
unsigned int   sgetU4( unsigned char **bp );
int   sgetVX( unsigned char **bp );
float sgetF4( unsigned char **bp );
char *sgetS0( lwFile *fp,unsigned char **bp );

#ifdef _WIN32
void revbytes( void *bp, int elsize, int elcount );
#else
#define revbytes( b, s, c )
#endif

#endif

#ifdef __cplusplus
}
#endif
