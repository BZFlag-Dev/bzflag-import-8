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

// implementation header
#include "MeshDrawInfo.h"

// system implementation headers
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <map>
#include <vector>
#include <sstream>
#include <iostream>
#include <ctype.h>
#include <glm/gtc/type_ptr.hpp>
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/norm.hpp>

// common implementation headers
#include "Pack.h"
#include "BzMaterial.h"
#include "MeshObstacle.h"
#include "TimeKeeper.h"
#include "bzfio.h" // for debugging info


// local types
typedef struct
{
    const char* name;
    DrawCmd::DrawModes code;
} DrawCmdLabel;


// local data
static DrawCmdLabel drawLabels[] =
{
    {"points",    DrawCmd::DrawPoints},
    {"lines",     DrawCmd::DrawLines},
    {"lineloop",  DrawCmd::DrawLineLoop},
    {"linestrip", DrawCmd::DrawLineStrip},
    {"tris",      DrawCmd::DrawTriangles},
    {"tristrip",  DrawCmd::DrawTriangleStrip},
    {"trifan",    DrawCmd::DrawTriangleFan},
    {"quads",     DrawCmd::DrawQuads},
    {"quadstrip", DrawCmd::DrawQuadStrip},
    {"polygon",   DrawCmd::DrawPolygon}
};

static const int MaxUShort = 0xFFFF;


// local function prototypes
static int compareLengthPerPixel(const void* a, const void* b);


/******************************************************************************/
/******************************************************************************/

MeshDrawInfo::MeshDrawInfo(const std::vector<std::string>& options)
{
    init(); // server-side constructor

    lodOptions = options;

    return;
}


MeshDrawInfo::MeshDrawInfo()
{
    init(); // client-side unpacking constructor
    return;
}


MeshDrawInfo::MeshDrawInfo(const MeshDrawInfo* di,
                           const MeshTransform& xform,
                           const std::map<const BzMaterial*,
                           const BzMaterial*>& _matMap)
{
    init(); // client side copy constructor

    source = di; // a copy this is

    name = di->getName() + "_copy";

    // copy extents and sphere  (xform applied later)
    extents = di->extents;
    position = di->position;
    radius2  = di->radius2;

    // counts
    cornerCount = di->cornerCount;
    lodCount = di->lodCount;
    radarCount = di->radarCount;

    // referenced data
    corners = di->corners;
    vertices = di->vertices;
    normals = di->normals;
    texcoords = di->texcoords;
    lods = di->lods;
    radarLods = di->radarLods;
    animInfo = di->animInfo;

    // new data
    matMap = new MaterialMap(_matMap);
    xformTool = new MeshTransform::Tool(xform);

    return;
}


MeshDrawInfo::~MeshDrawInfo()
{
    logDebugMessage(4,"~MeshDrawInfo(): source = %p\n", source);
    fflush(stdout);
    fflush(stderr);

    clear();

    logDebugMessage(4,"~MeshDrawInfo(): done\n");
    fflush(stdout);
    fflush(stderr);
}


void MeshDrawInfo::clear()
{
    // drawMgr is freed externally
    if (source == NULL)
    {
        delete[] corners;
        delete[] vertices;
        delete[] normals;
        delete[] texcoords;
        int i;
        for (i = 0; i < lodCount; i++)
            lods[i].clear();
        delete[] lods;
        for (i = 0; i < radarCount; i++)
            radarLods[i].clear();
        delete[] radarLods;
        delete animInfo;
    }
    delete matMap;
    delete xformTool;
    delete[] rawVerts;
    delete[] rawNorms;
    delete[] rawTxcds;
    init();
    return;
}


void MeshDrawInfo::init()
{
    name = "";
    lines = 0;

    valid = true;
    serverSide = true;

    lodOptions.clear();

    source = NULL;
    drawMgr = NULL;

    extents.reset();
    position = glm::vec3(+MAXFLOAT);
    radius2  = +MAXFLOAT;

    cornerCount = 0;
    corners = NULL;
    vertices = NULL;
    normals = NULL;
    texcoords = NULL;

    rawVertCount = 0;
    rawVerts = NULL;
    rawNormCount = 0;
    rawNorms = NULL;
    rawTxcdCount = 0;
    rawTxcds = NULL;

    lodCount = 0;
    lods = NULL;

    radarCount = 0;
    radarLods = NULL;

    matMap = NULL;
    xformTool = NULL;

    animInfo = NULL;

    return;
}


bool MeshDrawInfo::validate(const MeshObstacle* mesh) const
{
    if (mesh == NULL)
    {
        logDebugMessage(0,"ERROR: drawInfo has no mesh\n");
        return false;
    }

    int vCount, nCount, tCount;
    if (rawVertCount > 0)
    {
        vCount = rawVertCount;
        nCount = rawNormCount;
        tCount = rawTxcdCount;
    }
    else
    {
        vCount = mesh->getVertexCount();
        nCount = mesh->getNormalCount();
        tCount = mesh->getTexcoordCount();
    }

    // verify the corners
    for (int i = 0; i < cornerCount; i++)
    {
        const int v = corners[i].vertex;
        const int n = corners[i].normal;
        const int t = corners[i].texcoord;
        if ((v < 0) || (v >= vCount) ||
                (n < 0) || (n >= nCount) ||
                (t < 0) || (t >= tCount))
        {
            logDebugMessage(0,"ERROR: Bad corner: %i %i %i\n", v, n, t);
            return false;
        }
    }

    // verify the command indices
    for (int lod = 0; lod < lodCount; lod++)
    {
        DrawLod& drawLod = lods[lod];
        for (int set = 0; set < drawLod.count; set++)
        {
            DrawSet& drawSet = drawLod.sets[set];
            for (int cmd = 0; cmd < drawSet.count; cmd++)
            {
                DrawCmd& drawCmd = drawSet.cmds[cmd];
                if (drawCmd.indexType == DrawCmd::DrawIndexUShort)
                {
                    unsigned short* array = (unsigned short*)drawCmd.indices;
                    for (int idx = 0; idx < drawCmd.count; idx++)
                    {
                        if ((int)array[idx] >= cornerCount)
                        {
                            logDebugMessage(0,"ERROR: Bad cmd\n");
                            return false;
                        }
                    }
                }
                else if (drawCmd.indexType == DrawCmd::DrawIndexUInt)
                {
                    unsigned int* array = (unsigned int*)drawCmd.indices;
                    for (int idx = 0; idx < drawCmd.count; idx++)
                    {
                        if ((int)array[idx] >= cornerCount)
                        {
                            logDebugMessage(0,"ERROR: Bad cmd\n");
                            return false;
                        }
                    }
                }
                else
                {
                    logDebugMessage(0,"ERROR: Bad index type (%i)\n", drawCmd.indexType);
                    return false;
                }
            }
        }
    }
    return true;
}


