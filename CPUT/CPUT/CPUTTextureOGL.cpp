//--------------------------------------------------------------------------------------
// Copyright 2013 Intel Corporation
// All Rights Reserved
//
// Permission is granted to use, copy, distribute and prepare derivative works of this
// software for any purpose and without fee, provided, that the above copyright notice
// and this statement appear in all copies.  Intel makes no representations about the
// suitability of this software for any purpose.  THIS SOFTWARE IS PROVIDED "AS IS."
// INTEL SPECIFICALLY DISCLAIMS ALL WARRANTIES, EXPRESS OR IMPLIED, AND ALL LIABILITY,
// INCLUDING CONSEQUENTIAL AND OTHER INDIRECT DAMAGES, FOR THE USE OF THIS SOFTWARE,
// INCLUDING LIABILITY FOR INFRINGEMENT OF ANY PROPRIETARY RIGHTS, AND INCLUDING THE
// WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.  Intel does not
// assume any responsibility for any errors which may appear in this software nor any
// responsibility to update it.
//--------------------------------------------------------------------------------------

#include "CPUTTextureOGL.h"
#include "../middleware/stb/stb_image.h"
#include "../middleware/libktx/ktx.h"
#include "../middleware/libktx/ktxint.h"


///////////////////////////////////////////////////////////////////////////////////////////////////
// Loads a S3TC texture and returns a handle
//
// Extension defined here: http://oss.sgi.com/projects/ogl-sample/registry/EXT/texture_compression_s3tc.txt
// File format defined here: http://msdn.microsoft.com/en-us/library/windows/desktop/bb943982(v=vs.85).aspx
// Loader code taken from here: http://software.intel.com/en-us/articles/android-texture-compression
//
// These defines are part of DT GL, but not the standard ES extension.
// We (Intel) do support them however, but the extension has not been uploaded to the
// Khronos registry yet.
//
#ifndef GL_COMPRESSED_RGB_S3TC_DXT1_EXT
#define GL_COMPRESSED_RGB_S3TC_DXT1_EXT  0x83F0
#endif
#ifndef GL_COMPRESSED_RGBA_S3TC_DXT1_EXT
#define GL_COMPRESSED_RGBA_S3TC_DXT1_EXT 0x83F1
#endif
#ifndef GL_COMPRESSED_RGBA_S3TC_DXT3_EXT
#define GL_COMPRESSED_RGBA_S3TC_DXT3_EXT 0x83F2
#endif
#ifndef GL_COMPRESSED_RGBA_S3TC_DXT5_EXT
#define GL_COMPRESSED_RGBA_S3TC_DXT5_EXT 0x83F3
#endif

typedef struct 
{ 
    unsigned int mSize;
    unsigned int mFlags;
    unsigned int mFourCC;
    unsigned int mRGBBitCount;
    unsigned int mRedBitMask;
    unsigned int mGreenBitMask;
    unsigned int mBlueBitMask;
    unsigned int mAlphaBitMask;
} DDSPixelFormat;

typedef struct 
{
    char           mFileType[4];
    unsigned int   mSize;
    unsigned int   mFlags;
    unsigned int   mHeight;
    unsigned int   mWidth;
    unsigned int   mPitchOrLinearSize;
    unsigned int   mDepth;
    unsigned int   mMipMapCount;
    unsigned int   mReserved1[11];
    DDSPixelFormat mPixelFormat;
    unsigned int   mCaps;
    unsigned int   mCaps2;
    unsigned int   mCaps3;
    unsigned int   mCaps4;
    unsigned int   mReserved2;
} DDSHeader;

static bool IsS3TCSupported()
{
    return true;
    bool S3TCSupported = false;
    
    // Determine if we have support for PVRTC
    const GLubyte* pExtensions = glGetString( GL_EXTENSIONS );
    S3TCSupported = strstr( (char*)pExtensions, "GL_EXT_texture_compression_s3tc" ) != NULL;
    
    return S3TCSupported;
}

