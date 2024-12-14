/* bzflag
 * Copyright (c) 1993-2023 Tim Riker
 * Written By Jeffrey Myers
 *
 * This package is free software;  you can redistribute it and/or
 * modify it under the terms of the license found in the file
 * named COPYING that should have accompanied this file.
 *
 * THIS PACKAGE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 */

#include "OpenGLUtils.h"

// system headers
#include <math.h>
#include <glm/gtc/type_ptr.hpp>

// common headers
#include "bzfio.h"
#include "BZDBCache.h"
#include "BzMaterial.h"
#include "DynamicColor.h"
#include "OpenGLGState.h"
#include "OpenGLMaterial.h"
#include "SceneRenderer.h"
#include "TextureManager.h"
#include "TextureMatrix.h"


//============================================================================//
//============================================================================//

void bzMat2gstate(const BzMaterial* bzmat, OpenGLGState& gstate,
                  glm::vec4 &color, const glm::vec4 *&colorPtr)
{
    bzmat->setReference();

    OpenGLGStateBuilder builder;

    // ways of requiring blending
    bool colorAlpha   = false;
    bool textureAlpha = false;

    bool useDiffuseColor = true;

    // texturing
    if (BZDBCache::texture && (bzmat->getTextureCount() > 0))
    {
        TextureManager &tm = TextureManager::instance();
        int texID = -1;

        const std::string& texName = bzmat->getTextureLocal(0);
        if (texName.size() > 0)
            texID = tm.getTextureID(texName.c_str());

        if (texID < 0)
            texID = tm.getTextureID("mesh", false /* no failure reports */);
        else
        {
            useDiffuseColor = bzmat->getUseColorOnTexture(0);
            if (bzmat->getUseTextureAlpha(0))
            {
                const ImageInfo& imageInfo = tm.getInfo(texID);
                textureAlpha = imageInfo.alpha;
            }
        }

        if (texID >= 0)
        {
            // sphere mapping
            if (bzmat->getUseSphereMap(0))
                builder.enableSphereMap(true);
            // texture matrix
            const int texMatId = bzmat->getTextureMatrix(0);
            const TextureMatrix* texmat = TEXMATRIXMGR.getMatrix(texMatId);
            if (texmat != NULL)
            {
                const GLfloat* matrix = texmat->getMatrix();
                if (matrix != NULL)
                {
                    builder.setTextureMatrix(matrix);
                    builder.enableTextureMatrix(true);
                }
            }
            // setup the builder
            builder.setTexture(texID);
            builder.enableTexture(true);
        }
    }

    // lighting
    builder.setShading(GL_FLAT);
    if (BZDBCache::lighting && !bzmat->getNoLighting())
    {
        OpenGLMaterial oglMaterial(bzmat->getSpecular(),
                                   bzmat->getEmission(),
                                   bzmat->getShininess());
        builder.setMaterial(oglMaterial);
    }

    // color
    if (useDiffuseColor)
    {
        color = bzmat->getDiffuse();
        colorAlpha = (color.a < 1.0f);
    }
    else
    {
        // set it to white, this should only happen when
        // we've gotten a user texture, and there's a
        // request to not use the material's diffuse color.
        color = glm::vec4(1.0f);
    }

    // dynamic color
    const DynamicColor* dyncol = DYNCOLORMGR.getColor(bzmat->getDynamicColor());
    if (dyncol != NULL)
    {
        const auto c = dyncol->getColor();
        // Do we free it ?
        colorPtr = new glm::vec4(*c);
        colorAlpha = dyncol->canHaveAlpha(); // override
    }
    else
        colorPtr = &color;

    // blending
    bool needsSorting   = false;
    bool blendingForced = false;

    if (!blendingForced)
    {
        const bool isAlpha = (colorAlpha || textureAlpha);
        if (isAlpha)
        {
            if (BZDBCache::blend)
            {
                builder.setBlending(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
                builder.setStipple(1.0f);
                needsSorting = true;
            }
            else
            {
                builder.resetBlending();
                if (dyncol != NULL)
                    builder.setStipple(0.5f);
                else
                    builder.setStipple(color.a);
            }
        }
    }

    // sorting  (do this after using setBlending())
    builder.setNeedsSorting(needsSorting && !bzmat->getNoSorting());

    // alpha thresholding
    float alphaThreshold = bzmat->getAlphaThreshold();
    if (alphaThreshold != 0.0f)
        builder.setAlphaFunc(GL_GEQUAL, alphaThreshold);

    // culling
    if (bzmat->getNoCulling())
        builder.disableCulling();

    // generate the gstate
    gstate = builder.getState();
}


//============================================================================//
//============================================================================//

static bool parseBlendFactor(const std::string& s, GLenum& factor)
{
    static std::map<std::string, GLenum> factors;
    if (factors.empty())
    {
        factors["one"]  = GL_ONE;
        factors["zero"] = GL_ZERO;
        factors["src_alpha"] = GL_SRC_ALPHA;
        factors["src_color"] = GL_SRC_COLOR;
        factors["dst_alpha"] = GL_DST_ALPHA;
        factors["dst_color"] = GL_DST_COLOR;
        factors["one_minus_src_alpha"] = GL_ONE_MINUS_SRC_ALPHA;
        factors["one_minus_src_color"] = GL_ONE_MINUS_SRC_COLOR;
        factors["one_minus_dst_alpha"] = GL_ONE_MINUS_DST_ALPHA;
        factors["one_minus_dst_color"] = GL_ONE_MINUS_DST_COLOR;
        factors["src_alpha_saturate"] = GL_SRC_ALPHA_SATURATE;
    }
    const std::map<std::string, GLenum>::const_iterator it = factors.find(s);
    if (it == factors.end())
        return false;
    factor = it->second;
    return true;
}


bool parseBlendFactors(const std::string& s, GLenum& src, GLenum& dst)
{
    if (s == "disable")
    {
        src = GL_ONE;
        dst = GL_ZERO;
        return true;
    }
    else if (s == "add")
    {
        src = GL_ONE;
        dst = GL_ONE;
        return true;
    }
    else if (s == "addalpha")
    {
        src = GL_SRC_ALPHA;
        dst = GL_ONE;
        return true;
    }
    else if (s == "modulate")
    {
        src = GL_DST_COLOR;
        dst = GL_ZERO;
        return true;
    }
    else if (s == "alpha")
    {
        src = GL_SRC_ALPHA;
        dst = GL_ONE_MINUS_SRC_ALPHA;
        return true;
    }
    else if (s == "color")
    {
        src = GL_SRC_COLOR;
        dst = GL_ONE_MINUS_SRC_COLOR;
        return true;
    }

    const std::string::size_type pos = s.find('/');
    if (pos == std::string::npos)
        return false;
    const std::string srcStr = s.substr(0, pos);
    const std::string dstStr = s.substr(pos + 1);
    if (!parseBlendFactor(srcStr, src) ||
            !parseBlendFactor(dstStr, dst))
        return false;

    return true;
}


//============================================================================//
//============================================================================//

float getFloatColor(int val)
{
    return val / 255.0f;
}

void setColor(float dst[3], int r, int g, int b)
{
    dst[0] = getFloatColor(r);
    dst[1] = getFloatColor(g);
    dst[2] = getFloatColor(b);
}

void glSetColor(const float c[3], float alpha)
{
    glColor4f(c[0], c[1], c[2], alpha);
}

void glTranslatefv (const float v[3] )
{
    glTranslatef(v[0], v[1], v[2]);
}

#define GL_RAD_CON 0.017453292519943295769236907684886f


// Local Variables: ***
// mode: C++ ***
// tab-width: 4 ***
// c-basic-offset: 4 ***
// indent-tabs-mode: nil ***
// End: ***
// ex: shiftwidth=4 tabstop=4
