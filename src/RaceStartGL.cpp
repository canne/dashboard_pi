/******************************************************************************
* $Id: RaceStartGL.cpp, v1.0 2019/11/30 VaderDarth Exp $
*
* Project:  OpenCPN
* Purpose:  dahboard_tactics_pi plug-in
* Author:   Jean-Eudes Onfray
* 
***************************************************************************
*   Copyright (C) 2010 by David S. Register   *
*                                                                         *
*   This program is free software; you can redistribute it and/or modify  *
*   it under the terms of the GNU General Public License as published by  *
*   the Free Software Foundation; either version 2 of the License, or     *
*   (at your option) any later version.                                   *
*                                                                         *
*   This program is distributed in the hope that it will be useful,       *
*   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
*   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
*   GNU General Public License for more details.                          *
*                                                                         *
*   You should have received a copy of the GNU General Public License     *
*   along with this program; if not, write to the                         *
*   Free Software Foundation, Inc.,                                       *
*   51 Franklin Street, Fifth Floor, Boston, MA 02110-1301,  USA.         *
***************************************************************************
*/
using namespace std;

// For compilers that support precompilation, includes "wx/wx.h".
#include <wx/wxprec.h>

// for all others, include the necessary headers (this file is usually all you
// need because it includes almost all "standard" wxWidgets headers)
#ifndef WX_PRECOMP
#include <wx/wx.h>
#endif

#include <wx/version.h>
#include <wx/glcanvas.h>

#include "RaceStart.h"
#include "tactics_pi_ext.h"
using namespace std::placeholders;

void DashboardInstrument_RaceStart::ClearRendererCalcs()
{
    m_renStartLineDrawn = false;
    m_renSlineLength = std::nan("1");
    m_renSlineDir = std::nan("1");
    m_renOppositeSlineDir = std::nan("1");
    m_renBiasSlineDir = std::nan("1");
    m_renWindBias = std::nan("1");
    m_renWindBiasAdvDist = std::nan("1");
    m_renWindBiasAdvDir = std::nan("1");
    m_renWindBiasLineDir = std::nan("1");
    m_renWindBiasDrawn = false;
    m_renLLPortDir = std::nan("1");
    m_renLLStbdDir = std::nan("1");
    m_renLaylinesCalculated = false;
    m_renGridBoxDir = std::nan("1");
    m_renGridDirEast = std::nan("1");
    m_renGridDirWest = std::nan("1");
    m_renGridEndOffset = std::nan("1");
    m_renGridLineMaxLen = std::nan("1");
    m_gridStepWestOnStartline = std::nan("1");
    m_gridStepEastOnStartline = std::nan("1");
    m_renGridEndPointStartlineWest_lat = std::nan("1");
    m_renGridEndPointStartlineWest_lon = std::nan("1");
    m_renGridEndPointStartlineEast_lat = std::nan("1");
    m_renGridEndPointStartlineEast_lon = std::nan("1");
    m_renGridEndPointOtherWest_lat = std::nan("1");
    m_renGridEndPointOtherWest_lon = std::nan("1");
    m_renGridEndPointOtherEast_lat = std::nan("1");
    m_renGridEndPointOtherEast_lon = std::nan("1");
    m_renDistanceToStartLine = std::nan("1");
    m_renDistanceCogToStartLine = std::nan("1");
    m_renGridBoxCalculated = false;
    m_renGridDrawn = false;
    m_renZeroBurnDrawn = false;
}

void DashboardInstrument_RaceStart::DoRenderGLOverLay(
    wxGLContext *pcontext, PlugIn_ViewPort *vp )
{
    if ( !(m_startStbdWp) || !(m_startPortWp) ) {
        ClearRendererCalcs();
        return;
    }
    this->RenderGLStartLine( pcontext, vp );
    this->RenderGLWindBias( pcontext, vp );
    this->RenderGLLaylines( pcontext, vp );
    this->RenderGLGrid( pcontext, vp );
    this->CalculateDistancesToStartline( pcontext, vp );
    this->RenderGLZeroBurn(  pcontext, vp );
}

void DashboardInstrument_RaceStart::RenderGLStartLine(
    wxGLContext *pcontext, PlugIn_ViewPort *vp )
{
    GetCanvasPixLL(
        vp, &m_renPointStbd, m_startStbdWp->m_lat, m_startStbdWp->m_lon );
    GetCanvasPixLL(
        vp, &m_renPointPort, m_startPortWp->m_lat, m_startPortWp->m_lon );
    glColor4ub(255, 128, 0, 168); //orange
    glLineWidth(4);
    glBegin(GL_LINES);
    glVertex2d( m_renPointStbd.x, m_renPointStbd.y );
    glVertex2d( m_renPointPort.x, m_renPointPort.y );
    glEnd();
    m_renStartLineDrawn = true;
}

