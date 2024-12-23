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
#include "RadarRenderer.h"

// common implementation headers
#include "SceneRenderer.h"
#include "MainWindow.h"
#include "OpenGLGState.h"
#include "BZDBCache.h"
#include "TextureManager.h"
#include "PhysicsDriver.h"
#include "ObstacleMgr.h"
#include "MeshSceneNode.h"
#include "ObstacleList.h"
#include "WallObstacle.h"
#include "BoxBuilding.h"
#include "PyramidBuilding.h"
#include "MeshObstacle.h"

// local implementation headers
#include "LocalPlayer.h"
#include "World.h"
#include "FlashClock.h"
#include "ShotPath.h"


static FlashClock flashTank;
static bool toggleTank = false;

const float RadarRenderer::colorFactor = 40.0f;

RadarRenderer::RadarRenderer(const SceneRenderer&, World* _world)
    : world(_world),
      x(0),
      y(0),
      w(0),
      h(0),
      dimming(0.0f),
      ps(),
      range(),
      decay(0.01f),
      smooth(false),
      jammed(false),
      useTankModels(false),
      useTankDimensions(false),
      triangleCount()
{

    setControlColor();
}

void RadarRenderer::setWorld(World* _world)
{
    world = _world;
}


void RadarRenderer::setControlColor(const GLfloat *color)
{
    if (color)
        memcpy(teamColor, color, 3 * sizeof(float));
    else
        memset(teamColor, 0, 3 * sizeof(float));
}


void RadarRenderer::setShape(int _x, int _y, int _w, int _h)
{
    x = _x;
    y = _y;
    w = _w;
    h = _h;
}


void RadarRenderer::setJammed(bool _jammed)
{
    jammed = _jammed;
    decay = 0.01;
}


void RadarRenderer::setDimming(float newDimming)
{
    dimming = (1.0f - newDimming > 1.0f) ? 1.0f : (1.0f - newDimming < 0.0f) ? 0.0f : 1.0f - newDimming;
}


void RadarRenderer::setTankColor(const Player* player)
{
    //The height box also uses the tank color

    const LocalPlayer *myTank = LocalPlayer::getMyTank();

    //my tank
    if (player->getId() == myTank->getId() )
    {
        glColor3f(1.0f, 1.0f, 1.0f);
        return;
    }

    //remote player
    if (player->isPaused() || player->isNotResponding())
    {
        const float dimfactor = 0.4f;

        const float *color;
        if (myTank->getFlag() == Flags::Colorblindness)
            color = Team::getRadarColor(RogueTeam);
        else
            color = Team::getRadarColor(player->getTeam());

        float dimmedcolor[3];
        dimmedcolor[0] = color[0] * dimfactor;
        dimmedcolor[1] = color[1] * dimfactor;
        dimmedcolor[2] = color[2] * dimfactor;
        glColor3fv(dimmedcolor);
    }
    else
    {
        glColor3fv(Team::getRadarColor(myTank->getFlag() ==
                                       Flags::Colorblindness ? RogueTeam : player->getTeam()));
    }
    // If this tank is hunted flash it on the radar
    if (player->isHunted() && myTank->getFlag() != Flags::Colorblindness)
    {
        if (flashTank.isOn())
        {
            if (!toggleTank)
            {
                float flashcolor[3];
                flashcolor[0] = 0.0f;
                flashcolor[1] = 0.8f;
                flashcolor[2] = 0.9f;
                glColor3fv(flashcolor);
            }
        }
        else
        {
            toggleTank = !toggleTank;
            flashTank.setClock(0.2f);
        }
    }
}

void RadarRenderer::drawTank(const float pos[3], const Player* player, bool useSquares)
{
    // 'ps' is pixel scale, setup in render()
    const float tankRadius = BZDBCache::tankRadius;
    float minSize = 1.5f + (ps * BZDBCache::radarTankPixels);
    GLfloat size;
    if (tankRadius < minSize)
        size = minSize;
    else
        size = tankRadius;
    if (pos[2] < 0.0f)
        size = 0.5f;

    // NOTE: myTank was checked in render()
    const float myAngle = LocalPlayer::getMyTank()->getAngle();

    // draw the tank
    if (useSquares || !useTankDimensions)
    {
        setTankColor(player);
        // align to the screen axes
        glRotatef(float(myAngle * 180.0 / M_PI), 0.0f, 0.0f, 1.0f);
        glRectf(-size, -size, +size, +size);
    }
    else
    {
        const float tankAngle = player->getAngle();
        glPushMatrix();
        glRotatef(float(tankAngle * 180.0 / M_PI), 0.0f, 0.0f, 1.0f);
        if (useTankModels)
        {
            drawFancyTank(player);
            setTankColor(player);
        }
        else
        {
            setTankColor(player);
            const float* dims = player->getDimensions();
            glRectf(-dims[0], -dims[1], +dims[0], +dims[1]);
        }
        glPopMatrix();

        // align to the screen axes
        glRotatef(float(myAngle * 180.0 / M_PI), 0.0f, 0.0f, 1.0f);
    }

    // adjust with height box size
    const float boxHeight = BZDB.eval(StateDatabase::BZDB_BOXHEIGHT);
    size = size * (1.0f + (0.5f * (pos[2] / boxHeight)));

    // draw the height box
    glBegin(GL_LINE_STRIP);
    glVertex2f(-size, 0.0f);
    glVertex2f(0.0f, -size);
    glVertex2f(+size, 0.0f);
    glVertex2f(0.0f, +size);
    glVertex2f(-size, 0.0f);
    glEnd();
}


