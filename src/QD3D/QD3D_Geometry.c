/****************************/
/*   	QD3D GEOMETRY.C	    */
/* (c)1997-99 Pangea Software  */
/* By Brian Greenstone      */
/****************************/


/****************************/
/*    EXTERNALS             */
/****************************/

#include "game.h"


/****************************/
/*    PROTOTYPES            */
/****************************/

static void ExplodeTriMesh(
		const TQ3TriMeshData* inMesh,
		const TQ3Matrix4x4* transform,
		float boomForce,
		Byte shardMode,
		int shardDensity,
		float shardDecaySpeed);


/****************************/
/*    CONSTANTS             */
/****************************/

static const TQ3Matrix4x4 kIdentity4x4 =
{{
	{1, 0, 0, 0},
	{0, 1, 0, 0},
	{0, 0, 1, 0},
	{0, 0, 0, 1},
}};

/*********************/
/*    VARIABLES      */
/*********************/

long				gNumShards = 0;
ShardType			gShards[MAX_SHARDS];

static RenderModifiers kShardRenderMods;

/*************** QD3D: CALC OBJECT BOUNDING BOX ************************/

void QD3D_CalcObjectBoundingBox(int numMeshes, TQ3TriMeshData** meshList, TQ3BoundingBox* boundingBox)
{
	GAME_ASSERT(numMeshes);
	GAME_ASSERT(meshList);
	GAME_ASSERT(boundingBox);

	boundingBox->isEmpty = true;
	boundingBox->min = (TQ3Point3D) { 0, 0, 0 };
	boundingBox->max = (TQ3Point3D) { 0, 0, 0 };

	for (int i = 0; i < numMeshes; i++)
	{
		TQ3TriMeshData* mesh = meshList[i];
		for (int v = 0; v < mesh->numPoints; v++)
		{
			TQ3Point3D p = mesh->points[v];

			if (boundingBox->isEmpty)
			{
				boundingBox->isEmpty = false;
				boundingBox->min = p;
				boundingBox->max = p;
			}
			else
			{
				if (p.x < boundingBox->min.x) boundingBox->min.x = p.x;
				if (p.y < boundingBox->min.y) boundingBox->min.y = p.y;
				if (p.z < boundingBox->min.z) boundingBox->min.z = p.z;

				if (p.x > boundingBox->max.x) boundingBox->max.x = p.x;
				if (p.y > boundingBox->max.y) boundingBox->max.y = p.y;
				if (p.z > boundingBox->max.z) boundingBox->max.z = p.z;
			}
		}
	}
}


/*************** QD3D: CALC OBJECT BOUNDING SPHERE ************************/

void QD3D_CalcObjectBoundingSphere(int numMeshes, TQ3TriMeshData** meshList, TQ3BoundingSphere* boundingSphere)
{
	GAME_ASSERT(numMeshes);
	GAME_ASSERT(meshList);
	GAME_ASSERT(boundingSphere);

	int totalNumPoints = 0;
	TQ3Point3D origin = (TQ3Point3D) {0, 0, 0};

	for (int i = 0; i < numMeshes; i++)
	{
		TQ3TriMeshData* mesh = meshList[i];
		for (int v = 0; v < mesh->numPoints; v++)
		{
			TQ3Point3D p = mesh->points[v];
			origin.x += p.x;
			origin.y += p.y;
			origin.z += p.z;
			totalNumPoints++;
		}
	}

	origin.x /= (float)totalNumPoints;
	origin.y /= (float)totalNumPoints;
	origin.z /= (float)totalNumPoints;

	float radiusSquared = 0;

	for (int i = 0; i < numMeshes; i++)
	{
		TQ3TriMeshData* mesh = meshList[i];
		for (int v = 0; v < mesh->numPoints; v++)
		{
			float newRadius = Q3Point3D_DistanceSquared(&origin, &mesh->points[v]);
			if (newRadius > radiusSquared)
				radiusSquared = newRadius;
		}
	}

	boundingSphere->isEmpty = totalNumPoints == 0 ? kQ3True : kQ3False;
	boundingSphere->origin = origin;
	boundingSphere->radius = sqrtf(radiusSquared);
}




