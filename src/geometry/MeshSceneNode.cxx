/* bzflag
 * Copyright (c) 1993-2023 Tim Riker
 *
 * This package is free software;  you can redistribute it and/or
 * modify it under the terms of the license found in the file
 * named COPYING that should have accompanied this file.
 *
 * THIS PACKAGE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 */

// interface header
#include "MeshSceneNode.h"

// system headers
#include <math.h>
#include <string.h>
#include <stdlib.h>

// common implementation headers
#include "Extents.h"
#include "Intersect.h"
#include "TimeKeeper.h"

#include "MeshFace.h"
#include "MeshObstacle.h"
#include "MeshDrawInfo.h"
#include "MeshRenderNode.h"
#include "SceneNode.h"

#include "BzMaterial.h"
#include "OpenGLGState.h"
#include "OpenGLMaterial.h"
#include "DynamicColor.h"
#include "TextureMatrix.h"
#include "ViewFrustum.h"
#include "SceneRenderer.h"
#include "TextureManager.h"

#include "StateDatabase.h"
#include "BZDBCache.h"

//
// NOTE
//
//   This type of SceneNode does not support
//   tessellation or splitting of translucent
//   nodes for improved back-to-front sorting.
//


float MeshSceneNode::LodScale = 1.0f;
float MeshSceneNode::RadarLodScale = 1.0f;

MeshSceneNode::MeshSceneNode(const MeshObstacle* _mesh)
{
    mesh = _mesh;

    lodCount = 0;
    lods = NULL;

    if (mesh == NULL)
        return;

    drawInfo = mesh->getDrawInfo();
    if (drawInfo == NULL)
        return;

    drawMgr = drawInfo->getDrawMgr();
    if (drawMgr == NULL)
        return;

    // setup the extents, sphere, and lengthPerPixel adjustment
    float lengthAdj = 1.0f;
    const Extents& diExts = drawInfo->getExtents();
    const MeshTransform::Tool* xformTool = drawInfo->getTransformTool();
    if (xformTool == NULL)
    {
        extents = drawInfo->getExtents();
        setCenter(drawInfo->getPosition());
        setRadius(drawInfo->getRadius2());
    }
    else
    {
        // sloppy way to recalcuate the transformed extents
        glm::vec3 c[8];
        c[0] = diExts.mins;
        c[6][0] = c[5][0] = c[3][0] = diExts.mins[0];
        c[1][0] = c[2][0] = c[4][0] = diExts.maxs[0];
        c[1][1] = c[5][1] = c[4][1] = diExts.mins[1];
        c[6][1] = c[2][1] = c[3][1] = diExts.maxs[1];
        c[1][2] = c[2][2] = c[3][2] = diExts.mins[2];
        c[6][2] = c[5][2] = c[4][2] = diExts.maxs[2];
        c[7] = diExts.maxs;
        extents.reset();
        for (int v = 0; v < 8; v++)
        {
            xformTool->modifyVertex(c[v]);
            extents.expandToPoint(c[v]);
        }
        // lengthPerPixel adjustment
        lengthAdj = +MAXFLOAT;
        const auto &s = c[0]; // all mins
        for (int a = 0; a < 3; a++)
        {
            const float oldWidth = diExts.maxs[a] - diExts.mins[a];
            if (oldWidth <= 0.0f)
                continue;
            // mins, except: c[1] -> max[0], c[3] -> max[1], c[5] -> max[2]
            const auto &e = c[(a * 2) + 1];
            const float newWidth = glm::distance(s, e);
            const float scale = (newWidth / oldWidth);
            if (scale < lengthAdj)
                lengthAdj = scale;
        }
        // adjust the sphere
        auto myCenter = drawInfo->getPosition();
        xformTool->modifyVertex(myCenter);
        setCenter(myCenter);

        float myRadius = drawInfo->getRadius2() * lengthAdj * lengthAdj;
        setRadius(myRadius);
    }

    // setup lod/set nodes
    lods = NULL;
    lodCount = drawInfo->getLodCount();
    lods = new LodNode[lodCount];
    lodLengths = new float[lodCount];
    const DrawLod* drawLods = drawInfo->getDrawLods();
    for (int lod = 0; lod < lodCount; lod++)
    {
        LodNode& lodNode = lods[lod];
        const DrawLod& drawLod = drawLods[lod];
        lodLengths[lod] = drawLod.lengthPerPixel * lengthAdj; // lengthPerPixel
        lodNode.count = drawLods[lod].count;
        lodNode.sets = new SetNode[lodNode.count];
        for (int set = 0; set < lodNode.count; set++)
        {
            const DrawSet& drawSet = drawLod.sets[set];
            SetNode& setNode = lodNode.sets[set];
            setNode.node = NULL;
            setNode.radarNode = NULL;
            setNode.meshMat.bzmat = convertMaterial(drawSet.material);
        }
    }

    // FIXME - use alternate radar LODs
    radarCount = lodCount;
    radarLengths = new float[lodCount];
    memcpy(radarLengths, lodLengths, radarCount * sizeof(float));
    radarLods = lods;

    // build the transform display list
    makeXFormList();
    OpenGLGState::registerContextInitializer(freeContext, initContext, this);

    // build gstates and render nodes
    notifyStyleChange();

    return;
}


