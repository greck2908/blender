/*
 * ***** BEGIN GPL LICENSE BLOCK *****
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * The Original Code is Copyright (C) Blender Foundation
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): Lukas Toenne
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file DNA_groom_types.h
 *  \ingroup DNA
 */

#ifndef __DNA_GROOM_TYPES_H__
#define __DNA_GROOM_TYPES_H__

#include "DNA_defs.h"
#include "DNA_listBase.h"
#include "DNA_ID.h"
#include "DNA_meshdata_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Vertex in a closed curve for a bundle section */
typedef struct GroomSectionVertex
{
	int flag;
	float co[2];                            /* Location in the section plane */
} GroomSectionVertex;

typedef enum GroomVertexFlag
{
	GM_VERTEX_SELECT        = (1 << 0),
} GroomVertexFlag;

/* Cross-section of a bundle */
typedef struct GroomSection {
	int flag;
	int pad;
	
	float center[3];                        /* Center point */
	
	float mat[3][3];                        /* Local coordinate frame */
} GroomSection;

typedef enum GroomSectionFlag
{
	GM_SECTION_SELECT       = (1 << 0),
} GroomSectionFlag;

/* Single interpolated step along a groom curve */
typedef struct GroomCurveCache
{
	float co[3];                            /* Location vector */
} GroomCurveCache;

/* Data for generating hair guide curves */
typedef struct GroomHairGuide
{
	struct MeshSample root;                 /* Root point */
} GroomHairGuide;

/* Bundle of hair strands following the same curve path */
typedef struct GroomBundle {
	int guides_count;                       /* Number of guides to generate (totguides can be smaller) */
	
	int totsections;                        /* Number of sections along the curve */
	int totverts;                           /* Number of vertices of all sections combined */
	int curvesize;                          /* Number of vertices in a curve = (totsections - 1) * groom.curve_res + 1 */
	int totcurvecache;                      /* Number of cached curve steps = curve_size * (numshapeverts + 1) */
	int totguides;                          /* Actual number of hair guide curves */
	
	struct GroomSection *sections;          /* List of sections [totsections] */
	struct GroomSectionVertex *verts;       /* List of vertices [totsections][numshapeverts] */
	struct GroomCurveCache *curvecache;     /* Cached curve steps [numshapeverts + 1][curve_size], last is center curve */
	
	struct GroomHairGuide *guides;          /* Data for generating hair guide curves [totguides] */
	float *guide_shape_weights;             /* Weights for interpolating hair guide curves [totguides][numshapeverts] */
} GroomBundle;

/* Region on the scalp that generates hair guide curves */
typedef struct GroomRegion
{
	struct GroomRegion *next, *prev;        /* Pointers for ListBase element */
	
	int flag;
	
	int numverts;                           /* Number of vertices that make up the region */
	
	struct MeshSample *scalp_samples;       /* Mesh samples bind to a scalp region [numverts + 1], last is center position */
	
	/* XXX Face maps are used temporarily for creating regions,
	 * eventually should be replaced by a fully fledged 2D loop mesh */
	char scalp_facemap_name[64];            /* Scalp face map to use as region, MAX_VGROUP_NAME */
	
	GroomBundle bundle;                     /* Curve with sections for creating hair bundle */
	
	/* Shape parameters */
	float taper_length;                     /* Distance at which final thickness is reached */
	float taper_thickness;                  /* Relative thickness of the strand */
} GroomRegion;

typedef enum GroomRegionFlag
{
	GM_REGION_SELECT        = (1 << 0),
} GroomRegionFlag;

/* Editable groom data */
typedef struct EditGroom {
	ListBase regions;           /* List of GroomBundle */
} EditGroom;

/* Groom curves for creating hair styles */
typedef struct Groom {
	ID id;                      /* Groom data is a datablock */
	struct AnimData *adt;       /* Animation data - for animating settings */
	
	int curve_res;              /* Curve resolution */
	int pad;
	
	ListBase regions;           /* List of GroomRegion */
	int active_region;          /* Index of active region in regions list */
	int pad2;
	
	struct HairSystem *hair_system;                 /* Renderable hair geometry */
	struct HairDrawSettings *hair_draw_settings;    /* Draw settings for hair geometry */
	
	struct Object *scalp_object;                    /* Surface for attaching hairs */
	
	struct Material **mat;      /* Material slots */
	short totcol;               /* Number of material slots */
	short pad3;
	
	/* Material used for drawing and rendering hair fibers
	 * TODO make this a per-region setting (requires separate shader group for each region)
	 */
	int material_index;
	
	struct BoundBox *bb;
	
	EditGroom *editgroom;
	void *batch_cache;
} Groom;

#ifdef __cplusplus
}
#endif

#endif /* __DNA_GROOM_TYPES_H__ */