void RadarRenderer::drawFancyTank(const Player* player)
{
    if (smooth)
        glDisable(GL_BLEND);

    // we use the depth buffer so that the treads look ok
    if (BZDBCache::zbuffer)
    {
        glClear(GL_DEPTH_BUFFER_BIT);
        glEnable(GL_DEPTH_TEST);
    }

    OpenGLGState::resetState();
    glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
    RENDERER.enableSun(true);

    player->renderRadar(); // draws at (0,0,0)

    RENDERER.enableSun(false);
    OpenGLGState::resetState();

    if (BZDBCache::zbuffer)
        glDisable(GL_DEPTH_TEST);

    if (smooth)
    {
        glEnable(GL_BLEND);
        glEnable(GL_POINT_SMOOTH);
    }

    return;
}


void RadarRenderer::drawFlag(const float pos[3])
{
    GLfloat s = BZDBCache::flagRadius > 3.0f * ps ? BZDBCache::flagRadius : 3.0f * ps;
    glBegin(GL_LINES);
    glVertex2f(pos[0] - s, pos[1]);
    glVertex2f(pos[0] + s, pos[1]);
    glVertex2f(pos[0] + s, pos[1]);
    glVertex2f(pos[0] - s, pos[1]);
    glVertex2f(pos[0], pos[1] - s);
    glVertex2f(pos[0], pos[1] + s);
    glVertex2f(pos[0], pos[1] + s);
    glVertex2f(pos[0], pos[1] - s);
    glEnd();
}

void RadarRenderer::drawFlagOnTank()
{
    glPushMatrix();

    // align it to the screen axes
    const float angle = LocalPlayer::getMyTank()->getAngle();
    glRotatef(float(angle * 180.0 / M_PI), 0.0f, 0.0f, 1.0f);

    float tankRadius = BZDBCache::tankRadius;
    GLfloat s = 2.5f * tankRadius > 4.0f * ps ? 2.5f * tankRadius : 4.0f * ps;
    glBegin(GL_LINES);
    glVertex2f(-s, 0.0f);
    glVertex2f(+s, 0.0f);
    glVertex2f(+s, 0.0f);
    glVertex2f(-s, 0.0f);
    glVertex2f(0.0f, -s);
    glVertex2f(0.0f, +s);
    glVertex2f(0.0f, +s);
    glVertex2f(0.0f, -s);
    glEnd();

    glPopMatrix();
}


void RadarRenderer::renderFrame(SceneRenderer& renderer)
{
    const MainWindow& window = renderer.getWindow();

    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    window.setProjectionPlay();

    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadIdentity();

    OpenGLGState::resetState();

    const int ox = window.getOriginX();
    const int oy = window.getOriginY();

    glScissor(ox + x - 1, oy + y - 1, w + 2, h + 2);

    const float left = float(ox + x) - 0.5f;
    const float right = float(ox + x + w) + 0.5f;
    const float top = float(oy + y) - 0.5f;
    const float bottom = float(oy + y + h) + 0.5f;

    float outlineOpacity = RENDERER.getRadarOpacity();
    float fudgeFactor = BZDBCache::hudGUIBorderOpacityFactor; // bzdb cache this maybe?
    if ( outlineOpacity < 1.0f )
        outlineOpacity = (outlineOpacity*fudgeFactor) + (1.0f - fudgeFactor);

    if (BZDBCache::blend)
        glEnable(GL_BLEND);

    glColor4f(teamColor[0],teamColor[1],teamColor[2],outlineOpacity);

    glBegin(GL_LINE_LOOP);
    {
        glVertex2f(left, top);
        glVertex2f(right, top);
        glVertex2f(right, bottom);
        glVertex2f(left, bottom);
    }
    glEnd();

    if (BZDBCache::blend)
        glDisable(GL_BLEND);

    glColor4f(teamColor[0],teamColor[1],teamColor[2],1.0f);

    const float opacity = renderer.getRadarOpacity();
    if ((opacity < 1.0f) && (opacity > 0.0f))
    {
        glScissor(ox + x - 2, oy + y - 2, w + 4, h + 4);
        // draw nice blended background
        if (BZDBCache::blend && opacity < 1.0f)
            glEnable(GL_BLEND);
        glColor4f(0.0f, 0.0f, 0.0f, opacity);
        glRectf((float) x, (float) y, (float)(x + w), (float)(y + h));
        if (BZDBCache::blend && opacity < 1.0f)
            glDisable(GL_BLEND);
    }

    // note that this scissor setup is used for the reset of the rendering
    glScissor(ox + x, oy + y, w, h);

    if (opacity == 1.0f)
    {
        glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
    }

    glMatrixMode(GL_PROJECTION);
    glPopMatrix();

    glMatrixMode(GL_MODELVIEW);
    glPopMatrix();

    return;
}


