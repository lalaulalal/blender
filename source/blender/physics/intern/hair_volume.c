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
 * Contributor(s): Janne Karhu, Lukas Toenne
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/blenkernel/intern/hair_volume.c
 *  \ingroup bph
 */

#include "BLI_math.h"
#include "BLI_utildefines.h"

#if 0 // XXX TODO

/* ================ Volumetric Hair Interaction ================
 * adapted from
 *      Volumetric Methods for Simulation and Rendering of Hair
 *      by Lena Petrovic, Mark Henne and John Anderson
 *      Pixar Technical Memo #06-08, Pixar Animation Studios
 */

/* Note about array indexing:
 * Generally the arrays here are one-dimensional.
 * The relation between 3D indices and the array offset is
 *   offset = x + res_x * y + res_y * z
 */

/* TODO: This is an initial implementation and should be made much better in due time.
 * What should at least be implemented is a grid size parameter and a smoothing kernel
 * for bigger grids.
 */

/* 10x10x10 grid gives nice initial results */
static const int hair_grid_res = 10;

static int hair_grid_size(int res)
{
	return res * res * res;
}

BLI_INLINE void hair_grid_get_scale(int res, const float gmin[3], const float gmax[3], float scale[3])
{
	sub_v3_v3v3(scale, gmax, gmin);
	mul_v3_fl(scale, 1.0f / (res-1));
}

typedef struct HairGridVert {
	float velocity[3];
	float density;
} HairGridVert;

#define HAIR_GRID_INDEX_AXIS(vec, res, gmin, scale, axis) ( min_ii( max_ii( (int)((vec[axis] - gmin[axis]) / scale[axis]), 0), res-2 ) )

BLI_INLINE int hair_grid_offset(const float vec[3], int res, const float gmin[3], const float scale[3])
{
	int i, j, k;
	i = HAIR_GRID_INDEX_AXIS(vec, res, gmin, scale, 0);
	j = HAIR_GRID_INDEX_AXIS(vec, res, gmin, scale, 1);
	k = HAIR_GRID_INDEX_AXIS(vec, res, gmin, scale, 2);
	return i + (j + k*res)*res;
}

BLI_INLINE int hair_grid_interp_weights(int res, const float gmin[3], const float scale[3], const float vec[3], float uvw[3])
{
	int i, j, k, offset;
	
	i = HAIR_GRID_INDEX_AXIS(vec, res, gmin, scale, 0);
	j = HAIR_GRID_INDEX_AXIS(vec, res, gmin, scale, 1);
	k = HAIR_GRID_INDEX_AXIS(vec, res, gmin, scale, 2);
	offset = i + (j + k*res)*res;
	
	uvw[0] = (vec[0] - gmin[0]) / scale[0] - (float)i;
	uvw[1] = (vec[1] - gmin[1]) / scale[1] - (float)j;
	uvw[2] = (vec[2] - gmin[2]) / scale[2] - (float)k;
	
	return offset;
}

BLI_INLINE void hair_grid_interpolate(const HairGridVert *grid, int res, const float gmin[3], const float scale[3], const float vec[3],
                                      float *density, float velocity[3], float density_gradient[3])
{
	HairGridVert data[8];
	float uvw[3], muvw[3];
	int res2 = res * res;
	int offset;
	
	offset = hair_grid_interp_weights(res, gmin, scale, vec, uvw);
	muvw[0] = 1.0f - uvw[0];
	muvw[1] = 1.0f - uvw[1];
	muvw[2] = 1.0f - uvw[2];
	
	data[0] = grid[offset           ];
	data[1] = grid[offset         +1];
	data[2] = grid[offset     +res  ];
	data[3] = grid[offset     +res+1];
	data[4] = grid[offset+res2      ];
	data[5] = grid[offset+res2    +1];
	data[6] = grid[offset+res2+res  ];
	data[7] = grid[offset+res2+res+1];
	
	if (density) {
		*density = muvw[2]*( muvw[1]*( muvw[0]*data[0].density + uvw[0]*data[1].density )   +
		                      uvw[1]*( muvw[0]*data[2].density + uvw[0]*data[3].density ) ) +
		            uvw[2]*( muvw[1]*( muvw[0]*data[4].density + uvw[0]*data[5].density )   +
		                      uvw[1]*( muvw[0]*data[6].density + uvw[0]*data[7].density ) );
	}
	if (velocity) {
		int k;
		for (k = 0; k < 3; ++k) {
			velocity[k] = muvw[2]*( muvw[1]*( muvw[0]*data[0].velocity[k] + uvw[0]*data[1].velocity[k] )   +
			                         uvw[1]*( muvw[0]*data[2].velocity[k] + uvw[0]*data[3].velocity[k] ) ) +
			               uvw[2]*( muvw[1]*( muvw[0]*data[4].velocity[k] + uvw[0]*data[5].velocity[k] )   +
			                         uvw[1]*( muvw[0]*data[6].velocity[k] + uvw[0]*data[7].velocity[k] ) );
		}
	}
	if (density_gradient) {
		density_gradient[0] = muvw[1] * muvw[2] * ( data[0].density - data[1].density ) +
		                       uvw[1] * muvw[2] * ( data[2].density - data[3].density ) +
		                      muvw[1] *  uvw[2] * ( data[4].density - data[5].density ) +
		                       uvw[1] *  uvw[2] * ( data[6].density - data[7].density );
		
		density_gradient[1] = muvw[2] * muvw[0] * ( data[0].density - data[2].density ) +
		                       uvw[2] * muvw[0] * ( data[4].density - data[6].density ) +
		                      muvw[2] *  uvw[0] * ( data[1].density - data[3].density ) +
		                       uvw[2] *  uvw[0] * ( data[5].density - data[7].density );
		
		density_gradient[2] = muvw[2] * muvw[0] * ( data[0].density - data[4].density ) +
		                       uvw[2] * muvw[0] * ( data[1].density - data[5].density ) +
		                      muvw[2] *  uvw[0] * ( data[2].density - data[6].density ) +
		                       uvw[2] *  uvw[0] * ( data[3].density - data[7].density );
	}
}

