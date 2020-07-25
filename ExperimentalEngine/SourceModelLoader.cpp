#include "PCH.hpp"
#include "Engine.hpp"
#include "GlmStreamOps.hpp"
#include <iostream>

#pragma pack(push, 1)

#define MAX_NUM_BONES_PER_VERT 3
#define MAX_NUM_LODS 8
// 16 bytes
struct mstudioboneweight_t {
	float	weight[MAX_NUM_BONES_PER_VERT];
	char	bone[MAX_NUM_BONES_PER_VERT];
	byte	numbones;
};

struct mstudio_modelvertexdata_t {
	// base of external vertex data stores
	// These are void* in Source, but Source is 32 bit and we're 64 bit!
	int pVertexData; 
	int pTangentData; 
};

struct mstudio_meshvertexdata_t {
	// indirection to this mesh's model's vertex data
	//const mstudio_modelvertexdata_t* modelvertexdata;
	int modelVertexDataPtr;

	// used for fixup calcs when culling top level lods
	// expected number of mesh verts at desired lod
	int					numLODVertexes[MAX_NUM_LODS];
};

struct mstudiomesh_t {
	int					material;

	int					modelindex;

	int					numvertices;		// number of unique vertices/normals/texcoords
	int					vertexoffset;		// vertex mstudiovertex_t

	int					numflexes;			// vertex animation
	int					flexindex;

	// special codes for material operations
	int					materialtype;
	int					materialparam;

	// a unique ordinal for this mesh
	int					meshid;

	glm::vec3 center;

	mstudio_meshvertexdata_t vertexdata;

	int					unused[8]; // remove as appropriate

	mstudiomesh_t() {}
private:
	// No copy constructors allowed
	mstudiomesh_t(const mstudiomesh_t& vOther);
};

struct mstudiomodel_t {
	inline const char* pszName(void) const { return name; }
	char				name[64];

	int					type;

	float				boundingradius;

	int					nummeshes;
	int					meshindex;
	inline mstudiomesh_t* pMesh(int i) const { return (mstudiomesh_t*)(((byte*)this) + meshindex) + i; };

	// cache purposes
	int					numvertices;		// number of unique vertices/normals/texcoords
	int					vertexindex;		// vertex Vector
	int					tangentsindex;		// tangents Vector

	int					numattachments;
	int					attachmentindex;

	int					numeyeballs;
	int					eyeballindex;

	mstudio_modelvertexdata_t vertexdata;

	int					unused[8];		// remove as appropriate
};

struct mstudiobodyparts_t {
	int					sznameindex;
	inline char* const pszName(void) const { return ((char*)this) + sznameindex; }
	int					nummodels;
	int					base;
	int					modelindex; // index into models array
	inline mstudiomodel_t* pModel(int i) const { return (mstudiomodel_t*)(((byte*)this) + modelindex) + i; };
};

// NOTE: This is exactly 48 bytes
struct mstudiovertex_t {
	mstudioboneweight_t	m_BoneWeights;
	glm::vec3			m_vecPosition;
	glm::vec3			m_vecNormal;
	glm::vec2		m_vecTexCoord;
};

// Most of these structs are based on code from studio.h in the Source SDK
struct studiohdr2_t {
	// ??
	int		srcbonetransform_count;
	int		srcbonetransform_index;

	int		illumpositionattachmentindex;

	float		flMaxEyeDeflection;	//  If set to 0, then equivalent to cos(30)

	// mstudiolinearbone_t
	int		linearbone_index;

	int 		unknown[64];
};

struct mstudiotexture_t {
	// Number of bytes past the beginning of this structure
	// where the first character of the texture name can be found.
	int		name_offset; 	// Offset for null-terminated string
	int		flags;
	int		used; 		// ??

	int		unused; 	// ??

	int	material;		// Placeholder for IMaterial
	int	client_material;	// Placeholder for void*

	int		unused2[10];
};