static int compareLengthPerPixel(const void* a, const void* b)
{
    const DrawLod* lodA = (const DrawLod*)a;
    const DrawLod* lodB = (const DrawLod*)b;
    const float lenA = lodA->lengthPerPixel;
    const float lenB = lodB->lengthPerPixel;
    // higher resolution meshes for smaller lengths per pixel
    if (lenA < lenB)
        return -1;
    else if (lenA > lenB)
        return +1;
    else
        return 0;
}


bool MeshDrawInfo::serverSetup(const MeshObstacle* mesh)
{
    valid = validate(mesh);
    if (!valid)
        return false;

    int vCount;
    const glm::vec3 *verts;
    if (rawVertCount > 0)
    {
        vCount = rawVertCount;
        verts = rawVerts;
    }
    else
    {
        vCount = mesh->getVertexCount();
        verts = mesh->getVertices();
    }

    Extents tmpExts;
    const bool calcCenter = (position[0] == +MAXFLOAT) &&
                            (position[1] == +MAXFLOAT) &&
                            (position[2] == +MAXFLOAT);
    const bool calcRadius = (radius2 == +MAXFLOAT);
    const bool calcExtents = (extents.mins[0] == tmpExts.mins[0]) &&
                             (extents.mins[1] == tmpExts.mins[1]) &&
                             (extents.mins[2] == tmpExts.mins[2]) &&
                             (extents.maxs[0] == tmpExts.maxs[0]) &&
                             (extents.maxs[1] == tmpExts.maxs[1]) &&
                             (extents.maxs[2] == tmpExts.maxs[2]);

    // calculate the extents?
    if (calcCenter || calcRadius || calcExtents)
    {
        if ((animInfo == NULL) || (animInfo->angvel == 0.0f))
        {
            // no animation, use the raw vertices
            for (int v = 0; v < vCount; v++)
                tmpExts.expandToPoint(verts[v]);
        }
        else
        {
            // factor in the rotation animation
            float minZ = +MAXFLOAT;
            float maxZ = -MAXFLOAT;
            float maxDistSqr = -MAXFLOAT;
            for (int v = 0; v < vCount; v++)
            {
                const auto &p = verts[v];
                if (p[2] < minZ)
                    minZ = p[2];
                if (p[2] > maxZ)
                    maxZ = p[2];
                const float distSqr = glm::length2(glm::vec2(p));
                if (distSqr > maxDistSqr)
                    maxDistSqr = distSqr;
            }
            const float dist = sqrtf(maxDistSqr);
            tmpExts.mins[0] = -dist;
            tmpExts.mins[1] = -dist;
            tmpExts.mins[2] = minZ;
            tmpExts.maxs[0] = +dist;
            tmpExts.maxs[1] = +dist;
            tmpExts.maxs[2] = maxZ;
        }
        // set the extents
        if (calcExtents)
            extents = tmpExts;
    }

    // calculate the sphere params?
    if (calcCenter)
        position = 0.5f * (extents.maxs + extents.mins);
    if (calcRadius)
        // radius squared
        radius2 = 0.25f * glm::distance2(extents.maxs, extents.mins);

    // calculate the DrawSet spheres?
    for (int lod = 0; lod < lodCount; lod++)
    {
        DrawLod& drawLod = lods[lod];
        for (int set = 0; set < drawLod.count; set++)
        {
            DrawSet& drawSet = drawLod.sets[set];
            const bool calcSetCenter = (drawSet.position[0] == +MAXFLOAT) &&
                                       (drawSet.position[1] == +MAXFLOAT) &&
                                       (drawSet.position[2] == +MAXFLOAT);
            const bool calcSetRadius = (drawSet.radius2 == +MAXFLOAT);
            if (calcSetCenter || calcSetRadius)
            {
                Extents exts;
                for (int cmd = 0; cmd < drawSet.count; cmd++)
                {
                    DrawCmd& drawCmd = drawSet.cmds[cmd];
                    if (drawCmd.indexType == DrawCmd::DrawIndexUShort)
                    {
                        unsigned short* array = (unsigned short*)drawCmd.indices;
                        for (int idx = 0; idx < drawCmd.count; idx++)
                        {
                            const unsigned short cIndex = array[idx];
                            const Corner& corner = corners[cIndex];
                            const auto v = verts[corner.vertex];
                            exts.expandToPoint(v);
                        }
                    }
                    else if (drawCmd.indexType == DrawCmd::DrawIndexUInt)
                    {
                        unsigned int* array = (unsigned int*)drawCmd.indices;
                        for (int idx = 0; idx < drawCmd.count; idx++)
                        {
                            const unsigned int cIndex = array[idx];
                            const Corner& corner = corners[cIndex];
                            const auto v = verts[corner.vertex];
                            exts.expandToPoint(v);
                        }
                    }
                }
                if (calcSetCenter)
                    drawSet.position = 0.5f * (exts.maxs + exts.mins);
                if (calcSetRadius)
                    drawSet.radius2 = 0.25f
                                      * glm::distance2(exts.maxs, exts.mins); // radius squared
            }
        }
    }

    // sort the lods
    qsort(lods, lodCount, sizeof(DrawLod), compareLengthPerPixel);

    return true;
}


