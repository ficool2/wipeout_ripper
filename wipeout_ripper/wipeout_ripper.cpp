//
// Tool made to rip Wipeout models and convert them to .obj
// Reference & Documentation: https://github.com/phoboslab/wipeout/blob/master/wipeout.js
//

#include "pch.h"

#include "BMP.h"
#include "tga.h"

#include <iostream> 
#include <sstream>
#include <vector> 
#include <string>
#include <fstream>
#include <algorithm>
#include <iomanip>
#include <iterator>
#include <experimental/filesystem>

#include "wipeout_definitions.h"

#include <Windows.h>

// false = wipeout, true = wipeout2097
bool bSequel = false;

// debug text, makes execution significantly slower
#define DEBUG_OUTPUT 0
// prefer .bmp or .tga?
// .bmp is broken for non-power of 2 texture dimensions
// .bmps also cannot be read by blender, it must be converted to another format like png or tga
#define WRITE_BMP 0

//
// helper functions
//
void printText( std::string text, bool important = false )
{
	if ( important )
	{
		std::cout << "\n";
		std::cout << text << "\n";
		std::cout << "\n";	
	}
	else
	{
#if DEBUG_OUTPUT
		std::cout << "\n";
		std::cout << text << "\n";
		std::cout << "\n";
#endif
	}
}

int getFileSize( const std::string &fileName )
{
    std::ifstream file(fileName.c_str(), std::ifstream::in | std::ifstream::binary);

    if ( !file.is_open() )
        return -1;

    file.seekg(0, std::ifstream::end);
    int fileSize = file.tellg();
    file.close();

    return fileSize;
}


Color int32ToColor( int32_t v )
{
	Color color;
	color.r = ((v >> 24) & 0xff);
	color.g = ((v >> 16) & 0xff);
	color.b = ((v >> 8) & 0xff);
	return color;
}

// required to convert big endian -> little endian
template <class T>
void endswap(T *objp)
{
  char *memp = (char*)(objp);
  std::reverse(memp, memp + sizeof(T));
}

template<typename T>
// slice an array
std::vector<T> slice(std::vector<T> const &v, int m, int n)
{
	auto first = v.cbegin() + m;
	auto last = v.cbegin() + n + 1;

	std::vector<T> vec(first, last);
	return vec;
}

template<class T>
// reverse an uint16_t array
void reverse(T arr[], int n)
{
	std::reverse(arr, arr + n);
}

// get all names of files in this folder
std::vector<std::string> get_filenames( std::experimental::filesystem::path path )
{
    namespace stdfs = std::experimental::filesystem ;

    std::vector<std::string> filenames ;
    
    const stdfs::directory_iterator end{} ;
    
    for ( stdfs::directory_iterator iter{path} ; iter != end ; ++iter )
    {
        if ( stdfs::is_regular_file(*iter) )
            filenames.push_back( iter->path().string() ) ;
    }

    return filenames;
}

// images
uint8_t readBitfield( int size, int &bitMask, int &curByte, std::vector<uint8_t> &src, int &srcPos )
{
	uint8_t value = 0;
	while ( size > 0 ) 
	{
		if ( bitMask == 0x80 ) 
		{
			curByte = src[srcPos++];
		}

		if ( curByte & bitMask ) 
		{
			value |= size;
		}

		size >>= 1;

		bitMask >>= 1;
		if ( bitMask == 0 ) 
		{
			bitMask = 0x80;
		}
	}

	return value;
}

std::vector<std::vector<uint8_t>> unpackImages( const char *filename )
{
#if DEBUG_OUTPUT
	std::cout << "Unpacking images..." << "\n";
#endif

	std::string name(filename);

	std::ifstream fileTextures;
	fileTextures.open( name, std::ifstream::in | std::ifstream::binary );

	if ( !fileTextures.is_open() )
	{
		std::cout << "Error! " << name << " is missing or corrupt!" << "\n";
		system("pause");
		std::exit(0);
	}

#if DEBUG_OUTPUT
	std::cout << "Getting number of files..." << "\n";
#endif

	int offset = 0;

	fileTextures.seekg( offset );
	uint32_t numberOfFiles = 0;
	fileTextures.read((char*)&numberOfFiles, sizeof(numberOfFiles));
	offset += sizeof(numberOfFiles);

	int packedDataOffset = (numberOfFiles + 1) * 4;
	int unpackedLength = 0;

#if DEBUG_OUTPUT
	std::cout << "Number of files: " << std::to_string(numberOfFiles) << "\n";
#endif

	for( unsigned int i = 0; i < numberOfFiles; i++ ) 
	{
		fileTextures.seekg( (i+1)*4 );
		uint32_t length;
		fileTextures.read((char*)&length, sizeof(length));
		unpackedLength += length;
	}

#if DEBUG_OUTPUT
	std::cout << "Unpacked length: " << std::to_string(unpackedLength) << "\n";
	std::cout << "Packed data offset: " << std::to_string(packedDataOffset) << "\n";

	std::cout << "Building src byte array..." << "\n";
#endif

	std::vector<uint8_t> src;
	int srcoffset = packedDataOffset;
	int filesize = getFileSize( filename );
	while ( true )
	{
		if ( srcoffset > ( filesize - 1 ) )
		{
			fileTextures.clear();
			break;
		}
		fileTextures.seekg( srcoffset );
		uint8_t byte;
		fileTextures.read((char*)&byte, sizeof(byte));
		src.push_back(byte);
		srcoffset += sizeof(uint8_t);
	}

#if DEBUG_OUTPUT
	std::cout << "Building dst byte array..." << "\n";
#endif

	std::vector<uint8_t> dst;
	for ( int i = 0 ; i < unpackedLength; i++ )
	{
		uint8_t byte = 0;
		dst.push_back(byte);
	}

#if DEBUG_OUTPUT
	std::cout << "Building wnd byte array..." << "\n";
#endif

	std::vector<uint8_t> wnd;
	for ( int i = 0 ; i < 0x2000; i++ )
	{
		uint8_t byte = 0;
		wnd.push_back(byte);
	}

	unsigned int srcPos = 0;
	int dstPos = 0;
	int wndPos = 1;
	int curBit = 0;
	int curByte = 0;
	int bitMask = 0x80;

#if DEBUG_OUTPUT
	std::cout << "Unpacking src bytes..." << "\n";
#endif

	auto readBitfield = [&]( int size ) 
	{
		int value = 0;
		while ( size > 0 ) 
		{
			if ( bitMask == 0x80 ) 
			{
				curByte = src[srcPos++];
			}

			if ( curByte & bitMask ) 
			{
				value |= size;
			}

			size >>= 1;

			bitMask >>= 1;
			if ( bitMask == 0 ) 
			{
				bitMask = 0x80;
			}
		}

		return value;
	};

	while ( true ) 
	{
		if ( srcPos > src.size() || dstPos > unpackedLength ) 
		{
			break;
		}

		if ( bitMask == 0x80 ) 
		{
			curByte = src[srcPos++];
		}

		curBit = (curByte & bitMask);

		bitMask >>= 1;
		if ( bitMask == 0 ) 
		{
			bitMask = 0x80;
		}

		if ( curBit ) 
		{
			wnd[wndPos & 0x1fff] = dst[dstPos] = readBitfield(0x80);
			wndPos++;
			dstPos++;
		}
		else 
		{
			int position = readBitfield(0x1000);
			if ( position == 0 ) 
			{
				break;
			}

			int length = readBitfield(0x08) + 2;
			for ( int i = 0; i <= length; i++ ) 
			{
				wnd[wndPos & 0x1fff] = dst[dstPos] = wnd[(i + position) & 0x1fff];
				wndPos++;
				dstPos++;
			}
		}
	}

#if DEBUG_OUTPUT
	std::cout << "Slicing dst bytes array..." << "\n";
#endif

	// Split unpacked data into separate buffer for each file
	int fileOffset = 0;
	std::vector<std::vector<uint8_t>> files;
	for ( unsigned int i = 0; i < numberOfFiles; i++ ) 
	{
		fileTextures.seekg( (i+1)*4 );
		uint32_t fileLength = 0;
		fileTextures.read((char*)&fileLength, sizeof(fileLength));
		std::vector<uint8_t> buffer = slice(dst, fileOffset, fileOffset + fileLength );
		files.push_back(buffer);

		fileOffset += fileLength;
	}

#if DEBUG_OUTPUT
	std::cout << "Image unpacking finished successfully!" << "\n";

	std::cout << "Number of unpacked image files: " << std::to_string( files.size() ) << "\n";
#endif

	return files;
}

void putPixel(std::vector<uint8_t> &dst, int offset, uint16_t color)
{
	dst[offset + 0] = (color & 0x1f) << 3; // R
	dst[offset + 1] = ((color >> 5) & 0x1f) << 3; // G
	dst[offset + 2] = ((color >> 10) & 0x1f) << 3; // B
	dst[offset + 3] = color == 0 ? 0 : 0xff; // A
};