struct mstudiobbox_t {
	int					bone;
	int					group;				// intersection group
	glm::vec3				bbmin;				// bounding box
	glm::vec3				bbmax;
	int					szhitboxnameindex;	// offset to the name of the hitbox.
	int					unused[8];

	const char* pszHitboxName() {
		if (szhitboxnameindex == 0)
			return "";

		return ((const char*)this) + szhitboxnameindex;
	}

	mstudiobbox_t() {}

private:
	// No copy constructors allowed
	mstudiobbox_t(const mstudiobbox_t& vOther);
};

struct mstudiohitboxset_t {
	int					sznameindex;
	inline char* const	pszName(void) const { return ((char*)this) + sznameindex; }
	int					numhitboxes;
	int					hitboxindex;
	inline mstudiobbox_t* pHitbox(int i) const { return (mstudiobbox_t*)(((byte*)this) + hitboxindex) + i; };
};

struct studiohdr_t {
	int		id;		// Model format ID, such as "IDST" (0x49 0x44 0x53 0x54)
	int		version;	// Format version number, such as 48 (0x30,0x00,0x00,0x00)
	int		checksum;	// This has to be the same in the phy and vtx files to load!
	char		name[64];		// The internal name of the model, padding with null bytes.
					// Typically "my_model.mdl" will have an internal name of "my_model"
	int		dataLength;	// Data size of MDL file in bytes.

	// A vector is 12 bytes, three 4-byte float-values in a row.
	glm::vec3 eyeposition;	// Position of player viewpoint relative to model origin
	glm::vec3 illumposition;	// ?? Presumably the point used for lighting when per-vertex lighting is not enabled.
	glm::vec3 hull_min;	// Corner of model hull box with the least X/Y/Z values
	glm::vec3 hull_max;	// Opposite corner of model hull box
	glm::vec3 view_bbmin;
	glm::vec3 view_bbmax;

	int		flags;		// Binary flags in little-endian order. 
					// ex (00000001,00000000,00000000,11000000) means flags for position 0, 30, and 31 are set. 
					// Set model flags section for more information

	/*
	 * After this point, the header contains many references to offsets
	 * within the MDL file and the number of items at those offsets.
	 *
	 * Offsets are from the very beginning of the file.
	 *
	 * Note that indexes/counts are not always paired and ordered consistently.
	 */

	 // mstudiobone_t
	int		bone_count;	// Number of data sections (of type mstudiobone_t)
	int		bone_offset;	// Offset of first data section

	// mstudiobonecontroller_t
	int		bonecontroller_count;
	int		bonecontroller_offset;

	// mstudiohitboxset_t
	int		hitbox_count;
	int		hitbox_offset;
	mstudiohitboxset_t* getHitboxSet() { return (mstudiohitboxset_t*)((byte*)this + hitbox_offset); }

	// mstudioanimdesc_t
	int		localanim_count;
	int		localanim_offset;

	// mstudioseqdesc_t
	int		localseq_count;
	int		localseq_offset;

	int		activitylistversion; // ??
	int		eventsindexed;	// ??

	// VMT texture filenames
	// mstudiotexture_t
	int		texture_count;
	int		texture_offset;

	// This offset points to a series of ints.
		// Each int value, in turn, is an offset relative to the start of this header/the-file,
		// At which there is a null-terminated string.
	int		texturedir_count;
	int		texturedir_offset;

	// Each skin-family assigns a texture-id to a skin location
	int		skinreference_count;
	int		skinrfamily_count;
	int             skinreference_index;

	// mstudiobodyparts_t
	int		bodypart_count;
	int		bodypart_offset;

	// Local attachment points		
// mstudioattachment_t
	int		attachment_count;
	int		attachment_offset;

	// Node values appear to be single bytes, while their names are null-terminated strings.
	int		localnode_count;
	int		localnode_index;
	int		localnode_name_index;