void DashboardInstrument_RaceStart::RenderGLWindBias(
    wxGLContext *pcontext, PlugIn_ViewPort *vp )
{
    double AvgWind = AverageWind->GetAvgWindDir();
    if ( !( m_renStartLineDrawn && !std::isnan( AvgWind )) ) {
        ClearRendererCalcs();
        return;
    }
    if ( (AvgWind >= 90.0) && (AvgWind <= 270.0) ) {
        m_startWestWp = m_startStbdWp;
        m_renPointWest = m_renPointStbd;
        m_startEastWp = m_startPortWp;
        m_renPointEast = m_renPointPort;
        m_renbNorthSector = false;
    } // then wind from southern segment
    else {
        m_startWestWp = m_startPortWp;
        m_renPointWest = m_renPointPort;
        m_startEastWp = m_startStbdWp;
        m_renPointEast = m_renPointStbd;
        m_renbNorthSector = true;
    } // else wind from northern segment
    m_renRealPointWest = m_renPointWest;
    m_renRealPointEast = m_renPointEast;

    DistanceBearingMercator_Plugin(
        m_startEastWp->m_lat, m_startEastWp->m_lon, // "to"
        m_startWestWp->m_lat, m_startWestWp->m_lon, // "from"
        &m_renSlineDir, &m_renSlineLength ); // result
    m_renOppositeSlineDir = m_renSlineDir + 180.0;
    if ( m_renOppositeSlineDir > 360.0 )
        m_renOppositeSlineDir -= 360.;
    double zeroBiasWindDir;
    if ( m_renbNorthSector ) {
        zeroBiasWindDir = m_renSlineDir - 90.0;
        if ( zeroBiasWindDir < 0. ) {
            zeroBiasWindDir = 360. + zeroBiasWindDir;
        }
    }
    else {
        zeroBiasWindDir = m_renSlineDir + 90.0;
    }
    m_renWindBias = getSignedDegRange( zeroBiasWindDir, AvgWind );
    PlugIn_Waypoint *startPointBiasLine;
    PlugIn_Waypoint *endRefPointBiasLine;
    if ( m_renWindBias < 0. ) {
        startPointBiasLine = m_startPortWp;
        endRefPointBiasLine = m_startStbdWp;
        if ( m_renbNorthSector )
            m_renWindBiasLineDir = m_renSlineDir + m_renWindBias;
        else
            m_renWindBiasLineDir = m_renSlineDir -180. + m_renWindBias;
    } // then wind veers
    else {
        startPointBiasLine = m_startStbdWp;
        endRefPointBiasLine = m_startPortWp;
        if ( m_renbNorthSector )
            m_renWindBiasLineDir = m_renSlineDir - 180. + m_renWindBias;
        else
            m_renWindBiasLineDir = m_renSlineDir + m_renWindBias;
    } // else wind backs
    if ( m_renWindBiasLineDir > 360. )
        m_renWindBiasLineDir -= 360.;
    if ( m_renWindBiasLineDir < 0. )
        m_renWindBiasLineDir = 360. + m_renWindBiasLineDir;
    double startPointBiasEnd_lat;
    double startPointBiasEnd_lon;
    PositionBearingDistanceMercator_Plugin(
        startPointBiasLine->m_lat, startPointBiasLine->m_lon,
        m_renWindBiasLineDir, m_renSlineLength,
        &startPointBiasEnd_lat, &startPointBiasEnd_lon );
    DistanceBearingMercator_Plugin(
        startPointBiasEnd_lat, startPointBiasEnd_lon, // "to"
        endRefPointBiasLine->m_lat, endRefPointBiasLine->m_lon, // "from"
        &m_renWindBiasAdvDir, &m_renWindBiasAdvDist ); // result = "advantage" as vector
    m_renWindBiasAdvDist = m_renWindBiasAdvDist * cos( abs(m_renWindBias) * M_PI / 180. );
    GetCanvasPixLL(
        vp, &m_renPointBiasStart,
        startPointBiasLine->m_lat, startPointBiasLine->m_lon );
    GetCanvasPixLL(
        vp, &m_renPointBiasStop, startPointBiasEnd_lat, startPointBiasEnd_lon );
    // draw the wind turning biased "ladder rungs" start, as dotted line
    glEnable(GL_LINE_STIPPLE); // discontinuing line, stipple
    glLineWidth(4);
    glColor4ub(0, 0, 0, 168); // black, somwwhat opaque
    glLineStipple(5, 0xAAAA);  /* long dash */
    // glLineStipple(5, 0x0101);  /*  dotted  */
    // glLineStipple(5, 0x00FF);  /*  dashed  */
    // glLineStipple(5, 0x1C47);  /*  dash/dot/dash */
    glBegin(GL_LINES);
    glVertex2d( m_renPointBiasStart.x, m_renPointBiasStart.y );
    glVertex2d( m_renPointBiasStop.x, m_renPointBiasStop.y );
    glEnd();
    glDisable(GL_LINE_STIPPLE); //Disabling the Line Type.
    m_renWindBiasDrawn = true;
}

