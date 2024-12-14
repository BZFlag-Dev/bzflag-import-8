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
#include "sound.h"

// system headers
#include <vector>
#include <map>
#include <string.h>
#include <glm/gtc/type_ptr.hpp>

// common headers
#include "BzfMedia.h"
#include "TimeKeeper.h"
#include "PlatformFactory.h"
#include "BZDBCache.h"
#include "TextUtils.h"

static const float SpeedOfSound(343.0f);        // meters/sec
static const size_t MaxEvents(30);
static const float InterAuralDistance(0.1f);        // meters

/* NOTE:
 *  world sounds use the ptrFrac member, local sounds the ptr member.
 *  world sounds only use the monoaural samples so the ptrFrac member
 *  is incremented by 1 for each sample.  local sounds are in stereo
 *  and ptr is incremented by 2 for each (stereo) sample.
 */

/* last position of the receiver */
static float        lastX, lastY, lastZ, lastTheta;
static float        lastXLeft, lastYLeft;
static float        lastXRight, lastYRight;
static float        forwardX, forwardY;
static float        leftX, leftY;
static int      positionDiscontinuity;

/* motion info for Doppler shift */
static float        velX;
static float        velY;

/* volume */
static float        volumeAtten = 1.0f;
static int      mutingOn = 0;

/*
 * producer/consumer shared data types and defines
 */

/* sound queue commands */
#define SQC_CLEAR   0       /* no code; no data */
#define SQC_SET_POS 1       /* no code; x,y,z,t */
#define SQC_SET_VEL 2       /* no code; x,y,z */
#define SQC_SET_VOLUME  3       /* code = new volume; no data */
#define SQC_LOCAL_SFX   4       /* code=sfx; no data */
#define SQC_WORLD_SFX   5       /* code=sfx; x,y,z of sfx source */
#define SQC_FIXED_SFX   6       /* code=sfx; x,y,z of sfx source */
#define SQC_JUMP_POS    7       /* no code; x,y,z,t */
#define SQC_QUIT    8       /* no code; no data */
#define SQC_IWORLD_SFX  9       /* code=sfx; x,y,z of sfx source */

// This structure is held in the audio queue until it is processed here
struct SoundCommand
{
public:

    enum CMD {CLEAR,      // no code, no data
              SET_POS,        // no code; x,y,z,t
              SET_VEL,        // no code; x,y,z
              SET_VOL,        // code = new volume; no data
              LOCAL_SFX,      // code = sfx; no data
              WORLD_SFX,      // code = sfx; x,y,z of sfx source
              FIXED_SFX,      // code = sfx; x,y,z of sfx source
              JUMP_POS,       // no code; x,y,z,y
              QUIT,       // no code; no data
              IWORLD_SFX      // code = sfx; x,y,z of sfx source
             };

    SoundCommand();
    explicit SoundCommand(SoundCommand::CMD, int code_=-1,
                          float x_=0, float y_=0, float z_=0, float t_=0);

    CMD           cmd;
    int           code;
    float         x,y,z,t;
};

SoundCommand::SoundCommand()
    : cmd(CLEAR), code(-1), x(0), y(0), z(0), t(0)
{
}

SoundCommand::SoundCommand(CMD cmd_, int code_,
                           float x_, float y_, float z_, float t_)
    : cmd(cmd_), code(code_), x(x_), y(y_), z(z_), t(t_)
{
}

class AudioSamples
{
public:
    AudioSamples();
    bool resample(const float* in,    // stereo sampled data
                  int frames,     // number of stereo (left, right) frames
                  int rate,       // sample rate
                  std::string const& name); // sound name

    size_t        length() const; // size of the (stereo) data

    long          mlength;    /* total number samples in mono */
    double        dmlength;   /* mlength as a double minus one */
    std::vector<float>    data;       /* left in even, right in odd */
    std::vector<float>    monoRaw;    /* mono with silence before & after */
    size_t        monoIdx;    // index to start of mono samples
    double        duration;   /* time to play sound */
    std::string       file;
};


AudioSamples::AudioSamples()
    : mlength(0), dmlength(0), data(), monoRaw(), monoIdx(0), duration(0)
{
}

size_t AudioSamples::length() const
{
    return data.size();
}