MeshSceneNode::~MeshSceneNode()
{
    //FIXME - delete radar lods  (once they're implemented)
    for (int i = 0; i < lodCount; i++)
    {
        LodNode& lod = lods[i];
        for (int j = 0; j < lod.count; j++)
        {
            SetNode& set = lod.sets[j];
            delete set.node;
            delete set.radarNode;
        }
        delete[] lod.sets;
    }
    delete[] lods;

    delete [] lodLengths;
    delete [] radarLengths;

    OpenGLGState::unregisterContextInitializer(freeContext, initContext, this);

    return;
}


inline int MeshSceneNode::calcNormalLod(const ViewFrustum& vf)
{
    const auto &e = vf.getEye();
    const auto &s = getSphere();
    const auto r = s - e;
    const auto d = glm::vec3(vf.getDirection());
    const float dist = glm::dot(d, r);
    const float lengthPerPixel = dist * LodScale;
    for (int i = (lodCount - 1); i > 0; i--)
    {
        if (lengthPerPixel > lodLengths[i])
            return i;
    }
    return 0;
}


inline int MeshSceneNode::calcRadarLod()
{
    for (int i = (radarCount - 1); i > 0; i--)
    {
        if (radarLengths[i] < RadarLodScale)
            return i;
    }
    return 0;
}


void MeshSceneNode::addRenderNodes(SceneRenderer& renderer)
{
    const ViewFrustum& vf = renderer.getViewFrustum();
    const int level = (lodCount == 1) ? 0 : calcNormalLod(vf);
    LodNode& lod = lods[level];

    if (animRepos)
    {
        const MeshTransform::Tool* xformTool = drawInfo->getTransformTool();
        const DrawLod* drawLods = drawInfo->getDrawLods();
        const AnimationInfo* animInfo = drawInfo->getAnimationInfo();
        const float& cos_val = animInfo->cos_val;
        const float& sin_val = animInfo->sin_val;
        for (int i = 0; i < lod.count; i++)
        {
            SetNode& set = lod.sets[i];
            if (set.meshMat.animRepos)
            {
                const auto &s = drawLods[level].sets[i].position;
                glm::vec3 pos;
                pos[0] = (cos_val * s[0]) - (sin_val * s[1]);
                pos[1] = (sin_val * s[0]) + (cos_val * s[1]);
                pos[2] = s[2];
                if (xformTool != NULL)
                    xformTool->modifyVertex(pos);
                ((AlphaGroupRenderNode*)set.node)->setPosition(pos);
            }
        }
    }

    for (int i = 0; i < lod.count; i++)
    {
        SetNode& set = lod.sets[i];
        if (set.meshMat.colorPtr->a != 0.0f)
            renderer.addRenderNode(set.node, &set.meshMat.gstate);
    }

    return;
}


void MeshSceneNode::addShadowNodes(SceneRenderer& renderer)
{
    const ViewFrustum& vf = renderer.getViewFrustum();
    const int level = (lodCount == 1) ? 0 : calcNormalLod(vf);
    LodNode& lod = lods[level];
    for (int i = 0; i < lod.count; i++)
    {
        SetNode& set = lod.sets[i];
        const MeshMaterial& mat = set.meshMat;
        if (mat.drawShadow && (mat.colorPtr->a != 0.0f))
            renderer.addShadowNode(set.node);
    }
    return;
}


void MeshSceneNode::renderRadar()
{
    const int level = (lodCount == 1) ? 0 : calcRadarLod();
    LodNode& lod = radarLods[level];
    for (int i = 0; i < lod.count; i++)
    {
        SetNode& set = lod.sets[i];
        if (set.meshMat.drawRadar)
            set.radarNode->renderRadar();
    }
    return;
}


bool MeshSceneNode::cull(const ViewFrustum& frustum) const
{
    // if the Visibility culler tells us that we're
    // fully visible, then skip the rest of these tests
    if (octreeState == OctreeVisible)
        return false;

    const Frustum* f = (const Frustum *) &frustum;
    if (testAxisBoxInFrustum(extents, f) == Outside)
        return true;

    // probably visible
    return false;
}