bool MeshDrawInfo::clientSetup(const MeshObstacle* mesh)
{
    serverSide = false;

    valid = validate(mesh);
    if (!valid)
        return false;

    const glm::vec3 *verts;
    const glm::vec3 *norms;
    const glm::vec2 *txcds;
    if (rawVertCount > 0)
    {
        verts = rawVerts;
        norms = rawNorms;
        txcds = rawTxcds;
    }
    else
    {
        verts = mesh->getVertices();
        norms = mesh->getNormals();
        txcds = mesh->getTexcoords();
    }

    // make the element arrays
    vertices  = new glm::vec3[cornerCount];
    normals   = new glm::vec3[cornerCount];
    texcoords = new glm::vec2[cornerCount];
    for (int i = 0; i < cornerCount; i++)
    {
        Corner& corner = corners[i];
        vertices[i]  = verts[corner.vertex];
        normals[i]   = norms[corner.normal];
        texcoords[i] = txcds[corner.texcoord];
    }

    // tally the triangle counts
    for (int lod = 0; lod < lodCount; lod++)
    {
        DrawLod& drawLod = lods[lod];
        for (int set = 0; set < drawLod.count; set++)
        {
            DrawSet& drawSet = drawLod.sets[set];
            int tris = 0;
            for (int cmd = 0; cmd < drawSet.count; cmd++)
            {
                DrawCmd& drawCmd = drawSet.cmds[cmd];
                const int points = drawCmd.count;
                switch (drawCmd.drawMode)
                {
                // NOTE: points and lines are each counted as a triangle
                case DrawCmd::DrawPoints:
                    tris += points;
                    break;
                case DrawCmd::DrawLines:
                    tris += points / 2;
                    break;
                case DrawCmd::DrawLineLoop:
                    tris += points;
                    break;
                case DrawCmd::DrawLineStrip:
                    tris += points - 1;
                    break;
                case DrawCmd::DrawTriangles:
                    tris += points / 3;
                    break;
                case DrawCmd::DrawTriangleStrip:
                    tris += points - 2;
                    break;
                case DrawCmd::DrawTriangleFan:
                    tris += points - 2;
                    break;
                case DrawCmd::DrawQuads:
                    tris += points / 2;
                    break;
                case DrawCmd::DrawQuadStrip:
                    tris += points - 2;
                    break;
                case DrawCmd::DrawPolygon:
                    tris += points - 2;
                    break;
                default:
                    break;
                }
            }
            drawSet.triangleCount = tris;
        }
    }

    // sort the lods
    qsort(lods, lodCount, sizeof(DrawLod), compareLengthPerPixel);

    return true;
}


/******************************************************************************/

void MeshDrawInfo::setName(const std::string& _name)
{
    name = _name;
    return;
}


MeshDrawMgr* MeshDrawInfo::getDrawMgr() const
{
    if (!isCopy())
        return drawMgr;
    else
        return source->getDrawMgr();
}


void MeshDrawInfo::getMaterials(MaterialSet& matSet) const
{
    for (int i = 0; i < lodCount; i++)
    {
        DrawLod& lod = lods[i];
        for (int j = 0; j < lod.count; j++)
        {
            DrawSet& set = lod.sets[j];
            matSet.insert(set.material);
        }
    }
    return;
}


bool MeshDrawInfo::isInvisible() const
{
    for (int i = 0; i < lodCount; i++)
    {
        DrawLod& lod = lods[i];
        for (int j = 0; j < lod.count; j++)
        {
            DrawSet& set = lod.sets[j];
            const BzMaterial* mat = set.material;
            if (mat->getDiffuse()[3] != 0.0f)
                return false;
        }
    }
    return true;
}


/******************************************************************************/

void MeshDrawInfo::updateAnimation(double time)
{
    if (animInfo != NULL)
    {
        const float angle = (float)fmod((double)animInfo->angvel * time, 360.0);
        const float radians = angle * (float)(M_PI / 180.0);
        animInfo->angle = angle;
        animInfo->cos_val = cosf(radians);
        animInfo->sin_val = sinf(radians);
    }
    return;
}


/******************************************************************************/

/*
bool MeshDrawInfo::convertEnums(const UintMap& drawMap, const UintMap& typeMap)
{
  bool status = true;
  int lod, mat, cmd;

  for (lod = 0; lod < lodCount; lod++) {
    for (mat = 0; mat < materialCount; mat++) {
      DrawSet& set = sets[(lod * materialCount) + mat];
      for (cmd = 0; cmd < set.count; cmd++) {
    std::map<unsigned int, unsigned int>::const_iterator it;
    DrawCmd& command = set.cmds[cmd];
    // draw type
    it = drawMap.find(command.drawMode);
    if (it == drawMap.end()) {
      status = false;
    } else {
      command.drawMode = it->second;
    }
    // index type
    it = typeMap.find(command.indexType);
    if (it == typeMap.end()) {
      status = false;
    } else {
      command.indexType = it->second;
    }
      }
    }
  }

  for (lod = 0; lod < radarLods; lod++) {
    DrawSet& set = radarSets[lod];
    for (cmd = 0; cmd < set.count; cmd++) {
      std::map<unsigned int, unsigned int>::const_iterator it;
      DrawCmd& command = set.cmds[cmd];
      // draw type
      it = drawMap.find(command.drawMode);
      if (it == drawMap.end()) {
    status = false;
      } else {
    command.drawMode = it->second;
      }
      // index type
      it = typeMap.find(command.indexType);
      if (it == typeMap.end()) {
    status = false;
      } else {
    command.indexType = it->second;
      }
    }
  }

  return status;
}
*/

/******************************************************************************/

static std::map<std::string, unsigned int> drawModeMap;


static void setupDrawModeMap()
{
    drawModeMap["points"] =    DrawCmd::DrawPoints;
    drawModeMap["lines"] =     DrawCmd::DrawLines;
    drawModeMap["lineloop"] =  DrawCmd::DrawLineLoop;
    drawModeMap["linestrip"] = DrawCmd::DrawLineStrip;
    drawModeMap["tris"] =      DrawCmd::DrawTriangles;
    drawModeMap["tristrip"] =  DrawCmd::DrawTriangleStrip;
    drawModeMap["trifan"] =    DrawCmd::DrawTriangleFan;
    drawModeMap["quads"] =     DrawCmd::DrawQuads;
    drawModeMap["quadstrip"] = DrawCmd::DrawQuadStrip;
    drawModeMap["polygon"] =   DrawCmd::DrawPolygon;
    return;
}


static inline void finishLine(std::istream& input)
{
    std::string dummy;
    std::getline(input, dummy);
    return;
}


