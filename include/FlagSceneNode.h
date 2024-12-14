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

/* FlagSceneNode:
 *  Encapsulates information for rendering a flag.
 */

#pragma once

// Inherits from
#include "SceneNode.h"

class FlagSceneNode : public SceneNode
{
public:
    FlagSceneNode(const glm::vec3 &pos);
    ~FlagSceneNode();

    static void     waveFlag(float dt);
    static void     freeFlag();

    void        move(const glm::vec3 &pos);
    void        setAngle(GLfloat angle);
    void        setWind(const GLfloat wind[3], float dt);
    void        setBillboard(bool billboard);

    const glm::vec4 &getColor() const
    {
        return color;
    }
    void        setColor(GLfloat r, GLfloat g,
                         GLfloat b, GLfloat a = 1.0f);
    void        setColor(const glm::vec4 &rgba);
    void        setTexture(const int);

    void        notifyStyleChange() override;
    void        addRenderNodes(SceneRenderer&) override;
    void        addShadowNodes(SceneRenderer&) override;

    bool        cullShadow(int planeCount,
                           const glm::vec4 planes[]) const override;
protected:
    class FlagRenderNode : public RenderNode
    {
    public:
        FlagRenderNode(const FlagSceneNode*);
        ~FlagRenderNode();
        void        render() override;
        const glm::vec3 &getPosition() const override;
    private:
        const FlagSceneNode* sceneNode;
        int      waveReference;
    };
    friend class FlagRenderNode;

private:
    bool        billboard;
    GLfloat     angle;
    GLfloat     tilt;
    GLfloat     hscl;
    glm::vec4   color;
    bool        transparent;
    bool        texturing;
    OpenGLGState    gstate;
    FlagRenderNode  renderNode;
};


// Local Variables: ***
// mode: C++ ***
// tab-width: 4 ***
// c-basic-offset: 4 ***
// indent-tabs-mode: nil ***
// End: ***
// ex: shiftwidth=4 tabstop=4
