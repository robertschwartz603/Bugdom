/****************************/
/*      FILE ROUTINES       */
/* (c)1997-99 Pangea Software  */
/* By Brian Greenstone      */
/****************************/


/***************/
/* EXTERNALS   */
/***************/

#include <time.h>
#include <string.h>


extern	short			gMainAppRezFile,gTextureRezfile;
extern  TQ3Object		gObjectGroupList[MAX_3DMF_GROUPS][MAX_OBJECTS_IN_GROUP];
extern  short			gNumObjectsInGroupList[MAX_3DMF_GROUPS];
extern	short			gNumTerrainItems;
extern	short			gPrefsFolderVRefNum;
extern	long			gPrefsFolderDirID,gNumTerrainTextureTiles;
extern	long			gTerrainTileWidth,gTerrainTileDepth,gTerrainUnitWidth,gTerrainUnitDepth;		
extern	long			gNumSuperTilesDeep,gNumSuperTilesWide;
extern	u_short			**gFloorMap,**gCeilingMap,**gTileDataHandle;
extern	long			gCurrentSuperTileRow,gCurrentSuperTileCol;
extern	long			gMyStartX,gMyStartZ,gNumSplines;
extern	FSSpec			gDataSpec;
extern	TerrainInfoMatrixType	**gMapInfoMatrix;
extern	TerrainYCoordType		**gMapYCoords;
extern	TerrainItemEntryType 	**gMasterItemList;
extern	u_short			**gVertexColors[2],gLevelType,gAreaNum,gRealLevel;
extern	Boolean			gDoCeiling,gRestoringSavedGame,gMuteMusicFlag;
extern	SplineDefType	**gSplineList;
extern	FenceDefType	*gFenceList;
extern	long			gNumFences;
extern	u_long			gScore;
extern	float			gMyHealth,gBallTimer;
extern	short			gNumLives;
extern	short			gNumGoldClovers;

/****************************/
/*    PROTOTYPES            */
/****************************/

static void ReadDataFromSkeletonFile(SkeletonDefType *skeleton, const FSSpec* fsSpec3DMF);
static void ReadDataFromPlayfieldFile(FSSpec *specPtr);


/****************************/
/*    CONSTANTS             */
/****************************/

#define	BASE_PATH_TILE		900					// tile # of 1st path tile

#define	PICT_HEADER_SIZE	512

#define	SKELETON_FILE_VERS_NUM	0x0110			// v1.1

#define	SAVE_GAME_VERSION	0x00000120			// Bugdom v1.2

#define PREFS_HEADER_LENGTH 16
#define PREFS_FILE_NAME ":Bugdom:Prefs"
const char PREFS_HEADER_STRING[PREFS_HEADER_LENGTH+1] = "NewBugdomPrefs02";		// Bump this every time prefs struct changes -- note: this will reset user prefs


		/* PLAYFIELD HEADER */
		// (READ IN FROM FILE -- MUST BE BYTESWAPPED!)
		
typedef struct
{
	NumVersion	version;							// version of file
	int32_t		numItems;							// # items in map
	int32_t		mapWidth;							// width of map
	int32_t		mapHeight;							// height of map
	int32_t		numTilePages;						// # tile pages
	int32_t		numTilesInList;						// # extracted tiles in list
	float		tileSize;							// 3D unit size of a tile
	float		minY,maxY;							// min/max height values
	int32_t		numSplines;							// # splines
	int32_t		numFences;							// # fences
}PlayfieldHeaderType;

		/* FENCE STRUCTURE IN FILE */
		//
		// note: we copy this data into our own fence list
		//		since the game uses a slightly different
		//		data structure.
		//
		// (READ IN FROM FILE -- MUST BE BYTESWAPPED!)
		
typedef struct
{
	uint16_t		type;				// type of fence
	int16_t			numNubs;			// # nubs in fence
	int32_t 		_junkptr1;
	Rect			bBox;				// bounding box of fence area	
} File_FenceDefType;


/**********************/
/*     VARIABLES      */
/**********************/

float	g3DTileSize, g3DMinY, g3DMaxY;

int		gCurrentSaveSlot = -1;

/******************* LOAD SKELETON *******************/
//
// Loads a skeleton file & creates storage for it.
// 
// NOTE: Skeleton types 0..NUM_CHARACTERS-1 are reserved for player character skeletons.
//		Skeleton types NUM_CHARACTERS and over are for other skeleton entities.
//
// OUTPUT:	Ptr to skeleton data
//