std::vector<Image> readImages( std::vector<std::vector<uint8_t>> &rawImages )
{
#if DEBUG_OUTPUT
	std::cout << "Reading images..." << "\n";
#endif

	std::vector<Image> images;

	int imageindex = 0;

	for ( size_t ii = 0; ii < rawImages.size(); ii++ )
	{
#if DEBUG_OUTPUT
		std::cout << "Reading image index: " << std::to_string(imageindex) << "\n";
#endif

		int offset = 0;

		std::vector<uint8_t> image = rawImages.at(ii);

		// HACK: remove a stray byte leftover. don't know why this happens.
		image.pop_back();

#if DEBUG_OUTPUT
		std::cout << "Getting image header..." << "\n";
#endif

		ImageFileHeader file;
		file = *reinterpret_cast<ImageFileHeader*>(&image[offset]);
		offset += sizeof(file);

#if DEBUG_OUTPUT
		std::cout << "Getting image pallete..." << "\n";
#endif

		std::vector<uint16_t> palette;

		if ( file.type == PALETTED_4_BPP ||
			file.type == PALETTED_8_BPP ) 
		{
			unsigned int palleteoffset = offset;
			unsigned int length = 0;
			while ( true )
			{
				if ( length > file.paletteColors )
				{
					break;
				}
				uint16_t byte = 0;
				byte = *reinterpret_cast<uint16_t*>(&image[palleteoffset]);
				palleteoffset += sizeof(byte);
				palette.push_back(byte);
				length += 1;
			}

			// HACK: remove a stray byte leftover. don't know why this happens... again
			palette.pop_back();

			offset += file.paletteColors * 2;
		}

		offset += 4; // skip data size

#if DEBUG_OUTPUT
		std::cout << "Getting image pixel header..." << "\n";
#endif

		int pixelsPerShort = 1;
		if ( file.type == PALETTED_8_BPP )
			pixelsPerShort = 2;
		else if ( file.type == PALETTED_4_BPP )
			pixelsPerShort = 4;

		ImagePixelHeader dim;
		dim = *reinterpret_cast<ImagePixelHeader*>(&image[offset]);
		offset += sizeof(dim);

		int width = dim.width * pixelsPerShort;
		int height = dim.height;

		int entries = dim.width * dim.height;

		std::vector<uint8_t> pixels;
		for ( int i = 0 ; i < ((width * height) * 4); i++ ) // 4 = RGBA
		{
			uint8_t byte = 0;
			pixels.push_back(byte);
		}

#if DEBUG_OUTPUT
		std::cout << "Pixel count: " << std::to_string(pixels.size()) << "\n";

		std::cout << "Read pixels..." << "\n";
#endif

		if ( file.type == TRUE_COLOR_16_BPP )
		{
			for ( int i = 0; i < entries; i++ ) 
			{
				uint16_t c;
				c = *reinterpret_cast<uint16_t*>(&image[offset + i * 2]);
				putPixel(pixels, i*4, c);
			}
		}
		else if ( file.type == PALETTED_8_BPP ) 
		{
			for ( int i = 0; i < entries; i++ ) 
			{
				uint16_t p;
				p = *reinterpret_cast<uint16_t*>(&image[offset + i * 2]);

				putPixel(pixels, i*8+0, palette[ p & 0xff ]);
				putPixel(pixels, i*8+4, palette[ (p>>8) & 0xff ]);
			}
		}
		else if ( file.type == PALETTED_4_BPP ) 
		{
			for ( int i = 0; i < entries; i++ ) 
			{
				uint16_t p;
				p = *reinterpret_cast<uint16_t*>(&image[offset + i * 2]);

				putPixel(pixels, i*16+ 0, palette[ p & 0xf ]);
				putPixel(pixels, i*16+ 4, palette[ (p>>4) & 0xf ]);
				putPixel(pixels, i*16+ 8, palette[ (p>>8) & 0xf ]);
				putPixel(pixels, i*16+12, palette[ (p>>12) & 0xf ]);
			}
		}

		Image theImage;

		theImage.pixels = pixels;
		theImage.width = width;
		theImage.height = height;

		images.push_back(theImage);

		imageindex++;
	}

	return images;

#if DEBUG_OUTPUT
	std::cout << "Image reading successful!" << "\n";
#endif
};

void writeRawTrackImages( std::vector<Image> &images )
{
#if DEBUG_OUTPUT
	std::cout << "Writing raw images..." << "\n";
#endif

	int imageindex = 0;

	for ( size_t ii = 0; ii < images.size(); ii++ )
	{
		Image image = images.at(ii);

#if WRITE_BMP
#if DEBUG_OUTPUT
		std::cout << "Init BMP of width and height: " << std::to_string(image.width) << " " << std::to_string(image.height) << "\n";
#endif

		BMP theBMP( image.width, image.height, true );

		int pixeloffset = 0;

		std::cout << "Write raw pixels to BMP..." << "\n";

		for ( int x = 0; x < image.width; x++ )
		{
			for ( int y = 0; y < image.height; y++ )
			{
				ColorRGBA color;
				for ( int c = 0; c < 4; c++ )
				{
					if ( c == 0 )
						color.r = image.pixels.at(pixeloffset);
					else if ( c == 1 )
						color.g = image.pixels.at(pixeloffset);
					else if ( c == 2 )
						color.b = image.pixels.at(pixeloffset);
					else if ( c == 3 )
						color.a = image.pixels.at(pixeloffset);

					pixeloffset++;
				}
				
				theBMP.set_pixel( x, y, color.b, color.g, color.r, color.a );
			}
		}

#if DEBUG_OUTPUT
		std::cout << "Saving BMP..." << "\n";
#endif

		std::string filename = "ripped_track_raw/track_";
		filename += std::to_string(imageindex);
		filename += ".bmp";
		const char *cc = filename.c_str();

		theBMP.write( cc );
#else //tga
#if DEBUG_OUTPUT
		std::cout << "Init TGA of width and height: " << std::to_string(image.width) << " " << std::to_string(image.height) << "\n";
#endif
		std::string filename = "ripped_track_raw/track_";
		filename += std::to_string(imageindex);
		filename += ".tga";
		const char *cc = filename.c_str();

		// re-order from RGBA -> BGRA for .tga
		for ( size_t p = 0; p < image.pixels.size(); p += 4 )
		{
			uint8_t r = image.pixels[p];
			uint8_t b = image.pixels[p+2];

			image.pixels[p] = b;
			image.pixels[p+2] = r;
		}

		uint8_t* px = &image.pixels[0];
		tga_write( cc, image.width, image.height, px, 4, 4 );
#endif

		imageindex++;	
	}

	std::cout << "Raw image writing successful!" << "\n" << "\n";
}

void writeTrackImages( Track &theTrack )
{
	std::cout << "Writing combined images..." << "\n";

	int imageindex = 0;

	for ( size_t ii = 0; ii < theTrack.images.size(); ii++ )
	{
		Image image = theTrack.images.at(ii);

		// im too lazy to do image combining properly with TGA, so only .bmp will be supported
		// note: .bmps must be corrected with a rotation of 90 degrees clockwise!
#if /*WRITE_BMP*/ 1
#if DEBUG_OUTPUT
		std::cout << "Init BMP of width and height: " << std::to_string(image.width) << " " << std::to_string(image.height) << "\n";
#endif

		BMP theBMP( image.width, image.height, true );

#if DEBUG_OUTPUT
		std::cout << "Write combined pixels to BMP..." << "\n";
#endif

		int pixeloffset = 0;
		int xoffset = 0;
		int yoffset = 0;

		for ( int i = 0; i < 16; i++ )
		{
			if ( xoffset == 128 )
			{
				xoffset = 0;
				yoffset += 32;
			}

			for ( int x = 0; x < 32; x++ )
			{
				for ( int y = 0; y < 32; y++ )
				{
					ColorRGBA color;
					for ( int c = 0; c < 4; c++ )
					{
						if ( c == 0 )
							color.r = image.pixels.at(pixeloffset);
						else if ( c == 1 )
							color.g = image.pixels.at(pixeloffset);
						else if ( c == 2 )
							color.b = image.pixels.at(pixeloffset);
						else if ( c == 3 )
							color.a = image.pixels.at(pixeloffset);

						pixeloffset++;
					}
				
					theBMP.set_pixel( x+xoffset, y+yoffset, color.b, color.g, color.r, color.a );
				}
			}

			xoffset += 32;
		}

#if DEBUG_OUTPUT
		std::cout << "Saving BMP..." << "\n";
#endif

		std::string filename = "ripped_track/track_";
		filename += std::to_string(imageindex);
		filename += ".bmp"; 
		const char *cc = filename.c_str();

		// re-order from RGBA -> BGRA for .tga
		for ( size_t p = 0; p < image.pixels.size(); p += 4 )
		{
			uint8_t r = image.pixels[p];
			uint8_t b = image.pixels[p+2];

			image.pixels[p] = b;
			image.pixels[p+2] = r;
		}

		theBMP.write( cc );
#else //tga
#if DEBUG_OUTPUT
		std::cout << "Init TGA of width and height: " << std::to_string(image.width) << " " << std::to_string(image.height) << "\n";
#endif
		std::string filename = "ripped_track/track_";
		filename += std::to_string(imageindex);
		filename += ".tga";
		const char *cc = filename.c_str();

		uint8_t* px = &image.pixels[0];
		tga_write( cc, image.width, image.height, px, 4, 4 );
#endif

		imageindex++;	
	}

	std::cout << "Combined image writing successful!" << "\n";
}

//
// object
//

