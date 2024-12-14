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

// Interface
#include "Frustum.h"

// System headers
#include <string.h>
#include <glm/geometric.hpp>
#include <glm/vec2.hpp>
#include <glm/gtc/matrix_access.hpp>
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/norm.hpp>


Frustum::Frustum()
{
    static auto defaultEye = glm::vec3(0.0);
    static auto defaultTarget = glm::vec3(0.0, 1.0, 0.0);

    // initialize view and projection matrices to identity
    viewMatrix = glm::mat4(1.0f);
    billboardMatrix = glm::mat4(1.0f);
    projectionMatrix = glm::mat4(1.0f);
    deepProjectionMatrix = glm::mat4(1.0f);

    setProjection((float)(M_PI/4.0), 1.0f, 100.0f, 1000.0f, 1, 1, 1);
    setView(defaultEye, defaultTarget);
}


Frustum::~Frustum()
{
    // do nothing
}


void Frustum::setView(const glm::vec3 &_eye, const glm::vec3 &_target)
{
    // set eye and target points
    eye = _eye;
    target = _target;

    // build view matrix, including a transformation bringing
    // world up [0 0 1 0]T to eye up [0 1 0 0]T, world north
    // [0 1 0 0]T to eye forward [0 0 -1 0]T.
    viewMatrix = glm::lookAt(eye, target, glm::vec3(0.0f, 0.0f, 1.0f));

    // compute left vector
    right    =  glm::vec3(glm::row(viewMatrix, 0));

    // compute local up vector
    up       =  glm::vec3(glm::row(viewMatrix, 1));

    // compute forward vector
    auto dir = -glm::vec3(glm::row(viewMatrix, 2));

    // build billboard matrix.  billboard matrix performs rotation
    // so that polygons drawn in the xy plane face the camera.
    billboardMatrix = glm::mat4(glm::transpose(glm::mat3(viewMatrix)));

    // compute vectors of frustum edges
    const auto xs = right / fabsf(projectionMatrix[0][0]);
    const auto ys = up    / fabsf(projectionMatrix[1][1]);
    glm::vec3 edge[4];
    edge[0] = dir - xs - ys;
    edge[1] = dir + xs - ys;
    edge[2] = dir + xs + ys;
    edge[3] = dir - xs + ys;

    // make frustum planes
    plane[0] = glm::vec4(dir, -(glm::dot(eye, dir) + m_near));
    makePlane(edge[0], edge[3], 1);
    makePlane(edge[2], edge[1], 2);
    makePlane(edge[1], edge[0], 3);
    makePlane(edge[3], edge[2], 4);

    plane[5] = -plane[0];
    plane[5].w += m_far;

    // make far corners
    for (int i = 0; i < 4; i++)
        farCorner[i] = eye + m_far * edge[i];

    // setup tilt and angle
    tilt = (float)((180.0 / M_PI) * atan2((double)dir[2], 1.0));
    rotation = (float)((180.0 / M_PI) * atan2((double)dir[1], (double)dir[2]));
}


void Frustum::setFarPlaneCull(bool useCulling)
{
    // far clip plane
    if (useCulling)
        planeCount = 6;
    else
        planeCount = 5;
}


