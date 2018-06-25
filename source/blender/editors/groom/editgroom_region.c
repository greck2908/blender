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

/** \file blender/editors/groom/editgroom_region.c
 *  \ingroup edgroom
 */

#include "DNA_groom_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"

#include "MEM_guardedalloc.h"

#include "BLI_blenlib.h"
#include "BLI_math.h"

#include "BLT_translation.h"

#include "BKE_context.h"
#include "BKE_groom.h"
#include "BKE_library.h"
#include "BKE_object_facemap.h"

#include "DEG_depsgraph.h"

#include "RNA_access.h"
#include "RNA_define.h"
#include "RNA_enum_types.h"

#include "WM_api.h"
#include "WM_types.h"

#include "ED_groom.h"
#include "ED_object.h"
#include "ED_screen.h"
#include "ED_transform.h"
#include "ED_util.h"
#include "ED_view3d.h"

#include "UI_resources.h"
#include "UI_interface.h"

#include "groom_intern.h"

/* GROOM_OT_region_add */

static void region_add_set_bundle_curve(GroomRegion *region, const float loc[3], const float rot[3][3], float length)
{
	GroomBundle *bundle = &region->bundle;
	
	bundle->totsections = 2;
	bundle->sections = MEM_callocN(sizeof(GroomSection) * bundle->totsections, "groom bundle sections");
	
	madd_v3_v3v3fl(bundle->sections[0].center, loc, rot[2], 0.0f);
	madd_v3_v3v3fl(bundle->sections[1].center, loc, rot[2], length);
}

static int region_add_poll(bContext *C)
{
	if (!ED_groom_object_poll(C))
	{
		return false;
	}
	
	/* We want a scalp object to make this useful */
	Object *ob = ED_object_context(C);
	Groom *groom = ob->data;
	return groom->scalp_object != NULL;
}


static const EnumPropertyItem *region_add_facemap_itemf(bContext *C, PointerRNA *UNUSED(ptr), PropertyRNA *UNUSED(prop), bool *r_free)
{
	if (C == NULL) {
		return DummyRNA_NULL_items;
	}
	Object *ob = ED_object_context(C);
	if (!ob || ob->type != OB_GROOM)
	{
		return DummyRNA_NULL_items;
	}
	Groom *groom = ob->data;
	Object *scalp_ob = groom->scalp_object;
	if (!scalp_ob)
	{
		return DummyRNA_NULL_items;
	}

	EnumPropertyItem *item = NULL, item_tmp = {0};
	int totitem = 0;

	int i = 0;
	for (bFaceMap *fmap = scalp_ob->fmaps.first; fmap; fmap = fmap->next, ++i)
	{
		item_tmp.identifier = fmap->name;
		item_tmp.name = fmap->name;
		item_tmp.description = "";
		item_tmp.value = i;
		RNA_enum_item_add(&item, &totitem, &item_tmp);
	}

	RNA_enum_item_end(&item, &totitem);
	*r_free = true;

	return item;
}

static int region_add_exec(bContext *C, wmOperator *op)
{
	const Depsgraph *depsgraph = CTX_data_depsgraph(C);
	Object *ob = ED_object_context(C);
	Groom *groom = ob->data;
	if (!groom->scalp_object)
	{
		return OPERATOR_CANCELLED;
	}

	char scalp_facemap_name[MAX_VGROUP_NAME];
	int fmap_index = RNA_enum_get(op->ptr, "scalp_facemap");
	bFaceMap *fmap = BLI_findlink(&groom->scalp_object->fmaps, fmap_index);
	if (!fmap)
	{
		return OPERATOR_CANCELLED;
	}
	BLI_strncpy(scalp_facemap_name, fmap->name, sizeof(scalp_facemap_name));

	GroomRegion *region = BKE_groom_region_add(groom);

	float scalp_loc[3];
	float scalp_rot[3][3];
	zero_v3(scalp_loc);
	unit_m3(scalp_rot);
	
	if (BKE_groom_set_region_scalp_facemap(groom, region, scalp_facemap_name))
	{
		const struct Mesh *scalp = BKE_groom_get_scalp(depsgraph, groom);
		BLI_assert(scalp != NULL);
		
		if (BKE_groom_region_bind(depsgraph, groom, region, true))
		{
			BKE_groom_calc_region_transform_on_scalp(region, scalp, scalp_loc, scalp_rot);
		}
	}
	
	region_add_set_bundle_curve(region, scalp_loc, scalp_rot, 1.0f);
	BKE_groom_region_reset_shape(depsgraph, groom, region);
	
	WM_event_add_notifier(C, NC_OBJECT | ND_DRAW, ob);
	DEG_id_tag_update(&ob->id, OB_RECALC_DATA);

	return OPERATOR_FINISHED;
}

