// TEXT MESH.C
// (C) 2020 Iliyas Jorio
// This file is part of Bugdom. https://github.com/jorio/bugdom

#include "game.h"
#include <stdio.h>

typedef struct
{
	float x;
	float y;
	float w;
	float h;
	float xoff;
	float yoff;
	float xadv;
} AtlasGlyph;

static GLuint gFontTexture = 0;
static int gNumGlyphs = 0;
static float gLineHeight = 0;
static AtlasGlyph* gAtlasGlyphs = NULL;

static const TextMeshDef gDefaultTextMeshDef =
{
	.coord				= { 0, 0, 0 },
	.meshOrigin			= { 0, 0, 0 },
	.shadowOffset		= { 2, -2 },
	.scale				= .5f,
	.color				= { 1, 1, 1, 1 },
	.shadowColor		= { 0, 0, 0, 1 },
	.withShadow			= true,
	.slot				= 100,
	.align				= TEXTMESH_ALIGN_LEFT,
	.letterSpacing		= 0,
};

TQ3TriMeshData* TextMesh_CreateMesh(const TextMeshDef* def, const char* text)
{
	return TextMesh_SetMesh(def, text, NULL);
}

TQ3TriMeshData* TextMesh_SetMesh(const TextMeshDef* def, const char* text, TQ3TriMeshData* recycleMesh)
{
	float x = gDefaultTextMeshDef.meshOrigin.x;
	float y = gDefaultTextMeshDef.meshOrigin.y;
	float z = gDefaultTextMeshDef.meshOrigin.z;
	int align = gDefaultTextMeshDef.align;
	float spacing = gDefaultTextMeshDef.letterSpacing;
	if (def)
	{
		x = def->meshOrigin.x;
		y = def->meshOrigin.y;
		z = def->meshOrigin.z;
		align = def->align;
		spacing = def->letterSpacing;
	}

	GAME_ASSERT(gAtlasGlyphs);
	GAME_ASSERT(gFontTexture);

	// Compute number of quads and line width
	float lineWidth = 0;
	int numQuads = 0;
	for (const char* c = text; *c; c++)
	{
		if (*c == '\n')		// TODO: line widths for strings containing line breaks aren't supported yet
			continue;

		GAME_ASSERT(*c >= ' ');
		GAME_ASSERT(*c <= '~');
		const AtlasGlyph g = gAtlasGlyphs[*c - ' '];
		lineWidth += g.xadv + spacing;
		if (*c != ' ')
			numQuads++;
	}

	// Adjust start x for text alignment
	if (align == TEXTMESH_ALIGN_CENTER)
		x -= lineWidth * .5f;
	else if (align == TEXTMESH_ALIGN_RIGHT)
		x -= lineWidth;

	float x0 = x;

	// Adjust y for ascender
	y += gLineHeight * .7f;

	// Create the mesh
	TQ3TriMeshData* mesh;
	if (recycleMesh)
	{
		mesh = recycleMesh;
		GAME_ASSERT(mesh->numTriangles >= numQuads*2);
		GAME_ASSERT(mesh->numPoints >= numQuads*4);
		GAME_ASSERT(mesh->vertexUVs);
		mesh->numTriangles = numQuads*2;
		mesh->numPoints = numQuads*4;
	}
	else
	{
		mesh = Q3TriMeshData_New(numQuads*2, numQuads*4, kQ3TriMeshDataFeatureVertexUVs);
	}
	mesh->texturingMode = kQ3TexturingModeAlphaBlend;
	mesh->glTextureName = gFontTexture;

	// Create a quad for each character
	int t = 0;
	int p = 0;
	for (const char* c = text; *c; c++)
	{
		if (*c == '\n')
		{
			x = x0;
			y -= gLineHeight;
			continue;
		}

		GAME_ASSERT(*c >= ' ');
		GAME_ASSERT(*c <= '~');
		const AtlasGlyph g = gAtlasGlyphs[*c - ' '];

		if (*c == ' ')
		{
			x += g.xadv + spacing;
			continue;
		}

		float qx = x + g.xoff + g.w*.5f;
		float qy = y - g.yoff - g.h*.5f;

		mesh->triangles[t + 0].pointIndices[0] = p + 0;
		mesh->triangles[t + 0].pointIndices[1] = p + 1;
		mesh->triangles[t + 0].pointIndices[2] = p + 2;
		mesh->triangles[t + 1].pointIndices[0] = p + 0;
		mesh->triangles[t + 1].pointIndices[1] = p + 2;
		mesh->triangles[t + 1].pointIndices[2] = p + 3;
		mesh->points[p + 0] = (TQ3Point3D) { qx - g.w*.5f, qy - g.h*.5f, z };
		mesh->points[p + 1] = (TQ3Point3D) { qx + g.w*.5f, qy - g.h*.5f, z };
		mesh->points[p + 2] = (TQ3Point3D) { qx + g.w*.5f, qy + g.h*.5f, z };
		mesh->points[p + 3] = (TQ3Point3D) { qx - g.w*.5f, qy + g.h*.5f, z };
		mesh->vertexUVs[p + 0] = (TQ3Param2D) { g.x/512.0f,			(g.y+g.h)/256.0f };
		mesh->vertexUVs[p + 1] = (TQ3Param2D) { (g.x+g.w)/512.0f,	(g.y+g.h)/256.0f };
		mesh->vertexUVs[p + 2] = (TQ3Param2D) { (g.x+g.w)/512.0f,	g.y/256.0f };
		mesh->vertexUVs[p + 3] = (TQ3Param2D) { g.x/512.0f,			g.y/256.0f };

		x += g.xadv + spacing;
		t += 2;
		p += 4;
	}

	GAME_ASSERT(p == mesh->numPoints);

	return mesh;
}

