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

/* TankSceneNode:
 *  Encapsulates information for rendering a tank
 */

#ifndef BZF_TANK_SCENE_NODE_H
#define BZF_TANK_SCENE_NODE_H

// Inherits from
#include "SceneNode.h"

// System headers
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>

// Common headers
#include "OpenGLLight.h"
#include "TankGeometryMgr.h"

class TankSceneNode;

class TankDeathOverride
{
public:
    virtual ~TankDeathOverride() {};

    class DeathParams
    {
    public:
        DeathParams( float param, const glm::vec4 &c): part()
        {
            scale = glm::vec3(1);
            explodeParam = param;
            color = c;
            draw = true;
        }

        TankGeometryEnums::TankPart part;
        glm::vec3 pos;
        glm::vec3 rot;
        glm::vec3 scale;
        float explodeParam;
        glm::vec4 color;
        bool  draw;
    };

    virtual bool SetDeathRenderParams ( DeathParams &/*params*/ ) = 0;
    virtual bool ShowExplosion ( void ) = 0;
    virtual bool GetDeathVector (glm::vec3 &/*vel*/) = 0;
};

class TankIDLSceneNode : public SceneNode
{
public:
    TankIDLSceneNode(const TankSceneNode*);
    ~TankIDLSceneNode();

    void        move(const glm::vec4 &plane);

    void        notifyStyleChange() override;
    void        addRenderNodes(SceneRenderer&) override;
    // Irix 7.2.1 and solaris compilers appear to have a bug.  if the
    // following declaration isn't public it generates an error when trying
    // to declare SphereFragmentSceneNode::FragmentRenderNode a friend in
    // SphereSceneNode::SphereRenderNode.  i think this is a bug in the
    // compiler because:
    //   no other compiler complains
    //   public/protected/private adjust access not visibility
    //     SphereSceneNode isn't requesting access, it's granting it
//  protected:
public:
    class IDLRenderNode : public RenderNode
    {
    public:
        IDLRenderNode(const TankIDLSceneNode*);
        ~IDLRenderNode();
        void        render() override;
        const glm::vec3 &getPosition() const override;
    private:
        const TankIDLSceneNode* sceneNode;
        static const int    idlFaces[][5];
        static const glm::vec3 idlVertex[];
    };
    friend class IDLRenderNode;

private:
    const TankSceneNode *tank;
    glm::vec4       plane;
    OpenGLGState    gstate;
    IDLRenderNode   renderNode;
};

class TankSceneNode : public SceneNode
{
    friend class TankIDLSceneNode;
    friend class TankIDLSceneNode::IDLRenderNode;
public:
    TankSceneNode(const glm::vec3 &pos,
                  const glm::vec3 &forward);
    ~TankSceneNode();

    void move(const glm::vec3 &pos, const glm::vec3 &forward);

    void        setColor(GLfloat r, GLfloat g,
                         GLfloat b, GLfloat a = 1.0f);
    void        setColor(const glm::vec4 &rgba);
    void        setMaterial(const OpenGLMaterial&);
    void        setTexture(const int);
    void        setJumpJetsTexture(const int);

    void        setNormal();
    void        setObese();
    void        setTiny();
    void        setNarrow();
    void        setThief();
    void        setDimensions(const glm::vec3 &size);

    void        setClipPlane(const glm::vec4 &plane);
    void        resetClipPlane();
    void        setExplodeFraction(float t);
    void        setJumpJets(float scale);

    void        setInTheCockpit(bool value);
    void        setOnlyShadows(bool value);

    void        rebuildExplosion();
    void        addTreadOffsets(float left, float right);

    void        notifyStyleChange() override;
    void        addRenderNodes(SceneRenderer&) override;
    void        addShadowNodes(SceneRenderer&) override;

    bool        cullShadow(int planeCount,
                           const glm::vec4 *planes) const override;

    void        addLight(SceneRenderer&) override;

    void        renderRadar() override;

    static void     setMaxLOD(int maxLevel);

    void        setDeathOverride( TankDeathOverride* o)
    {
        deathOverride = o;
    }
    TankDeathOverride   *getDeathOverride( void )
    {
        return deathOverride;
    }

    glm::vec3 explodePos;
protected:
    TankDeathOverride   *deathOverride;

    class TankRenderNode : public RenderNode
    {
    public:
        TankRenderNode(const TankSceneNode*);
        ~TankRenderNode();
        void        setShadow();
        void        setRadar(bool);
        void        setTreads(bool);
        void        setTankLOD(TankGeometryEnums::TankLOD);
        void        setTankSize(TankGeometryEnums::TankSize);
        void        sortOrder(bool above, bool towards, bool left);
        void        setNarrowWithDepth(bool narrow);
        const glm::vec3 &getPosition() const override;

        void        render() override;
        void        renderPart(TankGeometryEnums::TankPart part);
        void        renderParts();
        void        renderTopParts();
        void        renderLeftParts();
        void        renderRightParts();
        void        renderNarrowWithDepth();
        void        renderLights();
        void        renderJumpJets();
        void        setupPartColor(TankGeometryEnums::TankPart part);
        bool        setupTextureMatrix(TankGeometryEnums::TankPart part);

    protected:
        const TankSceneNode* sceneNode;
        TankGeometryEnums::TankLOD drawLOD;
        TankGeometryEnums::TankSize drawSize;
        const glm::vec4 *color;
        GLfloat     alpha;
        bool        isRadar;
        bool        isTreads;
        bool        isShadow;
        bool        left;
        bool        above;
        bool        towards;
        bool        isExploding;
        bool        narrowWithDepth;
        GLfloat     explodeFraction;
        static const glm::vec3 centerOfGravity[TankGeometryEnums::LastTankPart];
    };
    friend class TankRenderNode;

private:
    GLfloat     azimuth, elevation;
    GLfloat     baseRadius;
    glm::vec3   dimensions; // tank dimensions
    float       leftTreadOffset;
    float       rightTreadOffset;
    float       leftWheelOffset;
    float       rightWheelOffset;
    bool        useDimensions;
    bool        onlyShadows;
    bool        transparent, sort;
    float       explodeFraction;
    bool        clip;
    bool        inTheCockpit;
    glm::vec4   color;
    glm::dvec4  clipPlane;
    OpenGLGState    gstate;
    OpenGLGState    treadState;
    OpenGLGState    lightsGState;
    TankRenderNode  tankRenderNode;
    TankRenderNode  treadsRenderNode;
    TankRenderNode  shadowRenderNode;
    TankGeometryEnums::TankSize tankSize;
    glm::vec3   vel[TankGeometryEnums::LastTankPart];
    glm::vec4   spin[TankGeometryEnums::LastTankPart];
    bool        jumpJetsOn;
    GLfloat     jumpJetsScale;
    GLfloat     jumpJetsLengths[4];
    glm::vec3   jumpJetsPositions[4];
    OpenGLLight     jumpJetsRealLight;
    OpenGLLight     jumpJetsGroundLights[4];
    OpenGLGState    jumpJetsGState;

    static int      maxLevel;
    static const int    numLOD;
    static glm::vec3 jumpJetsModel[4];
};


#endif // BZF_TANK_SCENE_NODE_H

// Local Variables: ***
// mode: C++ ***
// tab-width: 4 ***
// c-basic-offset: 4 ***
// indent-tabs-mode: nil ***
// End: ***
// ex: shiftwidth=4 tabstop=4
