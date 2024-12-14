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
#include "TeleporterSceneNodeGenerator.h"

// System headers
#include <math.h>

// Common headers
#include "Teleporter.h"
#include "bzfgl.h"
#include "QuadWallSceneNode.h"

//
// TeleporterSceneNodeGenerator
//

TeleporterSceneNodeGenerator::TeleporterSceneNodeGenerator(
    const Teleporter* _teleporter) :
    teleporter(_teleporter)
{
    // do nothing
}

TeleporterSceneNodeGenerator::~TeleporterSceneNodeGenerator()
{
    // do nothing
}

WallSceneNode*      TeleporterSceneNodeGenerator::getNextNode(
    float UNUSED(uRepeats), float UNUSED(vRepeats),
    bool lod)
{
    static const glm::vec2 texCoords[][4] =
    {
        {{ 0.0f, 0.0f }, { 0.5f, 0.0f }, { 0.5f, 9.5f }, { 0.0f, 9.5f }},
        {{ 0.5f, 0.0f }, { 1.0f, 0.0f }, { 1.0f, 9.5f }, { 0.5f, 9.5f }},
        {{ 0.0f, 0.0f }, { 0.5f, 0.0f }, { 0.5f, 9.0f }, { 0.0f, 9.0f }},
        {{ 0.5f, 0.0f }, { 1.0f, 0.0f }, { 1.0f, 9.0f }, { 0.5f, 9.0f }},
        {{ 0.5f, 0.0f }, { 1.0f, 0.0f }, { 1.0f, 9.0f }, { 0.5f, 9.0f }},
        {{ 0.0f, 0.0f }, { 0.5f, 0.0f }, { 0.5f, 9.0f }, { 0.0f, 9.0f }},
        {{ 0.5f, 0.0f }, { 1.0f, 0.0f }, { 1.0f, 9.0f }, { 0.5f, 9.0f }},
        {{ 0.0f, 0.0f }, { 0.5f, 0.0f }, { 0.5f, 9.0f }, { 0.0f, 9.0f }},
        {{ 0.0f, 0.0f }, { 0.0f, 0.0f }, { 0.5f, 5.0f }, { 0.5f, 5.0f }},
        {{ 0.0f, 0.0f }, { 0.0f, 0.0f }, { 0.5f, 4.0f }, { 0.5f, 4.0f }},
        {{ 0.0f, 0.0f }, { 5.0f, 0.0f }, { 5.0f, 0.5f }, { 0.0f, 0.5f }},
        {{ 0.0f, 0.5f }, { 5.0f, 0.5f }, { 5.0f, 1.0f }, { 0.0f, 1.0f }}
    };

    if (fabsf(teleporter->getBorder()) < 1.0e-6f)
        return NULL;

    if (teleporter->isHorizontal ())
    {
        if (getNodeNumber () >= 16)
            return NULL;

        glm::vec3 sEdge;
        glm::vec3 tEdge;
        const auto &pos = teleporter->getPosition ();
        const auto &size = teleporter->getSize ();
        const float c = cosf (teleporter->getRotation ());
        const float s = sinf (teleporter->getRotation ());
        const float w = teleporter->getWidth ();
        const float d = teleporter->getBreadth ();
        const float b = teleporter->getBorder ();

        // NOTE -- 1,2,3,4: outer sides
        // 3,4,5,6: inner sides
        // 7,8,9,10: bottom sides
        // 11,12,13,14: top sides
        // 15,16 top and bottom teleport faces

        auto base = pos;
        const int n = incNodeNumber ();
        switch (n)
        {
        case 1:           // -x outside edge
            base[0] += (-w) * c - (-d) * s;
            base[1] += (-w) * s + (-d) * c;
            base[2] += size[2] - b;
            sEdge[0] = 0.0f;
            sEdge[1] = 0.0f;
            sEdge[2] = b;
            tEdge[0] = -s * (2.0f * d);
            tEdge[1] = c * (2.0f * d);
            tEdge[2] = 0.0f;
            break;
        case 2:           // -y outside edge
            base[0] += (-w) * c - (-d) * s;
            base[1] += (-w) * s + (-d) * c;
            base[2] += size[2] - b;
            sEdge[0] = (2.0f * w) * c;
            sEdge[1] = (2.0f * w) * s;
            sEdge[2] = 0.0f;
            tEdge[0] = 0.0f;
            tEdge[1] = 0.0f;
            tEdge[2] = b;
            break;
        case 3:           // +x outside edge
            base[0] += (w) * c - (d) * s;
            base[1] += (w) * s + (d) * c;
            base[2] += size[2] - b;
            sEdge[0] = 0.0f;
            sEdge[1] = 0.0f;
            sEdge[2] = b;
            tEdge[0] = -(-2.0f * d) * s;
            tEdge[1] = (-2.0f * d) * c;
            tEdge[2] = 0.0f;
            break;
        case 4:           // +y outside edge
            base[0] += (w) * c - (d) * s;
            base[1] += (w) * s + (d) * c;
            base[2] += size[2] - b;
            sEdge[0] = (-2.0f * w) * c;
            sEdge[1] = (-2.0f * w) * s;
            sEdge[2] = 0.0f;
            tEdge[0] = 0.0f;
            tEdge[1] = 0.0f;
            tEdge[2] = b;
            break;
        case 5:           // -x inner edge
            base[0] += (b - w) * c - (b - d) * s;
            base[1] += (b - w) * s + (b - d) * c;
            base[2] += size[2] - b;
            sEdge[0] = -s * (2.0f * (d - b));
            sEdge[1] = c * (2.0f * (d - b));
            sEdge[2] = 0.0f;
            tEdge[0] = 0.0f;
            tEdge[1] = 0.0f;
            tEdge[2] = b;
            break;
        case 6:           // -y inner edge
            base[0] += (b - w) * c - (b - d) * s;
            base[1] += (b - w) * s + (b - d) * c;
            base[2] += size[2] - b;
            sEdge[0] = 0.0f;
            sEdge[1] = 0.0f;
            sEdge[2] = b;
            tEdge[0] = (2.0f * (w - b)) * c;
            tEdge[1] = (2.0f * (w - b)) * s;
            tEdge[2] = 0.0f;
            break;
        case 7:           // +x inner edge
            base[0] += (w - b) * c - (d - b) * s;
            base[1] += (w - b) * s + (d - b) * c;
            base[2] += size[2] - b;
            sEdge[0] = -(-2.0f * (d - b)) * s;
            sEdge[1] = (-2.0f * (d - b)) * c;
            sEdge[2] = 0.0f;
            tEdge[0] = 0.0f;
            tEdge[1] = 0.0f;
            tEdge[2] = b;
            break;
        case 8:           // +y inner edge
            base[0] += (w - b) * c - (d - b) * s;
            base[1] += (w - b) * s + (d - b) * c;
            base[2] += size[2] - b;
            sEdge[0] = 0.0f;
            sEdge[1] = 0.0f;
            sEdge[2] = b;
            tEdge[0] = (-2.0f * (w - b)) * c;
            tEdge[1] = (-2.0f * (w - b)) * s;
            tEdge[2] = 0.0f;
            break;
        case 9:           // -x bottom edge
            base[0] += (-w) * c - (-d) * s;
            base[1] += (-w) * s + (-d) * c;
            base[2] += size[2] - b;
            sEdge[0] = -s * (2.0f * d);
            sEdge[1] = c * (2.0f * d);
            sEdge[2] = 0.0f;
            tEdge[0] = (b) * c;
            tEdge[1] = (b) * s;
            tEdge[2] = 0.0f;
            break;
        case 10:         // -y bottom edge
            base[0] += (-w) * c - (-d) * s;
            base[1] += (-w) * s + (-d) * c;
            base[2] += size[2] - b;
            sEdge[0] = -s * (b);
            sEdge[1] = c * (b);
            sEdge[2] = 0.0f;
            tEdge[0] = c * (2.0f * w);
            tEdge[1] = s * (2.0f * w);
            tEdge[2] = 0.0f;
            break;
        case 11:         // +x bottom edge
            base[0] += (w) * c - (d) * s;
            base[1] += (w) * s + (d) * c;
            base[2] += size[2] - b;
            sEdge[0] = -(-2.0f * (d)) * s;
            sEdge[1] = (-2.0f * (d)) * c;
            sEdge[2] = 0.0f;
            tEdge[0] = c * (-b);
            tEdge[1] = s * (-b);
            tEdge[2] = 0.0f;
            break;
        case 12:         // +y bottom edge
            base[0] += (w) * c - (d) * s;
            base[1] += (w) * s + (d) * c;
            base[2] += size[2] - b;
            sEdge[0] = -s * (-b);
            sEdge[1] = c * (-b);
            sEdge[2] = 0.0f;
            tEdge[0] = (-2.0f * (w)) * c;
            tEdge[1] = (-2.0f * (w)) * s;
            tEdge[2] = 0.0f;
            break;
        case 13:         // -x top edge
            base[0] += (-w) * c - (-d) * s;
            base[1] += (-w) * s + (-d) * c;
            base[2] += size[2];
            sEdge[0] = c * (b);
            sEdge[1] = s * (b);
            sEdge[2] = 0.0f;
            tEdge[0] = -s * (2.0f * (d));
            tEdge[1] = c * (2.0f * (d));
            tEdge[2] = 0.0f;
            break;
        case 14:         // -y top edge
            base[0] += (-w) * c - (-d) * s;
            base[1] += (-w) * s + (-d) * c;
            base[2] += size[2];
            sEdge[0] = c * (2.0f * (w));
            sEdge[1] = s * (2.0f * (w));
            sEdge[2] = 0.0f;
            tEdge[0] = -s * (b);
            tEdge[1] = c * (b);
            tEdge[2] = 0.0f;
            break;
        case 15:         // +x top edge
            base[0] += (w) * c - (d) * s;
            base[1] += (w) * s + (d) * c;
            base[2] += size[2];
            sEdge[0] = c * (-b);
            sEdge[1] = s * (-b);
            sEdge[2] = 0.0f;
            tEdge[0] = -s * (-2.0f * (d));
            tEdge[1] = c * (-2.0f * (d));
            tEdge[2] = 0.0f;
            break;
        case 16:         // +y top edge
            base[0] += (w) * c - (d) * s;
            base[1] += (w) * s + (d) * c;
            base[2] += size[2];
            sEdge[0] = c * (2.0f * (-w));
            sEdge[1] = s * (2.0f * (-w));
            sEdge[2] = 0.0f;
            tEdge[0] = -s * (-b);
            tEdge[1] = c * (-b);
            tEdge[2] = 0.0f;
            break;
        }

        glm::vec2 uv;
        float uc, vc;
        if (n >= 1 && n <= 16)
        {
            uv = texCoords[0][0];
            uc = texCoords[0][1][0] - uv[0];
            vc = texCoords[0][3][1] - uv[1];
        }
        else
        {
            uv = glm::vec2(0.0f);
            uc = vc = 1.0f;
        }
        return new QuadWallSceneNode (base, sEdge, tEdge, uv, uc, vc, lod);
    }
    else
    {
        if (getNodeNumber () >= 12)
            return NULL;

        glm::vec3 sEdge;
        glm::vec3 tEdge;
        const auto &pos = teleporter->getPosition ();
        const float c = cosf (teleporter->getRotation ());
        const float s = sinf (teleporter->getRotation ());
        const float h = teleporter->getBreadth () - teleporter->getBorder ();
        const float b = 0.5f * teleporter->getBorder ();
        const float d = h + b;
        const float z = teleporter->getHeight () - teleporter->getBorder ();
        GLfloat x[2], y[2];
        x[0] = c;
        x[1] = s;
        y[0] = -s;
        y[1] = c;
        // NOTE -- 1,2: outer sides
        // 3,4: inner sides
        // 5-8: front and back vertical sides
        // 9,10: horizontal bar top and bottom
        // 11,12: front and back horizontal sides
        // 13,14: front and back teleport faces
        auto base = pos;
        const int n = incNodeNumber ();
        switch (n)
        {
        case 1:
            base[0] += + d * y[0] + b * x[0] + b * y[0];
            base[1] += + d * y[1] + b * x[1] + b * y[1];
            sEdge[0] = -2.0f * b * x[0];
            sEdge[1] = -2.0f * b * x[1];
            sEdge[2] = 0.0f;
            tEdge[0] = 0.0f;
            tEdge[1] = 0.0f;
            tEdge[2] = z + 2.0f * b;
            break;
        case 2:
            base[0] += - d * y[0] - b * x[0] - b * y[0];
            base[1] += - d * y[1] - b * x[1] - b * y[1];
            sEdge[0] = 2.0f * b * x[0];
            sEdge[1] = 2.0f * b * x[1];
            sEdge[2] = 0.0f;
            tEdge[0] = 0.0f;
            tEdge[1] = 0.0f;
            tEdge[2] = z + 2.0f * b;
            break;
        case 3:
            base[0] += + d * y[0] - b * x[0] - b * y[0];
            base[1] += + d * y[1] - b * x[1] - b * y[1];
            sEdge[0] = 2.0f * b * x[0];
            sEdge[1] = 2.0f * b * x[1];
            sEdge[2] = 0.0f;
            tEdge[0] = 0.0f;
            tEdge[1] = 0.0f;
            tEdge[2] = z;
            break;
        case 4:
            base[0] += - d * y[0] + b * x[0] + b * y[0];
            base[1] += - d * y[1] + b * x[1] + b * y[1];
            sEdge[0] = -2.0f * b * x[0];
            sEdge[1] = -2.0f * b * x[1];
            sEdge[2] = 0.0f;
            tEdge[0] = 0.0f;
            tEdge[1] = 0.0f;
            tEdge[2] = z;
            break;
        case 5:
            base[0] += + d * y[0] + b * x[0] - b * y[0];
            base[1] += + d * y[1] + b * x[1] - b * y[1];
            sEdge[0] = 2.0f * b * y[0];
            sEdge[1] = 2.0f * b * y[1];
            sEdge[2] = 0.0f;
            tEdge[0] = 0.0f;
            tEdge[1] = 0.0f;
            tEdge[2] = z;
            break;
        case 6:
            base[0] += - d * y[0] - b * x[0] + b * y[0];
            base[1] += - d * y[1] - b * x[1] + b * y[1];
            sEdge[0] = -2.0f * b * y[0];
            sEdge[1] = -2.0f * b * y[1];
            sEdge[2] = 0.0f;
            tEdge[0] = 0.0f;
            tEdge[1] = 0.0f;
            tEdge[2] = z;
            break;
        case 7:
            base[0] += + d * y[0] - b * x[0] + b * y[0];
            base[1] += + d * y[1] - b * x[1] + b * y[1];
            sEdge[0] = -2.0f * b * y[0];
            sEdge[1] = -2.0f * b * y[1];
            sEdge[2] = 0.0f;
            tEdge[0] = 0.0f;
            tEdge[1] = 0.0f;
            tEdge[2] = z;
            break;
        case 8:
            base[0] += - d * y[0] + b * x[0] - b * y[0];
            base[1] += - d * y[1] + b * x[1] - b * y[1];
            sEdge[0] = 2.0f * b * y[0];
            sEdge[1] = 2.0f * b * y[1];
            sEdge[2] = 0.0f;
            tEdge[0] = 0.0f;
            tEdge[1] = 0.0f;
            tEdge[2] = z;
            break;
        case 9:
            base[0] += - d * y[0] - b * x[0] - b * y[0];
            base[1] += - d * y[1] - b * x[1] - b * y[1];
            base[2] += + z + 2.0f * b;
            sEdge[0] = 2.0f * b * x[0];
            sEdge[1] = 2.0f * b * x[1];
            sEdge[2] = 0.0f;
            tEdge[0] = 2.0f * (d + b) * y[0];
            tEdge[1] = 2.0f * (d + b) * y[1];
            tEdge[2] = 0.0f;
            break;
        case 10:
            base[0] += - d * y[0] + b * x[0] + b * y[0];
            base[1] += - d * y[1] + b * x[1] + b * y[1];
            base[2] += + z;
            sEdge[0] = -2.0f * b * x[0];
            sEdge[1] = -2.0f * b * x[1];
            sEdge[2] = 0.0f;
            tEdge[0] = 2.0f * (d - b) * y[0];
            tEdge[1] = 2.0f * (d - b) * y[1];
            tEdge[2] = 0.0f;
            break;
        case 11:
            base[0] += - d * y[0] + b * x[0] - b * y[0];
            base[1] += - d * y[1] + b * x[1] - b * y[1];
            base[2] += + z;
            sEdge[0] = 2.0f * (d + b) * y[0];
            sEdge[1] = 2.0f * (d + b) * y[1];
            sEdge[2] = 0.0f;
            tEdge[0] = 0.0f;
            tEdge[1] = 0.0f;
            tEdge[2] = 2.0f * b;
            break;
        case 12:
            base[0] += + d * y[0] - b * x[0] + b * y[0];
            base[1] += + d * y[1] - b * x[1] + b * y[1];
            base[2] += + z;
            sEdge[0] = -2.0f * (d + b) * y[0];
            sEdge[1] = -2.0f * (d + b) * y[1];
            sEdge[2] = 0.0f;
            tEdge[0] = 0.0f;
            tEdge[1] = 0.0f;
            tEdge[2] = 2.0f * b;
            break;
        }
        glm::vec2 uv;
        float uc, vc;
        if (n >= 1 && n <= 12)
        {
            uv = texCoords[n - 1][0];
            uc = texCoords[n - 1][1][0] - uv[0];
            vc = texCoords[n - 1][3][1] - uv[1];
        }
        else
        {
            uv = glm::vec2(0.0f);
            uc = vc = 1.0f;
        }
        return new QuadWallSceneNode (base, sEdge, tEdge, uv, uc, vc, lod);
    }
}

// Local Variables: ***
// mode: C++ ***
// tab-width: 4 ***
// c-basic-offset: 4 ***
// indent-tabs-mode: nil ***
// End: ***
// ex: shiftwidth=4 tabstop=4
