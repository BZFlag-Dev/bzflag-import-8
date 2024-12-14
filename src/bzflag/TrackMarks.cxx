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


// Interface header
#include "TrackMarks.h"

// Common interface headers
#include "StateDatabase.h"
#include "BZDBCache.h"
#include "Obstacle.h"
#include "CollisionManager.h"
#include "PhysicsDriver.h"
#include "Ray.h"
#include "TextureManager.h"
#include "SceneDatabase.h"
#include "SceneRenderer.h"
#include "SceneNode.h"


using namespace TrackMarks;

//#define FANCY_TREADMARKS // uses glPolygonOffset()  (only for zbuffer)

enum TrackType
{
    TreadsTrack = 0,
    PuddleTrack = 1,
    SmokeTrack  = 2
};

enum TrackSides
{
    LeftTread =  (1 << 0),
    RightTread = (1 << 1),
    BothTreads = (LeftTread | RightTread)
};


//
// Helper Classes  (TrackEntry, TrackList, TrackRenderNode, TrackSceneNode)
//

class TrackEntry
{
public:
    ~TrackEntry();
    TrackEntry* getNext();

protected:
    TrackEntry* next;
    TrackEntry* prev;
public:
    glm::vec3 pos;
    float angle;
    float scale;
    char sides;
    int phydrv;
    float lifeTime;
    class TrackSceneNode* sceneNode;

    friend class TrackList;
};

inline TrackEntry* TrackEntry::getNext()
{
    return next;
}


class TrackList
{
public:
    TrackList()
    {
        start = end = NULL;
    }
    ~TrackList()
    {
        clear();
    }

    void clear();

    TrackEntry* getStart()
    {
        return start;
    }
    TrackEntry* getEnd()
    {
        return end;
    }

    void addNode(TrackEntry&);
    TrackEntry* removeNode(TrackEntry*); // return the next entry

private:
    TrackEntry* start;
    TrackEntry* end;
};

inline void TrackList::addNode(TrackEntry& te)
{
    TrackEntry* copy = new TrackEntry;
    *copy = te;
    copy->next = NULL;
    if (end == NULL)
    {
        copy->prev = NULL;
        start = end = copy;
    }
    else
    {
        copy->prev = end;
        end->next = copy;
        end = copy;
    }
    return;
}

inline TrackEntry* TrackList::removeNode(TrackEntry* te)
{
    TrackEntry* const next = te->next;
    TrackEntry* const prev = te->prev;
    if (next != NULL)
        next->prev = prev;
    else
        end = prev;
    if (prev != NULL)
        prev->next = next;
    else
        start = next;
    delete te; // delete it (and its sceneNode, in ~TrackEntry())
    return next;
}



class TrackRenderNode : public RenderNode
{
public:
    TrackRenderNode(const TrackEntry* te, TrackType type);
    ~TrackRenderNode();
    void render() override;
    void renderShadow() override
    {
        return;
    }
    const glm::vec3 &getPosition() const override
    {
        return te->pos;
    }

private:
    TrackType type;
    const TrackEntry* te;
};


class TrackSceneNode : public SceneNode
{
public:
    TrackSceneNode(const TrackEntry*, TrackType, const OpenGLGState*);
    ~TrackSceneNode();
    void addRenderNodes(SceneRenderer&);
    void update(); // set the sphere properties

private:
    TrackType type;
    const TrackEntry* te;
    const OpenGLGState* gstate;
    TrackRenderNode renderNode;
};


//
// Local Variables
//

static TrackList SmokeList;
static TrackList PuddleList;
static TrackList TreadsGroundList;
static TrackList TreadsObstacleList;
static float TrackFadeTime = 5.0f;
static float UserFadeScale = 1.0f;
static AirCullStyle AirCull = FullAirCull;

// FIXME - get these from AnimatedTreads
static const float TreadOutside = 1.4f;
static const float TreadInside = 0.875f;

static const float TreadMiddle = 0.5f * (TreadOutside + TreadInside);
static const float TreadMarkWidth = 0.2f;

static OpenGLGState smokeGState;
static const char smokeTexture[] = "puddle"; // FIXME - not implemented
static OpenGLGState puddleGState;
static const char puddleTexture[] = "puddle";
static OpenGLGState treadsGState;

#ifndef FANCY_TREADMARKS
static const float TextureHeightOffset = 0.05f;
#else
static const float TextureHeightOffset = 0.0f;
#endif // FANCY_TREADMARKS


//
// Function Prototypes
//