void RadarRenderer::render(SceneRenderer& renderer, bool blank, bool observer)
{
    RenderNode::resetTriangleCount();

    const float radarLimit = BZDBCache::radarLimit;
    if (!BZDB.isTrue("displayRadar") || (radarLimit <= 0.0f))
    {
        triangleCount = 0;
        return;
    }

    glPushAttrib(GL_SCISSOR_BIT);

    // render the frame
    renderFrame(renderer);

    if (blank || !world) {
        glPopAttrib();
        return;
    }

    smooth = BZDBCache::smooth;
    const bool fastRadar = ((BZDBCache::radarStyle == 1) ||
                            (BZDBCache::radarStyle == 2)) && BZDBCache::zbuffer;
    const LocalPlayer *myTank = LocalPlayer::getMyTank();

    // setup the radar range
    float radarRange = BZDB.eval("displayRadarRange") * radarLimit;
    float maxRange = radarLimit;
    // when burrowed, limit radar range
    if (myTank && (myTank->getFlag() == Flags::Burrow) &&
            (myTank->getPosition()[2] < 0.0f))
        maxRange = radarLimit / 4.0f;
    if (radarRange > maxRange)
    {
        radarRange = maxRange;
        // only clamp the user's desired range if it's actually
        // greater then 1. otherwise, we may be resetting it due
        // to burrow radar limiting.
        if (BZDB.eval("displayRadarRange") > 1.0f)
            BZDB.set("displayRadarRange", "1.0");
    }

    // prepare projection matrix
    glMatrixMode(GL_PROJECTION);
    const MainWindow& window = renderer.getWindow();
    // NOTE: the visual extents include passable objects
    double maxHeight = 0.0;
    const Extents* visExts = renderer.getVisualExtents();
    if (visExts)
        maxHeight = (double)visExts->maxs[2];
    window.setProjectionRadar(x, y, w, h, radarRange, (float)(maxHeight + 10.0));

    // prepare modelview matrix
    glMatrixMode(GL_MODELVIEW);

    OpenGLGState::resetState();


    // if jammed then draw white noise.  occasionally draw a good frame.
    if (jammed && (bzfrand() > decay))
    {
        glPushMatrix();
        glLoadIdentity();

        TextureManager &tm = TextureManager::instance();
        int noiseTexture = tm.getTextureID( "noise" );

        glColor3f(1.0f, 1.0f, 1.0f);

        if ((noiseTexture >= 0) && (renderer.useQuality() > 0))
        {

            const int sequences = 10;

            static float np[] =
            {
                0, 0, 1, 1,
                1, 1, 0, 0,
                0.5f, 0.5f, 1.5f, 1.5f,
                1.5f, 1.5f, 0.5f, 0.5f,
                0.25f, 0.25f, 1.25f, 1.25f,
                1.25f, 1.25f, 0.25f, 0.25f,
                0, 0.5f, 1, 1.5f,
                1, 1.5f, 0, 0.5f,
                0.5f, 0, 1.5f, 1,
                1.4f, 1, 0.5f, 0,
                0.75f, 0.75f, 1.75f, 1.75f,
                1.75f, 1.75f, 0.75f, 0.75f,
            };

            int noisePattern = 4 * int(floor(sequences * bzfrand()));

            glEnable(GL_TEXTURE_2D);
            tm.bind(noiseTexture);

            glBegin(GL_TRIANGLE_STRIP);
            {
                glTexCoord2f(np[noisePattern+0],np[noisePattern+1]);
                glVertex2f(-radarRange,-radarRange);
                glTexCoord2f(np[noisePattern+2],np[noisePattern+1]);
                glVertex2f( radarRange,-radarRange);
                glTexCoord2f(np[noisePattern+0],np[noisePattern+3]);
                glVertex2f(-radarRange, radarRange);
                glTexCoord2f(np[noisePattern+2],np[noisePattern+3]);
                glVertex2f( radarRange, radarRange);
            }
            glEnd();

            glDisable(GL_TEXTURE_2D);
        }

        else if ((noiseTexture >= 0) && BZDBCache::texture &&
                 (renderer.useQuality() == 0))
        {
            glEnable(GL_TEXTURE_2D);
            tm.bind(noiseTexture);

            glBegin(GL_TRIANGLE_STRIP);
            {
                glTexCoord2f(0,0);
                glVertex2f(-radarRange,-radarRange);
                glTexCoord2f(1,0);
                glVertex2f( radarRange,-radarRange);
                glTexCoord2f(0,1);
                glVertex2f(-radarRange, radarRange);
                glTexCoord2f(1,1);
                glVertex2f( radarRange, radarRange);
            }
            glEnd();

            glDisable(GL_TEXTURE_2D);
        }
        if (decay > 0.015f) decay *= 0.5f;

        glPopMatrix();
    }

    // only draw if there's a local player and a world
    else if (myTank)
    {
        glPushMatrix();
        glLoadIdentity();

        // if decay is sufficiently small then boost it so it's more
        // likely a jammed radar will get a few good frames closely
        // spaced in time.  value of 1 guarantees at least two good
        // frames in a row.
        if (decay <= 0.015f) decay = 1.0f;
        else decay *= 0.5f;


        // get size of pixel in model space (assumes radar is square)
        ps = 2.0f * (radarRange / GLfloat(w));
        MeshSceneNode::setRadarLodScale(ps);

        float tankWidth = BZDBCache::tankWidth;
        float tankLength = BZDBCache::tankLength;
        const float testMin = 8.0f * ps;
        // maintain the aspect ratio if it isn't square
        if ((tankWidth > testMin) &&  (tankLength > testMin))
            useTankDimensions = true;
        else
            useTankDimensions = false;
        if (useTankDimensions && (renderer.useQuality() >= 2))
            useTankModels = true;
        else
            useTankModels = false;

        // relative to my tank
        const float* myPos = myTank->getPosition();
        const float myAngle = myTank->getAngle();

        // draw the view angle below stuff
        // view frustum edges
        if (!BZDB.isTrue("hideRadarViewLines"))
        {
            glColor3f(1.0f, 0.625f, 0.125f);
            const float fovx = renderer.getViewFrustum().getFOVx();
            const float viewWidth = radarRange * tanf(0.5f * fovx);
            glBegin(GL_LINE_STRIP);
            glVertex2f(-viewWidth, radarRange);
            glVertex2f(0.0f, 0.0f);
            glVertex2f(viewWidth, radarRange);
            glEnd();
        }

        // transform to the observer's viewpoint
        glPushMatrix();
        glRotatef((float)(90.0 - myAngle * 180.0 / M_PI), 0.0f, 0.0f, 1.0f);
        glPushMatrix();
        glTranslatef(-myPos[0], -myPos[1], 0.0f);

        if (useTankModels)
        {
            // new modelview transform requires repositioning
            renderer.setupSun();
        }

        // setup the blending function
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

        // draw the buildings
        renderObstacles(fastRadar, radarRange);

        if (smooth)
        {
            glEnable(GL_BLEND);
            glEnable(GL_LINE_SMOOTH);
            glEnable(GL_POINT_SMOOTH);
        }

        // draw my shots
        int maxShots = world->getMaxShots();
        int i;
        float muzzleHeight = BZDB.eval(StateDatabase::BZDB_MUZZLEHEIGHT);
        for (i = 0; i < maxShots; i++)
        {
            const ShotPath* shot = myTank->getShot(i);
            if (shot)
            {
                const float cs = colorScale(shot->getPosition()[2], muzzleHeight);
                glColor3f(1.0f * cs, 1.0f * cs, 1.0f * cs);
                shot->radarRender();
            }
        }

        //draw world weapon shots
        WorldPlayer *worldWeapons = World::getWorld()->getWorldWeapons();
        maxShots = worldWeapons->getMaxShots();
        for (i = 0; i < maxShots; i++)
        {
            const ShotPath* shot = worldWeapons->getShot(i);
            if (shot)
            {
                const float cs = colorScale(shot->getPosition()[2], muzzleHeight);
                glColor3f(1.0f * cs, 1.0f * cs, 1.0f * cs);
                shot->radarRender();
            }
        }

        // draw other tanks (and any flags on them)
        // note about flag drawing.  each line segment is drawn twice
        // (once in each direction);  this degrades the antialiasing
        // but on systems that don't do correct filtering of endpoints
        // not doing it makes (half) the endpoints jump wildly.
        const int curMaxPlayers = world->getCurMaxPlayers();
        for (i = 0; i < curMaxPlayers; i++)
        {
            RemotePlayer* player = world->getPlayer(i);
            if (!player)
                continue;
            if (!player->isAlive() &&
                    (!useTankModels || !observer || !player->isExploding()))
                continue;
            if ((player->getFlag() == Flags::Stealth) &&
                    (myTank->getFlag() != Flags::Seer))
                continue;

            const float* position = player->getPosition();

            glPushMatrix();

            // transform to the tanks location
            glTranslatef(position[0], position[1], 0.0f);

            if (player->getFlag() != Flags::Null)
            {
                glColor3fv(player->getFlag()->getRadarColor());
                drawFlagOnTank();
            }

            if (!observer)
                drawTank(position, player, true);
            else
                drawTank(position, player, false);

            glPopMatrix();
        }

        bool coloredShot = BZDB.isTrue("coloredradarshots");
        // draw other tanks' shells
        bool iSeeAll = myTank && (myTank->getFlag() == Flags::Seer);
        maxShots = World::getWorld()->getMaxShots();
        for (i = 0; i < curMaxPlayers; i++)
        {
            RemotePlayer* player = world->getPlayer(i);
            if (!player) continue;
            for (int j = 0; j < maxShots; j++)
            {
                const ShotPath* shot = player->getShot(j);
                if (shot && (shot->getFlag() != Flags::InvisibleBullet || iSeeAll))
                {
                    const float cs = colorScale(shot->getPosition()[2], muzzleHeight);
                    if (coloredShot)
                    {
                        const float *shotcolor;
                        if (myTank->getFlag() == Flags::Colorblindness)
                            shotcolor = Team::getRadarColor(RogueTeam);
                        else
                            shotcolor = Team::getRadarColor(player->getTeam());
                        glColor3f(shotcolor[0] * cs, shotcolor[1] * cs, shotcolor[2] * cs);
                    }
                    else
                        glColor3f(cs, cs, cs);
                    shot->radarRender();
                }
            }
        }

        // draw flags not on tanks.
        // draw them in reverse order so that the team flags
        // (which come first), are drawn on top of the normal flags.
        const int maxFlags = world->getMaxFlags();
        const bool drawNormalFlags = BZDB.isTrue("displayRadarFlags");
        const bool hideTeamFlagsOnRadar = BZDB.isTrue(StateDatabase::BZDB_HIDETEAMFLAGSONRADAR);
        const bool hideFlagsOnRadar = BZDB.isTrue(StateDatabase::BZDB_HIDEFLAGSONRADAR);
        for (i = (maxFlags - 1); i >= 0; i--)
        {
            const Flag& flag = world->getFlag(i);
            // don't draw flags that don't exist or are on a tank
            if (flag.status == FlagNoExist || flag.status == FlagOnTank)
                continue;
            // don't draw normal flags if we aren't supposed to
            if (flag.type->flagTeam == NoTeam && !drawNormalFlags)
                continue;
            if (hideTeamFlagsOnRadar)
            {
                if (flag.type->flagTeam != ::NoTeam)
                    continue;
            }
            if (hideFlagsOnRadar)
            {
                if (flag.type)
                    continue;
            }
            // Flags change color by height
            const float cs = colorScale(flag.position[2], muzzleHeight);
            const float *flagcolor = flag.type->getRadarColor();
            glColor3f(flagcolor[0] * cs, flagcolor[1] * cs, flagcolor[2] * cs);
            drawFlag(flag.position);
        }
        // draw antidote flag
        const float* antidotePos =
            LocalPlayer::getMyTank()->getAntidoteLocation();
        if (antidotePos)
        {
            glColor3f(1.0f, 1.0f, 0.0f);
            drawFlag(antidotePos);
        }

        // draw these markers above all others always centered
        glPopMatrix();

        // north marker
        GLfloat ns = 0.05f * radarRange, ny = 0.9f * radarRange;
        glColor3f(1.0f, 1.0f, 1.0f);
        glBegin(GL_LINE_STRIP);
        glVertex2f(-ns, ny - ns);
        glVertex2f(-ns, ny + ns);
        glVertex2f(ns, ny - ns);
        glVertex2f(ns, ny + ns);
        glEnd();

        // always up
        glPopMatrix();

        // forward tick
        glBegin(GL_LINES);
        glVertex2f(0.0f, radarRange - ps);
        glVertex2f(0.0f, radarRange - 4.0f * ps);
        glEnd();

        if (!observer)
        {
            // revert to the centered transformation
            glRotatef((float)(90.0 - myAngle * 180.0 / M_PI), 0.0f, 0.0f, 1.0f);

            glPushMatrix();

            // my flag
            if (myTank->getFlag() != Flags::Null)
            {
                glColor3fv(myTank->getFlag()->getRadarColor());
                drawFlagOnTank();
            }

            // my tank
            drawTank(myPos, myTank, false);

            glPopMatrix();

            // re-setup the blending function
            // (was changed by drawing jump jets)
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        }

        glPopMatrix();

        if (dimming > 0.0f)
        {
            if (!smooth)
                glEnable(GL_BLEND);
            // darken the entire radar if we're dimmed
            // we're drawing positively, so dimming is actually an opacity
            glColor4f(0.0f, 0.0f, 0.0f, 1.0f - dimming);
            glRectf(-radarRange, -radarRange, +radarRange, +radarRange);
        }
        glDisable(GL_BLEND);
        glDisable(GL_LINE_SMOOTH);
        glDisable(GL_POINT_SMOOTH);
    }

    triangleCount = RenderNode::getTriangleCount();

    glPopAttrib();
}


