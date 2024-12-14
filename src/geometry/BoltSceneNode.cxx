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
#include "BoltSceneNode.h"

// system headers
#include <stdlib.h>
#include <math.h>

// common implementation headers
#include "StateDatabase.h"
#include "BZDBCache.h"
#include "TextureManager.h"
#include "OpenGLAPI.h"

// local implementation headers
#include "ViewFrustum.h"

// FIXME (SceneRenderer.cxx is in src/bzflag)
#include "SceneRenderer.h"

#include "TimeKeeper.h"

BoltSceneNode::BoltSceneNode(
    const glm::vec3 &pos, const glm::vec3 &vel, bool super) :
    isSuper(super),
    invisible(false),
    drawFlares(false),
    texturing(false),
    colorblind(false),
    size(1.0f),
    renderNode(this),
    azimuth(0),
    elevation(0),
    length(1.0f)
{

    OpenGLGStateBuilder builder(gstate);
    builder.setBlending();
    builder.setAlphaFunc();
    //builder.setTextureEnvMode(GL_DECAL);
    gstate = builder.getState();

    // prepare light
    light.setAttenuation(0, 0.05f);
    light.setAttenuation(1, 0.0f);
    light.setAttenuation(2, 0.03f);

    // prepare geometry
    move(pos, vel);
    setSize(size);
    setColor(1.0f, 1.0f, 1.0f);
    teamColor = glm::vec4(1.0f);
}

BoltSceneNode::~BoltSceneNode()
{
    // do nothing
}

void            BoltSceneNode::setFlares(bool on)
{
    drawFlares = on;
}

void            BoltSceneNode::setSize(float radius)
{
    size = radius;
    setRadius(size * size);
}
void            BoltSceneNode::setTextureColor(GLfloat r, GLfloat g, GLfloat b, GLfloat a)
{
    color = glm::vec4(r, g, b, a);
    light.setColor(1.5f * glm::vec3(color));
    renderNode.setTextureColor(color);
}

void            BoltSceneNode::setColor(GLfloat r, GLfloat g, GLfloat b, GLfloat a)
{
    color = glm::vec4(r, g, b, a);
    light.setColor(1.5f * glm::vec3(color));
    renderNode.setColor(color);
}

void            BoltSceneNode::setTeamColor(const glm::vec3 &c)
{
    teamColor = glm::vec4(c, 1.0f);
}

void BoltSceneNode::setColor(const glm::vec3 &rgb)
{
    setColor(rgb[0], rgb[1], rgb[2]);
}

bool            BoltSceneNode::getColorblind() const
{
    return colorblind;
}

void            BoltSceneNode::setColorblind(bool _colorblind)
{
    colorblind = _colorblind;
}

void            BoltSceneNode::setTexture(const int texture)
{
    OpenGLGStateBuilder builder(gstate);
    builder.setTexture(texture);
    builder.enableTexture(texture>=0);
    gstate = builder.getState();
}

void            BoltSceneNode::setTextureAnimation(int cu, int cv)
{
    renderNode.setAnimation(cu, cv);
}

void BoltSceneNode::move(const glm::vec3 &pos, const glm::vec3 &vel)
{
    const auto xy_vel = glm::length(glm::vec2(vel));
    setCenter(pos);
    light.setPosition(pos);
    velocity = vel;
    length = glm::length(vel);

    azimuth   = (float)(+RAD2DEG * atan2f(vel[1], vel[0]));
    elevation = (float)(-RAD2DEG * atan2f(vel[2], xy_vel));
}

void            BoltSceneNode::addLight(
    SceneRenderer& renderer)
{
    renderer.addLight(light);
}