static bool parseCorner(std::istream& input, Corner& corner)
{
    bool success = true;
    if (!(input >> corner.vertex) ||
            !(input >> corner.normal) ||
            !(input >> corner.texcoord))
    {
        success = false;
        logDebugMessage(0,"Bad corner\n");
    }
    return success;
}


static bool parseDrawCmd(std::istream& input, const std::string& mode,
                         DrawCmd& cmd)
{
    // parse the draw mode
    std::map<std::string, unsigned int>::const_iterator it;
    it = drawModeMap.find(mode);
    if (it == drawModeMap.end())
    {
        finishLine(input);
        return false;
    }
    cmd.drawMode = it->second;

    // parse the indices
    std::vector<int> list;
    int index;
    while (input >> index)
        list.push_back(index);
    cmd.count = list.size();
    unsigned int* array = new unsigned int[cmd.count];
    for (int i = 0; i < cmd.count; i++)
        array[i] = list[i];
    cmd.indices = array;
    cmd.finalize(); // setup the index type

    return true;
}


static bool parseDrawSet(std::istream& input, DrawSet& set, int& lines)
{
    bool success = true;

    std::vector<DrawCmd> pCmds;
    std::string line, label;

    while (std::getline(input, line))
    {
        ++lines;

        std::istringstream parms(line);
        if (!(parms >> label))
            continue;

        if (label[0] == '#')
            continue;
        else if (strcasecmp(label.c_str(), "end") == 0)
            break;
        else if (strcasecmp(label.c_str(),"dlist") == 0)
            set.wantList = true;
        else if (strcasecmp(label.c_str(), "center") == 0)
        {
            if (!(parms >> set.position[0]) || !(parms >> set.position[1]) ||
                    !(parms >> set.position[2]))
            {
                success = false;
                logDebugMessage(0,"Bad center\n");
            }
        }
        else if (strcasecmp(label.c_str(), "sphere") == 0)
        {
            if (!(parms >> set.position[0]) || !(parms >> set.position[1]) ||
                    !(parms >> set.position[2]) || !(parms >> set.radius2))
            {
                success = false;
                logDebugMessage(0,"Bad sphere\n");
            }
        }
        else
        {
            DrawCmd cmd;
            if (parseDrawCmd(parms, label, cmd))
                pCmds.push_back(cmd);
            else
            {
                success = false;
                logDebugMessage(0,"Bad drawSet\n");
            }
        }
    }

    // make commands
    set.count = pCmds.size();
    set.cmds = new DrawCmd[set.count];
    for (int i = 0; i < set.count; i++)
        set.cmds[i] = pCmds[i];

    return success;
}


static bool parseDrawLod(std::istream& input, DrawLod& lod, int& lines)
{
    bool success = true;

    std::vector<DrawSet> pSets;
    std::string line, cmd;

    while (std::getline(input, line))
    {
        ++lines;

        std::istringstream parms(line);
        if (!(parms >> cmd))
            continue;

        if (cmd[0] == '#')
            continue;
        else if (strcasecmp(cmd.c_str(), "end") == 0)
            break;
        else if ((strcasecmp(cmd.c_str(), "length") == 0) ||
                 (strcasecmp(cmd.c_str(), "lengthPerPixel") == 0))
        {
            float lengthPerPixel;
            if (parms >> lengthPerPixel)
                lod.lengthPerPixel = lengthPerPixel;
            else
            {
                success = false;
                logDebugMessage(0,"Bad lengthPerPixel:\n");
            }
        }
        else if (strcasecmp(cmd.c_str(), "matref") == 0)
        {
            std::string matName;
            if (parms >> matName)
            {
                DrawSet set;
                set.material = MATERIALMGR.findMaterial(matName);
                if (parseDrawSet(input, set, lines))
                    pSets.push_back(set);
                else
                {
                    success = false;
                    logDebugMessage(0,"Bad material:\n");
                }
            }
            else
            {
                success = false;
                logDebugMessage(0,"Bad drawLod parameter:\n");
            }
        }
    }

    // make sets
    lod.count = pSets.size();
    lod.sets = new DrawSet[lod.count];
    for (int i = 0; i < lod.count; i++)
        lod.sets[i] = pSets[i];

    return success;
}