void DashboardInstrument_RaceStart::RenderGLLaylines(
    wxGLContext *pcontext, PlugIn_ViewPort *vp )
{
    if ( !( m_renStartLineDrawn && m_renWindBiasDrawn &&
            !std::isnan(m_renSlineDir) && !std::isnan(m_renWindBias) ) ) {
        ClearRendererCalcs();
        return;
    }
    if ( m_renbNorthSector ) {
        m_renLLStbdDir = m_renSlineDir + 45.0 + m_renWindBias;
        m_renLLPortDir = m_renLLStbdDir + 90.;
    }
    else {
        m_renLLPortDir = m_renLLStbdDir - 45.0 - m_renWindBias;
        if ( m_renLLPortDir < 0. )
            m_renLLPortDir += 360.0;
        m_renLLStbdDir = m_renSlineDir - 90.0;
        if ( m_renLLStbdDir < 0. )
            m_renLLStbdDir += 360.0;
    }

    if ( !CalculateGridBox( pcontext, vp ) ) {
        ClearRendererCalcs();
        return;
    } // then give up, avoid crash if angles are beyond our understanding

    if ( !m_renDrawLaylines ) {
            m_renLaylinesCalculated = true;
            return;
    }
    // calculate the righ lenght for the layline so that it ends with the grid
    double llWestStbd_lat;
    double llWestStbd_lon;
    PositionBearingDistanceMercator_Plugin(
        m_startWestWp->m_lat, m_startWestWp->m_lon,
        m_renLLStbdDir, m_renGridLineMaxLen,
        &llWestStbd_lat, &llWestStbd_lon );
    wxPoint llWestEndStbd;
    GetCanvasPixLL( vp, &llWestEndStbd, llWestStbd_lat, llWestStbd_lon );
    wxRealPoint lineEndRealPointWestStbd( llWestEndStbd );
    wxRealPoint squareEndRealPointWestStbd = GetLineIntersection(
        m_renRealPointWest, lineEndRealPointWestStbd,
        m_renGridEndRealPointStartlineWest, m_renGridEndRealPointOtherWest );
    wxPoint squareEndPointWestStbd;
    if ( (squareEndRealPointWestStbd.x == -999.) ||
         (squareEndRealPointWestStbd.y == -999.) ) {
        squareEndRealPointWestStbd = GetLineIntersection(
            m_renRealPointWest, lineEndRealPointWestStbd,
            m_renGridEndRealPointOtherEast, m_renGridEndRealPointOtherWest );
        if ( (squareEndRealPointWestStbd.x == -999.) ||
             (squareEndRealPointWestStbd.y == -999.) ) {
            squareEndRealPointWestStbd = GetLineIntersection(
                m_renRealPointWest, lineEndRealPointWestStbd,
                m_renGridEndRealPointStartlineEast,
                m_renGridEndRealPointOtherEast );
            if ( (squareEndRealPointWestStbd.x == -999.) ||
                 (squareEndRealPointWestStbd.y == -999.) )
                squareEndPointWestStbd = m_renPointWest;
            else
                squareEndPointWestStbd = squareEndRealPointWestStbd;
        }
        else
            squareEndPointWestStbd = squareEndRealPointWestStbd;
    }
    else
        squareEndPointWestStbd = squareEndRealPointWestStbd;

    glColor4ub(0, 200, 0, 128); // green
    glLineWidth(3);
    glBegin(GL_LINES);
    glVertex2d( m_renPointWest.x, m_renPointWest.y );
    glVertex2d( squareEndPointWestStbd.x, squareEndPointWestStbd.y );
    glEnd();

    double llWestPort_lat;
    double llWestPort_lon;
    PositionBearingDistanceMercator_Plugin(
        m_startWestWp->m_lat, m_startWestWp->m_lon,
        m_renLLPortDir, m_renGridLineMaxLen,
        &llWestPort_lat, &llWestPort_lon );
    wxPoint llWestEndPort;
    GetCanvasPixLL( vp, &llWestEndPort, llWestPort_lat, llWestPort_lon );
    wxRealPoint lineEndRealPointWestPort( llWestEndPort );
    wxRealPoint squareEndRealPointWestPort = GetLineIntersection(
        m_renRealPointWest, lineEndRealPointWestPort,
        m_renGridEndRealPointStartlineWest, m_renGridEndRealPointOtherWest );
    wxPoint squareEndPointWestPort;
    if ( (squareEndRealPointWestPort.x == -999.) ||
         (squareEndRealPointWestPort.y == -999.) ) {
        squareEndRealPointWestPort = GetLineIntersection(
            m_renRealPointWest, lineEndRealPointWestPort,
            m_renGridEndRealPointOtherEast, m_renGridEndRealPointOtherWest );
        if ( (squareEndRealPointWestPort.x == -999.) ||
             (squareEndRealPointWestPort.y == -999.) ) {
            squareEndRealPointWestPort = GetLineIntersection(
                m_renRealPointWest, lineEndRealPointWestPort,
                m_renGridEndRealPointStartlineEast,
                m_renGridEndRealPointOtherEast );
            if ( (squareEndRealPointWestPort.x == -999.) ||
                 (squareEndRealPointWestPort.y == -999.) )
                squareEndPointWestPort = m_renPointWest;
            else
                squareEndPointWestPort = squareEndRealPointWestPort;
        }
        else
            squareEndPointWestPort = squareEndRealPointWestPort;
    }
    else
        squareEndPointWestPort = squareEndRealPointWestPort;
 
    glColor4ub(204, 41, 41, 128); // red
    glLineWidth(3);
    glBegin(GL_LINES);
    glVertex2d( m_renPointWest.x, m_renPointWest.y );
    glVertex2d( squareEndPointWestPort.x, squareEndPointWestPort.y );
    glEnd();

    double llEastStbd_lat;
    double llEastStbd_lon;
    PositionBearingDistanceMercator_Plugin(
        m_startEastWp->m_lat, m_startEastWp->m_lon,
        m_renLLStbdDir, m_renGridLineMaxLen,
        &llEastStbd_lat, &llEastStbd_lon );
    wxPoint llEastEndStbd;
    GetCanvasPixLL( vp, &llEastEndStbd, llEastStbd_lat, llEastStbd_lon );
    wxRealPoint lineEndRealPointEastStbd( llEastEndStbd );
    wxRealPoint squareEndRealPointEastStbd = GetLineIntersection(
        m_renRealPointEast, lineEndRealPointEastStbd,
        m_renGridEndRealPointStartlineEast, m_renGridEndRealPointOtherEast );
    wxPoint squareEndPointEastStbd;
    if ( (squareEndRealPointEastStbd.x == -999.) ||
         (squareEndRealPointEastStbd.y == -999.) ) {
        squareEndRealPointEastStbd = GetLineIntersection(
            m_renRealPointEast, lineEndRealPointEastStbd,
            m_renGridEndRealPointOtherEast, m_renGridEndRealPointOtherWest );
        if ( (squareEndRealPointEastStbd.x == -999.) ||
             (squareEndRealPointEastStbd.y == -999.) ) {
            squareEndRealPointEastStbd = GetLineIntersection(
                m_renRealPointEast, lineEndRealPointEastStbd,
                m_renGridEndRealPointStartlineEast,
                m_renGridEndRealPointOtherEast );
            if ( (squareEndRealPointEastStbd.x == -999.) ||
                 (squareEndRealPointEastStbd.y == -999.) )
                squareEndPointEastStbd = m_renPointEast;
            else
                squareEndPointEastStbd = squareEndRealPointEastStbd;
        }
        else
            squareEndPointEastStbd = squareEndRealPointEastStbd;
    }
    else
        squareEndPointEastStbd = squareEndRealPointEastStbd;

    glColor4ub(0, 200, 0, 128); // green
    glLineWidth(3);
    glBegin(GL_LINES);
    glVertex2d( m_renPointEast.x, m_renPointEast.y );
    glVertex2d( squareEndPointEastStbd.x, squareEndPointEastStbd.y );
    glEnd();

    double llEastPort_lat;
    double llEastPort_lon;
    PositionBearingDistanceMercator_Plugin(
        m_startEastWp->m_lat, m_startEastWp->m_lon,
        m_renLLPortDir, m_renGridLineMaxLen,
        &llEastPort_lat, &llEastPort_lon );
    wxPoint llEastEndPort;
    GetCanvasPixLL( vp, &llEastEndPort, llEastPort_lat, llEastPort_lon );
    wxRealPoint lineEndRealPointEastPort( llEastEndPort );
    wxRealPoint squareEndRealPointEastPort = GetLineIntersection(
        m_renRealPointEast, lineEndRealPointEastPort,
        m_renGridEndRealPointStartlineEast, m_renGridEndRealPointOtherEast );
    wxPoint squareEndPointEastPort;
    if ( (squareEndRealPointEastPort.x == -999.) ||
         (squareEndRealPointEastPort.y == -999.) ) {
        squareEndRealPointEastPort = GetLineIntersection(
            m_renRealPointEast, lineEndRealPointEastPort,
            m_renGridEndRealPointOtherEast, m_renGridEndRealPointOtherWest );
        if ( (squareEndRealPointEastPort.x == -999.) ||
             (squareEndRealPointEastPort.y == -999.) ) {
            squareEndRealPointEastPort = GetLineIntersection(
                m_renRealPointEast, lineEndRealPointEastPort,
                m_renGridEndRealPointStartlineWest,
                m_renGridEndRealPointOtherWest );
            if ( (squareEndRealPointEastPort.x == -999.) ||
                 (squareEndRealPointEastPort.y == -999.) )
                squareEndPointEastPort = m_renPointEast;
            else
                squareEndPointEastPort = squareEndRealPointEastPort;
        }
        else
            squareEndPointEastPort = squareEndRealPointEastPort;
    }
    else
        squareEndPointEastPort = squareEndRealPointEastPort;

    glColor4ub(204, 41, 41, 128); // red
    glLineWidth(3);
    glBegin(GL_LINES);
    glVertex2d( m_renPointEast.x, m_renPointEast.y );
    glVertex2d( squareEndPointEastPort.x, squareEndPointEastPort.y );
    glEnd();

    m_renLaylinesCalculated = true;
 }