float RadarRenderer::colorScale(const float z, const float h)
{
    float scaleColor;
    if (BZDBCache::radarStyle > 0)
    {
        const LocalPlayer* myTank = LocalPlayer::getMyTank();

        // Scale color so that objects that are close to tank's level are opaque
        const float zTank = myTank->getPosition()[2];

        if (zTank > (z + h))
            scaleColor = 1.0f - (zTank - (z + h)) / colorFactor;
        else if (zTank < z)
            scaleColor = 1.0f - (z - zTank) / colorFactor;
        else
            scaleColor = 1.0f;

        // Don't fade all the way
        if (scaleColor < 0.35f)
            scaleColor = 0.35f;
    }
    else
        scaleColor = 1.0f;

    return scaleColor;
}


float RadarRenderer::transScale(const float z, const float h)
{
    float scaleColor;
    const LocalPlayer* myTank = LocalPlayer::getMyTank();

    // Scale color so that objects that are close to tank's level are opaque
    const float zTank = myTank->getPosition()[2];
    if (zTank > (z + h))
        scaleColor = 1.0f - (zTank - (z + h)) / colorFactor;
    else if (zTank < z)
        scaleColor = 1.0f - (z - zTank) / colorFactor;
    else
        scaleColor = 1.0f;

    if (scaleColor < 0.5f)
        scaleColor = 0.5f;

    return scaleColor;
}