Object readObject(std::ifstream& buffer, int offset )
{
	Object object;

	int initialOffset = offset;

	buffer.seekg( offset );
	ObjectHeader header;
	buffer.read((char*)&header, sizeof(header));
	//endswap(&header.name);
	endswap(&header.vertexCount);
	endswap(&header.polygonCount);
	endswap(&header.index1);
	endswap(&header.origin);
	endswap(&header.position);
	// HACK: swap origin and position values, i dont know why its out of order
	Vector3 origin = header.origin;
	Vector3 position = header.position;
	header.origin.x = origin.z;
	header.origin.z = origin.x;
	header.position.x = position.z;
	header.position.z = position.x;
	offset += sizeof(header);
	object.header = header;

	for ( int i = 0; i < header.vertexCount; i++ )
	{
		buffer.seekg(offset);
		Vertex vertex;
		buffer.read((char*)&vertex, sizeof(vertex));
		endswap(&vertex.x);
		endswap(&vertex.y);
		endswap(&vertex.z);
		offset += sizeof(vertex);
		Vertex32 vertex32;
		vertex32.x = (int32_t)vertex.x;
		vertex32.y = (int32_t)vertex.y;
		vertex32.z = (int32_t)vertex.z;
		object.vertices.push_back( vertex32 );
	}

	for ( int i = 0; i < header.polygonCount; i++ )
	{
		buffer.seekg( offset );
		PolygonHeader polyheader;
		buffer.read((char*)&polyheader, sizeof(polyheader));
		endswap(&polyheader.type);
		endswap(&polyheader.subtype);
		buffer.seekg(offset);

		Polygon0x00 polygon0x00;
		Polygon0x01 polygon0x01;
		Polygon0x02 polygon0x02;
		Polygon0x03 polygon0x03;
		Polygon0x04 polygon0x04;
		Polygon0x05 polygon0x05;
		Polygon0x06 polygon0x06;
		Polygon0x07 polygon0x07;
		Polygon0x08 polygon0x08;
		Polygon0x0A polygon0x0A;
		Polygon0x0B polygon0x0B;

		PolygonBase polygon;

		switch ( polyheader.type )
		{
			case UNKNOWN_00:
				buffer.read((char*)&polygon0x00, sizeof(polygon0x00));
				endswap(&polygon0x00.header);
				offset += sizeof(polygon0x00);
				polygon.type = UNKNOWN_00;
				polygon.polygon0x00 = polygon0x00;
				object.polygons.push_back(polygon);
				break;
			case FLAT_TRIS_FACE_COLOR:
				buffer.read((char*)&polygon0x01, sizeof(polygon0x01));
				endswap(&polygon0x01.header);
				endswap(&polygon0x01.indices);
				endswap(&polygon0x01.color);
				offset += sizeof(polygon0x01);
				polygon.type = FLAT_TRIS_FACE_COLOR;
				polygon.polygon0x01 = polygon0x01;
				object.polygons.push_back(polygon);
				break;
			case TEXTURED_TRIS_FACE_COLOR:
				buffer.read((char*)&polygon0x02, sizeof(polygon0x02));
				endswap(&polygon0x02.header);
				endswap(&polygon0x02.indices);
				endswap(&polygon0x02.texture);
				endswap(&polygon0x02.uv);
				endswap(&polygon0x02.color);
				offset += sizeof(polygon0x02);
				polygon.type = TEXTURED_TRIS_FACE_COLOR;
				polygon.polygon0x02 = polygon0x02;
				object.polygons.push_back(polygon);
				break;
			case FLAT_QUAD_FACE_COLOR:
				buffer.read((char*)&polygon0x03, sizeof(polygon0x03));
				endswap(&polygon0x03.header);
				endswap(&polygon0x03.indices);
				endswap(&polygon0x03.color);
				offset += sizeof(polygon0x03);
				polygon.type = FLAT_QUAD_FACE_COLOR;
				polygon.polygon0x03 = polygon0x03;
				object.polygons.push_back(polygon);
				break;
			case TEXTURED_QUAD_FACE_COLOR:
				buffer.read((char*)&polygon0x04, sizeof(polygon0x04));
				endswap(&polygon0x04.header);
				endswap(&polygon0x04.indices);
				endswap(&polygon0x04.texture);
				endswap(&polygon0x04.uv);
				endswap(&polygon0x04.color);
				offset += sizeof(polygon0x04);
				polygon.type = TEXTURED_QUAD_FACE_COLOR;
				polygon.polygon0x04 = polygon0x04;
				object.polygons.push_back(polygon);
				break;
			case FLAT_TRIS_VERTEX_COLOR:
				buffer.read((char*)&polygon0x05, sizeof(polygon0x05));
				endswap(&polygon0x05.header);
				endswap(&polygon0x05.indices);
				endswap(&polygon0x05.colors);
				offset += sizeof(polygon0x05);
				polygon.type = FLAT_TRIS_VERTEX_COLOR;
				polygon.polygon0x05 = polygon0x05;
				object.polygons.push_back(polygon);
				break;
			case TEXTURED_TRIS_VERTEX_COLOR:
				buffer.read((char*)&polygon0x06, sizeof(polygon0x06));
				endswap(&polygon0x06.header);
				endswap(&polygon0x06.indices);
				endswap(&polygon0x06.texture);
				endswap(&polygon0x06.uv);
				endswap(&polygon0x06.colors);
				offset += sizeof(polygon0x06);
				polygon.type = TEXTURED_TRIS_VERTEX_COLOR;
				polygon.polygon0x06 = polygon0x06;
				object.polygons.push_back(polygon);
				break;
			case FLAT_QUAD_VERTEX_COLOR:
				buffer.read((char*)&polygon0x07, sizeof(polygon0x07));
				endswap(&polygon0x07.header);
				endswap(&polygon0x07.indices);
				endswap(&polygon0x07.colors);
				offset += sizeof(polygon0x07);
				polygon.type = FLAT_QUAD_VERTEX_COLOR;
				polygon.polygon0x07 = polygon0x07;
				object.polygons.push_back(polygon);
				break;
			case TEXTURED_QUAD_VERTEX_COLOR:
				buffer.read((char*)&polygon0x08, sizeof(polygon0x08));
				endswap(&polygon0x08.header);
				endswap(&polygon0x08.indices);
				endswap(&polygon0x08.texture);
				endswap(&polygon0x08.uv);
				endswap(&polygon0x08.colors);
				offset += sizeof(polygon0x08);
				polygon.type = TEXTURED_QUAD_VERTEX_COLOR;
				polygon.polygon0x08 = polygon0x08;
				object.polygons.push_back(polygon);
				break;
			case SPRITE_TOP_ANCHOR:
				buffer.read((char*)&polygon0x0A, sizeof(polygon0x0A));
				endswap(&polygon0x0A.header);
				endswap(&polygon0x0A.index);
				endswap(&polygon0x0A.width);
				endswap(&polygon0x0A.height);
				endswap(&polygon0x0A.texture);
				endswap(&polygon0x0A.color);
				offset += sizeof(polygon0x0A);
				polygon.type = SPRITE_TOP_ANCHOR;
				polygon.polygon0x0A = polygon0x0A;
				object.polygons.push_back(polygon);
				break;
			case SPRITE_BOTTOM_ANCHOR:
				buffer.read((char*)&polygon0x0B, sizeof(polygon0x0B));
				endswap(&polygon0x0B.header);
				endswap(&polygon0x0B.index);
				endswap(&polygon0x0B.width);
				endswap(&polygon0x0B.height);
				endswap(&polygon0x0B.texture);
				endswap(&polygon0x0B.color);
				offset += sizeof(polygon0x0B);
				polygon.type = SPRITE_BOTTOM_ANCHOR;
				polygon.polygon0x0B = polygon0x0B;
				object.polygons.push_back(polygon);
				break;
		}
	}

	object.byteLength = offset - initialOffset;

#if DEBUG_OUTPUT
	std::cout << "Object header:" << "\n";
	std::cout << "name: " << object.header.name << "\n";
	std::cout << "vertexCount: " << std::to_string(object.header.vertexCount) << "\n";
	std::cout << "polygonCount: " << std::to_string(object.header.polygonCount) << "\n";
	std::cout << "index1: " << std::to_string(object.header.index1) << "\n";
	std::cout << "origin xyz: " << std::to_string(object.header.origin.x) << " "
		<< std::to_string(object.header.origin.y) << " "
		<< std::to_string(object.header.origin.z) 
		<< "\n";
	std::cout << "position xyz: " << std::to_string(object.header.position.x) << " "
		<< std::to_string(object.header.position.y) << " "
		<< std::to_string(object.header.position.z)
		<< "\n";
	std::cout << "bytelength: " << std::to_string(object.byteLength) << "\n";
#endif

	return object;
}

std::vector<Object> readObjects( std::ifstream& buffer, int fileSize )
{
	int offset = 0;
	int i = 0;
	std::vector<Object> objects;
	while ( offset < fileSize ) 
	{
		objects.push_back( readObject( buffer, offset ) );
		offset += objects[i].byteLength;
		i++;
	}

	return objects;
}

std::vector<Object> loadObjects( const char *filename )
{
	std::string name(filename);

	std::cout << "Reading 3D Objects from " << name << "..." << "\n" ;

	std::ifstream fileprm;
	fileprm.open( name, std::ifstream::in | std::ifstream::binary );
	if ( !fileprm.is_open() )
	{
		printText( "Error! Object .PRM is corrupt or missing!" );
		system("pause");
		std::exit(0);
	}

	int fileSize = getFileSize( name );
	std::cout << "Filesize of Object .PRM : " << std::to_string( getFileSize( name ) ) << "\n";

	std::vector<Object> fileObjects = readObjects( fileprm, fileSize );
	int fileObjectsCount = fileObjects.size();

	printText( "3D Objects from Object .PRM read succesfully." );
	std::cout << "3D Objects count: " << std::to_string( fileObjectsCount ) << "\n";
	std::cout << "\n";

	return fileObjects;
}

void writeObjectImages( std::vector<Image> &images, const char *filename, const char *path )
{
	std::cout << "Writing object images..." << "\n";

	int imageindex = 0;

	for ( size_t ii = 0; ii < images.size(); ii++ )
	{
		Image image = images.at(ii);

#if WRITE_BMP
#if DEBUG_OUTPUT
		std::cout << "Init BMP of width and height: " << std::to_string(image.width) << " " << std::to_string(image.height) << "\n";
#endif

		BMP theBMP( image.width, image.height, true );

		int pixeloffset = 0;

#if DEBUG_OUTPUT
		std::cout << "Write pixels to BMP..." << "\n";
#endif

		for ( int x = 0; x < image.width; x++ )
		{
			for ( int y = 0; y < image.height; y++ )
			{
				ColorRGBA color;
				for ( int c = 0; c < 4; c++ )
				{
					if ( c == 0 )
						color.r = image.pixels.at(pixeloffset);
					else if ( c == 1 )
						color.g = image.pixels.at(pixeloffset);
					else if ( c == 2 )
						color.b = image.pixels.at(pixeloffset);
					else if ( c == 3 )
						color.a = image.pixels.at(pixeloffset);

					pixeloffset++;
				}
				
				theBMP.set_pixel( x, y, color.b, color.g, color.r, color.a );
			}
		}

#if DEBUG_OUTPUT
		std::cout << "Saving BMP..." << "\n";
#endif

		std::string fname(path);
		fname += filename;
		fname += std::to_string(imageindex);
		fname += ".bmp";
		const char *cc = fname.c_str();

		theBMP.write( cc );
#else // tga
#if DEBUG_OUTPUT
		std::cout << "Init TGA of width and height: " << std::to_string(image.width) << " " << std::to_string(image.height) << "\n";
#endif

		// re-order from RGBA -> BGRA for .tga
		for ( size_t p = 0; p < image.pixels.size(); p += 4 )
		{
			uint8_t r = image.pixels[p];
			uint8_t b = image.pixels[p+2];

			image.pixels[p] = b;
			image.pixels[p+2] = r;
		}

		std::string fname(path);
		fname += filename;
		fname += std::to_string(imageindex);
		fname += ".tga";
		const char *cc = fname.c_str();

		uint8_t* px = &image.pixels[0];
		tga_write( cc, image.width, image.height, px, 4, 4 );
#endif

		imageindex++;	
	}

	std::cout << "Object image writing successful!" << "\n" << "\n";
}

// i should have put this in the cmd line...

// print debug information in the OBJ?
#define DEBUG_OBJ 0
// assign materials to faces in OBJ?
#define MTL_OBJ 1
// write every object into a single OBJ?
#define MERGED_OBJ 1
// remove degenerate polys in OBJ? (polygons that have >= 2 identical indices)
// this will fix broken meshes in track objects, but it will break common objects
#define DEGENERATES_OBJ 0
// write sections? (path)
#define SECTIONS_OBJ 1
// write sprites?
#define SPRITES_OBJ 0
// apply position offsets to vertices? if not, the offset will be written to a .txt
#define POSITION_OBJ 1