bool DashboardInstrument_RaceStart::CalculateGridBox(wxGLContext *pcontext, PlugIn_ViewPort *vp) {
    if ( !( m_renStartLineDrawn && m_renWindBiasDrawn &&
            !std::isnan(m_renSlineDir)  && !std::isnan(m_renSlineLength) &&
            !std::isnan(m_renLLPortDir) && !std::isnan(m_renLLStbdDir) ) ) {
        ClearRendererCalcs();
        return false;
    }
    if ( m_renbNorthSector ) {
        m_renGridBoxDir = m_renSlineDir + 90.;
        m_renGridDirWest = m_renLLPortDir;
        m_renGridDirEast = m_renLLStbdDir;
    }
    else {
        m_renGridBoxDir = m_renSlineDir - 90.;
        if ( m_renGridBoxDir < 0 )
            m_renGridBoxDir += 360.;
        m_renGridDirWest = m_renLLStbdDir;
        m_renGridDirEast = m_renLLPortDir;
    }
    // Calculate grid square's corner coordinates on the chart
    m_renGridEndOffset = 
        (m_renSlineLength >= m_renGridSize) ? 0.0 :
        (m_renGridSize - m_renSlineLength)/2.;
    m_renGridLineMaxLen = 1.1 * (1.41421 * m_renGridSize); // cross the opposite line
    // avoid a division by zero in case layline is laying on the startline:
    double stepOnStartLineMax =
        ((m_renGridSize - m_renSlineLength) / 2.) + m_renSlineLength; // toward east
    double maxPossibleDirAngle =
        asin( m_renGridStep / stepOnStartLineMax ) * 180. / M_PI;
    double dirWestOffset = getDegRange(m_renGridDirWest, m_renOppositeSlineDir);
    double angleWestForStartlineStep = (dirWestOffset < maxPossibleDirAngle) ?
        maxPossibleDirAngle : dirWestOffset;
    m_gridStepWestOnStartline =
        m_renGridStep / sin( angleWestForStartlineStep * M_PI / 180. );
    double dirEastOffset = getDegRange(m_renGridDirEast, m_renSlineDir);
    double angleEastForStartlineStep = (dirEastOffset < maxPossibleDirAngle) ?
        maxPossibleDirAngle : dirEastOffset;
    m_gridStepEastOnStartline =
        m_renGridStep / sin( angleEastForStartlineStep * M_PI / 180. );
    
    PositionBearingDistanceMercator_Plugin(
        m_startWestWp->m_lat, m_startWestWp->m_lon,
        m_renOppositeSlineDir, m_renGridEndOffset,
        &m_renGridEndPointStartlineWest_lat, &m_renGridEndPointStartlineWest_lon );
    wxPoint m_renGridEndPointStartlineWest;
    GetCanvasPixLL(
        vp, &m_renGridEndPointStartlineWest,
        m_renGridEndPointStartlineWest_lat, m_renGridEndPointStartlineWest_lon );
    m_renGridEndRealPointStartlineWest = m_renGridEndPointStartlineWest;

    PositionBearingDistanceMercator_Plugin(
        m_renGridEndPointStartlineWest_lat, m_renGridEndPointStartlineWest_lon,
        m_renGridBoxDir, m_renGridSize,
        &m_renGridEndPointOtherWest_lat, &m_renGridEndPointOtherWest_lon );
    wxPoint m_renGridEndPointOtherWest;
    GetCanvasPixLL(
        vp, &m_renGridEndPointOtherWest,
        m_renGridEndPointOtherWest_lat, m_renGridEndPointOtherWest_lon );
    m_renGridEndRealPointOtherWest = m_renGridEndPointOtherWest;
    
    PositionBearingDistanceMercator_Plugin(
        m_startEastWp->m_lat, m_startEastWp->m_lon,
        m_renSlineDir, m_renGridEndOffset,
        &m_renGridEndPointStartlineEast_lat, &m_renGridEndPointStartlineEast_lon );
    wxPoint m_renGridEndPointStartlineEast;
    GetCanvasPixLL(
        vp, &m_renGridEndPointStartlineEast,
        m_renGridEndPointStartlineEast_lat, m_renGridEndPointStartlineEast_lon );
    m_renGridEndRealPointStartlineEast = m_renGridEndPointStartlineEast;

    PositionBearingDistanceMercator_Plugin(
        m_renGridEndPointStartlineEast_lat, m_renGridEndPointStartlineEast_lon,
        m_renGridBoxDir, m_renGridSize,
        &m_renGridEndPointOtherEast_lat, &m_renGridEndPointOtherEast_lon );
    wxPoint m_renGridEndPointOtherEast;
    GetCanvasPixLL(
        vp, &m_renGridEndPointOtherEast,
        m_renGridEndPointOtherEast_lat, m_renGridEndPointOtherEast_lon );
    m_renGridEndRealPointOtherEast = m_renGridEndPointOtherEast;

    m_renGridBoxCalculated = true;
    return true;
}

