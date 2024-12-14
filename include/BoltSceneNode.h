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

/* BoltSceneNode:
 *  Encapsulates information for rendering a ball lighting bolt.
 */

#ifndef BZF_BOLT_SCENE_NODE_H
#define BZF_BOLT_SCENE_NODE_H

// Inherits from
#include "SceneNode.h"

// System headers
#include <glm/vec4.hpp>

// Common headers
#include "OpenGLLight.h"

class BoltSceneNode : public SceneNode
{
public:
    BoltSceneNode(const glm::vec3 &pos, const glm::vec3 &vel, bool super);
    ~BoltSceneNode();

    void        setFlares(bool);
    void        setSize(float radius);
    void        setColor(GLfloat r, GLfloat g, GLfloat b, GLfloat a = 1.0f);
    void        setTextureColor(GLfloat r, GLfloat g, GLfloat b, GLfloat a = 1.0f);
    void        setColor(const glm::vec3 &rgb);
    void        setTeamColor(const glm::vec3 &rgb);
    void        setTexture(const int);
    void        setTextureAnimation(int cu, int cv);

    bool        getColorblind() const;
    void        setColorblind(bool);

    void move(const glm::vec3 &pos, const glm::vec3 &forward);
    void        addLight(SceneRenderer&) override;

    void        notifyStyleChange() override;
    void        addRenderNodes(SceneRenderer&) override;

protected:
    bool        isSuper;

    class BoltRenderNode : public RenderNode
    {
    public:
        BoltRenderNode(const BoltSceneNode*);
        ~BoltRenderNode();
        void        setColor(const glm::vec4 &rgba);
        void        setTextureColor(const glm::vec4 &rgba);
        void        render() override;
        const glm::vec3 &getPosition() const override;
        void        setAnimation(int cu, int cv);

        void        renderGeoBolt();
        void        renderGeoGMBolt();
        void        renderGeoPill( float radius, float len, int segments, float endRad = -1);

    private:
        const BoltSceneNode* sceneNode;
        int     u, v, cu, cv;
        GLfloat     du, dv;
        glm::vec4   mainColor;
        glm::vec4   innerColor;
        glm::vec4   outerColor;
        glm::vec4   coronaColor;
        glm::vec4   flareColor;
        glm::vec4   textureColor;
        int     numFlares;
        float       theta[6];
        float       phi[6];

        static glm::vec2 core[9];
        static const glm::vec2 corona[8];
        static const GLfloat CoreFraction;
        static const GLfloat FlareSize;
        static const GLfloat FlareSpread;
    };
    friend class BoltRenderNode;

private:
    bool        invisible;
    bool        drawFlares;
    bool        texturing;
    bool        colorblind;
    float       size;
    glm::vec4   color;
    glm::vec4   teamColor;
    glm::vec3       velocity;
    OpenGLLight     light;
    OpenGLGState    gstate;
    OpenGLGState    colorblindGState;
    BoltRenderNode  renderNode;
    float       azimuth, elevation, length;
};

#endif // BZF_BOLT_SCENE_NODE_H

// Local Variables: ***
// mode: C++ ***
// tab-width: 4 ***
// c-basic-offset: 4 ***
// indent-tabs-mode: nil ***
// End: ***
// ex: shiftwidth=4 tabstop=4
