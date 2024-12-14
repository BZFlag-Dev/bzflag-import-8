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
#include "EighthDimShellNode.h"

// system headers
#include <stdlib.h>

// common headers
#include "SceneRenderer.h"
#include "OpenGLGState.h"
#include "BZDBCache.h"


EighthDimShellNode::EighthDimShellNode(SceneNode* node, bool _ownTheNode)
{
    sceneNode = node;
    ownTheNode = _ownTheNode;

    makeNodes();

    return;
}


EighthDimShellNode::~EighthDimShellNode()
{
    if (ownTheNode)
        delete sceneNode;
    killNodes();
    return;
}

void EighthDimShellNode::makeNodes()
{
    std::vector<RenderSet> rnodes;
    sceneNode->getRenderNodes(rnodes);

    shellNodeCount = (int)rnodes.size();
    if (shellNodeCount <= 0)
    {
        shellNodeCount = 0;
        shellNodes = NULL;
        return;
    }

    shellNodes = new ShellRenderNode*[shellNodeCount];
    for (int i = 0; i < shellNodeCount; i++)
        shellNodes[i] = new ShellRenderNode(rnodes[i].node, rnodes[i].gstate);

    return;
}


void EighthDimShellNode::killNodes()
{
    for (int i = 0; i < shellNodeCount; i++)
        delete shellNodes[i];
    delete[] shellNodes;
    shellNodes = NULL;
    shellNodeCount = 0;
    return;
}


void EighthDimShellNode::notifyStyleChange()
{
    killNodes();
    makeNodes();
    return;
}


bool EighthDimShellNode::cull(const ViewFrustum&) const
{
    // no culling
    return false;
}


void EighthDimShellNode::addRenderNodes(SceneRenderer& renderer)
{
    for (int i = 0; i < shellNodeCount; i++)
        renderer.addRenderNode(shellNodes[i], shellNodes[i]->getGState());
    return;
}


//
// EighthDimShellNode::ShellRenderNode
//

EighthDimShellNode::ShellRenderNode::ShellRenderNode(RenderNode *node,
        const OpenGLGState* gs)

{
    renderNode = node;

    OpenGLGStateBuilder gb = *gs;
    if (BZDBCache::blend && (RENDERER.useQuality() >= 2))
        gb.setBlending(GL_ONE, GL_ONE);
    else
        gb.resetBlending();
    gb.setCulling(GL_FRONT); // invert the culling
    gstate = gb.getState(); // get the modified gstate

    return;
}


EighthDimShellNode::ShellRenderNode::~ShellRenderNode()
{
    return;
}

const glm::vec3 &EighthDimShellNode::ShellRenderNode::getPosition() const
{
    return renderNode->getPosition();
}

const OpenGLGState* EighthDimShellNode::ShellRenderNode::getGState() const
{
    return &gstate;
}


void EighthDimShellNode::ShellRenderNode::render()
{
//  glLogicOp(GL_XOR);
//  glEnable(GL_COLOR_LOGIC_OP);
//  glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_FALSE);

    if (BZDBCache::blend && RENDERER.useQuality() >= 2)
        renderNode->render();

    // workaround for an issue on macOS M1 systems where setting the polygon mode to only back faces causes a fallback
    // to software fragment processing due to "polygon mode mismatch," resulting in a massive loss of framerate
    // FIXME: remove this workaround if Apple ever corrects this issue in their drivers
#if defined(__APPLE__) && defined(__aarch64__)
    glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
#else
    glPolygonMode(GL_BACK, GL_LINE);
#endif
    glLineWidth(3.0f);

    renderNode->render();

    glLineWidth(1.0f);
    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

//  glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
//  glDisable(GL_COLOR_LOGIC_OP);

    return;
}

void EighthDimShellNode::ShellRenderNode::renderShadow()
{
}


// Local Variables: ***
// mode: C++ ***
// tab-width: 4 ***
// c-basic-offset: 4 ***
// indent-tabs-mode: nil ***
// End: ***
// ex: shiftwidth=4 tabstop=4