bool DashboardInstrument_RaceStart::IsSlineWbiasLaylinesGridbox()
{
    if ( ( m_renStartLineDrawn && m_renWindBiasDrawn && m_renLaylinesCalculated &&
           m_renGridBoxCalculated &&
           !std::isnan(m_renSlineDir)  && !std::isnan(m_renSlineLength) &&
           !std::isnan(m_renLLPortDir) && !std::isnan(m_renLLStbdDir) ) )
        return true;
    return false;
}

void DashboardInstrument_RaceStart::RenderGLGrid(
    wxGLContext *pcontext, PlugIn_ViewPort *vp )
{
    if ( !m_renDrawGrid )
        return;

    if ( !IsSlineWbiasLaylinesGridbox() ) {
        ClearRendererCalcs();
        return;

    } // then it is safe to quit now, this routine does not produce any module calcs

    // To avoid jumping of the grid w/ possible wind turn, pivot around point West

    // First, let's move towards the west end of the grid from the point West
    // We will do it twice, first for west oriented line, then for east
    // The order is important since we want to draw more east oriented lines after.

    double distanceToStart = 0.0;
    double distanceToStartLimit = (m_renGridSize - m_renSlineLength) / 2.; // towards west
    double thisPoint_lat = m_startWestWp->m_lat;
    double thisPoint_lon = m_startWestWp->m_lon;
    wxPoint thisPoint;

    bool MoveToNextPoint = ( m_renDrawLaylines ? true : false );
    int  boldLineCounter =
        ( MoveToNextPoint? m_renGridBoldInterval : (m_renGridBoldInterval - 1) );
    
#define __GRID_SET_LINE_CHARACTERISTICS__         if ( boldLineCounter >= m_renGridBoldInterval ) { \
            glLineWidth(2); \
            glColor4ub(166, 166, 166, 138); /* light gray, somewhat opaque */ \
        } \
        else { \
            glLineWidth(1); \
        glColor4ub(191, 191, 191, 168); /* light light gray, opaqueness */ \
        }
        // end of __GRID_SET_LINE_CHARACTERISTICS__

    while ( 1 ) {
        if ( MoveToNextPoint ) {
            // Calculate next point's position on the imaginary endless line
            PositionBearingDistanceMercator_Plugin(
                thisPoint_lat, thisPoint_lon,
                m_renOppositeSlineDir, m_gridStepWestOnStartline,
                &thisPoint_lat, &thisPoint_lon );
        } // then there is a layline, do not paint on it
        MoveToNextPoint = true;
        // Make available for the loop end the distance to the start point
#define __FROM_STARTLINE_SET_NEXT_POINT__ double brg; \
        DistanceBearingMercator_Plugin( \
            thisPoint_lat, thisPoint_lon, \
            m_startWestWp->m_lat, m_startWestWp->m_lon, \
            &brg, &distanceToStart ); \
        if ( distanceToStart >= distanceToStartLimit ) { \
            break; \
        } \
        boldLineCounter++; \
        if ( boldLineCounter > m_renGridBoldInterval ) { \
            boldLineCounter = 1; \
        } \
        GetCanvasPixLL( \
            vp, &thisPoint, \
            thisPoint_lat, thisPoint_lon );

        __FROM_STARTLINE_SET_NEXT_POINT__
        
#define __FROM_LINE_TOWARD_WEST__         double lineEndPointWest_lat; \
        double lineEndPointWest_lon; \
        PositionBearingDistanceMercator_Plugin( \
            thisPoint_lat, thisPoint_lon, \
            m_renGridDirWest, m_renGridLineMaxLen, \
            &lineEndPointWest_lat, &lineEndPointWest_lon ); \
        wxPoint lineEndPointWest; \
        GetCanvasPixLL( \
            vp, &lineEndPointWest, \
            lineEndPointWest_lat, lineEndPointWest_lon ); \
        wxRealPoint lineEndRealPointWest( lineEndPointWest ); \
        wxRealPoint squareEndRealPointWest = GetLineIntersection( \
            thisPoint, lineEndRealPointWest, \
            m_renGridEndRealPointStartlineWest, m_renGridEndRealPointOtherWest ); \
        wxPoint squareEndPointWest; \
        if ( (squareEndRealPointWest.x == -999.) || (squareEndRealPointWest.y == -999.) ) { \
            squareEndRealPointWest = GetLineIntersection( \
                thisPoint, lineEndRealPointWest, \
                m_renGridEndRealPointOtherWest, m_renGridEndRealPointOtherEast ); \
            if ( (squareEndRealPointWest.x == -999.) || (squareEndRealPointWest.y == -999.) ) {  \
                squareEndRealPointWest = GetLineIntersection( \
                    thisPoint, lineEndRealPointWest, \
                    m_renGridEndRealPointStartlineEast, m_renGridEndRealPointOtherEast ); \
                if ( (squareEndRealPointWest.x == -999.) || (squareEndRealPointWest.y == -999.) ) \
                    squareEndPointWest = thisPoint; \
                else \
                    squareEndPointWest = squareEndRealPointWest; \
            } \
            else  \
                squareEndPointWest = squareEndRealPointWest; \
        } \
        else \
            squareEndPointWest = squareEndRealPointWest;

        __FROM_LINE_TOWARD_WEST__

        __GRID_SET_LINE_CHARACTERISTICS__

#define __GRID_DRAW_TO_END_WEST__  glBegin(GL_LINES); \
        glVertex2d( thisPoint.x, thisPoint.y ); \
        glVertex2d( squareEndPointWest.x, squareEndPointWest.y ); \
        glEnd();

        __GRID_DRAW_TO_END_WEST__

    } // while drawing from point West towards grid's west end point

    distanceToStart = 0.0;
    thisPoint_lat = m_startWestWp->m_lat;
    thisPoint_lon = m_startWestWp->m_lon;
    double prevPoint_lat, prevPoint_lon;

    MoveToNextPoint = ( m_renDrawLaylines ? true : false );
    boldLineCounter =
        ( MoveToNextPoint? m_renGridBoldInterval : (m_renGridBoldInterval - 1) );
    
    while ( 1 ) {
        if ( MoveToNextPoint ) {
            prevPoint_lat = thisPoint_lat;
            prevPoint_lon = thisPoint_lon;
            // Calculate next point's position on the imaginary endless line
            PositionBearingDistanceMercator_Plugin(
                thisPoint_lat, thisPoint_lon,
                m_renOppositeSlineDir, m_gridStepEastOnStartline,
                &thisPoint_lat, &thisPoint_lon );
        } // then there is a layline, do not paint on it
        MoveToNextPoint = true;
        // Make available for the loop end the distance to the start point

        __FROM_STARTLINE_SET_NEXT_POINT__
        
#define __FROM_LINE_TOWARD_EAST__         double lineEndPointEast_lat; \
        double lineEndPointEast_lon; \
        PositionBearingDistanceMercator_Plugin( \
            thisPoint_lat, thisPoint_lon, \
            m_renGridDirEast, m_renGridLineMaxLen, \
            &lineEndPointEast_lat, &lineEndPointEast_lon ); \
        wxPoint lineEndPointEast; \
        GetCanvasPixLL( \
            vp, &lineEndPointEast,  \
            lineEndPointEast_lat, lineEndPointEast_lon ); \
        wxRealPoint lineEndRealPointEast( lineEndPointEast ); \
        wxRealPoint squareEndRealPointEast = GetLineIntersection( \
            thisPoint, lineEndRealPointEast, \
            m_renGridEndRealPointStartlineEast, m_renGridEndRealPointOtherEast ); \
        wxPoint squareEndPointEast; \
        if ( (squareEndRealPointEast.x == -999.) || (squareEndRealPointEast.y == -999.) ) { \
            squareEndRealPointEast = GetLineIntersection( \
                thisPoint, lineEndRealPointEast, \
                m_renGridEndRealPointOtherWest, m_renGridEndRealPointOtherEast ); \
            if ( (squareEndRealPointEast.x == -999.) || (squareEndRealPointEast.y == -999.) ) {  \
                squareEndRealPointEast = GetLineIntersection( \
                    thisPoint, lineEndRealPointEast, \
                    m_renGridEndRealPointStartlineWest, m_renGridEndRealPointOtherWest ); \
                if ( (squareEndRealPointEast.x == -999.) || (squareEndRealPointEast.y == -999.) ) \
                    squareEndPointEast = thisPoint; \
                else \
                    squareEndPointEast = squareEndRealPointEast; \
            } \
            else  \
                squareEndPointEast = squareEndRealPointEast; \
        } \
        else \
            squareEndPointEast = squareEndRealPointEast;

        __FROM_LINE_TOWARD_EAST__
        
        __GRID_SET_LINE_CHARACTERISTICS__

#define __GRID_DRAW_TO_END_EAST__  glBegin(GL_LINES); \
        glVertex2d( thisPoint.x, thisPoint.y ); \
        glVertex2d( squareEndPointEast.x, squareEndPointEast.y ); \
        glEnd();

        __GRID_DRAW_TO_END_EAST__

    } // while drawing from point West towards grid's west end point

    // Above, we finished with east-oriented lines starting from startline.
    // We continue now with east-oriented lines but moving along the grid's
    // edge. Note that the step is not the same as on startline but the opposite!

    // Calculate the distance from the last point to the grid corner west/startline
    double projectedLinePointWestSide_lat;
    double projectedLinePointWestSide_lon;
    PositionBearingDistanceMercator_Plugin(
        prevPoint_lat, prevPoint_lon,
        m_renGridDirWest, m_renGridStep,
        &projectedLinePointWestSide_lat, &projectedLinePointWestSide_lon );
    wxPoint projectedLinePointWestSide;
    GetCanvasPixLL(
        vp, &projectedLinePointWestSide, 
        projectedLinePointWestSide_lat, projectedLinePointWestSide_lon );
    wxRealPoint projectedLineRealPointWestSide( projectedLinePointWestSide );
    double projectedLinePointWestToEast_lat;
    double projectedLinePointWestToEast_lon;
    PositionBearingDistanceMercator_Plugin(
        projectedLinePointWestSide_lat, projectedLinePointWestSide_lon,
        m_renGridDirEast, m_renGridLineMaxLen,
        &projectedLinePointWestToEast_lat, &projectedLinePointWestToEast_lon );
    wxPoint projectedLinePointWestToEast;
    GetCanvasPixLL(
        vp, &projectedLinePointWestToEast, 
        projectedLinePointWestToEast_lat, projectedLinePointWestToEast_lon );
    wxRealPoint projectedLineRealPointWestToEast( projectedLinePointWestToEast );
    wxRealPoint squareEndRealPointWestSide = GetLineIntersection(
            projectedLineRealPointWestSide, projectedLineRealPointWestToEast,
            m_renGridEndRealPointStartlineWest, m_renGridEndRealPointOtherWest );
    if ( (squareEndRealPointWestSide.x == -999.) ||
         (squareEndRealPointWestSide.y == -999.) ) {
        thisPoint_lat = prevPoint_lat;
        thisPoint_lon = prevPoint_lon;
    } // else must to this approximation, no intersection was found
    else {
        wxPoint squareEndPointWestSide( squareEndRealPointWestSide );
        GetCanvasLLPix(
            vp, squareEndPointWestSide, 
            &thisPoint_lat, &thisPoint_lon );
    }
    while ( 1 ) {
        boldLineCounter += 1;
        if ( boldLineCounter > m_renGridBoldInterval )
            boldLineCounter = 1;
        GetCanvasPixLL( vp, &thisPoint,
                        thisPoint_lat, thisPoint_lon );

        __FROM_LINE_TOWARD_EAST__
            
        __GRID_SET_LINE_CHARACTERISTICS__
            
        __GRID_DRAW_TO_END_EAST__

        PositionBearingDistanceMercator_Plugin(
            thisPoint_lat, thisPoint_lon,
            m_renGridBoxDir, m_gridStepWestOnStartline, // yes, opposite angle!
            &thisPoint_lat, &thisPoint_lon );
        // Make available for the loop end the distance to the start point
        double brg;
        double distanceToBackWestCorner;
        DistanceBearingMercator_Plugin(
            thisPoint_lat, thisPoint_lon,
            m_renGridEndPointStartlineWest_lat, m_renGridEndPointStartlineWest_lon,
            &brg, &distanceToBackWestCorner );
        if ( distanceToBackWestCorner >= m_renGridSize ) {
            break;
        }

    } // while drawing from grid west side towards east

    // Next, let's move towards the east end of the grid from the point West
    // This time, we draw east-oriented lines first, then finish with west ones

    distanceToStart = 0.0;
    distanceToStartLimit =
        ((m_renGridSize - m_renSlineLength) / 2.) + m_renSlineLength; // towards east
    thisPoint_lat = m_startWestWp->m_lat; 
    thisPoint_lon = m_startWestWp->m_lon;
    boldLineCounter = m_renGridBoldInterval;

    while ( 1 ) {
        // Calculate next point's position on the imaginary endless line
        PositionBearingDistanceMercator_Plugin(
            thisPoint_lat, thisPoint_lon,
            m_renSlineDir, m_gridStepEastOnStartline,
            &thisPoint_lat, &thisPoint_lon );

        __FROM_STARTLINE_SET_NEXT_POINT__

        __FROM_LINE_TOWARD_EAST__

        __GRID_SET_LINE_CHARACTERISTICS__

        __GRID_DRAW_TO_END_EAST__

    } // while drawing from point West towards grid's end point   

    distanceToStart = 0.0;
    thisPoint_lat = m_startWestWp->m_lat; 
    thisPoint_lon = m_startWestWp->m_lon;
    boldLineCounter = m_renGridBoldInterval;

    while ( 1 ) {
        // Calculate next point's position on the imaginary endless line
        prevPoint_lat = thisPoint_lat;
        prevPoint_lon = thisPoint_lon;
        PositionBearingDistanceMercator_Plugin(
            thisPoint_lat, thisPoint_lon,
            m_renSlineDir, m_gridStepEastOnStartline,
            &thisPoint_lat, &thisPoint_lon );

        __FROM_STARTLINE_SET_NEXT_POINT__

        __FROM_LINE_TOWARD_WEST__

        __GRID_SET_LINE_CHARACTERISTICS__

        __GRID_DRAW_TO_END_WEST__

    } // while drawing from point West towards grid's end point

    // This is very much the same as in the opposite edge of the grid, just west.
    
    // Calculate the distance from the last point to the grid corner east/startline
    double projectedLinePointEastSide_lat;
    double projectedLinePointEastSide_lon;
    PositionBearingDistanceMercator_Plugin(
        prevPoint_lat, prevPoint_lon,
        m_renGridDirEast, m_renGridStep,
        &projectedLinePointEastSide_lat, &projectedLinePointEastSide_lon );
    wxPoint projectedLinePointEastSide;
    GetCanvasPixLL(
        vp, &projectedLinePointEastSide, 
        projectedLinePointEastSide_lat, projectedLinePointEastSide_lon );
    wxRealPoint projectedLineRealPointEastSide( projectedLinePointEastSide );
    double projectedLinePointEastToWest_lat;
    double projectedLinePointEastToWest_lon;
    PositionBearingDistanceMercator_Plugin(
        projectedLinePointEastSide_lat, projectedLinePointEastSide_lon,
        m_renGridDirEast, m_renGridLineMaxLen,
        &projectedLinePointEastToWest_lat, &projectedLinePointEastToWest_lon );
    wxPoint projectedLinePointEastToWest;
    GetCanvasPixLL(
        vp, &projectedLinePointEastToWest, 
        projectedLinePointEastToWest_lat, projectedLinePointEastToWest_lon );
    wxRealPoint projectedLineRealPointEastToWest( projectedLinePointEastToWest );
    wxRealPoint squareEndRealPointEastSide = GetLineIntersection(
            projectedLineRealPointEastSide, projectedLineRealPointEastToWest,
            m_renGridEndRealPointStartlineEast, m_renGridEndRealPointOtherEast );
    if ( (squareEndRealPointEastSide.x == -999.) ||
         (squareEndRealPointEastSide.y == -999.) ) {
        thisPoint_lat = prevPoint_lat;
        thisPoint_lon = prevPoint_lon;
    }
    else {
        wxPoint squareEndPointEastSide( squareEndRealPointEastSide );
        GetCanvasLLPix(
            vp, squareEndPointEastSide, 
            &thisPoint_lat, &thisPoint_lon );
    }
    while ( 1 ) {
        boldLineCounter += 1;
        if ( boldLineCounter > m_renGridBoldInterval )
            boldLineCounter = 1;
        GetCanvasPixLL( vp, &thisPoint,
                        thisPoint_lat, thisPoint_lon );

        __FROM_LINE_TOWARD_WEST__
            
        __GRID_SET_LINE_CHARACTERISTICS__
            
        __GRID_DRAW_TO_END_WEST__

        PositionBearingDistanceMercator_Plugin(
            thisPoint_lat, thisPoint_lon,
            m_renGridBoxDir, m_gridStepEastOnStartline,
            &thisPoint_lat, &thisPoint_lon );
        // Make available for the loop end the distance to the start point
        double brg;
        double distanceToBackEastCorner;
        DistanceBearingMercator_Plugin(
            thisPoint_lat, thisPoint_lon,
            m_renGridEndPointStartlineEast_lat, m_renGridEndPointStartlineEast_lon,
            &brg, &distanceToBackEastCorner );
        if ( distanceToBackEastCorner >= m_renGridSize ) {
            break;
        }

    } // while drawing from grid west side towards east

    m_renGridDrawn = true;
}

