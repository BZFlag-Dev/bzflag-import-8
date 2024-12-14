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
#include "BillboardSceneNode.h"

// system headers
#include <stdlib.h>
#include <math.h>
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/norm.hpp>

// common implementation headers
#include "BZDBCache.h"
#include "TextureManager.h"
#include "mathRoutine.h"
#include "OpenGLAPI.h"

// local implementation headers
#include "ViewFrustum.h"
#include "StateDatabase.h"

// FIXME (SceneRenderer.cxx is in src/bzflag)
#include "SceneRenderer.h"

BillboardSceneNode::BillboardSceneNode(const glm::vec3 &pos) :
    show(false),
    hasAlpha(false),
    hasTexture(false),
    hasTextureAlpha(false),
    looping(false),
    lightSource(false),
    width(1.0f), height(1.0f),
    cu(1), cv(1),
    t(0.0f), duration(0.0f),
    renderNode(this)
{
    OpenGLGStateBuilder builder(gstate);
    builder.setBlending();
    builder.setAlphaFunc();
    //builder.setTextureEnvMode(GL_DECAL);
    gstate = builder.getState();

    // prepare light
    lightScale = 1.0f;
    setLightColor(1.0f, 1.0f, 1.0f);
    setLightAttenuation(0.05f, 0.0f, 0.03f);
    setLightFadeStartTime(duration);
    setGroundLight(false);

    // prepare geometry
    move(pos);
    setAngle(0.0f);
    setSize(width, height);
    setColor(1.0f, 1.0f, 1.0f, 1.0f);

    // prepare animation
    resetTime();
}

BillboardSceneNode::~BillboardSceneNode()
{
    // do nothing
}

BillboardSceneNode* BillboardSceneNode::copy() const
{
    BillboardSceneNode* e = new BillboardSceneNode(getSphere());
    e->show = show;
    e->hasAlpha = hasAlpha;
    e->hasTexture = hasTexture;
    e->hasTextureAlpha = hasTextureAlpha;
    e->looping = looping;
    e->lightSource = lightSource;
    e->lightColor = lightColor;
    e->lightScale = lightScale;
    e->lightCutoffTime = lightCutoffTime;
    e->width = width;
    e->height = height;
    e->color = color;
    e->angle = angle;
    e->duration = duration;
    e->light = light;
    e->gstate = gstate;

    // begin at the beginning
    e->t = 0.0f;
    e->setTextureAnimation(cu, cv);
    e->prepLight();

    return e;
}

void            BillboardSceneNode::setLoop(bool _looping)
{
    looping = _looping;
}

void            BillboardSceneNode::setDuration(float _duration)
{
    duration = _duration;
    if (t > duration) t = duration;
    setFrame();
}

void            BillboardSceneNode::resetTime()
{
    t = 0.0f;
    setFrame();
}

void            BillboardSceneNode::updateTime(float dt)
{
    // change time by dt then make sure it's in bounds
    const float ot = t;
    t += dt;
    if (duration == 0.0f) t = 0.0f;
    else if (!looping)
    {
        if (t < 0.0f) t = 0.0f;
        else if (t > duration) t = duration;
    }
    else
    {
        if (t < 0.0f) t = duration - fmodf(-t, duration);
        else if (t >= duration) t = fmodf(t, duration);
    }
    setFrame();

    // update light intensity if it changed
    if (t > lightCutoffTime || ot > lightCutoffTime)
        prepLight();
}

bool            BillboardSceneNode::isAtEnd() const
{
    return t == duration;
}

void            BillboardSceneNode::setFrame()
{
    // update frame
    if (duration == 0.0f)
        renderNode.setFrame(0.0f, 0.0f);
    else
    {
        int frame = (int)((t / duration) * (float)cu * (float)cv);
        if (frame >= cu * cv) frame = cu * cv - 1;
        renderNode.setFrame((float)(frame % cu) / (float)cu,
                            (float)(frame / cu) / (float)cv);
    }
}

bool            BillboardSceneNode::isLight() const
{
    return lightSource && show;
}

void            BillboardSceneNode::setLight(bool on)
{
    if (lightSource == on) return;
    lightSource = on;
    if (lightSource) prepLight();
}

void            BillboardSceneNode::setLightColor(
    GLfloat r, GLfloat g, GLfloat b)
{
    lightColor = glm::vec3(r, g, b);
    prepLight();
}

void            BillboardSceneNode::setLightAttenuation(
    GLfloat c, GLfloat l, GLfloat q)
{
    light.setAttenuation(0, c);
    light.setAttenuation(1, l);
    light.setAttenuation(2, q);
}

void            BillboardSceneNode::setLightScaling(GLfloat s)
{
    lightScale = s;
    prepLight();
}

void            BillboardSceneNode::setLightFadeStartTime(float _t)
{
    lightCutoffTime = _t;
    prepLight();
}

void            BillboardSceneNode::setGroundLight(bool value)
{
    groundLight = value;
    light.setOnlyGround(value);
}

void            BillboardSceneNode::prepLight()
{
    if (!lightSource) return;
    const float s = (t <= lightCutoffTime || lightCutoffTime >= duration) ? 1.0f :
                    (1.0f - (t - lightCutoffTime) / (duration - lightCutoffTime));
    light.setColor(lightColor * lightScale * s);
}

