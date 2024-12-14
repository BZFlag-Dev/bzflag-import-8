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

/* FIXME -- ugh.. the class header for this file is listed as a public header
 * and is used by other libs, yet this cxx is here.. bad bad bad.  need to
 * decouple this file from the bzflag front-end specific sources so that it
 * may be moved elsewhere.
 */

// interface header
#include "SceneRenderer.h"

// System headers
#include <glm/gtc/type_ptr.hpp>

/* common implementation headers */
#include "SceneDatabase.h"
#include "MainWindow.h"
#include "DynamicColor.h"
#include "TextureMatrix.h"
#include "TankSceneNode.h"
#include "StateDatabase.h"
#include "TextUtils.h"
#include "ParseColor.h"
#include "BZDBCache.h"
#include "MeshSceneNode.h"
#include "OpenGLAPI.h"

/* FIXME - local implementation dependancies */
#include "BackgroundRenderer.h"
#include "LocalPlayer.h"
#include "daylight.h"
#include "World.h"
#include "TrackMarks.h"


static bool mapFog;
static bool setupMapFog();


//
// FlareLight
//

FlareLight::FlareLight(const float* _pos, const float* _color)
{
    pos[0] = _pos[0];
    pos[1] = _pos[1];
    pos[2] = _pos[2];
    color[0] = _color[0];
    color[1] = _color[1];
    color[2] = _color[2];
}

FlareLight::~FlareLight()
{
    // do nothing
}

//
// SceneRenderer
//