void RadarRenderer::renderObstacles(bool fastRadar, float _range)
{
    if (smooth)
    {
        glEnable(GL_BLEND);
        glEnable(GL_LINE_SMOOTH);
    }

    // draw the walls
    renderWalls();

    // draw the boxes, pyramids, and meshes
    if (!fastRadar)
        renderBoxPyrMesh();
    else
        renderBoxPyrMeshFast(_range);

    // draw the team bases and teleporters
    renderBasesAndTeles();

    if (smooth)
    {
        glDisable(GL_BLEND);
        glDisable(GL_LINE_SMOOTH);
    }

    return;
}


void RadarRenderer::renderWalls()
{
    const ObstacleList& walls = OBSTACLEMGR.getWalls();
    int count = walls.size();
    glColor3f(0.25f, 0.5f, 0.5f);
    glBegin(GL_LINES);
    for (int i = 0; i < count; i++)
    {
        const WallObstacle& wall = *((const WallObstacle*) walls[i]);
        const float wid = wall.getBreadth();
        const float c   = wid * cosf(wall.getRotation());
        const float s   = wid * sinf(wall.getRotation());
        const float* pos = wall.getPosition();
        glVertex2f(pos[0] - s, pos[1] + c);
        glVertex2f(pos[0] + s, pos[1] - c);
    }
    glEnd();

    return;
}