void            BillboardSceneNode::setSize(float side)
{
    setSize(side, side);
}

void            BillboardSceneNode::setSize(float _width, float _height)
{
    width = 0.5f * _width;
    height = 0.5f * _height;
    setRadius(width * width + height * height);
}

void            BillboardSceneNode::setColor(
    GLfloat r, GLfloat g, GLfloat b, GLfloat a)
{
    color = glm::vec4(r, g, b, a);
    hasAlpha = (color[3] != 1.0f || hasTextureAlpha);
}

void            BillboardSceneNode::setColor(const GLfloat* rgba)
{
    setColor(rgba[0], rgba[1], rgba[2], rgba[3]);
}

void            BillboardSceneNode::setTexture(
    const int texture)
{
    hasTexture = texture >= 0;
    TextureManager &tm = TextureManager::instance();

    hasTextureAlpha = hasTexture && tm.getInfo(texture).alpha;
    hasAlpha = (color[3] != 1.0f || hasTextureAlpha);
    OpenGLGStateBuilder builder(gstate);
    builder.setTexture(texture);
    builder.enableTexture(hasTexture);
    gstate = builder.getState();
}

void            BillboardSceneNode::
setTextureAnimation(int _cu, int _cv)
{
    cu = _cu;
    cv = _cv;
    renderNode.setFrameSize(1.0f / (float)cu, 1.0f / (float)cv);
    setFrame();
}

void BillboardSceneNode::move(const glm::vec3 &pos)
{
    setCenter(pos);
    light.setPosition(pos);
}

void            BillboardSceneNode::setAngle(GLfloat _angle)
{
    angle = (float)(180.0 / M_PI * _angle);
}

void            BillboardSceneNode::addLight(
    SceneRenderer& renderer)
{
    if (show && lightSource)
        renderer.addLight(light);
}

void            BillboardSceneNode::notifyStyleChange()
{
    show = hasTexture && BZDBCache::texture &&
           (!hasAlpha || BZDBCache::blend);
    if (show)
    {
        OpenGLGStateBuilder builder(gstate);
        if (hasAlpha)
        {
            builder.setBlending(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
            builder.setAlphaFunc();
        }
        else
        {
            builder.resetBlending();
            builder.resetAlphaFunc();
        }
        gstate = builder.getState();
    }
}

void            BillboardSceneNode::addRenderNodes(
    SceneRenderer& renderer)
{
    if (show)
        renderer.addRenderNode(&renderNode, &gstate);
}

//
// BillboardSceneNode::BillboardRenderNode
//

BillboardSceneNode::BillboardRenderNode::BillboardRenderNode(
    const BillboardSceneNode* _sceneNode) :
    sceneNode(_sceneNode)
{
    setFrame(0.0f, 0.0f);
    setFrameSize(1.0f, 1.0f);
}

BillboardSceneNode::BillboardRenderNode::~BillboardRenderNode()
{
    // do nothing
}

const glm::vec3 &BillboardSceneNode::BillboardRenderNode::getPosition() const
{
    return sceneNode->getSphere();
}

void            BillboardSceneNode::BillboardRenderNode::
setFrame(float _u, float _v)
{
    u = _u;
    v = _v;
}

void            BillboardSceneNode::BillboardRenderNode::
setFrameSize(float _du, float _dv)
{
    du = _du;
    dv = _dv;
}

void            BillboardSceneNode::BillboardRenderNode::render()
{
    glEnable(GL_CLIP_PLANE0);

    // want to move the billboard directly towards the eye a little bit.
    // can't translate in Z after the billboard is applied because that
    // will move in the direction of the view, which isn't necessarily
    // the direction to the billboard from the eye.
    ViewFrustum& frustum = RENDERER.getViewFrustum();
    const auto &eye    = frustum.getEye();
    const auto &center = getPosition();

    auto dir       = eye - center;
    dir           *= bzInverseSqrt(glm::length2(dir));
    const auto pos = center + sceneNode->width * dir;

    glPushMatrix();
    {
        glTranslate(pos);
        frustum.executeBillboard();
        glRotatef(sceneNode->angle, 0.0f, 0.0f, 1.0f);

        // draw billboard
        myColor4fv(sceneNode->color);
        glBegin(GL_TRIANGLE_STRIP);
        glTexCoord2f(   u,    v);
        glVertex2f  (-sceneNode->width, -sceneNode->height);
        glTexCoord2f(du+u,    v);
        glVertex2f  ( sceneNode->width, -sceneNode->height);
        glTexCoord2f(   u, dv+v);
        glVertex2f  (-sceneNode->width,  sceneNode->height);
        glTexCoord2f(du+u, dv+v);
        glVertex2f  ( sceneNode->width,  sceneNode->height);
        glEnd();
    }
    glPopMatrix();

    addTriangleCount(2);

    glDisable(GL_CLIP_PLANE0);
}

// Local Variables: ***
// mode: C++ ***
// tab-width: 4 ***
// c-basic-offset: 4 ***
// indent-tabs-mode: nil ***
// End: ***
// ex: shiftwidth=4 tabstop=4
