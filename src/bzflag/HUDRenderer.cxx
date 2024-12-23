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

/* interface header */
#include "HUDRenderer.h"

// system headers
#include <time.h>

/* common implementation headers */
#include "BundleMgr.h"
#include "Bundle.h"
#include "FontManager.h"
#include "BZDBCache.h"

/* local implementation headers */
#include "LocalPlayer.h"
#include "World.h"
#include "HUDui.h"
#include "Roaming.h"
#include "playing.h"
#include "TextUtils.h"


//
// HUDRenderer
//

// headingOffset:  the number of degrees from the center of the heading
// strip display to either side.  altitudeOffset is similar.
const float     HUDRenderer::altitudeOffset = 20.0f;
const GLfloat       HUDRenderer::black[3] = { 0.0f, 0.0f, 0.0f };
std::string     HUDRenderer::headingLabel[36];
std::string     HUDRenderer::restartLabelFormat("Press %s to start");
std::string     HUDRenderer::resumeLabel("Press Pause to resume");
std::string     HUDRenderer::autoPilotLabel("AutoPilot on");
std::string     HUDRenderer::cancelDestructLabel("Press Destruct to cancel");
std::string     HUDRenderer::gameOverLabel("GAME OVER");

static const GLfloat whiteColor[3]  = { 1.0f, 1.0f, 1.0f };
static const GLfloat redColor[3]    = { 1.0f, 0.0f, 0.0f };
static const GLfloat yellowColor[3] = { 1.0f, 1.0f, 0.0f };
static const GLfloat greenColor[3]  = { 0.0f, 1.0f, 0.0f };

HUDRenderer::HUDRenderer(const BzfDisplay* _display,
                         const SceneRenderer& renderer) :
    display(_display),
    window(renderer.getWindow()),
    firstRender(true),
    playing(false),
    roaming(false),
    dim(false),
    numPlayers(0),
    timeLeft(~0u),
    playerHasHighScore(false),
    teamHasHighScore(false),
    heading(0.0),
    altitude(0.0),
    altitudeTape(false),
    fps(-1.0),
    drawTime(-1.0),
    restartLabel(restartLabelFormat),
    showCompose(false),
    showCracks(true),
    dater(false),
    lastTimeChange((unsigned long)time(NULL)),
    triangleCount(0),
    radarTriangleCount(0)
{
    if (BZDB.eval("timedate") == 0) //we just want the time
        dater = false;
    else if (BZDB.eval("timedate") == 1) //just the date
        dater = true;

    // initialize colors
    hudColor[0] = 1.0f;
    hudColor[1] = 0.625f;
    hudColor[2] = 0.125f;
    messageColor[0] = 1.0f;
    messageColor[1] = 1.0f;
    messageColor[2] = 1.0f;
    warningColor[0] = 1.0f;
    warningColor[1] = 0.0f;
    warningColor[2] = 0.0f;

    // make sure we're notified when MainWindow resizes
    window.getWindow()->addResizeCallback(resizeCallback, this);

    // initialize heading and altitude labels
    if (headingLabel[0].length() == 0)
    {
        int i;
        for (i = 0; i < 36; i++)
            headingLabel[i] = TextUtils::format("%d", i * 10);
    }

    // initialize clocks
    globalClock.setClock(-1.0f, 0.8f, 0.4f);
    scoreClock.setClock(-1.0f, 0.5f, 0.2f);

    // create compose dialog
    composeTypeIn = new HUDuiTypeIn();
    composeTypeIn->setLabel("Send:");
    composeTypeIn->setMaxLength(MessageLen - 1);
    composeTypeIn->showFocus(false);

    // create scoreboard renderer
    scoreboard = new ScoreboardRenderer();

    // initialize fonts
    resize(true);
}

HUDRenderer::~HUDRenderer()
{
    // don't notify me anymore (cos you can't wake the dead!)
    window.getWindow()->removeResizeCallback(resizeCallback, this);

    // release ui controls
    delete composeTypeIn;
}


ScoreboardRenderer *HUDRenderer::getScoreboard ()
{
    return scoreboard;
}



void            HUDRenderer::resizeCallback(void* self)
{
    ((HUDRenderer*)self)->resize(false);
}

void            HUDRenderer::resize(bool firstTime)
{
    // get important metrics
    const int w = firstTime ? MinX : window.getWidth();
    const int vh = firstTime ? MinY : window.getViewHeight();

    // compute good targeting box sizes
    {
        const float xScale = (float)w / (float) MinX;
        const float yScale = (float)vh / (float) MinY;
        const float scale = (xScale < yScale) ? xScale : yScale;
        const float effScale =  scale * ( 0.7f + RENDERER.getMaxMotionFactor() / 16.667f);
        maxMotionSize = (int)((float)MaxMotionSize * effScale);
        noMotionSize = (int)((float)NoMotionSize * effScale / 2.0f);
        headingOffset = 22.0f * (effScale > 1.0f ? 1.0f : effScale);
    }

    // initialize readout spacings
    headingMarkSpacing = 5.0f * float(maxMotionSize) / headingOffset;
    altitudeMarkSpacing = floorf(5.0f * float(maxMotionSize) / altitudeOffset);

    setBigFontSize(w, vh);
    setAlertFontSize(w, vh);
    setMajorFontSize(w, vh);
    setMinorFontSize(w, vh);
    setHeadingFontSize(w, vh);
    setComposeFontSize(w, vh);
    setLabelsFontSize(w, vh);

    // set scoreboard window size & location
    const float sby = window.getViewHeight() - majorFontHeight - alertFontHeight * 2.0f;
    scoreboard->setWindowSize (0.01f * window.getWidth(), sby,
                               0.98f * window.getWidth(), sby);

    // resize the input prompt
    if (showCompose)
    {
        int cFontFace = composeTypeIn->getFontFace();
        float cFontSize = composeTypeIn->getFontSize();
        if (cFontFace >= 0)
        {
            FontManager &fm = FontManager::instance();
            const float x = fm.getStrLength(cFontFace, cFontSize, composeTypeIn->getLabel()) +
                            fm.getStrLength(cFontFace, cFontSize, "99");
            const float y = 1.0f;
            composeTypeIn->setLabelWidth(x);
            composeTypeIn->setPosition(x, y);
        }
    }

    // resize cracks
    if (showCracks)
        initCracks();
}

int         HUDRenderer::getNoMotionSize() const
{
    return noMotionSize;
}

int         HUDRenderer::getMaxMotionSize() const
{
    return maxMotionSize;
}

void            HUDRenderer::setBigFontSize(int, int height)
{
    const float s = (float)height / 22.0f;
    FontManager &fm = FontManager::instance();
    bigFontFace = fm.getFaceID(BZDB.get("sansSerifFont"));
    bigFontSize = floorf(s);

    restartLabelWidth = fm.getStrLength(bigFontFace, bigFontSize, restartLabel);
    resumeLabelWidth = fm.getStrLength(bigFontFace, bigFontSize, resumeLabel);
    cancelDestructLabelWidth = fm.getStrLength(bigFontFace, bigFontSize, cancelDestructLabel);
    gameOverLabelWidth = fm.getStrLength(bigFontFace, bigFontSize, gameOverLabel);
    autoPilotWidth = fm.getStrLength(bigFontFace, bigFontSize, autoPilotLabel);
}

void            HUDRenderer::setAlertFontSize(int, int height)
{
    const float s = (float)height / 36.0f;
    FontManager &fm = FontManager::instance();
    alertFontFace = fm.getFaceID(BZDB.get("sansSerifFont"));
    alertFontSize = floorf(s);
    alertFontHeight = fm.getStrHeight(alertFontFace, alertFontSize, " ");

    for (int i = 0; i < MaxAlerts; i++)
        if (alertClock[i].isOn())
            alertLabelWidth[i] = fm.getStrLength(alertFontFace, alertFontSize, alertLabel[i]);
}

void            HUDRenderer::setMajorFontSize(int, int height)
{
    const float s = (float)height / 36.0f;
    FontManager &fm = FontManager::instance();
    majorFontFace = fm.getFaceID(BZDB.get("sansSerifFont"));
    majorFontSize = floorf(s);
    majorFontHeight = fm.getStrHeight(majorFontFace, majorFontSize, " ");
}

void            HUDRenderer::setMinorFontSize(int, int height)
{
    FontManager &fm = FontManager::instance();
    minorFontFace = fm.getFaceID(BZDB.get("consoleFont"));

    switch (static_cast<int>(BZDB.eval("scorefontsize")))
    {
    case 0:   // auto
    {
        const float s = (float)height / 72.0f;
        minorFontSize = floorf(s);
        break;
    }
    case 1: // tiny
        minorFontSize = 6;
        break;
    case 2: // small
        minorFontSize = 8;
        break;
    case 3: // medium
        minorFontSize = 12;
        break;
    case 4: // big
        minorFontSize = 16;
        break;
    }
}