bool MeshSceneNode::inAxisBox(const Extents& exts) const
{
    // quick and dirty
    return extents.touches(exts);
}


void MeshSceneNode::notifyStyleChange()
{
    const DrawLod* drawLods = drawInfo->getDrawLods();

    animRepos = false;

    for (int lod = 0; lod < lodCount; lod++)
    {
        LodNode& lodNode = lods[lod];
        for (int set = 0; set < lodNode.count; set++)
        {
            SetNode& setNode = lodNode.sets[set];
            MeshMaterial& mat = setNode.meshMat;
            const DrawSet& drawSet = drawLods[lod].sets[set];

            delete setNode.node;
            delete setNode.radarNode;

            updateMaterial(&mat);

            // how shall we normalize?
            bool normalize = false;
            const MeshTransform::Tool* xformTool = drawInfo->getTransformTool();
            if (xformTool != NULL)
                normalize = xformTool->isSkewed();

            // enough elements to warrant disabling lights?
            const Extents* extPtr = &extents;
            if ((drawSet.triangleCount < 100) ||
                    !BZDBCache::lighting || mat.bzmat->getNoLighting())
                extPtr = NULL;

            if (!mat.needsSorting)
            {
                setNode.node =
                    new OpaqueRenderNode(drawMgr, xformMatrix, normalize,
                                         mat.colorPtr, lod, set, extPtr,
                                         drawSet.triangleCount);
                mat.animRepos = false;
            }
            else
            {
                auto setPos = drawSet.position;
                if (xformTool != NULL)
                    xformTool->modifyVertex(setPos);
                setNode.node =
                    new AlphaGroupRenderNode(drawMgr, xformMatrix, normalize,
                                             mat.colorPtr, lod, set, extPtr, setPos,
                                             drawSet.triangleCount);
                if ((fabsf(drawSet.position.x) > 0.001f) &&
                        (fabsf(drawSet.position.y) > 0.001f) &&
                        (mat.color[3] != 0.0f) &&
                        (drawInfo->getAnimationInfo() != NULL))
                {
                    animRepos = true;
                    mat.animRepos = true;
                }
                else
                    mat.animRepos = false;
            }

            setNode.radarNode =
                new OpaqueRenderNode(drawMgr, xformMatrix, normalize,
                                     mat.colorPtr, lod, set, extPtr,
                                     drawSet.triangleCount);
        }
    }
    return;
}


const BzMaterial* MeshSceneNode::convertMaterial(const BzMaterial* bzmat)
{
    const MaterialMap* matMap = drawInfo->getMaterialMap();
    if (matMap == NULL)
        return bzmat;

    MaterialMap::const_iterator it = matMap->find(bzmat);
    if (it == matMap->end())
        return bzmat;
    else
        return it->second;
}