bool AudioSamples::resample(const float* in, int frames, int rate,
                            std::string const& name)
{
    // safety check... SNH
    if (in == 0) return false;

    // attenuation on all sounds
    static const float GlobalAtten(0.5f);
    float const outputRate( (float)PlatformFactory::getMedia()->getAudioOutputRate() );

    if (rate != outputRate)
    {
        std::cout << name << " rate is " << rate
                  << "; audio output is " << outputRate
                  << "\n";
        // FIXME -- should resample  -- let it through at wrong sample rate
        // return false;
    }

    // compute safety margin for left/right ear time discrepancy.  since
    // each ear can hear a sound at slightly different times one ear might
    // be hearing a sound before the other hears it or one ear may no longer
    // be hearing a sound that the other is still hearing.  if we didn't
    // account for this we'd end up sampling outside the mono buffer (and
    // only the mono buffer because the data buffer is only used for local
    // sounds, which don't have this effect).  to avoid doing a lot of tests
    // in inner loops, we'll just put some silent samples before and after
    // the sound effect.  here we compute how many samples can be needed.
    const int safetyMargin((int)(1.5f + InterAuralDistance / SpeedOfSound * outputRate));

    // fill in samples structure
    file = name;
    data.resize( 2 * frames );
    mlength = frames;
    dmlength = double(mlength - 1);
    duration = (float)mlength / outputRate;
    monoRaw.assign( frames + (2 * safetyMargin), 0.0f );  // fill with silence
    monoIdx = safetyMargin;       // start after the (leading) safety margin

    // filter samples
    size_t localIdx( monoIdx );
    for (long dst(0); dst < (2 * frames); dst += 2)
    {
        data[dst] = GlobalAtten * in[dst];
        data[dst+1] = GlobalAtten * in[dst+1];
        monoRaw[ localIdx++ ] = (data[dst] + data[dst+1])/2;
    }

    return 1;
}

/*
 * local functions
 */

static void     sendSound(SoundCommand* s);
static void     audioLoop(void*);
static bool     allocAudioSamples();
static void     freeAudioSamples(void);


/*
 * general purpose audio stuff
 */

static bool     usingAudio(false);
static const char*  soundFiles[] =
{
    "fire",
    "explosion",
    "ricochet",
    "flag_grab",
    "flag_drop",
    "flag_won",
    "flag_lost",
    "flag_alert",
    "jump",
    "land",
    "teleport",
    "laser",
    "shock",
    "pop",
    "explosion",
    "flag_grab",
    "boom",
    "killteam",
    "phantom",
    "missile",
    "lock",
    "teamgrab",
    "hunt",
    "hunt_select",
    "steamroller",
    "thief",
    "burrow",
    "message_private",
    "message_team",
    "message_admin",
    "flap",
    "bounce"
};

// the number of "Standard" sounds
#define STD_SFX_COUNT   ((int)(bzcountof(soundFiles)))

/*
 * producer/consumer shared arena
 */

static std::vector<AudioSamples>    soundSamples;

static std::map<std::string, int>   customSamples;

static size_t       audioBufferSize(0);
static int      soundLevel(0);

static BzfMedia*    media(0);

/* speed of sound stuff */
static float        timeSizeOfWorld(0);     /* in seconds */
static TimeKeeper   startTime;
static double       prevTime(0), curTime(0);

struct SoundEvent
{
    SoundEvent();

    void reset(AudioSamples* audioSample, float x=0, float y=0, float z=0);

    bool isWorld() const;
    bool isFixed() const;
    bool isIgnoring() const;
    bool isImportant() const;

    void setWorld(bool val);
    void setFixed(bool val);
    void setIgnoring(bool val);
    void setImportant(bool val);

    void recalcDistance();

    AudioSamples*     samples;        /* event sound effect */
    bool          busy;           /* true iff in use */
    size_t        ptr;            /* current sample */
    double        ptrFracLeft;        /* fractional step ptr */
    double        ptrFracRight;       /* fractional step ptr */
    float         x, y, z;        /* event location */
    double        time;           /* time of event */
    float         dx, dy, dz;     /* last relative position */
    float         d;          /* last relative distance */
    float         dLeft;          /* last relative distance */
    float         dRight;         /* last relative distance */
    float         amplitude;      /* last sfx amplitude */

private:
    float calcDistance(float prevX, float prevY, float d3);

    bool          world;
    bool          fixed;
    bool          ignoring;
    bool          important;
};

SoundEvent::SoundEvent()
    : samples(0), busy(false), ptr(0), ptrFracLeft(0), ptrFracRight(0), x(0), y(0), z(0), time(0),
      dx(0), dy(0), dz(0), d(0), dLeft(0), dRight(0), amplitude(0),
      world(false), fixed(false), ignoring(false), important(false)
{
}