void            HUDRenderer::setHeadingFontSize(int, int height)
{
    const float s = (float)height / 144.0f;
    FontManager &fm = FontManager::instance();
    headingFontFace = fm.getFaceID(BZDB.get("sansSerifFont"));
    headingFontSize = floorf(s);

    // compute heading labels and (half) widths
    int i;
    for (i = 0; i < 36; i++)
        headingLabelWidth[i] = 0.5f * fm.getStrLength(headingFontFace, headingFontSize, headingLabel[i]);

    // compute maximum width over all altitude labels
    altitudeLabelMaxWidth = fm.getStrLength(headingFontFace, headingFontSize, "9999");
}

void            HUDRenderer::setComposeFontSize(int, int height)
{
    const float s = (float)height / 72.0f;
    FontManager &fm = FontManager::instance();
    composeFontFace = fm.getFaceID(BZDB.get("consoleFont"));
    composeTypeIn->setFontFace(composeFontFace);
    composeTypeIn->setFontSize(floorf(s));
}

void            HUDRenderer::setLabelsFontSize(int, int height)
{
    const float s = (float)height / 96.0f;
    FontManager &fm = FontManager::instance();
    labelsFontFace = fm.getFaceID(BZDB.get("consoleFont"));
    labelsFontSize = floorf(s);
}

void            HUDRenderer::setColor(float r, float g, float b)
{
    hudColor[0] = r;
    hudColor[1] = g;
    hudColor[2] = b;
}

void            HUDRenderer::setPlaying(bool _playing)
{
    playing = _playing;
}

void            HUDRenderer::setRoaming(bool _roaming)
{
    roaming = _roaming;
}

void            HUDRenderer::setDim(bool _dim)
{
    dim = _dim;
    scoreboard->setDim (_dim);
}

void            HUDRenderer::setPlayerHasHighScore(bool hasHigh)
{
    playerHasHighScore = hasHigh;
}

void            HUDRenderer::setTeamHasHighScore(bool hasHigh)
{
    teamHasHighScore = hasHigh;
}

void            HUDRenderer::setHeading(float angle)
{
    heading = (float)(90.0 - 180.0 * angle / M_PI);
    while (heading < 0.0f) heading += 360.0f;
    while (heading >= 360.0f) heading -= 360.0f;
}

void            HUDRenderer::setAltitude(float _altitude)
{
    altitude = _altitude;
}

void            HUDRenderer::setAltitudeTape(bool on)
{
    altitudeTape = on;
}

void            HUDRenderer::setFPS(float _fps)
{
    fps = _fps;
}

void            HUDRenderer::setDrawTime(float drawTimeInseconds)
{
    const int maxCnt = 10000;
    if (drawTimeInseconds < 0)
    {
        drawTime = drawTimeInseconds;
        return;
    }
    if (drawTime < 0.)
        drawTimeCnt = maxCnt;
    if (drawTimeCnt == maxCnt)
    {
        minDrawTime = drawTimeInseconds;
        maxDrawTime = drawTimeInseconds;
        drawTimeTmp = 0;
        drawTimeCnt = 0;
    }
    else if (drawTime < minDrawTime)
        minDrawTime = drawTime;
    else if (drawTime > maxDrawTime)
        maxDrawTime = drawTime;
    drawTimeTmp += drawTimeInseconds;
    drawTimeCnt++;
    drawTime = drawTimeTmp / drawTimeCnt;
}

void            HUDRenderer::setFrameTriangleCount(int tpf)
{
    triangleCount = tpf;
}

void            HUDRenderer::setFrameRadarTriangleCount(int rtpf)
{
    radarTriangleCount = rtpf;
}

void            HUDRenderer::setAlert(int index, const char* string,
                                      float duration, bool warning)
{
    if (index < 0) index = 0;
    else if (index >= MaxAlerts) index = MaxAlerts - 1;
    if (!string)
        alertClock[index].setClock(0.0f);
    else
    {
        FontManager &fm = FontManager::instance();
        alertLabel[index] = BundleMgr::getCurrentBundle()->getLocalString(string);
        alertLabelWidth[index] = fm.getStrLength(alertFontFace, alertFontSize, alertLabel[index]);
        alertColor[index] = warning ? warningColor : messageColor;
        alertClock[index].setClock(duration);
    }
}

bool            HUDRenderer::getComposing() const
{
    return showCompose;
}

std::string     HUDRenderer::getComposeString() const
{
    return composeTypeIn->getString();
}

// Sets the string and allows editing by default
void            HUDRenderer::setComposeString(const std::string &message) const
{
    composeTypeIn->setEditing(true);
    composeTypeIn->setString(message);
}

// Sets the string and allows you to edit if _allowEdit is true
void            HUDRenderer::setComposeString(const std::string &message,
        bool _allowEdit) const
{
    composeTypeIn->setEditing(_allowEdit);
    composeTypeIn->setString(message);
}


// Set the prompt and allow editing by default
void            HUDRenderer::setComposing(const std::string &prompt)
{
    this->setComposing(prompt, true);
}


// Set the prompt and allow editing or not depending on _allowEdit
void            HUDRenderer::setComposing(const std::string &prompt,
        bool _allowEdit)
{
    showCompose = (prompt.length() != 0);
    if (showCompose)
    {
        composeTypeIn->setEditing(_allowEdit);
        composeTypeIn->setLabel(prompt);
        composeTypeIn->setString("");
        composeTypeIn->setFocus();

        int cFontFace = composeTypeIn->getFontFace();
        float cFontSize = composeTypeIn->getFontSize();
        if (cFontFace >= 0)
        {
            FontManager &fm = FontManager::instance();
            const float x = fm.getStrLength(cFontFace, cFontSize, composeTypeIn->getLabel()) +
                            fm.getStrLength(cFontFace, cFontSize, "99");
            const float y = 1.0f;
            composeTypeIn->setLabelWidth(x);
            composeTypeIn->setPosition(x, y);
            // FIXME what is this supposed to do?
            composeTypeIn->setSize(window.getWidth() - x, fm.getStrHeight(cFontFace, cFontSize, " ") * 0);
        }
    }
    else
        HUDui::setFocus(NULL);
}

void            HUDRenderer::setFlagHelp(FlagType* desc, float duration)
{
    flagHelpClock.setClock(duration);

    // Generate the formatted help for this flag
    flagHelpText = makeHelpString(desc->flagHelp.c_str());

    // count the number of lines in the help message
    flagHelpLines = 0;
    const int helpLength = flagHelpText.size();
    const char* helpMsg = flagHelpText.c_str();
    for (int i = 0; i < helpLength; i++)
        if (helpMsg[i] == '\0')
            flagHelpLines++;
}

void            HUDRenderer::initCracks()
{
    for (int i = 0; i < HUDNumCracks; i++)
    {
        const float d = 0.90f * float(maxMotionSize) * ((float)bzfrand() + 0.90f);
        const float a = (float)(2.0 * M_PI * (double(i) + bzfrand()) / double(HUDNumCracks));
        cracks[i][0][0] = 0.0f;
        cracks[i][0][1] = 0.0f;
        cracks[i][1][0] = d * cosf(a);
        cracks[i][1][1] = d * sinf(a);
        makeCrack(cracks, i, 1, a);
    }
}

void            HUDRenderer::setCracks(bool _showCracks)
{
    if ((showCracks != _showCracks) && _showCracks)
    {
        initCracks();
        crackStartTime = TimeKeeper::getTick();
    }
    showCracks = _showCracks;
}

void            HUDRenderer::addMarker(float _heading, const float *_color )
{
    markers.resize(markers.size() + 1);
    HUDMarker &m = markers[markers.size() - 1];

    _heading = (float)(90.0 - 180.0 * _heading / M_PI);
    while (_heading < 0.0f) _heading += 360.0f;
    while (_heading >= 360.0f) _heading -= 360.0f;
    m.heading = _heading;
    memcpy(m.color, _color, sizeof(m.color));
}


void HUDRenderer::AddEnhancedNamedMarker(const fvec3& pos, const fvec3& color,
        std::string name, bool friendly,
        float zShift)
{
    return;

    EnhancedHUDMarker newMarker(pos, color);
    newMarker.pos.z += zShift;
    newMarker.name = name;
    newMarker.friendly = friendly;
    enhancedMarkers.push_back(newMarker);
}

void HUDRenderer::AddEnhancedMarker(const fvec3& pos, const fvec3& color,
                                    bool friendly, float zShift )
{
    EnhancedHUDMarker newMarker(pos, color);
    newMarker.pos.z += zShift;
    newMarker.friendly = friendly;
    enhancedMarkers.push_back(newMarker);
}