void            BoltSceneNode::notifyStyleChange()
{
    texturing = BZDBCache::texture && BZDBCache::blend;
    OpenGLGStateBuilder builder(gstate);
    builder.enableTexture(texturing);
    if (BZDBCache::blend)
    {
        const int shotLength = (int)(BZDBCache::shotLength * 3.0f);
        if (shotLength > 0 && !drawFlares)
            builder.setBlending(GL_SRC_ALPHA, GL_ONE);
        else
            builder.setBlending(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        builder.setStipple(1.0f);
        builder.setAlphaFunc();
        if ((RENDERER.useQuality() >= 3) && drawFlares)
        {
            builder.setShading(GL_SMOOTH);
            builder.enableMaterial(false);
        }
        else
            builder.setShading(texturing ? GL_FLAT : GL_SMOOTH);
    }
    else
    {
        builder.resetBlending();
        builder.resetAlphaFunc();
        builder.setStipple(0.5f);
        builder.setShading(GL_FLAT);
    }
    gstate = builder.getState();
}

void            BoltSceneNode::addRenderNodes(
    SceneRenderer& renderer)
{
    renderer.addRenderNode(&renderNode, &gstate);
}

//
// BoltSceneNode::BoltRenderNode
//

const GLfloat       BoltSceneNode::BoltRenderNode::CoreFraction = 0.4f;
const GLfloat       BoltSceneNode::BoltRenderNode::FlareSize = 1.0f;
const GLfloat       BoltSceneNode::BoltRenderNode::FlareSpread = 0.08f;
glm::vec2           BoltSceneNode::BoltRenderNode::core[9];
const glm::vec2     BoltSceneNode::BoltRenderNode::corona[8] =
{
    { 1.0f, 0.0f },
    { (float)M_SQRT1_2, (float)M_SQRT1_2 },
    { 0.0f, 1.0f },
    { (float)-M_SQRT1_2, (float)M_SQRT1_2 },
    { -1.0f, 0.0f },
    { (float)-M_SQRT1_2, (float)-M_SQRT1_2 },
    { 0.0f, -1.0f },
    { (float)M_SQRT1_2, (float)-M_SQRT1_2 }
};

BoltSceneNode::BoltRenderNode::BoltRenderNode(
    const BoltSceneNode* _sceneNode) :
    sceneNode(_sceneNode),
    numFlares(0)
{
    // initialize core and corona if not already done
    static bool init = false;
    if (!init)
    {
        init = true;
        core[0] = glm::vec2(0.0f);
        for (int i = 0; i < 8; i++)
            core[i+1] = CoreFraction * corona[i];
    }

    textureColor = glm::vec4(1.0f);

    setAnimation(1, 1);
}

BoltSceneNode::BoltRenderNode::~BoltRenderNode()
{
    // do nothing
}

const glm::vec3 &BoltSceneNode::BoltRenderNode::getPosition() const
{
    return sceneNode->getSphere();
}

void            BoltSceneNode::BoltRenderNode::setAnimation(
    int _cu, int _cv)
{
    cu = _cu;
    cv = _cv;
    du = 1.0f / (float)cu;
    dv = 1.0f / (float)cv;

    // pick a random start frame
    const int index = (int)((float)cu * (float)cv * bzfrand());
    u = index % cu;
    v = index / cu;
    if (v >= cv) v = 0;
}
void BoltSceneNode::BoltRenderNode::setTextureColor(const glm::vec4 &rgba)
{
    textureColor = rgba;
}


void BoltSceneNode::BoltRenderNode::setColor(const glm::vec4 &rgba)
{
    mainColor   = rgba;
    innerColor  = rgba + 0.5f * (1.0f - rgba);
    outerColor  = rgba;
    coronaColor = rgba;
    flareColor  = rgba;

    innerColor[3] = rgba[3];
    if (rgba.a == 1.0f)
    {
        outerColor[3]  = 0.1f;
        coronaColor[3] = 0.5f;
        flareColor[3]  = 0.667f;
    }
}

void drawFin ( float maxRad, float finRadius, float boosterLen, float finForeDelta, float finCapSize)
{
    glBegin(GL_TRIANGLE_STRIP);
    glNormal3f(1,0,0);
    glVertex3f(0,maxRad,0);
    glVertex3f(0,maxRad,boosterLen);
    glVertex3f(0,maxRad+finRadius,boosterLen-finForeDelta-finCapSize);
    glVertex3f(0,maxRad+finRadius,boosterLen-finForeDelta);
    glEnd();

    glBegin(GL_TRIANGLE_STRIP);
    glNormal3f(-1,0,0);
    glVertex3f(0,maxRad+finRadius,boosterLen-finForeDelta-finCapSize);
    glVertex3f(0,maxRad+finRadius,boosterLen-finForeDelta);
    glVertex3f(0,maxRad,0);
    glVertex3f(0,maxRad,boosterLen);
    glEnd();
}

void BoltSceneNode::BoltRenderNode::renderGeoGMBolt()
{
    // bzdb these 2? they control the shot size
    float gmMissleSize = BZDBCache::gmSize;

    // parametrics
    float maxRad = gmMissleSize * 0.16f;
    float noseRad = gmMissleSize * 0.086f;
    float waistRad = gmMissleSize * 0.125f;
    float engineRad = gmMissleSize * 0.1f;

    float noseLen = gmMissleSize * 0.1f;
    float bodyLen = gmMissleSize * 0.44f;
    float bevelLen = gmMissleSize * 0.02f;
    float waistLen = gmMissleSize * 0.16f;
    float boosterLen = gmMissleSize * 0.2f;
    float engineLen = gmMissleSize * 0.08f;

    float finRadius = gmMissleSize * 0.16f;
    float finCapSize = gmMissleSize * 0.15f;
    float finForeDelta = gmMissleSize * 0.02f;

    int slices = 8;

    float rotSpeed = 90.0f;

    glDepthMask(GL_TRUE);
    glPushMatrix();
    glRotatef(sceneNode->azimuth, 0.0f, 0.0f, 1.0f);
    glRotatef(sceneNode->elevation, 0.0f, 1.0f, 0.0f);
    glRotatef(90, 0.0f, 1.0f, 0.0f);

    glDisable(GL_TEXTURE_2D);
    //glEnable(GL_LIGHTING);

    auto noseColor = sceneNode->teamColor;
    auto finColor  = glm::vec4(glm::vec3(noseColor) * 0.5f, 1.0f);
    auto coneColor = glm::vec4(0.125f, 0.125f, 0.125f, 1.0f);
    auto bodyColor = glm::vec4(1.0f);

    glPushMatrix();

    GLUquadric *q = gluNewQuadric();

    glColor4f(noseColor.r,noseColor.g,noseColor.b,1.0f);
    glTranslatef(0, 0, gmMissleSize);
    glRotatef((float)TimeKeeper::getCurrent().getSeconds() * rotSpeed,0,0,1);

    // nosecone
    gluDisk(q,0,noseRad,slices,1);
    glTranslatef(0, 0, -noseLen);
    gluCylinder(q,maxRad,noseRad,noseLen,slices,1);
    addTriangleCount(slices * 2);

    // body
    myColor4f(bodyColor.r, bodyColor.g, bodyColor.b, bodyColor.a);
    glTranslatef(0, 0, -bodyLen);
    gluCylinder(q,maxRad,maxRad,bodyLen,slices,1);
    addTriangleCount(slices);

    glTranslatef(0, 0, -bevelLen);
    gluCylinder(q,waistRad,maxRad,bevelLen,slices,1);
    addTriangleCount(slices);

    // waist
    myColor4f(coneColor.r, coneColor.g, coneColor.b, coneColor.a);
    glTranslatef(0, 0, -waistLen);
    gluCylinder(q,waistRad,waistRad,waistLen,slices,1);
    addTriangleCount(slices);

    // booster
    myColor4f(bodyColor.r, bodyColor.g, bodyColor.b, 1.0f);
    glTranslatef(0, 0, -bevelLen);
    gluCylinder(q,maxRad,waistRad,bevelLen,slices,1);
    addTriangleCount(slices);

    glTranslatef(0, 0, -boosterLen);
    gluCylinder(q,maxRad,maxRad,boosterLen,slices,1);
    addTriangleCount(slices);

    glTranslatef(0, 0, -bevelLen);
    gluCylinder(q,waistRad,maxRad,bevelLen,slices,1);
    addTriangleCount(slices);

    // engine
    myColor4f(coneColor.r, coneColor.g, coneColor.b, 1.0f);
    glTranslatef(0, 0, -engineLen);
    gluCylinder(q,engineRad,waistRad,engineLen,slices,1);
    addTriangleCount(slices);

    // fins
    myColor4f(finColor.r, finColor.g, finColor.b, 1.0f);
    glTranslatef(0, 0, engineLen + bevelLen);

    for ( int i = 0; i < 4; i++)
    {
        glRotatef(i*90.0f,0,0,1);
        drawFin ( maxRad, finRadius, boosterLen, finForeDelta, finCapSize);
    }

    glPopMatrix();

    gluDeleteQuadric(q);

    glEnable(GL_TEXTURE_2D);
    // glDisable(GL_LIGHTING);

    glPopMatrix();

    glDepthMask(GL_FALSE);
}


void BoltSceneNode::BoltRenderNode::renderGeoBolt()
{
    // bzdb these 2? they control the shot size
    float lenMod = 0.0675f + (BZDBCache::shotLength * 0.0125f);
    float baseRadius = 0.225f;

    float len = sceneNode->length * lenMod;
    glPushMatrix();
    glRotatef(sceneNode->azimuth, 0.0f, 0.0f, 1.0f);
    glRotatef(sceneNode->elevation, 0.0f, 1.0f, 0.0f);
    glRotatef(90, 0.0f, 1.0f, 0.0f);

    float alphaMod = 1.0f;
// if (sceneNode->phasingShot)
//   alphaMod = 0.85f;

    glDisable(GL_TEXTURE_2D);

    float coreBleed = 4.5f;
    float minimumChannelVal = 0.45f;

    const auto c = glm::vec3(sceneNode->color);

    auto coreColor = glm::max(c * coreBleed, minimumChannelVal);

    myColor4f(coreColor.r, coreColor.g, coreColor.b, 0.85f * alphaMod);
    renderGeoPill(baseRadius,len,16);

    float radInc = 1.5f * baseRadius - baseRadius;
    glPushMatrix();
    glTranslatef(0, 0, -radInc * 0.5f);

    myColor4f(c.r, c.g, c.b, 0.5f);
    renderGeoPill(1.5f * baseRadius, len + radInc, 25);
    glPopMatrix();

    radInc = 2.7f * baseRadius - baseRadius;
    glPushMatrix();
    glTranslatef(0, 0, -radInc*0.5f);
    myColor4f(c.r, c.g, c.b, 0.25f);
    renderGeoPill(2.7f * baseRadius, len + radInc, 32);
    glPopMatrix();

    radInc = 3.8f * baseRadius - baseRadius;
    glPushMatrix();
    glTranslatef(0, 0,-radInc*0.5f);
    myColor4f(c.r, c.g, c.b, 0.125f);
    renderGeoPill(3.8f * baseRadius, len + radInc, 48);
    glPopMatrix();

    glEnable(GL_TEXTURE_2D);

    glPopMatrix();
}


void BoltSceneNode::BoltRenderNode::renderGeoPill(float radius, float len,
        int segments, float endRad)
{
    glPushMatrix();

    float assRadius = radius;
    if (endRad >= 0)
        assRadius = endRad;

    float lenMinusRads = len - (radius+assRadius);

    GLUquadric *q = gluNewQuadric();
    if (assRadius > 0)
    {
        // 4 parts of the first hemisphere
        gluCylinder(q,0,assRadius*0.43589,assRadius*0.1f,segments,1);
        addTriangleCount(segments);
        glTranslatef(0,0,assRadius*0.1f);

        gluCylinder(q,assRadius*0.43589,assRadius*0.66144,assRadius*0.15f,segments,1);
        addTriangleCount(segments);
        glTranslatef(0,0,assRadius*0.15f);

        gluCylinder(q,assRadius*0.66144f,assRadius*0.86603f,assRadius*0.25f,segments,1);
        addTriangleCount(segments);
        glTranslatef(0,0,assRadius*0.25f);

        gluCylinder(q,assRadius*0.86603,assRadius,assRadius*0.5f,segments,1);
        addTriangleCount(segments);
        glTranslatef(0,0,assRadius*0.5f);
    }

    // the "shaft"
    if (lenMinusRads > 0)
    {
        gluCylinder(q,assRadius,radius,lenMinusRads,segments,1);
        addTriangleCount(segments);
        glTranslatef(0,0,lenMinusRads);
    }

    if (radius > 0)
    {
        // 4 parts of the last hemisphere
        gluCylinder(q,radius,radius*0.86603,radius*0.5f,segments,1);
        addTriangleCount(segments);
        glTranslatef(0,0,radius*0.5f);

        gluCylinder(q,radius*0.86603f,radius*0.66144f,radius*0.25f,segments,1);
        addTriangleCount(segments);
        glTranslatef(0,0,radius*0.25f);

        gluCylinder(q,radius*0.66144,radius*0.43589,radius*0.15f,segments,1);
        addTriangleCount(segments);
        glTranslatef(0,0,radius*0.15f);

        gluCylinder(q,radius*0.43589,0,radius*0.1f,segments,1);
        addTriangleCount(segments);
        glTranslatef(0,0,radius*0.1f);
    }

    gluDeleteQuadric(q);
    glPopMatrix();
}

void            BoltSceneNode::BoltRenderNode::render()
{
    if (sceneNode->invisible)
        return;
    const float radius = sceneNode->size;
    const int   shotLength = (int)(BZDBCache::shotLength * 3.0f);
    const bool  experimental = (RENDERER.useQuality() >= 3);

    const bool blackFog = RENDERER.isFogActive() && BZDBCache::blend &&
                          ((shotLength > 0) || experimental);
    if (blackFog)
        glSetFogColor(glm::vec4(0.0f));

    const auto &sphere = getPosition();
    glPushMatrix();
    glTranslate(sphere);

    bool drawBillboardShot = false;
    if (experimental)
    {
        if (sceneNode->isSuper)
            renderGeoBolt();
        else
        {
            if (sceneNode->drawFlares)
            {
                if (BZDBCache::shotLength > 0)
                    renderGeoGMBolt();
                drawBillboardShot = true;
            }
            else
                drawBillboardShot = true;
        }
    }
    else
        drawBillboardShot = true;

    if (drawBillboardShot)
    {
        RENDERER.getViewFrustum().executeBillboard();
        glScalef(radius, radius, radius);
        // draw some flares
        if (sceneNode->drawFlares)
        {
            if (!RENDERER.isSameFrame())
            {
                numFlares = 3 + int(3.0f * (float)bzfrand());
                for (int i = 0; i < numFlares; i++)
                {
                    theta[i] = (float)(2.0 * M_PI * bzfrand());
                    phi[i] = (float)bzfrand() - 0.5f;
                    phi[i] *= (float)(2.0 * M_PI * fabsf(phi[i]));
                }
            }

            if (sceneNode->texturing) glDisable(GL_TEXTURE_2D);
            myColor4fv(flareColor);
            if (!BZDBCache::blend) myStipple(flareColor[3]);
            for (int i = 0; i < numFlares; i++)
            {
                // pick random direction in 3-space.  picking a random theta with
                // a uniform distribution is fine, but doing so with phi biases
                // the directions toward the poles.  my correction doesn't remove
                // the bias completely, but moves it towards the equator, which is
                // really where i want it anyway cos the flares are more noticeable
                // there.
                const float c = FlareSize * cosf(phi[i]);
                const float s = FlareSize * sinf(phi[i]);
                const float ti = theta[i];
                const float fs = FlareSpread;
                glBegin(GL_TRIANGLE_STRIP);
                glVertex3f(0.0f,                0.0f,                0.0f);
                glVertex3f(c * cosf(ti - fs),   c * sinf(ti - fs),   s);
                glVertex3f(c * cosf(ti + fs),   c * sinf(ti + fs),   s);
                glVertex3f(c * cosf(ti) * 2.0f, c * sinf(ti) * 2.0f, s * 2.0f);
                glEnd();
            }
            if (sceneNode->texturing) glEnable(GL_TEXTURE_2D);

            addTriangleCount(numFlares * 2);
        }

        if (sceneNode->texturing)
        {
            // draw billboard square
            const float u0 = (float)u * du;
            const float v0 = (float)v * dv;
            const float u1 = u0 + du;
            const float v1 = v0 + dv;
            myColor4fv(textureColor); // 1.0f all
            glBegin(GL_TRIANGLE_STRIP);
            glTexCoord2f(u0, v0);
            glVertex2f(-1.0f, -1.0f);
            glTexCoord2f(u1, v0);
            glVertex2f(+1.0f, -1.0f);
            glTexCoord2f(u0, v1);
            glVertex2f(-1.0f, +1.0f);
            glTexCoord2f(u1, v1);
            glVertex2f(+1.0f, +1.0f);
            glEnd();
            addTriangleCount(2);

            // draw shot trail  (more billboarded quads)
            if ((shotLength > 0) && (sceneNode->length > 1.0e-6f))
            {
                const float startSize  = 0.6f;
                const float startAlpha = 0.8f;

                glPushAttrib(GL_TEXTURE_BIT);
                TextureManager &tm = TextureManager::instance();
                const int texID = tm.getTextureID("shot_tail");
                const ImageInfo& texInfo = tm.getInfo(texID);
                if (texInfo.id >= 0)
                    texInfo.texture->execute();

                auto dir = sceneNode->velocity / sceneNode->length;

                const float invLenPlusOne = 1.0f / (float)(shotLength + 1);
                const float shiftScale = 90.0f / (150.0f + (float)shotLength);
                float Size = sceneNode->size * startSize;
                float alpha = startAlpha;
                const float sizeStep  = Size  * invLenPlusOne;
                const float alphaStep = alpha * invLenPlusOne;

                auto pos = sphere;

                int uvCell = rand() % 16;

                for (int i = 0; i < shotLength; i++)
                {
                    Size  -= sizeStep;
                    const float s = Size * (0.65f + (1.0f * (float)bzfrand()));
                    const float shift = s * shiftScale;

                    pos -= (shift * dir);
                    if (pos.z < 0.0f)
                        continue;

                    uvCell = (uvCell + 1) % 16;
                    const float U0 = (uvCell % 4 ) * 0.25f;
                    const float V0 = (uvCell / 4 ) * 0.25f;
                    const float U1 = U0 + 0.25f;
                    const float V1 = V0 + 0.25f;

                    alpha -= alphaStep;
                    glColor4f(mainColor[0],mainColor[1],mainColor[2], alpha);
                    glPopMatrix();
                    glPushMatrix();

                    glTranslatef(pos.x, pos.y, pos.z);
                    RENDERER.getViewFrustum().executeBillboard();
                    glScalef(s, s, s);

                    glBegin(GL_TRIANGLE_STRIP);
                    glTexCoord2f(U0, V0);
                    glVertex2f(-1.0f, -1.0f);
                    glTexCoord2f(U1, V0);
                    glVertex2f(+1.0f, -1.0f);
                    glTexCoord2f(U0, V1);
                    glVertex2f(-1.0f, +1.0f);
                    glTexCoord2f(U1, V1);
                    glVertex2f(+1.0f, +1.0f);
                    glEnd();
                }

                addTriangleCount(shotLength * 2);
                glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
                glPopAttrib(); // revert the texture
            }
        }
        else if (BZDBCache::blend)
        {
            // draw corona
            glBegin(GL_TRIANGLE_STRIP);
            myColor4fv(mainColor);
            glVertex2fv(core[1]);
            myColor4fv(outerColor);
            glVertex2fv(corona[0]);
            myColor4fv(mainColor);
            glVertex2fv(core[2]);
            myColor4fv(outerColor);
            glVertex2fv(corona[1]);
            myColor4fv(mainColor);
            glVertex2fv(core[3]);
            myColor4fv(outerColor);
            glVertex2fv(corona[2]);
            myColor4fv(mainColor);
            glVertex2fv(core[4]);
            myColor4fv(outerColor);
            glVertex2fv(corona[3]);
            myColor4fv(mainColor);
            glVertex2fv(core[5]);
            myColor4fv(outerColor);
            glVertex2fv(corona[4]);
            myColor4fv(mainColor);
            glVertex2fv(core[6]);
            myColor4fv(outerColor);
            glVertex2fv(corona[5]);
            myColor4fv(mainColor);
            glVertex2fv(core[7]);
            myColor4fv(outerColor);
            glVertex2fv(corona[6]);
            myColor4fv(mainColor);
            glVertex2fv(core[8]);
            myColor4fv(outerColor);
            glVertex2fv(corona[7]);
            myColor4fv(mainColor);
            glVertex2fv(core[1]);
            myColor4fv(outerColor);
            glVertex2fv(corona[0]);
            glEnd(); // 18 verts -> 16 tris

            // draw core
            glBegin(GL_TRIANGLE_FAN);
            myColor4fv(innerColor);
            glVertex2fv(core[0]);
            myColor4fv(mainColor);
            glVertex2fv(core[1]);
            glVertex2fv(core[2]);
            glVertex2fv(core[3]);
            glVertex2fv(core[4]);
            glVertex2fv(core[5]);
            glVertex2fv(core[6]);
            glVertex2fv(core[7]);
            glVertex2fv(core[8]);
            glVertex2fv(core[1]);
            glEnd(); // 10 verts -> 8 tris

            addTriangleCount(24);
        }
        else
        {
            // draw corona
            myColor4fv(coronaColor);
            myStipple(coronaColor[3]);
            glBegin(GL_TRIANGLE_STRIP);
            glVertex2fv(core[1]);
            glVertex2fv(corona[0]);
            glVertex2fv(core[2]);
            glVertex2fv(corona[1]);
            glVertex2fv(core[3]);
            glVertex2fv(corona[2]);
            glVertex2fv(core[4]);
            glVertex2fv(corona[3]);
            glVertex2fv(core[5]);
            glVertex2fv(corona[4]);
            glVertex2fv(core[6]);
            glVertex2fv(corona[5]);
            glVertex2fv(core[7]);
            glVertex2fv(corona[6]);
            glVertex2fv(core[8]);
            glVertex2fv(corona[7]);
            glVertex2fv(core[1]);
            glVertex2fv(corona[0]);
            glEnd(); // 18 verts -> 16 tris

            // draw core
            myStipple(1.0f);
            glBegin(GL_TRIANGLE_FAN);
            myColor4fv(innerColor);
            glVertex2fv(core[0]);
            myColor4fv(mainColor);
            glVertex2fv(core[1]);
            glVertex2fv(core[2]);
            glVertex2fv(core[3]);
            glVertex2fv(core[4]);
            glVertex2fv(core[5]);
            glVertex2fv(core[6]);
            glVertex2fv(core[7]);
            glVertex2fv(core[8]);
            glVertex2fv(core[1]);
            glEnd(); // 10 verts -> 8 tris

            myStipple(0.5f);

            addTriangleCount(24);
        }

    }

    glPopMatrix();

    if (blackFog)
        glSetFogColor(RENDERER.getFogColor());

    if (RENDERER.isLastFrame())
    {
        if (++u == cu)
        {
            u = 0;
            if (++v == cv)
                v = 0;
        }
    }
}

// Local Variables: ***
// mode: C++ ***
// tab-width: 4 ***
// c-basic-offset: 4 ***
// indent-tabs-mode: nil ***
// End: ***
// ex: shiftwidth=4 tabstop=4