void GROOM_OT_region_add(wmOperatorType *ot)
{
	PropertyRNA *prop;

	/* identifiers */
	ot->name = "Add Region";
	ot->description = "Add a new region to the groom object";
	ot->idname = "GROOM_OT_region_add";

	/* api callbacks */
	ot->exec = region_add_exec;
	ot->poll = region_add_poll;
	ot->invoke = WM_enum_search_invoke;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

	prop = RNA_def_enum(ot->srna, "scalp_facemap", DummyRNA_NULL_items, 0, "Scalp Facemap", "Facemap to which to bind the new region");
	RNA_def_enum_funcs(prop, region_add_facemap_itemf);
	RNA_def_property_flag(prop, PROP_ENUM_NO_TRANSLATE);
	ot->prop = prop;
}

/* GROOM_OT_region_remove */

static int region_remove_exec(bContext *C, wmOperator *UNUSED(op))
{
	Object *ob = ED_object_context(C);
	Groom *groom = ob->data;

	ListBase *regions = (groom->editgroom ? &groom->editgroom->regions : &groom->regions);
	GroomRegion *region = CTX_data_pointer_get_type(C, "groom_region", &RNA_GroomRegion).data;
	if (region == NULL)
	{
		region = BLI_findlink(regions, groom->active_region);
		if (region == NULL)
		{
			return OPERATOR_CANCELLED;
		}
	}
	
	BKE_groom_region_remove(groom, region);
	
	WM_event_add_notifier(C, NC_OBJECT | ND_DRAW, ob);
	DEG_id_tag_update(&ob->id, OB_RECALC_DATA);
	
	return OPERATOR_FINISHED;
}

