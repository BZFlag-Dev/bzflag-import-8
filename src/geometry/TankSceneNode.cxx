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
#include "TankSceneNode.h"

// system headers
#include <math.h>
#include <string.h>
#include <glm/gtc/random.hpp>

// common implementation headers
#include "StateDatabase.h"
#include "SceneRenderer.h"
#include "BZDBCache.h"
#include "OpenGLAPI.h"

// local implementation headers
#include "ViewFrustum.h"

#include "TextureManager.h"

#define _NO_LIST_ID 0xffffffff

using namespace TankGeometryEnums;

static const float MuzzleMaxX = 4.94f;
static const float maxExplosionVel = 40.0f;
static const float vertExplosionRatio = 0.5f;


// parts: body, turret, barrel, left tread, right tread

const int TankSceneNode::numLOD = 3;
int       TankSceneNode::maxLevel = numLOD;

TankSceneNode::TankSceneNode(const glm::vec3 &pos, const glm::vec3 &forward) :
    leftTreadOffset(0.0f), rightTreadOffset(0.0f),
    leftWheelOffset(0.0f), rightWheelOffset(0.0f),
    useDimensions(false), onlyShadows(false),
    clip(false), inTheCockpit(false), tankRenderNode(this),
    treadsRenderNode(this), shadowRenderNode(this),
    tankSize(TankGeometryEnums::Normal)
{
    // setup style factors (BZDB isn't set up at global init time
    // prepare geometry
    deathOverride = NULL;

    move(pos, forward);
    float length = BZDBCache::tankLength;
    length = 0.5f * (length + MuzzleMaxX);
    const float width = BZDBCache::tankWidth;
    const float height = 0.5f * BZDBCache::tankHeight;

    baseRadius = (length * length) + (width * width) + (height * height);
    setRadius(baseRadius);

    color[3] = 1.0f;
    setColor(1.0f, 1.0f, 1.0f);
    setExplodeFraction(0.0f);
    setJumpJets(0.0f);

    rebuildExplosion();

    shadowRenderNode.setShadow();
    shadowRenderNode.setTankLOD(LowTankLOD);

    jumpJetsRealLight.setAttenuation(0, 0.05f);
    jumpJetsRealLight.setAttenuation(1, 0.0f);
    jumpJetsRealLight.setAttenuation(2, 0.03f);
    jumpJetsRealLight.setOnlyReal(true);
    for (int i = 0; i < 4; i++)
    {
        jumpJetsGroundLights[i].setAttenuation(0, 0.05f);
        jumpJetsGroundLights[i].setAttenuation(1, 0.0f);
        jumpJetsGroundLights[i].setAttenuation(2, 0.03f);
        jumpJetsGroundLights[i].setOnlyGround(true);
    }

    return;
}


TankSceneNode::~TankSceneNode()
{
    // do nothing
}


void TankSceneNode::setColor(GLfloat r, GLfloat g, GLfloat b, GLfloat a)
{
    color = glm::vec4(r, g, b, a);
    transparent = (color[3] != 1.0f);
}


void TankSceneNode::setColor(const glm::vec4 &rgba)
{
    color = rgba;
    transparent = (color[3] != 1.0f);
}


void TankSceneNode::setMaterial(const OpenGLMaterial& mat)
{
    OpenGLGStateBuilder builder(gstate);
    builder.setMaterial(mat);
    gstate = builder.getState();
}


void TankSceneNode::setTexture(const int texture)
{
    OpenGLGStateBuilder builder(gstate);
    builder.setTexture(texture);
    gstate = builder.getState();
}


void TankSceneNode::setJumpJetsTexture(const int texture)
{
    OpenGLGStateBuilder builder(jumpJetsGState);
    builder.setTexture(texture);
    jumpJetsGState = builder.getState();
}


void TankSceneNode::move(const glm::vec3 &pos, const glm::vec3 &forward)
{
    const float rad2deg = (float)(180.0 / M_PI);
    azimuth = rad2deg * atan2f(forward[1], forward[0]);
    elevation = -rad2deg * atan2f(forward[2], hypotf(forward[0], forward[1]));
    setCenter(pos);

    // setup the extents
    const float maxRadius = 0.5f * (BZDBCache::tankLength + MuzzleMaxX);
    extents.mins[0] = pos[0] - maxRadius;
    extents.mins[1] = pos[1] - maxRadius;
    extents.mins[2] = pos[2];
    extents.maxs[0] = pos[0] + maxRadius;
    extents.maxs[1] = pos[1] + maxRadius;
    extents.maxs[2] = pos[2] + BZDBCache::tankHeight;
    return;
}


void TankSceneNode::addTreadOffsets(float left, float right)
{
    const float wheelScale = TankGeometryUtils::getWheelScale();
    const float treadScale = TankGeometryUtils::getTreadScale();
    const float treadTexLen = TankGeometryUtils::getTreadTexLen();

    leftTreadOffset += left * treadScale;
    leftTreadOffset = fmodf (leftTreadOffset, treadTexLen);
    leftWheelOffset += left * wheelScale;
    leftWheelOffset = fmodf (leftWheelOffset, 360.0f);

    rightTreadOffset += right * treadScale;
    rightTreadOffset = fmodf (rightTreadOffset, treadTexLen);
    rightWheelOffset += right * wheelScale;
    rightWheelOffset = fmodf (rightWheelOffset, 360.0f);

    return;
}