void SoundEvent::reset(AudioSamples* sample_, float x_, float y_, float z_)
{
    samples = sample_;
    busy = true;
    ptr = 0;
    ptrFracLeft = ptrFracRight = 0; // not explicit?
    x = x_;
    y = y_;
    z = z_;
    time = curTime;
    dx = dy = dz = 0;     // not explicit?
    d = dLeft = dRight = 0;   // not explicit?
    amplitude = 0;        // not explicit?
    world = fixed = ignoring = important = false;
}

bool SoundEvent::isWorld() const
{
    return world;
}

bool SoundEvent::isFixed() const
{
    return fixed;
}

bool SoundEvent::isIgnoring() const
{
    return ignoring;
}

bool SoundEvent::isImportant() const
{
    return important;
}

void SoundEvent::setWorld(bool val)
{
    world = val;
}

void SoundEvent::setFixed(bool val)
{
    fixed = val;
}

void SoundEvent::setIgnoring(bool val)
{
    ignoring = val;
}

void SoundEvent::setImportant(bool val)
{
    important = val;
}

void SoundEvent::recalcDistance()
{
    dx = x - lastX;
    dy = y - lastY;
    dz = z - lastZ;
    const float d2 = dx * dx + dy * dy;
    const float d3 = dz * dz;
    if (d2 <= 1.0f)
    {
        d = 0.0f;
        dLeft = 0.0f;
        dRight = 0.0f;
    }
    else
    {
        const float delta = 1.0f / sqrtf(d2);
        dx *= delta;
        dy *= delta;
        d = sqrtf(d2 + d3);
        // Since we are on a forked process, we cannot use BZDB_TANKRADIUS
        // We put in the old tankRadius value (4.32)
        float minEventDist = 20.0f * 4.32f;
        amplitude = (d < minEventDist) ? 1.0f : minEventDist / d;

        // compute distance to each ear
        dLeft  = calcDistance(lastXLeft,  lastYLeft,  d3);
        dRight = calcDistance(lastXRight, lastYRight, d3);
    }
}

float SoundEvent::calcDistance(float prevX, float prevY, float d3)
{
    float deltaX( x - prevX );
    float deltaY( y - prevY );
    return sqrtf( deltaX*deltaX + deltaY*deltaY + d3 );
}


/* list of events currently pending */
static SoundEvent   events[MaxEvents];
static int      portUseCount;
static double       endTime;

/* scratch buffer for adding contributions from sources */
static std::vector<float> scratch;

static bool     usingSameThread = false;

static bool     audioInnerLoop();

void            openSound(const char*)
{
    if (usingAudio) return;           // already opened

    media = PlatformFactory::getMedia();
    if (!media->openAudio())
        return;

    // open audio data files
    if (!allocAudioSamples())
    {
        media->closeAudio();
#ifndef DEBUG
        std::cout << "WARNING: Unable to open audio data files" << std::endl;
#endif
        return;                 // couldn't get samples
    }

    audioBufferSize = media->getAudioBufferChunkSize() * 2;

    /* initialize */
    timeSizeOfWorld = 1.414f * BZDBCache::worldSize / SpeedOfSound;
    portUseCount = 0;
    scratch.resize(audioBufferSize);

    startTime = TimeKeeper::getCurrent();
    curTime = 0.0;
    endTime = -1.0;

    usingSameThread = !media->hasAudioThread();

    if (media->hasAudioCallback())
        media->startAudioCallback(audioInnerLoop);
    else
    {
        // start audio thread
        if (!usingSameThread && !media->startAudioThread(audioLoop, NULL))
        {
            media->closeAudio();
            freeAudioSamples();
#ifndef DEBUG
            std::cout << "WARNING: Unable to start the audio thread" << std::endl;
#endif
            return;
        }
    }

    setSoundVolume(10);

    usingAudio = true;
}

void            closeSound(void)
{
    if (!usingAudio) return;

    // send stop command to audio thread
    SoundCommand s(SoundCommand::QUIT);
    sendSound(&s);

    // stop audio thread
    PlatformFactory::getMedia()->stopAudioThread();

    // reset audio hardware
    PlatformFactory::getMedia()->closeAudio();

    // free memory used for sfx samples
    freeAudioSamples();

    usingAudio = false;
}

bool            isSoundOpen()
{
    return usingAudio;
}