static void hair_velocity_smoothing(const HairGridVert *hairgrid, const float gmin[3], const float scale[3], float smoothfac,
                                    lfVector *lF, lfVector *lX, lfVector *lV, unsigned int numverts)
{
	int v;
	/* calculate forces */
	for (v = 0; v < numverts; v++) {
		float density, velocity[3];
		
		hair_grid_interpolate(hairgrid, hair_grid_res, gmin, scale, lX[v], &density, velocity, NULL);
		
		sub_v3_v3(velocity, lV[v]);
		madd_v3_v3fl(lF[v], velocity, smoothfac);
	}
}

static void hair_velocity_collision(const HairGridVert *collgrid, const float gmin[3], const float scale[3], float collfac,
                                    lfVector *lF, lfVector *lX, lfVector *lV, unsigned int numverts)
{
	int v;
	/* calculate forces */
	for (v = 0; v < numverts; v++) {
		int offset = hair_grid_offset(lX[v], hair_grid_res, gmin, scale);
		
		if (collgrid[offset].density > 0.0f) {
			lF[v][0] += collfac * (collgrid[offset].velocity[0] - lV[v][0]);
			lF[v][1] += collfac * (collgrid[offset].velocity[1] - lV[v][1]);
			lF[v][2] += collfac * (collgrid[offset].velocity[2] - lV[v][2]);
		}
	}
}

static void hair_pressure_force(const HairGridVert *hairgrid, const float gmin[3], const float scale[3], float pressurefac, float minpressure,
                                lfVector *lF, lfVector *lX, unsigned int numverts)
{
	int v;
	
	/* calculate forces */
	for (v = 0; v < numverts; v++) {
		float density, gradient[3], gradlen;
		
		hair_grid_interpolate(hairgrid, hair_grid_res, gmin, scale, lX[v], &density, NULL, gradient);
		
		gradlen = normalize_v3(gradient) - minpressure;
		if (gradlen < 0.0f)
			continue;
		mul_v3_fl(gradient, gradlen);
		
		madd_v3_v3fl(lF[v], gradient, pressurefac);
	}
}

static void hair_volume_get_boundbox(lfVector *lX, unsigned int numverts, float gmin[3], float gmax[3])
{
	int i;
	
	INIT_MINMAX(gmin, gmax);
	for (i = 0; i < numverts; i++)
		DO_MINMAX(lX[i], gmin, gmax);
}

BLI_INLINE bool hair_grid_point_valid(const float vec[3], float gmin[3], float gmax[3])
{
	return !(vec[0] < gmin[0] || vec[1] < gmin[1] || vec[2] < gmin[2] ||
	         vec[0] > gmax[0] || vec[1] > gmax[1] || vec[2] > gmax[2]);
}

BLI_INLINE float dist_tent_v3f3(const float a[3], float x, float y, float z)
{
	float w = (1.0f - fabsf(a[0] - x)) * (1.0f - fabsf(a[1] - y)) * (1.0f - fabsf(a[2] - z));
	return w;
}