	// mstudioflexdesc_t
	int		flexdesc_count;
	int		flexdesc_index;

	// mstudioflexcontroller_t
	int		flexcontroller_count;
	int		flexcontroller_index;

	// mstudioflexrule_t
	int		flexrules_count;
	int		flexrules_index;

	// IK probably referse to inverse kinematics
	// mstudioikchain_t
	int		ikchain_count;
	int		ikchain_index;

	// Information about any "mouth" on the model for speech animation
	// More than one sounds pretty creepy.
	// mstudiomouth_t
	int		mouths_count;
	int		mouths_index;

	// mstudioposeparamdesc_t
	int		localposeparam_count;
	int		localposeparam_index;

	/*
	 * For anyone trying to follow along, as of this writing,
	 * the next "surfaceprop_index" value is at position 0x0134 (308)
	 * from the start of the file.
	 */

	 // Surface property value (single null-terminated string)
	int		surfaceprop_index;

	// Unusual: In this one index comes first, then count.
	// Key-value data is a series of strings. If you can't find
	// what you're interested in, check the associated PHY file as well.
	int		keyvalue_index;
	int		keyvalue_count;

	// More inverse-kinematics
	// mstudioiklock_t
	int		iklock_count;
	int		iklock_index;


	float		mass; 		// Mass of object (4-bytes)
	int		contents;	// ??

	// Other models can be referenced for re-used sequences and animations
	// (See also: The $includemodel QC option.)
	// mstudiomodelgroup_t
	int		includemodel_count;
	int		includemodel_index;

	int		virtualModel;	// Placeholder for mutable-void*
							// Source engine is 32 bit, so this is 4 bytes

	// mstudioanimblock_t
	int		animblocks_name_index;
	int		animblocks_count;
	int		animblocks_index;

	int		animblockModel; // Placeholder for mutable-void*

	// Points to a series of bytes?
	int		bonetablename_index;

	int		vertex_base;	// Placeholder for void*
	int		offset_base;	// Placeholder for void*

	// Used with $constantdirectionallight from the QC 
	// Model should have flag #13 set if enabled
	byte		directionaldotproduct;

	byte		rootLod;	// Preferred rather than clamped

	// 0 means any allowed, N means Lod 0 -> (N-1)
	byte		numAllowedRootLods;

	byte		unused0; // ??
	int		unused1; // ??

	// mstudioflexcontrollerui_t
	int		flexcontrollerui_count;
	int		flexcontrollerui_index;

	float flVertAnimFixedPointScale;

	int unused2;

	/**
	 * Offset for additional header information.
	 * May be zero if not present, or also 408 if it immediately
	 * follows this studiohdr_t
	 */
	 // studiohdr2_t
	int		studiohdr2index;

	int		unused3; // ??

	studiohdr2_t* getHdr2() { return (studiohdr2_t*)((byte*)this + studiohdr2index); }
	mstudiobodyparts_t* getBodyPart(int idx) {
		return (mstudiobodyparts_t*)((byte*)this + bodypart_offset) + idx;
	}
};

struct vertexFileHeader_t {
	int	id;				// MODEL_VERTEX_FILE_ID
	int	version;			// MODEL_VERTEX_FILE_VERSION
	int	checksum;			// same as studiohdr_t, ensures sync
	int	numLODs;			// num of valid lods
	int	numLODVertexes[8];	// num verts for desired root lod
	int	numFixups;			// num of vertexFileFixup_t
	int	fixupTableStart;		// offset from base to fixup table
	int	vertexDataStart;		// offset from base to vertex block
	int	tangentDataStart;		// offset from base to tangent block

	mstudiovertex_t* getVertexBlock() {
		return (mstudiovertex_t*)((byte*)this + vertexDataStart);
	}

	byte* getTangentBlock() {
		return ((byte*)this + tangentDataStart);
	}
};

struct VtxVertex_t {
	// these index into the mesh's vert[origMeshVertID]'s bones
	unsigned char boneWeightIndex[3];
	unsigned char numBones;