void HUDRenderer::AddLockOnMarker(const fvec3& pos, std::string name,
                                  bool friendly, float zShift )
{
    const fvec3 color(0.75f, 0.125f, 0.125f);
    EnhancedHUDMarker newMarker(pos, color);
    newMarker.pos.z += zShift;
    newMarker.name = name;
    newMarker.friendly = friendly;
    lockOnMarkers.push_back(newMarker);
}

void            HUDRenderer::setRestartKeyLabel(const std::string& label)
{
    restartLabel = TextUtils::format(BundleMgr::getCurrentBundle()->getLocalString(restartLabelFormat).c_str(),
                                     label.c_str());
    FontManager &fm = FontManager::instance();
    restartLabelWidth = fm.getStrLength(bigFontFace, bigFontSize, restartLabel);
}

void            HUDRenderer::setTimeLeft(uint32_t _timeLeft)
{
    timeLeft = _timeLeft;
    timeSet = TimeKeeper::getTick();
}

/* FIXME - makeHelpString should return an array of strings instead of
 * using implicit null chars.
 */
std::string     HUDRenderer::makeHelpString(const char* help) const
{
    if (!help) return std::string();

    FontManager &fm = FontManager::instance();
    static const float spaceWidth = fm.getStrLength(minorFontFace, minorFontSize, " ");

    // find sections of string not more than maxWidth pixels wide
    // and put them into a std::string separated by \0's.
    const float maxWidth = (float)window.getWidth() * 0.75f;
    std::string msg;
    std::string text = BundleMgr::getCurrentBundle()->getLocalString(help);

    char c;
    float wordWidth;
    std::string word = "";
    float currentLineWidth = 0.0f;
    unsigned int position = 0;
    while (position < text.size())
    {
        c = text[position];
        // when we hit a space, append the previous word
        if (c == ' ')
        {
            if (word.size() == 0)
            {
                position++;
                continue;
            }

            wordWidth = fm.getStrLength(minorFontFace, minorFontSize, word);
            msg += c;
            if (wordWidth + currentLineWidth + spaceWidth < maxWidth)
                currentLineWidth += wordWidth;
            else
            {
                msg += '\0';
                currentLineWidth = 0.0f;
            }
            msg.append(word);
            word.resize(0);

        }
        else
            word += c;
        position++;
    }

    if (word.size() > 0)
    {
        wordWidth = fm.getStrLength(minorFontFace, minorFontSize, word);
        if (wordWidth + currentLineWidth + spaceWidth >= maxWidth)
            msg += '\0';
        msg += ' ';
        msg.append(word);
    }

    // append terminating null so line counts are correct
    msg += '\0';

    return msg;
}

void            HUDRenderer::makeCrack(float crackpattern[HUDNumCracks][(1 << HUDCrackLevels) + 1][2], int n, int l,
                                       float a)
{
    if (l >= (1 << (HUDCrackLevels - 1))) return;
    float d = 0.5f * float(maxMotionSize) *
              ((float)bzfrand() + 0.5f) * powf(0.5f, 0.69f * logf(float(l)));
    float newAngle = (float)(a + M_PI * bzfrand() / double(HUDNumCracks));
    crackpattern[n][2*l][0] = crackpattern[n][l][0] + d * cosf(newAngle);
    crackpattern[n][2*l][1] = crackpattern[n][l][1] + d * sinf(newAngle);
    makeCrack(crackpattern, n, 2*l, newAngle);
    d = 0.5f * float(maxMotionSize) *
        ((float)bzfrand() + 0.5f) * powf(0.5f, 0.69f * logf(float(l)));
    newAngle = (float)(a - M_PI * bzfrand() / double(HUDNumCracks));
    crackpattern[n][2*l+1][0] = crackpattern[n][l][0] + d * cosf(newAngle);
    crackpattern[n][2*l+1][1] = crackpattern[n][l][1] + d * sinf(newAngle);
    makeCrack(crackpattern, n, 2*l+1, newAngle);
}

static const float dimFactor = 0.2f;


void HUDRenderer::hudColor3Afv(const float * c, const float a)
{
    if ( dim )
        glColor4f( dimFactor *c[0], dimFactor *c[1], dimFactor *c[2], a );
    else
        glColor4f( c[0],c[1],c[2],a );
}

void            HUDRenderer::drawGeometry()
{
    float lockonSize = 40;

    float segmentation = 360.0f/32.0f;
    float rad = lockonSize * 0.25f;

    // white outline
    hudColor3Afv(whiteColor, 0.85f);
    glLineWidth(4.0f);
    glBegin(GL_LINES);
    glVertex3f(-rad,rad,0.03f);
    glVertex3f(rad,-rad,0.02f);
    // glVertex3f(-lockonSize*xFactor,lockonSize,0.02f);
    // glVertex3f(lockonSize*xFactor,0,0.02f);
    glEnd();

    glBegin(GL_LINES);
    for (float t = 0; t < 360; t += segmentation)
    {
        const float s = (t - segmentation);
        const float tRads = t * DEG2RADf;
        const float sRads = s * DEG2RADf;
        glVertex3f(sinf(sRads) * rad, cosf(sRads) * rad, 0.02f);
        glVertex3f(sinf(tRads) * rad, cosf(tRads) * rad, 0.02f);
    }
    glEnd();

    // red X
    hudColor3Afv(redColor, 0.85f);
    glLineWidth(2.0f);
    glBegin(GL_LINES);
    glVertex3f(-rad,rad,0.03f);
    glVertex3f(rad,-rad,0.02f);
    // glVertex3f(-lockonSize*xFactor,lockonSize,0.03f);
    //  glVertex3f(lockonSize*xFactor,0,0.02f);
    glEnd();

    glBegin(GL_LINES);
    for (float t = 0; t < 360; t += segmentation)
    {
        const float s = (t - segmentation);
        const float tRads = t * DEG2RADf;
        const float sRads = s * DEG2RADf;
        glVertex3f(sinf(sRads) * rad, cosf(sRads) * rad, 0.02f);
        glVertex3f(sinf(tRads) * rad, cosf(tRads) * rad, 0.02f);
    }
    glEnd();

    glLineWidth(2.0f);
}

void            HUDRenderer::render(SceneRenderer& renderer)
{
    if (firstRender)
    {
        firstRender = false;
        resize(false);
    }

    OpenGLGState::resetState();
    if (!BZDB.isTrue("noGUI"))
    {
        if (playing)
            renderPlaying(renderer);
        else if (roaming)
            renderRoaming(renderer);
        else
            renderNotPlaying(renderer);
    }
    else
    {
        const bool showTimes = (fps > 0.0f) || (drawTime > 0.0f) ||
                               (triangleCount > 0) || (radarTriangleCount > 0);
        const bool showTankLabels = roaming &&
                                    BZDB.isTrue("displayLabels");

        if (showCompose || showTimes || showTankLabels)
        {
            // use one-to-one pixel projection
            setOneToOnePrj();

            if (showCompose)
                renderCompose(renderer);
            if (showTimes)
                renderTimes();
            if (showTankLabels)
                renderTankLabels(renderer);

            glPopMatrix();
        }
    }
}

void            HUDRenderer::renderAlerts(void)
{
    const float centerx = 0.5f * (float)window.getWidth();

    FontManager &fm = FontManager::instance();

    float y = (float)window.getViewHeight() +
              -fm.getStrHeight(majorFontFace, majorFontSize, " ") +
              -fm.getStrHeight(alertFontFace, alertFontSize, " ");

    for (int i = 0; i < MaxAlerts; i++)
    {
        if (alertClock[i].isOn())
        {
            hudColor3Afv(alertColor[i], 1.0f);
            std::string newAlertLabel = (dim ? ColorStrings[DimColor] : "") + alertLabel[i];
            // FIXME: this assumes that there's not more than one reset in the string.
            if (dim)
            {
                newAlertLabel.insert(newAlertLabel.find(ColorStrings[ResetColor], 0) - 1
                                     + ColorStrings[ResetColor].size(), ColorStrings[DimColor]);
            }
            fm.drawString(centerx - 0.5f * alertLabelWidth[i], y, 0,
                          alertFontFace, alertFontSize, newAlertLabel);
            y -= fm.getStrHeight(alertFontFace, alertFontSize, " ");
        }
    }
}

