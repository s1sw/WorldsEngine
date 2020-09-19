#include "TextureLoader.hpp"

namespace worlds {
    typedef unsigned char byte;
    struct VTFHeader {
        char		signature[4];		// File signature ("VTF\0"). (or as little-endian integer, 0x00465456)
        unsigned int	version[2];		// version[0].version[1] (currently 7.2).
        unsigned int	headerSize;		// Size of the header struct  (16 byte aligned; currently 80 bytes) + size of the resources dictionary (7.3+).
        unsigned short	width;			// Width of the largest mipmap in pixels. Must be a power of 2.
        unsigned short	height;			// Height of the largest mipmap in pixels. Must be a power of 2.
        unsigned int	flags;			// VTF flags.
        unsigned short	frames;			// Number of frames, if animated (1 for no animation).
        unsigned short	firstFrame;		// First frame in animation (0 based).
        unsigned char	padding0[4];		// reflectivity padding (16 byte alignment).
        float		reflectivity[3];	// reflectivity vector.
        unsigned char	padding1[4];		// reflectivity padding (8 byte packing).
        float		bumpmapScale;		// Bumpmap scale.
        unsigned int	highResImageFormat;	// High resolution image format.
        unsigned char	mipmapCount;		// Number of mipmaps.
        unsigned int	lowResImageFormat;	// Low resolution image format (always DXT1).
        unsigned char	lowResImageWidth;	// Low resolution image width.
        unsigned char	lowResImageHeight;	// Low resolution image height.

        // 7.2+
        unsigned short	depth;			// Depth of the largest mipmap in pixels.
                            // Must be a power of 2. Is 1 for a 2D texture.

        // 7.3+
        unsigned char	padding2[3];		// depth padding (4 byte alignment).
        unsigned int	numResources;		// Number of resources this vtf has. The max appears to be 32.

        unsigned char   padding3[8];		// Necessary on certain compilers
    };

    enum VTFFormat {
        IMAGE_FORMAT_NONE = -1,
        IMAGE_FORMAT_RGBA8888 = 0,
        IMAGE_FORMAT_ABGR8888,
        IMAGE_FORMAT_RGB888,
        IMAGE_FORMAT_BGR888,
        IMAGE_FORMAT_RGB565,
        IMAGE_FORMAT_I8,
        IMAGE_FORMAT_IA88,
        IMAGE_FORMAT_P8,
        IMAGE_FORMAT_A8,
        IMAGE_FORMAT_RGB888_BLUESCREEN,
        IMAGE_FORMAT_BGR888_BLUESCREEN,
        IMAGE_FORMAT_ARGB8888,
        IMAGE_FORMAT_BGRA8888,
        IMAGE_FORMAT_DXT1,
        IMAGE_FORMAT_DXT3,
        IMAGE_FORMAT_DXT5,
        IMAGE_FORMAT_BGRX8888,
        IMAGE_FORMAT_BGR565,
        IMAGE_FORMAT_BGRX5551,
        IMAGE_FORMAT_BGRA4444,
        IMAGE_FORMAT_DXT1_ONEBITALPHA,
        IMAGE_FORMAT_BGRA5551,
        IMAGE_FORMAT_UV88,
        IMAGE_FORMAT_UVWQ8888,
        IMAGE_FORMAT_RGBA16161616F,
        IMAGE_FORMAT_RGBA16161616,
        IMAGE_FORMAT_UVLX8888
    };

    inline uint32_t mipScale(uint32_t value, uint32_t mipLevel) {
        return std::max(value >> mipLevel, (uint32_t)1);
    }

    uint32_t getDataSize(int width, int height, VTFFormat format, int mipLevel = 0) {
        width = mipScale(width, mipLevel);
        height = mipScale(height, mipLevel);
        switch (format) {
        case IMAGE_FORMAT_DXT1:
            return ((width + 3) / 4) * ((height + 3) / 4) * 8;
        case IMAGE_FORMAT_DXT3:
        case IMAGE_FORMAT_DXT5:
            return ((width + 3) / 4) * ((height + 3) / 4) * 16;
        case IMAGE_FORMAT_RGBA8888:
        case IMAGE_FORMAT_ARGB8888:
        case IMAGE_FORMAT_BGRA8888:
        case IMAGE_FORMAT_ABGR8888:
        default:
            return width * height * 4;
        }
    }