static void setup();
static void drawPuddle(const TrackEntry& te);
static void drawTreads(const TrackEntry& te);
static bool onBuilding(const float pos[3]);
static void updateList(TrackList& list, float dt);
static void addEntryToList(TrackList& list,
                           TrackEntry& te, TrackType type);


/****************************************************************************/
//
// TrackMarks
//

void TrackMarks::init()
{
    clear();
    setup();
    setUserFade(BZDB.eval("userTrackFade"));
    setAirCulling((AirCullStyle)BZDB.evalInt("trackMarkCulling"));
    return;
}


void TrackMarks::kill()
{
    clear();
    return;
}


void TrackMarks::clear()
{
    SmokeList.clear();
    PuddleList.clear();
    TreadsGroundList.clear();
    TreadsObstacleList.clear();
    return;
}


void TrackMarks::setUserFade(float value)
{
    if (value < 0.0f)
        value = 0.0f;
    else if (value > 1.0f)
        value = 1.0f;

    UserFadeScale = value;
    BZDB.setFloat("userTrackFade", value);

    return;
}


float TrackMarks::getUserFade()
{
    return BZDB.eval("userTrackFade");
}


void TrackMarks::setAirCulling(AirCullStyle style)
{
    if ((style < NoAirCull) || (style > FullAirCull))
        style = NoAirCull;

    AirCull = style;
    BZDB.setInt("trackMarkCulling", style);

    return;
}


AirCullStyle TrackMarks::getAirCulling()
{
    return (AirCullStyle)BZDB.evalInt("trackMarkCulling");
}


static void addEntryToList(TrackList& list,
                           TrackEntry& te, TrackType type)
{
    // push the entry
    list.addNode(te);

    // make a sceneNode for the BSP rendering, if not on the ground
    if (!BZDBCache::zbuffer && (te.pos[2] != TextureHeightOffset))
    {
        const OpenGLGState* gstate = NULL;
        if (type == TreadsTrack)
            gstate = &treadsGState;
        else if (type == PuddleTrack)
            gstate = &puddleGState;
        else if (type == SmokeTrack)
            gstate = &smokeGState;
        else
            return;
        TrackEntry* copy = list.getEnd();
        copy->sceneNode = new TrackSceneNode(copy, type, gstate);
    }
    return;
}


bool TrackMarks::addMark(const glm::vec3 &pos, float scale, float angle,
                         int phydrv)
{
    TrackEntry te;
    TrackType type;
    te.lifeTime = 0.0f;
    te.sceneNode = NULL;

    // determine the track mark type
    if ((pos[2] <= 0.1f) && BZDB.get(StateDatabase::BZDB_MIRROR) != "none")
    {
        type = PuddleTrack;
        if (pos[2] < 0.0f)
        {
            scale = 0.0f; // single puddle, like Narrow tanks
        }
    }
    else
    {
        type = TreadsTrack;
        if (scale < 0.01f)
        {
            return false; // Narrow tanks don't draw tread marks
        }
        if (pos[2] < 0.0f)
        {
            return false; // Burrowed tanks don't draw tread marks
        }
    }

    // copy some parameters
    te.pos = pos;
    if (pos[2] < 0.0f)
        te.pos[2] = TextureHeightOffset;
    else
        te.pos[2] += TextureHeightOffset;
    te.scale = scale;
    te.angle = (float)(angle * (180.0 / M_PI)); // in degress, for glRotatef()

    // only use the physics driver if it matters
    const PhysicsDriver* driver = PHYDRVMGR.getDriver(phydrv);
    if (driver == NULL)
        te.phydrv = -1;
    else
    {
        const float* v = driver->getLinearVel();
        const float av = driver->getAngularVel();
        if ((v[0] == 0.0f) && (v[1] == 0.0f) && (av == 0.0f))
            te.phydrv = -1;
        else
            te.phydrv = phydrv;
    }

    if (type == PuddleTrack)
    {
        // Puddle track marks
        addEntryToList(PuddleList, te, type);
    }
    else
    {
        // Treads track marks
        if (pos[2] == 0.0f)
        {
            // no culling required
            te.sides = BothTreads;
            addEntryToList(TreadsGroundList, te, type);
        }
        else if ((AirCull & InitAirCull) == 0)
        {
            // do not cull the air marks
            te.sides = BothTreads;
            addEntryToList(TreadsObstacleList, te, type);
        }
        else
        {
            // cull based on track mark support
            te.sides = 0;
            float markPos[3];
            markPos[2] = pos[2];
            const float dx = -sinf(angle) * TreadMiddle;
            const float dy = +cosf(angle) * TreadMiddle;
            // left tread
            markPos[0] = pos[0] + dx;
            markPos[1] = pos[1] + dy;
            if (onBuilding(markPos))
                te.sides |= LeftTread;
            // right tread
            markPos[0] = pos[0] - dx;
            markPos[1] = pos[1] - dy;
            if (onBuilding(markPos))
                te.sides |= RightTread;
            // add if required
            if (te.sides != 0)
                addEntryToList(TreadsObstacleList, te, type);
            else
                return false;
        }
    }

    return true;
}