SkeletonDefType *LoadSkeletonFile(short skeletonType)
{
short		fRefNum;
FSSpec		fsSpecSkeleton;
FSSpec		fsSpec3DMF;
SkeletonDefType	*skeleton;
const char* modelName = NULL;
char		pathBuf[128];

				/* SET CORRECT FILENAME */

	switch(skeletonType)
	{
		case	SKELETON_TYPE_BOXERFLY:		modelName = "BoxerFly";			break;
		case	SKELETON_TYPE_ME:			modelName = "DoodleBug";		break;
		case	SKELETON_TYPE_SLUG:			modelName = "Slug";				break;
		case	SKELETON_TYPE_ANT:			modelName = "Ant";				break;
		case	SKELETON_TYPE_FIREANT:		modelName = "WingedFireAnt";	break;
		case	SKELETON_TYPE_WATERBUG:		modelName = "WaterBug";			break;
		case	SKELETON_TYPE_DRAGONFLY:	modelName = "DragonFly";		break;
		case	SKELETON_TYPE_PONDFISH:		modelName = "PondFish";			break;
		case	SKELETON_TYPE_MOSQUITO:		modelName = "Mosquito";			break;
		case	SKELETON_TYPE_FOOT:			modelName = "Foot";				break;
		case	SKELETON_TYPE_SPIDER:		modelName = "Spider";			break;
		case	SKELETON_TYPE_CATERPILLER:	modelName = "Caterpillar";		break;
		case	SKELETON_TYPE_FIREFLY:		modelName = "FireFly";			break;
		case	SKELETON_TYPE_BAT:			modelName = "Bat";				break;
		case	SKELETON_TYPE_LADYBUG:		modelName = "LadyBug";			break;
		case	SKELETON_TYPE_ROOTSWING:	modelName = "RootSwing";		break;
		case	SKELETON_TYPE_LARVA:		modelName = "Larva";			break;
		case	SKELETON_TYPE_FLYINGBEE:	modelName = "FlyingBee";		break;
		case	SKELETON_TYPE_WORKERBEE:	modelName = "WorkerBee";		break;
		case	SKELETON_TYPE_QUEENBEE:		modelName = "QueenBee";			break;
		case	SKELETON_TYPE_ROACH:		modelName = "Roach";			break;
		case	SKELETON_TYPE_BUDDY:		modelName = "Buddy";			break;
		case	SKELETON_TYPE_SKIPPY:		modelName = "Skippy";			break;
		case	SKELETON_TYPE_KINGANT:		modelName = "AntKing";			break;
		default:
				DoFatalAlert("LoadSkeleton: Unknown skeletonType!");
	}



	snprintf(pathBuf, sizeof(pathBuf), ":Skeletons:%s.skeleton", modelName);
	FSMakeFSSpec(gDataSpec.vRefNum, gDataSpec.parID, pathBuf, &fsSpecSkeleton);

	snprintf(pathBuf, sizeof(pathBuf), ":Skeletons:%s.3dmf", modelName);
	FSMakeFSSpec(gDataSpec.vRefNum, gDataSpec.parID, pathBuf, &fsSpec3DMF);



			/* OPEN THE FILE'S REZ FORK */

	fRefNum = FSpOpenResFile(&fsSpecSkeleton, fsRdPerm);
	GAME_ASSERT(fRefNum != -1);

	UseResFile(fRefNum);
	GAME_ASSERT(noErr == ResError());

			
			/* ALLOC MEMORY FOR SKELETON INFO STRUCTURE */
			
	skeleton = (SkeletonDefType *)AllocPtr(sizeof(SkeletonDefType));
	GAME_ASSERT(skeleton);


			/* READ SKELETON RESOURCES */
			
	ReadDataFromSkeletonFile(skeleton, &fsSpec3DMF);
	PrimeBoneData(skeleton);
	
			/* CLOSE REZ FILE */
			
	CloseResFile(fRefNum);
	UseResFile(gMainAppRezFile);
	UseResFile(gTextureRezfile);
		
		
	return(skeleton);
}


/************* READ DATA FROM SKELETON FILE *******************/
//
// Current rez file is set to the file. 
//