// When we have a startline, a grid box for starting area we can calculate distances
void DashboardInstrument_RaceStart::CalculateDistancesToStartline(
    wxGLContext* pcontext, PlugIn_ViewPort* vp )
{
    if ( !IsAllMeasurementDataValid() || !IsSlineWbiasLaylinesGridbox() ) {
        m_renDistanceToStartLine = std::nan("1");
        m_renDistanceCogToStartLine = std::nan("1");
        return;
    }
    // Check are we on the 'wrong' side of the startline
    double brgToWest;
    double distanceToWestPoint;
    DistanceBearingMercator_Plugin( m_Lat, m_Lon, // "to" boat
                                    m_startWestWp->m_lat, m_startWestWp->m_lon, 
                                    &brgToWest, &distanceToWestPoint );
    double deltaFromSlineDir = getSignedDegRange( m_renSlineDir, brgToWest );
    bool isOnWrongSide = false;
    if ( deltaFromSlineDir < 0. ) {
        if ( m_renbNorthSector ) {
            isOnWrongSide = true;
        } // then the organizer expects the boats being on the "southern" section
    } // then we are in the "northern" 180-deg section of the startline
    double deltaFromOppositeSlineDir = getSignedDegRange(
        m_renOppositeSlineDir, brgToWest );
    if ( abs( deltaFromOppositeSlineDir ) < 90. ) {
        m_renDistanceToStartLine = distanceToWestPoint;
    } // the the shortest distance is to the west side marker
    else {
        double brgToEast;
        double distanceToEastPoint;
        DistanceBearingMercator_Plugin( m_Lat, m_Lon, // "to" boat
                                        m_startEastWp->m_lat, m_startEastWp->m_lon, 
                                        &brgToEast, &distanceToEastPoint );
        deltaFromSlineDir = getSignedDegRange( m_renSlineDir, brgToEast );
        if ( abs( deltaFromSlineDir ) < 90. ) {
            m_renDistanceToStartLine = distanceToEastPoint;
        } // then the shortest distance is to the east side marker
        else {
            m_renDistanceToStartLine =
                distanceToEastPoint * ((abs(deltaFromSlineDir) - 90.0) * M_PI / 180.);
        } // else the shortest distance is a direct line to a point on startline
    } // else we are either above startline or east to it
    /*
     If we are in the "wrong side" of the startline (racing organizer's point
     of view), then do not attempt to calculate the COG distance to the startline
    */
    if ( isOnWrongSide ) {
        m_renDistanceCogToStartLine = std::nan("1");
        return;
    } // then on the wind side of the startling line, cheating!
    wxPoint boatPositionPoint;
    GetCanvasPixLL(
        vp, &boatPositionPoint, 
        m_Lat, m_Lon );
    wxRealPoint boatPositionRealPoint( boatPositionPoint );
    double projectedCogLinePointEnd_lat;
    double projectedCogLinePointEnd_lon;
    PositionBearingDistanceMercator_Plugin(
        m_Lat, m_Lon,
        m_Cog, m_renGridLineMaxLen, // we must be reasonably close
        &projectedCogLinePointEnd_lat, &projectedCogLinePointEnd_lon );
    wxPoint projectedCogLineEndPoint;
    GetCanvasPixLL(
        vp, &projectedCogLineEndPoint, 
        projectedCogLinePointEnd_lat, projectedCogLinePointEnd_lon );
    wxRealPoint projectedCogLineEndRealPoint( projectedCogLineEndPoint );
    m_renCogCrossingStartlineRealPoint = GetLineIntersection(
            boatPositionRealPoint, projectedCogLineEndPoint,
            m_renGridEndRealPointStartlineWest, m_renGridEndRealPointStartlineEast );
    if ( (m_renCogCrossingStartlineRealPoint.x == -999.) ||
         (m_renCogCrossingStartlineRealPoint.y == -999.) ) {
        m_renDistanceCogToStartLine = std::nan("1");
        return;
    } // then we have a COG which is point out of the startline
    else {
        wxPoint intersectionCogPoint( m_renCogCrossingStartlineRealPoint );
        double intersectionCogPoint_lat;
        double intersectionCogPoint_lon;
        GetCanvasLLPix(
            vp, intersectionCogPoint, 
            &intersectionCogPoint_lat, &intersectionCogPoint_lon );
        double brgToIntersectionCogPoint;
        double distanceToIntersctionCogPoint;
        DistanceBearingMercator_Plugin(
           intersectionCogPoint_lat, intersectionCogPoint_lon, // "to"
           m_Lat, m_Lon, // "from" boat
           &brgToIntersectionCogPoint, &distanceToIntersctionCogPoint );
        // Let's make a sanity check, boat's GPS can be jumping around
        double deltaCogVsCalcBearing = getDegRange(
            m_Cog, brgToIntersectionCogPoint );
        if ( deltaCogVsCalcBearing > RACESTART_COG_MAX_JITTER ) {
            m_renDistanceCogToStartLine = std::nan("1");
            return;
        } // then too much jitter, COG is jumping around
        m_renDistanceCogToStartLine = distanceToIntersctionCogPoint;
    } // else we have a COG which may take us to the startline
    
}

