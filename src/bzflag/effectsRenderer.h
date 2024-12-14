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

/*
* EffectsRenderer:
*   Encapsulates rendering of effects ( spawn flashes, sparks, explosions, etc...)
*
*/

#ifndef BZF_EFFECTS_RENDERER_H
#define BZF_EFFECTS_RENDERER_H

#include "common.h"

/* system headers */
#include <string>
#include <vector>

/* common interface headers */
#include "OpenGLGState.h"
#include "SceneRenderer.h"

#include "Singleton.h"

#include "TankSceneNode.h"
#include "Flag.h"
#include "Player.h"


#define EFFECTS (EffectsRenderer::instance())

class BasicEffect
{
public:
    BasicEffect();
    virtual ~BasicEffect() {};

    virtual void setPos(const glm::vec3 &pos, const float *rot);
    virtual void setVel (const glm::vec3 &vel);
    virtual void setColor(const glm::vec3 &rgb);
    virtual void setStartTime ( float time );

    virtual void freeContext(void) {};
    virtual void rebuildContext(void) {};

    virtual bool update ( float time );
    virtual void draw(const SceneRenderer &) {};

protected:

    glm::vec3 position;
    float   rotation[3];
    glm::vec3 velocity;
    glm::vec3 color;
    float   startTime;
    float   lifetime;
    float   lastTime;
    float   deltaTime;
    float   age;
    float   lifeParam;
};

class DeathEffect : public BasicEffect, public TankDeathOverride
{
public:
    DeathEffect() : BasicEffect(),TankDeathOverride(),player(NULL) {};
    virtual ~DeathEffect()
    {
        /*if (player)player->setDeathEffect(NULL)*/;
    }

    virtual bool SetDeathRenderParams ( TankDeathOverride::DeathParams &UNUSED(params) )
    {
        return false;
    }
    virtual bool ShowExplosion ( void )
    {
        return true;
    }
    virtual bool GetDeathVector (glm::vec3 &UNUSED(vel))
    {
        return false;
    }

    void setPlayer ( Player* p)
    {
        player=p;
    }
protected:
    Player *player;
};

typedef std::vector<BasicEffect*>   tvEffectsList;

class EffectsRenderer : public Singleton<EffectsRenderer>
{
public:
    // called once to setup the effects system
    void init(void);

    // called to update the various effects
    void update(void);

    // called to draw all the current effects
    void draw(const SceneRenderer& sr);

    // called when the GL lists need to be deleted
    void freeContext(void);

    // called when the GL lists need to be remade
    void rebuildContext(void);

    // spawn flashes
    void addSpawnEffect(const glm::vec3 &rgb, const glm::vec3 &pos);
    std::vector<std::string> getSpawnEffectTypes ( void );

    // shot flashes
    void addShotEffect (
        const glm::vec3 &rgb,
        const glm::vec3 &pos,
        float rot,
        const glm::vec3 &vel,
        int _type = -1);
    std::vector<std::string> getShotEffectTypes ( void );

    // gm puffs
    void addGMPuffEffect(const glm::vec3 &pos, float rot[2]);
    std::vector<std::string> getGMPuffEffectTypes ( void );

    // death effects
    DeathEffect* addDeathEffect (
        const glm::vec3 &rgb,
        const glm::vec3 &pos,
        float rot,
        int reason,
        Player *player,
        FlagType* flag = NULL );
    std::vector<std::string> getDeathEffectTypes ( void );

    // landing effects
    void addLandEffect (const glm::vec3 &rgb, const glm::vec3 &pos, float rot);
    std::vector<std::string> getLandEffectTypes ( void );

    // rico effect
    void addRicoEffect(const glm::vec3 &pos, float rot[2]);
    std::vector<std::string> getRicoEffectTypes ( void );

    // shot teleport effect
    void addShotTeleportEffect(const glm::vec3 &pos, float rot[2]);
    std::vector<std::string> getShotTeleportEffectTypes ( void );


protected:
    friend class Singleton<EffectsRenderer>;

protected:
    EffectsRenderer();
    ~EffectsRenderer();

    tvEffectsList   effectsList;
};

#endif // BZF_EFFECTS_RENDERER_H

// Local Variables: ***
// mode: C++ ***
// tab-width: 4 ***
// c-basic-offset: 4 ***
// indent-tabs-mode: nil ***
// End: ***
// ex: shiftwidth=4 tabstop=4