static bool     allocAudioSamples()
{
    bool anyFile = false;

    soundSamples.reserve(STD_SFX_COUNT + 10); // if the world loads sounds, we don't want to resize

    // load the default samples
    for (int i = 0; i < STD_SFX_COUNT; i++)
    {
        std::string sound( TextUtils::tolower(soundFiles[i]) );
        // read it
        int numFrames, rate;
        float* samples = PlatformFactory::getMedia()->readSound(sound.c_str(), numFrames, rate);
        AudioSamples newSample;
        if (newSample.resample(samples, numFrames, rate, sound))
            anyFile = true;
        soundSamples.push_back(newSample);
        // If a "custom" sound wants to play this, make it available
        customSamples[sound] = i;

        delete[] samples;
    }

    return anyFile;
}

static void     freeAudioSamples(void)
{
    // do nothing.
    // the samples are self freeing now
}

/*
 * sound fx producer stuff
 */

static void     sendSound(SoundCommand* s)
{
    if (!usingAudio) return;
    PlatformFactory::getMedia()->writeSoundCommand(s, sizeof(SoundCommand));
}

void            moveSoundReceiver(float x, float y, float z, float t,
                                  int discontinuity)
{
    if (soundLevel <= 0)
        return;
    SoundCommand::CMD cmd( discontinuity ? SoundCommand::JUMP_POS
                           : SoundCommand::SET_POS );
    SoundCommand s(cmd, 0, x, y, z, t);

    sendSound(&s);
}

void            speedSoundReceiver(const glm::vec3 &v)
{
    SoundCommand s(SoundCommand::SET_VEL, 0, v.x, v.y, v.z);
    sendSound(&s);
}

void playSound(int soundCode, const glm::vec3 &pos,
               bool important, bool localSound)
{
    if (localSound)
        playLocalSound(soundCode);
    else
        playWorldSound(soundCode, pos, important);
    return;
}

void playWorldSound(int soundCode, const glm::vec3 &pos, bool important)
{
    if (soundLevel <= 0)
        return;
    if ((int)soundSamples.size() <= soundCode) return;
    if (soundSamples[soundCode].length() == 0) return;
    SoundCommand s(important ? SoundCommand::IWORLD_SFX : SoundCommand::WORLD_SFX,
                   soundCode, pos[0], pos[1], pos[2]);
    sendSound(&s);
}

void            playLocalSound(int soundCode)
{
    // Check for conditions which preclude sounds
    if (soundLevel <= 0                   // no volume
            || soundCode >= (int)soundSamples.size()      // unknown sound
            || soundSamples[soundCode].length() == 0)     // empty sound
        return;
    SoundCommand s(SoundCommand::LOCAL_SFX, soundCode);
    sendSound(&s);
}

void            playLocalSound(std::string sound)
{
    sound = TextUtils::tolower(sound); // modify the local copy
    int  soundCode( -1 );

    std::map<std::string,int>::iterator itr = customSamples.find(sound);
    if (itr == customSamples.end())
    {
        int numFrames(0), rate(0);
        float* samples( PlatformFactory::getMedia()->readSound(sound.c_str(), numFrames, rate) );
        AudioSamples newSample;
        if (newSample.resample(samples, numFrames, rate, sound))
        {
            soundSamples.push_back(newSample);
            soundCode = (int)soundSamples.size()-1;
            customSamples[sound] = soundCode;
        }
        delete[] samples;
    }
    else
        soundCode = itr->second;

    if (soundCode > 0)
        playLocalSound(soundCode);
}

void            playFixedSound(int soundCode,
                               float x, float y, float z)
{
    // Check for conditions which preclude sounds
    if (soundLevel <= 0                   // no volume
            || soundCode > (int)soundSamples.size()       // unknown sound
            || soundSamples[soundCode].length() == 0)     // empty sound
        return;
    SoundCommand s(SoundCommand::FIXED_SFX, soundCode, x, y, z);
    sendSound(&s);
}

void            setSoundVolume(int newLevel)
{
    soundLevel = newLevel;
    if (soundLevel < 0) soundLevel = 0;
    else if (soundLevel > 10) soundLevel = 10;

    SoundCommand s(SoundCommand::SET_VOL, soundLevel);
    sendSound(&s);
}

int         getSoundVolume()
{
    return soundLevel;
}


/*
 * Below this point is stuff for real-time audio thread
 */