static void ReadDataFromSkeletonFile(
		SkeletonDefType *skeleton,
		const FSSpec *fsSpec3DMF)
{
Handle				hand;
short				i,k,j;
long				numJoints,numAnims,numKeyframes;
AnimEventType		*animEventPtr;
JointKeyframeType	*keyFramePtr;
SkeletonFile_Header_Type	*headerPtr;
short				version;
TQ3Point3D				*pointPtr;
SkeletonFile_AnimHeader_Type	*animHeaderPtr;


			/************************/
			/* READ HEADER RESOURCE */
			/************************/

	hand = GetResource('Hedr',1000);
	GAME_ASSERT(hand);
	headerPtr = (SkeletonFile_Header_Type *)*hand;
	BYTESWAP_STRUCTS(SkeletonFile_Header_Type, 1, headerPtr);
	version = headerPtr->version;
	GAME_ASSERT_MESSAGE(version == SKELETON_FILE_VERS_NUM, "Skeleton file has wrong version #");
	
	numAnims = skeleton->NumAnims = headerPtr->numAnims;			// get # anims in skeleton
	numJoints = skeleton->NumBones = headerPtr->numJoints;			// get # joints in skeleton
	ReleaseResource(hand);

	GAME_ASSERT(numJoints <= MAX_JOINTS);							// check for overload


				/*************************************/
				/* ALLOCATE MEMORY FOR SKELETON DATA */
				/*************************************/

	AllocSkeletonDefinitionMemory(skeleton);



		/********************************/
		/* 	LOAD THE REFERENCE GEOMETRY */
		/********************************/

#if 1
	// Source port change: original game used to resolve path to 3DMF via alias resource within skeleton rez fork.
	// Instead, we're forcing the 3DMF's filename (sans extension) to match the skeleton's.
	LoadBonesReferenceModel(fsSpec3DMF, skeleton);
#else
	AliasHandle				alias;
	FSSpec					target;
	alias = (AliasHandle)GetResource(rAliasType,1000);				// alias to geometry 3DMF file
	if (alias != nil)
	{
		iErr = ResolveAlias(fsSpec, alias, &target, &wasChanged);	// try to resolve alias
		GAME_ASSERT_MESSAGE(noErr == iErr, "Cannot find Skeleton's 3DMF file!");
		LoadBonesReferenceModel(&target,skeleton);
		ReleaseResource((Handle)alias);
	}
#endif



		/***********************************/
		/*  READ BONE DEFINITION RESOURCES */
		/***********************************/

	for (i=0; i < numJoints; i++)
	{
		File_BoneDefinitionType	*bonePtr;
		u_short					*indexPtr;

			/* READ BONE DATA */
			
		hand = GetResource('Bone',1000+i);
		GAME_ASSERT(hand);
		HLock(hand);
		bonePtr = (File_BoneDefinitionType *)*hand;
		BYTESWAP_STRUCTS(File_BoneDefinitionType, 1, bonePtr);

			/* COPY BONE DATA INTO ARRAY */
		
		skeleton->Bones[i].parentBone = bonePtr->parentBone;								// index to previous bone
		skeleton->Bones[i].coord = bonePtr->coord;											// absolute coord (not relative to parent!)
		skeleton->Bones[i].numPointsAttachedToBone = bonePtr->numPointsAttachedToBone;		// # vertices/points that this bone has
		skeleton->Bones[i].numNormalsAttachedToBone = bonePtr->numNormalsAttachedToBone;	// # vertex normals this bone has		
		ReleaseResource(hand);

			/* ALLOC THE POINT & NORMALS SUB-ARRAYS */
				
		skeleton->Bones[i].pointList = (u_short *)AllocPtr(sizeof(u_short) * (int)skeleton->Bones[i].numPointsAttachedToBone);
		GAME_ASSERT(skeleton->Bones[i].pointList);

		skeleton->Bones[i].normalList = (u_short *)AllocPtr(sizeof(u_short) * (int)skeleton->Bones[i].numNormalsAttachedToBone);
		GAME_ASSERT(skeleton->Bones[i].normalList);

			/* READ POINT INDEX ARRAY */
			
		hand = GetResource('BonP',1000+i);
		GAME_ASSERT(hand);
		HLock(hand);
		indexPtr = (u_short *)(*hand);
		ByteswapInts(sizeof(u_short), skeleton->Bones[i].numPointsAttachedToBone, indexPtr);
			
			/* COPY POINT INDEX ARRAY INTO BONE STRUCT */

		for (j=0; j < skeleton->Bones[i].numPointsAttachedToBone; j++)
			skeleton->Bones[i].pointList[j] = indexPtr[j];
		ReleaseResource(hand);


			/* READ NORMAL INDEX ARRAY */
			
		hand = GetResource('BonN',1000+i);
		GAME_ASSERT(hand);
		HLock(hand);
		indexPtr = (u_short *)(*hand);
		ByteswapInts(sizeof(u_short), skeleton->Bones[i].numNormalsAttachedToBone, indexPtr);
			
			/* COPY NORMAL INDEX ARRAY INTO BONE STRUCT */

		for (j=0; j < skeleton->Bones[i].numNormalsAttachedToBone; j++)
			skeleton->Bones[i].normalList[j] = indexPtr[j];
		ReleaseResource(hand);
						
	}
	
	
		/*******************************/
		/* READ POINT RELATIVE OFFSETS */
		/*******************************/
		//
		// The "relative point offsets" are the only things
		// which do not get rebuilt in the ModelDecompose function.
		// We need to restore these manually.
	
	hand = GetResource('RelP', 1000);
	GAME_ASSERT(hand);
	HLock(hand);
	pointPtr = (TQ3Point3D *)*hand;
	
	if ((long)(GetHandleSize(hand) / sizeof(TQ3Point3D)) != skeleton->numDecomposedPoints)
		DoFatalAlert("# of points in Reference Model has changed!");
	else
	{
		BYTESWAP_STRUCTS(TQ3Point3D, skeleton->numDecomposedPoints, pointPtr);
		for (i = 0; i < skeleton->numDecomposedPoints; i++)
			skeleton->decomposedPointList[i].boneRelPoint = pointPtr[i];
	}

	ReleaseResource(hand);
	
	
			/*********************/
			/* READ ANIM INFO   */
			/*********************/
			
	for (i=0; i < numAnims; i++)
	{
				/* READ ANIM HEADER */

		hand = GetResource('AnHd',1000+i);
		GAME_ASSERT(hand);
		HLock(hand);
		animHeaderPtr = (SkeletonFile_AnimHeader_Type *)*hand;
		BYTESWAP_STRUCTS(SkeletonFile_AnimHeader_Type, 1, animHeaderPtr);

		skeleton->NumAnimEvents[i] = animHeaderPtr->numAnimEvents;			// copy # anim events in anim	
		ReleaseResource(hand);

			/* READ ANIM-EVENT DATA */
			
		hand = GetResource('Evnt',1000+i);
		GAME_ASSERT(hand);
		animEventPtr = (AnimEventType *)*hand;
		BYTESWAP_STRUCTS(AnimEventType, skeleton->NumAnimEvents[i], animEventPtr);
		for (j=0;  j < skeleton->NumAnimEvents[i]; j++)
			skeleton->AnimEventsList[i][j] = *animEventPtr++;
		ReleaseResource(hand);		


			/* READ # KEYFRAMES PER JOINT IN EACH ANIM */
					
		hand = GetResource('NumK',1000+i);									// read array of #'s for this anim
		GAME_ASSERT(hand);
		// (no need to byteswap, it's an array of chars)
		for (j=0; j < numJoints; j++)
			skeleton->JointKeyframes[j].numKeyFrames[i] = (*hand)[j];
		ReleaseResource(hand);
	}


	for (j=0; j < numJoints; j++)
	{
				/* ALLOC 2D ARRAY FOR KEYFRAMES */
				
		Alloc_2d_array(JointKeyframeType,skeleton->JointKeyframes[j].keyFrames,	numAnims,MAX_KEYFRAMES);
		
		GAME_ASSERT((skeleton->JointKeyframes[j].keyFrames) && (skeleton->JointKeyframes[j].keyFrames[0]));

					/* READ THIS JOINT'S KF'S FOR EACH ANIM */
					
		for (i=0; i < numAnims; i++)								
		{
			numKeyframes = skeleton->JointKeyframes[j].numKeyFrames[i];					// get actual # of keyframes for this joint
			GAME_ASSERT(numKeyframes <= MAX_KEYFRAMES);

					/* READ A JOINT KEYFRAME */
					
			hand = GetResource('KeyF',1000+(i*100)+j);
			GAME_ASSERT(hand);
			keyFramePtr = (JointKeyframeType *)*hand;
			BYTESWAP_STRUCTS(JointKeyframeType, numKeyframes, keyFramePtr);
			for (k = 0; k < numKeyframes; k++)												// copy this joint's keyframes for this anim
				skeleton->JointKeyframes[j].keyFrames[i][k] = *keyFramePtr++;
			ReleaseResource(hand);		
		}
	}
	
}

#pragma mark -

/**************** OPEN GAME FILE **********************/

void	OpenGameFile(Str255 filename,short *fRefNumPtr, Str255 errString)
{
OSErr		iErr;
FSSpec		spec;
Str255		s;

				/* FIRST SEE IF WE CAN GET IT OFF OF DEFAULT VOLUME */

	iErr = FSMakeFSSpec(gDataSpec.vRefNum, gDataSpec.parID, filename, &spec);
	if (iErr == noErr)
	{
		iErr = FSpOpenDF(&spec, fsRdPerm, fRefNumPtr);
		if (iErr == noErr)
			return;
	}

	if (iErr == fnfErr)
		DoFatalAlert2(errString,"FILE NOT FOUND.");
	else
	{
		NumToStringC(iErr,s);
		DoFatalAlert2(errString,s);
	}
}




/******************** LOAD PREFS **********************/
//
// Load in standard preferences
//