bool MeshDrawInfo::parse(std::istream& input)
{
    TimeKeeper start = TimeKeeper::getCurrent();

    bool success = true;
    bool allDList = false;

    int i;

    std::vector<Corner> pCorners;
    std::vector<DrawLod> pLods;
    std::vector<DrawLod> pRadarLods;
    std::vector<glm::vec3> pVerts;
    std::vector<glm::vec3> pNorms;
    std::vector<glm::vec2> pTxcds;

    setupDrawModeMap();
    finishLine(input); // flush the rest of the "drawInfo" line

    DrawLod lod;
    std::string line, cmd;

    while (std::getline(input, line))
    {
        ++lines;

        std::istringstream parms(line);
        if (!(parms >> cmd))
            continue;

        if (cmd[0] == '#')
            continue;
        else if (strcasecmp(cmd.c_str(), "end") == 0)
            break;
        else if (strcasecmp(cmd.c_str(), "dlist") == 0)
            allDList = true;
        else if (strcasecmp(cmd.c_str(), "angvel") == 0)
        {
            if (animInfo != NULL)
            {
                success = false;
                logDebugMessage(0,"Double angvel\n");
            }
            float angvel;
            if (parms >> angvel)
            {
                animInfo = new AnimationInfo;
                animInfo->angvel = angvel;
            }
            else
            {
                success = false;
                logDebugMessage(0,"Bad angvel\n");
            }
        }
        else if (strcasecmp(cmd.c_str(), "extents") == 0)
        {
            if (!(parms >> extents.mins[0]) || !(parms >> extents.mins[1]) ||
                    !(parms >> extents.mins[2]) || !(parms >> extents.maxs[0]) ||
                    !(parms >> extents.maxs[1]) || !(parms >> extents.maxs[2]))
            {
                success = false;
                logDebugMessage(0,"Bad extents\n");
            }
        }
        else if (strcasecmp(cmd.c_str(), "center") == 0)
        {
            if (!(parms >> position[0]) || !(parms >> position[1]) ||
                    !(parms >> position[2]))
            {
                success = false;
                logDebugMessage(0,"Bad center\n");
            }
        }
        else if (strcasecmp(cmd.c_str(), "sphere") == 0)
        {
            if (!(parms >> position[0]) || !(parms >> position[1]) ||
                    !(parms >> position[2]) || !(parms >> radius2))
            {
                success = false;
                logDebugMessage(0,"Bad sphere\n");
            }
        }
        else if (strcasecmp(cmd.c_str(), "option") == 0)
        {
            const char* c = line.c_str();
            while ((*c != '\0') && isspace(*c))
                c++;
            lodOptions.push_back(c);
        }
        else if (strcasecmp(cmd.c_str(), "corner") == 0)
        {
            Corner corner;
            if (parseCorner(parms, corner))
                pCorners.push_back(corner);
            else
            {
                success = false;
                logDebugMessage(0,"Bad corner\n");
            }
        }
        else if (strcasecmp(cmd.c_str(), "vertex") == 0)
        {
            glm::vec3 v;
            if ((parms >> v[0]) && (parms >> v[1]) && (parms >> v[2]))
                pVerts.push_back(v);
            else
            {
                success = false;
                logDebugMessage(0,"Bad Vertex\n");
            }
        }
        else if (strcasecmp(cmd.c_str(), "normal") == 0)
        {
            glm::vec3 n;
            if ((parms >> n[0]) && (parms >> n[1]) && (parms >> n[2]))
                pNorms.push_back(n);
            else
            {
                success = false;
                logDebugMessage(0,"Bad Normal\n");
            }
        }
        else if (strcasecmp(cmd.c_str(), "texcoord") == 0)
        {
            glm::vec2 t;
            if ((parms >> t[0]) && (parms >> t[1]))
                pTxcds.push_back(t);
            else
            {
                success = false;
                logDebugMessage(0,"Bad Texcoord\n");
            }
        }
        else if (strcasecmp(cmd.c_str(), "lod") == 0)
        {
            if (parseDrawLod(input, lod, lines))
                pLods.push_back(lod);
            else
            {
                success = false;
                logDebugMessage(0,"Bad lod\n");
            }
        }
        else if (strcasecmp(cmd.c_str(), "radarlod") == 0)
        {
            if (parseDrawLod(input, lod, lines))
                pRadarLods.push_back(lod);
            else
            {
                success = false;
                logDebugMessage(0,"Bad radarlod\n");
            }
        }
    }

    input.putback('\n');

    // make corners
    cornerCount = pCorners.size();
    corners = new Corner[cornerCount];
    for (i = 0; i < cornerCount; i++)
        corners[i] = pCorners[i];

    // make lods
    lodCount = pLods.size();
    lods = new DrawLod[lodCount];
    for (i = 0; i < lodCount; i++)
        lods[i] = pLods[i];

    // make radar lods
    radarCount = pRadarLods.size();
    radarLods = new DrawLod[radarCount];
    for (i = 0; i < radarCount; i++)
        radarLods[i] = pRadarLods[i];

    // make raw verts
    if (!pVerts.empty())
    {
        rawVertCount = pVerts.size();
        rawVerts = new glm::vec3[rawVertCount];
        for (i = 0; i < rawVertCount; i++)
            rawVerts[i] = pVerts[i];
    }
    // make raw norms
    if (!pNorms.empty())
    {
        rawNormCount = pNorms.size();
        rawNorms = new glm::vec3[rawNormCount];
        for (i = 0; i < rawNormCount; i++)
            rawNorms[i] = pNorms[i];
    }
    // make raw texcoords
    if (!pTxcds.empty())
    {
        rawTxcdCount = pTxcds.size();
        rawTxcds = new glm::vec2[rawTxcdCount];
        for (i = 0; i < rawTxcdCount; i++)
            rawTxcds[i] = pTxcds[i];
    }

    // ask for all display lists
    if (allDList)
    {
        for (i = 0; i < lodCount; i++)
        {
            DrawLod& lodref = lods[i];
            for (int j = 0; j < lodref.count; j++)
            {
                DrawSet& set = lodref.sets[j];
                set.wantList = true;
            }
        }
        for (i = 0; i < radarCount; i++)
        {
            DrawLod& lodref = radarLods[i];
            for (int j = 0; j < lodref.count; j++)
            {
                DrawSet& set = lodref.sets[j];
                set.wantList = true;
            }
        }
    }

    if (debugLevel >= 4)
    {
        TimeKeeper end = TimeKeeper::getCurrent();
        const float elapsed = float(end - start);
        logDebugMessage(0,"MeshDrawInfo::parse() processed in %f seconds.\n", elapsed);
    }

    return success;
}


/******************************************************************************/