static GLuint LoadTextureS3TC( const cString TextureFileName, bool forceSRGB = true)
{
#ifdef CPUT_FOR_OGLES3
#define GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT1_EXT 0
#define GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT3_EXT 0
#define GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT5_EXT 0
#define GL_SRGB_ALPHA 0
#define GL_BGRA 0
#define GL_UNSIGNED_INT_8_8_8_8_REV 0
    forceSRGB = false;
#endif
    // Load the texture file
    char* pData = NULL;
    unsigned int fileSize = 0;
    
    CPUTResult result = CPUTFileSystem::ReadFileContents( TextureFileName, &fileSize, (void **)&pData);

    if (result != CPUT_SUCCESS)
    {
        DEBUG_PRINT(_L("Failed to load texture"));// + TextureFileName);
        return 0;
    }
    
    // Read the header
    DDSHeader* pHeader = (DDSHeader*)pData;

    // Generate handle
    GLuint handle;
    glGenTextures( 1, &handle );
    
    // Bind the texture
    glBindTexture( GL_TEXTURE_2D, handle );
    
    // Set filtering mode for 2D textures (bilinear filtering)
    //glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST );
    //glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST );
    glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR );
    glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR );    
    if( pHeader->mMipMapCount > 1 )
    {
        // Use mipmaps with bilinear filtering
        glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_NEAREST );
    }
    
    // Determine texture format
    GLenum internalFormat;
    GLenum format;
    GLenum type;
    GLuint blockSize;
#define UNCOMPRESSED 0x40
    bool compressed = ((pHeader->mPixelFormat.mFlags) & UNCOMPRESSED) == 0;
#define ALPHA 0x1
    bool alpha = ((pHeader->mPixelFormat.mFlags) & ALPHA) != 0;
    if(compressed)
    {
        switch( pHeader->mPixelFormat.mFourCC )
        {
            case 0x31545844: 
            {
                //FOURCC_DXT1
                internalFormat = forceSRGB ? GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT1_EXT : GL_COMPRESSED_RGBA_S3TC_DXT1_EXT;
                blockSize = 8;
                break;
            }
            case 0x33545844: 
            {
                //FOURCC_DXT3
                internalFormat = forceSRGB ? GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT3_EXT : GL_COMPRESSED_RGBA_S3TC_DXT3_EXT;
                blockSize = 16;
                break;
            }
            case 0x35545844: 
            {
                //FOURCC_DXT5
                internalFormat = forceSRGB ? GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT5_EXT : GL_COMPRESSED_RGBA_S3TC_DXT5_EXT;
                blockSize = 16;
                break;
            }
            default: 
            {
                // Unknown format
                DEBUG_PRINT(_L("Unknown DDS format"));
                return 0;
            }
        }
    }
    else
    {
        //fixme - problem with srgb formats...
//        if(false && forceSRGB)
//        {
//            internalFormat = alpha ? GL_SRGB8_ALPHA8 : GL_SRGB8;
//            format = alpha ? GL_SRGB_ALPHA : GL_SRGB;
//        }
//        else
        {
            internalFormat = alpha ? GL_RGBA : GL_RGB;
            format = alpha ? GL_RGBA : GL_RGB;
        }

        type = GL_UNSIGNED_BYTE;//GL_UNSIGNED_INT_8_8_8_8_REV;

        blockSize = 4;

    }
    
    // Initialize the texture
    unsigned int offset = 0;
    unsigned int mipWidth = pHeader->mWidth;
    unsigned int mipHeight = pHeader->mHeight;

    unsigned int mip = 0;
    do
    {
        // Determine size
        // As defined in extension: size = ceil(<w>/4) * ceil(<h>/4) * blockSize
        unsigned int pixelDataSize = 0;
        if(compressed)
        {
            pixelDataSize = ((mipWidth + 3) >> 2) * ((mipHeight + 3) >> 2) * blockSize;
            // Upload texture data for this mip
            GL_CHECK(glCompressedTexImage2D( GL_TEXTURE_2D, mip, internalFormat, mipWidth, mipHeight, 0, pixelDataSize, (pData + sizeof(DDSHeader)) + offset )); 
        }
        else
        {//internalformat format,
            pixelDataSize = mipWidth*mipHeight*blockSize;
//			GL_CHECK(glTexImage2D(GL_TEXTURE_2D,mip,internalFormat,mipWidth, mipHeight,0,GL_RGBA,GL_UNSIGNED_BYTE,(pData + sizeof(DDSHeader)) + offset));
            GL_CHECK(glTexImage2D( GL_TEXTURE_2D, mip, internalFormat, mipWidth, mipHeight, 0, format, type, (pData + sizeof(DDSHeader)) + offset));
        }
        // Next mips is half the size (divide by 2) with a min of 1
        mipWidth = mipWidth >> 1;
        mipWidth = ( mipWidth == 0 ) ? 1 : mipWidth;

        mipHeight = mipHeight >> 1;
        mipHeight = ( mipHeight == 0 ) ? 1 : mipHeight; 

        // Move to next mip map
        offset += pixelDataSize;
        mip++;
    } while(mip < pHeader->mMipMapCount);

       
    // clean up
    free( pData );
    // Return handle
    return handle;
}