void DashboardInstrument_RaceStart::RenderGLZeroBurn(
    wxGLContext *pcontext, PlugIn_ViewPort *vp )
{
    if ( !IsSlineWbiasLaylinesGridbox() ) {
        ClearRendererCalcs();
        return;
    }
    if ( BoatPolar == nullptr ) {
        if ( g_iDbgRes_Polar_Status != DBGRES_POLAR_INVALID ) {
            wxLogMessage (
                "dashboard_tactics_pi: >>> Missing or invalid Polar file:"
                "no Performance data, Laylines, Polar graphs available. <<<" );
            g_iDbgRes_Polar_Status = DBGRES_POLAR_INVALID;
        } // then debug print
        ClearRendererCalcs();
        return;
    }
    if ( !!BoatPolar->isValid() ) {
        if ( g_iDbgRes_Polar_Status != DBGRES_POLAR_INVALID ) {
            wxLogMessage (
                "dashboard_tactics_pi: >>> Missing or invalid Polar file:"
                "no Performance data, Laylines, Polar graphs available. <<<" );
            g_iDbgRes_Polar_Status = DBGRES_POLAR_INVALID;
        } // then debug print
        ClearRendererCalcs();
        return;
    }
    if ( !std::isnan(m_Twa) && !std::isnan(m_Tws) && !std::isnan(m_Cog) ) {
        ClearRendererCalcs();
        return;
    } // then no reason to continue, no data yet, new cycle.
    

    m_renZeroBurnDrawn = true;
}