static bool onBuilding(const float pos[3])
{
    const auto dir = glm::vec3(0.0f, 0.0f, -1.0f);
    const auto org = glm::vec3(pos[0], pos[1], pos[2] + 0.1f);
    Ray ray(org, dir);
    const ObsList* olist = COLLISIONMGR.rayTest (&ray, 0.5f);
    for (int i = 0; i < olist->count; i++)
    {
        const Obstacle* obs = olist->list[i];
        if (obs->isFlatTop())
        {
            const float top = obs->getExtents().maxs[2];
            if ((pos[2] >= (top - 0.1f)) && (pos[2] <= (top + 0.1f)))
            {
                const float hitTime = obs->intersect(ray);
                if (hitTime >= 0.0f)
                    return true;
            }
        }
    }
    return false;
}


static void updateList(TrackList& list, float dt)
{
    TrackEntry* ptr = list.getStart();
    while (ptr != NULL)
    {
        TrackEntry& te = *ptr;

        // increase the lifeTime
        te.lifeTime += dt;

        // see if this mark has expired
        if (te.lifeTime > TrackFadeTime)
        {
            ptr = list.removeNode(ptr);
            continue;
        }

        // update for the Physics Driver
        const PhysicsDriver* phydrv = PHYDRVMGR.getDriver(te.phydrv);
        if (phydrv != NULL)
        {

            const float* v = phydrv->getLinearVel();
            te.pos[0] += (v[0] * dt);
            te.pos[1] += (v[1] * dt);

            const float av = phydrv->getAngularVel();
            if (av != 0.0f)
            {
                const float* ap = phydrv->getAngularPos();
                const float da = (av * dt);
                const float cos_val = cosf(da);
                const float sin_val = sinf(da);
                const float dx = te.pos[0] - ap[0];
                const float dy = te.pos[1] - ap[1];
                te.pos[0] = ap[0] + ((cos_val * dx) - (sin_val * dy));
                te.pos[1] = ap[1] + ((cos_val * dy) + (sin_val * dx));
                te.angle += (float)(da * (180.0 / M_PI));
            }

            if ((AirCull & PhyDrvAirCull) != 0)
            {
                // no need to cull ground marks
                if (te.pos[2] == 0.0f)
                    continue;
                // cull the track marks if they aren't supported
                float markPos[3];
                markPos[2] = te.pos[2] - TextureHeightOffset;
                const float radians = (float)(te.angle * (M_PI / 180.0));
                const float dx = -sinf(radians) * TreadMiddle;
                const float dy = +cosf(radians) * TreadMiddle;
                // left tread
                if ((te.sides & LeftTread) != 0)
                {
                    markPos[0] = te.pos[0] + dx;
                    markPos[1] = te.pos[1] + dy;
                    if (!onBuilding(markPos))
                        te.sides &= ~LeftTread;
                }
                // right tread
                if ((te.sides & RightTread) != 0)
                {
                    markPos[0] = te.pos[0] - dx;
                    markPos[1] = te.pos[1] - dy;
                    if (!onBuilding(markPos))
                        te.sides &= ~RightTread;
                }
                // cull this node
                if (te.sides == 0)
                {
                    ptr = list.removeNode(ptr);
                    continue;
                }
            }
        }

        ptr = ptr->getNext();
    }

    return;
}


void TrackMarks::update(float dt)
{
    TrackFadeTime = BZDB.eval(StateDatabase::BZDB_TRACKFADE);
    TrackFadeTime = TrackFadeTime * UserFadeScale;

    if (TrackFadeTime <= 0.0f)
    {
        clear();
        return;
    }

    updateList(SmokeList, dt);
    updateList(PuddleList, dt);
    updateList(TreadsGroundList, dt);
    updateList(TreadsObstacleList, dt);

    return;
}


