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
#include "Pack.h"

// System headers
#include <string.h>
#include <glm/vec3.hpp>

// Common headers
#include "network.h"

#define ADV(_b, _t) ((void*)(((char*)(_b)) + sizeof(_t)))
#define cADV(_b, _t)    ((const void*)(((const char*)(_b)) + sizeof(_t)))

//
// Unions
//

union floatintuni
{
    float floatval;
    uint32_t intval;
};


//
// Buffer Overrun Checks
//

static bool Error = false;
static bool ErrorChecking = false;
static unsigned int Length = 0;

void nboUseErrorChecking(bool checking)
{
    ErrorChecking = checking;
}

bool nboGetBufferError()
{
    return Error;
}

void nboClearBufferError()
{
    Error = false;
}

void nboSetBufferLength(unsigned int length)
{
    Length = length;
}

unsigned int nboGetBufferLength()
{
    return Length;
}


//
// Packers
//

void*           nboPackUByte(void* b, uint8_t v)
{
    ::memcpy(b, &v, sizeof(uint8_t));
    return ADV(b, uint8_t);
}

void*           nboPackShort(void* b, int16_t v)
{
    const int16_t x = (int16_t)htons(v);
    ::memcpy(b, &x, sizeof(int16_t));
    return ADV(b, int16_t);
}

void*           nboPackInt(void* b, int32_t v)
{
    const int32_t x = (int32_t)htonl(v);
    ::memcpy(b, &x, sizeof(int32_t));
    return ADV(b, int32_t);
}

void*           nboPackUShort(void* b, uint16_t v)
{
    const uint16_t x = (uint16_t)htons(v);
    ::memcpy(b, &x, sizeof(uint16_t));
    return ADV(b, uint16_t);
}

void*           nboPackUInt(void* b, uint32_t v)
{
    const uint32_t x = (uint32_t)htonl(v);
    ::memcpy(b, &x, sizeof(uint32_t));
    return ADV(b, uint32_t);
}

void*           nboPackFloat(void* b, float v)
{
    // hope that float is a 4 byte IEEE 754 standard encoding
    floatintuni u;
    u.floatval = v;

    uint32_t x = (uint32_t)htonl(u.intval);
    ::memcpy(b, &x, sizeof(uint32_t));
    return ADV(b, uint32_t);
}

void*           nboPackVector(void* b, const float *v)
{
    // hope that float is a 4 byte IEEE 754 standard encoding
    floatintuni u;
    uint32_t data[3];

    for (int i=0; i<3; i++)
    {
        u.floatval = v[i];
        data[i] = (uint32_t)htonl(u.intval);
    }

    ::memcpy( b, data, 3*sizeof(uint32_t));
    return (void*) (((char*)b)+3*sizeof(uint32_t));
}

void *nboPackVector(void* b, const glm::vec3 &v)
{
    // hope that float is a 4 byte IEEE 754 standard encoding
    floatintuni u;
    uint32_t data[3];

    for (int i=0; i<3; i++)
    {
        u.floatval = v[i];
        data[i] = (uint32_t)htonl(u.intval);
    }

    ::memcpy( b, data, 3*sizeof(uint32_t));
    return (void*) (((char*)b)+3*sizeof(uint32_t));
}

void*           nboPackString(void* b, const void* m, int len)
{
    if (!m || len == 0) return b;
    ::memcpy(b, m, len);
    return (void*)((char*)b + len);
}

void*           nboPackStdString(void* b, const std::string& str)
{
    uint32_t strSize = str.size();
    b = nboPackUInt(b, strSize);
    b = nboPackString(b, str.c_str(), strSize);
    return b;
}


//
// UnPackers
//

const void*     nboUnpackUByte(const void* b, uint8_t& v)
{
    if (ErrorChecking)
    {
        if (Length < sizeof(uint8_t))
        {
            Error = true;
            v = 0;
            return b;
        }
        else
            Length -= sizeof(uint8_t);
    }
    ::memcpy(&v, b, sizeof(uint8_t));
    return cADV(b, uint8_t);
}

const void*     nboUnpackShort(const void* b, int16_t& v)
{
    if (ErrorChecking)
    {
        if (Length < sizeof(int16_t))
        {
            Error = true;
            v = 0;
            return b;
        }
        else
            Length -= sizeof(int16_t);
    }
    int16_t x;
    ::memcpy(&x, b, sizeof(int16_t));
    v = (int16_t)ntohs(x);
    return cADV(b, int16_t);
}

const void*     nboUnpackInt(const void* b, int32_t& v)
{
    if (ErrorChecking)
    {
        if (Length < sizeof(int32_t))
        {
            Error = true;
            v = 0;
            return b;
        }
        else
            Length -= sizeof(int32_t);
    }
    int32_t x;
    ::memcpy(&x, b, sizeof(int32_t));
    v = (int32_t)ntohl(x);
    return cADV(b, uint32_t);
}