/* returns the grid array offset as well to avoid redundant calculation */
static int hair_grid_weights(int res, const float gmin[3], const float scale[3], const float vec[3], float weights[8])
{
	int i, j, k, offset;
	float uvw[3];
	
	i = HAIR_GRID_INDEX_AXIS(vec, res, gmin, scale, 0);
	j = HAIR_GRID_INDEX_AXIS(vec, res, gmin, scale, 1);
	k = HAIR_GRID_INDEX_AXIS(vec, res, gmin, scale, 2);
	offset = i + (j + k*res)*res;
	
	uvw[0] = (vec[0] - gmin[0]) / scale[0];
	uvw[1] = (vec[1] - gmin[1]) / scale[1];
	uvw[2] = (vec[2] - gmin[2]) / scale[2];
	
	weights[0] = dist_tent_v3f3(uvw, (float)i    , (float)j    , (float)k    );
	weights[1] = dist_tent_v3f3(uvw, (float)(i+1), (float)j    , (float)k    );
	weights[2] = dist_tent_v3f3(uvw, (float)i    , (float)(j+1), (float)k    );
	weights[3] = dist_tent_v3f3(uvw, (float)(i+1), (float)(j+1), (float)k    );
	weights[4] = dist_tent_v3f3(uvw, (float)i    , (float)j    , (float)(k+1));
	weights[5] = dist_tent_v3f3(uvw, (float)(i+1), (float)j    , (float)(k+1));
	weights[6] = dist_tent_v3f3(uvw, (float)i    , (float)(j+1), (float)(k+1));
	weights[7] = dist_tent_v3f3(uvw, (float)(i+1), (float)(j+1), (float)(k+1));
	
	return offset;
}

static HairGridVert *hair_volume_create_hair_grid(ClothModifierData *clmd, lfVector *lX, lfVector *lV, unsigned int numverts)
{
	int res = hair_grid_res;
	int size = hair_grid_size(res);
	HairGridVert *hairgrid;
	float gmin[3], gmax[3], scale[3];
	/* 2.0f is an experimental value that seems to give good results */
	float smoothfac = 2.0f * clmd->sim_parms->velocity_smooth;
	unsigned int	v = 0;
	int	            i = 0;

	hair_volume_get_boundbox(lX, numverts, gmin, gmax);
	hair_grid_get_scale(res, gmin, gmax, scale);

	hairgrid = MEM_mallocN(sizeof(HairGridVert) * size, "hair voxel data");

	/* initialize grid */
	for (i = 0; i < size; ++i) {
		zero_v3(hairgrid[i].velocity);
		hairgrid[i].density = 0.0f;
	}

	/* gather velocities & density */
	if (smoothfac > 0.0f) {
		for (v = 0; v < numverts; v++) {
			float *V = lV[v];
			float weights[8];
			int di, dj, dk;
			int offset;
			
			if (!hair_grid_point_valid(lX[v], gmin, gmax))
				continue;
			
			offset = hair_grid_weights(res, gmin, scale, lX[v], weights);
			
			for (di = 0; di < 2; ++di) {
				for (dj = 0; dj < 2; ++dj) {
					for (dk = 0; dk < 2; ++dk) {
						int voffset = offset + di + (dj + dk*res)*res;
						int iw = di + dj*2 + dk*4;
						
						hairgrid[voffset].density += weights[iw];
						madd_v3_v3fl(hairgrid[voffset].velocity, V, weights[iw]);
					}
				}
			}
		}
	}

	/* divide velocity with density */
	for (i = 0; i < size; i++) {
		float density = hairgrid[i].density;
		if (density > 0.0f)
			mul_v3_fl(hairgrid[i].velocity, 1.0f/density);
	}
	
	return hairgrid;
}


static HairGridVert *hair_volume_create_collision_grid(ClothModifierData *clmd, lfVector *lX, unsigned int numverts)
{
	int res = hair_grid_res;
	int size = hair_grid_size(res);
	HairGridVert *collgrid;
	ListBase *colliders;
	ColliderCache *col = NULL;
	float gmin[3], gmax[3], scale[3];
	/* 2.0f is an experimental value that seems to give good results */
	float collfac = 2.0f * clmd->sim_parms->collider_friction;
	unsigned int	v = 0;
	int	            i = 0;

	hair_volume_get_boundbox(lX, numverts, gmin, gmax);
	hair_grid_get_scale(res, gmin, gmax, scale);

	collgrid = MEM_mallocN(sizeof(HairGridVert) * size, "hair collider voxel data");

	/* initialize grid */
	for (i = 0; i < size; ++i) {
		zero_v3(collgrid[i].velocity);
		collgrid[i].density = 0.0f;
	}

	/* gather colliders */
	colliders = get_collider_cache(clmd->scene, NULL, NULL);
	if (colliders && collfac > 0.0f) {
		for (col = colliders->first; col; col = col->next) {
			MVert *loc0 = col->collmd->x;
			MVert *loc1 = col->collmd->xnew;
			float vel[3];
			float weights[8];
			int di, dj, dk;
			
			for (v=0; v < col->collmd->numverts; v++, loc0++, loc1++) {
				int offset;
				
				if (!hair_grid_point_valid(loc1->co, gmin, gmax))
					continue;
				
				offset = hair_grid_weights(res, gmin, scale, lX[v], weights);
				
				sub_v3_v3v3(vel, loc1->co, loc0->co);
				
				for (di = 0; di < 2; ++di) {
					for (dj = 0; dj < 2; ++dj) {
						for (dk = 0; dk < 2; ++dk) {
							int voffset = offset + di + (dj + dk*res)*res;
							int iw = di + dj*2 + dk*4;
							
							collgrid[voffset].density += weights[iw];
							madd_v3_v3fl(collgrid[voffset].velocity, vel, weights[iw]);
						}
					}
				}
			}
		}
	}
	free_collider_cache(&colliders);

	/* divide velocity with density */
	for (i = 0; i < size; i++) {
		float density = collgrid[i].density;
		if (density > 0.0f)
			mul_v3_fl(collgrid[i].velocity, 1.0f/density);
	}
	
	return collgrid;
}