static void SkipLine(const char** dataPtr)
{
	const char* data = *dataPtr;

	while (*data)
	{
		char c = data[0];
		data++;
		if (c == '\r' && *data != '\n')
			break;
		if (c == '\n')
			break;
	}

	GAME_ASSERT(*data);
	*dataPtr = data;
}

// Parse an SFL file produced by fontbuilder
static void ParseSFL(const char* data)
{
	int nArgs = 0;
	int junk = 0;

	SkipLine(&data);	// Skip font name

	nArgs = sscanf(data, "%d %f", &junk, &gLineHeight);
	GAME_ASSERT(nArgs == 2);
	SkipLine(&data);

	SkipLine(&data);	// Skip image filename

	nArgs = sscanf(data, "%d", &gNumGlyphs);
	GAME_ASSERT(nArgs == 1);
	SkipLine(&data);

	GAME_ASSERT_MESSAGE(!gAtlasGlyphs, "atlas glyphs were already loaded");
	gAtlasGlyphs = (AtlasGlyph*) NewPtrClear(gNumGlyphs * sizeof(AtlasGlyph));

	for (int i = 0; i < gNumGlyphs; i++)
	{
		nArgs = sscanf(
				data,
				"%d %f %f %f %f %f %f %f",
				&junk,
				&gAtlasGlyphs[i].x,
				&gAtlasGlyphs[i].y,
				&gAtlasGlyphs[i].w,
				&gAtlasGlyphs[i].h,
				&gAtlasGlyphs[i].xoff,
				&gAtlasGlyphs[i].yoff,
				&gAtlasGlyphs[i].xadv);
		GAME_ASSERT(nArgs == 8);

		SkipLine(&data);
	}
}

void TextMesh_Init(void)
{
	OSErr err;

	gFontTexture = QD3D_LoadTextureFile(3000, kRendererTextureFlags_GrayscaleIsAlpha);

	short refNum = OpenGameFile(":images:textures:3000.sfl");

	// Get number of bytes until EOF
	long eof = 0;
	GetEOF(refNum, &eof);

	// Prep data buffer
	Ptr data = NewPtrClear(eof+1);

	// Read file into data buffer
	err = FSRead(refNum, &eof, data);
	GAME_ASSERT(err == noErr);
	FSClose(refNum);

	ParseSFL(data);

	DisposePtr(data);
}

void TextMesh_Shutdown(void)
{
	if (gAtlasGlyphs)
	{
		DisposePtr((Ptr) gAtlasGlyphs);
		gAtlasGlyphs = NULL;
	}

	if (gFontTexture)
	{
		glDeleteTextures(1, &gFontTexture);
		gFontTexture = 0;
	}
}

void TextMesh_FillDef(TextMeshDef* def)
{
	*def = gDefaultTextMeshDef;
}

ObjNode* TextMesh_Create(const TextMeshDef* def, const char* text)
{
	TQ3TriMeshData* mesh = TextMesh_CreateMesh(def, text);
	mesh->diffuseColor = def->color;

	gNewObjectDefinition.genre		= DISPLAY_GROUP_GENRE;
	gNewObjectDefinition.group		= MODEL_GROUP_ILLEGAL;
	gNewObjectDefinition.slot		= def->slot;
	gNewObjectDefinition.coord		= def->coord;
	gNewObjectDefinition.flags		= STATUS_BIT_NULLSHADER | STATUS_BIT_NOZWRITE | STATUS_BIT_NOFOG;
	gNewObjectDefinition.moveCall 	= nil;
	gNewObjectDefinition.rot		= 0.0f;
	gNewObjectDefinition.scale		= def->scale;
	ObjNode* textNode = MakeNewObject(&gNewObjectDefinition);

	// Attach color mesh
	AttachGeometryToDisplayGroupObject(textNode, 1, &mesh, kAttachGeometry_TransferMeshOwnership);

	//textNode->BoundingSphere.isEmpty = kQ3False;
	//textNode->BoundingSphere.radius = def->scale * lineWidth / 2.0f;

	UpdateObjectTransforms(textNode);

	// Create shadow node. We could just attach a new mesh to the main node,
	// but that wouldn't guarantee that the shadow mesh gets sorted before the main mesh.
	if (def->withShadow)
	{
		TextMeshDef shadowDef = *def;
		shadowDef.meshOrigin.x += shadowDef.shadowOffset.x;
		shadowDef.meshOrigin.y += shadowDef.shadowOffset.y;
		gNewObjectDefinition.coord.z += -0.1f;
		gNewObjectDefinition.slot = 0x7FFF;  // shadow node slot must be AFTER text node slot
		TQ3TriMeshData* shadowMesh = TextMesh_CreateMesh(&shadowDef, text);
		shadowMesh->diffuseColor = def->shadowColor;

		ObjNode* shadowNode = MakeNewObject(&gNewObjectDefinition);
		AttachGeometryToDisplayGroupObject(shadowNode, 1, &shadowMesh, kAttachGeometry_TransferMeshOwnership);
		UpdateObjectTransforms(shadowNode);

		// Set it as the textNode's shadow. textNode becomes responsible for deleting shadowNode.
		// The shadow's slot must absolutely follow the text's slot.
		textNode->ShadowNode = shadowNode;
		GAME_ASSERT_MESSAGE(textNode->Slot < shadowNode->Slot, "text node slot must precede shadow node slot!");
	}

	return textNode;
}