// this is a gigantic disaster, I had no idea what I was doing
// this would be written way better if I revisited it today (i wish i knew about void type pointers earlier)
void writeObjects( std::vector<Object> &objects, std::vector<Image> &images, const char *filename, const char *path )
{
#if MERGED_OBJ
	std::string fname(filename);
	std::string objname(path);
	std::string mtlname(path);
	std::string txtname(path);
	fname += "model";
	objname += fname;
	objname += ".obj";
	mtlname += fname;
	mtlname += ".mtl";
#if !POSITION_OBJ
	txtname += fname;
	txtname += "_pos";
	txtname += ".txt";
	std::ofstream txt(txtname);
#endif

	std::ofstream obj(objname);

#if DEBUG_OUTPUT
	std::cout << "Writing object to OBJ file " << fname << "\n";
#endif

#if MTL_OBJ
	obj << "mtllib " << filename << "model" << ".mtl" << "\n";
#endif

	int vertexindex = 0;
	int vertexcoordindex = 0;
	int vertexcolorindex = 0;

#endif

#if SPRITES_OBJ
	std::string sprname(path);
	sprname += fname;
	sprname += ".spr";
	std::ofstream spr(sprname);
	int sprindex = 0;
#endif

	for ( size_t o = 0; o < objects.size(); o++ )
	{
		std::string name(objects[o].header.name);
		name += "_";
		name += std::to_string(o);

#if !MERGED_OBJ
		std::string fname(filename);
		std::string objname(path);
		std::string mtlname(path);
		std::string txtname(path);
		fname += name;
		objname += fname;
		objname += ".obj";
		mtlname += fname;
		mtlname += ".mtl";
#if !POSITION_OBJ
		txtname += fname;
		txtname += "_pos";
		txtname += ".txt";
		std::ofstream txt(txtname);
#endif
		std::ofstream obj(objname);
		obj << "mtllib " << mtlname << "\n";

		int vertexindex = 0;
		int vertexcoordindex = 0;
		int vertexcolorindex = 0;

#if DEBUG_OUTPUT
		std::cout << "Writing object to OBJ file " << fname << "\n";
#endif
#endif

		// DEBUGDBEUG
#if 0
		if (!strncmp(objects[o].header.name, "wall_b6_1", 9))
		{
		}
#elif 0
		if ( o == 73 )
		{
		}
		else
		{
			continue;
		}
#endif

		obj << "o " << name << "\n";

#if DEBUG_OBJ
		int d_polygons0x00 = 0;
		int d_polygons0x01 = 0;
		int d_polygons0x02 = 0;
		int d_polygons0x03 = 0;
		int d_polygons0x04 = 0;
		int d_polygons0x05 = 0;
		int d_polygons0x06 = 0;
		int d_polygons0x07 = 0;
		int d_polygons0x08 = 0;
		int d_polygons0x0A = 0;
		int d_polygons0x0B = 0;

		for ( size_t p = 0; p < objects[o].polygons.size(); p++ )
		{
			if ( objects[o].polygons[p].type == UNKNOWN_00 )
				d_polygons0x00++;
			else if ( objects[o].polygons[p].type == FLAT_TRIS_FACE_COLOR )
				d_polygons0x01++;
			else if ( objects[o].polygons[p].type == TEXTURED_TRIS_FACE_COLOR )
				d_polygons0x02++;
			else if ( objects[o].polygons[p].type == FLAT_QUAD_FACE_COLOR )
				d_polygons0x03++;
			else if ( objects[o].polygons[p].type == TEXTURED_QUAD_FACE_COLOR )
				d_polygons0x04++;
			else if ( objects[o].polygons[p].type == FLAT_TRIS_VERTEX_COLOR )
				d_polygons0x05++;
			else if ( objects[o].polygons[p].type == TEXTURED_TRIS_VERTEX_COLOR )
				d_polygons0x06++;
			else if ( objects[o].polygons[p].type == FLAT_QUAD_VERTEX_COLOR )
				d_polygons0x07++;
			else if ( objects[o].polygons[p].type == TEXTURED_QUAD_VERTEX_COLOR )
				d_polygons0x08++;
			else if ( objects[o].polygons[p].type == SPRITE_TOP_ANCHOR )
				d_polygons0x0A++;
			else if ( objects[o].polygons[p].type == SPRITE_BOTTOM_ANCHOR )
				d_polygons0x0B++;
		}
		obj << "# HEADER POLYGON COUNT : " << std::to_string(objects[o].header.polygonCount) << "\n";
		obj << "# HEADER VERTEX COUNT : " << std::to_string(objects[o].header.vertexCount) << "\n";
		obj << "# ACTUAL POLYGON COUNT : " << std::to_string(objects[o].polygons.size()) << "\n";
		obj << "# ACTUAL VERTEX COUNT : " << std::to_string(objects[o].vertices.size()) << "\n";
		obj << "# POLYGONS 0x00 : " << std::to_string(d_polygons0x00) << "\n";
		obj << "# POLYGONS 0x01 : " << std::to_string(d_polygons0x01) << "\n";
		obj << "# POLYGONS 0x02 : " << std::to_string(d_polygons0x02) << "\n";
		obj << "# POLYGONS 0x03 : " << std::to_string(d_polygons0x03) << "\n";
		obj << "# POLYGONS 0x04 : " << std::to_string(d_polygons0x04) << "\n";
		obj << "# POLYGONS 0x05 : " << std::to_string(d_polygons0x05) << "\n";
		obj << "# POLYGONS 0x06 : " << std::to_string(d_polygons0x06) << "\n";
		obj << "# POLYGONS 0x07 : " << std::to_string(d_polygons0x07) << "\n";
		obj << "# POLYGONS 0x08 : " << std::to_string(d_polygons0x08) << "\n";
		obj << "# POLYGONS 0x0A : " << std::to_string(d_polygons0x0A) << "\n";
		obj << "# POLYGONS 0x0B : " << std::to_string(d_polygons0x0B) << "\n";
#endif

		std::vector<Vector2> vertextexcoords;
		std::vector<Color> vertexcolors;

		int currentvertexindex = vertexindex;
		int currentvertexcoordindex = vertexcoordindex;
		int currentvertexcolorindex = vertexcolorindex;

#if DEBUG_OUTPUT
		std::cout << "Offseting vertices..." << "\n";
#endif

		// correct the orientation of vertices
#if 0
		for ( size_t i = 0; i < objects[o].vertices.size(); i++ )
			objects[o].vertices[i].y *= -1;
			objects[o].vertices[i].z *= -1;
		}
#endif

#if POSITION_OBJ
		// offset vertices to position
		for ( size_t i = 0; i < objects[o].vertices.size(); i++ )
		{
			objects[o].vertices[i].x += objects[o].header.position.x;
			objects[o].vertices[i].y += objects[o].header.position.y;
			objects[o].vertices[i].z += objects[o].header.position.z;
		}
#else
		// write .txt of what position each object is at. I use this for the port to source engine
		txt << objects[o].header.position.x << " " << objects[o].header.position.y << " " << objects[o].header.position.z << "\n";
		txt.close();
#endif

#if DEBUG_OUTPUT
		std::cout << "Writing vertices..." << "\n";
#endif

#if DEBUG_OBJ
			obj << "# VERTEXINDEX_START " << std::to_string(vertexindex) << "\n";
			obj << "# VERTEXINDEX_END " << std::to_string(vertexindex + objects[o].vertices.size()) << "\n";
#endif

		// write vertices
		for ( size_t i = 0; i < objects[o].vertices.size(); i++ )
		{

			obj << "v " 
				<< std::to_string( objects[o].vertices[i].x )
				<< " " 
				<< std::to_string( objects[o].vertices[i].y )
				<< " "
				<< std::to_string( objects[o].vertices[i].z )
				<< "\n";

			vertexindex++;
		}

#if DEBUG_OUTPUT
		std::cout << "Reading texcoords..." << "\n";
#endif

		// read tex coords from polygons
		for ( size_t p = 0; p < objects[o].polygons.size(); p++ )
		{
			if ( objects[o].polygons[p].type == TEXTURED_TRIS_FACE_COLOR )
			{
				if ( images.size() > 0 )
				{
					Image image = images.at(objects[o].polygons[p].polygon0x02.texture);
					for ( int j = 0; j < 3; j++ )
					{
						UV uv = objects[o].polygons[p].polygon0x02.uv[j];
						Vector2 vector;
						vector.u = (float)uv.u;
						vector.v = (float)uv.v;
						vector.u = vector.u / image.height;
						vector.v = 1 - ( vector.v / image.width);
						// rotate by -90 degrees to fix orientation
						Vector2 finalvector = vector;
						finalvector.u = vector.v;
						finalvector.v = 1 - vector.u;
						// flip x and y
						finalvector.u = 1 - finalvector.u;
						finalvector.v = 1 - finalvector.v;
						// wow...
						finalvector.v = 1 - finalvector.v;

						vertextexcoords.push_back(finalvector);
						vertexcoordindex++;
					}
				}
			}
			if ( objects[o].polygons[p].type == TEXTURED_QUAD_FACE_COLOR )
			{
				if ( images.size() > 0 )
				{
					Image image = images.at(objects[o].polygons[p].polygon0x04.texture);
					for ( int j = 0; j < 4; j++ )
					{
						UV uv = objects[o].polygons[p].polygon0x04.uv[j];
						Vector2 vector;
						vector.u = (float)uv.u;
						vector.v = (float)uv.v;
						vector.u = vector.u / image.height;
						vector.v = 1 - ( vector.v / image.width);
						// rotate by 90 degrees to fix orientation
						Vector2 finalvector = vector;
						finalvector.u = vector.v;
						finalvector.v = 1 - vector.u;
						// flip x and y
						finalvector.u = 1 - finalvector.u;
						finalvector.v = 1 - finalvector.v;
						// wow...
						finalvector.v = 1 - finalvector.v;

						vertextexcoords.push_back(finalvector);
						vertexcoordindex++;
					}
				}
			}
			if ( objects[o].polygons[p].type == TEXTURED_TRIS_VERTEX_COLOR )
			{
				if ( images.size() > 0 )
				{
					Image image = images.at(objects[o].polygons[p].polygon0x06.texture);
					for ( int j = 0; j < 3; j++ )
					{
						UV uv = objects[o].polygons[p].polygon0x06.uv[j];
						Vector2 vector;
						vector.u = (float)uv.u;
						vector.v = (float)uv.v;
						vector.u = vector.u / image.height;
						vector.v = 1 - ( vector.v / image.width);
						// rotate by 90 degrees to fix orientation
						Vector2 finalvector = vector;
						finalvector.u = vector.v;
						finalvector.v = 1 - vector.u;
						// flip x and y
						finalvector.u = 1 - finalvector.u;
						finalvector.v = 1 - finalvector.v;
						// wow...
						finalvector.v = 1 - finalvector.v;

						vertextexcoords.push_back(finalvector);
						vertexcoordindex++;
					}
				}
			}
			if ( objects[o].polygons[p].type == TEXTURED_QUAD_VERTEX_COLOR )
			{
				if ( images.size() > 0 )
				{
					Image image = images.at(objects[o].polygons[p].polygon0x08.texture);
					for ( int j = 0; j < 4; j++ )
					{
						UV uv = objects[o].polygons[p].polygon0x08.uv[j];
						Vector2 vector;
						vector.u = (float)uv.u;
						vector.v = (float)uv.v;
						vector.u = vector.u / image.height;
						vector.v = 1 - ( vector.v / image.width);
						// rotate by 90 degrees to fix orientation
						Vector2 finalvector = vector;
						finalvector.u = vector.v;
						finalvector.v = 1 - vector.u;
						// flip x and y
						finalvector.u = 1 - finalvector.u;
						finalvector.v = 1 - finalvector.v;
						// wow...
						finalvector.v = 1 - finalvector.v;

						vertextexcoords.push_back(finalvector);
						vertexcoordindex++;
					}
				}
			}
		}

#if DEBUG_OUTPUT
		std::cout << "Reading vertexcolors..." << "\n";
#endif

		// read vertex colors from polygons
		for ( size_t p = 0; p < objects[o].polygons.size(); p++ )
		{
			if ( objects[o].polygons[p].type == FLAT_TRIS_FACE_COLOR )
			{
				Color vertexcolor = int32ToColor( objects[o].polygons[p].polygon0x01.color );
				vertexcolors.push_back(vertexcolor);
				vertexcolorindex++;
			}
			if ( objects[o].polygons[p].type == TEXTURED_TRIS_FACE_COLOR )
			{
				Color vertexcolor = int32ToColor( objects[o].polygons[p].polygon0x02.color );
				vertexcolors.push_back(vertexcolor);
				vertexcolorindex++;
			}
			if ( objects[o].polygons[p].type == FLAT_QUAD_FACE_COLOR )
			{
				Color vertexcolor = int32ToColor( objects[o].polygons[p].polygon0x03.color );
				vertexcolors.push_back(vertexcolor);
				vertexcolorindex++;
			}
			if ( objects[o].polygons[p].type == TEXTURED_QUAD_FACE_COLOR )
			{
				Color vertexcolor = int32ToColor( objects[o].polygons[p].polygon0x04.color );
				vertexcolors.push_back(vertexcolor);
				vertexcolorindex++;
			}
			if ( objects[o].polygons[p].type == FLAT_TRIS_VERTEX_COLOR )
			{
				for ( int j = 0; j < 3; j++ )
				{
					Color vertexcolor = int32ToColor( objects[o].polygons[p].polygon0x05.colors[j] );
					vertexcolors.push_back(vertexcolor);
					vertexcolorindex++;
				}
			}
			if ( objects[o].polygons[p].type == TEXTURED_TRIS_VERTEX_COLOR )
			{
				for ( int j = 0; j < 3; j++ )
				{
					Color vertexcolor = int32ToColor( objects[o].polygons[p].polygon0x06.colors[j] );
					vertexcolors.push_back(vertexcolor);
					vertexcolorindex++;
				}
			}
			if ( objects[o].polygons[p].type == FLAT_QUAD_VERTEX_COLOR )
			{
				for ( int j = 0; j < 4; j++ )
				{
					Color vertexcolor = int32ToColor( objects[o].polygons[p].polygon0x07.colors[j] );
					vertexcolors.push_back(vertexcolor);
					vertexcolorindex++;
				}
			}
			if ( objects[o].polygons[p].type == TEXTURED_QUAD_VERTEX_COLOR )
			{
				for ( int j = 0; j < 4; j++ )
				{
					Color vertexcolor = int32ToColor( objects[o].polygons[p].polygon0x08.colors[j] );
					vertexcolors.push_back(vertexcolor);
					vertexcolorindex++;
				}
			}
		}

#if DEBUG_OBJ
		obj << "# VERTEXuvINDEX_START " << std::to_string(currentvertexcoordindex) << "\n";
		obj << "# VERTEXuvINDEX_END " << std::to_string(currentvertexcoordindex + vertextexcoords.size()) << "\n";
#endif

#if DEBUG_OUTPUT
		std::cout << "Writing texcoords..." << "\n";
#endif

		// write out the vertex texcoords
		for ( size_t i = 0; i < vertextexcoords.size(); i++ )
		{

			obj << "vt " 
				<< std::to_string(vertextexcoords[i].u) 
				<< " " 
				<< std::to_string(vertextexcoords[i].v) 
				<< "\n";
		}

#if DEBUG_OUTPUT
		std::cout << "Writing vertex colors..." << "\n";
#endif

#if DEBUG_OBJ
			obj << "# VERTEXCOLORINDEX_START " << std::to_string(currentvertexcolorindex) << "\n";
			obj << "# VERTEXCOLORINDEX_END " << std::to_string(currentvertexcolorindex + vertexcolors.size()) << "\n";
#endif

		// write out the vertex colors
		for ( size_t i = 0; i < vertexcolors.size(); i++ )
		{
			obj 
				<< "#vcolor" 
				<< " " << std::to_string(vertexcolors[i].r) 
				<< " " << std::to_string(vertexcolors[i].g) 
				<< " " << std::to_string(vertexcolors[i].b) 
				<< "\n";
		}

		int coloroffset = 1;
		int texcoordoffset = 1;

#define VERTEX0 3
#define VERTEX1 1
#define VERTEX2 0
#define VERTEX3 2

#if DEBUG_OUTPUT
		std::cout << "Writing polygons..." << "\n";
#endif

		obj << "s off" << "\n";
		
		// write polygons... sigh
		for ( size_t p = 0; p < objects[o].polygons.size(); p++ )
		{
#if SPRITES_OBJ
			if ( objects[o].polygons[p].type == SPRITE_TOP_ANCHOR )
			{

				Vertex32 vertex = objects[o].vertices[objects[o].polygons[p].polygon0x0A.index];
				Color vertexcolor = int32ToColor( objects[o].polygons[p].polygon0x0A.color );
				uint16_t tex = objects[o].polygons[p].polygon0x0A.texture;
				uint16_t width = objects[o].polygons[p].polygon0x0A.width;
				uint16_t height = objects[o].polygons[p].polygon0x0A.height;

				spr << "o sprite_" << sprindex << "\n";
				spr << "v " << vertex.x << " " << vertex.y << " " << vertex.z << "\n";
				spr << "# color:" << vertexcolor.r << " " << vertexcolor.g << " " << vertexcolor.b << "\n";
				spr << "# texture:" << tex << "\n";
				spr << "# width:" << width << "\n";
				spr << "# height:" << height << "\n";
				sprindex++;
			}
			if ( objects[o].polygons[p].type == SPRITE_BOTTOM_ANCHOR )
			{

				Vertex32 vertex = objects[o].vertices[objects[o].polygons[p].polygon0x0B.index];
				Color vertexcolor = int32ToColor( objects[o].polygons[p].polygon0x0B.color );
				uint16_t tex = objects[o].polygons[p].polygon0x0B.texture;
				uint16_t width = objects[o].polygons[p].polygon0x0B.width;
				uint16_t height = objects[o].polygons[p].polygon0x0B.height;
						
				spr << "o sprite_" << sprindex << "\n";
				spr << "v " << vertex.x << " " << vertex.y << " " << vertex.z << "\n";
				spr << "# color:" << vertexcolor.r << " " << vertexcolor.g << " " << vertexcolor.b << "\n";
				spr << "# texture:" << filename << tex << "\n";
				spr << "# width:" << width << "\n";
				spr << "# height:" << height << "\n";
				sprindex++;
			}
#endif
			if ( objects[o].polygons[p].type == FLAT_TRIS_FACE_COLOR )
			{
				bool degenerate = false;

				for ( int i = 0; i < 3; i++ )
				{
					for ( int j = 0; j < 3; j++ )
					{
						if ( j != i && objects[o].polygons[p].polygon0x01.indices[i] == objects[o].polygons[p].polygon0x01.indices[j] )
						{
#if DEGENERATES_OBJ
							degenerate = true;
#else
							degenerate = false;
#endif
						}
					}
				}

				if ( !degenerate )
				{
#if MTL_OBJ
					obj << "usemtl dummy" << "\n";
#endif

					obj << "f " 
						<< std::to_string( objects[o].polygons[p].polygon0x01.indices[VERTEX3] + 1 + currentvertexindex )
						<< "/" 
						<< "/"  
						<< " " ;
					obj << std::to_string( objects[o].polygons[p].polygon0x01.indices[VERTEX1] + 1 + currentvertexindex )
						<< "/" 
						<< "/"  
						<< " " ;
					obj << std::to_string( objects[o].polygons[p].polygon0x01.indices[VERTEX2] + 1 + currentvertexindex )
						<< "/" 
						<< "/"  
						<< " " ;
					obj << "\n";

					obj << "#fvcolorindex " << 
						std::to_string(coloroffset + currentvertexcolorindex)
						<< " " << 
						std::to_string(coloroffset + currentvertexcolorindex)
						<< " " << 
						std::to_string(coloroffset + currentvertexcolorindex)
						<< "\n";

					coloroffset++;
				}
			}
			if ( objects[o].polygons[p].type == TEXTURED_TRIS_FACE_COLOR )
			{
				bool degenerate = false;

				for ( int i = 0; i < 3; i++ )
				{
					for ( int j = 0; j < 3; j++ )
					{
						if ( j != i && objects[o].polygons[p].polygon0x02.indices[i] == objects[o].polygons[p].polygon0x02.indices[j] )
						{
#if DEGENERATES_OBJ
							degenerate = true;
#else
							degenerate = false;
#endif
						}
					}
				}

				if ( !degenerate )
				{
#if MTL_OBJ
					obj << "usemtl " << filename << std::to_string(objects[o].polygons[p].polygon0x02.texture) << "\n";
#endif

					obj << "f " 
						<< std::to_string( objects[o].polygons[p].polygon0x02.indices[VERTEX3] + 1 + currentvertexindex )
						<< "/" 
						<< std::to_string( texcoordoffset + VERTEX3 + currentvertexcoordindex )
						<< "/" 
						<< std::to_string( texcoordoffset + VERTEX3 + currentvertexcoordindex )
						<< " " ;
					obj << std::to_string( objects[o].polygons[p].polygon0x02.indices[VERTEX1] + 1 + currentvertexindex )
						<< "/" 
						<< std::to_string( texcoordoffset + VERTEX1 + currentvertexcoordindex )
						<< "/" 
						<< std::to_string( texcoordoffset + VERTEX2 + currentvertexcoordindex ) 
						<< " " ;
					obj << std::to_string( objects[o].polygons[p].polygon0x02.indices[VERTEX2] + 1 + currentvertexindex )
						<< "/" 
						<< std::to_string( texcoordoffset + currentvertexcoordindex )
						<< "/" 
						<< std::to_string( texcoordoffset + currentvertexcoordindex ) 
						<< " " ;
					obj << "\n";

					obj << "#fvcolorindex " << 
						std::to_string(coloroffset + currentvertexcolorindex)
						<< " " << 
						std::to_string(coloroffset + currentvertexcolorindex)
						<< " " << 
						std::to_string(coloroffset + currentvertexcolorindex)
						<< "\n";

					coloroffset++;
					texcoordoffset += 3;
				}
			}
			if ( objects[o].polygons[p].type == FLAT_QUAD_FACE_COLOR )
			{
				bool degenerate = false;

				for ( int i = 0; i < 4; i++ )
				{
					for ( int j = 0; j < 4; j++ )
					{
						if ( j != i && objects[o].polygons[p].polygon0x04.indices[i] == objects[o].polygons[p].polygon0x04.indices[j] )
						{
#if DEGENERATES_OBJ
							degenerate = true;
#else
							degenerate = false;
#endif
						}
					}
				}

				if ( !degenerate )
				{
#if MTL_OBJ
					obj << "usemtl dummy" << "\n";
#endif

					obj << "f " 
						<< std::to_string( objects[o].polygons[p].polygon0x03.indices[VERTEX0] + 1 + currentvertexindex )
						<< "/" 
						<< "/"  
						<< " " ;
					obj << std::to_string( objects[o].polygons[p].polygon0x03.indices[VERTEX1] + 1 + currentvertexindex )
						<< "/" 
						<< "/"  
						<< " " ;
					obj << std::to_string( objects[o].polygons[p].polygon0x03.indices[VERTEX2] + 1 + currentvertexindex )
						<< "/" 
						<< "/"  
						<< " " ;
					obj << std::to_string( objects[o].polygons[p].polygon0x03.indices[VERTEX3] + 1 + currentvertexindex )
						<< "/" 
						<< "/"  
						<< " " ;
					obj << "\n";

					obj << "#fvcolorindex " << 
						std::to_string(coloroffset + currentvertexcolorindex)
						<< " " << 
						std::to_string(coloroffset + currentvertexcolorindex)
						<< " " << 
						std::to_string(coloroffset + currentvertexcolorindex)
						<< " " << 
						std::to_string(coloroffset + currentvertexcolorindex)
						<< "\n";

					coloroffset++;
				}
			}
			if ( objects[o].polygons[p].type == TEXTURED_QUAD_FACE_COLOR )
			{
				bool degenerate = false;

				for ( int i = 0; i < 4; i++ )
				{
					for ( int j = 0; j < 4; j++ )
					{
						if ( j != i && objects[o].polygons[p].polygon0x04.indices[i] == objects[o].polygons[p].polygon0x04.indices[j] )
						{
#if DEGENERATES_OBJ
							degenerate = true;
#else
							degenerate = false;
#endif
						}
					}
				}

				if ( !degenerate )
				{
#if MTL_OBJ
					obj << "usemtl " << filename << std::to_string(objects[o].polygons[p].polygon0x04.texture) << "\n";
#endif

					obj << "f " 
						<< std::to_string( objects[o].polygons[p].polygon0x04.indices[VERTEX0] + 1 + currentvertexindex )
						<< "/" 
						<< std::to_string( texcoordoffset + VERTEX0 + currentvertexcoordindex )
						<< "/" 
						<< std::to_string( texcoordoffset + VERTEX0 + currentvertexcoordindex )
						<< " " ;
					obj << std::to_string( objects[o].polygons[p].polygon0x04.indices[VERTEX1] + 1 + currentvertexindex )
						<< "/" 
						<< std::to_string( texcoordoffset + VERTEX1 + currentvertexcoordindex )
						<< "/" 
						<< std::to_string( texcoordoffset + VERTEX1 + currentvertexcoordindex )
						<< " " ;
					obj << std::to_string( objects[o].polygons[p].polygon0x04.indices[VERTEX2] + 1 + currentvertexindex )
						<< "/" 
						<< std::to_string( texcoordoffset + VERTEX2 + currentvertexcoordindex )
						<< "/" 
						<< std::to_string( texcoordoffset + VERTEX2 + currentvertexcoordindex )
						<< " " ;
					obj << std::to_string( objects[o].polygons[p].polygon0x04.indices[VERTEX3] + 1 + currentvertexindex )
						<< "/" 
						<< std::to_string( texcoordoffset + VERTEX3 + currentvertexcoordindex )
						<< "/" 
						<< std::to_string( texcoordoffset + VERTEX3 + currentvertexcoordindex )
						<< " " ;
					obj << "\n";

					obj << "#fvcolorindex " << 
						std::to_string(coloroffset + currentvertexcolorindex)
						<< " " << 
						std::to_string(coloroffset + currentvertexcolorindex)
						<< " " << 
						std::to_string(coloroffset + currentvertexcolorindex)
						<< " " << 
						std::to_string(coloroffset + currentvertexcolorindex)
						<< "\n";

					coloroffset++;
					texcoordoffset += 4;
				}
			}
			if ( objects[o].polygons[p].type == FLAT_TRIS_VERTEX_COLOR )
			{
				bool degenerate = false;

				for ( int i = 0; i < 3; i++ )
				{
					for ( int j = 0; j < 3; j++ )
					{
						if ( j != i && objects[o].polygons[p].polygon0x05.indices[i] == objects[o].polygons[p].polygon0x05.indices[j] )
						{
#if DEGENERATES_OBJ
							degenerate = true;
#else
							degenerate = false;
#endif
						}
					}
				}

				if ( !degenerate )
				{
#if MTL_OBJ
					obj << "usemtl dummy" << "\n";
#endif

					obj << "f " 
						<< std::to_string( objects[o].polygons[p].polygon0x05.indices[VERTEX3] + 1 + currentvertexindex )
						<< "/" 
						<< "/"  
						<< " " ;
					obj << std::to_string( objects[o].polygons[p].polygon0x05.indices[VERTEX1] + 1 + currentvertexindex )
						<< "/" 
						<< "/"  
						<< " " ;
					obj << std::to_string( objects[o].polygons[p].polygon0x05.indices[VERTEX2] + 1 + currentvertexindex )
						<< "/" 
						<< "/"  
						<< " " ;
					obj << "\n";

					obj << "#fvcolorindex " << 
						std::to_string(coloroffset + 2 + currentvertexcolorindex)
						<< " " << 
						std::to_string(coloroffset + 1 + currentvertexcolorindex)
						<< " " << 
						std::to_string(coloroffset + currentvertexcolorindex)
						<< "\n";

					coloroffset += 3;
				}
			}
			if ( objects[o].polygons[p].type == TEXTURED_TRIS_VERTEX_COLOR )
			{
				bool degenerate = false;

				for ( int i = 0; i < 3; i++ )
				{
					for ( int j = 0; j < 3; j++ )
					{
						if ( j != i && objects[o].polygons[p].polygon0x06.indices[i] == objects[o].polygons[p].polygon0x06.indices[j] )
						{
#if DEGENERATES_OBJ
							degenerate = true;
#else
							degenerate = false;
#endif
						}
					}
				}

				if ( !degenerate )
				{
#if MTL_OBJ
					obj << "usemtl " << filename << std::to_string(objects[o].polygons[p].polygon0x06.texture) << "\n";
#endif
					obj << "f " 
						<< std::to_string( objects[o].polygons[p].polygon0x06.indices[VERTEX3] + 1 + currentvertexindex )
						<< "/" 
						<< std::to_string( texcoordoffset + VERTEX3 + currentvertexcoordindex )
						<< "/" 
						<< std::to_string( texcoordoffset + VERTEX3 + currentvertexcoordindex )
						<< " " ;
					obj << std::to_string( objects[o].polygons[p].polygon0x06.indices[VERTEX1] + 1 + currentvertexindex )
						<< "/" 
						<< std::to_string( texcoordoffset + VERTEX1 + currentvertexcoordindex )
						<< "/" 
						<< std::to_string( texcoordoffset + VERTEX1 + currentvertexcoordindex )
						<< " " ;
					obj << std::to_string( objects[o].polygons[p].polygon0x06.indices[VERTEX2] + 1 + currentvertexindex )
						<< "/" 
						<< std::to_string( texcoordoffset + VERTEX2 + currentvertexcoordindex )
						<< "/" 
						<< std::to_string( texcoordoffset + VERTEX2 + currentvertexcoordindex )
						<< " " ;
					obj << "\n";

					obj << "#fvcolorindex " << 
						std::to_string(coloroffset + 2 + currentvertexcolorindex)
						<< " " << 
						std::to_string(coloroffset + 1 + currentvertexcolorindex)
						<< " " << 
						std::to_string(coloroffset + currentvertexcolorindex)
						<< "\n";

					coloroffset += 3;
					texcoordoffset += 3;
				}
			}
			if ( objects[o].polygons[p].type == FLAT_QUAD_VERTEX_COLOR )
			{
				bool degenerate = false;

				for ( int i = 0; i < 4; i++ )
				{
					for ( int j = 0; j < 4; j++ )
					{
						if ( j != i && objects[o].polygons[p].polygon0x07.indices[i] == objects[o].polygons[p].polygon0x07.indices[j] )
						{
#if DEGENERATES_OBJ
							degenerate = true;
#else
							degenerate = false;
#endif
						}
					}
				}

				if ( !degenerate )
				{
#if MTL_OBJ
					obj << "usemtl dummy" << "\n";
#endif

					obj << "f " 
						<< std::to_string( objects[o].polygons[p].polygon0x07.indices[VERTEX0] + 1 + currentvertexindex )
						<< "/" 
						<< "/"  
						<< " " ;
					obj << std::to_string( objects[o].polygons[p].polygon0x07.indices[VERTEX1] + 1 + currentvertexindex )
						<< "/" 
						<< "/"  
						<< " " ;
					obj << std::to_string( objects[o].polygons[p].polygon0x07.indices[VERTEX2] + 1 + currentvertexindex )
						<< "/" 
						<< "/"  
						<< " " ;
					obj << std::to_string( objects[o].polygons[p].polygon0x07.indices[VERTEX3] + 1 + currentvertexindex )
						<< "/" 
						<< "/"  
						<< " " ;
					obj << "\n";

					obj << "#fvcolorindex " << 
						std::to_string(coloroffset + 3 + currentvertexcolorindex)
						<< " " << 
						std::to_string(coloroffset + 1 + currentvertexcolorindex)
						<< " " << 
						std::to_string(coloroffset + 0 + currentvertexcolorindex)
						<< " " << 
						std::to_string(coloroffset + 2 + currentvertexcolorindex)
						<< "\n";

					coloroffset += 4;
				}
			}
			if ( objects[o].polygons[p].type == TEXTURED_QUAD_VERTEX_COLOR )
			{
				bool degenerate = false;

				for ( int i = 0; i < 4; i++ )
				{
					for ( int j = 0; j < 4; j++ )
					{
						if ( j != i && objects[o].polygons[p].polygon0x08.indices[i] == objects[o].polygons[p].polygon0x08.indices[j] )
						{
#if DEGENERATES_OBJ
							degenerate = true;
#else
							degenerate = false;
#endif
						}
					}
				}

				if ( !degenerate )
				{
#if MTL_OBJ
					obj << "usemtl " << filename << std::to_string(objects[o].polygons[p].polygon0x08.texture) << "\n";
#endif
					obj << "f " 
						<< std::to_string( objects[o].polygons[p].polygon0x08.indices[VERTEX0] + 1 + currentvertexindex )
						<< "/" 
						<< std::to_string( texcoordoffset + VERTEX0 + currentvertexcoordindex )
						<< "/" 
						<< std::to_string( texcoordoffset + VERTEX0 + currentvertexcoordindex )
						<< " " ;
					obj << std::to_string( objects[o].polygons[p].polygon0x08.indices[VERTEX1] + 1 + currentvertexindex )
						<< "/" 
						<< std::to_string( texcoordoffset + VERTEX1 + currentvertexcoordindex )
						<< "/" 
						<< std::to_string( texcoordoffset + VERTEX1 + currentvertexcoordindex )
						<< " " ;
					obj << std::to_string( objects[o].polygons[p].polygon0x08.indices[VERTEX2] + 1 + currentvertexindex )
						<< "/" 
						<< std::to_string( texcoordoffset + VERTEX2 + currentvertexcoordindex )
						<< "/" 
						<< std::to_string( texcoordoffset + VERTEX2 + currentvertexcoordindex )
						<< " " ;
					obj << std::to_string( objects[o].polygons[p].polygon0x08.indices[VERTEX3] + 1 + currentvertexindex )
						<< "/" 
						<< std::to_string( texcoordoffset + VERTEX3 + currentvertexcoordindex )
						<< "/" 
						<< std::to_string( texcoordoffset + VERTEX3 + currentvertexcoordindex )
						<< " " ;
					obj << "\n";

					obj << "#fvcolorindex " << 
						std::to_string(coloroffset + 3 + currentvertexcolorindex)
						<< " " << 
						std::to_string(coloroffset + 1 + currentvertexcolorindex)
						<< " " << 
						std::to_string(coloroffset + 0 + currentvertexcolorindex)
						<< " " << 
						std::to_string(coloroffset + 2 + currentvertexcolorindex)
						<< "\n";

					coloroffset += 4;
					texcoordoffset += 4;
				}
			}
		}

#if !MERGED_OBJ
#if DEBUG_OUTPUT
			printText("Writing MTL file ...");
#endif
			std::ofstream mtl( mtlname );
	
			for ( size_t i = 0; i < images.size(); i++ )
			{
				mtl << "newmtl " << filename << std::to_string(i) << "\n";
				mtl << "map_Kd " << filename << std::to_string(i) << ".tga" << "\n" << "\n";
			}

			mtl << "newmtl dummy" << "\n";
			mtl << "map_Kd white.tga" << "\n" << "\n";	

#if DEBUG_OUTPUT
			printText("MTL file written successfully!");
#endif

			obj.close();
			mtl.close();
#endif

#if DEBUG_OUTPUT
		std::cout << "Object vertex count: " << std::to_string( objects[o].vertices.size()) << "\n";

		printText( "OBJ file written successfully!" );
#endif
	}

