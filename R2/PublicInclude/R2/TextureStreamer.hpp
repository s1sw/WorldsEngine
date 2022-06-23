#pragma once
#include <vector>

namespace R2::VK
{
    class Texture;
}

namespace R2
{
    struct FileOps
    {
        size_t (*ReadData)(void* handle, void* data, size_t numBytes);
        void (*Close)(void* handle);
    };

    class StreamedTexture
    {
    public:
        StreamedTexture(void* handle);
        ~StreamedTexture();
    private:
        void* assetHandle;
        VK::Texture* currentTexture;
        friend class TextureStreamer;
    };

    class TextureStreamer
    {
    public:
        TextureStreamer(FileOps ops);
        void RegisterStreamedTexture(StreamedTexture* streamedTexture);

    private:
        FileOps fileOps;
    };
}