void            HUDRenderer::renderStatus(void)
{
    LocalPlayer* myTank = LocalPlayer::getMyTank();
    if (!myTank || !World::getWorld()) return;

    Bundle *bdl = BundleMgr::getCurrentBundle();

    FontManager &fm = FontManager::instance();

    const float h = fm.getStrHeight(majorFontFace, majorFontSize, " ");
    float x = 0.25f * h;
    float y = (float)window.getViewHeight() - h;
    TeamColor teamIndex = myTank->getTeam();
    FlagType* flag = myTank->getFlag();

    // print player name and score in upper left corner in team (tank) color
    if (!roaming && (!playerHasHighScore || scoreClock.isOn()))
    {
        hudColor3Afv(Team::getTankColor(teamIndex), 1.0f);
        fm.drawString(x, y, 0, majorFontFace, majorFontSize,
                      TextUtils::format("%s: %d", myTank->getCallSign(), myTank->getScore()));
    }

    // print flag if player has one in upper right
    if (flag != Flags::Null)
    {
        const std::string flagname = bdl->getLocalString(flag->flagName);
        x = (float)window.getWidth() - 0.25f * h - fm.getStrLength(majorFontFace, majorFontSize, flagname);
        if (flag->endurance == FlagSticky)
            hudColor3Afv(warningColor, 1.0f);
        else
            hudColor3Afv(messageColor, 1.0f);
        fm.drawString(x, y, 0, majorFontFace, majorFontSize, flagname);
    }
    else
    {
        time_t timeNow;
        struct tm userTime;
        time(&timeNow);
        userTime = *localtime(&timeNow);

        // switch date and time if necessary
        if (BZDB.eval("timedate") == 2)
        {
            if (time(NULL) - lastTimeChange >= 2)
            {
                dater = !dater;
                lastTimeChange = (unsigned int)time(NULL);
            }
        }
        else
            dater = (BZDB.eval("timedate") == 1);

        // print time or date
        const std::string datetime = (dater) ?
                                     TextUtils::format("%4d.%02d.%02d", 1900 + userTime.tm_year, userTime.tm_mon + 1, userTime.tm_mday) :
                                     TextUtils::format("%2d:%2.2d", userTime.tm_hour, userTime.tm_min);
        x = (float)window.getWidth() - 0.25f * h - fm.getStrLength(majorFontFace, majorFontSize, datetime);
        hudColor3Afv(messageColor, 1.0f);
        fm.drawString(x, y, 0, majorFontFace, majorFontSize, datetime);
    }

    // print current position of tank
    if (BZDB.isTrue("showCoordinates"))
    {
        Player *target = ROAM.getTargetTank();
        if (!target)
        {
            const std::string pos = TextUtils::format("[%d %d %d]", (int)myTank->getPosition()[0],
                                    (int)myTank->getPosition()[1], (int)myTank->getPosition()[2]);
            x = (float)window.getWidth() - 0.25f * h - fm.getStrLength(majorFontFace, majorFontSize, pos);
            fm.drawString(x, (y - h), 0, majorFontFace, majorFontSize, pos);
        }
    }

    if (roaming && BZDB.isTrue("showVelocities"))
    {
        Player *target = ROAM.getTargetTank();
        if (target)
        {
            float vel[3] = {0};
            memcpy(vel,target->getVelocity(),sizeof(float)*3);

            float apparentVel[3] = {0};
            memcpy(apparentVel,target->getApparentVelocity(),sizeof(float)*3);

            float linSpeed = sqrt(vel[0]*vel[0]+vel[1]*vel[1]);
            float vertSpeed = vel[2];
            float rotSpeed = fabs(target->getAngularVelocity());
            float apparentLinSpeed = sqrt(apparentVel[0]*apparentVel[0]+apparentVel[1]*apparentVel[1]);

            // calc maximum apparent velocity value for each 0.5s interval
            static float maxApparentLinSpeed = 0.0f;
            static TimeKeeper maxApparentLinTime = TimeKeeper::getStartTime();

            if (maxApparentLinSpeed > apparentLinSpeed &&
                    TimeKeeper::getTick() - maxApparentLinTime < 0.5f)
                apparentLinSpeed = maxApparentLinSpeed;
            else
            {
                maxApparentLinSpeed = apparentLinSpeed;
                maxApparentLinTime = TimeKeeper::getTick();
            }

            std::string msg = "X";
            float smallZHeight = fm.getStrHeight(minorFontFace, minorFontSize, msg) * 1.125f;
            float drawY = y - smallZHeight;
            // draw header
            msg = "Target Info";
            x = (float)window.getWidth() - 0.25f * h - fm.getStrLength(minorFontFace, minorFontSize, msg);
            fm.drawString(x, drawY, 0, minorFontFace, minorFontSize, msg);

            // draw the postion
            msg = TextUtils::format("Position [%d %d %d]", (int)target->getPosition()[0], (int)target->getPosition()[1],
                                    (int)target->getPosition()[2]);
            x = (float)window.getWidth() - 0.25f * h - fm.getStrLength(minorFontFace, minorFontSize, msg);
            fm.drawString(x,drawY-smallZHeight, 0, minorFontFace, minorFontSize, msg);

            msg = "Linear Speed:";
            if ( linSpeed > target->getMaxSpeed() )
                msg += "!";

            msg += TextUtils::format("%5.2f", linSpeed);
            if (BZDB.evalInt("showVelocities") > 1)
                msg += TextUtils::format("(%5.2f)", apparentLinSpeed);

            x = (float)window.getWidth() - 0.25f * h - fm.getStrLength(minorFontFace, minorFontSize, msg);
            fm.drawString(x,drawY-smallZHeight*2.0f, 0, minorFontFace, minorFontSize, msg);

            msg = TextUtils::format("Vertical Speed:%5.2f", vertSpeed);
            if (BZDB.evalInt("showVelocities") > 1)
                msg += TextUtils::format("(%5.2f)", apparentVel[2]);

            x = (float)window.getWidth() - 0.25f * h - fm.getStrLength(minorFontFace, minorFontSize, msg);
            fm.drawString(x, drawY-smallZHeight*3.0f, 0, minorFontFace, minorFontSize, msg);

            msg = "Angular Speed:";
            if (rotSpeed > BZDB.eval(StateDatabase::BZDB_TANKANGVEL))
                msg += "!";
            msg += TextUtils::format("%5.2f", rotSpeed);
            x = (float)window.getWidth() - 0.25f * h - fm.getStrLength(minorFontFace, minorFontSize, msg);
            fm.drawString(x,drawY-smallZHeight*4.0f, 0, minorFontFace, minorFontSize, msg);

            float shotTime = (float)target->getShotStatistics()->getLastShotTimeDelta();
            float shotDeviation = (float)target->getShotStatistics()->getLastShotDeviation();

            msg = TextUtils::format("Last Shot Info  Time:%6.4f  Deviation:%6.3f", shotTime, shotDeviation);
            x = (float)window.getWidth() - 0.25f * h - fm.getStrLength(minorFontFace, minorFontSize, msg);
            fm.drawString(x,drawY-smallZHeight*5.0f, 0, minorFontFace, minorFontSize, msg);

            float offset = 6.0f;
            if (BZDB.evalInt("showVelocities") > 2)
            {
                offset += 1.0f;
                msg = TextUtils::format("ReportedHits %d ComputedHits %d ratio %.2f", target->reportedHits, target->computedHits,
                                        target->computedHits == 0 ? 0.0f : (float)target->reportedHits/(float)target->computedHits);
                x = (float)window.getWidth() - 0.25f * h - fm.getStrLength(minorFontFace, minorFontSize, msg);
                fm.drawString(x,drawY-smallZHeight*offset, 0, minorFontFace, minorFontSize, msg);
            }

            offset += 1.5f;
            scoreboard->setTeamScoreY(drawY - smallZHeight*offset);
        }
        else
            scoreboard->setTeamScoreY(0);
    }
    else
        scoreboard->setTeamScoreY(0);


    // print status top-center
    const GLfloat* statusColor = warningColor;
    std::string msg;
    // TODO: the upper 4 values of timeLeft (~0u-3 to ~0u)
    // are reserved for future use as timer flags (e.g. paused)
    if ((timeLeft == 0) || (timeLeft >= (~0u - 3)))
        msg = "";
    else
    {
        int t = timeLeft - (int)(TimeKeeper::getTick() - timeSet);
        if (t < 0) t = 0;
        if (t >= 3600)
            msg = TextUtils::format("%d:%02d:%02d   ", t / 3600, (t / 60) % 60, t % 60);
        else if (t >= 60)
            msg = TextUtils::format("%d:%02d   ", t / 60, t % 60);
        else
            msg = TextUtils::format("0:%02d   ", t);
    }
    if (!roaming)
    {
        switch (myTank->getFiringStatus())
        {
        case LocalPlayer::Deceased:
            msg += bdl->getLocalString("Dead");
            break;

        case LocalPlayer::Ready:
            if (flag != Flags::Null && flag->endurance == FlagSticky &&
                    World::getWorld()->allowShakeTimeout())
            {
                /* have a bad flag -- show time left 'til we shake it */
                statusColor = yellowColor;
                msg = TextUtils::format(bdl->getLocalString("%.1f").c_str(), myTank->getFlagShakingTime());
            }
            else
            {
                statusColor = greenColor;
                msg += bdl->getLocalString("Ready");
            }
            break;

        case LocalPlayer::Loading:

            if (World::getWorld()->getMaxShots() != 0)
            {
                statusColor = redColor;
                msg = TextUtils::format(bdl->getLocalString("Reloaded in %.1f").c_str(), myTank->getReloadTime());
            }
            break;

        case LocalPlayer::Sealed:
            msg += bdl->getLocalString("Sealed");
            break;

        case LocalPlayer::Zoned:
            msg += bdl->getLocalString("Zoned");
            break;
        }
    }

    if (roaming)
    {
        statusColor = messageColor;
        if (dim) msg += ColorStrings[DimColor];
        msg += ROAM.getRoamingLabel();
    }

    x = 0.5f * ((float)window.getWidth() - fm.getStrLength(majorFontFace, majorFontSize, msg));
    hudColor3Afv(statusColor, 1.0f);
    fm.drawString(x, y, 0, majorFontFace, majorFontSize, msg);
}