///////////////////////////////////////////////////////////////////////////////////////////////////
// Loads a PNG texture and returns a handle
//
// PNG loading code provided as public domain by Sean Barrett (http://nothings.org/)
static GLuint LoadTexturePNG( const cString TextureFileName, bool forceSRGB = true )
{   
    // Load Texture File
    char* pFileData = NULL;
    unsigned int fileSize = 0;
    
    CPUTResult result = CPUTFileSystem::ReadFileContents( TextureFileName, &fileSize, (void **)&pFileData, false, true );
    
    int width, height, numComponents;
    unsigned char* pData = stbi_load_from_memory( (unsigned char*)pFileData, fileSize, &width, &height, &numComponents, 0 );
    
    // Generate handle
    GLuint handle;
    glGenTextures( 1, &handle );
    
    // Bind the texture
    glBindTexture( GL_TEXTURE_2D, handle );
    
    // Set filtering mode for 2D textures (using mipmaps with bilinear filtering)
    glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR );
    glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_NEAREST );

    glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT );
    glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT );
    
    // Determine the format
    GLenum format;
    
    switch( numComponents )
    {
        case 1:
        {
            // Gray
            format = GL_LUMINANCE;
            break;
        }
        case 2: 
        {
            // Gray and Alpha
            format = GL_LUMINANCE_ALPHA;
            break;
        }
        case 3: 
        {
            // RGB
            format = GL_RGB;
            break;
        }
        case 4: 
        {
            // RGBA
            format = GL_RGBA;
            break;
        }
        default: 
        {
            // Unknown format
            assert(0);
            stbi_image_free( pData ); pData = NULL;
            SAFE_DELETE_ARRAY( pFileData );
            return 0;
        }
    }
    
    // Initialize the texture
    glTexImage2D( GL_TEXTURE_2D, 0, format, width, height, 0, format, GL_UNSIGNED_BYTE, pData);
    
    // Generate mipmaps - to have better quality control and decrease load times, this should be done offline
    glGenerateMipmap( GL_TEXTURE_2D );

    stbi_image_free( pData ); pData = NULL;
    SAFE_DELETE_ARRAY( pFileData );

    // Return handle
    return handle;
}

///////////////////////////////////////////////////////////////////////////////////////////////////
// Loads a ETC texture and returns a handle
//
// KTX file defined at http://www.khronos.org/opengles/sdk/tools/KTX/file_format_spec/
// This uses the KTX/ETC library provided by the Khronos Group (see libktx for details)
static GLuint LoadTextureETC_KTX(const cString TextureFileName, bool forceSRGB = true)
{    
    // Read/Load Texture File
    char* pFileData = NULL;
    unsigned int fileSize = 0;
    
	CPUTResult result = CPUTFileSystem::ReadFileContents( TextureFileName, &fileSize, (void **)&pFileData, false, true );
    
    // Generate handle & Load Texture
    GLuint handle = 0;
    GLenum target;
    GLboolean mipmapped;
        
    KTX_error_code ktxResult = ktxLoadTextureM( pFileData, fileSize, &handle, &target, NULL, &mipmapped, NULL, NULL, NULL );
        
    if( ktxResult != KTX_SUCCESS )
    {
 //       LogError( "KTXLib couldn't load texture %s. Error: %d", TextureFileName, result );
        return 0;
    }
 
    // Bind the texture
    glBindTexture( target, handle );
        
    // Set filtering mode for 2D textures (bilinear filtering)
    glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR );
    glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR );
    if( mipmapped )
    {
        // Use mipmaps with bilinear filtering
        glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_NEAREST );
    }

    // clean up
    free( pFileData );
    
    // Return handle
    return handle;  
}

CPUTTexture *CPUTTextureOGL::CreateTexture(const cString &name, GLenum internalFormat, int width, int height, 
                   GLenum format, GLenum type, void* pData)
{
    GLuint texture = 0;
    glGenTextures(1, &texture);
	
	// "Bind" the newly created texture : all future texture functions will modify this texture
	glBindTexture(GL_TEXTURE_2D, texture);

	// Give an empty image to OpenGL ( the last "0" means "empty" )
#define BOARDER 0
	GL_CHECK(glTexImage2D(GL_TEXTURE_2D, 0,internalFormat, width, height, BOARDER, format, type, pData));

	// Poor filtering
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST); 
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glBindTexture(GL_TEXTURE_2D, 0);
    CPUTTextureOGL *pTexture = new CPUTTextureOGL();
    pTexture->mName = name;
    pTexture->mWidth = width;
    pTexture->mHeight = height;
    pTexture->mTextureID = texture;
    glBindTexture(GL_TEXTURE_2D, 0);
    CPUTAssetLibrary::GetAssetLibrary()->AddTexture(_L(""), name, _L(""), pTexture);
    return pTexture;

}