void Frustum::setProjection(float fov,
                            float _m_near, float _m_far, float m_deep_far,
                            int width, int height, int viewHeight)
{
    // do easy stuff
    m_near = _m_near;
    m_far = _m_far;
    fovx = fov;

    // clear the far plane culling here
    planeCount = 5;

    // compute projectionMatrix
    const float s = 1.0f / tanf(fov / 2.0f);
    const float fracHeight = 1.0f - float(viewHeight) / float(height);
    projectionMatrix[0][0] = s;
    projectionMatrix[1][1] = (1.0f - fracHeight) * s * float(width) / float(viewHeight);
    projectionMatrix[2][0] = 0.0f;
    projectionMatrix[2][1] = -fracHeight;
    projectionMatrix[2][2] = -(m_far + m_near) / (m_far - m_near);
    projectionMatrix[2][3] = -1.0f;
    projectionMatrix[3][0] = 0.0f;
    projectionMatrix[3][2] = -2.0f * m_far * m_near / (m_far - m_near);
    projectionMatrix[3][3] = 0.0f;

    deepProjectionMatrix[0][0] = projectionMatrix[0][0];
    deepProjectionMatrix[1][1] = projectionMatrix[1][1];
    deepProjectionMatrix[2][0] = projectionMatrix[2][0];
    deepProjectionMatrix[2][1] = projectionMatrix[2][1];
    deepProjectionMatrix[2][3] = projectionMatrix[2][3];
    deepProjectionMatrix[3][0] = projectionMatrix[3][0];
    deepProjectionMatrix[3][3] = projectionMatrix[3][3];
    deepProjectionMatrix[2][2] = -(m_deep_far + m_near) / (m_deep_far - m_near);
    deepProjectionMatrix[3][2] = -2.0f * m_deep_far * m_near / (m_deep_far - m_near);

    // get field of view in y direction
    fovy = 2.0f * atanf(1.0f / projectionMatrix[1][1]);

    // compute areaFactor
    areaFactor = 0.25f * s * float(height);
    areaFactor = (float)(M_PI * areaFactor * areaFactor);
}


void Frustum::setOffset(float eyeOffset, float focalPlane)
{
    projectionMatrix[3][0] = 0.5f * eyeOffset * projectionMatrix[0][0];
    projectionMatrix[2][0] = projectionMatrix[3][0] / focalPlane;
    deepProjectionMatrix[2][0] = projectionMatrix[2][0];
    deepProjectionMatrix[3][0] = projectionMatrix[3][0];
}


void Frustum::makePlane(const glm::vec3 &v1, const glm::vec3 &v2, int index)
{
    // get normal by crossing v1 and v2 and normalizing
    auto n = glm::normalize(glm::cross(v1, v2));
    plane[index] = glm::vec4(n, -glm::dot(eye, n));
}


// these next two functions should be more generic
// flipX, flipY, flipZ, all with and offset along the axis
void Frustum::flipVertical()
{
    eye[2] = -eye[2];
    target[2] = -target[2];
    setView(eye, target);
    projectionMatrix[1][1]     = -projectionMatrix[1][1];
    deepProjectionMatrix[1][1] = -deepProjectionMatrix[1][1];

    return;
}


void Frustum::flipHorizontal()
{
    eye[0] = -eye[0];
    target[0] = -target[0];
    setView(eye, target);
    projectionMatrix[0][0]     = -projectionMatrix[0][0];
    deepProjectionMatrix[0][0] = -deepProjectionMatrix[0][0];
    return;
}


// used for radar culling, not really a frustum
void Frustum::setOrthoPlanes(const Frustum& view, float width, float breadth)
{
    // setup the eye, and the clipping planes
    eye = view.getEye();

    glm::vec3 front;
    auto tmp = glm::vec2(view.getDirection());
    if (tmp.x || tmp.y)
        front = glm::vec3(glm::normalize(tmp), 0.0f);
    else
        front = glm::vec3(1.0f, 0.0f, 0.0f);

    auto left = glm::vec3(-front[1], front[0], 0.0f);

    float dotl = glm::dot(eye, left);
    float dotf = glm::dot(eye, front);
    plane[1] = glm::vec4( left,  -dotl  + width);
    plane[2] = glm::vec4(-left,   dotl  + width);
    plane[3] = glm::vec4( front, -dotf + breadth);
    plane[4] = glm::vec4(-front,  dotf + breadth);

    // disable the near and far planes
    plane[0] = glm::vec4(0.0f, 0.0f, 1.0f, -1.0e6);
    plane[5] = glm::vec4(0.0f, 0.0f, 1.0f, -1.0e6);

    planeCount = 5;

    return;
}


// Local Variables: ***
// mode: C++ ***
// tab-width: 4 ***
// c-basic-offset: 4 ***
// indent-tabs-mode: nil ***
// End: ***
// ex: shiftwidth=4 tabstop=4