void            HUDRenderer::renderTankLabels(SceneRenderer& renderer)
{
    if (!World::getWorld()) return;

    int offset = window.getViewHeight() - window.getHeight();

    GLint view[] = {window.getOriginX(), window.getOriginY(),
                    window.getWidth(), window.getHeight()
                   };
    const GLfloat *projf = renderer.getViewFrustum().getProjectionMatrix();
    const GLfloat *modelf = renderer.getViewFrustum().getViewMatrix();

    // convert to doubles
    GLdouble proj[16], model[16];
    for (int j = 0; j < 16; j++)
    {
        proj[j] = projf[j];
        model[j] = modelf[j];
    }

    for (int i = 0; i < curMaxPlayers; i++)
    {
        RemotePlayer *pl = World::getWorld()->getPlayer(i);
        if (pl && pl->isAlive())
        {
            const std::string name = pl->getCallSign();
            double x, y, z;
            hudColor3Afv(Team::getRadarColor(pl->getTeam()), 1.0f);
            gluProject(pl->getPosition()[0], pl->getPosition()[1],
                       pl->getPosition()[2]/*+BZDB.eval(StateDatabase::BZDB_MUZZLEHEIGHT)*3.0f*/, model, proj, view, &x, &y, &z);
            if (z >= 0.0 && z <= 1.0)
            {
                FontManager &fm = FontManager::instance();
                fm.drawString(float(x) - fm.getStrLength(labelsFontFace, labelsFontSize, name) / 2.0f,
                              float(y) + offset - fm.getStrHeight(labelsFontFace, labelsFontSize, name),
                              0, labelsFontFace, labelsFontSize, name);
                FlagType* flag = pl->getFlag();
                if (flag != Flags::Null)
                {
                    std::string flagStr = "(";
                    flagStr += flag->endurance == FlagNormal ? flag->flagName : flag->flagAbbv;
                    flagStr += ")";
                    fm.drawString(float(x) - fm.getStrLength(labelsFontFace, labelsFontSize, flagStr) / 2.0f,
                                  float(y) + offset -
                                  (2.0f * fm.getStrHeight(labelsFontFace, labelsFontSize, flagStr)),
                                  0, labelsFontFace, labelsFontSize, flagStr);
                }

                if (roaming && BZDB.isTrue("showVelocities"))
                {
                    float vel[3] = {0};
                    memcpy(vel,pl->getVelocity(),sizeof(float)*3);
                    std::string speedStr = TextUtils::format("[%5.2f]",sqrt(vel[0]*vel[0]+vel[1]*vel[1]));
                    fm.drawString(float(x) - fm.getStrLength(labelsFontFace, labelsFontSize, speedStr.c_str()) / 2.0f,
                                  float(y) + offset -
                                  (3.0f * fm.getStrHeight(labelsFontFace, labelsFontSize, speedStr.c_str())),
                                  0, labelsFontFace, labelsFontSize, speedStr.c_str());
                }
            }
        }
    }
}

void            HUDRenderer::renderCracks()
{
    double delta = (TimeKeeper::getTick() - crackStartTime) * 5.0;
    if (delta > 1.0)
        delta = 1.0;
    int maxLevels = (int) (HUDCrackLevels * delta);

    glPushMatrix();
    glTranslatef(GLfloat(window.getWidth() >> 1),
                 GLfloat(window.getViewHeight() >> 1), 0.0f);
    glLineWidth(3.0);
    hudColor3Afv(whiteColor, 1.0f);
    glBegin(GL_LINES);
    for (int i = 0; i < HUDNumCracks; i++)
    {
        glVertex2fv(cracks[i][0]);
        glVertex2fv(cracks[i][1]);
        for (int j = 0; j < maxLevels-1; j++)
        {
            const int num = 1 << j;
            for (int k = 0; k < num; k++)
            {
                glVertex2fv(cracks[i][num + k]);
                glVertex2fv(cracks[i][2 * (num + k)]);
                glVertex2fv(cracks[i][num + k]);
                glVertex2fv(cracks[i][2 * (num + k) + 1]);
            }
        }
    }
    glEnd();
    glLineWidth(1.0);
    glPopMatrix();
}

void            HUDRenderer::renderCompose(SceneRenderer&)
{
    composeTypeIn->render();
    OpenGLGState::resetState();
}

void            HUDRenderer::renderTimes(void)
{
    const int centerx = window.getWidth() >> 1;
    const int centery = window.getViewHeight() >> 1;
    FontManager &fm = FontManager::instance();

    // draw frames per second
    if (fps > 0.0f)
    {
        hudColor3Afv(whiteColor, 1.0f);
        fm.drawString((float)(centerx - maxMotionSize), (float)centery + (float)maxMotionSize +
                      3.0f * fm.getStrHeight(headingFontFace, headingFontSize, "0"), 0,
                      headingFontFace, headingFontSize, TextUtils::format("FPS: %d", int(fps)));
    }
    float triCountYOffset = 4.5f;
    if (radarTriangleCount > 0)
    {
        hudColor3Afv(whiteColor, 1.0f);
        fm.drawString((float)(centerx - maxMotionSize), (float)centery + (float)maxMotionSize +
                      triCountYOffset * fm.getStrHeight(headingFontFace, headingFontSize, "0"), 0,
                      headingFontFace, headingFontSize, TextUtils::format("rtris: %d", radarTriangleCount));
        triCountYOffset += 1.5f;
    }
    if (triangleCount > 0)
    {
        hudColor3Afv(whiteColor, 1.0f);
        fm.drawString((float)(centerx - maxMotionSize), (float)centery + (float)maxMotionSize +
                      triCountYOffset * fm.getStrHeight(headingFontFace, headingFontSize, "0"), 0,
                      headingFontFace, headingFontSize, TextUtils::format("tris: %d", triangleCount));
    }
    if (drawTime > 0.0f)
    {
        const std::string buf = TextUtils::format("time: %.2f/%.2f/%.2f ms",
                                minDrawTime * 1000.0f,
                                drawTime * 1000.0f,
                                maxDrawTime * 1000.0f);
        hudColor3Afv(whiteColor, 1.0f);
        fm.drawString((float)(centerx + maxMotionSize) - fm.getStrLength(headingFontFace, headingFontSize, buf),
                      (float)centery + (float)maxMotionSize +
                      3.0f * fm.getStrHeight(headingFontFace, headingFontSize, "0"), 0, headingFontFace, headingFontSize, buf);
    }
}

void HUDRenderer::saveMatrixes(const float *mm, const float *pm )
{
    // ssave off the stuff before we reset it
    for (int i = 0; i < 16; i++)
    {
        modelMatrix[i] = mm[i];
        projMatrix[i] = pm[i];
    }
    glGetIntegerv(GL_VIEWPORT,(GLint*)viewport);
}