OSErr LoadPrefs(PrefsType *prefBlock)
{
OSErr		iErr;
short		refNum;
FSSpec		file;
long		count;
char		header[PREFS_HEADER_LENGTH + 1];
				
				/*************/
				/* READ FILE */
				/*************/
					
	FSMakeFSSpec(gPrefsFolderVRefNum, gPrefsFolderDirID, PREFS_FILE_NAME, &file);
	iErr = FSpOpenDF(&file, fsRdPerm, &refNum);	
	if (iErr)
		return(iErr);

				/* READ HEADER */

	count = PREFS_HEADER_LENGTH;
	iErr = FSRead(refNum, &count, header);
	header[PREFS_HEADER_LENGTH] = '\0';
	if (iErr ||
		count != PREFS_HEADER_LENGTH ||
		0 != strncmp(header, PREFS_HEADER_STRING, PREFS_HEADER_LENGTH))
	{
		FSClose(refNum);
		return badFileFormat;
	}

				/* READ PREFS STRUCT */

	count = sizeof(PrefsType);
	iErr = FSRead(refNum, &count,  (Ptr)prefBlock);		// read data from file
	if (iErr ||
		count != sizeof(PrefsType))
	{
		FSClose(refNum);			
		return(badFileFormat);
	}
	
	FSClose(refNum);			


				/* SAFETY CHECKS */

	if (prefBlock->mouseSensitivityLevel >= NUM_MOUSE_SENSITIVITY_LEVELS)
	{
		DoAlert("Illegal mouse sensitivity level in prefs!");
		prefBlock->mouseSensitivityLevel = DEFAULT_MOUSE_SENSITIVITY_LEVEL;
	}

	
	return(noErr);
}


/******************** SAVE PREFS **********************/

void SavePrefs(PrefsType *prefs)
{
FSSpec				file;
OSErr				iErr;
short				refNum;
long				count;
						
				/* CREATE BLANK FILE */
				
	FSMakeFSSpec(gPrefsFolderVRefNum, gPrefsFolderDirID, PREFS_FILE_NAME, &file);
	FSpDelete(&file);															// delete any existing file
	iErr = FSpCreate(&file, 'BalZ', 'Pref', smSystemScript);					// create blank file
	if (iErr)
		return;

				/* OPEN FILE */
					
	iErr = FSpOpenDF(&file, fsRdWrPerm, &refNum);
	if (iErr)
	{
		FSpDelete(&file);
		return;
	}


				/* WRITE HEADER */

	count = PREFS_HEADER_LENGTH;
	iErr = FSWrite(refNum, &count, (Ptr) PREFS_HEADER_STRING);
	GAME_ASSERT(iErr == noErr);
	GAME_ASSERT(count == PREFS_HEADER_LENGTH);


				/* WRITE DATA */

	count = sizeof(PrefsType);
	iErr = FSWrite(refNum, &count, (Ptr) prefs);
	GAME_ASSERT(iErr == noErr);
	GAME_ASSERT(count == sizeof(PrefsType));

	FSClose(refNum);
}

#pragma mark -

/******************* GET FSSPEC FOR SAVE GAME SLOT *********************/

static FSSpec MakeSaveGameFSSpec(int slot)
{
	char			path[128];
	FSSpec			spec;

	GAME_ASSERT(slot >= 0);
	GAME_ASSERT(slot < NUM_SAVE_FILES);

	snprintf(path, sizeof(path), ":Bugdom:Save%c", slot + 'A');
	FSMakeFSSpec(gPrefsFolderVRefNum, gPrefsFolderDirID, path, &spec);

	return spec;
}


/***************************** SAVE GAME ********************************/

void SaveGame(int slot)
{
SaveGameType 	saveData;
short			fRefNum;
FSSpec			spec;
OSErr			err;

			/*************************/
			/* CREATE SAVE GAME DATA */
			/*************************/	

	saveData.version		= SAVE_GAME_VERSION;				// save file version #
	saveData.score 			= gScore;
	saveData.realLevel		= gRealLevel+1;						// save @ beginning of next level
	saveData.numLives 		= gNumLives;
	saveData.health			= gMyHealth;
	saveData.ballTimer		= gBallTimer;
	saveData.numGoldClovers	= gNumGoldClovers;
	saveData.timestamp		= time(NULL);

			/* CREATE & OPEN THE DATA FORK */

	spec = MakeSaveGameFSSpec(slot);

	err = FSpCreate(&spec,'BalZ','BSav',0);
	if (err != noErr)
	{
		DoAlert("Error creating save file");
		return;
	}

	err = FSpOpenDF(&spec, fsWrPerm, &fRefNum);
	if (err != noErr)
	{
		DoAlert("Error opening save file for writing");
		return;
	}

			/* WRITE TO FILE */

	long count = sizeof(SaveGameType);
	err = FSWrite(fRefNum, &count, (Ptr) &saveData);

			/* CLOSE FILE */

	FSClose(fRefNum);

			/* CHECK THAT WRITING WENT OK */

	if (err != noErr || count != sizeof(SaveGameType))
	{
		DoAlert("Error writing save file");
		return;
	}
}


/***************************** LOAD SAVED GAME ********************************/

OSErr GetSaveGameData(int slot, SaveGameType* saveData)
{
short			fRefNum;
OSErr			err;

	FSSpec spec = MakeSaveGameFSSpec(slot);

			/* OPEN THE DATA FORK */

	err = FSpOpenDF(&spec, fsRdPerm, &fRefNum);
	if (err != noErr)
		return err;

			/* READ STRUCT FROM FILE */

	long count = sizeof(SaveGameType);
	err = FSRead(fRefNum, &count, (Ptr) saveData);
	FSClose(fRefNum);

	if (err != noErr)
		return err;

			/* CHECK INTEGRITY */

	if (count != sizeof(SaveGameType))
		return badFileFormat;

	if (saveData->version != SAVE_GAME_VERSION)
		return badFileFormat;

			/* IT'S ALL GOOD */

	return noErr;
}


