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

/* Functions to write various data types packed into a buffer
 * in network byte order, or to read various types out of a
 * buffer.
 *
 * floats may be converted to some platform independent form before
 * packing and converted from that form when unpacking;  precision
 * may be lost.
 */

#ifndef BZF_PACK_H
#define BZF_PACK_H
// 1st
#include "common.h"

// System headers
#include <string>
#include <glm/fwd.hpp>


extern void* nboPackUByte(void*, uint8_t);
extern void* nboPackShort(void*, int16_t);
extern void* nboPackInt(void*, int32_t);
extern void* nboPackUShort(void*, uint16_t);
extern void* nboPackUInt(void*, uint32_t);
extern void* nboPackFloat(void*, float);
extern void* nboPackVector(void*, const float*);
extern void* nboPackVector(void*, const glm::vec3 &);
extern void* nboPackString(void*, const void*, int len);
extern void* nboPackStdString(void*, const std::string& str);

extern const void* nboUnpackUByte(const void*, uint8_t&);
extern const void* nboUnpackShort(const void*, int16_t&);
extern const void* nboUnpackInt(const void*, int32_t&);
extern const void* nboUnpackUShort(const void*, uint16_t&);
extern const void* nboUnpackUInt(const void*, uint32_t&);
extern const void* nboUnpackFloat(const void*, float&);
extern const void* nboUnpackVector(const void*, float*);
extern const void* nboUnpackVector(const void*, glm::vec3 &);
extern const void* nboUnpackString(const void*, void*, int len);
extern const void* nboUnpackStdString(const void*, std::string& str);
extern const void* nboUnpackStdStringRaw(const void*, std::string& str);

extern unsigned int nboStdStringPackSize(const std::string& str);

// buffer overrun checking
extern void nboUseErrorChecking(bool checking);
extern bool nboGetBufferError();
extern void nboClearBufferError();
extern void nboSetBufferLength(unsigned int length);
extern unsigned int nboGetBufferLength();


#endif // BZF_PACK_H

// Local Variables: ***
// mode: C++ ***
// tab-width: 4 ***
// c-basic-offset: 4 ***
// indent-tabs-mode: nil ***
// End: ***
// ex: shiftwidth=4 tabstop=4