void HUDRenderer::drawWaypointMarker(float* color, float alpha, float* object,
                                     const float* viewPos, std::string name,
                                     bool friendly)
{
    double map[3] = {0,0,0};
    double o[3];
    o[0] = object[0];
    o[1] = object[1];
    o[2] = object[2];

    hudColor3Afv( color, alpha );

    gluProject(o[0], o[1], o[2], modelMatrix, projMatrix,
               (GLint*)viewport, &map[0], &map[1], &map[2]);

    float halfWidth = window.getWidth() * 0.5f;
    float halfHeight = window.getHeight() * 0.5f;

    // comp us back to the view
    map[0] -= halfWidth;
    map[1] -= halfHeight;

    const fvec2 headingVec(sinf(heading * DEG2RADf),
                           cosf(heading * DEG2RADf));

    const fvec2 toPosVec((float)object[0] - viewPos[0],
                         (float)object[1] - viewPos[1]);

    if (fvec2::dot(toPosVec, headingVec) <= 1.0f /*0.866f*/)
    {
        if (NEAR_ZERO(map[0], ZERO_TOLERANCE))
        {
            map[0] = -halfWidth;
            map[1] = 0;
        }
        else
        {
            map[0] = -halfWidth * (fabs(map[0])/map[0]);
            map[1] = 0;
        }
    }
    else
    {
        if (map[0] < -halfWidth)
            map[0] = -halfWidth;
        if (map[0] > halfWidth)
            map[0] = halfWidth;

        if (map[1] < -halfHeight)
            map[1] = -halfHeight;
        if (map[1] > halfHeight)
            map[1] = halfHeight;
    }

    glPushMatrix();
    glTranslatef((float)map[0],(float)map[1],0);
    glPushMatrix();
    float triangleSize = BZDB.eval("hudWayPMarkerSize");
    if (name.size())
        triangleSize *= 0.75f;

    if (map[0] == halfWidth && map[1] != -halfHeight && map[1] != halfHeight) // right side
        glRotatef(90,0,0,1);

    if (map[0] == -halfWidth && map[1] != -halfHeight && map[1] != halfHeight)    // Left side
        glRotatef(-90,0,0,1);

    if (map[1] == halfHeight && map[0] != -halfWidth && map[0] != halfWidth)  // Top side
        glRotatef(180,0,0,1);

    if (map[0] == halfWidth && map[1] == -halfHeight) // Lower right
        glRotatef(45,0,0,1);

    if (map[0] == -halfWidth && map[1] == -halfHeight)    // Lower left
        glRotatef(-45,0,0,1);

    if (map[0] == halfWidth && map[1] == halfHeight)  // upper right
        glRotatef(180-45,0,0,1);

    if (map[0] == -halfWidth && map[1] == halfHeight) // upper left
        glRotatef(180+45,0,0,1);

    glBegin(GL_TRIANGLES);
    glVertex2f(0,0);
    glVertex2f(triangleSize,triangleSize);
    glVertex2f(-triangleSize,triangleSize);
    glEnd();

    glBegin(GL_LINE_STRIP);
    glVertex3f(0,0,0.01f);
    glVertex3f(triangleSize,triangleSize,0.01f);
    glVertex3f(-triangleSize,triangleSize,0.01f);
    glEnd();

    if (friendly)
        drawGeometry();

    glPopMatrix();

    if (name.size())
    {
        hudColor3Afv( color, alpha );
        float textOffset = 5.0f;
        float width = FontManager::instance().getStrLength(headingFontFace, headingFontSize, name);
        glEnable(GL_TEXTURE_2D);
        FontManager::instance().drawString(-width * 0.5f, textOffset + triangleSize,
                                           0, headingFontFace, headingFontSize, name);
        glDisable(GL_TEXTURE_2D);
    }

    glPopMatrix();
}


//-------------------------------------------------------------------------
// HUDRenderer::drawLockonMarker
//-------------------------------------------------------------------------

void HUDRenderer::drawLockonMarker(float* color, float alpha, float* object,
                                   const float *viewPos, std::string name,
                                   bool friendly)
{
    double map[3] = {0,0,0};
    double o[3];
    o[0] = object[0];
    o[1] = object[1];
    o[2] = object[2];

    hudColor3Afv( color, alpha );

    gluProject(o[0], o[1], o[2], modelMatrix,projMatrix,
               (GLint*)viewport, &map[0], &map[1], &map[2]);

    float halfWidth = window.getWidth() * 0.5f;
    float halfHeight = window.getHeight() * 0.5f;

    // comp us back to the view
    map[0] -= halfWidth;
    map[1] -= halfHeight;

    const fvec2 headingVec(sinf(heading * DEG2RADf),
                           cosf(heading * DEG2RADf));

    const fvec2 toPosVec((float)object[0] - viewPos[0],
                         (float)object[1] - viewPos[1]);

    if (fvec2::dot(toPosVec, headingVec) <= 1.0f)
    {
        if (NEAR_ZERO(map[0], ZERO_TOLERANCE))
        {
            map[0] = -halfWidth;
            map[1] = 0;
        }
        else
        {
            map[0] = -halfWidth * (fabs(map[0])/map[0]);
            map[1] = 0;
        }
    }
    else
    {
        if ( map[0] < -halfWidth )
            map[0] = -halfWidth;
        if ( map[0] > halfWidth )
            map[0] = halfWidth;

        if ( map[1] < -halfHeight )
            map[1] = -halfHeight;
        if ( map[1] > halfHeight )
            map[1] = halfHeight;
    }

    glPushMatrix();
    glTranslatef((float)map[0],(float)map[1],0);
    glPushMatrix();

    float lockonSize = 40;
    float lockonInset = 15;
    float lockonDeclination = 15;

    glLineWidth(3.0f);

    glBegin(GL_LINE_STRIP);
    glVertex2f(-lockonInset,lockonSize-lockonDeclination);
    glVertex2f(-lockonSize,lockonSize);
    glVertex2f(-lockonSize,-lockonSize);
    glVertex2f(-lockonInset,-lockonSize+lockonDeclination);
    glEnd();

    glBegin(GL_LINE_STRIP);
    glVertex2f(lockonInset,lockonSize-lockonDeclination);
    glVertex2f(lockonSize,lockonSize);
    glVertex2f(lockonSize,-lockonSize);
    glVertex2f(lockonInset,-lockonSize+lockonDeclination);
    glEnd();

    if (friendly)
        drawGeometry();

    glLineWidth(1.0f);

    glPopMatrix();

    if (name.size())
    {
        hudColor3Afv( color, alpha );
        float textOffset = 5.0f;
        float width = FontManager::instance().getStrLength(headingFontFace, headingFontSize, name);
        glEnable(GL_TEXTURE_2D);
        FontManager::instance().drawString(-width * 0.5f, textOffset + lockonSize,
                                           0, headingFontFace, headingFontSize, name);
        glDisable(GL_TEXTURE_2D);
    }

    glPopMatrix();
}

