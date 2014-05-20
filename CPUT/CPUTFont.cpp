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
#include "CPUTFont.h"
#include "CPUTOSServices.h"
#include "CPUTTexture.h"
#include "CPUTGuiController.h"
#include "CPUTAssetLibrary.h"

CPUTFont *CPUTFont::CreateFont(cString FontName, cString AbsolutePathAndFilename)
{
#ifdef UNICODE
    char *pName = ws2s(FontName);
    char *pPath = ws2s(AbsolutePathAndFilename);
    CPUTFont *pFont;
    pFont = CPUTFont::LoadFont( pName, pPath );
    delete pName;
    delete pPath;

    return pFont;
#else
    return CPUTFont::LoadFont( FontName, AbsolutePathAndFilename );
#endif
}

CPUTFont *CPUTFont::LoadFont(const std::string& pPath, const std::string& pFileName)
{
    UINT      fileSize   = 0;
    uint8_t  *pData      = NULL;
    uint32_t  currentPos = 0;

    CPUTFont *pNewFont = new CPUTFont();
    //
    // Load font file contents into a local buffer for processing
    //
    std::string fontDescriptionFile(pPath + pFileName);
    CPUTFileSystem::ReadFileContents(fontDescriptionFile.c_str(), &fileSize, (void **) &pData, false, true);

    //
    // Validate the header info for the file
    //
    uint8_t fileIdentifier[3];

    fileIdentifier[0] = *(pData + currentPos++);
    fileIdentifier[1] = *(pData + currentPos++);
    fileIdentifier[2] = *(pData + currentPos++);

    if ((fileIdentifier[0] != 66) || (fileIdentifier[1] != 77) || (fileIdentifier[2] != 70)) {
        DEBUG_PRINT("File Identifier not valid for a BMFont binary file.");
        return NULL;
    }

    uint8_t version;
    version = *(pData + currentPos++);
    if (version != 3) {
        DEBUG_PRINT("BMFont file is not the correct version. Must be version 3.");
        return NULL;
    }

    //
    // Load the various blocks of the BMFont file
    //
    while (currentPos < fileSize) {
        uint8_t  blockType;
        uint32_t blockSize;
        blockType = *(pData + currentPos++);
        memcpy(&blockSize, pData + currentPos, 4);
        currentPos += 4;

        switch (blockType) {
        case 1:
            pNewFont->mpFontInfo = (BMFontInfo *) malloc(blockSize); 
            memcpy(pNewFont->mpFontInfo, pData + currentPos, blockSize);
            currentPos += blockSize;
            break;
        case 2:
            pNewFont->mpFontCommon = (BMFontCommon *) malloc(blockSize);
            memcpy(pNewFont->mpFontCommon, pData + currentPos, blockSize);
            currentPos += blockSize;
            break;
        case 3:
            pNewFont->mpFontPages = (BMFontPages *) malloc(blockSize);
            memcpy(pNewFont->mpFontPages, pData + currentPos, blockSize);
            currentPos += blockSize;
            break;
        case 4:
            pNewFont->mNumChars = blockSize / 20; // could do a check to make sure the block size is evenly divided by 20
            pNewFont->mpFontChars = (BMFontChars *) malloc(blockSize);
            memcpy(pNewFont->mpFontChars, pData + currentPos, blockSize);
            currentPos += blockSize;
            break;
        case 5:
            pNewFont->mNumKerningPairs = blockSize / 10; // could do a check to make sure the block size is evenly divided by 20
            pNewFont->mpFontKerningPairs = (BMFontKerningPairs *) malloc(blockSize);
            memcpy(pNewFont->mpFontKerningPairs, pData + currentPos, blockSize);
            {
            int testSize = sizeof(BMFontKerningPairs);
            
            currentPos += blockSize;
            }
            break;
        default:
            DEBUG_PRINT("Invalid block type");
            break;
        }
    }

    delete pData;

    //
    // Load the atlas for the font
    //

 //   cString atlasFile(s2cs(pNewFont->mpFontPages->pageNames));
 //   cString path(s2cs(pPath));

 //   pNewFont->pTextureAtlas = CPUTTexture::CreateTexture(atlasFile, path + atlasFile, false);

    // add font to the asset library
    CPUTAssetLibrary::GetAssetLibrary()->AddFont( s2cs(pFileName), _L(""), _L(""), pNewFont);

    return pNewFont;
}