#if MERGED_OBJ
#if DEBUG_OUTPUT
	printText("Writing MTL file ...");
#endif

	std::ofstream mtl( mtlname );
	
	for ( size_t i = 0; i < images.size(); i++ )
	{
		mtl << "newmtl " << filename << std::to_string(i) << "\n";
		mtl << "map_Kd " << filename << std::to_string(i) << ".tga" << "\n" << "\n";
	}

	mtl << "newmtl dummy" << "\n";
	mtl << "map_Kd white.tga" << "\n" << "\n";	

#if DEBUG_OUTPUT
	printText("MTL file written successfully!");
#endif

	obj.close();
	mtl.close();
#endif
}

//
// track
//


Track loadTrack( std::vector<Image> &images )
{
	Track theTrack;
	std::ifstream fileTextureIndex;
	std::ifstream fileVertices;
	std::ifstream fileFaces;
	std::ifstream fileSections;
	std::ifstream fileTrackTexture;

	fileTextureIndex.open( "LIBRARY.TTF", std::ifstream::in | std::ifstream::binary );
	fileVertices.open( "TRACK.TRV", std::ifstream::in | std::ifstream::binary );
	fileFaces.open( "TRACK.TRF", std::ifstream::in | std::ifstream::binary );
	fileSections.open( "TRACK.TRS", std::ifstream::in | std::ifstream::binary );
	if ( bSequel )
		fileTrackTexture.open( "TRACK.TEX", std::ifstream::in | std::ifstream::binary );

	if ( !fileTextureIndex.is_open() )
	{
		printText( "Error! LIBRARY.TFF is missing or corrupt!" ); 
		system("pause");
		std::exit(0);
	}

	int textureindexOffset = 0;
	int indexEntries = ( getFileSize( "LIBRARY.TTF" ) / sizeof( TrackTextureIndex ) );

	for ( int i = 0; i < indexEntries; i++ )
	{
		TrackTextureIndex textureindexHeader;
		fileTextureIndex.seekg( textureindexOffset );
		fileTextureIndex.read((char*)(&textureindexHeader), sizeof(textureindexHeader));
		// big endian -> little endian
		endswap(&textureindexHeader.nearest);
		endswap(&textureindexHeader.mediumest);
		endswap(&textureindexHeader.farthest);
		textureindexOffset += sizeof(textureindexHeader);
		// i don't know why this array is being read backwards...
		int size = sizeof(textureindexHeader.nearest) / sizeof(textureindexHeader.nearest[0]);
		reverse(textureindexHeader.nearest, size);
		theTrack.textureIndex.push_back(textureindexHeader);
	}

	// 4x4 32px tiles
	std::vector<Image> composedImages;

	for ( unsigned int i = 0; i < theTrack.textureIndex.size(); i++ )
	{
		TrackTextureIndex idx = theTrack.textureIndex[i];
		std::vector<uint8_t> composedImage;

		for ( int x = 0; x < 4; x++ ) 
		{
			for ( int y = 0; y < 4; y++ ) 
			{		
				std::vector<uint8_t> ctx = images[idx.nearest[y * 4 + x]].pixels;
				for ( unsigned int j = 0; j < ctx.size(); j++ )
				{
					composedImage.push_back(ctx[j]);
				}
			}
		}

		Image canvas;
		canvas.pixels = composedImage;
		canvas.width = 128;
		canvas.height = 128; 
		composedImages.push_back(canvas);
	}

	theTrack.images = composedImages;
	
	if ( !fileVertices.is_open() )
	{
		printText( "Error! TRACK.TRV is missing or corrupt!" );
		system("pause");
		std::exit(0);
	}

	int vertexOffset = 0;
	int vertexCount = ( getFileSize( "TRACK.TRV" ) / sizeof( TrackVertex ) );

	std::vector<TrackVertex> rawVertices;

	for ( int i = 0; i < vertexCount; i++ )
	{
		TrackVertex vertexHeader;
		fileVertices.seekg( vertexOffset );
		fileVertices.read((char*)(&vertexHeader), sizeof(vertexHeader));
		// big endian -> little endian
		endswap(&vertexHeader.x);
		endswap(&vertexHeader.y);
		endswap(&vertexHeader.z);
		endswap(&vertexHeader.padding);
		vertexOffset += sizeof(vertexHeader);
		rawVertices.push_back( vertexHeader );
	}

	for ( size_t i = 0; i < rawVertices.size(); i++ )
	{
#if 0
		rawVertices[i].y *= -1;
		rawVertices[i].z *= -1;
#endif
		theTrack.vertices.push_back( rawVertices[i] );
	}
	if ( !fileFaces.is_open() )
	{
		printText( "Error! TRACK.TRF is missing or corrupt!" );
		system("pause");
		std::exit(0);
	}

	int faceOffset = 0;
	int faceCount = ( getFileSize( "TRACK.TRF" ) / sizeof( TrackFace ) );
	std::vector<TrackFace> faces;

	for ( int i = 0; i < faceCount; i++ )
	{
		TrackFace faceHeader;
		fileFaces.seekg( faceOffset );
		fileFaces.read((char*)&faceHeader, sizeof(faceHeader));
		endswap(&faceHeader.indices);
		endswap(&faceHeader.normalx);
		endswap(&faceHeader.normaly);
		endswap(&faceHeader.normalz);
		endswap(&faceHeader.color);
		faceOffset += sizeof(faceHeader);
		faces.push_back(faceHeader);
	}

	if ( bSequel )
	{
		if ( !fileTrackTexture.is_open() )
		{
			printText( "Error! TRACK.TEX is missing or corrupt!" );
			system("pause");
			std::exit(0);
		}	

		int trackTextureOffset = 0;
		int trackTextureCount = ( getFileSize( "TRACK.TEX" ) / sizeof( TrackTexture ) );
		std::vector<TrackTexture> trackTextures;

		for ( int i = 0; i < trackTextureCount; i++ )
		{
			TrackTexture trackTextureHeader;
			fileTrackTexture.seekg( trackTextureOffset );
			fileTrackTexture.read((char*)(&trackTextureHeader), sizeof(trackTextureHeader));
			trackTextureOffset += sizeof(trackTextureHeader);
			trackTextures.push_back( trackTextureHeader );
		}

		for ( size_t i = 0; i < faces.size(); i++ )
		{
			faces[i].tile = trackTextures[i].tile;
			faces[i].flags = trackTextures[i].flags;
		}
	}

	for ( size_t i = 0; i < faces.size(); i++ )
	{
		theTrack.faces.push_back( faces[i] );
	}

	int sectionOffset = 0;
	int sectionCount = ( getFileSize( "TRACK.TRS" ) / sizeof( TrackSection ) );
	std::vector<TrackSection> sections;

	for ( int i = 0; i < sectionCount; i++ )
	{
		TrackSection sectionHeader;
		fileSections.seekg( sectionOffset );
		fileSections.read((char*)&sectionHeader, sizeof(sectionHeader));
		endswap(&sectionHeader.nextJunction);
		endswap(&sectionHeader.previous);
		endswap(&sectionHeader.next);
		endswap(&sectionHeader.x);
		endswap(&sectionHeader.y);
		endswap(&sectionHeader.z);
		endswap(&sectionHeader.firstFace);
		endswap(&sectionHeader.numFaces);
		endswap(&sectionHeader.flag);
		sectionOffset += sizeof(sectionHeader);
		sections.push_back(sectionHeader);
	}

	for ( size_t i = 0; i < sections.size(); i++ )
	{
		theTrack.sections.push_back( sections[i] );
	}

	fileTextureIndex.close();
	fileVertices.close();
	fileFaces.close();
	fileSections.close();
	if ( bSequel )
		fileTrackTexture.close();

	return theTrack;
}