void MeshDrawInfo::print(std::ostream& out, const std::string& indent) const
{
    int i;

    out << indent << "drawInfo" << std::endl;

    // options
    for (i = 0; i < (int)lodOptions.size(); i++)
        out << indent << "  " << lodOptions[i] << std::endl;

    out << indent << "  extents " << extents.mins[0] << " "
        << extents.mins[1] << " "
        << extents.mins[2] << " "
        << extents.maxs[0] << " "
        << extents.maxs[1] << " "
        << extents.maxs[2] << std::endl;
    out << indent << "  sphere " << position[0] << " "
        << position[1] << " "
        << position[2] << " "
        << radius2 << std::endl;

    if (animInfo != NULL)
    {
        const float angvel = animInfo->angvel;
        if (angvel != 0.0f)
            out << indent << "  angvel " << angvel << std::endl;
    }

    // raw vertices
    for (i = 0; i < rawVertCount; i++)
    {
        const auto &v = rawVerts[i];
        out << indent << "  vertex " << v[0] << " " << v[1] << " "
            << v[2] << std::endl;
    }
    // raw normals
    for (i = 0; i < rawNormCount; i++)
    {
        const auto &n = rawNorms[i];
        out << indent << "  normal " << n[0] << " " << n[1] << " "
            << n[2] << std::endl;
    }
    // raw texcoords
    for (i = 0; i < rawTxcdCount; i++)
    {
        const auto &t = rawTxcds[i];
        out << indent << "  texcoord " << t[0] << " " << t[1] << std::endl;
    }

    // corners
    for (i = 0; i < cornerCount; i++)
    {
        const Corner& corner = corners[i];
        out << indent << "  corner " << corner.vertex << " "
            << corner.normal << " "
            << corner.texcoord << std::endl;
    }

    // normal draw sets
    for (i = 0; i < lodCount; i++)
    {
        DrawLod& lod = lods[i];
        out << indent << "  lod  # " << i << std::endl;
        out << indent << "    lengthPerPixel " << lod.lengthPerPixel << std::endl;
        for (int j = 0; j < lod.count; j++)
        {
            DrawSet& set = lod.sets[j];
            out << indent << "    matref ";
            MATERIALMGR.printReference(out, set.material);
            out << std::endl;
            if (set.wantList)
                out << indent << "      dlist" << std::endl;
            out << indent << "      sphere " << set.position[0] << " "
                << set.position[1] << " "
                << set.position[2] << " "
                << set.radius2 << std::endl;
            const int cmdCount = set.count;
            for (int k = 0; k < cmdCount; k++)
            {
                const DrawCmd& command = set.cmds[k];
                unsigned int mode = command.drawMode;
                if (mode < DrawCmd::DrawModeCount)
                {
                    out << indent << "      " << drawLabels[mode].name;
                    for (int e = 0; e < command.count; e++)
                    {
                        if (command.indexType == DrawCmd::DrawIndexUShort)
                        {
                            unsigned short* data = (unsigned short*) command.indices;
                            out << " " << data[e];
                        }
                        else if (command.indexType == DrawCmd::DrawIndexUInt)
                        {
                            unsigned int* data = (unsigned int*) command.indices;
                            out << " " << data[e];
                        }
                    }
                    out << std::endl;
                }
            }
            out << indent << "    end  # material ";
            MATERIALMGR.printReference(out, set.material);
            out << std::endl;
        }
        out << indent << "  end  # lod " << i << std::endl;
    }

    // radar draw sets
    for (i = 0; i < radarCount; i++)
    {
        DrawLod& lod = radarLods[i];
        out << indent << "  radarlod  # " << i << std::endl;
        out << indent << "    lengthPerPixel " << lod.lengthPerPixel << std::endl;
        for (int j = 0; j < lod.count; j++)
        {
            DrawSet& set = lod.sets[j];
            out << indent << "    material ";
            MATERIALMGR.printReference(out, set.material);
            out << std::endl;
            if (set.wantList)
                out << indent << "      dlist" << std::endl;
            const int cmdCount = set.count;
            for (int k = 0; k < cmdCount; k++)
            {
                const DrawCmd& command = set.cmds[k];
                unsigned int mode = command.drawMode;
                if (mode < DrawCmd::DrawModeCount)
                {
                    out << indent << "      " << drawLabels[mode].name;
                    for (int e = 0; e < command.count; e++)
                    {
                        if (command.indexType == DrawCmd::DrawIndexUShort)
                        {
                            unsigned short* data = (unsigned short*) command.indices;
                            out << " " << data[e];
                        }
                        else if (command.indexType == DrawCmd::DrawIndexUInt)
                        {
                            unsigned int* data = (unsigned int*) command.indices;
                            out << " " << data[e];
                        }
                    }
                    out << std::endl;
                }
            }
            out << indent << "    end  # material ";
            MATERIALMGR.printReference(out, set.material);
            out << std::endl;
        }
        out << indent << "  end  # lod " << i << std::endl;
    }

    out << indent << "end  # drawInfo" << std::endl;

    return;
}


/******************************************************************************/

int MeshDrawInfo::packSize() const
{
    int i;
    int fullSize = 0;

    // name
    fullSize += nboStdStringPackSize(name);

    // options
    fullSize += sizeof(int32_t); // count
    for (i = 0; i < (int)lodOptions.size(); i++)
        fullSize += nboStdStringPackSize(lodOptions[i]);

    // state bits
    fullSize += sizeof(uint32_t);

    // animation information
    if (animInfo != NULL)
        fullSize += animInfo->packSize();

    // corners
    fullSize += sizeof(int32_t); // count
    for (i = 0; i < cornerCount; i++)
        fullSize += corners[i].packSize();

    // raw vertices
    fullSize += sizeof(int32_t); // count
    fullSize += sizeof(float[3]) * rawVertCount;
    // raw normals
    fullSize += sizeof(int32_t); // count
    fullSize += sizeof(float[3]) * rawNormCount;
    // raw texcoords
    fullSize += sizeof(int32_t); // count
    fullSize += sizeof(float[2]) * rawTxcdCount;

    // lods
    fullSize += sizeof(int32_t); // count
    for (i = 0; i < lodCount; i++)
        fullSize += lods[i].packSize();

    // radar lods
    fullSize += sizeof(int32_t); // count
    for (i = 0; i < radarCount; i++)
        fullSize += radarLods[i].packSize();

    // sphere and extents
    fullSize += (4 + 6) * sizeof(float);

    return fullSize;
}


void* MeshDrawInfo::pack(void* buf) const
{
    int i;

    // name
    buf = nboPackStdString(buf, name);

    // options
    buf = nboPackInt(buf, lodOptions.size());
    for (i = 0; i < (int)lodOptions.size(); i++)
        buf = nboPackStdString(buf, lodOptions[i]);

    // state bits
    const bool haveAnim = (animInfo != NULL);
    uint32_t state = 0;
    state |= haveAnim   ? (1 << 0) : 0;
    buf = nboPackUInt(buf, state);

    // animation information
    if (haveAnim)
        buf = animInfo->pack(buf);

    // corners
    buf = nboPackInt (buf, cornerCount);
    for (i = 0; i < cornerCount; i++)
        buf = corners[i].pack(buf);

    // raw vertices
    buf = nboPackInt (buf, rawVertCount);
    for (i = 0; i < rawVertCount; i++)
        buf = nboPackVector(buf, rawVerts[i]);
    // raw normals
    buf = nboPackInt (buf, rawNormCount);
    for (i = 0; i < rawNormCount; i++)
        buf = nboPackVector(buf, rawNorms[i]);
    // raw texcoords
    buf = nboPackInt (buf, rawTxcdCount);
    for (i = 0; i < rawTxcdCount; i++)
    {
        buf = nboPackFloat(buf, rawTxcds[i][0]);
        buf = nboPackFloat(buf, rawTxcds[i][1]);
    }

    // lods
    buf = nboPackInt (buf, lodCount);
    for (i = 0; i < lodCount; i++)
        buf = lods[i].pack(buf);

    // radar lods
    buf = nboPackInt (buf, radarCount);
    for (i = 0; i < radarCount; i++)
        buf = radarLods[i].pack(buf);

    // sphere and extents
    buf = nboPackVector(buf, position);
    buf = nboPackFloat(buf, radius2);
    buf = nboPackVector(buf, extents.mins);
    buf = nboPackVector(buf, extents.maxs);

    return buf;
}