void RadarRenderer::renderBoxPyrMeshFast(float _range)
{
    // FIXME - This is hack code at the moment, but even when
    //     rendering the full world, it draws the aztec map
    //     3X faster (the culling algo is actually slows us
    //     down in that case)
    //       - need a better default gradient texture
    //         (better colors, and tied in to show max jump height?)
    //       - build a procedural texture if default is missing
    //       - use a GL_TEXTURE_1D
    //       - setup the octree to return Z sorted elements (partially done)
    //       - add a renderClass() member to SceneNode (also for coloring)
    //       - also add a renderShadow() member (they don't need sorting,
    //         and if you don't have double-buffering, you shouldn't be
    //         using shadows)
    //       - vertex shaders would be faster
    //       - it would probably be a better approach to attach a radar
    //         rendering object to each obstacle... no time

    // get the texture
    int gradientTexId = -1;
    TextureManager &tm = TextureManager::instance();
    gradientTexId = tm.getTextureID("radar", false);

    // safety: no texture, no service
    if (gradientTexId < 0)
    {
        renderBoxPyrMesh();
        return;
    }

    // GL state
    OpenGLGStateBuilder gb;
    gb.setTexture(gradientTexId);
    gb.setShading(GL_FLAT);
    gb.setCulling(GL_BACK);
    gb.setBlending(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    OpenGLGState gs = gb.getState();
    gs.setState();

    // disable the unused arrays
    glDisableClientState(GL_NORMAL_ARRAY);
    glDisableClientState(GL_TEXTURE_COORD_ARRAY);

    // now that the texture is bound, setup the clamp mode
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);

    // do this after the GState setting
    if (smooth)
        glEnable(GL_POLYGON_SMOOTH);

    // setup the texturing mapping
    const float hf = 128.0f; // height factor, goes from 0.0 to 1.0 in texcoords
    const float vfz = RENDERER.getViewFrustum().getEye()[2];
    const GLfloat plane[4] =
    { 0.0f, 0.0f, (1.0f / hf), (((hf * 0.5f) - vfz) / hf) };
    glTexGeni(GL_S, GL_TEXTURE_GEN_MODE, GL_EYE_LINEAR);
    glTexGenfv(GL_S, GL_EYE_PLANE, plane);

    // setup texture generation
    glEnable(GL_TEXTURE_GEN_S);

    // set the color
    glColor4f(1.0f, 1.0f, 1.0f, 1.0f);

//  if (!BZDB.isTrue("visualRadar")) {
    ViewFrustum radarClipper;
    radarClipper.setOrthoPlanes(RENDERER.getViewFrustum(), _range, _range);
    RENDERER.getSceneDatabase()->renderRadarNodes(radarClipper);
//  } else {
//    RENDERER.getSceneDatabase()->renderRadarNodes(RENDERER.getViewFrustum());
//  }

    // restore texture generation
    glDisable(GL_TEXTURE_GEN_S);

    OpenGLGState::resetState();

    // re-enable the arrays
    glEnableClientState(GL_NORMAL_ARRAY);
    glEnableClientState(GL_TEXTURE_COORD_ARRAY);

    // do this after the GState setting
    if (smooth)
    {
        glEnable(GL_BLEND);
        glEnable(GL_LINE_SMOOTH);
        glDisable(GL_POLYGON_SMOOTH);
    }

    return;
}