void writeTrack( Track& theTrack )
{
	// NOTE: this uses the Goldeneye OBJ format, due to it supporting vertex colors, which normal OBJ does not.

	printText( "Writing OBJ file to track.obj ..." );

	std::ofstream obj("ripped_track/track.obj");

	std::vector<UV> vertextexcoords;
	std::vector<Color> vertexcolors;

	obj << "mtllib track.mtl"
		<< "\n";

	// write out the vertices
	for ( size_t i = 0; i < theTrack.vertices.size(); i++ )
	{
		obj << "v " << theTrack.vertices[i].x << " " << theTrack.vertices[i].y << " " << theTrack.vertices[i].z << "\n";
	}

	// read texcoords and vertexcolors from faces
	for ( size_t i = 0; i < theTrack.faces.size(); i++ )	
	{
		Color vertexcolor = int32ToColor( theTrack.faces[i].color );

		if ( theTrack.faces[i].flags & BOOST )
		{
			vertexcolor.r = 32;
			vertexcolor.g = 32;
			vertexcolor.b = 255;
		}

		vertexcolors.push_back( vertexcolor );

		int flipx = 0;

		if ( theTrack.faces[i].flags & FLIP )
			flipx = 1;

		UV uv;
		uv.u = 1 - flipx; uv.v = 0;
		vertextexcoords.push_back( uv );
		uv.u = 0 + flipx; uv.v = 0;
		vertextexcoords.push_back( uv );
		uv.u = 0 + flipx; uv.v = 1;
		vertextexcoords.push_back( uv );
		uv.u = 1 - flipx; uv.v = 1;
		vertextexcoords.push_back( uv );
	}

	// write out the vertex texcoords
	for ( size_t i = 0; i < vertextexcoords.size(); i++ )
	{
		obj << "vt " 
			<< std::to_string(vertextexcoords[i].u) 
			<< " " 
			<< std::to_string(vertextexcoords[i].v) 
			<< "\n";
	}

	// write out the vertex vcolors
	for ( size_t i = 0; i < vertexcolors.size(); i++ )
	{
		obj 
			<< "#vcolor" 
			<< " " << std::to_string(vertexcolors[i].r) 
			<< " " << std::to_string(vertexcolors[i].g) 
			<< " " << std::to_string(vertexcolors[i].b) 
			<< "\n";
	}

	int j = 1;

	obj << "s off" << "\n";

	// write out the faces
	for ( size_t i = 0; i < theTrack.faces.size(); i++ )
	{
		obj << "usemtl track_" << std::to_string(theTrack.faces[i].tile) << "\n";

		obj << "f " 
			<< std::to_string( theTrack.faces[i].indices[0] + 1 ) 
			<< "/" 
			<< std::to_string( j )
			<< "/" 
			<< std::to_string( j ) 
			<< " " ;
		obj << std::to_string( theTrack.faces[i].indices[1] + 1 ) 
			<< "/" 
			<< std::to_string( j + 1 )
			<< "/" 
			<< std::to_string( j + 1 ) 
			<< " " ;
		obj << std::to_string( theTrack.faces[i].indices[2] + 1 )	
			<< "/" 
			<< std::to_string( j + 2 )
			<< "/" 
			<< std::to_string( j + 2 ) 
			<< " " ;
		obj << std::to_string( theTrack.faces[i].indices[3] + 1 ) 
			<< "/" 
			<< std::to_string( j + 3 )
			<< "/" 
			<< std::to_string( j + 3 ) 
			<< " " ;
		obj << "\n";

		j += 4;

		obj << "#fvcolorindex " << 
			std::to_string(i + 1)
			<< " " << 
			std::to_string(i + 1)
			<< " " << 
			std::to_string(i + 1)
			<< " " << 
			std::to_string(i + 1)
			<< "\n";
	}
	
	obj.close();

	// path
#if SECTIONS_OBJ
	std::ofstream objs("ripped_track/sections.obj");

	for ( size_t i = 0; i < theTrack.sections.size(); i++ )
	{
		objs << "v " << theTrack.sections[i].x << " " << theTrack.sections[i].y << " " << theTrack.sections[i].z << "\n";
	}

	objs.close();
#endif

	std::cout << "Track vertex count: " << std::to_string(theTrack.vertices.size()) << "\n";

	printText( "OBJ file written successfully!" );

	printText( "Writing MTL file to track.mtl ..." );

	std::ofstream mtl("ripped_track/track.mtl");

	for ( size_t i = 0; i < theTrack.textureIndex.size(); i++ )
	{
		mtl << "newmtl track_" << std::to_string(i) << "\n";
		mtl << "map_Kd " << "track_" << std::to_string(i) << ".tga" << "\n" << "\n";
	}

	mtl.close();

	printText("MTL file written successfully!");
}

