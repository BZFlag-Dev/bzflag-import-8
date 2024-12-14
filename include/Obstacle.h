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

/* Obstacle:
 *  Interface for all obstacles in the game environment,
 *  including boxes, pyramids, and teleporters.
 *
 * isInside(const float*, float) is a rough test that considers
 *  the tank as a circle
 * isInside(const float*, float, float, float) is a careful test
 *  that considers the tank as a rectangle
 */

#ifndef BZF_OBSTACLE_H
#define BZF_OBSTACLE_H

#include "common.h"

// system headers
#include <string>
#include <iostream>
#include <glm/fwd.hpp>

// common headers
#include "Extents.h"

class Ray;
class SceneNode;
class MeshTransform;

/** This ABC represents a (normally) solid object in a world. It has pure
    virtual functions for getting information about it's size, checking ray
    intersections, checking point intersections, computing normals etc.
    All these functions have to be implemented in concrete subclasses.
*/

class Obstacle
{

    friend class ObstacleModifier;

public:

    /** The default constructor. It sets all values to 0
        and is not very useful. */
    Obstacle();

    /** This function initializes the Obstacle with the given parameters.
        @param pos    The position of the obstacle in world coordinates
        @param rotation   The rotation around the obstacle's Z axis
        @param hwidth     Half the X size of the obstacle
        @param hbreadth   Half the Y size of the obstacle
        @param height     The Z size of the obstacle
        @param drive      @c true if the obstacle is drivethrough, i.e. tanks
          can pass through it
        @param shoot      @c true if the obstacle is shootthrough, i.e. bullets
          can pass through it
    */
    Obstacle(const glm::vec3 &pos, float rotation, float hwidth, float hbreadth,
             float height, bool drive = false, bool shoot = false, bool rico = false);

    /** This function makes a copy using the given transform */
    virtual Obstacle* copyWithTransform(const MeshTransform&) const;

    /** A virtual destructor is needed to let subclasses do their cleanup. */
    virtual ~Obstacle();

    /** This function returns a string describing what kind of obstacle this is.
     */
    virtual const char* getType() const = 0;

    /** This function calculates extents from pos, size, and rotation */
    void setExtents();

    /** This function returns true if the obstacle is valid */
    virtual bool isValid() const;

    /** This function returns true if the obstacle has a flat top */
    virtual bool isFlatTop() const;

    /** TThis function returns the network packed size in bytes */
    virtual int packSize() const = 0;

    /** This function packs the obstacle into buf */
    virtual void *pack(void* buf) const = 0;

    /** This function unpacks the obstacle from buf */
    virtual const void *unpack(const void* buf) = 0;

    /** This function prints the obstacle to the stream */
    virtual void print(std::ostream& out, const std::string& indent) const = 0;

    /** This function prints the obstacle in Alias Wavefront format to the stream */
    virtual void printOBJ(std::ostream&, const std::string&) const
    {
        return;
    }

    /** This function returns the position of this obstacle. */
    const Extents& getExtents() const;

    /** This function returns the position of this obstacle. */
    const glm::vec3 &getPosition() const;

    /** This function returns the sizes of this obstacle. */
    const glm::vec3 &getSize() const;

    /** This function returns the obstacle's rotation around its own Y axis. */
    float getRotation() const;

    /** This function returns half the obstacle's X size. */
    float getWidth() const;

    /** This function returns half the obstacle's Y size. */
    float getBreadth() const;

    /** This function returns the obstacle's full height. */
    float getHeight() const;

    /** This function returns the time of intersection between the obstacle
        and a Ray object. If the ray does not intersect this obstacle -1 is
        returned. */
    virtual float intersect(const Ray&) const = 0;

    /** This function computes the two-dimensional surface normal of this
        obstacle at the point @c p. The normal is stored in @c n. */
    virtual void getNormal(const glm::vec3 &p, glm::vec3 &n) const = 0;

    /** This function computes the three-dimensional surface normal of this
        obstacle at the point @c p. The normal is stored in @c n. */
    virtual void get3DNormal(const glm::vec3 &p, glm::vec3 &n) const;

    /** This function checks if a tank, approximated as a cylinder with base
        centre in point @c p and radius @c radius, intersects this obstacle. */
    virtual bool inCylinder(const glm::vec3 &p, float radius, float height) const = 0;

    /** This function checks if a tank, approximated as a box rotated around its
        Z axis, intersects this obstacle. */
    virtual bool inBox(const glm::vec3 &p, float angle,
                       float halfWidth, float halfBreadth, float height) const;

    /** This function checks if a tank, approximated as a box rotated around its
        Z axis, intersects this obstacle. It also factors in the difference
        between the old Z location and the new Z location */
    virtual bool inMovingBox(const glm::vec3 &oldP, float oldAngle,
                             const glm::vec3 &newP, float newAngle,
                             float halfWidth, float halfBreadth, float height) const;

    /** This function checks if a horizontal rectangle crosses the surface of
        this obstacle.
        @param p       The position of the centre of the rectangle
        @param angle   The rotation of the rectangle
        @param halfWidth   Half the width of the rectangle
        @param halfBreadth Half the breadth of the rectangle
        @param plane   The tangent plane of the obstacle where it's
           intersected by the rectangle will be stored here
    */
    virtual bool isCrossing(const glm::vec3 &p, float angle,
                            float halfWidth, float halfBreadth, float height,
                            glm::vec4 *plane) const;