static void setup()
{
    OpenGLGStateBuilder gb;

    int puddleTexId = -1;
    if (BZDBCache::texture)
    {
        TextureManager &tm = TextureManager::instance();
        puddleTexId = tm.getTextureID(puddleTexture, false);
    }
    gb.reset();
    gb.setShading(GL_FLAT);
    gb.setAlphaFunc(GL_GEQUAL, 0.1f);
    gb.setBlending(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    gb.enableMaterial(false); // no lighting
    if (puddleTexId >= 0)
        gb.setTexture(puddleTexId);
    puddleGState = gb.getState();

    int smokeTexId = -1;
    if (BZDBCache::texture)
    {
        TextureManager &tm = TextureManager::instance();
        smokeTexId = tm.getTextureID(smokeTexture, false);
    }
    gb.reset();
    gb.setShading(GL_FLAT);
    gb.setBlending(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    gb.enableMaterial(false); // no lighting
    if (smokeTexId >= 0)
        gb.setTexture(smokeTexId);
    smokeGState = gb.getState();

    gb.reset();
    gb.setShading(GL_FLAT);
    gb.setBlending(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    gb.enableMaterial(false); // no lighting
    treadsGState = gb.getState();

    return;
}


void TrackMarks::notifyStyleChange()
{
    setup();
    return;
}


void TrackMarks::renderGroundTracks()
{
    TrackEntry* ptr;

    // disable the zbuffer for drawing on the ground
    if (BZDBCache::zbuffer)
    {
        glDepthMask(GL_FALSE);
        glDisable(GL_DEPTH_TEST);
    }

    // draw ground treads
    treadsGState.setState();
    for (ptr = TreadsGroundList.getStart(); ptr != NULL; ptr = ptr->getNext())
        drawTreads(*ptr);

    // draw puddles
    puddleGState.setState();
    for (ptr = PuddleList.getStart(); ptr != NULL; ptr = ptr->getNext())
        drawPuddle(*ptr);

    // re-enable the zbuffer
    if (BZDBCache::zbuffer)
    {
        glDepthMask(GL_TRUE);
        glEnable(GL_DEPTH_TEST);
    }

    return;
}


void TrackMarks::renderObstacleTracks()
{
    if (!BZDBCache::zbuffer)
    {
        return; // this is not for the BSP rendering
    }

    TrackEntry* ptr;

    // disable the zbuffer writing (these are the last things drawn)
    // this helps to avoid the zbuffer fighting/flickering effect
    glDepthMask(GL_FALSE);

    // draw treads
#ifdef FANCY_TREADMARKS
    glDepthFunc(GL_LEQUAL);
    glEnable(GL_POLYGON_OFFSET_FILL);
    glPolygonOffset(-1.0f, -1.0f);
#endif // FANCY_TREADMARKS
    treadsGState.setState();
    for (ptr = TreadsObstacleList.getStart(); ptr != NULL; ptr = ptr->getNext())
        drawTreads(*ptr);
#ifdef FANCY_TREADMARKS
    glDepthFunc(GL_LESS);
    glDisable(GL_POLYGON_OFFSET_FILL);
#endif // FANCY_TREADMARKS

    // draw smoke
    smokeGState.setState();
    for (ptr = SmokeList.getStart(); ptr != NULL; ptr = ptr->getNext())
        drawTreads(*ptr);

    // re-enable the zbuffer writing
    glDepthMask(GL_TRUE);

    return;
}


static void drawPuddle(const TrackEntry& te)
{
    const float ratio = (te.lifeTime / TrackFadeTime);
    const float scale = 2.0f * ratio;
    const float offset = te.scale * TreadMiddle;

    glColor4f(1.0f, 1.0f, 1.0f, 1.0f - ratio);

    glPushMatrix();
    {
        glTranslatef(te.pos[0], te.pos[1], te.pos[2]);
        glRotatef(te.angle, 0.0f, 0.0f, 1.0f);
        glTranslatef(0.0f, +offset, 0.0f);
        glScalef(scale, scale, 1.0f);
        glBegin(GL_TRIANGLE_STRIP);
        {
            glTexCoord2f(0.0f, 0.0f);
            glVertex3f(-1.0f, -1.0f, 0.0f);
            glTexCoord2f(1.0f, 0.0f);
            glVertex3f(+1.0f, -1.0f, 0.0f);
            glTexCoord2f(0.0f, 1.0f);
            glVertex3f(-1.0f, +1.0f, 0.0f);
            glTexCoord2f(1.0f, 1.0f);
            glVertex3f(+1.0f, +1.0f, 0.0f);
        }
        glEnd();
    }
    glPopMatrix();

    // Narrow tanks only need 1 puddle
    if (offset > 0.01f)
    {
        glPushMatrix();
        {
            glTranslatef(te.pos[0], te.pos[1], te.pos[2]);
            glRotatef(te.angle, 0.0f, 0.0f, 1.0f);
            glTranslatef(0.0f, -offset, 0.0f);
            glScalef(scale, scale, 1.0f);

            glBegin(GL_TRIANGLE_STRIP);
            {
                glTexCoord2f(0.0f, 0.0f);
                glVertex3f(-1.0f, -1.0f, 0.0f);
                glTexCoord2f(1.0f, 0.0f);
                glVertex3f(+1.0f, -1.0f, 0.0f);
                glTexCoord2f(0.0f, 1.0f);
                glVertex3f(-1.0f, +1.0f, 0.0f);
                glTexCoord2f(1.0f, 1.0f);
                glVertex3f(+1.0f, +1.0f, 0.0f);
            }
            glEnd();
        }
        glPopMatrix();
    }

    return;
}


static void drawTreads(const TrackEntry& te)
{
    if (!TrackFadeTime)
        return;

    const float ratio = (te.lifeTime / TrackFadeTime);

    glColor4f(0.0f, 0.0f, 0.0f, 1.0f - ratio);

    glPushMatrix();
    {
        glTranslatef(te.pos[0], te.pos[1], te.pos[2]);
        glRotatef(te.angle, 0.0f, 0.0f, 1.0f);
        glScalef(1.0f, te.scale, 1.0f);

        const float halfWidth = 0.5f * TreadMarkWidth;

        if ((te.sides & LeftTread) != 0)
            glRectf(-halfWidth, +TreadInside, +halfWidth, +TreadOutside);
        if ((te.sides & RightTread) != 0)
            glRectf(-halfWidth, -TreadOutside, +halfWidth, -TreadInside);
    }
    glPopMatrix();

    return;
}


void TrackMarks::addSceneNodes(SceneDatabase* scene)
{
    // Depth Buffer does not need to use SceneNodes
    if (BZDBCache::zbuffer)
        return;

    // tread track marks on obstacles
    TrackEntry* ptr;
    for (ptr = TreadsObstacleList.getStart(); ptr != NULL; ptr = ptr->getNext())
    {
        const TrackEntry& te = *ptr;
        if (te.sceneNode != NULL)
        {
            te.sceneNode->update();
            scene->addDynamicNode(te.sceneNode);
        }
    }

    // smoke track marks in the air
    for (ptr = SmokeList.getStart(); ptr != NULL; ptr = ptr->getNext())
    {
        const TrackEntry& te = *ptr;
        if (te.sceneNode != NULL)
        {
            te.sceneNode->update();
            scene->addDynamicNode(te.sceneNode);
        }
    }

    return;
}


/****************************************************************************/
//
// TrackEntry
//

TrackEntry::~TrackEntry()
{
    delete sceneNode;
    return;
}


//
// TrackList
//

void TrackList::clear()
{
    TrackEntry* te = start;
    while (te != NULL)
    {
        TrackEntry* next = te->next;
        delete te;
        te = next;
    }
    start = end = NULL;
    return;
}


//
// TrackRenderNode
//

TrackRenderNode::TrackRenderNode(const TrackEntry* _te, TrackType _type)
{
    te = _te;
    type = _type;
    return;
}


TrackRenderNode::~TrackRenderNode()
{
    return;
}


void TrackRenderNode::render()
{
    if (type == TreadsTrack)
        drawTreads(*te);
    else if (type == PuddleTrack)
        drawPuddle(*te);
    else if (type == SmokeTrack)
        drawTreads(*te);
    return;
}


//
// TrackSceneNode
//

TrackSceneNode::TrackSceneNode(const TrackEntry* _te, TrackType _type,
                               const OpenGLGState* _gstate) :
    renderNode(_te, _type)
{
    te = _te;
    type = _type;
    gstate = _gstate;
    return;
}

TrackSceneNode::~TrackSceneNode()
{
    return;
}

void TrackSceneNode::addRenderNodes(SceneRenderer& renderer)
{
    renderer.addRenderNode(&renderNode, gstate);
    return;
}

void TrackSceneNode::update()
{
    // update the position
    setCenter(te->pos);

    // update the radius squared (for culling)
    float radius = 0;
    if (type == TreadsTrack)
        radius = (te->scale * TreadOutside);
    else if (type == PuddleTrack)
        radius = (te->scale * (TreadMiddle + 1.0f));
    else if (type == SmokeTrack)
        radius = (te->scale * TreadOutside);
    setRadius(radius * radius);

    return;
}


// Local Variables: ***
// mode: C++ ***
// tab-width: 4 ***
// c-basic-offset: 4 ***
// indent-tabs-mode: nil ***
// End: ***
// ex: shiftwidth=4 tabstop=4