void GROOM_OT_region_remove(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Remove Region";
	ot->description = "Remove a region from the groom object";
	ot->idname = "GROOM_OT_region_remove";

	/* api callbacks */
	ot->exec = region_remove_exec;
	ot->poll = ED_groom_object_poll;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/* GROOM_OT_region_bind */

static int region_bind_poll(bContext *C)
{
	if (!ED_operator_scene_editable(C))
	{
		return 0;
	}
	
	Object *ob = ED_object_context(C);
	Groom *groom = ob->data;
	if (groom->editgroom)
	{
		return 0;
	}
	
	return 1;
}

static int region_bind_exec(bContext *C, wmOperator *op)
{
	const Depsgraph *depsgraph = CTX_data_depsgraph(C);
	Object *ob = ED_object_context(C);
	Groom *groom = ob->data;
	const bool force_rebind = RNA_boolean_get(op->ptr, "force_rebind");

	GroomRegion *region = CTX_data_pointer_get_type(C, "groom_region", &RNA_GroomRegion).data;
	if (!region)
	{
		region = BLI_findlink(&groom->regions, groom->active_region);
		if (!region)
		{
			return OPERATOR_CANCELLED;
		}
	}

	BKE_groom_region_bind(depsgraph, groom, region, force_rebind);

	WM_event_add_notifier(C, NC_OBJECT | ND_DRAW, ob);
	DEG_id_tag_update(&ob->id, OB_RECALC_DATA);

	return OPERATOR_FINISHED;
}

void GROOM_OT_region_bind(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Bind Region";
	ot->description = "Bind a groom bundle to its scalp region";
	ot->idname = "GROOM_OT_region_bind";

	/* api callbacks */
	ot->exec = region_bind_exec;
	ot->poll = region_bind_poll;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

	RNA_def_boolean(ot->srna, "force_rebind", true, "Force Rebind",
	                "Force rebinding of the groom region even if a binding already exists");
}

/* GROOM_OT_extrude_bundle */

static void groom_bundle_extrude(const Depsgraph *depsgraph, const Groom *groom, GroomRegion *region)
{
	GroomBundle *bundle = &region->bundle;
	const int numverts = region->numverts;
	
	bundle->totsections += 1;
	bundle->sections = MEM_reallocN_id(bundle->sections, sizeof(GroomSection) * bundle->totsections, "groom bundle sections");
	
	GroomSection *new_section = &bundle->sections[bundle->totsections - 1];
	if (bundle->totsections > 1)
	{
		/* Initialize by copying from the last section */
		const GroomSection *prev_section = &bundle->sections[bundle->totsections - 2];
		memcpy(new_section, prev_section, sizeof(GroomSection));
		
		bundle->totverts += numverts;
		bundle->verts = MEM_reallocN_id(bundle->verts, sizeof(GroomSectionVertex) * bundle->totverts, "groom bundle vertices");
		
		GroomSectionVertex *new_verts = &bundle->verts[bundle->totverts - numverts];
		const GroomSectionVertex *prev_verts = &bundle->verts[bundle->totverts - 2*numverts];
		memcpy(new_verts, prev_verts, sizeof(GroomSectionVertex) * numverts);
	}
	else
	{
		const struct Mesh *scalp = BKE_groom_get_scalp(depsgraph, groom);
		BKE_groom_calc_region_transform_on_scalp(region, scalp, new_section->center, new_section->mat);
		
		BKE_groom_region_reset_shape(depsgraph, groom, region);
	}

	{
		/* Select the last section */
		GroomSection *section = region->bundle.sections;
		for (int i = 0; i < bundle->totsections - 1; ++i, ++section)
		{
			section->flag &= ~GM_SECTION_SELECT;
		}
		bundle->sections[bundle->totsections - 1].flag |= GM_SECTION_SELECT;
	}
}

static int groom_extrude_bundle_poll(bContext *C)
{
	if (!ED_operator_editgroom(C))
	{
		return false;
	}
	
	Scene *scene = CTX_data_scene(C);
	if (scene->toolsettings->groom_edit_settings.mode != GM_EDIT_MODE_CURVES)
	{
		return false;
	}
	
	return true;
}

static int groom_extrude_bundle_exec(bContext *C, wmOperator *UNUSED(op))
{
	const Depsgraph *depsgraph = CTX_data_depsgraph(C);
	Object *ob = ED_object_context(C);
	Groom *groom = ob->data;
	EditGroom *edit = groom->editgroom;
	
	for (GroomRegion *region = edit->regions.first; region; region = region->next)
	{
		if (region->flag & GM_REGION_SELECT)
		{
			groom_bundle_extrude(depsgraph, groom, region);
		}
	}
	
	WM_event_add_notifier(C, NC_OBJECT | ND_DRAW, ob);
	DEG_id_tag_update(&ob->id, OB_RECALC_DATA);

	return OPERATOR_FINISHED;
}

void GROOM_OT_extrude_bundle(struct wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Extrude Bundle";
	ot->idname = "GROOM_OT_extrude_bundle";
	ot->description = "Extrude hair bundle";

	/* api callbacks */
	ot->exec = groom_extrude_bundle_exec;
	ot->poll = groom_extrude_bundle_poll;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

	Transform_Properties(ot, P_NO_DEFAULTS);
}