    /** This function checks if a box moving from @c pos1 to @c pos2 will hit
        this obstacle, and if it does what the surface normal at the hitpoint is.
        @param pos1    The original position of the box
        @param azimuth1    The original rotation of the box
        @param pos2    The position of the box at the hit
        @param azimuth2    The rotation of the box at the hit
        @param halfWidth   Half the width of the box
        @param halfBreadth Half the breadth of the box
        @param height  The height of the box
        @param normal  The surface normal of this obstacle at the hit point
           will be stored here
        @returns      @c true if the box hits this obstacle, @c false
           otherwise
    */
    virtual bool getHitNormal(const glm::vec3 &pos1, float azimuth1,
                              const glm::vec3 &pos2, float azimuth2,
                              float halfWidth, float halfBreadth,
                              float height, glm::vec3 &normal) const;

    /** This function returns @c true if tanks can pass through this object,
        @c false if they can't. */
    bool isDriveThrough() const;

    /** This function returns @c true if bullets can pass through this object,
        @c false if they can't. */
    bool isShootThrough() const;

    /** This function returns @c true if tanks and bullets can pass through
        this object, @c false if either can not */
    bool isPassable() const;

    /** This function returns @c true if bullets will bounce off of this
     * object, @c false if they simply die of contact */
    bool canRicochet() const;

    /** This function sets the "zFlip" flag of this obstacle, i.e. if it's
        upside down. */
    void setZFlip(void);


    /** This function returns the "zFlip" flag of this obstacle.
        @see setZFlip()
    */
    bool getZFlip(void) const;

    // where did the object come from?
    enum SourceBits
    {
        WorldSource     = 0,
        GroupDefSource  = (1 << 0),
        ContainerSource = (1 << 1)
    };
    void setSource(char);
    char getSource() const;
    bool isFromWorldFile() const;
    bool isFromGroupDef() const;
    bool isFromContainer() const;

    /** This function resets the object ID counter for printing OBJ files */
    static void resetObjCounter();

    // inside sceneNodes
    void addInsideSceneNode(SceneNode* node);
    void freeInsideSceneNodeList();
    int getInsideSceneNodeCount() const;
    SceneNode** getInsideSceneNodeList() const;

    /** This boolean is used by CollisionManager.
        Someone else can 'friend'ify it later.
    */
    bool collisionState;

    /** The maximum extent of any object parameter
     */
    static const float maxExtent;



protected:
    /** This function checks if a moving horizontal rectangle will hit a
        box-shaped obstacle, and if it does, computes the obstacle's normal
        at the hitpoint.
        @param pos1    The original position of the rectangle
        @param azimuth1    The original rotation of the rectangle
        @param pos2    The final position of the rectangle
        @param azimuth2    The final rotation of the rectangle
        @param halfWidth   Half the width of the rectangle
        @param halfBreadth Half the breadth of the rectangle
        @param oPos    The position of the obstacle
        @param oAzimuth    The rotation of the obstacle
        @param oWidth  Half the width of the obstacle
        @param oBreadth    Half the breadth of the obstacle
        @param oHeight     The height of the obstacle
        @param normal  The surface normal of the obstacle at the hitpoint
           will be stored here
        @returns       The time of the hit, where 0 is the time when the
           rectangle is at @c pos1 and 1 is the time when it's
           at @c pos2, and -1 means "no hit"
    */
    float getHitNormal(const glm::vec3 &pos1, float azimuth1,
                       const glm::vec3 &pos2, float azimuth2,
                       float halfWidth, float halfBreadth,
                       const glm::vec3 &oPos, float oAzimuth,
                       float oWidth, float oBreadth, float oHeight,
                       glm::vec3 &normal) const;

protected:
    static int getObjCounter();
    static void incObjCounter();

protected:
    Extents extents;
    glm::vec3 pos;
    glm::vec3 size; // width, breadth, height
    float angle;
    bool driveThrough;
    bool shootThrough;
    bool ricochet;
    bool ZFlip;
    char source;

private:
    int insideNodeCount;
    SceneNode** insideNodes;

private:
    static int objCounter;
};

//
// Obstacle
//

inline const Extents& Obstacle::getExtents() const
{
    return extents;
}

inline const glm::vec3 &Obstacle::getPosition() const
{
    return pos;
}

inline const glm::vec3 &Obstacle::getSize() const
{
    return size;
}

inline float Obstacle::getRotation() const
{
    return angle;
}

inline float Obstacle::getWidth() const
{
    return size[0];
}

inline float Obstacle::getBreadth() const
{
    return size[1];
}

inline float Obstacle::getHeight() const
{
    return size[2];
}

inline bool Obstacle::isDriveThrough() const
{
    return driveThrough;
}

inline bool Obstacle::isShootThrough() const
{
    return shootThrough;
}

inline bool Obstacle::isPassable() const
{
    return (driveThrough && shootThrough);
}

inline bool Obstacle::canRicochet() const
{
    return ricochet;
}

inline void Obstacle::setSource(char _source)
{
    source = _source;
    return;
}

inline char Obstacle::getSource() const
{
    return source;
}

inline bool Obstacle::isFromWorldFile() const
{
    return (source == WorldSource);
}

inline bool Obstacle::isFromGroupDef() const
{
    return ((source & GroupDefSource) != 0);
}

inline bool Obstacle::isFromContainer() const
{
    return ((source & ContainerSource) != 0);
}

inline int Obstacle::getObjCounter()
{
    return objCounter;
}
inline void Obstacle::incObjCounter()
{
    objCounter++;
}
inline void Obstacle::resetObjCounter()
{
    objCounter = 0;
}

#endif // BZF_OBSTACLE_H

// Local Variables: ***
// mode: C++ ***
// tab-width: 4 ***
// c-basic-offset: 4 ***
// indent-tabs-mode: nil ***
// End: ***
// ex: shiftwidth=4 tabstop=4
