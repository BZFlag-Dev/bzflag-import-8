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
 * HUDuiLabel:
 *  User interface class for the heads-up display's label (text display) control
 */

#ifndef __HUDUILABEL_H__
#define __HUDUILABEL_H__

// ancestor class
#include "HUDuiControl.h"

// System headers
#include <string>
#include <vector>
#include <glm/vec3.hpp>

#include "BzfEvent.h"

class HUDuiLabel : public HUDuiControl
{
public:
    HUDuiLabel();
    ~HUDuiLabel();

    std::string     getString() const;
    void        setString(const std::string&, const std::vector<std::string> *_params = NULL);
    void        setDarker(bool d); // render darker than usual when not in focus
    void        setColor(GLfloat r, GLfloat g, GLfloat b);

protected:
    void        onSetFont();
    bool        doKeyPress(const BzfKeyEvent&);
    bool        doKeyRelease(const BzfKeyEvent&);
    void        doRender();

private:
    std::string     string;
    std::vector<std::string> *params;
    bool        darker;
    glm::vec3   color;
};

#endif // __HUDUILABEL_H__

// Local Variables: ***
// mode: C++ ***
// tab-width: 4 ***
// c-basic-offset: 4 ***
// indent-tabs-mode: nil ***
// End: ***
// ex: shiftwidth=4 tabstop=4