void RadarRenderer::renderBoxPyrMesh()
{
    int i;

    const bool enhanced = (BZDBCache::radarStyle > 0);

    if (!smooth)
    {
        // smoothing has blending disabled
        if (enhanced)
        {
            glEnable(GL_BLEND); // always blend the polygons if we're enhanced
        }
    }
    else
    {
        // smoothing has blending enabled
        if (!enhanced)
        {
            glDisable(GL_BLEND); // don't blend the polygons if we're not enhanced
        }
    }

    // draw box buildings.
    const ObstacleList& boxes = OBSTACLEMGR.getBoxes();
    int count = boxes.size();
    for (i = 0; i < count; i++)
    {
        const BoxBuilding& box = *((const BoxBuilding*) boxes[i]);
        if (box.isInvisible())
            continue;
        const float z = box.getPosition()[2];
        const float bh = box.getHeight();
        const float cs = colorScale(z, bh);
        glColor4f(0.25f * cs, 0.5f * cs, 0.5f * cs, transScale(z, bh));
        const float c = cosf(box.getRotation());
        const float s = sinf(box.getRotation());
        const float wx = c * box.getWidth(), wy = s * box.getWidth();
        const float hx = -s * box.getBreadth(), hy = c * box.getBreadth();
        const float* pos = box.getPosition();
        glBegin(GL_TRIANGLE_STRIP);
        glVertex2f(pos[0] - wx - hx, pos[1] - wy - hy);
        glVertex2f(pos[0] + wx - hx, pos[1] + wy - hy);
        glVertex2f(pos[0] - wx + hx, pos[1] - wy + hy);
        glVertex2f(pos[0] + wx + hx, pos[1] + wy + hy);
        glEnd();
    }

    // draw pyramid buildings
    const ObstacleList& pyramids = OBSTACLEMGR.getPyrs();
    count = pyramids.size();
    for (i = 0; i < count; i++)
    {
        const PyramidBuilding& pyr = *((const PyramidBuilding*) pyramids[i]);
        const float z = pyr.getPosition()[2];
        const float bh = pyr.getHeight();
        const float cs = colorScale(z, bh);
        glColor4f(0.25f * cs, 0.5f * cs, 0.5f * cs, transScale(z, bh));
        const float c = cosf(pyr.getRotation());
        const float s = sinf(pyr.getRotation());
        const float wx = c * pyr.getWidth(), wy = s * pyr.getWidth();
        const float hx = -s * pyr.getBreadth(), hy = c * pyr.getBreadth();
        const float* pos = pyr.getPosition();
        glBegin(GL_TRIANGLE_STRIP);
        glVertex2f(pos[0] - wx - hx, pos[1] - wy - hy);
        glVertex2f(pos[0] + wx - hx, pos[1] + wy - hy);
        glVertex2f(pos[0] - wx + hx, pos[1] - wy + hy);
        glVertex2f(pos[0] + wx + hx, pos[1] + wy + hy);
        glEnd();
    }

    // draw mesh obstacles
    if (smooth)
        glEnable(GL_POLYGON_SMOOTH);
    if (!enhanced)
        glDisable(GL_CULL_FACE);
    const ObstacleList& meshes = OBSTACLEMGR.getMeshes();
    count = meshes.size();
    for (i = 0; i < count; i++)
    {
        const MeshObstacle* mesh = (const MeshObstacle*) meshes[i];
        int faces = mesh->getFaceCount();

        for (int f = 0; f < faces; f++)
        {
            const MeshFace* face = mesh->getFace(f);
            if (enhanced)
            {
                if (face->getPlane()[2] <= 0.0f)
                    continue;
                const BzMaterial* bzmat = face->getMaterial();
                if ((bzmat != NULL) && bzmat->getNoRadar())
                    continue;
            }
            float z = face->getPosition()[2];
            float bh = face->getSize()[2];

            if (BZDBCache::useMeshForRadar)
            {
                z = mesh->getPosition()[2];
                bh = mesh->getSize()[2];
            }

            const float cs = colorScale(z, bh);
            // draw death faces with a soupcon of red
            const PhysicsDriver* phydrv = PHYDRVMGR.getDriver(face->getPhysicsDriver());
            if ((phydrv != NULL) && phydrv->getIsDeath())
                glColor4f(0.75f * cs, 0.25f * cs, 0.25f * cs, transScale(z, bh));
            else
                glColor4f(0.25f * cs, 0.5f * cs, 0.5f * cs, transScale(z, bh));
            // draw the face as a triangle fan
            int vertexCount = face->getVertexCount();
            glBegin(GL_TRIANGLE_FAN);
            for (int v = 0; v < vertexCount; v++)
            {
                const float* pos = face->getVertex(v);
                glVertex2f(pos[0], pos[1]);
            }
            glEnd();
        }
    }
    if (!enhanced)
        glEnable(GL_CULL_FACE);
    if (smooth)
        glDisable(GL_POLYGON_SMOOTH);

    // NOTE: revert from the enhanced setting
    if (enhanced && !smooth)
        glDisable(GL_BLEND);

    // now draw antialiased outlines around the polygons
    if (smooth)
    {
        glEnable(GL_BLEND); // NOTE: revert from the enhanced setting
        count = boxes.size();
        for (i = 0; i < count; i++)
        {
            const BoxBuilding& box = *((const BoxBuilding*) boxes[i]);
            if (box.isInvisible())
                continue;
            const float z = box.getPosition()[2];
            const float bh = box.getHeight();
            const float cs = colorScale(z, bh);
            glColor4f(0.25f * cs, 0.5f * cs, 0.5f * cs, transScale(z, bh));
            const float c = cosf(box.getRotation());
            const float s = sinf(box.getRotation());
            const float wx = c * box.getWidth(), wy = s * box.getWidth();
            const float hx = -s * box.getBreadth(), hy = c * box.getBreadth();
            const float* pos = box.getPosition();
            glBegin(GL_LINE_LOOP);
            glVertex2f(pos[0] - wx - hx, pos[1] - wy - hy);
            glVertex2f(pos[0] + wx - hx, pos[1] + wy - hy);
            glVertex2f(pos[0] + wx + hx, pos[1] + wy + hy);
            glVertex2f(pos[0] - wx + hx, pos[1] - wy + hy);
            glEnd();
        }

        count = pyramids.size();
        for (i = 0; i < count; i++)
        {
            const PyramidBuilding& pyr = *((const PyramidBuilding*) pyramids[i]);
            const float z = pyr.getPosition()[2];
            const float bh = pyr.getHeight();
            const float cs = colorScale(z, bh);
            glColor4f(0.25f * cs, 0.5f * cs, 0.5f * cs, transScale(z, bh));
            const float c = cosf(pyr.getRotation());
            const float s = sinf(pyr.getRotation());
            const float wx = c * pyr.getWidth(), wy = s * pyr.getWidth();
            const float hx = -s * pyr.getBreadth(), hy = c * pyr.getBreadth();
            const float* pos = pyr.getPosition();
            glBegin(GL_LINE_LOOP);
            glVertex2f(pos[0] - wx - hx, pos[1] - wy - hy);
            glVertex2f(pos[0] + wx - hx, pos[1] + wy - hy);
            glVertex2f(pos[0] + wx + hx, pos[1] + wy + hy);
            glVertex2f(pos[0] - wx + hx, pos[1] - wy + hy);
            glEnd();
        }
    }

    return;
}