//===================================================================================================
//===================================================================================================
//===================================================================================================

#pragma mark =========== shard explosion ==============


/********************** QD3D: INIT SHARDS **********************/

void QD3D_InitShards(void)
{
	gNumShards = 0;

	for (int i = 0; i < MAX_SHARDS; i++)
	{
		ShardType* shard = &gShards[i];
		shard->isUsed = false;
		shard->mesh = Q3TriMeshData_New(1, 3, kQ3TriMeshDataFeatureVertexUVs | kQ3TriMeshDataFeatureVertexNormals);
		for (int v = 0; v < 3; v++)
			shard->mesh->triangles[0].pointIndices[v] = v;
	}

	Render_SetDefaultModifiers(&kShardRenderMods);
	kShardRenderMods.statusBits |= STATUS_BIT_KEEPBACKFACES;			// draw both faces on shards
}


/********************** QD3D: DISPOSE SHARD MESHES **********************/


void QD3D_DisposeShards(void)
{
	gNumShards = 0;

	for (int i = 0; i < MAX_SHARDS; i++)
	{
		ShardType* shard = &gShards[i];
		if (shard->mesh)
		{
			Q3TriMeshData_Dispose(shard->mesh);
			shard->mesh = NULL;
		}
		shard->isUsed = false;
	}
}


/********************* FIND FREE SHARD ***********************/
//
// OUTPUT: -1 == none free found
//

static inline long FindFreeShard(void)
{
long	i;

	if (gNumShards >= MAX_SHARDS)
		return(-1);

	for (i = 0; i < MAX_SHARDS; i++)
		if (gShards[i].isUsed == false)
			return(i);

	return(-1);
}


/****************** QD3D: EXPLODE GEOMETRY ***********************/
//
// Given any object as input, breaks up all polys into separate objNodes &
// calculates velocity et.al.
//

void QD3D_ExplodeGeometry(ObjNode* theNode, float boomForce, Byte shardMode, int shardDensity, float shardDecaySpeed)
{
		/* SEE IF EXPLODING SKELETON OR PLAIN GEOMETRY */

	const TQ3Matrix4x4* transform;

	if (theNode->Genre == SKELETON_GENRE)
	{
		transform = &kIdentity4x4;							// init to identity matrix (skeleton vertices are pre-transformed)
	}
	else
	{
		transform = &theNode->BaseTransformMatrix;			// static object: set pos/rot/scale from its base transform matrix
	}


		/* EXPLODE EACH TRIMESH INDIVIDUALLY */

	for (int i = 0; i < theNode->NumMeshes; i++)
	{
		ExplodeTriMesh(theNode->MeshList[i], transform, boomForce, shardMode, shardDensity, shardDecaySpeed);
	}
}


/************* MIRROR MESHES ON Z AXIS *************/
//
// Makes mirrored clones of an object's meshes on the Z axis.
// This is meant to complete half-sphere or half-cylinder cycloramas
// which don't cover the entire viewport when using an ultra-wide display.
//

void QD3D_MirrorMeshesZ(ObjNode* theNode)
{
	int numOriginalMeshes = theNode->NumMeshes;

	AttachGeometryToDisplayGroupObject(theNode, numOriginalMeshes, theNode->MeshList, kAttachGeometry_CloneMeshes);

	GAME_ASSERT(theNode->NumMeshes == numOriginalMeshes*2);

	for (int meshID = numOriginalMeshes; meshID < theNode->NumMeshes; meshID++)
	{
		TQ3TriMeshData* mesh = theNode->MeshList[meshID];

		for (int p = 0; p < mesh->numPoints; p++)
		{
			mesh->points[p].z = -mesh->points[p].z;
		}

		// Invert triangle winding
		for (int t = 0; t < mesh->numTriangles; t++)
		{
			uint16_t* triPoints = theNode->MeshList[meshID]->triangles[t].pointIndices;
			uint16_t temp = triPoints[0];
			triPoints[0] = triPoints[2];
			triPoints[2] = temp;
		}
	}
}