OSErr LoadSavedGame(int slot)
{
OSErr err;
SaveGameType saveData;

	gCurrentSaveSlot = slot;

	err = GetSaveGameData(slot, &saveData);

	if (err != noErr)
	{
		InitInventoryForGame();
		gRealLevel = 0;
		return noErr;
	}

			/**********************/
			/* USE SAVE GAME DATA */
			/**********************/	

	gScore 			= saveData.score;
	gRealLevel 		= saveData.realLevel;
	gMyHealth		= saveData.health;
	gNumLives		= saveData.numLives;
	gBallTimer		= saveData.ballTimer;
	gNumGoldClovers = saveData.numGoldClovers;

	gRestoringSavedGame = true;

	return(noErr);
}

OSErr DeleteSavedGame(int slot)
{

	FSSpec spec = MakeSaveGameFSSpec(slot);
	return FSpDelete(&spec);
}



#pragma mark -

/******************* LOAD PLAYFIELD *******************/

void LoadPlayfield(FSSpec *specPtr)
{
short	fRefNum;

			
				/* OPEN THE REZ-FORK */
			
	fRefNum = FSpOpenResFile(specPtr,fsRdPerm);
	GAME_ASSERT(fRefNum != -1);
	UseResFile(fRefNum);
	
	
			/* READ PLAYFIELD RESOURCES */
						
	ReadDataFromPlayfieldFile(specPtr);
	
	
			/* CLOSE REZ FILE */
			
	CloseResFile(fRefNum);
	UseResFile(gMainAppRezFile);
	UseResFile(gTextureRezfile);
	
		
	
				/***********************/
				/* DO ADDITIONAL SETUP */
				/***********************/
	
	gTerrainTileWidth = (gTerrainTileWidth/SUPERTILE_SIZE)*SUPERTILE_SIZE;		// round size down to nearest supertile multiple
	gTerrainTileDepth = (gTerrainTileDepth/SUPERTILE_SIZE)*SUPERTILE_SIZE;	
	
	gTerrainUnitWidth = gTerrainTileWidth*TERRAIN_POLYGON_SIZE;					// calc world unit dimensions of terrain
	gTerrainUnitDepth = gTerrainTileDepth*TERRAIN_POLYGON_SIZE;
	gNumSuperTilesDeep = gTerrainTileDepth/SUPERTILE_SIZE;						// calc size in supertiles
	gNumSuperTilesWide = gTerrainTileWidth/SUPERTILE_SIZE;	
	
			/* PRECALC THE TILE SPLIT MODE MATRIX */
			
	CalculateSplitModeMatrix();

		
	BuildTerrainItemList();	

	
				/* INITIALIZE CURRENT SCROLL SETTINGS */

	InitCurrentScrollSettings();
}


/********************** READ DATA FROM PLAYFIELD FILE ************************/