void MeshSceneNode::updateMaterial(MeshSceneNode::MeshMaterial* mat)
{
// FIXME - deal with invisibility

    // get the references
    const BzMaterial*       bzmat = mat->bzmat;
    OpenGLGState&   gstate = mat->gstate;
    auto color = &mat->color;

    OpenGLGStateBuilder builder;
    TextureManager &tm = TextureManager::instance();

    bzmat->setReference();

    // ways of requiring blending
    bool colorAlpha = false;
    bool textureAlpha = false;

    bool useDiffuseColor = true;

    // texturing
    if (BZDBCache::texture)
    {
        bool userTexture = (bzmat->getTextureCount() > 0);
        if (userTexture)
        {
            int faceTexture = -1;
            const std::string& texname = bzmat->getTextureLocal(0);
            if (texname.size() > 0)
                faceTexture = tm.getTextureID(texname.c_str());
            if (faceTexture >= 0)
            {
                useDiffuseColor = bzmat->getUseColorOnTexture(0);
                if (bzmat->getUseTextureAlpha(0))
                {
                    const ImageInfo& imageInfo = tm.getInfo(faceTexture);
                    textureAlpha = imageInfo.alpha;
                }
            }
            else
                faceTexture = tm.getTextureID("mesh", false /* no failure reports */);
            if (faceTexture >= 0)
            {
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
                // sphere mapping
                if (bzmat->getUseSphereMap(0))
                    builder.enableSphereMap(true);
            }
            builder.setTexture(faceTexture);
            builder.enableTexture(true);
        }
    }

    // lighting
    if (BZDBCache::lighting && !bzmat->getNoLighting())
    {
        OpenGLMaterial oglMaterial(bzmat->getSpecular(),
                                   bzmat->getEmission(),
                                   bzmat->getShininess());
        builder.setMaterial(oglMaterial);
        builder.setShading(GL_SMOOTH);
    }
    else
        builder.setShading(GL_FLAT);

    // color
    if (useDiffuseColor)
    {
        *color = bzmat->getDiffuse();
        colorAlpha = (color->a != 1.0f);
    }
    else
    {
        // set it to white, this should only happen when
        // we've gotten a user texture, and there's a
        // request to not use the material's diffuse color.
        *color = glm::vec4(1.0f);
    }

    // dynamic color
    const DynamicColor* dyncol = DYNCOLORMGR.getColor(bzmat->getDynamicColor());
    if (dyncol != NULL)
    {
        mat->colorPtr = dyncol->getColor();
        colorAlpha = dyncol->canHaveAlpha(); // override
    }
    else
        mat->colorPtr = color;

    // blending
    const bool isAlpha = (colorAlpha || textureAlpha);
    if (isAlpha)
    {
        if (BZDBCache::blend)
        {
            builder.setBlending(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
            builder.setStipple(1.0f);
        }
        else
        {
            builder.resetBlending();
            if (dyncol != NULL)
                builder.setStipple(0.5f);
            else
                builder.setStipple(color->a);
        }
    }

    // sorting  (do this after using setBlending())
    //
    // NOTE:  getGroupAlpha() isn't used because all MeshSceneNode
    //    elements are sorted as groups rather then individually
    //
    mat->needsSorting = (isAlpha && !bzmat->getNoSorting());
    builder.setNeedsSorting(mat->needsSorting);

    // alpha thresholding
    float alphaThreshold = bzmat->getAlphaThreshold();
    if (alphaThreshold != 0.0f)
        builder.setAlphaFunc(GL_GEQUAL, alphaThreshold);

    // radar and shadows
    mat->drawRadar = !bzmat->getNoRadar();
    mat->drawShadow = !bzmat->getNoShadow();

    // culling
    if (bzmat->getNoCulling())
        builder.disableCulling();

    // generate the gstate
    gstate = builder.getState();

    return;
}


void MeshSceneNode::makeXFormList()
{
    GLenum error;
    const MeshTransform::Tool* xformTool = drawInfo->getTransformTool();
    if (xformTool != NULL)
    {
        int errCount = 0;
        // reset the error state
        while (true)
        {
            error = glGetError();
            if (error == GL_NO_ERROR)
                break;
            errCount++; // avoid a possible spin-lock?
            if (errCount > 666)
            {
                logDebugMessage(0,"ERROR: MeshSceneNode::makeXFormList() glError: %i\n", error);
                return; // don't make the list, something is borked
            }
        };

        // oops, transpose
        const auto &matrix = xformTool->getMatrix();
        for (int i = 0; i < 4; i++)
        {
            for (int j = 0; j < 4; j++)
                xformMatrix[(i*4)+j] = matrix[j][i];
        }
    }
    return;
}


void MeshSceneNode::initContext(void* data)
{
    ((MeshSceneNode*)data)->makeXFormList();
    return;
}


void MeshSceneNode::freeContext(void*)
{
}


void MeshSceneNode::getRenderNodes(std::vector<RenderSet>& rnodes)
{
    // NOTES:
    // - used for ShellNodes
    // - send out the highest LOD, because only used up close
    // - FIXME  only use opaque nodes?
    if (lodCount > 0)
    {
        LodNode& lodNode = lods[0];
        for (int set = 0; set < lodNode.count; set++)
        {
            SetNode& setNode = lodNode.sets[set];
            RenderSet rs = { setNode.node, &setNode.meshMat.gstate };
            rnodes.push_back(rs);
        }
    }
    return;
}


//////////////////////////
// Static Class Members //
//////////////////////////

void MeshSceneNode::setLodScale(int pixelsX, float fovx,
                                int pixelsY, float fovy)
{
    const float lppx = 2.0f * sinf(fovx * 0.5f) / (float)pixelsX;
    const float lppy = 2.0f * sinf(fovy * 0.5f) / (float)pixelsY;
    const float lpp = (lppx < lppy) ? lppx : lppy;
    LodScale = lpp * BZDB.eval("lodScale");
    return;
}


void MeshSceneNode::setRadarLodScale(float lengthPerPixel)
{
    RadarLodScale = lengthPerPixel * BZDB.eval("radarLodScale");
    return;
}


// Local Variables: ***
// mode: C++ ***
// tab-width: 4 ***
// c-basic-offset: 4 ***
// indent-tabs-mode: nil ***
// End: ***
// ex: shiftwidth=4 tabstop=4