static void hair_volume_forces(ClothModifierData *clmd, lfVector *lF, lfVector *lX, lfVector *lV, unsigned int numverts)
{
	HairGridVert *hairgrid, *collgrid;
	float gmin[3], gmax[3], scale[3];
	/* 2.0f is an experimental value that seems to give good results */
	float smoothfac = 2.0f * clmd->sim_parms->velocity_smooth;
	float collfac = 2.0f * clmd->sim_parms->collider_friction;
	float pressfac = clmd->sim_parms->pressure;
	float minpress = clmd->sim_parms->pressure_threshold;
	
	if (smoothfac <= 0.0f && collfac <= 0.0f && pressfac <= 0.0f)
		return;
	
	hair_volume_get_boundbox(lX, numverts, gmin, gmax);
	hair_grid_get_scale(hair_grid_res, gmin, gmax, scale);
	
	hairgrid = hair_volume_create_hair_grid(clmd, lX, lV, numverts);
	collgrid = hair_volume_create_collision_grid(clmd, lX, numverts);
	
	hair_velocity_smoothing(hairgrid, gmin, scale, smoothfac, lF, lX, lV, numverts);
	hair_velocity_collision(collgrid, gmin, scale, collfac, lF, lX, lV, numverts);
	hair_pressure_force(hairgrid, gmin, scale, pressfac, minpress, lF, lX, numverts);
	
	MEM_freeN(hairgrid);
	MEM_freeN(collgrid);
}

#if 0
bool implicit_hair_volume_get_texture_data(Object *UNUSED(ob), ClothModifierData *clmd, ListBase *UNUSED(effectors), VoxelData *vd)
{
	lfVector *lX, *lV;
	HairGridVert *hairgrid/*, *collgrid*/;
	int numverts;
	int totres, i;
	int depth;

	if (!clmd->clothObject || !clmd->clothObject->implicit)
		return false;

	lX = clmd->clothObject->implicit->X;
	lV = clmd->clothObject->implicit->V;
	numverts = clmd->clothObject->numverts;

	hairgrid = hair_volume_create_hair_grid(clmd, lX, lV, numverts);
//	collgrid = hair_volume_create_collision_grid(clmd, lX, numverts);

	vd->resol[0] = hair_grid_res;
	vd->resol[1] = hair_grid_res;
	vd->resol[2] = hair_grid_res;
	
	totres = hair_grid_size(hair_grid_res);
	
	if (vd->hair_type == TEX_VD_HAIRVELOCITY) {
		depth = 4;
		vd->data_type = TEX_VD_RGBA_PREMUL;
	}
	else {
		depth = 1;
		vd->data_type = TEX_VD_INTENSITY;
	}
	
	if (totres > 0) {
		vd->dataset = (float *)MEM_mapallocN(sizeof(float) * depth * (totres), "hair volume texture data");
		
		for (i = 0; i < totres; ++i) {
			switch (vd->hair_type) {
				case TEX_VD_HAIRDENSITY:
					vd->dataset[i] = hairgrid[i].density;
					break;
				
				case TEX_VD_HAIRRESTDENSITY:
					vd->dataset[i] = 0.0f; // TODO
					break;
				
				case TEX_VD_HAIRVELOCITY:
					vd->dataset[i + 0*totres] = hairgrid[i].velocity[0];
					vd->dataset[i + 1*totres] = hairgrid[i].velocity[1];
					vd->dataset[i + 2*totres] = hairgrid[i].velocity[2];
					vd->dataset[i + 3*totres] = len_v3(hairgrid[i].velocity);
					break;
				
				case TEX_VD_HAIRENERGY:
					vd->dataset[i] = 0.0f; // TODO
					break;
			}
		}
	}
	else {
		vd->dataset = NULL;
	}
	
	MEM_freeN(hairgrid);
//	MEM_freeN(collgrid);
	
	return true;
}
#endif

#endif