CPUTTexture *CPUTTextureOGL::CreateTexture(const cString &name)
{
    CPUTTextureOGL *pNewTexture = new CPUTTextureOGL();
    pNewTexture->mName = name;
    CPUTAssetLibrary::GetAssetLibrary()->AddTexture(_L(""), name, _L(""), pNewTexture);
    return pNewTexture;
}

//-----------------------------------------------------------------------------
CPUTTexture *CPUTTextureOGL::CreateTexture( const cString &name, const cString &absolutePathAndFilename, bool loadAsSRGB )
{
//    assert(false);
    
    // TODO:  Delegate to derived class.  We don't currently have CPUTTextureDX11
//    ID3D11ShaderResourceView *pShaderResourceView = NULL;
    GLint textureID;
//    ID3D11Resource *pTexture = NULL;
//    ID3D11Device *pD3dDevice= CPUT_DX11::GetDevice();
    CPUTResult result = CreateNativeTexture( /*pD3dDevice, */absolutePathAndFilename, /*&pShaderResourceView,*/ &textureID, loadAsSRGB );
    ASSERT( CPUTSUCCESS(result), _L("Error loading texture: '") + absolutePathAndFilename );

    CPUTTextureOGL *pNewTexture = new CPUTTextureOGL();
    pNewTexture->mName = name;
    pNewTexture->mTextureID = textureID;
    pNewTexture->SetTextureInformation();

    CPUTAssetLibrary::GetAssetLibrary()->AddTexture( _L(""), absolutePathAndFilename, _L(""), pNewTexture);

    return pNewTexture;
     
}

//-----------------------------------------------------------------------------
CPUTResult CPUTTextureOGL::CreateNativeTexture(
//    ID3D11Device *pD3dDevice,
    const cString &fileName,
//    ID3D11ShaderResourceView **ppShaderResourceView,
//    ID3D11Resource **ppTexture,
    GLint *textureID,
    bool ForceLoadAsSRGB)
{
    GLint textureName = 0;
    int index = fileName.find_last_of(_L("."));
    int length = fileName.length();
    cString ext = fileName.substr(index + 1, (length - 1 - index));
    
    if (!ext.compare(_L("dds")))
    {
        // TODO: Should move this bool to be stored in the CPUT_OGL class so it is only lookedup once.
        if (IsS3TCSupported())
        {
            textureName = LoadTextureS3TC(fileName, ForceLoadAsSRGB);
        }
        else
        {
            ASSERT(false, _L(""));
            DEBUG_PRINT(_L("S3TC not supported"));
            return CPUT_ERROR;
        }
    }
    else if (!ext.compare(_L("png"))) {
        textureName = LoadTexturePNG(fileName, ForceLoadAsSRGB);
    } else if (!ext.compare(_L("ktx"))) {
		textureName = LoadTextureETC_KTX(fileName, ForceLoadAsSRGB);
	}
    else
    {
        ASSERT(false, _L(""));
        DEBUG_PRINT(_L("Non supported texture extension found"));
        return CPUT_ERROR;
    }
    *textureID = textureName;
    
    if (textureName)
        return CPUT_SUCCESS;

    return CPUT_ERROR;
}

void CPUTTextureOGL::SetTextureInformation()
{
    GL_CHECK(glBindTexture(GL_TEXTURE_2D, mTextureID));
#ifndef CPUT_FOR_OGLES
    GL_CHECK(glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_WIDTH, &mWidth));
    GL_CHECK(glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_HEIGHT, &mHeight));
#endif
    GL_CHECK(glBindTexture(GL_TEXTURE_2D, 0));

    return;
}

//-----------------------------------------------------------------------------
void* CPUTTextureOGL::MapTexture( CPUTRenderParameters &params, eCPUTMapType type, bool wait )
{
    assert(0);
    return NULL;
} // CPUTTextureOGL::Map()

//-----------------------------------------------------------------------------
void CPUTTextureOGL::UnmapTexture( CPUTRenderParameters &params )
{
    assert(false);
} // CPUTTextureDX11::Unmap()