const void*     nboUnpackUShort(const void* b, uint16_t& v)
{
    if (ErrorChecking)
    {
        if (Length < sizeof(uint16_t))
        {
            Error = true;
            v = 0;
            return b;
        }
        else
            Length -= sizeof(uint16_t);
    }
    uint16_t x;
    ::memcpy(&x, b, sizeof(uint16_t));
    v = (uint16_t)ntohs(x);
    return cADV(b, uint16_t);
}

const void*     nboUnpackUInt(const void* b, uint32_t& v)
{
    if (ErrorChecking)
    {
        if (Length < sizeof(uint32_t))
        {
            Error = true;
            v = 0;
            return b;
        }
        else
            Length -= sizeof(uint32_t);
    }
    uint32_t x;
    ::memcpy(&x, b, sizeof(uint32_t));
    v = (uint32_t)ntohl(x);
    return cADV(b, uint32_t);
}

const void*     nboUnpackFloat(const void* b, float& v)
{
    if (ErrorChecking)
    {
        if (Length < sizeof(float))
        {
            Error = true;
            v = 0.0f;
            return b;
        }
        else
            Length -= sizeof(float);
    }
    // hope that float is a 4 byte IEEE 754 standard encoding
    uint32_t x;
    ::memcpy(&x, b, sizeof(uint32_t));
    floatintuni u;
    u.intval = (uint32_t)ntohl(x);
    if (isnan(u.floatval))
    {
        fprintf(stderr, "nboUnpackFloat(): replacing NaN with zero\n");
        Error = true;
        v = 0.0f;
        return b;
    }
    else
        v = u.floatval;
    return cADV(b, uint32_t);
}

const void*     nboUnpackVector(const void* b, float *v)
{
    if (ErrorChecking)
    {
        if (Length < sizeof(float[3]))
        {
            Error = true;
            v[0] = v[1] = v[2] = 0.0f;
            return b;
        }
        else
            Length -= sizeof(float[3]);
    }
    // hope that float is a 4 byte IEEE 754 standard encoding
    uint32_t data[3];
    floatintuni u;
    ::memcpy( data, b, 3*sizeof(uint32_t));

    for (int i=0; i<3; i++)
    {
        u.intval = (uint32_t)ntohl(data[i]);
        v[i] = u.floatval;
    }

    return (const char*)b + 3*sizeof(float);
}

const void *nboUnpackVector(const void* b, glm::vec3 &v)
{
    if (ErrorChecking)
    {
        if (Length < sizeof(float[3]))
        {
            Error = true;
            v[0] = v[1] = v[2] = 0.0f;
            return b;
        }
        else
            Length -= sizeof(float[3]);
    }
    // hope that float is a 4 byte IEEE 754 standard encoding
    uint32_t data[3];
    floatintuni u;
    ::memcpy( data, b, 3*sizeof(uint32_t));

    for (int i=0; i<3; i++)
    {
        u.intval = (uint32_t)ntohl(data[i]);
        v[i] = u.floatval;
    }

    return (const char*)b + 3*sizeof(float);
}

const void*     nboUnpackString(const void* b, void* m, int len)
{
    if (!m || len == 0) return b;
    if (ErrorChecking)
    {
        if (Length < (unsigned int)len)
        {
            Error = true;
            ((char*)m)[0] = '\0';
            return b;
        }
        else
            Length -= len;
    }
    ::memcpy(m, b, len);
    return (const char*)b + len;
}

const void*     nboUnpackStdString(const void* b, std::string& str)
{
    uint32_t strSize;
    b = nboUnpackUInt(b, strSize);
    char* buffer = new char[strSize + 1];
    b = nboUnpackString(b, buffer, strSize);
    buffer[strSize] = 0;
    str = buffer;
    delete[] buffer;
    if (ErrorChecking && Error)
    {
        str = "";
        return b;
    }
    return b;
}

const void*     nboUnpackStdStringRaw(const void* b, std::string& str)
{
    uint32_t strSize;
    b = nboUnpackUInt(b, strSize);
    char* buffer = new char[strSize + 1];
    b = nboUnpackString(b, buffer, strSize);
    buffer[strSize] = 0;
    str.resize(strSize);
    for (uint32_t i = 0; i < strSize; i++)
        str[i] = buffer[i];
    delete[] buffer;
    if (ErrorChecking && Error)
    {
        str = "";
        return b;
    }
    return b;
}


//
// Utilities
//

unsigned int nboStdStringPackSize(const std::string& str)
{
    return (sizeof(uint32_t) + str.size());
}


// Local Variables: ***
// mode: C++ ***
// tab-width: 4 ***
// c-basic-offset: 4 ***
// indent-tabs-mode: nil ***
// End: ***
// ex: shiftwidth=4 tabstop=4