static void ReadDataFromPlayfieldFile(FSSpec *specPtr)
{
Handle					hand;
PlayfieldHeaderType		**header;
long					i,row,col,numLayers;
float					yScale;
short					**xlateTableHand,*xlateTbl;

#pragma unused (specPtr)
	
	if (gDoCeiling)									// see if need to read in ceiling data
		numLayers = 2;
	else
		numLayers = 1;

			/************************/
			/* READ HEADER RESOURCE */
			/************************/

	hand = GetResource('Hedr',1000);
	GAME_ASSERT(hand);

	BYTESWAP_STRUCT_ARRAY_HANDLE_2(PlayfieldHeaderType, 1, hand);
	header = (PlayfieldHeaderType **)hand;
	gNumTerrainItems		= (**header).numItems;
	gTerrainTileWidth		= (**header).mapWidth;
	gTerrainTileDepth		= (**header).mapHeight;	
	gNumTerrainTextureTiles	= (**header).numTilesInList;	
	g3DTileSize				= (**header).tileSize;
	g3DMinY					= (**header).minY;
	g3DMaxY					= (**header).maxY;
	gNumSplines				= (**header).numSplines;
	gNumFences				= (**header).numFences;
	ReleaseResource(hand);

	GAME_ASSERT(gNumTerrainTextureTiles <= MAX_TERRAIN_TILES);

			/**************************/
			/* TILE RELATED RESOURCES */
			/**************************/


			/* READ TILE IMAGE DATA */

	hand = GetResource('Timg',1000);
	GAME_ASSERT(hand);
	{
		DetachResource(hand);							// lets keep this data around
		BYTESWAP_SCALAR_ARRAY_HANDLE(u_short, hand);
		gTileDataHandle = (u_short **)hand;
	}


			/* READ TILE->IMAGE XLATE TABLE */

	hand = GetResource('Xlat',1000);
	{
		GAME_ASSERT(hand);
		DetachResource(hand);
		HLockHi(hand);									// hold on to this rez until we're done reading maps below
		BYTESWAP_SCALAR_ARRAY_HANDLE(short, hand);
		xlateTableHand = (short **)hand;
		xlateTbl = *xlateTableHand;
	}


			/*******************************/
			/* MAP LAYER RELATED RESOURCES */
			/*******************************/

			/* READ FLOOR MAP */
				
	Alloc_2d_array(u_short, gFloorMap, gTerrainTileDepth, gTerrainTileWidth);		// alloc 2D array for floor map
	
	
	hand = GetResource('Layr',1000);												// load map from rez
	GAME_ASSERT(hand);
																					// copy rez into 2D array
	{
		BYTESWAP_SCALAR_ARRAY_HANDLE_2(u_short, gTerrainTileDepth*gTerrainTileWidth, hand);
		u_short	*src = (u_short*) *hand;
		for (row = 0; row < gTerrainTileDepth; row++)
			for (col = 0; col < gTerrainTileWidth; col++)
			{
				u_short	tile, imageNum;
				
				tile = *src++;														// get original tile with all bits
				imageNum = xlateTbl[tile & TILENUM_MASK];							// get image # from xlate table
				gFloorMap[row][col] = (tile&(~TILENUM_MASK)) | imageNum;			// insert image # into bitfield
			}
		ReleaseResource(hand);
	}		


	if (gDoCeiling)
	{
				/* READ CEILING MAP */
					
		Alloc_2d_array(u_short, gCeilingMap, gTerrainTileDepth, gTerrainTileWidth);		// alloc 2D array for map
		
		hand = GetResource('Layr',1001);												// load map from rez
		GAME_ASSERT(hand);
																						// copy rez into 2D array
		{
			BYTESWAP_SCALAR_ARRAY_HANDLE_2(u_short, gTerrainTileDepth*gTerrainTileWidth, hand);
			u_short	*src = (u_short*) *hand;
			for (row = 0; row < gTerrainTileDepth; row++)
				for (col = 0; col < gTerrainTileWidth; col++)
				{
					u_short	tile, imageNum;

					tile = *src++;														// get original tile with all bits
					imageNum = xlateTbl[tile & TILENUM_MASK];							// get image # from xlate table
					gCeilingMap[row][col] = (tile&(~TILENUM_MASK)) | imageNum;			// insert image # into bitfield
				}
			ReleaseResource(hand);
		}
	}
	
	ReleaseResource((Handle)xlateTableHand);								// we dont need the xlate table anymore


			/* READ HEIGHT DATA MATRIX */
	
	yScale = TERRAIN_POLYGON_SIZE / g3DTileSize;						// need to scale original geometry units to game units
	
	Alloc_2d_array(TerrainYCoordType, gMapYCoords, gTerrainTileDepth+1, gTerrainTileWidth+1);	// alloc 2D array for map
	
	for (i = 0; i < numLayers; i++)
	{		
		hand = GetResource('YCrd',1000+i);
		GAME_ASSERT(hand);
		{
			BYTESWAP_SCALAR_ARRAY_HANDLE_2(float, (gTerrainTileDepth+1)*(gTerrainTileWidth+1), hand);
			float* src = (float*) *hand;
			for (row = 0; row <= gTerrainTileDepth; row++)
				for (col = 0; col <= gTerrainTileWidth; col++)
					gMapYCoords[row][col].layerY[i] = *src++ * yScale;
			ReleaseResource(hand);
		}
	}			


			/* READ VERTEX COLOR MATRIX */
			
	
	for (i = 0; i < numLayers; i++)
	{		
		Alloc_2d_array(u_short, gVertexColors[i], gTerrainTileDepth+1, gTerrainTileWidth+1);	// alloc 2D array for map
		GAME_ASSERT(gVertexColors[i]);

		hand = GetResource('Vcol',1000+i);
		GAME_ASSERT(hand);
		{
			BYTESWAP_SCALAR_ARRAY_HANDLE_2(u_short, (gTerrainTileDepth+1)*(gTerrainTileWidth+1), hand);
			u_short* src = (u_short*) *hand;
			for (row = 0; row <= gTerrainTileDepth; row++)
				for (col = 0; col <= gTerrainTileWidth; col++)
					gVertexColors[i][row][col] = *src++;
			ReleaseResource(hand);
		}
	}			

			/* READ SPLIT MODE MATRIX */
	
	Alloc_2d_array(TerrainInfoMatrixType, gMapInfoMatrix, gTerrainTileDepth, gTerrainTileWidth);	// alloc 2D array for map
	
	for (i = 0; i < numLayers; i++)												// floor & ceiling
	{		
		hand = GetResource('Splt',1000+i);
		GAME_ASSERT(hand);
		{
			Byte* src = (Byte*)*hand;  // no need to byteswap - it's just bytes
			for (row = 0; row < gTerrainTileDepth; row++)						
				for (col = 0; col < gTerrainTileWidth; col++)
					gMapInfoMatrix[row][col].splitMode[i] = *src++;
			ReleaseResource(hand);
		}
	}

	
				/**************************/
				/* ITEM RELATED RESOURCES */
				/**************************/
	
				/* READ ITEM LIST */
				
	hand = GetResource('Itms',1000);
	GAME_ASSERT(hand);
	{
		DetachResource(hand);							// lets keep this data around		
		HLockHi(hand);									// LOCK this one because we have the lookup table into this
		gMasterItemList = (TerrainItemEntryType **)hand;
		BYTESWAP_STRUCT_ARRAY_HANDLE_2(TerrainItemEntryType, gNumTerrainItems, gMasterItemList);
	}
		
	
			/****************************/
			/* SPLINE RELATED RESOURCES */
			/****************************/
	
			/* READ SPLINE LIST */
			
	hand = GetResource('Spln',1000);
	if (hand)
	{	
		DetachResource(hand);
		HLockHi(hand);

		// SOURCE PORT NOTE: we have to convert this structure manually,
		// because the original contains 4-byte pointers
		BYTESWAP_STRUCT_ARRAY_HANDLE_2(File_SplineDefType, gNumSplines, hand);

		gSplineList = (SplineDefType **) NewHandleClear(gNumSplines * sizeof(SplineDefType));

		for (i = 0; i < gNumSplines; i++)
		{
			const File_SplineDefType*	srcSpline = &(*((File_SplineDefType **) hand))[i];
			SplineDefType*				dstSpline = &(*gSplineList)[i];

			dstSpline->numItems		= srcSpline->numItems;
			dstSpline->numNubs		= srcSpline->numNubs;
			dstSpline->numPoints	= srcSpline->numPoints;
			dstSpline->bBox			= srcSpline->bBox;
		}

		DisposeHandle(hand);
	}
	else
	{
		gNumSplines = 0;
		gSplineList = nil;
	}

	
			/* READ SPLINE POINT LIST */
			
	for (i = 0; i < gNumSplines; i++)
	{
		// Level 2's spline #16 has 0 points. Skip the byteswapping, but do alloc an empty handle, which the game expects.
		if ((*gSplineList)[i].numPoints == 0)
		{
			printf("WARNING: Spline #%ld has 0 points\n", i);
			(*gSplineList)[i].pointList = (SplinePointType**) AllocHandle(0);
			continue;
		}

		hand = GetResource('SpPt',1000+i);
		GAME_ASSERT(hand);
		{
			DetachResource(hand);
			HLockHi(hand);
			BYTESWAP_STRUCT_ARRAY_HANDLE_2(SplinePointType, (*gSplineList)[i].numPoints, hand);
			(*gSplineList)[i].pointList = (SplinePointType **)hand;
		}
	}


			/* READ SPLINE ITEM LIST */
			
	for (i = 0; i < gNumSplines; i++)
	{
		// Level 2's spline #16 has 0 items. Skip the byteswapping, but do alloc an empty handle, which the game expects.
		if ((*gSplineList)[i].numItems == 0)
		{
			printf("WARNING: Spline #%ld has 0 items\n", i);
			(*gSplineList)[i].itemList = (SplineItemType**) AllocHandle(0);
			continue;
		}

		hand = GetResource('SpIt',1000+i);
		GAME_ASSERT(hand);
		{
			DetachResource(hand);
			HLockHi(hand);
			BYTESWAP_STRUCT_ARRAY_HANDLE_2(SplineItemType, (*gSplineList)[i].numItems, hand);
			(*gSplineList)[i].itemList = (SplineItemType **)hand;
		}
	}
	
	
			/****************************/
			/* FENCE RELATED RESOURCES */
			/****************************/
	
			/* READ FENCE LIST */
			
	hand = GetResource('Fenc',1000);
	if (hand)
	{	
		File_FenceDefType *inData;

		gFenceList = (FenceDefType *)AllocPtr(sizeof(FenceDefType) * gNumFences);	// alloc new ptr for fence data
		GAME_ASSERT(gFenceList);

		BYTESWAP_STRUCT_ARRAY_HANDLE_2(File_FenceDefType, gNumFences, hand);
		inData = (File_FenceDefType *)*hand;							// get ptr to input fence list
		
		for (i = 0; i < gNumFences; i++)								// copy data from rez to new list
		{
			gFenceList[i].type 		= inData[i].type;
			gFenceList[i].numNubs 	= inData[i].numNubs;
			gFenceList[i].nubList 	= nil;
			gFenceList[i].bBox.top		= inData[i].bBox.top;
			gFenceList[i].bBox.bottom	= inData[i].bBox.bottom;
			gFenceList[i].bBox.left		= inData[i].bBox.left;
			gFenceList[i].bBox.right	= inData[i].bBox.right;
		}		
		ReleaseResource(hand);
	}
	else
	{
		gNumFences = 0;
		gFenceList = nil;
	}

	
			/* READ FENCE NUB LIST */
			
	for (i = 0; i < gNumFences; i++)
	{
		hand = GetResource('FnNb',1000+i);
		GAME_ASSERT(hand);
		{
			DetachResource(hand);
			HLockHi(hand);
			BYTESWAP_STRUCT_ARRAY_HANDLE_2(FencePointType, gFenceList[i].numNubs, hand);
			gFenceList[i].nubList = (FencePointType **)hand;
		}
	}
}