static int      recalcEventIgnoring(SoundEvent* e)
{
    if (e->isFixed() || !e->isWorld()) return 0;

    float travelTime = (float)(curTime - e->time);
    if (travelTime > e->samples->duration + timeSizeOfWorld)
    {
        // sound front has passed all points in world
        e->busy = false;
        return (e->isIgnoring()) ? 0 : -1;
    }

    int useChange = 0;
    float eventDistance = e->d / SpeedOfSound;
    if (travelTime < eventDistance)
    {
        if (e->isIgnoring())
        {
            /* do nothing -- still ignoring */
        }
        else
        {
            /* ignoring again */
            e->setIgnoring(true);
            useChange = -1;
        }
        /* don't sleep past the time the sound front will pass by */
        endTime = eventDistance;
    }
    else
    {
        if (e->isIgnoring())
        {
            float timeFromFront;
            /* compute time from sound front */
            timeFromFront = travelTime - e->dLeft / SpeedOfSound;
            if (!positionDiscontinuity && timeFromFront < 0.0f) timeFromFront = 0.0f;

            /* recompute sample pointers */
            e->ptrFracLeft = timeFromFront *
                             (float)PlatformFactory::getMedia()->getAudioOutputRate();
            if (e->ptrFracLeft >= 0.0 && e->ptrFracLeft < e->samples->dmlength)
            {
                /* not ignoring anymore */
                e->setIgnoring(false);
                useChange = 1;
            }

            /* now do it again for right ear */
            timeFromFront = travelTime - e->dRight / SpeedOfSound;
            if (!positionDiscontinuity && timeFromFront < 0.0f) timeFromFront = 0.0f;
            e->ptrFracRight = timeFromFront *
                              (float)PlatformFactory::getMedia()->getAudioOutputRate();
            if (e->ptrFracRight >= 0.0 && e->ptrFracRight < e->samples->dmlength)
            {
                e->setIgnoring(false);
                useChange = 1;
            }
        }
        else
        {
            /* do nothing -- still not ignoring */
        }
    }
    return useChange;
}

static void     receiverMoved(float x, float y, float z, float t)
{
    // save data
    lastX = x;
    lastY = y;
    lastZ = z;
    lastTheta = t;

    // compute forward and left vectors
    forwardX = cosf(lastTheta);
    forwardY = sinf(lastTheta);
    leftX = -forwardY;
    leftY = forwardX;

    // compute position of each ear
    lastXLeft = lastX + 0.5f * InterAuralDistance * leftX;
    lastYLeft = lastY + 0.5f * InterAuralDistance * leftY;
    lastXRight = lastX - 0.5f * InterAuralDistance * leftX;
    lastYRight = lastY - 0.5f * InterAuralDistance * leftY;

    for (size_t i(0); i < MaxEvents; i++)
        if (events[i].busy && (events[i].isWorld()))
            events[i].recalcDistance();
}

static void     receiverVelocity(float vx, float vy)
{
    static const float s = 1.0f / SpeedOfSound;

    velX = s * vx;
    velY = s * vy;
}

static int      addLocalContribution(SoundEvent* e, size_t& len)
{
    size_t numSamples( e->samples->length() - e->ptr );
    if (numSamples > audioBufferSize) numSamples = audioBufferSize;

    if (!mutingOn && numSamples != 0)
    {
        /* initialize new areas of scratch space and adjust output sample count */
        if (numSamples > len)
        {
            for (size_t n = len; n < numSamples; n += 2)
                scratch[n] = scratch[n+1] = 0.0f;
            len = numSamples;
        }

        float* src( &e->samples->data.at(e->ptr) );
        try
        {

            for (size_t n(0); n < numSamples; n += 2)
            {
                scratch[n] += src[n] * volumeAtten;
                scratch[n+1] += src[n+1] * volumeAtten;
            }
        }
        catch (std::exception const& ex)
        {
            std::cout << "Exception on sound " << e->samples->file << "\n" << ex.what() << std::endl;
        }
    }

    /* free event if ran out of samples */
    if ((e->ptr += numSamples) == e->samples->length())
    {
        e->busy = false;
        return -1;
    }

    return 0;
}