void            HUDRenderer::renderBox(SceneRenderer&)
{
    // get view metrics
    const int width = window.getWidth();
    const int height = window.getHeight();
    const int viewHeight = window.getViewHeight();
    const int ox = window.getOriginX();
    const int oy = window.getOriginY();
    const int centerx = width >> 1;
    const int centery = viewHeight >> 1;
    int i;
    float x, y;

    FontManager &fm = FontManager::instance();

    OpenGLGState::resetState();
    const bool smooth = BZDBCache::smooth;

    // draw targeting box
    hudColor3Afv(hudColor, 1.0f);
    glBegin(GL_LINE_LOOP);
    {
        glVertex2i(centerx - noMotionSize, centery - noMotionSize);
        glVertex2i(centerx + noMotionSize, centery - noMotionSize);
        glVertex2i(centerx + noMotionSize, centery + noMotionSize);
        glVertex2i(centerx - noMotionSize, centery + noMotionSize);
    }
    glEnd();
    glBegin(GL_LINE_LOOP);
    {
        glVertex2i(centerx - maxMotionSize, centery - maxMotionSize);
        glVertex2i(centerx + maxMotionSize, centery - maxMotionSize);
        glVertex2i(centerx + maxMotionSize, centery + maxMotionSize);
        glVertex2i(centerx - maxMotionSize, centery + maxMotionSize);
    }
    glEnd();

    // draw heading strip
    if (true /* always draw heading strip */)
    {
        // first clip to area
        glPushAttrib(GL_SCISSOR_BIT);
        glScissor(ox + centerx - maxMotionSize, oy + height - viewHeight + centery + maxMotionSize - 5,
                  2 * maxMotionSize, 25 + (int)(headingFontSize + 0.5f));

        // draw heading mark
        glBegin(GL_LINES);
        glVertex2i(centerx, centery + maxMotionSize);
        glVertex2i(centerx, centery + maxMotionSize - 5);
        glEnd();

        // figure out which marker is closest to center
        int baseMark = int(heading) / 10;
        // get minimum and maximum visible marks (leave some leeway)
        int minMark = baseMark - int(headingOffset / 10.0f) - 1;
        int maxMark = baseMark + int(headingOffset / 10.0f) + 1;

        // draw tick marks
        glPushMatrix();
        if (smooth)
        {
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
            glEnable(GL_LINE_SMOOTH);
            glEnable(GL_BLEND);
        }
        GLfloat basex = maxMotionSize * (heading - 10.0f * float(minMark)) /
                        headingOffset;
        if (!smooth) basex = floorf(basex);
        glTranslatef((float)centerx - basex, (float)(centery + maxMotionSize), 0.0f);
        x = smooth ? 0.0f : -0.5f;
        glBegin(GL_LINES);
        for (i = minMark; i <= maxMark; i++)
        {
            glVertex2i((int)x, 0);
            glVertex2i((int)x, 8);
            x += headingMarkSpacing;
            glVertex2i((int)x, 0);
            glVertex2i((int)x, 4);
            x += headingMarkSpacing;
        }
        glEnd();

        // back to our regular rendering mode
        if (smooth)
        {
            glDisable(GL_LINE_SMOOTH);
            glDisable(GL_BLEND);
        }
        glPopMatrix();

        bool smoothLabel = smooth;
        x = (float)centerx - basex;
        y = 7.0f + (float)(centery + maxMotionSize);
        if (smoothLabel)
        {
            x -= 0.5f;
            hudColor3Afv(hudColor, basex - floorf(basex));
        }
        for (i = minMark; i <= maxMark; i++)
        {
            fm.drawString(x - headingLabelWidth[(i + 36) % 36], y, 0, headingFontFace,
                          headingFontSize, headingLabel[(i + 36) % 36]);
            x += 2.0f * headingMarkSpacing;
        }
        if (smoothLabel)
        {
            x = (float)centerx - basex + 0.5f;
            basex -= floorf(basex);
            hudColor3Afv(hudColor, 1.0f - basex);
            for (i = minMark; i <= maxMark; i++)
            {
                fm.drawString(x - headingLabelWidth[(i + 36) % 36], y, 0, headingFontFace,
                              headingFontSize, headingLabel[(i + 36) % 36]);
                x += 2.0f * headingMarkSpacing;
            }
        }
        OpenGLGState::resetState();

        // draw markers (give 'em a little more space on the sides)
        glScissor(ox + centerx - maxMotionSize - 8, oy + height - viewHeight + centery + maxMotionSize,
                  2 * maxMotionSize + 16, 10);
        glPushMatrix();
        glTranslatef((float)centerx, (float)(centery + maxMotionSize), 0.0f);
        for (MarkerList::const_iterator it = markers.begin(); it != markers.end(); ++it)
        {
            const HUDMarker &m = *it;
            const float relAngle = fmodf(360.0f + m.heading - heading, 360.0f);
            hudColor3Afv(m.color, 1.0f);
            if (relAngle <= headingOffset || relAngle >= 360.0f - headingOffset)
            {
                // on the visible part of tape
                GLfloat mx = maxMotionSize / headingOffset *
                             ((relAngle < 180.0f) ? relAngle : relAngle - 360.0f);
                glBegin(GL_TRIANGLE_STRIP);
                glVertex2f(mx, 0.0f);
                glVertex2f(mx + 4.0f, 4.0f);
                glVertex2f(mx - 4.0f, 4.0f);
                glVertex2f(mx, 8.0f);
                glEnd();
            }
            else if (relAngle <= 180.0)
            {
                // off to the right
                glBegin(GL_TRIANGLES);
                glVertex2f((float)maxMotionSize, 0.0f);
                glVertex2f((float)maxMotionSize + 4.0f, 4.0f);
                glVertex2f((float)maxMotionSize, 8.0f);
                glEnd();
            }
            else
            {
                // off to the left
                glBegin(GL_TRIANGLES);
                glVertex2f(-(float)maxMotionSize, 0.0f);
                glVertex2f(-(float)maxMotionSize, 8.0f);
                glVertex2f(-(float)maxMotionSize - 4.0f, 4.0f);
                glEnd();
            }
        }
        markers.clear();
        glPopMatrix();
        glPopAttrib();
    }

    // draw altitude strip
    if (altitudeTape)
    {
        // clip to area
        glPushAttrib(GL_SCISSOR_BIT);
        glScissor(ox + centerx + maxMotionSize - 5, oy + height - viewHeight + centery - maxMotionSize,
                  (int)altitudeLabelMaxWidth + 15, 2 * maxMotionSize);

        // draw altitude mark
        hudColor3Afv(hudColor, 1.0f);
        glBegin(GL_LINES);
        glVertex2i(centerx + maxMotionSize, centery);
        glVertex2i(centerx + maxMotionSize - 5, centery);
        glEnd();

        // figure out which marker is closest to center
        int baseMark = int(altitude) / 5;

        // get minimum and maximum visible marks (leave some leeway)
        int minMark = baseMark - int(altitudeOffset / 5.0f) - 1;
        if (minMark < 0) minMark = 0;

        int maxMark = baseMark + int(altitudeOffset / 5.0f) + 1;

        // draw tick marks
        glPushMatrix();
        if (smooth)
        {
            glEnable(GL_LINE_SMOOTH);
            glEnable(GL_BLEND);
        }
        // NOTE: before I (Steve Krenzel) made changes, minMark was always 0, which appears
        // to have made basey always equal 0, maybe I overlooked something
        GLfloat basey = maxMotionSize * (altitude - 5.0f * float(minMark)) /
                        altitudeOffset;
        if (!smooth) basey = floorf(basey);
        glTranslatef((float)(centerx + maxMotionSize),
                     (float)centery - basey, 0.0f);
        y = smooth ? 0.0f : -0.5f;
        glBegin(GL_LINES);
        for (i = minMark; i <= maxMark; i++)
        {
            glVertex2i(0, (int)y);
            glVertex2i(8, (int)y);
            y += altitudeMarkSpacing;
        }
        glEnd();

        // back to our regular rendering mode
        if (smooth)
        {
            glDisable(GL_LINE_SMOOTH);
            glDisable(GL_BLEND);
        }
        glPopMatrix();

        bool smoothLabel = smooth;
        x = (float)(10 + centerx + maxMotionSize);
        y = (float)centery - basey + floorf(fm.getStrHeight(headingFontFace, headingFontSize, "0") / 2);
        if (smoothLabel)
        {
            y -= 0.5f;
            hudColor3Afv(hudColor, basey - floorf(basey));
        }
        for (i = minMark; i <= maxMark; i++)
        {
            fm.drawString(x, y, 0, headingFontFace, headingFontSize, TextUtils::format("%d", i * 5));
            y += altitudeMarkSpacing;
        }
        if (smoothLabel)
        {
            y = (float)centery - basey + floorf(fm.getStrHeight(headingFontFace, headingFontSize, "0") / 2);
            y += 0.5f;
            basey -= floorf(basey);
            hudColor3Afv(hudColor, 1.0f - basey);
            for (i = minMark; i <= maxMark; i++)
            {
                fm.drawString(x, y, 0, headingFontFace, headingFontSize, TextUtils::format("%d", i * 5));
                y += altitudeMarkSpacing;
            }
        }

        glPopAttrib();
    }
}

void HUDRenderer::drawMarkersInView( int centerx, int centery, const LocalPlayer* myTank )
{
    if (myTank)
    {
        glPushMatrix();

        hudColor3Afv( hudColor, 0.5f );

        glTranslatef((float)centerx,(float)centery,0);
        glLineWidth(2.0f);

        // draw any waypoint markers
        for (int i = 0; i < (int)enhancedMarkers.size(); i++)
        {
            drawWaypointMarker(enhancedMarkers[i].color, 0.45f,
                               enhancedMarkers[i].pos, myTank->getPosition(),
                               enhancedMarkers[i].name, enhancedMarkers[i].friendly);
        }

        enhancedMarkers.clear();

        // draw any lockon markers
        for (int i = 0; i < (int)lockOnMarkers.size(); i++)
        {
            drawLockonMarker(lockOnMarkers[i].color, 0.45f,
                             lockOnMarkers[i]. pos,myTank->getPosition(),
                             lockOnMarkers[i].name, lockOnMarkers[i].friendly);
        }

        lockOnMarkers.clear();

        glLineWidth(1.0f);

        glPopMatrix();

        hudColor3Afv( hudColor, 0.5f );
    }
}


void            HUDRenderer::setOneToOnePrj()
{
    // get view metrics
    const int width = window.getWidth();
    const int height = window.getHeight();
    const int viewHeight = window.getViewHeight();
    const int ox = window.getOriginX();
    const int oy = window.getOriginY();

    // use one-to-one pixel projection
    glMatrixMode(GL_PROJECTION);
    window.setProjectionHUD();
    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadIdentity();
}


void            HUDRenderer::coverWhenBurrowed(const LocalPlayer &myTank)
{
    const float z = myTank.getPosition()[2];
    if (z >= 0.0f)
        return;

    const int width = window.getWidth();
    const int viewHeight = window.getViewHeight();

    // cover the lower portion of the screen when burrowed
    GLfloat y2 = z /
                 (BZDB.eval(StateDatabase::BZDB_BURROWDEPTH) - 0.1f) *
                 (float)viewHeight / 2.0f;
    glColor4f(0.02f, 0.01f, 0.01f, 1.0);
    glRectf(0.0f, 0.0f, (float)width, y2);
}