#pragma mark -

/************************** LOAD LEVEL ART ***************************/

void LoadLevelArt(void)
{
FSSpec	spec;

			/* LOAD GLOBAL STUFF */

	FSMakeFSSpec(gDataSpec.vRefNum, gDataSpec.parID, ":models:Global_Models1.3dmf", &spec);
	LoadGrouped3DMF(&spec,MODEL_GROUP_GLOBAL1);	
	FSMakeFSSpec(gDataSpec.vRefNum, gDataSpec.parID, ":models:Global_Models2.3dmf", &spec);
	LoadGrouped3DMF(&spec,MODEL_GROUP_GLOBAL2);	
			
	FSMakeFSSpec(gDataSpec.vRefNum, gDataSpec.parID, ":Audio:Main.sounds", &spec);
	LoadSoundBank(&spec, SOUND_BANK_DEFAULT);

	LoadASkeleton(SKELETON_TYPE_ME);			
	LoadASkeleton(SKELETON_TYPE_LADYBUG);			
	LoadASkeleton(SKELETON_TYPE_BUDDY);			
	
			/*****************************/
			/* LOAD LEVEL SPECIFIC STUFF */
			/*****************************/
			
	switch(gLevelType)
	{
				/***********************/
				/* LEVEL 1: THE GARDEN */
				/***********************/
				
		case	LEVEL_TYPE_LAWN:
				if (gAreaNum == 0)
					FSMakeFSSpec(gDataSpec.vRefNum, gDataSpec.parID, ":terrain:Training.ter", &spec);
				else
					FSMakeFSSpec(gDataSpec.vRefNum, gDataSpec.parID, ":terrain:Lawn.ter", &spec);
				
				LoadPlayfield(&spec);

				/* LOAD MODELS */
						
				FSMakeFSSpec(gDataSpec.vRefNum, gDataSpec.parID, ":models:Lawn_Models1.3dmf", &spec);
				LoadGrouped3DMF(&spec,MODEL_GROUP_LEVELSPECIFIC);	
				FSMakeFSSpec(gDataSpec.vRefNum, gDataSpec.parID, ":models:Lawn_Models2.3dmf", &spec);
				LoadGrouped3DMF(&spec,MODEL_GROUP_LEVELSPECIFIC2);	
				
				
				/* LOAD SKELETON FILES */
				
				LoadASkeleton(SKELETON_TYPE_BOXERFLY);			
				LoadASkeleton(SKELETON_TYPE_SLUG);			
				LoadASkeleton(SKELETON_TYPE_ANT);			

				/* LOAD SOUNDS */
				
				FSMakeFSSpec(gDataSpec.vRefNum, gDataSpec.parID, ":Audio:Lawn.sounds", &spec);
				LoadSoundBank(&spec, SOUND_BANK_LEVELSPECIFIC);
				break;


				/*****************/
				/* LEVEL 2: POND */
				/*****************/
				
		case	LEVEL_TYPE_POND:
				FSMakeFSSpec(gDataSpec.vRefNum, gDataSpec.parID, ":terrain:Pond.ter", &spec);
				LoadPlayfield(&spec);

				/* LOAD MODELS */
						
				FSMakeFSSpec(gDataSpec.vRefNum, gDataSpec.parID, ":models:Pond_Models.3dmf", &spec);
				LoadGrouped3DMF(&spec,MODEL_GROUP_LEVELSPECIFIC);	
				
				
				/* LOAD SKELETON FILES */
				
				LoadASkeleton(SKELETON_TYPE_MOSQUITO);			
				LoadASkeleton(SKELETON_TYPE_WATERBUG);			
				LoadASkeleton(SKELETON_TYPE_PONDFISH);			
				LoadASkeleton(SKELETON_TYPE_SKIPPY);			
				LoadASkeleton(SKELETON_TYPE_SLUG);			


				/* LOAD SOUNDS */
				
				FSMakeFSSpec(gDataSpec.vRefNum, gDataSpec.parID, ":Audio:Pond.sounds", &spec);
				LoadSoundBank(&spec, SOUND_BANK_LEVELSPECIFIC);
				break;


				/*******************/
				/* LEVEL 3: FOREST */
				/*******************/
				
		case	LEVEL_TYPE_FOREST:
				if (gAreaNum == 0)
					FSMakeFSSpec(gDataSpec.vRefNum, gDataSpec.parID, ":terrain:Beach.ter", &spec);
				else
					FSMakeFSSpec(gDataSpec.vRefNum, gDataSpec.parID, ":terrain:Flight.ter", &spec);
				LoadPlayfield(&spec);

				/* LOAD MODELS */
						
				FSMakeFSSpec(gDataSpec.vRefNum, gDataSpec.parID, ":models:Forest_Models.3dmf", &spec);
				LoadGrouped3DMF(&spec,MODEL_GROUP_LEVELSPECIFIC);	
				
				
				/* LOAD SKELETON FILES */
				
				LoadASkeleton(SKELETON_TYPE_DRAGONFLY);			
				LoadASkeleton(SKELETON_TYPE_FOOT);	
				LoadASkeleton(SKELETON_TYPE_SPIDER);	
				LoadASkeleton(SKELETON_TYPE_CATERPILLER);	
				LoadASkeleton(SKELETON_TYPE_BAT);	
				LoadASkeleton(SKELETON_TYPE_FLYINGBEE);			
				LoadASkeleton(SKELETON_TYPE_ANT);			
				
				/* LOAD SOUNDS */
				
				FSMakeFSSpec(gDataSpec.vRefNum, gDataSpec.parID, ":Audio:Forest.sounds", &spec);
				LoadSoundBank(&spec, SOUND_BANK_LEVELSPECIFIC);
						
				break;


				/*************************/
				/* LEVEL 4:  BEE HIVE    */
				/*************************/
				
		case	LEVEL_TYPE_HIVE:
			
				if (gAreaNum == 0)
					FSMakeFSSpec(gDataSpec.vRefNum, gDataSpec.parID, ":terrain:BeeHive.ter", &spec);
				else
					FSMakeFSSpec(gDataSpec.vRefNum, gDataSpec.parID, ":terrain:QueenBee.ter", &spec);
				LoadPlayfield(&spec);

				/* LOAD MODELS */
						
				FSMakeFSSpec(gDataSpec.vRefNum, gDataSpec.parID, ":models:BeeHive_Models.3dmf", &spec);
				LoadGrouped3DMF(&spec,MODEL_GROUP_LEVELSPECIFIC);	
				
				
				/* LOAD SKELETON FILES */
				
				LoadASkeleton(SKELETON_TYPE_LARVA);			
				LoadASkeleton(SKELETON_TYPE_FLYINGBEE);			
				LoadASkeleton(SKELETON_TYPE_WORKERBEE);			
				LoadASkeleton(SKELETON_TYPE_QUEENBEE);			

				
				/* LOAD SOUNDS */
				
				FSMakeFSSpec(gDataSpec.vRefNum, gDataSpec.parID, ":Audio:Hive.sounds", &spec);
				LoadSoundBank(&spec, SOUND_BANK_LEVELSPECIFIC);
				
				break;


				/*******************/
				/* LEVEL 5:  NIGHT */
				/*******************/
				
		case	LEVEL_TYPE_NIGHT:
				FSMakeFSSpec(gDataSpec.vRefNum, gDataSpec.parID, ":terrain:Night.ter", &spec);
				LoadPlayfield(&spec);

				/* LOAD MODELS */
						
				FSMakeFSSpec(gDataSpec.vRefNum, gDataSpec.parID, ":models:Night_Models.3dmf", &spec);
				LoadGrouped3DMF(&spec,MODEL_GROUP_LEVELSPECIFIC);	
				
				
				/* LOAD SKELETON FILES */
				
				LoadASkeleton(SKELETON_TYPE_FIREANT);			
				LoadASkeleton(SKELETON_TYPE_FIREFLY);			
				LoadASkeleton(SKELETON_TYPE_CATERPILLER);	
				LoadASkeleton(SKELETON_TYPE_SLUG);	
				LoadASkeleton(SKELETON_TYPE_ROACH);	
				LoadASkeleton(SKELETON_TYPE_ANT);	

				
				/* LOAD SOUNDS */
				
				FSMakeFSSpec(gDataSpec.vRefNum, gDataSpec.parID, ":Audio:Night.sounds", &spec);
				LoadSoundBank(&spec, SOUND_BANK_LEVELSPECIFIC);
				break;

	
				/*************************/
				/* LEVEL 6:  ANT HILL    */
				/*************************/
				
		case	LEVEL_TYPE_ANTHILL:
				if (gAreaNum == 0)
					FSMakeFSSpec(gDataSpec.vRefNum, gDataSpec.parID, ":terrain:AntHill.ter", &spec);
				else
					FSMakeFSSpec(gDataSpec.vRefNum, gDataSpec.parID, ":terrain:AntKing.ter", &spec);
				LoadPlayfield(&spec);

				/* LOAD MODELS */
						
				FSMakeFSSpec(gDataSpec.vRefNum, gDataSpec.parID, ":models:AntHill_Models.3dmf", &spec);
				LoadGrouped3DMF(&spec,MODEL_GROUP_LEVELSPECIFIC);	
				
				
				/* LOAD SKELETON FILES */
				
				if (gRealLevel == LEVEL_NUM_ANTKING)
					LoadASkeleton(SKELETON_TYPE_KINGANT);			
					
				LoadASkeleton(SKELETON_TYPE_SLUG);			
				LoadASkeleton(SKELETON_TYPE_ANT);			
				LoadASkeleton(SKELETON_TYPE_FIREANT);			
				LoadASkeleton(SKELETON_TYPE_ROOTSWING);			
				LoadASkeleton(SKELETON_TYPE_ROACH);	

				/* LOAD SOUNDS */
				
				FSMakeFSSpec(gDataSpec.vRefNum, gDataSpec.parID, ":Audio:AntHill.sounds", &spec);
				LoadSoundBank(&spec, SOUND_BANK_LEVELSPECIFIC);
				break;

		default:
				DoFatalAlert("LoadLevelArt: unsupported level #");
	}
	
	
			/* CAST SHADOWS */
			
	DoItemShadowCasting();
}