const void* MeshDrawInfo::unpack(const void* buf)
{
    int i;
    int32_t s32;

    // name
    buf = nboUnpackStdString(buf, name);

    // options
    buf = nboUnpackInt (buf, s32);
    lodOptions.clear();
    for (i = 0; i < s32; i++)
    {
        std::string option;
        buf = nboUnpackStdString(buf, option);
        lodOptions.push_back(option);
    }

    // state bits
    bool haveAnim;
    uint32_t state;
    buf = nboUnpackUInt(buf, state);
    haveAnim   = (state & (1 << 0)) != 0;

    // animation information
    if (haveAnim)
    {
        animInfo = new AnimationInfo;
        buf = animInfo->unpack(buf);
    }

    // corners
    buf = nboUnpackInt (buf, s32);
    cornerCount = s32;
    corners = new Corner[cornerCount];
    for (i = 0; i < cornerCount; i++)
        buf = corners[i].unpack(buf);

    // raw vertices
    buf = nboUnpackInt (buf, s32);
    rawVertCount = s32;
    rawVerts = new glm::vec3[rawVertCount];
    for (i = 0; i < rawVertCount; i++)
        buf = nboUnpackVector(buf, rawVerts[i]);
    // raw normals
    buf = nboUnpackInt (buf, s32);
    rawNormCount = s32;
    rawNorms = new glm::vec3[rawNormCount];
    for (i = 0; i < rawNormCount; i++)
        buf = nboUnpackVector(buf, rawNorms[i]);
    // raw texcoords
    buf = nboUnpackInt (buf, s32);
    rawTxcdCount = s32;
    rawTxcds = new glm::vec2[rawTxcdCount];
    for (i = 0; i < rawTxcdCount; i++)
    {
        buf = nboUnpackFloat(buf, rawTxcds[i][0]);
        buf = nboUnpackFloat(buf, rawTxcds[i][1]);
    }

    // lods
    buf = nboUnpackInt (buf, s32);
    lodCount = s32;
    lods = new DrawLod[lodCount];
    for (i = 0; i < lodCount; i++)
        buf = lods[i].unpack(buf);

    // radar lods
    buf = nboUnpackInt (buf, s32);
    radarCount = s32;
    radarLods = new DrawLod[radarCount];
    for (i = 0; i < radarCount; i++)
        buf = radarLods[i].unpack(buf);

    // sphere and extents
    buf = nboUnpackVector(buf, position);
    buf = nboUnpackFloat(buf, radius2);
    buf = nboUnpackVector(buf, extents.mins);
    buf = nboUnpackVector(buf, extents.maxs);

    return buf;
}


/******************************************************************************/

Corner::Corner()
{
    vertex = normal = texcoord = -1;
    return;
}


Corner::~Corner()
{
    return;
}


int Corner::packSize() const
{
    if ((vertex > MaxUShort) || (vertex < 0) ||
            (normal > MaxUShort) || (normal < 0) ||
            (texcoord > MaxUShort) || (texcoord < 0))
        return sizeof(uint8_t) + (3 * sizeof(int32_t));
    else
        return sizeof(uint8_t) + (3 * sizeof(int16_t));
}


void* Corner::pack(void* buf) const
{
    if ((vertex > MaxUShort) || (vertex < 0) ||
            (normal > MaxUShort) || (normal < 0) ||
            (texcoord > MaxUShort) || (texcoord < 0))
    {
        buf = nboPackUByte(buf, 0);
        buf = nboPackInt(buf, vertex);
        buf = nboPackInt(buf, normal);
        buf = nboPackInt(buf, texcoord);
    }
    else
    {
        buf = nboPackUByte(buf, 1);
        buf = nboPackUShort(buf, vertex);
        buf = nboPackUShort(buf, normal);
        buf = nboPackUShort(buf, texcoord);
    }
    return buf;
}


const void* Corner::unpack(const void* buf)
{
    uint8_t u8;
    buf = nboUnpackUByte(buf, u8);
    if (u8 == 0)
    {
        int32_t s32;
        buf = nboUnpackInt(buf, s32);
        vertex = s32;
        buf = nboUnpackInt(buf, s32);
        normal = s32;
        buf = nboUnpackInt(buf, s32);
        texcoord = s32;
    }
    else
    {
        uint16_t u16;
        buf = nboUnpackUShort(buf, u16);
        vertex = u16;
        buf = nboUnpackUShort(buf, u16);
        normal = u16;
        buf = nboUnpackUShort(buf, u16);
        texcoord = u16;
    }
    return buf;
}


/******************************************************************************/

DrawCmd::DrawCmd()
{
    drawMode = DrawTriangleStrip;
    count = 0;
    indices = NULL;
    indexType = DrawIndexUInt;
    return;
}


void DrawCmd::finalize()
{
    if (indexType == DrawIndexUShort)
    {
        return; // safety
    }

    int i;
    const unsigned int* tmp = (unsigned int*)indices;

    // setup the minimum and maximum indices
    minIndex = 0xFFFFFFFF;
    maxIndex = 0;
    for (i = 0; i < count; i++)
    {
        const unsigned int value = tmp[i];
        if (value < minIndex)
            minIndex = value;
        if (value > maxIndex)
            maxIndex = value;
    }

    // check if they can be convert to unsigned shorts
    for (i = 0; i < count; i++)
    {
        if (tmp[i] > (unsigned int)MaxUShort)
        {
            return; // leave them as unsigned ints
        }
    }

    // convert to unsigned shorts
    unsigned short* shortArray = new unsigned short[count];
    for (i = 0; i < count; i++)
        shortArray[i] = tmp[i];
    indexType = DrawIndexUShort;
    delete[] (unsigned int*)indices;
    indices = shortArray;

    return;
}


