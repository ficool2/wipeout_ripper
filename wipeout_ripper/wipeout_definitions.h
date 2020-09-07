//
// Tool made to rip Wipeout models and convert them to .obj
// Reference & Documentation: https://github.com/phoboslab/wipeout/blob/master/wipeout.js
//

#pragma once
#pragma pack()

// ----------------------------------------------------------------------------
// Wipeout Data Types

// .TRV Files ---------------------------------------------

struct TrackVertex
{
	int32_t x;
	int32_t y;
	int32_t z;
	int32_t padding;
};

// .TRF Files ---------------------------------------------

struct TrackFace
{
	int16_t indices[4];
	int16_t normalx;
	int16_t normaly;
	int16_t normalz;
	uint8_t tile;
	uint8_t flags;
	uint32_t color;
};

enum TrackFaceFlags
{
	WALL = 0,
	TRACK = 1,
	WEAPON = 2,
	FLIP = 4,
	WEAPON_2 = 8,
	UNKNOWN = 16,
	BOOST = 32
};

// .TTF Files ---------------------------------------------

struct TrackTextureIndex
{
	int16_t nearest[16];
	int16_t mediumest[4];
	int16_t farthest;
};

// .TRS Files ---------------------------------------------

struct TrackSection
{
	int32_t nextJunction;
	int32_t previous;
	int32_t next;
	int32_t x;
	int32_t y;
	int32_t z;
	char skip1[116];
	uint32_t firstFace;
	uint16_t numFaces;
	char skip2[4];
	uint16_t flag;
	char skip3[4];
};

// .TEX Files ---------------------------------------------

struct TrackTexture
{
	uint8_t tile;
	uint8_t flags;
};

enum TrackSectionFlags
{
	JUMP = 1,
	JUNCTION_END = 8,
	JUNCTION_START = 16,
	JUNCTION = 32
};

// .PRM Files ---------------------------------------------

struct Vector3
{
	int32_t x;
	int32_t y;
	int32_t z;
};

struct Vertex
{
	int16_t x;
	int16_t y;
	int16_t z;
	int16_t padding;
};

struct UV
{
	uint8_t u;
	uint8_t v;
};

struct ObjectHeader
{
	char name[15];
	char skip1[1];
	uint16_t vertexCount;
	char skip2[14];
	uint16_t polygonCount;
	char skip3[20];
	uint16_t index1;
	char skip4[28];
	Vector3 origin;
	char skip5[20];
	Vector3 position;
	char skip6[16];
};

enum PolygonType
{
	UNKNOWN_00 = 0x00,
	FLAT_TRIS_FACE_COLOR = 0x01,
	TEXTURED_TRIS_FACE_COLOR = 0x02,
	FLAT_QUAD_FACE_COLOR = 0x03,
	TEXTURED_QUAD_FACE_COLOR = 0x04,
	FLAT_TRIS_VERTEX_COLOR = 0x05,
	TEXTURED_TRIS_VERTEX_COLOR = 0x06,
	FLAT_QUAD_VERTEX_COLOR = 0x07,
	TEXTURED_QUAD_VERTEX_COLOR = 0x08,
	SPRITE_TOP_ANCHOR = 0x0A,
	SPRITE_BOTTOM_ANCHOR = 0x0B
};

struct PolygonHeader
{
	uint16_t type;
	uint16_t subtype;
};

// UNKNOWN_00
struct Polygon0x00
{
	PolygonHeader header;
	uint16_t unknown[7];
};

// FLAT_TRIS_FACE_COLOR
struct Polygon0x01
{
	PolygonHeader header;
	uint16_t indices[3];
	uint16_t unknown;
	uint32_t color;
};

// TEXTURED_TRIS_FACE_COLOR
struct Polygon0x02
{
	PolygonHeader header;
	uint16_t indices[3];
	uint16_t texture;
	uint16_t unknown[2];
	UV uv[3];
	uint16_t unknown2[1];
	uint32_t color;
};

// FLAT_QUAD_FACE_COLOR
struct Polygon0x03
{
	PolygonHeader header;
	uint16_t indices[4];
	uint32_t color;
};

// TEXTURED_QUAD_FACE_COLOR
struct Polygon0x04
{
	PolygonHeader header;
	uint16_t indices[4];
	uint16_t texture;
	uint16_t unknown[2];
	UV uv[4];
	uint16_t unknown2[1];
	uint32_t color;
};

