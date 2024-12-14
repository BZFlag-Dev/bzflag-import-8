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

/* WallSceneNode:
 *  Encapsulates information for rendering an wall.
 *
 * WallGeometry:
 *  Encapsulates vertices and uv's for a wall
 *
 * Walls are flat and don't move.  A wall also picks a level of
 * detail based on its projected area and on the presence of
 * light sources nearby (to capture light highlights).
 */

#ifndef BZF_WALL_SCENE_NODE_H
#define BZF_WALL_SCENE_NODE_H

// Inherits from
#include "SceneNode.h"

class WallSceneNode : public SceneNode
{
public:
    WallSceneNode();
    ~WallSceneNode();

    const glm::vec4 *getPlane() const override;
    const glm::vec4 &getColor() const;
    const glm::vec4 *getDynamicColor() const;
    const glm::vec4 &getModulateColor() const;
    const glm::vec4 &getLightedColor() const;
    const glm::vec4 &getLightedModulateColor() const;
    GLfloat         getDistance(const glm::vec3 &) const override;
    bool            inAxisBox (const Extents& exts) const override;

    void        setColor(GLfloat r, GLfloat g,
                         GLfloat b, GLfloat a = 1.0f);
    void        setColor(const glm::vec4 &rgba);
    void        setModulateColor(GLfloat r, GLfloat g,
                                 GLfloat b, GLfloat a = 1.0f);
    void        setModulateColor(const glm::vec4 &rgba);
    void        setLightedColor(GLfloat r, GLfloat g,
                                GLfloat b, GLfloat a = 1.0f);
    void        setLightedColor(const glm::vec4 &rgba);
    void        setLightedModulateColor(GLfloat r, GLfloat g,
                                        GLfloat b, GLfloat a = 1.0f);
    void        setLightedModulateColor(const glm::vec4 &rgba);
    void        setMaterial(const OpenGLMaterial&);
    void        setTexture(const int);
    void        setTextureMatrix(const GLfloat* texmat);
    void        setDynamicColor(const glm::vec4 *color);
    void        setBlending(bool);
    void        setSphereMap(bool);
    void        setNoCulling(bool);
    void        setNoSorting(bool);
    void        setAlphaThreshold(float);

    void        setColor();

    bool        cull(const ViewFrustum&) const override;
    void        notifyStyleChange() override;

    void        copyStyle(WallSceneNode*);

    void        setUseColorTexture(bool use)
    {
        useColorTexture = use;
    }
protected:
    int         getNumLODs() const;
    void        setNumLODs(int, float* elementAreas);
    void        setPlane(const glm::vec4 &);
    int         pickLevelOfDetail(const SceneRenderer&) const;

    int         getStyle() const;
    const OpenGLGState* getGState() const
    {
        return &gstate;
    }
    const OpenGLGState* getWallGState() const;

    static int      splitWall(const glm::vec4 &plane,
                              const std::vector<glm::vec3> &vertices,
                              const std::vector<glm::vec2> &uvs,
                              SceneNode*& front, SceneNode*& back); // const

    glm::vec4 plane;   // unit normal, distance to origin
private:
    static void splitEdge(float d1, float d2,
                          const glm::vec3 &p1, const glm::vec3 &p2,
                          const glm::vec2 &uv1, const glm::vec2 &uv2,
                          glm::vec3 &p, glm::vec2 &uv); //const

private:
    int         numLODs;
    float*      elementAreas;
    const glm::vec4 *dynamicColor;
    glm::vec4   color;
    glm::vec4   modulateColor;
    glm::vec4   lightedColor;
    glm::vec4   lightedModulateColor;
    float       alphaThreshold;
    int         style;
    bool        noCulling;
    bool        noSorting;
    bool        isBlended;
    bool        wantBlending;
    bool        wantSphereMap;
    OpenGLGState    gstate;
    bool        useColorTexture;
};

//
// WallSceneNode
//

inline int WallSceneNode::getNumLODs() const
{
    return numLODs;
}

inline const glm::vec4 &WallSceneNode::getColor() const
{
    return color;
}
inline const glm::vec4 *WallSceneNode::getDynamicColor() const
{
    return dynamicColor;
}
inline const glm::vec4 &WallSceneNode::getModulateColor() const
{
    return modulateColor;
}
inline const glm::vec4 &WallSceneNode::getLightedColor() const
{
    return lightedColor;
}
inline const glm::vec4 &WallSceneNode::getLightedModulateColor() const
{
    return lightedModulateColor;
}

inline int WallSceneNode::getStyle() const
{
    return style;
}
inline const OpenGLGState* WallSceneNode::getWallGState() const
{
    return &gstate;
}


#endif // BZF_WALL_SCENE_NODE_H

// Local Variables: ***
// mode: C++ ***
// tab-width: 4 ***
// c-basic-offset: 4 ***
// indent-tabs-mode: nil ***
// End: ***
// ex: shiftwidth=4 tabstop=4