static void     getWorldStuff(SoundEvent *e, float* la, float* ra,
                              double* sampleStep)
{
    float leftAtten, rightAtten;

    // compute left and right attenuation factors
    // FIXME -- should be a more general HRTF
    if (e->d == 0.0f)
    {
        leftAtten = 1.0f;
        rightAtten = 1.0f;
    }
    else
    {
        const float ff = (2.0f + forwardX * e->dx + forwardY * e->dy) / 3.0f;
        const float fl = (2.0f + leftX * e->dx + leftY * e->dy) / 3.0f;
        leftAtten = ff * fl * e->amplitude;
        rightAtten = ff * (4.0f/3.0f - fl) * e->amplitude;
    }

    *la = mutingOn ? 0.0f : leftAtten * volumeAtten;
    *ra = mutingOn ? 0.0f : rightAtten * volumeAtten;

    /* compute doppler effect */
    // FIXME -- should be per ear
    *sampleStep = double(1.0 + velX * e->dx + velY * e->dy);
}

static int      addWorldContribution(SoundEvent* e, size_t& len)
{
    bool      fini(false);
    size_t    n;
    long      nmL, nmR;
    float*    src( &e->samples->monoRaw[ e->samples->monoIdx ] );
    float     leftAtten, rightAtten, fracL, fracR, fsampleL, fsampleR;
    double    sampleStep;

    if (e->isIgnoring()) return 0;

    getWorldStuff(e, &leftAtten, &rightAtten, &sampleStep);
    if (sampleStep <= 0.0) fini = true;

    /* initialize new areas of scratch space and adjust output sample count */
    if (audioBufferSize > len)
    {
        for (n = len; n < audioBufferSize; n += 2)
            scratch[n] = scratch[n+1] = 0.0f;
        len = audioBufferSize;
    }

    try
    {
        // add contribution
        for (n = 0; !fini && n < audioBufferSize; n += 2)
        {
            // get sample position (to subsample resolution)
            nmL = (long)e->ptrFracLeft;
            nmR = (long)e->ptrFracRight;
            fracL = (float)(e->ptrFracLeft - floor(e->ptrFracLeft));
            fracR = (float)(e->ptrFracRight - floor(e->ptrFracRight));

            // get sample (lerp closest two samples)
            fsampleL = (1.0f - fracL) * src[nmL] + fracL * src[nmL+1];
            fsampleR = (1.0f - fracR) * src[nmR] + fracR * src[nmR+1];

            // filter and accumulate
            scratch[n] += fsampleL * leftAtten;
            scratch[n+1] += fsampleR * rightAtten;

            // next sample
            if ((e->ptrFracLeft += sampleStep) >= e->samples->dmlength)
                fini = true;
            if ((e->ptrFracRight += sampleStep) >= e->samples->dmlength)
                fini = true;
        }
    }
    catch (std::exception const& ex)
    {
        std::cout << "Exception on sound " << e->samples->file << "\n" << ex.what() << std::endl;
    }

    /* NOTE: running out of samples just means the world sound front
     *    has passed our location.  if we teleport it may pass us again.
     *    so we can't free the event until the front passes out of the
     *    world.  compute time remaining until that happens and set
     *    endTime if smaller than current endTime. */
    if (fini)
    {
        double et = e->samples->duration + timeSizeOfWorld - (prevTime - e->time);
        if (endTime == -1.0 || et < endTime) endTime = et;
        e->setIgnoring(true);
        return -1;
    }
    return 0;
}

static int      addFixedContribution(SoundEvent* e, size_t& len)
{
    size_t    n;
    long      nmL, nmR;
    float*    src( &e->samples->monoRaw[ e->samples->monoIdx ] );
    float     leftAtten, rightAtten, fracL, fracR, fsampleL, fsampleR;
    double    sampleStep;

    getWorldStuff(e, &leftAtten, &rightAtten, &sampleStep);

    /* initialize new areas of scratch space and adjust output sample count */
    if (audioBufferSize > len)
    {
        for (n = len; n < audioBufferSize; n += 2)
            scratch[n] = scratch[n+1] = 0.0f;
        len = audioBufferSize;
    }

    // add contribution
    for (n = 0; n < audioBufferSize; n += 2)
    {
        // get sample position (to subsample resolution)
        nmL = (long)e->ptrFracLeft;
        nmR = (long)e->ptrFracRight;
        fracL = (float)(e->ptrFracLeft - floor(e->ptrFracLeft));
        fracR = (float)(e->ptrFracRight - floor(e->ptrFracRight));

        // get sample (lerp closest two samples)
        fsampleL = (1.0f - fracL) * src[nmL] + fracL * src[nmL+1];
        fsampleR = (1.0f - fracR) * src[nmR] + fracR * src[nmR+1];

        // filter and accumulate
        scratch[n] += fsampleL * leftAtten;
        scratch[n+1] += fsampleR * rightAtten;

        // next sample
        if ((e->ptrFracLeft += sampleStep) >= e->samples->dmlength)
            e->ptrFracLeft -= e->samples->dmlength;
        if ((e->ptrFracRight += sampleStep) >= e->samples->dmlength)
            e->ptrFracRight -= e->samples->dmlength;
    }

    return 0;
}