/********************** EXPLODE TRIMESH *******************************/
//
// INPUT: 	theTriMesh = trimesh object if input is object (nil if inData)
//			inData = trimesh data if input is raw data (nil if above)
//

static void ExplodeTriMesh(
		const TQ3TriMeshData* inMesh,
		const TQ3Matrix4x4* transform,
		float boomForce,
		Byte shardMode,
		int shardDensity,
		float shardDecaySpeed)
{
			/*******************************/
			/* SCAN THRU ALL TRIMESH FACES */
			/*******************************/
					
	for (int t = 0; t < inMesh->numTriangles; t += shardDensity)		// scan thru all faces
	{
				/* GET FREE SHARD INDEX */

		long shardIndex = FindFreeShard();
		if (shardIndex == -1)											// see if all out
			break;

		ShardType* shard = &gShards[shardIndex];
		TQ3TriMeshData* sMesh = shard->mesh;

		const uint16_t* ind = inMesh->triangles[t].pointIndices;		// get indices of 3 points

				/*********************/
				/* INIT TRIMESH DATA */
				/*********************/
 
				/* DO POINTS */

		for (int v = 0; v < 3; v++)
		{
			Q3Point3D_Transform(&inMesh->points[ind[v]], transform, &sMesh->points[v]);		// transform points
		}

		TQ3Point3D centerPt =
		{
			(sMesh->points[0].x + sMesh->points[1].x + sMesh->points[2].x) * 0.3333f,		// calc center of polygon
			(sMesh->points[0].y + sMesh->points[1].y + sMesh->points[2].y) * 0.3333f,
			(sMesh->points[0].z + sMesh->points[1].z + sMesh->points[2].z) * 0.3333f,
		};

		for (int v = 0; v < 3; v++)
		{
			sMesh->points[v].x -= centerPt.x;											// offset coords to be around center
			sMesh->points[v].y -= centerPt.y;
			sMesh->points[v].z -= centerPt.z;
		}

		sMesh->bBox.min = sMesh->bBox.max = centerPt;
		sMesh->bBox.isEmpty = kQ3False;


				/* DO VERTEX NORMALS */

		GAME_ASSERT(inMesh->hasVertexNormals);
		for (int v = 0; v < 3; v++)
		{
			Q3Vector3D_Transform(&inMesh->vertexNormals[ind[v]], transform, &sMesh->vertexNormals[v]);		// transform normals
			Q3Vector3D_Normalize(&sMesh->vertexNormals[v], &sMesh->vertexNormals[v]);						// normalize normals
		}

				/* DO VERTEX UV'S */

		sMesh->texturingMode = inMesh->texturingMode;
		if (inMesh->texturingMode != kQ3TexturingModeOff)				// see if also has UV
		{
			GAME_ASSERT(inMesh->vertexUVs);
			for (int v = 0; v < 3; v++)									// get vertex u/v's
			{
				sMesh->vertexUVs[v] = inMesh->vertexUVs[ind[v]];
			}
			sMesh->glTextureName = inMesh->glTextureName;
			sMesh->internalTextureID = inMesh->internalTextureID;
		}

				/* DO VERTEX COLORS */

		sMesh->diffuseColor = inMesh->diffuseColor;

		sMesh->hasVertexColors = inMesh->hasVertexColors;				// has per-vertex colors?
		if (inMesh->hasVertexColors)
		{
			for (int v = 0; v < 3; v++)									// get per-vertex colors
			{
				sMesh->vertexColors[v] = inMesh->vertexColors[ind[v]];
			}
		}

			/*********************/
			/* SET PHYSICS STUFF */
			/*********************/

		shard->coord = centerPt;
		shard->rot = (TQ3Vector3D) {0, 0, 0};
		shard->scale = 1.0f;

		shard->coordDelta.x = (RandomFloat() - 0.5f) * boomForce;
		shard->coordDelta.y = (RandomFloat() - 0.5f) * boomForce;
		shard->coordDelta.z = (RandomFloat() - 0.5f) * boomForce;
		if (shardMode & SHARD_MODE_UPTHRUST)
			shard->coordDelta.y += 1.5f * boomForce;

		shard->rotDelta.x = (RandomFloat() - 0.5f) * 4.0f;			// random rotation deltas
		shard->rotDelta.y = (RandomFloat() - 0.5f) * 4.0f;
		shard->rotDelta.z = (RandomFloat() - 0.5f) * 4.0f;

		shard->decaySpeed = shardDecaySpeed;
		shard->mode = shardMode;

				/* SET VALID & INC COUNTER */

		shard->isUsed = true;
		gNumShards++;
	}
}