void DrawCmd::clear()
{
    if (indexType == DrawIndexUInt)
        delete[] (unsigned int*)indices;
    else
        delete[] (unsigned short*)indices;
    return;
}


int DrawCmd::packSize() const
{
    int fullSize = 0;
    fullSize += sizeof(uint32_t); // draw mode
    fullSize += sizeof(int32_t); // count
    fullSize += sizeof(uint32_t); // index type
    if (indexType == DrawIndexUShort)
        fullSize += count * sizeof(uint16_t);
    else
        fullSize += count * sizeof(uint32_t);
    return fullSize;
}


void* DrawCmd::pack(void* buf) const
{
    buf = nboPackUInt(buf, drawMode);
    buf = nboPackInt(buf, count);
    buf = nboPackUInt(buf, indexType);
    if (indexType == DrawIndexUShort)
    {
        for (int i = 0; i < count; i++)
        {
            uint16_t tmp = ((unsigned short*)indices)[i];
            buf = nboPackUShort(buf, tmp);
        }
    }
    else
    {
        for (int i = 0; i < count; i++)
        {
            uint32_t tmp = ((unsigned int*)indices)[i];
            buf = nboPackUInt(buf, tmp);
        }
    }
    return buf;
}


const void* DrawCmd::unpack(const void* buf)
{
    uint16_t u16;
    int32_t s32;
    uint32_t u32;

    buf = nboUnpackUInt(buf, u32);
    drawMode = u32;
    buf = nboUnpackInt(buf, s32);
    count = s32;
    buf = nboUnpackUInt(buf, u32);
    indexType = u32;
    if (indexType == DrawIndexUShort)
    {
        indices = new unsigned short[count];
        for (int i = 0; i < count; i++)
        {
            buf = nboUnpackUShort(buf, u16);
            ((unsigned short*)indices)[i] = u16;
        }
    }
    else
    {
        indices = new unsigned int[count];
        for (int i = 0; i < count; i++)
        {
            buf = nboUnpackUInt(buf, u32);
            ((unsigned int*)indices)[i] = u32;
        }
    }
    return buf;
}


/******************************************************************************/

DrawSet::DrawSet()
{
    count = 0;
    cmds = NULL;
    material = NULL;
    wantList = false;
    position = glm::vec3(+MAXFLOAT);
    radius2  = +MAXFLOAT;
    return;
}


void DrawSet::clear()
{
    for (int i = 0; i < count; i++)
        cmds[i].clear();
    delete[] cmds;
    return;
}


int DrawSet::packSize() const
{
    int fullSize = 0;
    fullSize += sizeof(uint32_t); // count
    for (int i = 0; i < count; i++)
        fullSize += cmds[i].packSize();
    fullSize += sizeof(int32_t); // material
    fullSize += sizeof(float[4]); // sphere
    fullSize += sizeof(uint8_t); // state bits

    return fullSize;
}


void* DrawSet::pack(void* buf) const
{
    buf = nboPackInt(buf, count);
    for (int i = 0; i < count; i++)
        buf = cmds[i].pack(buf);

    // material
    int matindex = MATERIALMGR.getIndex(material);
    buf = nboPackInt(buf, matindex);

    // sphere
    buf = nboPackVector(buf, position);
    buf = nboPackFloat(buf, radius2);

    // state bits
    uint8_t state = 0;
    state |= wantList ? (1 << 0) : 0;
    buf = nboPackUByte(buf, state);

    return buf;
}


const void* DrawSet::unpack(const void* buf)
{
    int32_t s32;
    buf = nboUnpackInt(buf, s32);
    count = s32;
    cmds = new DrawCmd[count];
    for (int i = 0; i < count; i++)
        buf = cmds[i].unpack(buf);

    // material
    buf = nboUnpackInt(buf, s32);
    material = MATERIALMGR.getMaterial(s32);

    // sphere
    buf = nboUnpackVector(buf, position);
    buf = nboUnpackFloat(buf, radius2);

    // state bits
    uint8_t state;
    buf = nboUnpackUByte(buf, state);
    wantList = (state & (1 << 0)) != 0;

    return buf;
}


/******************************************************************************/

DrawLod::DrawLod()
{
    count = 0;
    sets = NULL;
    lengthPerPixel = 0.0f;
    return;
}


void DrawLod::clear()
{
    for (int i = 0; i < count; i++)
        sets[i].clear();
    delete[] sets;
    return;
}


int DrawLod::packSize() const
{
    int fullSize = 0;
    fullSize += sizeof(uint32_t); // count
    for (int i = 0; i < count; i++)
        fullSize += sets[i].packSize();
    fullSize += sizeof(float); // lengthPerPixel
    return fullSize;
}


void* DrawLod::pack(void* buf) const
{
    buf = nboPackInt(buf, count);
    for (int i = 0; i < count; i++)
        buf = sets[i].pack(buf);
    buf = nboPackFloat(buf, lengthPerPixel);
    return buf;
}


const void* DrawLod::unpack(const void* buf)
{
    int32_t s32;
    buf = nboUnpackInt(buf, s32);
    count = s32;
    sets = new DrawSet[count];
    for (int i = 0; i < count; i++)
        buf = sets[i].unpack(buf);
    buf = nboUnpackFloat(buf, lengthPerPixel);

    return buf;
}


/******************************************************************************/

AnimationInfo::AnimationInfo()
{
    angle = 0.0f;
    angvel = 0.0f;
    dummy = "";
    return;
}


int AnimationInfo::packSize() const
{
    int fullSize = 0;
    fullSize += sizeof(float); // angvel
    fullSize += nboStdStringPackSize(dummy);
    return fullSize;
}


void* AnimationInfo::pack(void* buf) const
{
    buf = nboPackFloat(buf, angvel);
    buf= nboPackStdString(buf, dummy);
    return buf;
}


const void* AnimationInfo::unpack(const void* buf)
{
    buf = nboUnpackFloat(buf, angvel);
    buf= nboUnpackStdString(buf, dummy);
    return buf;
}


/******************************************************************************/

// Local Variables: ***
// mode: C++ ***
// tab-width: 4 ***
// c-basic-offset: 4 ***
// indent-tabs-mode: nil ***
// End: ***
// ex: shiftwidth=4 tabstop=4