void TankSceneNode::notifyStyleChange()
{
    sort = !BZDBCache::zbuffer;
    OpenGLGStateBuilder builder(gstate);
    builder.enableTexture(BZDBCache::texture);
    builder.enableMaterial(BZDBCache::lighting);
    builder.setSmoothing(BZDBCache::smooth);
    if (BZDBCache::blend && transparent)
    {
        builder.setBlending(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        builder.setStipple(1.0f);
    }
    else
    {
        builder.resetBlending();
        builder.setStipple(transparent ? 0.5f : 1.0f);
    }
    gstate = builder.getState();

    builder.setTexture(TextureManager::instance().getTextureID("treads"));
    treadState = builder.getState();

    OpenGLGStateBuilder builder2(lightsGState);
    if (BZDBCache::smooth)
    {
        builder2.setBlending(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        builder2.setSmoothing();
    }
    else
    {
        builder2.resetBlending();
        builder2.setSmoothing(false);
    }
    lightsGState = builder2.getState();


    OpenGLGStateBuilder builder3(jumpJetsGState);
    builder3.disableCulling();
    builder3.setBlending(GL_SRC_ALPHA, GL_ONE);
    jumpJetsGState = builder3.getState();
}


void TankSceneNode::addRenderNodes(SceneRenderer& renderer)
{
    // pick level of detail
    const float myRadius = getRadius2();
    const ViewFrustum& view = renderer.getViewFrustum();
    const auto &eye = view.getEye();
    const float size = myRadius * (view.getAreaFactor() /getDistance(eye));

    // set the level of detail
    TankLOD mode = LowTankLOD;
    if ((maxLevel == -1) || ((maxLevel > 2) && (size > 55.0f)))
        mode = HighTankLOD;
    else if ((maxLevel > 1) && (size > 25.0f))
        mode = MedTankLOD;
    else
        mode = LowTankLOD;
    tankRenderNode.setTankLOD(mode);
    treadsRenderNode.setTankLOD(mode);

    // set the tank's scaling size
    tankRenderNode.setTankSize(tankSize);
    treadsRenderNode.setTankSize(tankSize);

    bool narrow = false;
    if ((tankSize == Narrow) &&
            (!useDimensions || (dimensions[1] < 0.01f)) &&
            BZDBCache::animatedTreads && BZDBCache::zbuffer)
        narrow = true;
    tankRenderNode.setNarrowWithDepth(narrow);
    treadsRenderNode.setNarrowWithDepth(narrow);

    // if drawing in sorted order then decide which order
    if (sort || transparent || narrow)
    {
        const auto &mySphere = getSphere();
        const auto d = eye - mySphere;
        const float radians = (float)(azimuth * M_PI / 180.0);
        const float cos_val = cosf(radians);
        const float sin_val = sinf(radians);

        const float frontDot = (cos_val * d.x) + (sin_val * d.y);
        const bool towards = (frontDot > 0.0f);

        const float leftDot = (-sin_val * d.x) + (cos_val * d.y);
        const bool left = (leftDot > 0.0f);

        const bool above = d.z > 0.0f;

        tankRenderNode.sortOrder(above, towards, left);
    }

    treadsRenderNode.setTreads(true);

    // don't draw onlyShadows tanks.  this is mainly to avoid drawing player's
    // tank when player is using view from tank.  can't simply not include
    // player, though, cos then we wouldn't get the tank's shadow.
    //
    // Also don't return without doing anything,
    // fancy radar tanks need TankLOD set.
    if (onlyShadows)
        return;

    renderer.addRenderNode(&tankRenderNode, &gstate);
    renderer.addRenderNode(&treadsRenderNode, &treadState);

}


void TankSceneNode::addShadowNodes(SceneRenderer& renderer)
{
    // use HighTankLOD shadows in experimental mode
    if (TankSceneNode::maxLevel == -1)
        shadowRenderNode.setTankLOD (HighTankLOD);
    else
        shadowRenderNode.setTankLOD (LowTankLOD);
    renderer.addShadowNode(&shadowRenderNode);
}


void TankSceneNode::addLight(SceneRenderer& renderer)
{
    if (jumpJetsOn)
    {
        // the real light
        jumpJetsRealLight.setColor(jumpJetsScale * 1.5f * 2.0f,
                                   jumpJetsScale * 1.0f * 2.0f,
                                   jumpJetsScale * 0.5f * 2.0f);
        renderer.addLight(jumpJetsRealLight);
        // the ground lights
        for (int i = 0; i < 4; i++)
        {
            jumpJetsGroundLights[i].setColor(jumpJetsLengths[i] * 1.5f * 0.5f,
                                             jumpJetsLengths[i] * 1.0f * 0.5f,
                                             jumpJetsLengths[i] * 0.5f * 0.5f);
            renderer.addLight(jumpJetsGroundLights[i]);
        }
    }
    return;
}


void TankSceneNode::setNormal()
{
    tankSize = Normal;
    setRadius(baseRadius);
    useDimensions = false;
}


void TankSceneNode::setObese()
{
    tankSize = Obese;
    float factor = BZDB.eval(StateDatabase::BZDB_OBESEFACTOR);
    setRadius(factor*factor*baseRadius);
    useDimensions = false;
}


void TankSceneNode::setTiny()
{
    tankSize = Tiny;
    float factor = BZDB.eval(StateDatabase::BZDB_TINYFACTOR);
    setRadius(factor*factor*baseRadius);
    useDimensions = false;
}


void TankSceneNode::setNarrow()
{
    tankSize = Narrow;
    setRadius(baseRadius);
    useDimensions = false;
}


void TankSceneNode::setThief()
{
    tankSize = Thief;
    float factor = BZDB.eval(StateDatabase::BZDB_THIEFTINYFACTOR);
    setRadius(factor*factor*baseRadius);
    useDimensions = false;
}


void TankSceneNode::setDimensions(const glm::vec3 &dims)
{
    tankSize = Normal;
    dimensions = dims;
    useDimensions = true;
    return;
}


void TankSceneNode::setExplodeFraction(float t)
{
    explodeFraction = t;
    if (t != 0.0f)
    {
        const float radius = sqrtf(getRadius2());
        const float radinc = t * maxExplosionVel;
        const float newrad = radius + radinc;
        setRadius(newrad * newrad);
    }
    return;
}


void TankSceneNode::setJumpJets(float scale)
{
    jumpJetsOn = false;
    if ((scale > 0.0f) && BZDBCache::zbuffer && BZDBCache::texture)
    {
        jumpJetsOn = true;
        jumpJetsScale = scale;

        // set the real light's position
        const auto &pos = getSphere();
        jumpJetsRealLight.setPosition(pos);

        // set the jet ground-light and model positions
        const float radians = (float)(azimuth * (M_PI / 180.0));
        const float cos_val = cosf(radians);
        const float sin_val = sinf(radians);
        const auto &scaleFactor = TankGeometryMgr::getScaleFactor(tankSize);
        for (int i = 0; i < 4; i++)
        {
            const auto &jm = jumpJetsModel[i];
            const float v[2] = {jm[0] * scaleFactor[0], jm[1] * scaleFactor[1]};
            auto &jetPos = jumpJetsPositions[i];
            jetPos = pos;
            jetPos[0] += cos_val * v[0] - sin_val * v[1];
            jetPos[1] += cos_val * v[1] + sin_val * v[0];
            jetPos[2] += jm[2];
            jumpJetsGroundLights[i].setPosition(jetPos);

            // setup the random lengths
            const float randomFactor = glm::linearRand(0.75f, 1.25f);
            jumpJetsLengths[i] = jumpJetsScale * randomFactor;
        }
    }
    return;
}


void TankSceneNode::setClipPlane(const glm::vec4 &_plane)
{
    clip = true;
    clipPlane = _plane;
}


void TankSceneNode::resetClipPlane()
{
    clip = false;
}


void TankSceneNode::setMaxLOD(int _maxLevel)
{
    maxLevel = _maxLevel;
    return;
}


void TankSceneNode::setOnlyShadows(bool _onlyShadows)
{
    onlyShadows = _onlyShadows;
    return;
}


void TankSceneNode::setInTheCockpit(bool value)
{
    inTheCockpit = value;
    return;
}


void TankSceneNode::rebuildExplosion()
{
    // prepare explosion rotations and translations
    const auto maxSpin = glm::vec3(0.5f);
    for (int i = 0; i < LastTankPart; i++)
    {
        // pick an unbiased rotation vector
        glm::vec3 spinVect;
        GLfloat d;
        do
        {
            spinVect = glm::linearRand(-maxSpin, maxSpin);
            d = glm::length(spinVect);
        }
        while (d < 0.001f || d > 0.5f);
        spinVect /= d;

        // now an angular velocity -- make sure we get at least 2 complete turns
        spin[i] = glm::vec4(spinVect, 360.0f * glm::linearRand(2.0f, 7.0f));

        // cheezy spheroid explosion pattern
        const float vhMax = maxExplosionVel;
        const float vhAng = (float)(M_PI * 0.5 * bzfrand());
        const float vhMag = vhMax * sinf(vhAng);
        const float vz    = vhMax * cosf(vhAng);
        const float vhAngle = (float)(2.0 * M_PI * bzfrand());
        vel[i][0] = cosf(vhAngle) * vhMag;
        vel[i][1] = sinf(vhAngle) * vhMag;
        vel[i][2] = vz * vertExplosionRatio; // flatten it a little
        if (bzfrand() > 0.5)
            vel[i][2] = -vel[i][2];
    }
    return;
}


void TankSceneNode::renderRadar()
{
    const float angleCopy = azimuth;
    const auto &mySphere = getSphere();
    auto posCopy = mySphere;

    // allow negative values for burrowed clipping
    auto tankPos = glm::vec3(0.0f);
    if (mySphere[2] < 0.0f)
        tankPos[2] = mySphere[2];

    setCenter(tankPos);
    azimuth = 0.0f;

    float oldAlpha = color[3];
    color[3] = std::max(color[3], 0.15f);

    if (BZDBCache::animatedTreads)
    {
        treadState.setState();
        treadsRenderNode.setRadar(true);
        treadsRenderNode.sortOrder(true /* above */, false, false);
        treadsRenderNode.render();
        treadsRenderNode.setRadar(false);
    }
    gstate.setState();

    tankRenderNode.setRadar(true);
    tankRenderNode.sortOrder(true /* above */, false, false);
    tankRenderNode.render();
    tankRenderNode.setRadar(false);

    color[3] = oldAlpha;

    setCenter(posCopy);
    azimuth = angleCopy;

    return;
}


bool TankSceneNode::cullShadow(int planeCount, const glm::vec4 *planes) const
{
    const auto s = glm::vec4(getSphere(), 1.0f);
    const float r = getRadius2();
    for (int i = 0; i < planeCount; i++)
    {
        const auto &p = planes[i];
        const float d = glm::dot(p, s);
        if ((d < 0.0f) && (d * d > r))
            return true;
    }
    return false;
}


//
// TankIDLSceneNode
//

TankIDLSceneNode::TankIDLSceneNode(const TankSceneNode* _tank) :
    tank(_tank),
    renderNode(this)
{
    static const auto defaultPlane = glm::vec4(1.0f, 0.0f, 0.0f, 0.0f);
    move(defaultPlane);
    float radius = BZDBCache::tankLength;
    radius = radius * 4.0f;
    setRadius(radius);

    OpenGLGStateBuilder builder(gstate);
    builder.disableCulling();
    builder.setShading(GL_SMOOTH);
    builder.setBlending(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    gstate = builder.getState();
    return;
}


TankIDLSceneNode::~TankIDLSceneNode()
{
    // do nothing
    return;
}


void TankIDLSceneNode::move(const glm::vec4 &_plane)
{
    plane = _plane;

    // compute new sphere
    const auto &s = tank->getSphere();
    setCenter(s + 1.5f * BZDBCache::tankLength * glm::vec3(plane));
    return;
}


void TankIDLSceneNode::notifyStyleChange()
{
    OpenGLGStateBuilder builder(gstate);
    if (BZDBCache::blend)
    {
        builder.setBlending(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        builder.setStipple(1.0f);
    }
    else
    {
        builder.resetBlending();
        builder.setStipple(0.5f);
    }
    gstate = builder.getState();
    return;
}


void TankIDLSceneNode::addRenderNodes(SceneRenderer& renderer)
{
    renderer.addRenderNode(&renderNode, &gstate);
    return;
}

//
// TankIDLSceneNode::IDLRenderNode
//

const int TankIDLSceneNode::IDLRenderNode::idlFaces[][5] =
{
    // body
    { 4,  1, 0, 4, 5 },
    { 4,  5, 4, 2, 3 },
    { 4,  3, 2, 7, 6 },
    { 4,  6, 7, 0, 1 },
    // turret
    { 3,  12, 15, 10 },
    { 3,  12, 10, 9 },
    { 3,  13, 8, 11 },
    { 3,  13, 11, 14 },
    { 4,  15, 14, 11, 10 },
    { 4,  10, 11, 8, 9 },
    { 4,  9, 8, 13, 12 },
    // barrel
    { 4,  21, 17, 18, 22 },
    { 4,  22, 18, 19, 23 },
    { 4,  23, 19, 16, 20 },
    { 4,  20, 16, 17, 21 },
    { 4,  17, 16, 19, 18 },
    // ltread
    { 4,  29, 26, 25, 28 },
    { 4,  28, 25, 27, 30 },
    { 4,  30, 27, 31, 24 },
    { 4,  24, 31, 26, 29 },
    { 4,  25, 26, 31, 27 },
    // rtread
    { 4,  37, 34, 33, 36 },
    { 4,  36, 33, 35, 38 },
    { 4,  38, 35, 39, 32 },
    { 4,  32, 39, 34, 37 },
    { 4,  37, 36, 38, 32 }
};

const glm::vec3 TankIDLSceneNode::IDLRenderNode::idlVertex[] =
{
    // body
    { 2.430f, 0.877f, 0.000f },
    { 2.430f, -0.877f, 0.000f },
    { -2.835f, 0.877f, 1.238f },
    { -2.835f, -0.877f, 1.238f },
    { 2.575f, 0.877f, 1.111f },
    { 2.575f, -0.877f, 1.111f },
    { -2.229f, -0.877f, 0.000f },
    { -2.229f, 0.877f, 0.000f },
    // turret
    { -1.370f, 0.764f, 2.050f },
    { -1.370f, -0.765f, 2.050f },
    { 1.580f, -0.434f, 1.790f },
    { 1.580f, 0.435f, 1.790f },
    { -0.456f, -1.060f, 1.040f },
    { -0.456f, 1.080f, 1.040f },
    { 1.480f, 0.516f, 1.040f },
    { 1.480f, -0.516f, 1.040f },
    // barrel
    { 4.940f, 0.047f, 1.410f },
    { 4.940f, -0.079f, 1.530f },
    { 4.940f, 0.047f, 1.660f },
    { 4.940f, 0.173f, 1.530f },
    { 1.570f, 0.047f, 1.350f },
    { 1.570f, -0.133f, 1.530f },
    { 1.570f, 0.047f, 1.710f },
    { 1.570f, 0.227f, 1.530f },
    // ltread
    { -2.229f, 0.877f, 0.000f },
    { 2.730f, 1.400f, 1.294f },
    { 2.597f, 1.400f, 0.000f },
    { -2.970f, 1.400f, 1.410f },
    { 2.730f, 0.877f, 1.294f },
    { 2.597f, 0.877f, 0.000f },
    { -2.970f, 0.877f, 1.410f },
    { -2.229f, 1.400f, 0.000f },
    // rtread
    { -2.229f, -1.400f, 0.000f },
    { 2.730f, -0.875f, 1.294f },
    { 2.597f, -0.875f, 0.000f },
    { -2.970f, -0.875f, 1.410f },
    { 2.730f, -1.400f, 1.294f },
    { 2.597f, -1.400f, 0.000f },
    { -2.970f, -1.400f, 1.410f },
    { -2.229f, -0.875f, 0.000f }
};


TankIDLSceneNode::IDLRenderNode::IDLRenderNode(
    const TankIDLSceneNode* _sceneNode) :
    sceneNode(_sceneNode)
{
    // do nothing
    return;
}


TankIDLSceneNode::IDLRenderNode::~IDLRenderNode()
{
    // do nothing
    return;
}

const glm::vec3 &TankIDLSceneNode::IDLRenderNode::getPosition() const
{
    return sceneNode->getSphere();
}

void TankIDLSceneNode::IDLRenderNode::render()
{
    static const auto innerColor = glm::vec4(1.0f, 1.0f, 1.0f, 0.75f);
    static const auto outerColor = glm::vec4(1.0f, 1.0f, 1.0f, 0.0f);

    // compute plane in tank's space
    const auto sphere = glm::vec4(sceneNode->tank->getSphere(), 1.0f);
    const auto &_plane = sceneNode->plane;
    const GLfloat azimuth = sceneNode->tank->azimuth;
    const GLfloat ca = cosf(-azimuth * (float)M_PI / 180.0f);
    const GLfloat sa = sinf(-azimuth * (float)M_PI / 180.0f);
    glm::vec4 tankPlane;
    tankPlane[0] = ca * _plane[0] - sa * _plane[1];
    tankPlane[1] = sa * _plane[0] + ca * _plane[1];
    tankPlane[2] = _plane[2];
    tankPlane[3] = glm::dot(sphere, _plane);

    // compute projection point -- one TankLength in from tankPlane
    const GLfloat pd = -1.0f * BZDBCache::tankLength - tankPlane[3];
    auto origin = pd * glm::vec3(tankPlane);

    glPushMatrix();
    glTranslate(sphere);
    glRotatef(azimuth, 0.0f, 0.0f, 1.0f);

    const int numFaces = bzcountof(idlFaces);
    for (int i = 0; i < numFaces; i++)
    {
        // get distances from tankPlane
        const int* face = idlFaces[i] + 1;
        const int numVertices = idlFaces[i][0];
        GLfloat d[4];
        int j;
        for (j = 0; j < numVertices; j++)
        {
            const auto v = glm::vec4(idlVertex[face[j]], 1.0f);
            d[j] = glm::dot(v, tankPlane);
        }

        // get crossing points
        glm::vec3 cross[2];
        int crossings = 0, k;
        for (j = 0, k = numVertices-1; j < numVertices; k = j++)
        {
            if ((d[k] < 0.0f && d[j] >= 0.0f) || (d[k] >= 0.0f && d[j] < 0.0f))
            {
                const GLfloat t = d[k] / (d[k] - d[j]);
                cross[crossings] = glm::mix(
                                       idlVertex[face[k]],
                                       idlVertex[face[j]],
                                       t);
                if (++crossings == 2) break;
            }
        }

        // if not enough crossings then skip
        if (crossings != 2) continue;

        // project points out
        glm::vec3 project[2];
        const GLfloat dist = glm::linearRand(1.85f, 2.15f);
        project[0] = glm::mix(origin, cross[0], dist);
        project[1] = glm::mix(origin, cross[1], dist);

        // draw it
        glBegin(GL_TRIANGLE_STRIP);
        myColor4fv(innerColor);
        glVertex(cross[0]);
        glVertex(cross[1]);
        myColor4fv(outerColor);
        glVertex(project[0]);
        glVertex(project[1]);
        glEnd();
    }

    glPopMatrix();
    return;
}


//
// TankSceneNode::TankRenderNode
//

const glm::vec3  // FIXME: setup so these come from TANKGEOMMGR
TankSceneNode::TankRenderNode::centerOfGravity[LastTankPart] =
{
    { 0.000f,  0.0f, 1.5f * 0.68f }, // body
    { 3.252f,  0.0f, 1.532f },       // barrel
    { 0.125f,  0.0f, 2.5f * 0.68f }, // turret
    { 0.000f, +0.7f, 0.5f * 0.68f }, // left case
    { 0.000f, -0.7f, 0.5f * 0.68f }, // right case
    { 0.000f, +0.7f, 0.7f }, // left tread
    { 0.000f, -0.9f, 0.7f }, // right tread
    { -2.25f, +0.9f, 0.7f }, // left wheel0
    { -0.75f, +0.9f, 0.7f }, // left wheel1
    { +0.75f, +0.9f, 0.7f }, // left wheel2
    { +2.25f, +0.9f, 0.7f }, // left wheel3
    { -2.25f, -0.9f, 0.7f }, // right wheel0
    { -0.75f, -0.9f, 0.7f }, // right wheel1
    { +0.75f, -0.9f, 0.7f }, // right wheel2
    { +2.25f, -0.9f, 0.7f }  // right wheel3
};


TankSceneNode::TankRenderNode::TankRenderNode(const TankSceneNode* _sceneNode) :
    sceneNode(_sceneNode), isShadow(false),
    left(false), above(false), towards(false)
{
    narrowWithDepth = false;
    drawLOD = LowTankLOD;
    drawSize = Normal;
    isRadar = false;
    isTreads = false;
    return;
}


TankSceneNode::TankRenderNode::~TankRenderNode()
{
    return;
}

const glm::vec3 &TankSceneNode::TankRenderNode::getPosition() const
{
    return sceneNode->getSphere();
}

void TankSceneNode::TankRenderNode::setRadar(bool radar)
{
    isRadar = radar;
    return;
}


void TankSceneNode::TankRenderNode::setShadow()
{
    isShadow = true;
    return;
}

void TankSceneNode::TankRenderNode::setTreads(bool treads )
{
    isTreads = treads;
}

void TankSceneNode::TankRenderNode::sortOrder(
    bool _above, bool _towards, bool _left)
{
    above = _above;
    towards = _towards;
    left = _left;
    return;
}


void TankSceneNode::TankRenderNode::setNarrowWithDepth(bool narrow)
{
    narrowWithDepth = narrow;
    return;
}


void TankSceneNode::TankRenderNode::setTankLOD(TankLOD lod)
{
    drawLOD = lod;
    return;
}


void TankSceneNode::TankRenderNode::setTankSize(TankSize size)
{
    drawSize = size;
    return;
}


void TankSceneNode::TankRenderNode::render()
{
    if (!sceneNode->useDimensions)
        drawSize = sceneNode->tankSize;
    else
    {
        // for animated resizing effects, setup with the Normal size,
        // and let useDimensions and glScalef() handle the scaling
        drawSize = Normal;
    }

    explodeFraction = sceneNode->explodeFraction;
    isExploding = (explodeFraction != 0.0f);
    color = &sceneNode->color;
    alpha = sceneNode->color[3];

    if (!BZDBCache::blend && sceneNode->transparent)
        myStipple(alpha);

    if (sceneNode->clip && !isShadow)
    {
        glClipPlane(GL_CLIP_PLANE1, sceneNode->clipPlane);
        glEnable(GL_CLIP_PLANE1);
    }

    const auto &sphere = getPosition();

    // save the MODELVIEW matrix
    glPushMatrix();

    glTranslate(sphere);
    glRotatef(sceneNode->azimuth, 0.0f, 0.0f, 1.0f);
    glRotatef(sceneNode->elevation, 0.0f, 1.0f, 0.0f);
    if (sceneNode->useDimensions)
    {
        const auto &dims = sceneNode->dimensions;
        glScalef(dims[0], dims[1], dims[2]);
        glEnable(GL_NORMALIZE);
    }

    // disable the dynamic lights, if it might help
    const bool switchLights = BZDBCache::lighting &&
                              !isShadow && (drawLOD == HighTankLOD);
    if (switchLights)
        RENDERER.disableLights(sceneNode->extents.mins, sceneNode->extents.maxs);

    if (isRadar && !isExploding)
    {
        renderPart(LeftCasing);
        renderPart(RightCasing);
        renderPart(LeftTread);
        renderPart(RightTread);
        renderPart(Body);
        renderPart(Turret);
        renderPart(Barrel);
    }
    else if (!isShadow && (sceneNode->sort || sceneNode->transparent))
    {
        // draw is some sorted order
        if (sceneNode->explodeFraction == 0.0f)
        {
            // normal state
            renderParts();
        }
        else
        {
            // exploding -- draw back facing stuff first then draw front facing stuff
            glCullFace(GL_FRONT);
            renderParts();
            glCullFace(GL_BACK);
            renderParts();
        }
    }
    else if (narrowWithDepth)
        renderNarrowWithDepth();
    else if (isShadow && (sphere[2] < 0.0f))
    {
        // burrowed or burrowing tank, just render the top shadows
        renderPart(Turret);
        renderPart(Barrel);
    }
    else
    {
        // any old order is fine.  if exploding then draw both sides.
        if (isExploding)
            glDisable(GL_CULL_FACE);
        renderPart(Body);
        renderPart(Turret);
        renderPart(Barrel);
        renderPart(LeftCasing);
        renderPart(RightCasing);
        if (BZDBCache::animatedTreads)
        {
            for (int i = 0; i < 4; i++)
            {
                if (isShadow && ((i == 1) || (i == 2)) && !isExploding)
                    continue;
                renderPart((TankPart)(LeftWheel0 + i));
                renderPart((TankPart)(RightWheel0 + i));
            }
            renderPart(LeftTread);
            renderPart(RightTread);
        }
        if (isExploding)
            glEnable(GL_CULL_FACE);
    }

    // re-enable the dynamic lights
    if (switchLights)
        RENDERER.reenableLights();

    if (sceneNode->useDimensions)
        glDisable(GL_NORMALIZE);

    // restore the MODELVIEW matrix
    glPopMatrix();

    // render the jumpJets
    if (!isExploding && !isShadow)
    {
        renderJumpJets(); // after the matrix has been restored
    }

    // FIXME -- add flare lights using addFlareLight().
    //      pass light position in world space.

    glShadeModel(GL_FLAT);
    if (!BZDBCache::blend && sceneNode->transparent)
        myStipple(0.5f);
    if (sceneNode->clip)
        glDisable(GL_CLIP_PLANE1);

    return;
}


void TankSceneNode::TankRenderNode::renderLeftParts()
{
    renderPart(LeftCasing);
    if (BZDBCache::animatedTreads)
    {
        for (int i = 0; i < 4; i++)
        {
            // don't need the middle two wheels for shadows
            if (isShadow && ((i == 1) || (i == 2)) && !isExploding)
                continue;
            renderPart((TankPart)(LeftWheel0 + i));
        }
        renderPart(LeftTread);
    }
    return;
}


void TankSceneNode::TankRenderNode::renderRightParts()
{
    renderPart(RightCasing);
    if (BZDBCache::animatedTreads)
    {
        for (int i = 0; i < 4; i++)
        {
            // don't need the middle two wheels for shadows
            if (isShadow && ((i == 1) || (i == 2)) && !isExploding)
                continue;
            renderPart((TankPart)(RightWheel0 + i));
        }
        renderPart(RightTread);
    }
    return;
}


void TankSceneNode::TankRenderNode::renderNarrowWithDepth()
{
    glDepthFunc(GL_LEQUAL);
    // render the middle stuff
    renderPart(Body);
    renderPart(Turret);
    renderPart(Barrel);

    // use a fill depth buffer offset to avoid flickering
    GLboolean usingPolyOffset;
    GLfloat factor, units;
    glGetBooleanv(GL_POLYGON_OFFSET_FILL, &usingPolyOffset);
    if (usingPolyOffset == GL_TRUE)
    {
        glGetFloatv(GL_POLYGON_OFFSET_FACTOR, &factor);
        glGetFloatv(GL_POLYGON_OFFSET_UNITS, &units);
    }
    else
        glEnable(GL_POLYGON_OFFSET_FILL);

    const float offsetFactor = -0.1f;
    const float offsetDepth = -1.0f;

    glPolygonOffset(1.0f * offsetFactor, 1.0f * offsetDepth);
    if (left)
        renderPart(LeftCasing);
    else
        renderPart(RightCasing);

    glPolygonOffset(2.0f * offsetFactor, 2.0f * offsetDepth);
    for (int i = 0; i < 4; i++)
    {
        if (isShadow && ((i == 1) || (i == 2)) && !isExploding)
            continue;
        if (left)
            renderPart((TankPart)(LeftWheel0 + i));
        else
            renderPart((TankPart)(RightWheel0 + i));
    }

    glPolygonOffset(3.0f * offsetFactor, 3.0f * offsetDepth);
    if (left)
        renderPart(LeftTread);
    else
        renderPart(RightTread);

    if (usingPolyOffset)
        glPolygonOffset(factor, units);
    else
        glDisable(GL_POLYGON_OFFSET_FILL);
    glDepthFunc(GL_LESS);

    return;
}


void TankSceneNode::TankRenderNode::renderTopParts()
{
    if (towards)
    {
        renderPart(Turret);
        renderPart(Barrel);
    }
    else
    {
        renderPart(Barrel);
        renderPart(Turret);
    }
    return;
}


void TankSceneNode::TankRenderNode::renderParts()
{
    if (!above)
        renderTopParts();

    if (left)
        renderRightParts();
    else
        renderLeftParts();

    if (!sceneNode->inTheCockpit)
        renderPart(Body);

    if (left)
        renderLeftParts();
    else
        renderRightParts();

    if (sceneNode->inTheCockpit)
        renderPart(Body);

    if (above)
        renderTopParts();

    return;
}


void TankSceneNode::TankRenderNode::renderPart(TankPart part)
{
    if (isTreads)
    {
        if ( part != RightTread && part != LeftTread )
            return;
    }
    else
    {
        if ( part == RightTread || part == LeftTread )
            return;
    }

    // apply explosion transform
    TankDeathOverride::DeathParams params(explodeFraction, *color);

    if (isExploding)
    {
        glPushMatrix();
        const auto &cog = centerOfGravity[part];
        if (sceneNode->deathOverride)
            sceneNode->deathOverride->SetDeathRenderParams(params);
        const auto &velocity = sceneNode->vel[part];
        const auto &rotation = sceneNode->spin[part];
        glTranslate(cog + explodeFraction * velocity);
        glRotatef(rotation[3] * explodeFraction, rotation[0], rotation[1], rotation[2]);
        glTranslate(-cog);
    }

    // setup the animation texture matrix
    bool usingTexMat = false;
    if (!isShadow && BZDBCache::animatedTreads && (part >= BasicTankParts))
        usingTexMat = setupTextureMatrix(part);

    // set color
    if (!isShadow)
        setupPartColor(part);

    // get the list
    GLuint list;
    TankShadow shadow = isShadow ? ShadowOn : ShadowOff;
    list = TankGeometryMgr::getPartList(shadow, part, drawSize, drawLOD);

    // draw the part
    glCallList(list);

    // add to the triangle count
    addTriangleCount(TankGeometryMgr::getPartTriangleCount(
                         shadow, part, drawSize, drawLOD));

    // draw the lights on the turret
    if ((part == Turret) && !isExploding && !isShadow)
        renderLights();

    // restore texture transform
    if (usingTexMat)
    {
        glMatrixMode(GL_TEXTURE);
        glLoadIdentity();
        glMatrixMode(GL_MODELVIEW);
    }

    // restore modelview transform
    if (isExploding)
        glPopMatrix();

    return;
}


void TankSceneNode::TankRenderNode::setupPartColor(TankPart part)
{
    const auto white = glm::vec4(1.0f);
    auto clr = color;

    // do not use color modulation with tank textures
    if (BZDBCache::texture)
        clr = &white;

    switch (part)
    {
    case Body:
    {
        myColor4f(clr->r, clr->g, clr->b, alpha);
        break;
    }
    case Barrel:
    {
        myColor4f(0.25f, 0.25f, 0.25f, alpha);
        break;
    }
    case Turret:
    {
        myColor4f(0.9f * clr->r, 0.9f * clr->g, 0.9f * clr->b, alpha);
        break;
    }
    case LeftCasing:
    case RightCasing:
    {
        myColor4f(0.7f * clr->r, 0.7f * clr->g, 0.7f * clr->b, alpha);
        break;
    }
    case LeftTread:
    case RightTread:
    {
        myColor4f(0.3f * clr->r, 0.3f * clr->g, 0.3f * clr->b, alpha);
        break;
    }
    case LeftWheel0:
    case LeftWheel1:
    case LeftWheel2:
    case LeftWheel3:
    case RightWheel0:
    case RightWheel1:
    case RightWheel2:
    case RightWheel3:
    {
        myColor4f(0.4f * clr->r, 0.4f * clr->g, 0.4f * clr->b, alpha);
        break;
    }
    default:   // avoid warnings about unused enumerated values
    {
        break;
    }
    }
    return;
}


bool TankSceneNode::TankRenderNode::setupTextureMatrix(TankPart part)
{
    bool usingTexMat = true;

    switch (part)
    {
    case LeftTread:
    {
        glMatrixMode(GL_TEXTURE);
        glLoadIdentity();
        glTranslatef(sceneNode->leftTreadOffset, 0.0f, 0.0f);
        glMatrixMode(GL_MODELVIEW);
        break;
    }
    case RightTread:
    {
        glMatrixMode(GL_TEXTURE);
        glLoadIdentity();
        glTranslatef(sceneNode->rightTreadOffset, 0.0f, 0.0f);
        glMatrixMode(GL_MODELVIEW);
        break;
    }
    case LeftWheel0:
    case LeftWheel1:
    case LeftWheel2:
    case LeftWheel3:
    {
        glMatrixMode(GL_TEXTURE);
        glLoadIdentity();
        glTranslatef(+0.5f, +0.5f, 0.0f);
        glRotatef(sceneNode->leftWheelOffset, 0.0f, 0.0f, 1.0f);
        glTranslatef(-0.5f, -0.5f, 0.0f);
        glMatrixMode(GL_MODELVIEW);
        break;
    }
    case RightWheel0:
    case RightWheel1:
    case RightWheel2:
    case RightWheel3:
    {
        glMatrixMode(GL_TEXTURE);
        glLoadIdentity();
        glTranslatef(+0.5f, +0.5f, 0.0f);
        glRotatef(sceneNode->rightWheelOffset, 0.0f, 0.0f, 1.0f);
        glTranslatef(-0.5f, -0.5f, 0.0f);
        glMatrixMode(GL_MODELVIEW);
        break;
    }
    default:
    {
        usingTexMat = false;
        break;
    }
    }

    return usingTexMat;
}


void TankSceneNode::TankRenderNode::renderLights()
{
    static const glm::vec3 lights[3] =
    {
        { 1.0f, 1.0f, 1.0f },
        { 1.0f, 0.0f, 0.0f },
        { 0.0f, 1.0f, 0.0f }
    };
    static const glm::vec3 lightsPos[3] =
    {
        { -1.53f,  0.00f, 2.1f },
        {  0.10f,  0.75f, 2.1f },
        {  0.10f, -0.75f, 2.1f }
    };
    sceneNode->lightsGState.setState();
    glPointSize(2.0f);

    glBegin(GL_POINTS);
    {
        const auto &scale
            = TankGeometryMgr::getScaleFactor(sceneNode->tankSize);

        for (int i = 0; i < 3; i++)
        {
            const auto pos = lightsPos[i] * scale;
            myColor3fv(lights[i]);
            glVertex(pos);
        }
    }
    glEnd();

    glPointSize(1.0f);
    sceneNode->gstate.setState();

    addTriangleCount(4);

    return;
}


glm::vec3 TankSceneNode::jumpJetsModel[4] =
{
    {-1.5f, -0.6f, +0.25f},
    {-1.5f, +0.6f, +0.25f},
    {+1.5f, -0.6f, +0.25f},
    {+1.5f, +0.6f, +0.25f}
};

void TankSceneNode::TankRenderNode::renderJumpJets()
{
    if (!sceneNode->jumpJetsOn)
        return;

    typedef struct
    {
        glm::vec3 vertex;
        glm::vec2 texcoord;
    } jetVertex;
    static const jetVertex jet[3] =
    {
        {{+0.3f,  0.0f, 0.0f}, {0.0f, 1.0f}},
        {{-0.3f,  0.0f, 0.0f}, {1.0f, 1.0f}},
        {{ 0.0f, -1.0f, 0.0f}, {0.5f, 0.0f}}
    };

    myColor4f(1.0f, 1.0f, 1.0f, 0.5f);

    // use a clip plane, because the ground has no depth
    glEnable(GL_CLIP_PLANE0);

    sceneNode->jumpJetsGState.setState();
    glDepthMask(GL_FALSE);
    for (int j = 0; j < 4; j++)
    {
        glPushMatrix();
        {
            const auto &pos = sceneNode->jumpJetsPositions[j];
            glTranslate(pos);
            glScalef(1.0f, 1.0f, sceneNode->jumpJetsLengths[j]);

            RENDERER.getViewFrustum().executeBillboard();

            glBegin(GL_TRIANGLES);
            {
                for (int v = 0; v < 3; v++)
                {
                    glTexCoord2fv(jet[v].texcoord);
                    glVertex3fv(jet[v].vertex);
                }
            }
            glEnd();
        }
        glPopMatrix();
    }
    glDepthMask(GL_TRUE);
    sceneNode->gstate.setState();

    glDisable(GL_CLIP_PLANE0);

    addTriangleCount(4);

    return;
}


// Local Variables: ***
// mode: C++ ***
// tab-width: 4 ***
// c-basic-offset: 4 ***
// indent-tabs-mode: nil ***
// End: ***
// ex: shiftwidth=4 tabstop=4