const GLint   SceneRenderer::SunLight = 0;      // also for the moon
const float   SceneRenderer::dimDensity = 0.75f;
const GLfloat SceneRenderer::dimnessColor[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
const GLfloat SceneRenderer::blindnessColor[4] = { 1.0f, 1.0f, 0.0f, 1.0f };

SceneRenderer::SceneRenderer() :
    window(NULL),
    blank(false),
    invert(false),
    sunBrightness(1.0f),
    scene(NULL),
    background(NULL),
    useQualityValue(2),
    useDepthComplexityOn(false),
    useWireframeOn(false),
    useHiddenLineOn(false),
    panelOpacity(0.3f),
    radarOpacity(0.3f),
    radarSize(8),
    panelHeight(4),
    maxMotionFactor(5),
    viewType(Normal),
    inOrder(false),
    depthRange(0),
    numDepthRanges(1),
    depthRangeSize(1.0),
    useDimming(false),
    canUseHiddenLine(false),
    exposed(true),
    lastFrame(true),
    sameFrame(false),
    needStyleUpdate(true),
    rebuildTanks(true),
    fogActive(false)
{
    lightsSize = 4;
    lights = new OpenGLLight*[lightsSize];
    lightsCount = 0;
    dynamicLights = 0;

    // init the track mark manager
    TrackMarks::init();

    // add callbacks for radar size and panel height
    BZDB.addCallback("radarsize", bzdbCallback, NULL);
    BZDB.addCallback("panelheight", bzdbCallback, NULL);

    return;
}


void SceneRenderer::setWindow(MainWindow* _window)
{
    window = _window;

    // get visual info
    window->getWindow()->makeCurrent();
    GLint bits;
    if (!BZDB.isSet("zbuffer"))
    {
//  glGetIntegerv(GL_DEPTH_BITS, &bits);
        BZDB.set("zbuffer", "1");
    }
    glGetIntegerv(GL_STENCIL_BITS, &bits);
    useStencilOn = (bits > 0);

    // can only do hidden line if polygon offset is available
    canUseHiddenLine = true;

    // prepare context with stuff that'll never change
    glLightModeli(GL_LIGHT_MODEL_TWO_SIDE, GL_FALSE);
    glGetIntegerv(GL_MAX_LIGHTS, &maxLights);
    reservedLights = 1;           // only one light between sun and moon
    maxLights -= reservedLights;      // can't use the reserved lights

    // prepare sun
    setTimeOfDay(unixEpoch);

    // force nodes to update their styles
    notifyStyleChange();
}


SceneRenderer::~SceneRenderer()
{
    // free database
    delete scene;

    // free lights list
    delete[] lights;

    // kill the track manager
    TrackMarks::kill();
}


bool SceneRenderer::useStencil() const
{
    return useStencilOn;
}


SceneRenderer::ViewType SceneRenderer::getViewType() const
{
    return viewType;
}


void SceneRenderer::setZBufferSplit(bool on)
{
    if (!on)
    {
        if (numDepthRanges != 1)
        {
            numDepthRanges = 1;
            depthRangeSize = 1.0;
            glDepthRange(0.0, 1.0);
        }
    }
    else
    {
        GLint bits;
        glGetIntegerv(GL_DEPTH_BITS, &bits);
        if (bits > 18)
        {
            // number of independent slices to split depth buffer into
            numDepthRanges = 1 << (bits - 18);

            // size of a single range
            depthRangeSize = 1.0 / (double)numDepthRanges;
        }
        else
        {
            numDepthRanges = 1;
            depthRangeSize = 1.0;
        }
    }
}


void SceneRenderer::setQuality(int value)
{
    // 0 = Low
    // 1 = Medium
    // 2 = High
    // 3 = Experimental

    if (value < 0)
        value = 0;
    else if (value > BZDB.evalInt("maxQuality"))
        value = BZDB.evalInt("maxQuality");
    if (useQualityValue != value)
        rebuildTanks = true;
    useQualityValue = value;

    notifyStyleChange();

    if (useQualityValue >= 1)
    {
        glHint(GL_LINE_SMOOTH_HINT, GL_NICEST);
        glHint(GL_POINT_SMOOTH_HINT, GL_NICEST);
        // GL_NICEST for polygon smoothing seems to make some drivers
        // cause massive slowdowns and "spikes" when drawing the radar
        glHint(GL_POLYGON_SMOOTH_HINT, GL_FASTEST);
    }
    else
    {
        glHint(GL_LINE_SMOOTH_HINT, GL_FASTEST);
        glHint(GL_POINT_SMOOTH_HINT, GL_FASTEST);
        glHint(GL_POLYGON_SMOOTH_HINT, GL_FASTEST);
    }

    if (useQualityValue >= 2)
        TankSceneNode::setMaxLOD(-1);
    else if (useQualityValue >= 1)
        TankSceneNode::setMaxLOD(3);
    else
        TankSceneNode::setMaxLOD(2);

    if (useQualityValue >= 2)
        BZDB.set("flagChunks","32");
    else if (useQualityValue >= 1)
        BZDB.set("flagChunks","12");
    else
        BZDB.set("flagChunks","8");

    if (useQualityValue >= 2)
        BZDB.set("moonSegments","64");
    else if (useQualityValue >= 1)
        BZDB.set("moonSegments","24");
    else
        BZDB.set("moonSegments","12");

    if (useQualityValue > 0)
    {
        // this can be modified by OpenGLMaterial
        glLightModeli(GL_LIGHT_MODEL_LOCAL_VIEWER, GL_TRUE);
    }
    else
    {
        // OpenGLMaterial will not modify if (quality <= 0)
        glLightModeli(GL_LIGHT_MODEL_LOCAL_VIEWER, GL_FALSE);
    }

    // this setting helps keep those specular highlights
    // highlighting when applied to a dark textured surface.
    // It was mainlined in OpenGL Version 1.2
    // (there's also the GL_EXT_separate_specular_color extension)
#ifdef GL_LIGHT_MODEL_COLOR_CONTROL
    if (useQualityValue >= 1)
        glLightModeli(GL_LIGHT_MODEL_COLOR_CONTROL, GL_SEPARATE_SPECULAR_COLOR);
    else
        glLightModeli(GL_LIGHT_MODEL_COLOR_CONTROL, GL_SINGLE_COLOR);
#  else // in case someone includes <GL/glext.h> at some point
#  ifdef GL_LIGHT_MODEL_COLOR_CONTROL_EXT
    if (useQualityValue >= 1)
        glLightModeli(GL_LIGHT_MODEL_COLOR_CONTROL_EXT,
                      GL_SEPARATE_SPECULAR_COLOR_EXT);
    else
        glLightModeli(GL_LIGHT_MODEL_COLOR_CONTROL_EXT,
                      GL_SINGLE_COLOR_EXT);
#  endif
#endif

    BZDB.set("useQuality", TextUtils::format("%d", value));
}


bool SceneRenderer::useDepthComplexity() const
{
    return useDepthComplexityOn;
}


void SceneRenderer::setDepthComplexity(bool on)
{
    if (on)
    {
        GLint bits;
        glGetIntegerv(GL_STENCIL_BITS, &bits);
        if (bits < 3) return;
    }
    useDepthComplexityOn = on;
}


void SceneRenderer::setRebuildTanks()
{
    rebuildTanks = true;
}


void SceneRenderer::bzdbCallback(const std::string& name, void *)
{
    if(name == "radarsize")
        RENDERER.setRadarSize(BZDB.evalInt("radarsize"));
    else if(name == "panelheight")
        RENDERER.setPanelHeight(BZDB.evalInt("panelheight"));
}


void SceneRenderer::setupBackgroundMaterials()
{
    if (background)
    {
        background->setupSkybox();
        background->setupGroundMaterials();
    }
    return;
}


void SceneRenderer::setWireframe(bool on)
{
    useWireframeOn = on;
}


bool SceneRenderer::useWireframe() const
{
    return useWireframeOn;
}


void SceneRenderer::setHiddenLine(bool on)
{
    useHiddenLineOn = on && BZDBCache::zbuffer && canUseHiddenLine;
    if (!useHiddenLineOn)
    {
        depthRange = 0;
        return;
    }
    glPolygonOffset(1.0f, 2.0f);
}


bool SceneRenderer::useHiddenLine() const
{
    return useHiddenLineOn;
}


void SceneRenderer::setPanelOpacity(float opacity)
{
    bool needtoresize = opacity == 1.0f || panelOpacity == 1.0f;

    panelOpacity = opacity;
    notifyStyleChange();
    if (needtoresize)
    {
        if (window)
        {
            window->setFullView(panelOpacity < 1.0f);
            window->getWindow()->callResizeCallbacks();
        }
    }
}


void SceneRenderer::setRadarOpacity(float opacity)
{
    radarOpacity = opacity;
}


float SceneRenderer::getPanelOpacity() const
{
    return panelOpacity;
}


float SceneRenderer::getRadarOpacity() const
{
    return radarOpacity;
}


void SceneRenderer::setRadarSize(int size)
{
    radarSize = size;
    notifyStyleChange();
    if (window)
        window->getWindow()->callResizeCallbacks();
}


void SceneRenderer::setPanelHeight(int size)
{
    panelHeight = size;
    notifyStyleChange();
    if (window)
        window->getWindow()->callResizeCallbacks();
}


int SceneRenderer::getRadarSize() const
{
    return radarSize;
}


int SceneRenderer::getPanelHeight() const
{
    return panelHeight;
}


void SceneRenderer::setMaxMotionFactor(int factor)
{
    if (factor < -11)
        factor = -11;
    maxMotionFactor = factor;
    notifyStyleChange();
    if (window)
        window->getWindow()->callResizeCallbacks();
}


int SceneRenderer::getMaxMotionFactor() const
{
    return maxMotionFactor;
}


void SceneRenderer::setDim(bool on)
{
    useDimming = on;
}


void SceneRenderer::setViewType(ViewType _viewType)
{
    viewType = _viewType;
}


void SceneRenderer::setExposed()
{
    exposed = true;
}


void SceneRenderer::clearRadar(float opacity)
{
    int size = window->getHeight() - window->getViewHeight();
    float op = (opacity > 1.0f) ? 1.0f : (opacity < 0.0f) ? 0.0f : opacity;
    glScissor(window->getOriginX(), 0, size, size);
    glClearColor(0.0f, 0.0f, 0.0f, op);
    glClear(GL_COLOR_BUFFER_BIT);
}


void SceneRenderer::setSceneDatabase(SceneDatabase* db)
{
    // update the styles
    needStyleUpdate = true;

    // free the current database
    delete scene;

    scene = db;
    if (scene)
        inOrder = scene->isOrdered();
    else
        inOrder = false;

    // update the background materials
    setupBackgroundMaterials();

    return;
}


void SceneRenderer::setBackground(BackgroundRenderer* br)
{
    background = br;
}


void SceneRenderer::getGroundUV(const glm::vec2 &p, glm::vec2 &uv) const
{
    float repeat = 0.01f;
    if (BZDB.isSet("groundTexRepeat"))
        repeat = BZDB.eval("groundTexRepeat");

    if (useQualityValue >= 3)
        repeat = BZDB.eval("groundHighResTexRepeat");

    uv = repeat * p;
}


void SceneRenderer::enableLight(int index, bool on)
{
    OpenGLLight::enableLight(index + reservedLights, on);
}


void SceneRenderer::enableSun(bool on)
{
    if (BZDBCache::lighting && sunOrMoonUp)
        theSun.enableLight(SunLight, on);
}


void SceneRenderer::setupSun()
{
    if (BZDBCache::lighting && sunOrMoonUp)
        theSun.execute(SunLight, BZDB.isTrue("lightLists"));
}


void SceneRenderer::addLight(OpenGLLight& light)
{
    // add a light, and grow the maximum list size if required
    lightsCount++;
    if (lightsCount > lightsSize)
    {
        OpenGLLight** newList = new OpenGLLight*[lightsSize * 2];
        memcpy (newList, lights, lightsSize * sizeof(OpenGLLight*));
        delete[] lights;
        lights = newList;
        lightsSize = lightsSize * 2;
    }
    lights[lightsCount - 1] = &light;
    return;
}


void SceneRenderer::addFlareLight(const float* pos, const float* color)
{
    flareLightList.push_back(FlareLight(pos, color));
}


int SceneRenderer::getNumLights() const
{
    return dynamicLights;
}


int SceneRenderer::getNumAllLights() const
{
    return lightsCount;
}


void SceneRenderer::clearLights()
{
    lightsCount = 0;
    dynamicLights = 0;
    return;
}


void SceneRenderer::setTimeOfDay(double julianDay)
{

    // get position of sun and moon at 0,0 lat/long
    glm::vec3 sunDir, moonDir;
    float latitude, longitude;
    if (!BZDB.isTrue(StateDatabase::BZDB_SYNCLOCATION))
    {
        // use local (client) settings
        latitude = BZDB.eval("latitude");
        longitude = BZDB.eval("longitude");
    }
    else
    {
        // server settings
        latitude = BZDB.eval(StateDatabase::BZDB_LATITUDE);
        longitude = BZDB.eval(StateDatabase::BZDB_LONGITUDE);
    }

    getSunPosition(julianDay, latitude, longitude, sunDir);
    getMoonPosition(julianDay, latitude, longitude, moonDir);
    ::getCelestialTransform(julianDay, latitude, longitude, celestialTransform);

    // set sun position
    if (sunDir[2] >= -0.009f)
    {
        // sun is our source of illumination
        sunOrMoonUp = true;
        theSun.setDirection(sunDir);
    }
    else if (moonDir[2] > -0.009f)
    {
        // moon is our source of illumination
        sunOrMoonUp = true;
        theSun.setDirection(moonDir);
    }
    else
    {
        // `the moon is down' (well, so is the sun, but that's not Shakespeare)
        // -- ambient only
        sunOrMoonUp = false;
    }

    // set sun and ambient colors
    glm::vec3 ambientColor;
    ::getSunColor(sunDir, sunColor, ambientColor, sunBrightness);
    theSun.setColor(sunColor);
    GLfloat maxComponent = sunColor[0];
    if (sunColor[1] > maxComponent) maxComponent = sunColor[1];
    if (sunColor[2] > maxComponent) maxComponent = sunColor[2];
    if (maxComponent <= 0.0f) maxComponent = 1.0f;
    sunScaledColor = sunColor / maxComponent;
    const auto color = glm::vec4(ambientColor, 1.0f);
    glLightModelfv(GL_LIGHT_MODEL_AMBIENT, glm::value_ptr(color));

    if (background)
        background->setCelestial(*this, sunDir, moonDir);
}


static int sortLights (const void* a, const void* b)
{
    // the higher getImportance(), the closer it is to the beginning
    const OpenGLLight* lightA = *((const OpenGLLight* const *) a);
    const OpenGLLight* lightB = *((const OpenGLLight* const *) b);
    const float valA = lightA->getImportance();
    const float valB = lightB->getImportance();

    // first sort by cull
    if (valA < 0.0f)
    {
        if (valB >= 0.0f)
            return +1;
        else
            return 0;
    }
    if (valB < 0.0f)
    {
        if (valA >= 0.0f)
            return -1;
        else
            return 0;
    }

    // sort by grounded state
    const bool groundedA = lightA->getOnlyGround();
    const bool groundedB = lightB->getOnlyGround();
    if (groundedA && !groundedB)
        return +1;
    if (!groundedA && groundedB)
        return -1;

    // sort by importance
    if (valA > valB)
        return -1;
    else
        return +1;
}


void SceneRenderer::render(bool _lastFrame, bool _sameFrame,
                           bool fullWindow)
{
    lastFrame = _lastFrame;
    sameFrame = _sameFrame;

    triangleCount = 0;
    RenderNode::resetTriangleCount();
    if (background)
        background->resetTriangleCount();

    // update the SceneNode, Background, and TrackMark styles
    if (needStyleUpdate)
    {
        if (scene)
            scene->updateNodeStyles();
        if (background)
            background->notifyStyleChange();
        TrackMarks::notifyStyleChange();
        needStyleUpdate = false;
    }

    if (rebuildTanks)
    {
        TankGeometryMgr::deleteLists();
        TankGeometryMgr::buildLists();
        rebuildTanks = false;
    }

    // update the dynamic colors
    DYNCOLORMGR.update();

    // update the texture matrices
    TEXMATRIXMGR.update();

    // make sure there is something to render on
    if (!window)
        return;

    // setup the viewport LOD scale
    MeshSceneNode::setLodScale(window->getWidth(), frustum.getFOVx(),
                               window->getViewHeight(), frustum.getFOVy());
    {
        const int pixelsX = window->getWidth();
        const int pixelsY = window->getViewHeight();
        const float fovx = frustum.getFOVx();
        const float fovy = frustum.getFOVy();
        const float lppx = 2.0f * sinf(fovx * 0.5f) / (float)pixelsX;
        const float lppy = 2.0f * sinf(fovy * 0.5f) / (float)pixelsY;
        const float lpp = (lppx < lppy) ? lppx : lppy;
        lengthPerPixel = lpp * BZDB.eval("lodScale");
    }

    // get the track mark sceneNodes (only for BSP)
    if (scene)
        TrackMarks::addSceneNodes(scene);

    // turn on fog for teleporter blindness if close to a teleporter
    teleporterProximity = 0.0f;
    if (!blank && LocalPlayer::getMyTank() &&
            (LocalPlayer::getMyTank()->getTeam() != ObserverTeam))
        teleporterProximity = LocalPlayer::getMyTank()->getTeleporterProximity();

    // fog setup
    mapFog = setupMapFog();

    mirror = (BZDB.get(StateDatabase::BZDB_MIRROR) != "none")
             && BZDB.isTrue("userMirror");

    clearZbuffer = true;
    drawGround = true;

    if (mirror)
    {
        drawGround = false;

        // flip for the reflection drawing
        frustum.flipVertical();
        glFrontFace(GL_CW);

        // different occluders for the mirror
        if (scene)
            scene->setOccluderManager(1);

        // the reflected scene
        renderScene(_lastFrame, _sameFrame, fullWindow);

        // different occluders for the mirror
        if (scene)
            scene->setOccluderManager(0);

        // flip back
        frustum.flipVertical();
        glFrontFace(GL_CCW);

        glm::vec4 mirrorColor;
        if (!parseColorString(BZDB.get(StateDatabase::BZDB_MIRROR), mirrorColor))
            mirrorColor = glm::vec4(0.0f, 0.0f, 0.0f, 0.5f);
        else if (mirrorColor[3] == 1.0f)
        {
            // probably a mistake
            mirrorColor[3] = 0.5f;
        }
        if (invert)
        {
            mirrorColor[0] = 1.0f - mirrorColor[0];
            mirrorColor[2] = 1.0f - mirrorColor[2];
            mirrorColor[3] = 0.2f;
        }

        float extent = 1.0f;
        // darken the reflection
        if (!mapFog)
        {
            glMatrixMode(GL_PROJECTION);
            glLoadIdentity();
            glMatrixMode(GL_MODELVIEW);
            glLoadIdentity();
        }
        else
        {
            // need the proper matrices for fog generation
            // if low quality then use stipple -- it's probably much faster
            frustum.executeView();
            frustum.executeProjection();
            extent = BZDBCache::worldSize * 10.0f;
        }
        // if low quality then use stipple -- it's probably much faster
        if (BZDBCache::blend && (useQualityValue >= 1))
        {
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
            glEnable(GL_BLEND);
            glColor(mirrorColor);
        }
        else
        {
            float stipple = mirrorColor[3];
            glColor(glm::vec3(mirrorColor));
            OpenGLGState::setStipple(stipple);
            glEnable(GL_POLYGON_STIPPLE);
        }
        glRectf(-extent, -extent, +extent, +extent);
        if (BZDBCache::blend && (useQualityValue >= 1))
            glDisable(GL_BLEND);
        else
            glDisable(GL_POLYGON_STIPPLE);
        if (mapFog)
        {
            glMatrixMode(GL_PROJECTION);
            glLoadIdentity();
            glMatrixMode(GL_MODELVIEW);
            glLoadIdentity();
        }

        clearZbuffer = false;
    }

    // the real scene
    renderScene(_lastFrame, _sameFrame, fullWindow);

    // finalize dimming
    if (mapFog)
        glDisable(GL_FOG);

    renderDimming();

    triangleCount = RenderNode::getTriangleCount();
    if (background)
        triangleCount += background->getTriangleCount();

    return;
}


void SceneRenderer::renderScene(bool UNUSED(_lastFrame), bool UNUSED(_sameFrame),
                                bool fullWindow)
{
    int i;
    const bool lightLists = BZDB.isTrue("lightLists");

    const World* world = World::getWorld();


    // avoid OpenGL calls as long as possible -- there's a good
    // chance we're waiting on the vertical retrace.

    // get a list of the dynamic lights
    getLights();

    // get the obstacle sceneNodes and shadowNodes
    if (world)
        getRenderNodes();

    // prepare transforms
    // note -- lights should not be positioned before view is set
    frustum.executeDeepProjection();
    glPushMatrix();
    frustum.executeView();

    // turn sunlight on -- the ground needs it
    if (BZDBCache::lighting && sunOrMoonUp)
    {
        theSun.execute(SunLight, lightLists);
        theSun.enableLight(SunLight, true);
    }

    // set scissor
    glScissor(window->getOriginX(), window->getOriginY() + window->getHeight() - window->getViewHeight(),
              window->getWidth(), window->getViewHeight());

    if (useDepthComplexityOn)
    {
        if (BZDBCache::stencilShadows)
            BZDB.set("stencilShadows", "0");
        glEnable(GL_STENCIL_TEST);
        if (!mirror || (clearZbuffer))
            glClear(GL_STENCIL_BUFFER_BIT);
        glStencilFunc(GL_ALWAYS, 0, 0xf);
        glStencilOp(GL_KEEP, GL_INCR, GL_INCR);
    }
    if (useHiddenLineOn)
    {
        if (!mirror || (clearZbuffer))
        {
            glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
            glClear(GL_COLOR_BUFFER_BIT);
        }
        glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
    }
    else if (useWireframeOn)
    {
        if (!mirror || (clearZbuffer))
        {
            glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
            glClear(GL_COLOR_BUFFER_BIT);
        }
        glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
    }

    // prepare z buffer
    if (BZDBCache::zbuffer)
    {
        if (sameFrame && ++depthRange == numDepthRanges)
            depthRange = 0;
        if (exposed || useHiddenLineOn || --depthRange < 0)
        {
            depthRange = numDepthRanges - 1;
            if (clearZbuffer)
                glClear(GL_DEPTH_BUFFER_BIT);
            exposed = false;
        }
        if (!sameFrame && numDepthRanges != 1)
        {
            if (useHiddenLineOn)
                glDepthRange(0.0, 1.0);
            else
            {
                GLclampd x_near = (GLclampd)depthRange * depthRangeSize;
                glDepthRange(x_near, x_near + depthRangeSize);
            }
        }
    }

    // draw start of background (no depth testing)
    OpenGLGState::resetState();

    const auto plane = glm::dvec4(0.0, 0.0, +1.0, 0.0);
    glClipPlane(GL_CLIP_PLANE0, plane);

    if (background)
    {
        background->setBlank(blank);
        background->setInvert(invert);

        const bool avoidSkyFog = (mapFog && BZDB.isTrue("_fogNoSky"));
        if (avoidSkyFog)
        {
            glDisable(GL_FOG);
            background->renderSky(*this, fullWindow, mirror);
            glEnable(GL_FOG);
        }
        else
            background->renderSky(*this, fullWindow, mirror);

        if (drawGround)
            background->renderGround(*this, fullWindow);
    }

    // prepare the other lights but don't turn them on yet --
    // we may need to turn them on when drawing the background.
    if (BZDBCache::lighting)
    {
        for (i = 0; i < dynamicLights; i++)
            lights[i]->execute(i + reservedLights, lightLists);
    }

    // draw rest of background
    // (ground grid, shadows, fake shot lights, mountains, clouds)
    if (background)
        background->renderGroundEffects(*this, mirror && clearZbuffer);

    if (!blank)
    {
        if (BZDBCache::lighting)
        {
            // now turn on the remaining lights
            for (i = 0; i < dynamicLights; i++)
                OpenGLLight::enableLight(i + reservedLights, true);
        }

        frustum.executeProjection();

        if (BZDBCache::zbuffer)
            glEnable(GL_DEPTH_TEST);

        if (useHiddenLineOn)
            glEnable(GL_POLYGON_OFFSET_FILL);

        ///////////////////////
        // THE BIG RENDERING //
        ///////////////////////
        if (world)
            doRender();


        if (scene && BZDBCache::showCullingGrid)
            scene->drawCuller();

        if (scene && BZDBCache::showCollisionGrid && (world != NULL))
            world->drawCollisionGrid();

        if (useHiddenLineOn)
        {
            glDisable(GL_POLYGON_OFFSET_FILL);
            glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
            glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
            doRender();
            glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
        }

        OpenGLGState::resetState();

        // shut off lights
        if (BZDBCache::lighting)
        {
            theSun.enableLight(SunLight, false);
            for (i = 0; i < dynamicLights; i++)
                OpenGLLight::enableLight(i + reservedLights, false);
        }

        if (BZDBCache::zbuffer)
            glDisable(GL_DEPTH_TEST);

        // FIXME -- must do post-rendering: flare lights, etc.
        // flare lights are in world coordinates.  trace ray to that world
        // position and calculate opacity.  if opaque then don't render
        // flare, otherwise modulate input color by opacity and draw a
        // billboard texture (constant size in screen space).
    }

    // back to original state
    if (!useHiddenLineOn && useWireframeOn)
        glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    glPopMatrix();

    // do depth complexity
    if (useDepthComplexityOn)
        renderDepthComplexity();

    return;
}


void SceneRenderer::doRender()
{
    const bool mirrorPass = (mirror && clearZbuffer);

    // render the ground tank tracks
    if (!mirrorPass)
        TrackMarks::renderGroundTracks();

    // NOTE -- this should go into a separate thread
    // now draw each render node list
    OpenGLGState::renderLists();

    // render the environmental conditions
    if (background)
    {
        // do not update for the second mirror pass
        background->renderEnvironment(*this, !mirror || clearZbuffer);
    }

    // draw all the stuff in the ordered list.  turn
    // off depth buffer updates for potentially transparent stuff.
    glDepthMask(GL_FALSE);
    orderedList.render();
    glDepthMask(GL_TRUE);

    // render the ground tank tracks
    if (!mirrorPass)
        TrackMarks::renderObstacleTracks();

    return;
}


static bool setupMapFog()
{
    RENDERER.setFogActive(false);
    if (BZDB.get(StateDatabase::BZDB_FOGMODE) == "none")
    {
        glDisable(GL_FOG);
        glHint(GL_FOG_HINT, GL_FASTEST);
        return false;
    }
    RENDERER.setFogActive(true);
    GLenum fogMode = GL_EXP;
    GLfloat fogDensity = 0.001f;
    GLfloat fogStart = 0.5f * BZDBCache::worldSize;
    GLfloat fogEnd = BZDBCache::worldSize;
    auto fogColor = glm::vec4(0.25f, 0.25f, 0.25f, 0.25f);

    // parse the values;
    const std::string modeStr = BZDB.get("_fogMode");
    if (modeStr == "linear")
        fogMode = GL_LINEAR;
    else if (modeStr == "exp")
        fogMode = GL_EXP;
    else if (modeStr == "exp2")
        fogMode = GL_EXP2;
    else
        fogMode = GL_EXP;
    fogDensity = BZDB.eval(StateDatabase::BZDB_FOGDENSITY);
    fogStart = BZDB.eval(StateDatabase::BZDB_FOGSTART);
    fogEnd = BZDB.eval(StateDatabase::BZDB_FOGEND);
    if (!parseColorString(BZDB.get(StateDatabase::BZDB_FOGCOLOR), fogColor))
        fogColor = glm::vec4(0.1f, 0.1f, 0.1f, 0.0f); // has no effect
    if (BZDB.evalInt("fogEffect") >= 1)
        glHint(GL_FOG_HINT, GL_NICEST);
    else
        glHint(GL_FOG_HINT, GL_FASTEST);

    // setup GL fog
    glFogi(GL_FOG_MODE, fogMode);
    glFogf(GL_FOG_DENSITY, fogDensity);
    glFogf(GL_FOG_START, fogStart);
    glFogf(GL_FOG_END, fogEnd);
    glSetFogColor(fogColor);
    glEnable(GL_FOG);

    RENDERER.setFogColor(fogColor);
    return true;
}


void SceneRenderer::renderDimming()
{
    float density = 0.0f;
    const GLfloat* color = NULL;
    if (useDimming)
    {
        color = dimnessColor;
        density = dimDensity;
    }
    else if (teleporterProximity > 0.0f)
    {
        color = blindnessColor;
        density =
            (teleporterProximity > 0.75f) ? 1.0f : teleporterProximity / 0.75f;
    }
    if (density > 0.0f && color != NULL)
    {
        glMatrixMode(GL_PROJECTION);
        glLoadIdentity();
        glMatrixMode(GL_MODELVIEW);
        glColor4f(color[0], color[1], color[2], density);

        // if low quality then use stipple -- it's probably much faster
        if (BZDBCache::blend && (useQualityValue >= 1))
            glEnable(GL_BLEND);
        else
        {
            OpenGLGState::setStipple(density);
            glEnable(GL_POLYGON_STIPPLE);
        }
        glRectf(-1.0f, -1.0f, +1.0f, +1.0f);
        if (BZDBCache::blend && (useQualityValue >= 1))
            glDisable(GL_BLEND);
        else
            glDisable(GL_POLYGON_STIPPLE);
    }
    return;
}


void SceneRenderer::renderDepthComplexity()
{
    static const GLfloat depthColors[][3] =
    {
        { 0.0f, 0.0f, 0.0f }, // black -- 0 times
        { 0.5f, 0.0f, 1.0f }, // purple -- 1 time
        { 0.0f, 0.0f, 1.0f }, // blue -- 2 times
        { 0.0f, 1.0f, 1.0f }, // cyan -- 3 times
        { 0.0f, 1.0f, 0.0f }, // green -- 4 times
        { 1.0f, 1.0f, 0.0f }, // yellow -- 5 times
        { 1.0f, 0.5f, 0.0f }, // orange -- 6 times
        { 1.0f, 0.0f, 0.0f }  // red -- 7 or more
    };
    static const int numColors = bzcountof(depthColors);

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glMatrixMode(GL_MODELVIEW);
    glStencilOp(GL_KEEP, GL_KEEP, GL_KEEP);
    for (int i = 0; i < numColors; i++)
    {
        glStencilFunc(i == numColors - 1 ? GL_LEQUAL : GL_EQUAL, i, 0xf);
        glColor3fv(depthColors[i]);
        glRectf(-1.0f, -1.0f, 1.0f, 1.0f);
    }
    glDisable(GL_STENCIL_TEST);

    return;
}


void SceneRenderer::getRenderNodes()
{
    // get the nodes to draw
    if (!blank)
    {
        // empty the render node lists in preparation for the next frame
        OpenGLGState::clearLists();
        orderedList.clear();
        shadowList.clear();
        flareLightList.clear();

        // make the lists of render nodes sorted in optimal rendering order
        if (scene)
            scene->addRenderNodes(*this);

        // sort ordered list in reverse depth order
        if (!inOrder)
            orderedList.sort(frustum.getEye());

        // add the shadow rendering nodes
        if (scene && BZDBCache::shadows && !BZDB.isTrue(StateDatabase::BZDB_NOSHADOWS)
                && (!mirror || !clearZbuffer))
            scene->addShadowNodes(*this);
    }
    return;
}


void SceneRenderer::getLights()
{
    // get the important lights in the scene
    if (!sameFrame)
    {

        lightsCount = 0;
        dynamicLights = 0;

        if (scene && !blank && BZDBCache::lighting)
        {
            // get the potential dynamic lights
            scene->addLights(*this);

            // calculate the light importances
            int i;
            for (i = 0; i < lightsCount; i++)
                lights[i]->calculateImportance(frustum);

            // sort by cull state, grounded state, and importance
            qsort (lights, lightsCount, sizeof(OpenGLLight*), sortLights);

            // count the unculled valid lights and potential dynamic lights
            // (negative values indicate culled lights)
            int unculledCount = 0;
            for (i = 0; i < lightsCount; i++)
            {
                // any value below 0.0f is culled
                if (lights[i]->getImportance() >= 0.0f)
                {
                    unculledCount++;
                    if (!lights[i]->getOnlyGround())
                        dynamicLights++;
                }
            }

            // set the total light count to the number of unculled lights
            lightsCount = unculledCount;

            // limit the dynamic OpenGL light count
            if (dynamicLights > maxLights)
                dynamicLights = maxLights;
        }
    }
    return;
}


void SceneRenderer::disableLights(const glm::vec3 &mins, const glm::vec3 &maxs)
{
    // temporarily turn off non-applicable lights for big meshes
    for (int i = 0; i < dynamicLights; i++)
    {
        const auto  pos  = glm::vec3(lights[i]->getPosition());
        const float dist = lights[i]->getMaxDist();
        if (glm::any(glm::lessThan(pos, mins - dist)) ||
                glm::any(glm::greaterThan(pos, maxs + dist)))
            lights[i]->enableLight(i + reservedLights, false);
    }
    return;
}


void SceneRenderer::reenableLights()
{
    // reenable the disabled lights
    for (int i = 0; i < dynamicLights; i++)
        lights[i]->enableLight(i + reservedLights, true);
    return;
}


void SceneRenderer::notifyStyleChange()
{
    needStyleUpdate = true;
    return;
}


const RenderNodeList& SceneRenderer::getShadowList() const
{
    return shadowList;
}


const glm::vec3 *SceneRenderer::getSunDirection() const
{
    if (background)
        return background->getSunDirection();
    else
        return NULL;
}


const Extents* SceneRenderer::getVisualExtents() const
{
    if (scene)
        return scene->getVisualExtents();
    else
        return NULL;
}


int SceneRenderer::getFrameTriangleCount() const
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