	unsigned short origMeshVertID;

	// for sw skinned verts, these are indices into the global list of bones
	// for hw skinned verts, these are hardware bone indices
	char boneID[3];
};

struct StripHeader_t {
	int numIndices;
	int indexOffset;

	int numVerts;
	int vertOffset;

	short numBones;

	unsigned char flags;

	int numBoneStateChanges;
	int boneStateChangeOffset;
};

struct StripGroupHeader_t {
	// These are the arrays of all verts and indices for this mesh.  strips index into this.
	int numVerts;
	int vertOffset;

	int numIndices;
	int indexOffset;

	int numStrips;
	int stripOffset;

	unsigned char flags;

	VtxVertex_t* getVertex(int idx) {
		return (VtxVertex_t*)((byte*)this + vertOffset) + idx;
	}

	unsigned short getIndex(int idx) {
		return ((unsigned short*)((byte*)this + indexOffset))[idx];
	}
};

// Sooo many structs to go through just to get the index data out :(
struct MeshHeader_t {
	int numStripGroups;
	int stripGroupHeaderOffset;

	unsigned char flags;

	StripGroupHeader_t* getStripGroup(int idx) {
		return (StripGroupHeader_t*)((byte*)this + stripGroupHeaderOffset) + idx;
	}
};

struct ModelLODHeader_t {
	//Mesh array
	int numMeshes;
	int meshOffset;

	float switchPoint;

	MeshHeader_t* getMeshHeader(int idx) {
		return ((MeshHeader_t*)((byte*)this + meshOffset)) + idx;
	}
};

// This maps one to one with models in the mdl file.
struct ModelHeader_t {
	//LOD mesh array
	int numLODs;   //This is also specified in FileHeader_t
	int lodOffset;

	ModelLODHeader_t* getModelLODHeader(int idx) {
		return ((ModelLODHeader_t*)((byte*)this + lodOffset)) + idx;
	}
};

struct BodyPartHeader_t {
	//Model array
	int numModels;
	int modelOffset;

	ModelHeader_t* getModelHeader(int idx) {
		return ((ModelHeader_t*)((byte*)this + modelOffset)) + idx;
	}
};

struct VtxFileHeader_t {
	// file version as defined by OPTIMIZED_MODEL_FILE_VERSION (currently 7)
	int version;

	// hardware params that affect how the model is to be optimized.
	int vertCacheSize;
	unsigned short maxBonesPerStrip;
	unsigned short maxBonesPerTri;
	int maxBonesPerVert;

	// must match checkSum in the .mdl
	int checkSum;

	int numLODs; // Also specified in ModelHeader_t's and should match

	// Offset to materialReplacementList Array. one of these for each LOD, 8 in total
	int materialReplacementListOffset;

	//Defines the size and location of the body part array
	int numBodyParts;
	int bodyPartOffset;

	BodyPartHeader_t* getBodyPartHeader(int idx) {
		return ((BodyPartHeader_t*)((byte*)this + bodyPartOffset)) + idx;
	}
};
#pragma pack(pop)

glm::vec3 flipVec(glm::vec3 vec) {
	return glm::vec3(vec.x, vec.z, vec.y);
}