void CPUTFont::LayoutText(CPUTGUIVertex *pVtxBuffer, int *pWidth, int *pHeight, const std::string& text, int tlx, int tly)
{
    int x = tlx, y = tly;
    BMFontChars *pPreviousChar = NULL;

    for (int j = 0, index = 0; j < text.length(); j++) {
        int charIndex = -1;
        for (int i = 0; i < mNumChars; i++) {
            if (((BMFontChars *)(((uint8_t *)(mpFontChars)) + (20 * i)))->id == text[j]) {
                charIndex = i;
            }
        }
        if (charIndex == -1) {
            DEBUG_PRINT("Invalid character being searched for value: %d, char: %c", (int)text[j], text[j]);
        }
        BMFontChars *pChar = ((BMFontChars *)(((uint8_t *)(mpFontChars)) + (20 * charIndex)));

        int kernAmount = 0;
        if (pPreviousChar != NULL) {
            for (int i = 0; i < mNumKerningPairs; i++) {
                if ((((BMFontKerningPairs *)(((uint8_t *)(mpFontKerningPairs)) + (10 * i)))->first == pPreviousChar->id) &&
                   (((BMFontKerningPairs *)(((uint8_t *)(mpFontKerningPairs)) + (10 * i)))->second == pChar->id)) {
                    kernAmount = ((BMFontKerningPairs *)(((uint8_t *)(mpFontKerningPairs)) + (10 * i)))->amount;
                }
            }
        }
        pPreviousChar = pChar;

        CPUTColor4 redColor;
        redColor.r = redColor.a = 1.0f;
        redColor.g = redColor.b = 1.0f;

        float texWidth = (float) mpFontCommon->scaleW;
        float texHeight = (float) mpFontCommon->scaleH;
        pVtxBuffer[index+0].Pos   = float3( x + 0.0f + pChar->xoffset + kernAmount, y + 0.0f + pChar->yoffset, 1.0f);
        pVtxBuffer[index+0].UV    = float2(pChar->x / texWidth, pChar->y / texHeight);
        pVtxBuffer[index+0].Color = redColor;

        pVtxBuffer[index+1].Pos   = float3( x + pChar->width + pChar->xoffset + kernAmount, y + 0.0f + pChar->yoffset, 1.0f);
        pVtxBuffer[index+1].UV    = float2((pChar->x + pChar->width) / texWidth, pChar->y / texHeight);
        pVtxBuffer[index+1].Color = redColor;

        pVtxBuffer[index+2].Pos   = float3( x + 0.0f + pChar->xoffset + kernAmount, y + pChar->height + pChar->yoffset, 1.0f);
        pVtxBuffer[index+2].UV    = float2(pChar->x / texWidth, (pChar->y + pChar->height) / texHeight);
        pVtxBuffer[index+2].Color = redColor;

        pVtxBuffer[index+3].Pos   = float3( x + pChar->width + pChar->xoffset + kernAmount, y + 0.0f + pChar->yoffset, 1.0f);
        pVtxBuffer[index+3].UV    = float2((pChar->x + pChar->width) / texWidth, pChar->y / texHeight);
        pVtxBuffer[index+3].Color = redColor;

        pVtxBuffer[index+4].Pos   = float3( x + pChar->width + pChar->xoffset + kernAmount, y + pChar->height + pChar->yoffset, 1.0f);
        pVtxBuffer[index+4].UV    = float2((pChar->x + pChar->width) / texWidth, (pChar->y + pChar->height) / texHeight);
        pVtxBuffer[index+4].Color = redColor;

        pVtxBuffer[index+5].Pos   = float3( x + 0.0f + pChar->xoffset + kernAmount, y + pChar->height + pChar->yoffset, 1.0f);
        pVtxBuffer[index+5].UV    = float2(pChar->x / texWidth, (pChar->y + pChar->height) / texHeight);
        pVtxBuffer[index+5].Color = redColor;

        x += pChar->xadvance + kernAmount;
        index += 6;
    }

    *pWidth = x - tlx;
    *pHeight = mpFontCommon->lineHeight;
}

CPUTFont::~CPUTFont()
{
    SAFE_DELETE(mpFontInfo);
    SAFE_DELETE(mpFontCommon);
    SAFE_DELETE(mpFontPages);
    SAFE_DELETE(mpFontChars);
    SAFE_DELETE(mpFontKerningPairs);

    SAFE_RELEASE(pTextureAtlas);
}