/************************** QD3D: MOVE SHARDS ****************************/

void QD3D_MoveShards(void)
{
float	ty,y,fps,x,z;
TQ3Matrix4x4	matrix,matrix2;

	if (gNumShards == 0)										// quick check if any shards at all
		return;

	fps = gFramesPerSecondFrac;
	ty = -100.0f;												// pin point to "floor" if no terrain

	for (int i = 0; i < MAX_SHARDS; i++)
	{
		if (!gShards[i].isUsed)
			continue;

		ShardType* shard = &gShards[i];

				/* ROTATE IT */

		shard->rot.x += shard->rotDelta.x * fps;
		shard->rot.y += shard->rotDelta.y * fps;
		shard->rot.z += shard->rotDelta.z * fps;

					/* MOVE IT */

		if (shard->mode & SHARD_MODE_HEAVYGRAVITY)
			shard->coordDelta.y -= fps * 1700.0f / 2;			// gravity
		else
			shard->coordDelta.y -= fps * 1700.0f / 3;			// gravity

		x = (shard->coord.x += shard->coordDelta.x * fps);
		y = (shard->coord.y += shard->coordDelta.y * fps);
		z = (shard->coord.z += shard->coordDelta.z * fps);


					/* SEE IF BOUNCE */

		if (gFloorMap)
			ty = GetTerrainHeightAtCoord(x,z, FLOOR);			// get terrain height here

		if (y <= ty)
		{
			if (shard->mode & SHARD_MODE_BOUNCE)
			{
				shard->coord.y  = ty;
				shard->coordDelta.y *= -0.5f;
				shard->coordDelta.x *= 0.9f;
				shard->coordDelta.z *= 0.9f;
			}
			else
				goto del;
		}

					/* SCALE IT */

		shard->scale -= shard->decaySpeed * fps;
		if (shard->scale <= 0.0f)
		{
				/* DEACTIVATE THIS SHARD */
	del:
			shard->isUsed = false;
			gNumShards--;
			continue;
		}

			/***************************/
			/* UPDATE TRANSFORM MATRIX */
			/***************************/


				/* SET SCALE MATRIX */

		Q3Matrix4x4_SetScale(&shard->matrix, shard->scale, shard->scale, shard->scale);

					/* NOW ROTATE IT */

		Q3Matrix4x4_SetRotate_XYZ(&matrix, shard->rot.x, shard->rot.y, shard->rot.z);
		MatrixMultiplyFast(&shard->matrix, &matrix, &matrix2);
	
					/* NOW TRANSLATE IT */

		Q3Matrix4x4_SetTranslate(&matrix, shard->coord.x, shard->coord.y, shard->coord.z);
		MatrixMultiplyFast(&matrix2,&matrix, &shard->matrix);
	}
}


/************************* QD3D: DRAW SHARDS ****************************/

void QD3D_DrawShards(const QD3DSetupOutputType *setupInfo)
{
	(void) setupInfo;

	if (gNumShards == 0)										// quick check if any shards at all
		return;

	for (int i = 0; i < MAX_SHARDS; i++)
	{
		ShardType* shard = &gShards[i];
		if (shard->isUsed)
		{
			Render_SubmitMesh(shard->mesh, &shard->matrix, &kShardRenderMods, &shard->coord);
		}
	}
}



//============================================================================================
//============================================================================================
//============================================================================================