//
// application
//

int main()
{
	printText( "Wipeout Ripper V1", true );

	printText( "Input 0 for WIPEOUT TRACK, input 1 for WIPEOUT2097 TRACK, input 2 for COMMON data", true );

	bool bCommon = false;

	std::string game;
	//std::cin >> game;
	std::getline(std::cin, game);
	int gamever = stoi(game);
	if ( gamever == 0 )
		bSequel = false;
	else if ( gamever == 1 )
		bSequel = true;
	else
		bCommon = true;
 
	std::cout << "\n";

	// common data... not a track
	if ( bCommon )
	{
		printText( "Input the path to the files to rip", true );

		std::string path;
		std::getline(std::cin, path);

		std::vector<std::string> filenames = get_filenames( path );
		for ( size_t i = 0; i < filenames.size(); i++ )
		{
			// extract the textures
			if ( filenames[i].find( ".CMP" ) != std::string::npos ) 
			{
				std::cout << "filenames: " << filenames[i] << "\n";
				std::vector<std::vector<uint8_t>> rawImages = unpackImages( filenames[i].c_str() );
				std::vector<Image> objectimages = readImages( rawImages );	

				// get file name without extension
				std::string base_filename = filenames[i].substr(filenames[i].find_last_of( "/\\" ) + 1 );
				std::cout << "base_filename: " << base_filename << "\n";
				std::string::size_type p( base_filename.find_last_of( '.' ) );
				std::string file_without_extension = base_filename.substr( 0, p );
				std::cout << "file_without_extension: " << file_without_extension << "\n";

				// append prefix
				std::string fname(file_without_extension);
				fname += "/";
				fname.insert( 0, "ripped_" );
				std::cout << "fname: " << fname << "\n";

				// convert to wchar
				std::wstring wide_string = std::wstring( fname.begin(), fname.end() );
				const wchar_t* wstr = wide_string.c_str();
			
				// make a folder
				CreateDirectory( wstr, NULL);

				// folder name
				std::string folderfname(file_without_extension);
				folderfname += "_";

				// write!
				writeObjectImages( objectimages, folderfname.c_str(), fname.c_str() );

				bool bFound = false;
				// check if this has a corresponding .PRM file for polygon data
				for ( size_t j = 0; j < filenames.size() && !bFound; j++ )
				{
					std::string prmfile(file_without_extension);
					prmfile += ".PRM";
					if ( filenames[j].find( prmfile ) != std::string::npos ) 
					{
						std::vector<Object> objects = loadObjects( prmfile.c_str() );
						writeObjects( objects, objectimages, folderfname.c_str(), fname.c_str() );
						bFound = true;
					}
				}
			} 
			// extract .PRM files, skip if it has a corresponding .CMP
			if ( filenames[i].find( ".PRM" ) != std::string::npos ) 
			{
				std::cout << "filenames: " << filenames[i] << "\n";

				// get file name without extension
				std::string base_filename = filenames[i].substr(filenames[i].find_last_of( "/\\" ) + 1 );
				std::cout << "base_filename: " << base_filename << "\n";
				std::string::size_type const p( base_filename.find_last_of( '.' ) );
				std::string file_without_extension = base_filename.substr( 0, p );
				std::cout << "file_without_extension: " << file_without_extension << "\n";

				// append prefix
				std::string fname(file_without_extension);
				fname += "/";
				fname.insert( 0, "ripped_" );
				std::cout << "fname: " << fname << "\n";

				// convert to wchar
				std::wstring wide_string = std::wstring( fname.begin(), fname.end() );
				const wchar_t* wstr = wide_string.c_str();		

				// folder name
				std::string folderfname(file_without_extension);
				std::cout << "folderfname: " << folderfname << "\n";
				folderfname += "_";

				bool bFound = false;
				// check if this has a corresponding .PRM file for polygon data
				for ( size_t j = 0; j < filenames.size() && !bFound; j++ )
				{
					std::string cmpfile(file_without_extension);
					cmpfile += ".CMP";
					if ( filenames[j].find( cmpfile ) != std::string::npos ) 
					{
						bFound = true;
					}
				}

				if ( !bFound )
				{
					// make a folder
					CreateDirectory( wstr, NULL);

					// write!
					std::vector<Image> dummyimages;
					std::vector<Object> objects = loadObjects(filenames[i].c_str());
					writeObjects( objects, dummyimages, folderfname.c_str(), fname.c_str() );
				}
			}
		}
	}
	else
	{
		std::vector<std::vector<uint8_t>> rawTrackImages = unpackImages( "LIBRARY.CMP" );
		std::vector<std::vector<uint8_t>> rawObjectImages = unpackImages( "SCENE.CMP" );
		std::vector<std::vector<uint8_t>> rawSkyImages = unpackImages( "SKY.CMP" );
		std::vector<Image> trackimages = readImages( rawTrackImages );
		std::vector<Image> objectimages = readImages( rawObjectImages );
		std::vector<Image> skyimages = readImages( rawSkyImages );
		std::vector<Object> objects = loadObjects( "SCENE.PRM" );
		std::vector<Object> sky = loadObjects( "SKY.PRM" );
		Track track = loadTrack( trackimages );

		printText( "Creating ripped folders..." );
		CreateDirectory(L"ripped_track/", NULL);
		//CreateDirectory(L"ripped_track_raw\\", NULL);
		CreateDirectory(L"ripped_objects/", NULL);
		CreateDirectory(L"ripped_sky/", NULL);

		//writeRawTrackImages( images );
		writeTrackImages( track );
		writeTrack( track );

		writeObjectImages( objectimages, "object_", "ripped_objects/" );
		writeObjects( objects, objectimages, "object_", "ripped_objects/" );

		writeObjectImages( skyimages, "sky_", "ripped_sky/" );
		writeObjects( sky, skyimages, "sky_", "ripped_sky/" );
	}

	system("pause");
	return 1;
}