static size_t       findBestWorldSlot()
{
    size_t i;

    // the best slot is an empty one
    for (i = 0; i < MaxEvents; i++)
        if (!events[i].busy)
            return i;

    // no available slots.  find an existing sound that won't be missed
    // (much).  this will cause a pop or crackle if the replaced sound is
    // currently playing.  first see if there are any world events.
    for (i = 0; i < MaxEvents; i++)
        if (events[i].isWorld() && !events[i].isFixed())
            break;

    // give up if no (non-fixed) world events
    if (i == MaxEvents) return MaxEvents;

    // found a world event.  see if there's an event that's
    // completely passed us.
    const size_t first(i);
    for (i = first; i < MaxEvents; i++)
    {
        if (events[i].isFixed() || !events[i].isWorld()) continue;
        if (!(events[i].isIgnoring())) continue;
        const float travelTime = (float)(curTime - events[i].time);
        const float eventDistance = events[i].d / SpeedOfSound;
        if (travelTime > eventDistance) return i;
    }

    // if no sound front has completely passed our position
    // then pick the most distant one that hasn't reached us
    // yet that isn't important.
    int farthestEvent = -1;
    float farthestDistance = 0.0f;
    for (i = first; i < MaxEvents; i++)
    {
        if (events[i].isImportant()) continue;
        if (events[i].isFixed() || !events[i].isWorld()) continue;
        if (!(events[i].isIgnoring())) continue;
        const float eventDistance = events[i].d / SpeedOfSound;
        if (eventDistance > farthestDistance)
        {
            farthestEvent = i;
            farthestDistance = eventDistance;
        }
    }
    if (farthestEvent != -1) return farthestEvent;

    // same thing but look at important sounds
    for (i = first; i < MaxEvents; i++)
    {
        if (!(events[i].isImportant())) continue;
        if (events[i].isFixed() || !events[i].isWorld()) continue;
        if (!(events[i].isIgnoring())) continue;
        const float eventDistance = events[i].d / SpeedOfSound;
        if (eventDistance > farthestDistance)
        {
            farthestEvent = i;
            farthestDistance = eventDistance;
        }
    }
    if (farthestEvent != -1) return farthestEvent;

    // we've only got playing world sounds to choose from.  pick the
    // most distant one since it's probably the quietest.
    farthestEvent = first;
    farthestDistance = events[farthestEvent].d / SpeedOfSound;
    for (i = first + 1; i < MaxEvents; i++)
    {
        if (events[i].isFixed() || !events[i].isWorld()) continue;
        const float eventDistance = events[i].d / SpeedOfSound;
        if (eventDistance > farthestDistance)
        {
            farthestEvent = i;
            farthestDistance = eventDistance;
        }
    }

    // replacing an active sound
    portUseCount--;
    return farthestEvent;
}

static size_t       findBestLocalSlot()
{
    // better to lose a world sound
    size_t slot( findBestWorldSlot() );
    if (slot != MaxEvents) return slot;

    // find the first local event
    size_t i;
    for (i = 0; i < MaxEvents; i++)
        if (!(events[i].isFixed()))
            break;

    // no available slot if only fixed sounds are playing (highly unlikely)
    if (i == MaxEvents) return MaxEvents;

    // find the local sound closest to completion.
    size_t minEvent = i;
    size_t minSamplesLeft = events[i].samples->length() - events[i].ptr;
    for (i++; i < MaxEvents; i++)
    {
        if (events[i].isFixed()) continue;
        if (events[i].samples->length() - events[i].ptr < minSamplesLeft)
        {
            minEvent = i;
            minSamplesLeft = events[i].samples->length() - events[i].ptr;
        }
    }

    // replacing an active sound
    portUseCount--;
    return minEvent;
}