// FLAT_TRIS_VERTEX_COLOR
struct Polygon0x05
{
	PolygonHeader header;
	uint16_t indices[3];
	uint16_t unknown;
	uint32_t colors[3];
};

// TEXTURED_TRIS_VERTEX_COLOR
struct Polygon0x06
{
	PolygonHeader header;
	uint16_t indices[3];
	uint16_t texture;
	uint16_t unknown[2];
	UV uv[3];
	uint16_t unknown2[1];
	uint32_t colors[3];
};

// FLAT_QUAD_VERTEX_COLOR
struct Polygon0x07
{
	PolygonHeader header;
	uint16_t indices[4];
	uint32_t colors[4];
};

// TEXTURED_QUAD_VERTEX_COLOR
struct Polygon0x08
{
	PolygonHeader header;
	uint16_t indices[4];
	uint16_t texture;
	uint16_t unknown[2];
	UV uv[4];
	uint8_t unknown2[2];
	uint32_t colors[4];
};

// SPRITE_TOP_ANCHOR
struct Polygon0x0A
{
	PolygonHeader header;
	uint16_t index;
	uint16_t width;
	uint16_t height;
	uint16_t texture;
	uint16_t color;
};

// SPRITE_BOTTOM_ANCHOR
struct Polygon0x0B
{
	PolygonHeader header;
	uint16_t index;
	uint16_t width;
	uint16_t height;
	uint16_t texture;
	uint32_t color;
};

// .TIM Files (Little Endian!) -------------------------------

enum ImageType
{
	PALETTED_4_BPP = 0x08,
	PALETTED_8_BPP = 0x09,
	TRUE_COLOR_16_BPP = 0x02
};

struct ImageFileHeader
{
	uint32_t magic;
	uint32_t type;
	uint32_t headerLength;
	uint16_t paletteX;
	uint16_t paletteY;
	uint16_t paletteColors;
	uint16_t palettes;
};

struct ImagePixelHeader
{
	uint16_t skipX;
	uint16_t skipY;
	uint16_t width;
	uint16_t height;
};

// INTERNAL DEFINITIONS --------------------------------------

struct Vertex32
{
	int32_t x;
	int32_t y;
	int32_t z;
};

struct Vectorf
{
	float x;
	float y;
	float z;
};

struct Color
{
	uint32_t r;
	uint32_t g;
	uint32_t b;
};

struct ColorRGBA
{
	int8_t r;
	int8_t g;
	int8_t b;
	int8_t a;
};

struct Vector2
{
	// only needing this for uvs
	float u;
	float v;
};

struct PolygonBase
{
	PolygonType type;
	Polygon0x00 polygon0x00; // UNKNOWN_00
	Polygon0x01 polygon0x01; // FLAT_TRIS_FACE_COLOR
	Polygon0x02 polygon0x02; // TEXTURED_TRIS_FACE_COLOR
	Polygon0x03 polygon0x03; // FLAT_QUAD_FACE_COLOR
	Polygon0x04 polygon0x04; // TEXTURED_QUAD_FACE_COLOR
	Polygon0x05 polygon0x05; // FLAT_TRIS_VERTEX_COLOR
	Polygon0x06 polygon0x06; // TEXTURED_TRIS_VERTEX_COLOR
	Polygon0x07 polygon0x07; // FLAT_QUAD_VERTEX_COLOR
	Polygon0x08 polygon0x08; // TEXTURED_QUAD_VERTEX_COLOR
	Polygon0x0A polygon0x0A; // SPRITE_TOP_ANCHOR
	Polygon0x0B polygon0x0B; // SPRITE_BOTTOM_ANCHOR
};


struct Object
{
	ObjectHeader header;
	std::vector<Vertex32> vertices;
	std::vector<PolygonBase> polygons;
	int byteLength;
};

struct Image
{
	std::vector<uint8_t> pixels;
	int					 width;
	int					 height;
};

struct Track
{
	std::vector<TrackVertex> vertices;
	std::vector<TrackFace> faces;
	std::vector<TrackTexture> textures;
	std::vector<TrackTextureIndex> textureIndex;
	std::vector<TrackSection> sections;
	std::vector<Image> images;
};