void            HUDRenderer::renderPlaying(SceneRenderer& renderer)
{
    // get view metrics
    const int width = window.getWidth();
    const int height = window.getHeight();
    const int viewHeight = window.getViewHeight();
    const int centerx = width >> 1;
    const int centery = viewHeight >> 1;

    FontManager &fm = FontManager::instance();

    setOneToOnePrj();

    // cover the lower portion of the screen when burrowed
    const LocalPlayer *myTank = LocalPlayer::getMyTank();
    if (myTank)
        coverWhenBurrowed(*myTank);

    // draw shot reload status
    if (BZDB.isTrue("displayReloadTimer"))
        renderShots(myTank);

    // draw cracks
    if (showCracks)
        renderCracks();

    // draw the markers, if we should
    const bool  experimental = (RENDERER.useQuality() >= 3);

    if (!BZDB.isTrue("_forbidMarkers") && experimental)
    {
        glDisable(GL_TEXTURE_2D);

        drawMarkersInView(centerx,centery,myTank);

        glEnable(GL_TEXTURE_2D);
    }


    // draw status line
    renderStatus();

    // draw alert messages
    renderAlerts();

    // show player scoreboard
    scoreboard->setRoaming(roaming);
    scoreboard->render(false);

    // draw flag help
    if (flagHelpClock.isOn())
    {
        int i;
        float y;
        hudColor3Afv(messageColor, 1.0f);
        flagHelpY = (float) ((window.getViewHeight() >> 1) - maxMotionSize);
        y = flagHelpY;
        const char* flagHelpBase = flagHelpText.c_str();
        for (i = 0; i < flagHelpLines; i++)
        {
            y -= fm.getStrHeight(minorFontFace, minorFontSize, "0");
            fm.drawString((float)(centerx - fm.getStrLength(minorFontFace, minorFontSize, flagHelpBase)/2.0),
                          y, 0, minorFontFace, minorFontSize, flagHelpBase);
            while (*flagHelpBase) flagHelpBase++;
            flagHelpBase++;
        }

    }

    if (myTank && globalClock.isOn())
    {
        float yy = 0.5f * (float)height
                   + fm.getStrHeight(bigFontFace, bigFontSize, "0");
        if (myTank->isAutoPilot())
        {
            hudColor3Afv(messageColor, 1.0f);
            fm.drawString(0.5f * ((float)width - autoPilotWidth), yy, 0, bigFontFace,
                          bigFontSize, autoPilotLabel);
        }
    }

    // draw times
    renderTimes();

    // draw message composition
    if (showCompose)
        renderCompose(renderer);

    // draw targeting box
    renderBox(renderer);

    // restore graphics state
    glPopMatrix();
}

void            HUDRenderer::renderNotPlaying(SceneRenderer& renderer)
{
    // get view metrics
    const int width = window.getWidth();
    const int viewHeight = window.getViewHeight();

    FontManager &fm = FontManager::instance();

    // use one-to-one pixel projection
    setOneToOnePrj();

    // draw cracks
    if (showCracks)
        renderCracks();

    // draw status line
    renderStatus();

    // draw alert messages
    renderAlerts();

    // show player scoreboard
    scoreboard->setRoaming(roaming);
    scoreboard->render(true);

    // draw times
    renderTimes();

    // draw message composition
    if (showCompose)
        renderCompose(renderer);

    // tell player what to do to start/resume playing
    LocalPlayer* myTank = LocalPlayer::getMyTank();
    if (myTank && globalClock.isOn())
    {
        float y = 0.5f * (float)viewHeight + fm.getStrHeight(bigFontFace, bigFontSize, "0");
        if (gameOver)
        {
            hudColor3Afv(messageColor, 1.0f);
            fm.drawString(0.5f * ((float)width - gameOverLabelWidth), y, 0,
                          bigFontFace, bigFontSize, gameOverLabel);
        }
        else if (!myTank->isAlive() && !myTank->isExploding())
        {
            hudColor3Afv(messageColor, 1.0f);
            fm.drawString(0.5f * ((float)width - restartLabelWidth), y, 0,
                          bigFontFace, bigFontSize, restartLabel);
        }
        else if (myTank->isPaused())
        {
            hudColor3Afv(messageColor, 1.0f);
            fm.drawString(0.5f * ((float)width - resumeLabelWidth), y, 0,
                          bigFontFace, bigFontSize, resumeLabel);
        }
        else if (myTank->isAutoPilot())
        {
            hudColor3Afv(messageColor, 1.0f);
            fm.drawString(0.5f * ((float)width - autoPilotWidth), y, 0,
                          bigFontFace, bigFontSize, autoPilotLabel);
        }
    }

    // restore graphics state
    glPopMatrix();
}

void            HUDRenderer::renderRoaming(SceneRenderer& renderer)
{
    // get view metrics
    const int width = window.getWidth();
    const int height = window.getHeight();

    FontManager &fm = FontManager::instance();

    // use one-to-one pixel projection
    setOneToOnePrj();

    // black out the underground if we're driving with a tank with BU
    LocalPlayer *myTank = LocalPlayer::getMyTank();
    if (myTank)
        coverWhenBurrowed(*myTank);

    // draw shot reload status
    if ((ROAM.getMode() == Roaming::roamViewFP) && BZDB.isTrue("displayReloadTimer"))
        renderShots(ROAM.getTargetTank());

    // draw alert messages
    renderAlerts();

    // show player scoreboard
    scoreboard->setRoaming(roaming);
    scoreboard->render(false);

    // show tank labels
    if (BZDB.isTrue("displayLabels")) renderTankLabels(renderer);

    // draw times
    renderTimes();

    // draw message composition
    if (showCompose)
        renderCompose(renderer);

    // display game over
    if (myTank && globalClock.isOn())
    {
        float y = 0.5f * (float)height + fm.getStrHeight(bigFontFace, bigFontSize, "0");
        if (gameOver)
        {
            hudColor3Afv(messageColor, 1.0f);
            fm.drawString(0.5f * ((float)width - gameOverLabelWidth), y, 0,
                          bigFontFace, bigFontSize, gameOverLabel);
        }
    }

    renderStatus();

    // draw targeting box
    if (altitude != -1.0f)
        renderBox(renderer);

    // restore graphics state
    glPopMatrix();
}



static int      compare_float(const void* a, const void* b)
{
    const float fa = *((const float*)a);
    const float fb = *((const float*)b);
    if (fa > fb)
        return 1;
    else
        return -1;
}

void            HUDRenderer::renderShots(const Player* target)
{
    // get the target tank
    if (!target) return;

    // get view metrics
    const int width = window.getWidth();
    const int height = window.getHeight();
    const int viewHeight = window.getViewHeight();
    const int centerx = width >> 1;
    const int centery = viewHeight >> 1;

    const int indicatorWidth = width / 50;
    const int indicatorHeight = height / 80;
    const int indicatorSpace = indicatorHeight / 10 + 2;
    const int indicatorLeft = centerx + maxMotionSize + indicatorWidth + 16;
    const int indicatorTop = centery - (int)(0.5f * (indicatorHeight + indicatorSpace) * target->getMaxShots());

    const int maxShots = target->getMaxShots();

    float* factors = new float[maxShots];

    // tally the reload values
    for (int i = 0; i < maxShots; ++i)
    {
        const ShotPath* shot = target->getShot(i);
        factors[i] = 1.0f;
        if (shot)
        {
            const TimeKeeper currentTime = shot->getCurrentTime();
            const TimeKeeper startTime = shot->getStartTime();
            const float reloadTime = shot->getReloadTime();
            factors[i] = float(1 - ((reloadTime - (currentTime - startTime)) / reloadTime));
            if (factors[i] > 1.0f) factors[i] = 1.0f;
        }
    }

    // sort the reload values
    qsort(factors, maxShots, sizeof(float), compare_float);

    // draw the reload values
    glEnable(GL_BLEND);
    for (int i = 0; i < maxShots; ++i)
    {
        const int myWidth = int(indicatorWidth * factors[i]);
        const int myTop = indicatorTop + i * (indicatorHeight + indicatorSpace);
        if (factors[i] < 1.0f)
        {
            hudColor3Afv(greenColor, 0.5f); // green
            glRecti(indicatorLeft, myTop, indicatorLeft + myWidth, myTop + indicatorHeight);
            hudColor3Afv(redColor, 0.5f); // red
            glRecti(indicatorLeft + myWidth + 1, myTop, indicatorLeft + indicatorWidth,
                    myTop + indicatorHeight);
        }
        else
        {
            hudColor3Afv(whiteColor, 0.5f); // white
            glRecti(indicatorLeft, myTop, indicatorLeft + myWidth, myTop + indicatorHeight);
        }
    }
    glDisable(GL_BLEND);

    delete[] factors;
}


// Local Variables: ***
// mode: C++ ***
// tab-width: 4 ***
// c-basic-offset: 4 ***
// indent-tabs-mode: nil ***
// End: ***
// ex: shiftwidth=4 tabstop=4