void RadarRenderer::renderBasesAndTeles()
{
    int i;

    // draw team bases
    if (world->allowTeamFlags())
    {
        for (i = 1; i < NumTeams; i++)
        {
            for (int j = 0;; j++)
            {
                const float *base = world->getBase(i, j);
                if (base == NULL)
                    break;
                glColor3fv(Team::getRadarColor(TeamColor(i)));
                glBegin(GL_LINE_LOOP);
                const float beta = atan2f(base[5], base[4]);
                const float r = hypotf(base[4], base[5]);
                glVertex2f(base[0] + r * cosf(base[3] + beta),
                           base[1] + r * sinf(base[3] + beta));
                glVertex2f(base[0] + r * cosf((float)(base[3] - beta + M_PI)),
                           base[1] + r * sinf((float)(base[3] - beta + M_PI)));
                glVertex2f(base[0] + r * cosf((float)(base[3] + beta + M_PI)),
                           base[1] + r * sinf((float)(base[3] + beta + M_PI)));
                glVertex2f(base[0] + r * cosf(base[3] - beta),
                           base[1] + r * sinf(base[3] - beta));
                glEnd();
            }
        }
    }

    // draw teleporters.  teleporters are pretty thin so use lines
    // (which, if longer than a pixel, are guaranteed to draw something;
    // not so for a polygon).  just in case the system doesn't correctly
    // filter the ends of line segments, we'll draw the line in each
    // direction (which degrades the antialiasing).  Newport graphics
    // is one system that doesn't do correct filtering.
    const ObstacleList& teleporters = OBSTACLEMGR.getTeles();
    int count = teleporters.size();
    glBegin(GL_LINES);
    for (i = 0; i < count; i++)
    {
        const Teleporter & tele = *((const Teleporter *) teleporters[i]);
        if (tele.isHorizontal ())
        {
            const float z = tele.getPosition ()[2];
            const float bh = tele.getHeight ();
            const float cs = colorScale (z, bh);
            glColor4f (1.0f * cs, 1.0f * cs, 0.25f * cs, transScale (z, bh));
            const float c = cosf (tele.getRotation ());
            const float s = sinf (tele.getRotation ());
            const float wx = c * tele.getWidth (), wy = s * tele.getWidth ();
            const float hx = -s * tele.getBreadth (), hy = c * tele.getBreadth ();
            const float *pos = tele.getPosition ();
            glVertex2f (pos[0] - wx - hx, pos[1] - wy - hy);
            glVertex2f (pos[0] + wx - hx, pos[1] + wy - hy);

            glVertex2f (pos[0] + wx - hx, pos[1] + wy - hy);
            glVertex2f (pos[0] + wx + hx, pos[1] + wy + hy);

            glVertex2f (pos[0] + wx + hx, pos[1] + wy + hy);
            glVertex2f (pos[0] - wx + hx, pos[1] - wy + hy);

            glVertex2f (pos[0] - wx + hx, pos[1] - wy + hy);
            glVertex2f (pos[0] - wx - hx, pos[1] - wy - hy);

            glVertex2f (pos[0] - wx - hx, pos[1] - wy - hy);
            glVertex2f (pos[0] - wx - hx, pos[1] - wy - hy);
        }
        else
        {
            const float z = tele.getPosition ()[2];
            const float bh = tele.getHeight ();
            const float cs = colorScale (z, bh);
            glColor4f (1.0f * cs, 1.0f * cs, 0.25f * cs, transScale (z, bh));
            const float tw = tele.getBreadth ();
            const float c = tw * cosf (tele.getRotation ());
            const float s = tw * sinf (tele.getRotation ());
            const float *pos = tele.getPosition ();
            glVertex2f (pos[0] - s, pos[1] + c);
            glVertex2f (pos[0] + s, pos[1] - c);
            glVertex2f (pos[0] + s, pos[1] - c);
            glVertex2f (pos[0] - s, pos[1] + c);
        }
    }
    glEnd();

    return;
}


int RadarRenderer::getFrameTriangleCount() const
{
    return triangleCount;
}


// Local Variables: ***
// mode: C++ ***
// tab-width: 4 ***
// c-basic-offset: 4 ***
// indent-tabs-mode: nil ***
// End: ***
// ex: shiftwidth=4 tabstop=4