#pragma mark -


/****************** QD3D: SCROLL UVs ***********************/
//
// Given any object as input this will scroll any u/v coordinates by the given amount
//

void QD3D_ScrollUVs(TQ3TriMeshData* mesh, float du, float dv)
{
	GAME_ASSERT(mesh->vertexUVs);
	for (int j = 0; j < mesh->numPoints; j++)
	{
		mesh->vertexUVs[j].u += du;
		mesh->vertexUVs[j].v += dv;
	}
}



#pragma mark -

//============================================================================================
//============================================================================================
//============================================================================================


TQ3TriMeshData* MakeQuadMesh(int numQuads, float width, float height)
{
	TQ3TriMeshData* mesh = Q3TriMeshData_New(2*numQuads, 4*numQuads, kQ3TriMeshDataFeatureVertexUVs);
	mesh->texturingMode = kQ3TexturingModeOff;

	for (int q = 0; q < numQuads; q++)
	{
		mesh->triangles[2 * q + 0].pointIndices[0] = 4 * q + 0;
		mesh->triangles[2 * q + 0].pointIndices[1] = 4 * q + 1;
		mesh->triangles[2 * q + 0].pointIndices[2] = 4 * q + 2;

		mesh->triangles[2 * q + 1].pointIndices[0] = 4 * q + 0;
		mesh->triangles[2 * q + 1].pointIndices[1] = 4 * q + 2;
		mesh->triangles[2 * q + 1].pointIndices[2] = 4 * q + 3;

		float xExtent = width * .5f;
		float yExtent = height * .5f;
		mesh->points[4 * q + 0] = (TQ3Point3D) {-xExtent, -yExtent, 0 };
		mesh->points[4 * q + 1] = (TQ3Point3D) {+xExtent, -yExtent, 0 };
		mesh->points[4 * q + 2] = (TQ3Point3D) {+xExtent, +yExtent, 0 };
		mesh->points[4 * q + 3] = (TQ3Point3D) {-xExtent, +yExtent, 0 };

		mesh->vertexUVs[4 * q + 0] = (TQ3Param2D) { 0, 1 };
		mesh->vertexUVs[4 * q + 1] = (TQ3Param2D) { 1, 1 };
		mesh->vertexUVs[4 * q + 2] = (TQ3Param2D) { 1, 0 };
		mesh->vertexUVs[4 * q + 3] = (TQ3Param2D) { 0, 0 };
	}

	return mesh;
}

TQ3TriMeshData* MakeQuadMesh_UI(
		float xyLeft, float xyTop, float xyRight, float xyBottom,
		float uvLeft, float uvTop, float uvRight, float uvBottom)
{
	TQ3TriMeshData* mesh = Q3TriMeshData_New(2, 4, kQ3TriMeshDataFeatureVertexUVs);
	mesh->texturingMode = kQ3TexturingModeOff;

	mesh->triangles[0].pointIndices[0] = 0;
	mesh->triangles[0].pointIndices[1] = 1;
	mesh->triangles[0].pointIndices[2] = 2;

	mesh->triangles[1].pointIndices[0] = 0;
	mesh->triangles[1].pointIndices[1] = 2;
	mesh->triangles[1].pointIndices[2] = 3;

	mesh->points[0] = (TQ3Point3D) {xyLeft,		xyBottom,	0};
	mesh->points[1] = (TQ3Point3D) {xyRight,	xyBottom,	0};
	mesh->points[2] = (TQ3Point3D) {xyRight,	xyTop,		0};
	mesh->points[3] = (TQ3Point3D) {xyLeft,		xyTop,		0};

	mesh->vertexUVs[0] = (TQ3Param2D) {uvLeft,	uvBottom};
	mesh->vertexUVs[1] = (TQ3Param2D) {uvRight,	uvBottom};
	mesh->vertexUVs[2] = (TQ3Param2D) {uvRight,	uvTop};
	mesh->vertexUVs[3] = (TQ3Param2D) {uvLeft,	uvTop};

	return mesh;
}