//
// audioLoop() simply generates samples and keeps the audio hw fed
//
static bool     audioInnerLoop()
{
    size_t slot(MaxEvents);

    /* get time step */
    prevTime = curTime;
    curTime = TimeKeeper::getCurrent() - startTime;
    endTime = -1.0;
    positionDiscontinuity = 0;

    /* get new commands from queue */
    SoundCommand cmd;
    SoundEvent* event(0);
    while (media->readSoundCommand(&cmd, sizeof(SoundCommand)))
    {
        switch (cmd.cmd)
        {
        case SoundCommand::QUIT:
            return true;

        case SoundCommand::CLEAR:
            /* FIXME */
            break;

        case SoundCommand::SET_POS:
        case SoundCommand::JUMP_POS:
        {
            positionDiscontinuity = (cmd.cmd == SoundCommand::JUMP_POS);
            receiverMoved(cmd.x, cmd.y, cmd.z, cmd.t);
            break;
        }

        case SoundCommand::SET_VEL:
            receiverVelocity(cmd.x, cmd.y);
            break;

        case SoundCommand::SET_VOL:
            // The provided volume value is multiplied by itself to compensate for
            // human hearing
            volumeAtten = 0.02f * cmd.code * cmd.code;
            if (volumeAtten <= 0.0f)
            {
                mutingOn = true;
                volumeAtten = 0.0f;
            }
            else if (volumeAtten >= 2.0f)
            {
                mutingOn = false;
                volumeAtten = 2.0f;
            }
            else
                mutingOn = false;
            break;

        case SoundCommand::LOCAL_SFX:
            slot = findBestLocalSlot();
            if (slot == MaxEvents) break;
            event = events + slot;

            event->reset(&soundSamples[cmd.code]);
            portUseCount++;
            break;

        case SoundCommand::IWORLD_SFX:
        case SoundCommand::WORLD_SFX:
            if (cmd.cmd == SoundCommand::IWORLD_SFX)
                slot = findBestWorldSlot();
            else
            {
                for (slot = 0; slot < MaxEvents; slot++)
                    if (!events[slot].busy)
                        break;
            }
            if (slot == MaxEvents) break;

            event = events + slot;
            event->reset(&soundSamples[cmd.code], cmd.x, cmd.y, cmd.z);
            event->setWorld(true);
            event->setIgnoring(true);
            if (cmd.cmd == SoundCommand::IWORLD_SFX) event->setImportant(true);

            /* don't increment use count because we're ignoring the sound */
            event->recalcDistance();
            break;

        case SoundCommand::FIXED_SFX:
            for (slot = 0; slot < MaxEvents; slot++)
                if (!events[slot].busy)
                    break;
            if (slot == MaxEvents) break;

            event = events + slot;
            event->reset(&soundSamples[cmd.code], cmd.x, cmd.y, cmd.z);
            event->setFixed(true);
            event->setWorld(true);

            portUseCount++;
            event->recalcDistance();
            break;
        }
    }
    for (slot = 0; slot < MaxEvents; slot++)
        if (events[slot].busy)
        {
            int deltaCount = recalcEventIgnoring(events + slot);
            portUseCount += deltaCount;
        }

    /* sum contributions to the port and output samples */
    if (media->isAudioTooEmpty())
    {
        size_t numSamples(0);
        if (portUseCount != 0)
        {
            for (size_t j = 0; j < MaxEvents; j++)
            {
                if (!events[j].busy) continue;

                int deltaCount;
                if (events[j].isWorld())
                {
                    if (events[j].isFixed())
                        deltaCount = addFixedContribution(events + j, numSamples);
                    else
                        deltaCount = addWorldContribution(events + j, numSamples);
                }
                else
                    deltaCount = addLocalContribution(events + j, numSamples);
                portUseCount += deltaCount;
            }
        }

        // replace all samples with silence if muting is on
        if (mutingOn)
            numSamples = 0;

        // fill out partial buffers with silence
        for (size_t j = numSamples; j < audioBufferSize; j++)
            scratch[j] = 0.0f;

        // write samples
        media->writeAudioFrames(&scratch.front(), audioBufferSize/2);
    }

    return false;
}

static void       audioLoop(void*)
{
    // loop until requested to stop
    while (true)
    {
        // sleep until audio buffers hit low water mark or new command available
        media->audioSleep(true, endTime);

        if (audioInnerLoop())
            break;
    }
}

void          updateSound()
{
    if (isSoundOpen() && usingSameThread)
    {
        // sleep until audio buffers hit low water mark or new command available
        media->audioSleep(true, 0.0);
        audioInnerLoop();
    }
}

// Local Variables: ***
// mode: C++ ***
// tab-width: 4 ***
// c-basic-offset: 4 ***
// indent-tabs-mode: nil ***
// End: ***
// ex: shiftwidth=4 tabstop=4