    /*uint32_t calcOffset(int width, int height, int mipLevel, int mipCount, VTFFormat format) {
        int offset = 0;
        for (int i = mipCount - 1; i > mipLevel; i--) {
            offset += getDataSize(width, height, format, i);
        }
    }*/

    vk::Format toVkFormat(VTFFormat format) {
        switch (format) {
        case IMAGE_FORMAT_DXT1:
            return vk::Format::eBc1RgbaSrgbBlock;
        case IMAGE_FORMAT_DXT5:
            return vk::Format::eBc3SrgbBlock;
        case IMAGE_FORMAT_DXT3:
            return vk::Format::eBc2SrgbBlock;
        case IMAGE_FORMAT_RGBA8888:
            return vk::Format::eR8G8B8A8Srgb;
        default:
            return vk::Format::eUndefined;
        }
    }

    TextureData loadVtfTexture(void* fileData, size_t fileLen, AssetID id) {
        VTFHeader* header = (VTFHeader*)fileData;
        std::cout << "Source Texture Loader\n";
        std::cout << "=====================\n";
        std::cout << "Loading " << g_assetDB.getAssetPath(id) << "\n";
        std::cout << header->signature << "\n";
        std::cout << "Version: " << header->version[0] << "." << header->version[1] << "\n";
        std::cout << "Size: " << header->width << "x" << header->height << "\n";
        std::cout << (int)header->mipmapCount << " mips\n";
        std::cout << "Header Size: " << header->headerSize << "\n";
        std::cout << "Low-res Format: " << header->lowResImageFormat << "\n";
        std::cout << "Low-res resolution: " << (int)header->lowResImageWidth << "x" << (int)header->lowResImageHeight << "\n";
        std::cout << "High-res Format: " << header->highResImageFormat << "\n";


        // The structure of files after v7.3 is fundamentally different
        if (header->version[1] == 2) {
            // I have no idea why the header size is multiplied by 3 here but that's how I got it to work
            byte* dataStart = (byte*)fileData + (80) + getDataSize(16, 16, IMAGE_FORMAT_DXT1);

            /*if (header->highResImageFormat == IMAGE_FORMAT_DXT5)
                dataStart += 16;
            else if (header->highResImageFormat == IMAGE_FORMAT_DXT1)
                dataStart += 8;*/

            // Skip low resolution
            //dataStart += (header->lowResImageWidth * header->lowResImageHeight * 4) / 8;

            uint32_t totalDataSize = getDataSize(header->width, header->height, (VTFFormat)header->highResImageFormat);
            std::cout << "totalDataSize w/o mips: " << totalDataSize << "\n";
            for (int i = header->mipmapCount - 1; i > 0; i--)
                totalDataSize += getDataSize(header->width, header->height, (VTFFormat)header->highResImageFormat, i);
            std::cout << "totalDataSize with mips: " << totalDataSize << "\n";

            void* allTexData = std::malloc(totalDataSize);
            for (int mip = header->mipmapCount - 1; mip >= 0; mip--) {
                byte* currentTexDataPtr = (byte*)allTexData + totalDataSize;
                for (int i = header->mipmapCount - 1; i >= mip; i--) {
                    currentTexDataPtr -= getDataSize(header->width, header->height, (VTFFormat)header->highResImageFormat, i);
                }

                uint32_t currMipSize = getDataSize(header->width, header->height, (VTFFormat)header->highResImageFormat, mip);
                memcpy(currentTexDataPtr, dataStart, currMipSize);
                dataStart += currMipSize;
            }

            TextureData td{};
            td.data = (unsigned char*)allTexData;
            td.numMips = header->mipmapCount;
            td.width = header->width;
            td.height = header->height;
            td.name = g_assetDB.getAssetPath(id);
            td.totalDataSize = totalDataSize;
            td.format = toVkFormat((VTFFormat)header->highResImageFormat);
            
            return td;
        }
        return TextureData{};
    }
}