void loadSourceModel(AssetID mdlId, AssetID vtxId, AssetID vvdId, std::vector<Vertex>& vertices, std::vector<uint32_t>& indices) {
	PHYSFS_File* mdlFile = g_assetDB.openDataFile(mdlId);

	// this is really, really awful
	size_t mdlLen = PHYSFS_fileLength(mdlFile);
	studiohdr_t* mdl = static_cast<studiohdr_t*>(std::malloc(mdlLen));

	PHYSFS_readBytes(mdlFile, mdl, mdlLen);
	PHYSFS_close(mdlFile);

	PHYSFS_File* vvdFile = g_assetDB.openDataFile(vvdId);
	
	size_t vvdLen = PHYSFS_fileLength(vvdFile);
	vertexFileHeader_t* vvd = static_cast<vertexFileHeader_t*>(std::malloc(vvdLen));

	PHYSFS_readBytes(vvdFile, vvd, vvdLen);
	PHYSFS_close(vvdFile);

	PHYSFS_File* vtxFile = g_assetDB.openDataFile(vtxId);

	size_t vtxLen = PHYSFS_fileLength(vtxFile);
	VtxFileHeader_t* vtx = static_cast<VtxFileHeader_t*>(std::malloc(vtxLen));

	PHYSFS_readBytes(vtxFile, vtx, vtxLen);
	PHYSFS_close(vtxFile);

	if (mdl->id != 0x54534449) {
		return; // err
	}

	if (vvd->checksum != mdl->checksum) {
		std::cout << "MDL checksum doesn't match VVD checksum!\n";
	}

	if (vtx->checkSum != mdl->checksum) {
		std::cout << "MDL checksum doesn't match VTX checksum!\n";
	}

	studiohdr2_t* hdr2 = mdl->getHdr2();
	mstudiohitboxset_t* hitboxSet = mdl->getHitboxSet();

	std::cout << "Source Model Loader\n";
	std::cout << "===================\n";
	std::cout << "Name: " << mdl->name << "\n";
	std::cout << "File version: " << mdl->version << " (only 48 is supported)\n";
	std::cout << "LOD count: " << vvd->numLODs << "\n";

	mstudiovertex_t* vertexBlock = vvd->getVertexBlock();
	vertices.reserve(vvd->numLODVertexes[0]);
	while((byte*)vertexBlock < vvd->getTangentBlock()) {
		mstudiovertex_t vert = *vertexBlock;
		Vertex eeVert;
		eeVert.position = flipVec(vert.m_vecPosition);
		eeVert.normal = flipVec(vert.m_vecNormal);
		eeVert.uv = vert.m_vecTexCoord;
		vertices.push_back(eeVert);
		vertexBlock++;
	}

	int indexOverflowCount = 0;
	for (int i = 0; i < vtx->numBodyParts; i++) {
		BodyPartHeader_t* bph = vtx->getBodyPartHeader(i);
		mstudiobodyparts_t* studioBodypart = mdl->getBodyPart(i);

		std::cout << "Body part " << i << ": " << studioBodypart->pszName() << "\n";

		for (int j = 0; j < bph->numModels; j++) {
			ModelHeader_t* mdh = bph->getModelHeader(j);
			mstudiomodel_t* studioModel = studioBodypart->pModel(j);
			ModelLODHeader_t* mlh = mdh->getModelLODHeader(0);
			std::cout << "\t Model " << j << ": " << studioModel->pszName() << "\n";

			for (int k = 0; k < mlh->numMeshes; k++) {
				MeshHeader_t* mh = mlh->getMeshHeader(k);
				mstudiomesh_t* studioMesh = studioModel->pMesh(k);

				for (int l = 0; l < mh->numStripGroups; l++) {
					StripGroupHeader_t* stripGroup = mh->getStripGroup(l);
					
					for (int m = 0; m < stripGroup->numIndices; m++) {
						int vertTableIndex = stripGroup->getIndex(m);
						int index = stripGroup->getVertex(vertTableIndex)->origMeshVertID;
						//index += studioModel->vertexindex;
						index += studioMesh->vertexoffset;

						if (index >= vertices.size()) {
							indices.push_back(0);
							indexOverflowCount++;
						} else
							indices.push_back(index);
					}
				}
			}
		}
	}

	std::cout << "Vertex count: " << vertices.size() << "\n";
	std::cout << "Index count: " << indices.size() << "\n";

	std::cout << "Index overflow count: " << indexOverflowCount << "\n";

	std::free(mdl);
	std::free(vvd);
	std::free(vtx);
}