/***************************************************************************
 * $Id: dashboard_pi.cpp, v1.0 2010/08/05 SethDart Exp $
 *
 * Project:  OpenCPN
 * Purpose:  Dashboard Plugin
 * Author:   Jean-Eudes Onfray
 *
 ***************************************************************************
 *   Copyright (C) 2010 by David S. Register                               *
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

#include <climits>
#include <cstdint>
#include <cmath>
#include <typeinfo>
#include <random>

#include "wx/wxprec.h"

#ifndef  WX_PRECOMP
#include "wx/wx.h"
#endif //precompiled headers

#include <wx/filename.h>

#include <wx/utils.h>

#include "version.h"

#include "icons.h"
#include "wxJSON/jsonreader.h"
#include "wxJSON/jsonwriter.h"

#include <wx/glcanvas.h>

#include "dashboard_pi.h"

#include "DashboardWindow.h"

#include "DashboardPreferencesDialog.h"


#include "dashboard_pi_ext.h"
// define above variable declarations
wxFont *g_pFontTitle;
wxFont *g_pFontData;
wxFont *g_pFontLabel;
wxFont *g_pFontSmall;
int g_iDashSpeedMax;
int g_iDashCOGDamp;
int g_iDashSpeedUnit;
int g_iDashSOGDamp;
int g_iDashDepthUnit;
int g_iDashDistanceUnit;
int g_iDashWindSpeedUnit;
int g_iDashTemperatureUnit;
int g_iUTCOffset;
double g_dDashDBTOffset;
int      g_sTitleFontSize;
wxString g_sTitleFontFamily;
wxString g_sTitleFontStyle;
wxString g_sTitleFontWeight;
int      g_sDataFontSize;
wxString g_sDataFontFamily;
wxString g_sDataFontStyle;
wxString g_sDataFontWeight;
int      g_sLabelFontSize;
wxString g_sLabelFontFamily;
wxString g_sLabelFontStyle;
wxString g_sLabelFontWeight;
int      g_sSmallFontSize;
wxString g_sSmallFontFamily;
wxString g_sSmallFontStyle;
wxString g_sSmallFontWeight;
wxString g_sDialColorBackground;
wxString g_sDialColorForeground;
wxString g_sDialColorLabel;
wxString g_sDialColorRed;
wxString g_sDialColorGreen;
wxString g_sDialColorIs1;
wxString g_sDialColorIs2;
wxString g_sDialNeedleColor;
wxString g_sDialSecondNeedleColor;
wxString g_sDialCentralCircleColor;
wxString g_sDialColorCompassBackgound;
bool g_bDialNeedleEmbossed;
wxString g_sDialNeedleContourColor;
bool g_bDialShowRedGreen;
int g_iDialLowDegRedGreen;
int g_iDialHighDegRedGreen;

#include "DashboardFunctions.h"

#include "plugin_ids.h"
wxBEGIN_EVENT_TABLE (dashboard_pi, wxTimer)
EVT_TIMER (myID_THREAD_AVGWIND, dashboard_pi::OnAvgWindUpdTimer)
wxEND_EVENT_TABLE ()

#include "plugin_static_ids.h"

const char *dashboard_pi::s_common_name = "DashT";

// the class factories, used to create and destroy instances of the PlugIn

extern "C" DECL_EXP opencpn_plugin* create_pi( void *ppimgr )
{
    // cppcheck-suppress cstyleCast
    return (opencpn_plugin *) new dashboard_pi( ppimgr );
}

extern "C" DECL_EXP void destroy_pi( opencpn_plugin* p )
{
    delete p;
}


//----------------------------------------------------------------------------
//
//    Dashboard PlugIn Implementation
//
//----------------------------------------------------------------------------


//----------------------------------------------------------------------------
//
//          PlugIn initialization and de-init
//
//----------------------------------------------------------------------------

dashboard_pi::dashboard_pi( void *ppimgr ) :
    tactics_pi(), wxTimer( this ), opencpn_plugin_117( ppimgr )
{
    m_nofStreamOut = 0;
    std::unique_lock<std::mutex> init_m_mtxNofStreamOut( m_mtxNofStreamOut, std::defer_lock );
    m_echoStreamerShow = wxEmptyString;
    m_nofStreamInSk = 0;
    std::unique_lock<std::mutex> init_m_mtxNofStreamInSk( m_mtxNofStreamInSk, std::defer_lock );
    m_echoStreamerInSkShow = wxEmptyString;

    m_bToggledStateVisible = false;
    m_iPlugInRequirements = 0;
    m_pluginFrame = NULL;
    m_show_id = 0;
    m_hide_id = 0;
    // cppcheck-suppress noCopyConstructor
    m_NMEA0183 = new NMEA0183();
    ResetAllSourcePriorities();
    mActiveLegInfo = nullptr;
    ClearActiveRouteMessages();
    mUTCDateTime.Set( (time_t) -1 );
    mUTCDateTzOffsetLL = 0LL;
    mUTCRealGpsEpoch = 0LL;
    mGNSSreceivedAtLocalMs = 0LL;
    mGNSSvsLocalTimeDeltaS = 0L;
    mUntrustedLocalTime = false;
    mLogUntrustedLocalTimeNotify = false;
    m_config_version = -1;
    mSrc_Watchdog = 2;
    mHDx_Watchdog = 2;
    mHDT_Watchdog = 2;
    mGPS_Watchdog = 2;
    mVar_Watchdog = 2;
    mStW_Watchdog = 2;
    mSiK_Watchdog = 0;
    mTim_Watchdog = 10;
    mApS_Watchcat = 0;
    mBmajorVersion_warning_given = false;
    mBminorVersion_warning_given = false;
    mSiK_DPT_environmentDepthBelowKeel = false;
    mSiK_navigationGnssMethodQuality = 0;
    APPLYSAVEWININIT;

    // Create the PlugIn icons
    initialize_images();
}

dashboard_pi::~dashboard_pi( void )
{
    delete m_NMEA0183;
    delete _img_dashboard_tactics_pi;
    delete _img_dashboard_tactics;
    delete _img_dial;
    delete _img_instrument;
    delete _img_minus;
    delete _img_plus;
    if ( mActiveLegInfo )
        delete mActiveLegInfo;
}

int dashboard_pi::Init( void )
{
    AddLocaleCatalog( _T("opencpn-dashboard_tactics_pi") );

    g_pFontTitle = new wxFont(
        9, wxFONTFAMILY_SWISS, wxFONTSTYLE_ITALIC, wxFONTWEIGHT_NORMAL );
    g_pFontData = new wxFont(
        12, wxFONTFAMILY_TELETYPE, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_NORMAL );
    g_pFontLabel = new wxFont(
        8, wxFONTFAMILY_TELETYPE, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_NORMAL );
    g_pFontSmall = new wxFont(
        8, wxFONTFAMILY_TELETYPE, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_NORMAL );    

    m_pauimgr = GetFrameAuiManager();
    m_pluginFrame = m_pauimgr->GetManagedWindow();
    m_pauimgr->Connect( wxEVT_AUI_RENDER, wxAuiManagerEventHandler( dashboard_pi::OnAuiRender ),
                        NULL, this );
    m_pauimgr->Connect( wxEVT_AUI_PANE_CLOSE, wxAuiManagerEventHandler( dashboard_pi::OnPaneClose ),
            NULL, this );

    //    Get a pointer to the opencpn configuration object
    m_pconfig = GetOCPNConfigObject();

    //    And load the configuration items

    m_pconfig->SetPath( _T("/PlugIns/DashT") );
    int what_tactics_pi_wants = this->TacticsInit( this, m_pconfig );
    /* Tick for average wind calculations is taken care in Tactics class,
       we host the timers */
    m_avgWindUpdTimer = new wxTimer ( this, myID_THREAD_AVGWIND );
    m_avgWindUpdTimer->Start(1000, wxTIMER_CONTINUOUS);

    LoadConfig();
    
#ifdef OCPN_USE_SVG
    m_toolbar_item_id = InsertPlugInToolSVG(
        this->GetCommonName(),
        _svg_dashboard_tactics, _svg_dashboard_tactics_rollover,
        _svg_dashboard_tactics_toggled,
        wxITEM_CHECK, this->GetShortDescription(), _T( "" ), NULL,
        DASHBOARD_TOOL_POSITION, 0, this);
#else
    // Use memory allocated PNG-images (icons.cpp)
    m_toolbar_item_id = InsertPlugInTool
        (_T(""),
         _img_dashboard_tactics_pi, _img_dashboard_tactics_pi,
         wxITEM_CHECK, this->GetCommonName(), _T(""), NULL,
         DASHBOARD_TOOL_POSITION, 0, this);
#endif // OCPN_USE_SVG


    bool init = true;
    ApplyConfig( init );

    //  If we loaded a version 1 config setup, convert now to version 2
    if(m_config_version == 1) {
        SaveConfig();
    }

    Start( 1000, wxTIMER_CONTINUOUS );

    m_iPlugInRequirements = // dashboard_pi requirements
        WANTS_CURSOR_LATLON | WANTS_TOOLBAR_CALLBACK | INSTALLS_TOOLBAR_TOOL |
        WANTS_PREFERENCES   | WANTS_CONFIG           | WANTS_NMEA_SENTENCES  |
        WANTS_NMEA_EVENTS   | USES_AUI_MANAGER       | WANTS_PLUGIN_MESSAGING;
    m_iPlugInRequirements = what_tactics_pi_wants | m_iPlugInRequirements;
    return m_iPlugInRequirements;
}

bool dashboard_pi::DeInit( void )
{
    SaveConfig();
    if( IsRunning() ) // Timer started?
        Stop(); // Stop timer

    for( size_t i = 0; i < m_ArrayOfDashboardWindow.GetCount(); i++ ) {
        DashboardWindow *dashboard_window = m_ArrayOfDashboardWindow.Item(
            i )->m_pDashboardWindow;
        if( dashboard_window ) {
            m_pauimgr->DetachPane( dashboard_window );
            dashboard_window->PluginIsClosing(); // tell that this is the end
            dashboard_window->Close( true ); // threaded apps will die
            dashboard_window->Destroy(); // brute force for instruments is OK
            m_ArrayOfDashboardWindow.Item( i )->m_pDashboardWindow = nullptr;
        }
    }

    /* Developer's note: is the below even necessary?
       It is done already in the DashboardWindow::~DashboardWindow() ?
       Maybe for the restart, let's check.
       for( size_t i = 0; i < m_ArrayOfDashboardWindow.GetCount(); i++ ) {
          DashboardWindowContainer *pdwc = m_ArrayOfDashboardWindow.Item( i );
          delete pdwc;
       }
    */

    delete g_pFontTitle;
    delete g_pFontData;
    delete g_pFontLabel;
    delete g_pFontSmall;

    this->m_avgWindUpdTimer->Stop();
    delete this->m_avgWindUpdTimer;
    return this->TacticsDeInit();

    return true;
}

void dashboard_pi::ResetAllSourcePriorities()
{
    mPriPosition = 99;
    mPriCOGSOG = 99;
    mPriHeadingT = 99; // True heading
    mPriHeadingM = 99; // Magnetic heading
    mPriVar = 99;
    mPriDateTime = 99;
    mPriAWA = 99; // Relative wind
    mPriTWA = 99; // True wind
    mPriDepth = 99;
    mVar = NAN;
    mSatsInView = 0.0;
    mHdm = 0.0;
}

void dashboard_pi::Notify()
{

    wxDateTime cpuDateTimeNow( wxGetUTCTimeMillis() );
    wxDateTime cpuDateTimeUTC = cpuDateTimeNow.ToUTC();
    time_t cpuTicksNow = cpuDateTimeUTC.GetTicks();
    time_t mGNSSsinceEpoch = mUTCDateTime.GetTicks(); // remember, this has TZ!
    mGNSSvsLocalTimeDeltaS = static_cast<long int>(mGNSSsinceEpoch - cpuTicksNow);
    if ( labs( mGNSSvsLocalTimeDeltaS) >=
         (DBP_I_TIMER_TICK * DBP_I_DATA_TIMEOUT/1000) ) {
        if ( mTim_Watchdog <= 0 ) {
            mUntrustedLocalTime = true; // CPU drifting, or playback from a recording
            if ( !mLogUntrustedLocalTimeNotify ) {
                int gteThan = DBP_I_TIMER_TICK * DBP_I_DATA_TIMEOUT;
                wxLogMessage(
                    "dashboard_tactics_pi: NOTE: CPU clock and GNSS (GPS) time "
                    "difference >= %i ms. Accuracy of timestamps for "
                    "calculated values or for values received from OpenCPN "
                    "with no timestamps is now reduced to the approximation "
                    "based on the last received GNSS (GPS) time, usually "
                    "obtained from the OpenCPN or from a SignalK server node. "
                    "Perhaps the GNSS (GPS) is from a play-back file?",
                    gteThan );
                mLogUntrustedLocalTimeNotify = true;
            }
        }
        else
            mTim_Watchdog--;
    }
    else {
        mTim_Watchdog = 10;
        mUntrustedLocalTime = false; // CPU withing reasonable limit
        if ( mLogUntrustedLocalTimeNotify ) {
            int lessThan = DBP_I_TIMER_TICK * DBP_I_DATA_TIMEOUT;
            wxLogMessage(
                "dashboard_tactics_pi: NOTE: CPU clock and GNSS (GPS) time "
                "difference returned to be < %i ms. Considering this accurate "
                "enough to timestamp with CPU time the calculated values and "
                "the values received from OpenCPN with no timestamps.",
                lessThan );
            mLogUntrustedLocalTimeNotify = false;
        }
    }
    
    SendUtcTimeToAllInstruments( mUTCDateTime );
    for( size_t i = 0; i < m_ArrayOfDashboardWindow.GetCount(); i++ ) {
        DashboardWindow *dashboard_window = m_ArrayOfDashboardWindow.Item(
            i )->m_pDashboardWindow;
        if ( dashboard_window )
            dashboard_window->Refresh();
    }
    //  Manage the watchdogs as left here by the original Dashboard
    mSrc_Watchdog--;
    if( mSrc_Watchdog <= 0 ) {
        ResetAllSourcePriorities();
        // Unlike trad. Dashboard, DashT instruments deal with their own timeouts
        mSrc_Watchdog = gps_watchdog_timeout_ticks;
    }
    if ( mSiK_Watchdog > 0)
        mSiK_Watchdog--; // control the switch between SK and OpenCPN

    mHDx_Watchdog--;
    if( mHDx_Watchdog <= 0 ) {
        mHdm = NAN;
        SendSentenceToAllInstruments( OCPN_DBP_STC_HDM, mHdm, _T("\u00B0") );
        mHDx_Watchdog = gps_watchdog_timeout_ticks;
    }

    mHDT_Watchdog--;
    if( mHDT_Watchdog <= 0 ) {
        SendSentenceToAllInstruments( OCPN_DBP_STC_HDT, NAN, _T("\u00B0T") );
        mHDT_Watchdog = gps_watchdog_timeout_ticks;
    }

    mVar_Watchdog--;
    if( mVar_Watchdog <= 0 ) {
        mVar = NAN;
        mPriVar = 99;
        SendSentenceToAllInstruments( OCPN_DBP_STC_HMV, NAN, _T("\u00B0T") );
        mVar_Watchdog = gps_watchdog_timeout_ticks;
    }

    mStW_Watchdog--;
    if( mStW_Watchdog <= 0 ) {
        SendSentenceToAllInstruments( OCPN_DBP_STC_STW, NAN, "" );
        mStW_Watchdog = gps_watchdog_timeout_ticks;
    }

    mGPS_Watchdog--;
    if( mGPS_Watchdog <= 0 ) {
        SAT_INFO sats[4];
        for(int i=0 ; i < 4 ; i++) {
            sats[i].SatNumber = 0;
            sats[i].SignalToNoiseRatio = 0;
        }
        SendSatInfoToAllInstruments( 0, 1, sats );
        SendSatInfoToAllInstruments( 0, 2, sats );
        SendSatInfoToAllInstruments( 0, 3, sats );

        mSatsInView = 0.;
        SendSentenceToAllInstruments( OCPN_DBP_STC_SAT, 0, _T("") );
        mGPS_Watchdog = gps_watchdog_timeout_ticks;
    }
    this->TacticsNotify();

    if ( APPLYSAVEWINREQUESTED ) {
        ApplyConfig();
        SaveConfig();
    }
    APPLYSAVEWINSERVED;
    return;
}

void dashboard_pi::OnAvgWindUpdTimer(wxTimerEvent &event)
{
    this->OnAvgWindUpdTimer_Tactics();
}

void dashboard_pi::OnAuiRender( wxAuiManagerEvent &event )
{
    event.Skip();
    if ( APPLYSAVEWINRUNNING )
        return;
    DashboardWindow *dashboard_window = nullptr;
    wxAuiPaneInfo pane;
    for( size_t i = 0; i < m_ArrayOfDashboardWindow.GetCount(); i++ ) {
        dashboard_window =
            m_ArrayOfDashboardWindow.Item( i )->m_pDashboardWindow;
        if( dashboard_window ) {
            pane = m_pauimgr->GetPane( dashboard_window );
            if ( pane.IsOk() ) {
                if ( pane.IsDocked() ) {
                    if ( !m_ArrayOfDashboardWindow.Item( i )->m_bIsDocked ) {
                        SetApplySaveWinRequest();
                        return;
                    } // then workaround missing Aui on-docking event
                } // then pane is docked
                else {
                    if ( m_ArrayOfDashboardWindow.Item( i )->m_bIsDocked ) {
                        SetApplySaveWinRequest();
                        return;
                    } // then workaround for missing AUI un-dock pane event
                } // else pane is floating
            } // then valid window pane of the dashboard window
        } // then valid dashboard window in the container
    } // for number of dashboard windows in the container
}

void dashboard_pi::OnPaneClose( wxAuiManagerEvent& event )
{
    // if name is unique, we should use it
    DashboardWindow *dashboard_window =
        static_cast <DashboardWindow *>(event.pane->window);
    int cnt = 0;
    for( size_t i = 0; i < m_ArrayOfDashboardWindow.GetCount(); i++ ) {
        DashboardWindowContainer *cont = m_ArrayOfDashboardWindow.Item( i );
        DashboardWindow *d_w = cont->m_pDashboardWindow;
        if( d_w ) {
            // we must not count this one because it is being closed
            if( dashboard_window != d_w ) {
                wxAuiPaneInfo &pane = m_pauimgr->GetPane( d_w );
                if( pane.IsOk() && pane.IsShown() ) cnt++;
            } else {
                cont->m_bIsVisible = false;
            }
        }
    }
    SetToolbarItemState( m_toolbar_item_id, cnt != 0 );

    event.Skip();
}

int dashboard_pi::GetAPIVersionMajor()
{
    return MY_API_VERSION_MAJOR;
}

int dashboard_pi::GetAPIVersionMinor()
{
    return MY_API_VERSION_MINOR;
}

int dashboard_pi::GetPlugInVersionMajor()
{
    return PLUGIN_VERSION_MAJOR;
}

int dashboard_pi::GetPlugInVersionPatch()
{
    return PLUGIN_VERSION_PATCH;
}
int dashboard_pi::GetPlugInVersionMinor()
{
    return PLUGIN_VERSION_MINOR;
}

wxBitmap *dashboard_pi::GetPlugInBitmap()
{
    return new wxBitmap(_img_dashboard_tactics_pi->ConvertToImage().Copy());
}

wxString dashboard_pi::GetNameVersion()
{
    char name_version[32];
    sprintf( name_version, "v%d.%d.%d",
             PLUGIN_VERSION_MAJOR, PLUGIN_VERSION_MINOR, PLUGIN_VERSION_PATCH );
    wxString retstr(name_version);
    return retstr;
}
wxString dashboard_pi::GetCommonNameVersion()
{
    wxString retstr ( GetCommonName() + " " + GetNameVersion() );
    return retstr;
}

wxString dashboard_pi::GetCommonName()
{
    wxString retstr( s_common_name );
    return retstr;
}


wxString dashboard_pi::GetShortDescription()
{
    return _T("Dashboard,Engine,Energy,Sailing,Race,SignalK,InfluxDB");
}

wxString dashboard_pi::GetLongDescription()
{
    return _("DashT - an Advanced plug-in for OpenCPN:\n"
             "- Navigation, engine/energy\n"
             "- Sailing performance and race\n"
             "- SignalK and InfluxDB.");
}

wxString dashboard_pi::GetStandardPath()
{
    wxString s = wxFileName::GetPathSeparator();
    wxString stdPath  = *GetpPrivateApplicationDataLocation();

    stdPath += _T("plugins");
    if (!wxDirExists(stdPath))
      wxMkdir(stdPath);

    stdPath += s + _T("dashboard_tactics_pi");

    if (!wxDirExists(stdPath))
      wxMkdir(stdPath);

    stdPath += s;
    return stdPath;
}

long long dashboard_pi::checkTimestamp( long long timestamp )
{
    long long datatimestamp = timestamp;
    if ( datatimestamp == 0LL ) {
        wxLongLong wxllNowMs = wxGetUTCTimeMillis();
        if ( mUntrustedLocalTime ) {
            wxLongLong msElapsedSinceLastGNSStime =
                wxllNowMs - mGNSSreceivedAtLocalMs;
            // DEBUG
            // wxString sBuffer = msElapsedSinceLastGNSStime.ToString();
            // wxString sString = sBuffer.wc_str();
            // wxLogMessage(
            //     "dashboard_tactics_pi: checkTimestamp() :\nmsElapsedSinceLastGNSStime : %s",
            //     sString );
            // END DEBUG
            datatimestamp =
                mUTCRealGpsEpoch + msElapsedSinceLastGNSStime.GetValue();
            // DEBUG
            // wxLongLong wxLL = datatimestamp;
            // sBuffer = wxLL.ToString();
            // sString = sBuffer.wc_str();
            // wxLogMessage(
            //     "dashboard_tactics_pi: checkTimestamp() :\ndatatimestamp : %s",
            //     sString );
            // Put the above in https://www.epochconverter.com
            // END DEBUG
            if ( datatimestamp < 0LL ) // the first GNSS data only arrived
                datatimestamp = wxllNowMs.GetValue(); // wait for next 
        }
        else {
            datatimestamp = wxllNowMs.GetValue();
        }
    } // then, oops, the source has no timestamps of its own, let's make one
    return datatimestamp;
}

/* Porting note: this section is the cornerstone of the Tactics porting effort.
   The below private method is about the original dashboard's simple and real
   method: it merely passes to dashboard instruments the NMEA sentences. Now,
   the NMEA sentences are first fed to Tactic's different engines in
   the original method and optional, Tactics specific NMEA sentences are
   passed to instruments here as in the original Dashboard_pi. */
void dashboard_pi::pSendSentenceToAllInstruments(
    unsigned long long st, double value, wxString unit, long long timestamp  )
{
    if ( APPLYSAVEWINRUNNING )
        return;
    long long datatimestamp = checkTimestamp( timestamp );
    // The classical Dashboard-type instrument push data by an ID
    for( size_t i = 0; i < m_ArrayOfDashboardWindow.GetCount(); i++ ) {
        DashboardWindow *dashboard_window =
            m_ArrayOfDashboardWindow.Item( i )->m_pDashboardWindow;
        if( dashboard_window )
            dashboard_window->SendSentenceToAllInstruments(
                st, value, unit, datatimestamp );
    }
    /* An instrument can also subscribe to the classical Dashboard-type
       sentences by their name */
    wxString stToDashboardPath = getDashboardTacticsInstrumentIdStr( st );
    if ( !stToDashboardPath.IsEmpty() )
        SendDataToAllPathSubscribers (
            stToDashboardPath, value, unit, datatimestamp );
}
/* Porting note: with Tactics, new, virtual NMEA sentences are introduced, like
   the true wind calculations. Likewise, the bearing to the TacticsWP
   (if it exists) to performance instruments as a specific NMEA sentence having
   a special unit. Current speed and leeway are also virtual, calculated
   NMEA sentences. We can publish to the outside world target angle information
   and similar to be displayed to the helmsman on a performance instruments.
   This is not enough: it is also possible to make corrections to the
   NMEA sentences coming from the boat's instruments (you need to read
   the excellent documentation of Tactics plugin for the theory).
   For these calculations, Tactics class and its helpers are using the real
   NMEA sentences. The logic of these operations is strict and defined below.
   It is not recommend to alter or experiment with this logic without a good
   understanding of the Tactics class' internal variables and how they get
   their data. A good debugging tool is a must to visualize the actual value
   and unit versus the calculated value(s) and unit(s). */
void dashboard_pi::SendSentenceToAllInstruments(
    unsigned long long st, double value, wxString unit, long long timestamp )
{
    long long datatimestamp = checkTimestamp( timestamp );
    if ( this->SendSentenceToAllInstruments_LaunchTrueWindCalculations(
             st, value ) ) {
        // we have a valid AWS sentence here, it may require heel correction
        double distvalue = value;
        wxString distunit = unit;
        if ( this->SendSentenceToAllInstruments_PerformanceCorrections (
                 st, distvalue, distunit ) ) {
            this->SetCalcVariables(st, distvalue, distunit);
            pSendSentenceToAllInstruments( st, distvalue, distunit, datatimestamp );
        } // then send with corrections
        else {
            this->SetCalcVariables(st, value, unit);
            if ( !( (st == OCPN_DBP_STC_TWA) || (st == OCPN_DBP_STC_TWD)  ||
                    (st == OCPN_DBP_STC_TWS) || (st == OCPN_DBP_STC_TWS2) ) )
                 pSendSentenceToAllInstruments( st, value, unit, datatimestamp );
        } // else send the sentence as it is
        // AWS corrected or not, it is now sent, move to TW calculations
        unsigned long long st_twa, st_tws, st_tws2, st_twd;
        double value_twa, value_tws, value_twd;
        wxString unit_twa, unit_tws, unit_twd;
        long long calctimestamp = 0LL;
        if ( mUntrustedLocalTime )
            calctimestamp = datatimestamp;
        if (this->SendSentenceToAllInstruments_GetCalculatedTrueWind (
                st, value, unit,
                st_twa, value_twa, unit_twa,
                st_tws, st_tws2, value_tws, unit_tws,
                st_twd, value_twd, unit_twd,
                calctimestamp)) {
            pSendSentenceToAllInstruments(
                st_twa, value_twa, unit_twa, calctimestamp );
            pSendSentenceToAllInstruments(
                st_tws, value_tws, unit_tws, calctimestamp );
            pSendSentenceToAllInstruments(
                st_tws2, value_tws, unit_tws, calctimestamp );
            pSendSentenceToAllInstruments(
                st_twd, value_twd, unit_twd, calctimestamp );
            this->SetNMEASentence_Arm_TWD_Watchdog();
            this->SetNMEASentence_Arm_TWS_Watchdog();
        } // then calculated wind values required and need to be distributed
    } // then Tactics true wind calculations
    else {
        // we have sentence which may or may not require correction
        double distvalue = value;
        wxString distunit = unit;
        bool perfCorrections = false;
        if ( this->SendSentenceToAllInstruments_PerformanceCorrections (
                 st, distvalue, distunit ) ) {
            perfCorrections = true;
            this->SetCalcVariables(st, distvalue, distunit);
            pSendSentenceToAllInstruments(
                st, distvalue, distunit, datatimestamp );
        } // then send with corrections
        else {
            this->SetCalcVariables(st, value, unit);
            if ( this->IsConfigSetToForcedTrueWindCalculation() ) {
                if ( !( (st == OCPN_DBP_STC_TWA) || (st == OCPN_DBP_STC_TWD) ||
                        (st == OCPN_DBP_STC_TWS) || (st == OCPN_DBP_STC_TWS2)
                         ) ) {
                    pSendSentenceToAllInstruments(
                        st, value, unit, datatimestamp );
                } // then not TW data from instruments, can send
            } // then forced True Wind calculations, check data type
            else {
                pSendSentenceToAllInstruments(
                    st, value, unit, datatimestamp );
            }
        } // else send the sentence as it is, unless it is True Wind
        // Leeway
        unsigned long long st_leeway;
        double value_leeway;
        wxString unit_leeway;
        long long calctimestamp = 0LL;
        if ( mUntrustedLocalTime )
            calctimestamp = datatimestamp;
        if (this->SendSentenceToAllInstruments_GetCalculatedLeeway (
                st_leeway, value_leeway, unit_leeway, calctimestamp)) {
            pSendSentenceToAllInstruments( st_leeway, value_leeway,
                                           unit_leeway, calctimestamp );
        } // then calculated leeway required, is avalaible can be be distributed
        // Current
        unsigned long long st_currdir, st_currspd;
        double value_currdir, value_currspd;
        wxString unit_currdir, unit_currspd;
        calctimestamp = 0LL;
        if ( mUntrustedLocalTime )
            calctimestamp = datatimestamp;
        if (this->SendSentenceToAllInstruments_GetCalculatedCurrent (
                st,
                (perfCorrections ? distvalue : value),
                (perfCorrections ? distunit : unit),
                st_currdir, value_currdir, unit_currdir,
                st_currspd, value_currspd, unit_currspd, calctimestamp)) {
            pSendSentenceToAllInstruments(
                st_currdir, value_currdir, unit_currdir, calctimestamp );
            pSendSentenceToAllInstruments(
                st_currspd, value_currspd, unit_currspd, calctimestamp );
        } // then calculated current required and need to be distributed
    } // else no true wind calculations
    // Take this opportunity to keep the Tactics performance enginge ticking for rendering
    this->CalculateLaylineDegreeRange();
    this->CalculatePerformanceData();
}

void dashboard_pi::SendDataToAllPathSubscribers (
    wxString path, double value, wxString unit, long long timestamp )
{
    if ( APPLYSAVEWINRUNNING )
        return;

    long long datatimestamp = checkTimestamp( timestamp );
    for( size_t i = 0; i < m_ArrayOfDashboardWindow.GetCount(); i++ ) {
        DashboardWindow *dashboard_window =
            m_ArrayOfDashboardWindow.Item( i )->m_pDashboardWindow;
        if( dashboard_window )
            dashboard_window->SendDataToAllPathSubscribers(
                path, value, unit, datatimestamp);
    }
}

void dashboard_pi::callAllRegisteredGLRenderers( wxGLContext *pcontext, PlugIn_ViewPort *vp, wxString className )
{
    for( size_t i = 0; i < m_ArrayOfDashboardWindow.GetCount(); i++ ) {
        DashboardWindow *dashboard_window =
            m_ArrayOfDashboardWindow.Item( i )->m_pDashboardWindow;
        if( dashboard_window )
            dashboard_window->callAllRegisteredGLRenderers(
                pcontext, vp, className );
    }
}


void dashboard_pi::SendUtcTimeToAllInstruments( wxDateTime value )
{
    for( size_t i = 0; i < m_ArrayOfDashboardWindow.GetCount(); i++ ) {
        DashboardWindow *dashboard_window =
            m_ArrayOfDashboardWindow.Item( i )->m_pDashboardWindow;
        if( dashboard_window )
            dashboard_window->SendUtcTimeToAllInstruments( value );
    }
}

void dashboard_pi::SendSatInfoToAllInstruments( int cnt, int seq, SAT_INFO sats[4] )
{
    for( size_t i = 0; i < m_ArrayOfDashboardWindow.GetCount(); i++ ) {
        DashboardWindow *dashboard_window =
            m_ArrayOfDashboardWindow.Item( i )->m_pDashboardWindow;
        if( dashboard_window )
            dashboard_window->SendSatInfoToAllInstruments( cnt, seq, sats );
    }
}

void dashboard_pi::SetNMEASentence(wxString &sentence)
{
    this->SetNMEASentence (sentence,
                           (wxString *)NULL, (wxString *)NULL,
                           (wxString *)NULL, (wxString *)NULL,
                           0, (wxString *)NULL, NAN, (wxString *)NULL,
                           0LL, (wxString *)NULL );
}

void dashboard_pi::SetNMEASentence(
    // NMEA0183-sentence either from O main, or from Signal K input
    wxString &sentence, wxString *type, wxString *sentenceId, wxString *talker,
    wxString *src, int pgn, wxString *path, double value, wxString *valStr,
    long long timestamp, wxString *key)
{
    bool SignalK = false;
    /* Select datasource: either O's NMEA event distribution or Signal K
       input stream */
    if ( (type != NULL) && (sentenceId != NULL) && (talker != NULL) &&
         (src != NULL) && (path != NULL) && (!std::isnan(value)) ) {
        SignalK = true;
        mSiK_Watchdog = gps_watchdog_timeout_ticks;
        mSrc_Watchdog = gps_watchdog_timeout_ticks;
    } // then Signal K input stream provided data
    else {
        if ( mSiK_Watchdog > 0 ) {
            (*m_NMEA0183) << sentence;  /* peek for exceptions (not sent as
                                           delta by Signal K) */
            if ( m_NMEA0183->PreParse() ) {
                /* list of NMEA-0183 sentences Signal K updates
                   from the TCP delta channel do not have */
                if ( !(m_NMEA0183->LastSentenceIDReceived == _T("XDR")) &&
                     !(m_NMEA0183->LastSentenceIDReceived == _T("GSV")) &&
                     !(m_NMEA0183->LastSentenceIDReceived == _T("MTA")) &&
                     !(m_NMEA0183->LastSentenceIDReceived == _T("MWD")) &&
                     !(m_NMEA0183->LastSentenceIDReceived == _T("VWT")) &&
                     !(m_NMEA0183->LastSentenceIDReceived == _T("ZDA")) )
                    return; /* no reason to interleave NMEA-0183 coming from
                               OpenCPN with Signal K */
                mSrc_Watchdog = gps_watchdog_timeout_ticks;
            }
            else
                return; // failure in NMEA sentence
        } /* else Signal K is active; this is an interelaving NMEA-0183
             coming via OpenCPN */
        else {
            (*m_NMEA0183) << sentence;
            if ( !m_NMEA0183->PreParse() )
                return; // failure in NMEA sentence
            mSrc_Watchdog = gps_watchdog_timeout_ticks;
        }  /* else no Signal K, this is a normal cycle with NMEA-0183
              coming from OpenCPN  */
    } // else this is NMEA-0183 coming via OpenCPN

    if ( !SignalK ) {

        if ( m_NMEA0183->LastSentenceIDReceived == _T("DBT") ) {
            if ( m_NMEA0183->Parse() ) {
                if ( mPriDepth >= 2 ) {
                    mPriDepth = 2;

                    /*
                      double m_NMEA0183->Dbt.DepthFeet;
                      double m_NMEA0183->Dbt.DepthMeters;
                      double m_NMEA0183->Dbt.DepthFathoms;
                    */
                    double depth = 999.;
                    if ( m_NMEA0183->Dbt.DepthMeters != 999. )
                        depth = m_NMEA0183->Dbt.DepthMeters;
                    else if ( m_NMEA0183->Dbt.DepthFeet != 999. )
                        depth = m_NMEA0183->Dbt.DepthFeet * 0.3048;
                    else if ( m_NMEA0183->Dbt.DepthFathoms != 999. )
                        depth = m_NMEA0183->Dbt.DepthFathoms * 1.82880;
                    depth += g_dDashDBTOffset;
                    SendSentenceToAllInstruments(
                        OCPN_DBP_STC_DPT,
                        toUsrDistance_Plugin( depth / 1852.0,
                                              g_iDashDepthUnit ),
                        getUsrDistanceUnit_Plugin( g_iDashDepthUnit ) );
                }
            }
        }

        else if ( m_NMEA0183->LastSentenceIDReceived == _T("DPT") ) {
            if ( m_NMEA0183->Parse() ) {
                if ( mPriDepth >= 1 ) {
                    mPriDepth = 1;
                    /* double m_NMEA0183->Dpt.DepthMeters
                       double m_NMEA0183->Dpt.OffsetFromTransducerMeters */
                    double depth = 999.;
                    if ( m_NMEA0183->Dpt.DepthMeters != 999. )
                        depth = m_NMEA0183->Dpt.DepthMeters;
                    if ( m_NMEA0183->Dpt.OffsetFromTransducerMeters != 999. )
                        depth += m_NMEA0183->Dpt.OffsetFromTransducerMeters;
                    depth += g_dDashDBTOffset;
                    SendSentenceToAllInstruments(
                        OCPN_DBP_STC_DPT,
                        toUsrDistance_Plugin( depth / 1852.0,
                                              g_iDashDepthUnit ),
                        getUsrDistanceUnit_Plugin( g_iDashDepthUnit ) );
                }
            }
        }
        // TODO: GBS - GPS Satellite fault detection
        else if ( m_NMEA0183->LastSentenceIDReceived == _T("GGA") ) {
            if ( m_NMEA0183->Parse() ) {
                if ( m_NMEA0183->Gga.GPSQuality > 0 ) {
                    if ( mPriPosition >= 3 ) {
                        mPriPosition = 3;
                        double lat, lon;
                        float llt = m_NMEA0183->Gga.Position.Latitude.Latitude;
                        int lat_deg_int = (int) ( llt / 100 );
                        float lat_deg = lat_deg_int;
                        float lat_min = llt - ( lat_deg * 100 );
                        lat = lat_deg + ( lat_min / 60. );
                        if ( m_NMEA0183->Gga.Position.Latitude.Northing ==
                            South )
                            lat = -lat;
                        SendSentenceToAllInstruments(
                            OCPN_DBP_STC_LAT, lat, _T("SDMM") );
                        float lln =
                            m_NMEA0183->Gga.Position.Longitude.Longitude;
                        int lon_deg_int = (int) ( lln / 100 );
                        float lon_deg = lon_deg_int;
                        float lon_min = lln - ( lon_deg * 100 );
                        lon = lon_deg + ( lon_min / 60. );
                        if ( m_NMEA0183->Gga.Position.Longitude.Easting ==
                            West ) lon = -lon;
                        SendSentenceToAllInstruments(
                            OCPN_DBP_STC_LON, lon, _T("SDMM") );
                    }

                    if ( mPriDateTime >= 4 ) {
                        // Not in use, we need the date too.
                        //mPriDateTime = 4;
                        //mUTCDateTime.ParseFormat(
                        //    m_NMEA0183->Gga.UTCTime.c_str(), _T("%H%M%S") );
                    }

                    mSatsInView = m_NMEA0183->Gga.NumberOfSatellitesInUse;
                }
            }
        }

        else if ( m_NMEA0183->LastSentenceIDReceived == _T("GLL") ) {
            if ( m_NMEA0183->Parse() ) {
                if ( m_NMEA0183->Gll.IsDataValid == NTrue ) {
                    if ( mPriPosition >= 1 ) {
                        mPriPosition = 1;
                        double lat, lon;
                        float llt = m_NMEA0183->Gll.Position.Latitude.Latitude;
                        int lat_deg_int = (int) ( llt / 100 );
                        float lat_deg = lat_deg_int;
                        float lat_min = llt - ( lat_deg * 100 );
                        lat = lat_deg + ( lat_min / 60. );
                        if ( m_NMEA0183->Gll.Position.Latitude.Northing ==
                            South )
                            lat = -lat;
                        SendSentenceToAllInstruments(
                            OCPN_DBP_STC_LAT, lat, _T("SDMM") );
                        float lln =
                            m_NMEA0183->Gll.Position.Longitude.Longitude;
                        int lon_deg_int = (int) ( lln / 100 );
                        float lon_deg = lon_deg_int;
                        float lon_min = lln - ( lon_deg * 100 );
                        lon = lon_deg + ( lon_min / 60. );
                        if ( m_NMEA0183->Gll.Position.Longitude.Easting ==
                            West )
                            lon = -lon;
                        SendSentenceToAllInstruments(
                            OCPN_DBP_STC_LON, lon, _T("SDMM") );
                    }

                    if ( mPriDateTime >= 5 ) {
                        // Not in use, we need the date too.
                        //mPriDateTime = 5;
                        //mUTCDateTime.ParseFormat(
                        //    m_NMEA0183->Gll.UTCTime.c_str(), _T("%H%M%S") );
                    }
                }
            }
        }

        else if ( m_NMEA0183->LastSentenceIDReceived == _T("GSV") ) {
            if ( m_NMEA0183->Parse() ) {
                mSatsInView = m_NMEA0183->Gsv.SatsInView;
                // m_NMEA0183->Gsv.NumberOfMessages;
                SendSentenceToAllInstruments(
                    OCPN_DBP_STC_SAT,
                    m_NMEA0183->Gsv.SatsInView,
                    _T("") );
                SendSatInfoToAllInstruments(
                    m_NMEA0183->Gsv.SatsInView,
                    m_NMEA0183->Gsv.MessageNumber,
                    m_NMEA0183->Gsv.SatInfo );
                mGPS_Watchdog = gps_watchdog_timeout_ticks;
            }
        }

        else if ( m_NMEA0183->LastSentenceIDReceived == _T("HDG") ) {
            if ( m_NMEA0183->Parse() )
            {
                if ( mPriVar >= 2 )
                {
                    /* Any device sending VAR=0.0 can be assumed to not
                       really know what the actual variation is, so in
                       this case we use WMM if available */
                    if ( (!std::isnan(
                             m_NMEA0183->Hdg.MagneticVariationDegrees )) &&
                        0.0 != m_NMEA0183->Hdg.MagneticVariationDegrees) {
                        mPriVar = 2;
                        if ( m_NMEA0183->Hdg.MagneticVariationDirection ==
                            East )
                            mVar =  m_NMEA0183->Hdg.MagneticVariationDegrees;
                        else if ( m_NMEA0183->Hdg.MagneticVariationDirection ==
                                 West )
                            mVar = -m_NMEA0183->Hdg.MagneticVariationDegrees;
                        SendSentenceToAllInstruments(
                            OCPN_DBP_STC_HMV, mVar, _T("\u00B0") );
                        mVar_Watchdog = gps_watchdog_timeout_ticks;
                    }

                }
                if ( mPriHeadingM >= 1 ) {
                    mPriHeadingM = 1;
                    mHdm = m_NMEA0183->Hdg.MagneticSensorHeadingDegrees;
                    SendSentenceToAllInstruments(
                        OCPN_DBP_STC_HDM, mHdm, _T("\u00B0") );
                }
                if ( !std::isnan(
                        m_NMEA0183->Hdg.MagneticSensorHeadingDegrees) )
                    mHDx_Watchdog = gps_watchdog_timeout_ticks;
                /* If Variation is available, no higher priority HDT is
                   available, then calculate and propagate calculated HDT */
                if ( !std::isnan(
                        m_NMEA0183->Hdg.MagneticSensorHeadingDegrees) ) {
                    if ( !std::isnan( mVar )  && (mPriHeadingT > 3) ) {
                        mPriHeadingT = 4;
                        double heading = mHdm + mVar;
                        if (heading < 0)
                            heading += 360;
                        else if (heading >= 360.0)
                            heading -= 360;
                        SendSentenceToAllInstruments(
                            OCPN_DBP_STC_HDT, heading, _T("\u00B0"));
                        mHDT_Watchdog = gps_watchdog_timeout_ticks;
                    }
                }
            }
        }

        else if ( m_NMEA0183->LastSentenceIDReceived == _T("HDM") ) {
            if ( m_NMEA0183->Parse() ) {
                if ( mPriHeadingM >= 2 ) {
                    mPriHeadingM = 2;
                    mHdm = m_NMEA0183->Hdm.DegreesMagnetic;
                    SendSentenceToAllInstruments(
                        OCPN_DBP_STC_HDM, mHdm, _T("\u00B0M") );
                }
                if ( !std::isnan(m_NMEA0183->Hdm.DegreesMagnetic) )
                    mHDx_Watchdog = gps_watchdog_timeout_ticks;
                /* If Variation is available, no higher priority HDT is
                   available, then calculate and propagate calculated HDT */
                if ( !std::isnan(m_NMEA0183->Hdm.DegreesMagnetic) ) {
                    if ( !std::isnan( mVar )  && (mPriHeadingT > 2) ) {
                        mPriHeadingT = 3;
                        double heading = mHdm + mVar;
                        if (heading < 0)
                            heading += 360;
                        else if (heading >= 360.0)
                            heading -= 360;
                        SendSentenceToAllInstruments(
                            OCPN_DBP_STC_HDT, heading, _T("\u00B0"));
                        mHDT_Watchdog = gps_watchdog_timeout_ticks;
                    }
                }
            }
        }

        else if ( m_NMEA0183->LastSentenceIDReceived == _T("HDT") ) {
            if ( m_NMEA0183->Parse() ) {
                if ( mPriHeadingT >= 1 ) {
                    mPriHeadingT = 1;
                    if ( m_NMEA0183->Hdt.DegreesTrue < 999. ) {
                        SendSentenceToAllInstruments(
                            OCPN_DBP_STC_HDT,
                            m_NMEA0183->Hdt.DegreesTrue,
                            _T("\u00B0T") );
                    }
                }
                if ( !std::isnan(m_NMEA0183->Hdt.DegreesTrue) )
                    mHDT_Watchdog = gps_watchdog_timeout_ticks;
            }
        }

        else if ( m_NMEA0183->LastSentenceIDReceived == _T("MTA") ) {
            // Air temperature
            if ( m_NMEA0183->Parse() ) {
                /*
                  double   m_NMEA0183->Mta.Temperature;
                  wxString m_NMEA0183->Mta.UnitOfMeasurement;
                */
                double TemperatureValue =  m_NMEA0183->Mta.Temperature;
                wxString TemperatureUnitOfMeasurement =
                    m_NMEA0183->Mta.UnitOfMeasurement;
                checkNMEATemperatureDataAndUnit(
                    TemperatureValue, TemperatureUnitOfMeasurement );
                SendSentenceToAllInstruments(
                    OCPN_DBP_STC_ATMP,
                    TemperatureValue,
                    TemperatureUnitOfMeasurement );
            }
        }

        else if ( m_NMEA0183->LastSentenceIDReceived == _T("MDA") ) {
            //Barometric pressure
            if ( m_NMEA0183->Parse() ) {
                /*
                  double   m_NMEA0183->Mda.Pressure;
                  wxString m_NMEA0183->Mda.UnitOfMeasurement;
                */
                if ( m_NMEA0183->Mda.Pressure > .8 &&
                    m_NMEA0183->Mda.Pressure < 1.1 ) {
                    SendSentenceToAllInstruments(
                        OCPN_DBP_STC_MDA,
                        m_NMEA0183->Mda.Pressure *1000,
                        _T("hPa") );
                }
            }
        }

        else if ( m_NMEA0183->LastSentenceIDReceived == _T("MTW") ) {
            if ( m_NMEA0183->Parse() ) {
                /*
                  double   m_NMEA0183->Mtw.Temperature;
                  wxString m_NMEA0183->Mtw.UnitOfMeasurement;
                */
                double TemperatureValue = m_NMEA0183->Mtw.Temperature;
                wxString TemperatureUnitOfMeasurement =
                    m_NMEA0183->Mtw.UnitOfMeasurement;
                checkNMEATemperatureDataAndUnit(
                    TemperatureValue, TemperatureUnitOfMeasurement );
                SendSentenceToAllInstruments(
                    OCPN_DBP_STC_TMP,
                    TemperatureValue,
                    TemperatureUnitOfMeasurement );
            }

        }

        else if ( m_NMEA0183->LastSentenceIDReceived == _T("VLW") ) {
            if ( m_NMEA0183->Parse() ) {
                /*
                  double   m_NMEA0183->Vlw.TotalMileage;
                  double   m_NMEA0183->Vlw.TripMileage;
                */
                SendSentenceToAllInstruments(
                    OCPN_DBP_STC_VLW1,
                    toUsrDistance_Plugin(
                        m_NMEA0183->Vlw.TripMileage,
                        g_iDashDistanceUnit ),
                    getUsrDistanceUnit_Plugin( g_iDashDistanceUnit ) );
                SendSentenceToAllInstruments(
                    OCPN_DBP_STC_VLW2,
                    toUsrDistance_Plugin( m_NMEA0183->Vlw.TotalMileage,
                                          g_iDashDistanceUnit ),
                    getUsrDistanceUnit_Plugin( g_iDashDistanceUnit ) );
            }
        }

        else if ( m_NMEA0183->LastSentenceIDReceived == _T("MWD") ) {
        // NMEA 0183 standard Wind Direction and Speed, with respect to north.
            if ( m_NMEA0183->Parse() ) {
                // Option for True vs Magnetic
                wxString windunit;
                if ( m_NMEA0183->Mwd.WindAngleTrue < 999. ) {
                    //if WindAngleTrue is available, use it ...
                    SendSentenceToAllInstruments(
                        OCPN_DBP_STC_TWD,
                        m_NMEA0183->Mwd.WindAngleTrue,
                        _T("\u00B0T") );
                    this->SetNMEASentence_Arm_TWD_Watchdog();
                }
                else if ( m_NMEA0183->Mwd.WindAngleMagnetic < 999. ) {
                    //otherwise try WindAngleMagnetic ...
                    SendSentenceToAllInstruments(
                        OCPN_DBP_STC_TWD,
                        m_NMEA0183->Mwd.WindAngleMagnetic,
                        _T("\u00B0M") );
                    this->SetNMEASentence_Arm_TWD_Watchdog();
                }
                if (!this->SetNMEASentenceMWD_NKEbug(
                        m_NMEA0183->Mwd.WindSpeedKnots)) {
                    SendSentenceToAllInstruments(
                        OCPN_DBP_STC_TWS,
                        toUsrSpeed_Plugin( m_NMEA0183->Mwd.WindSpeedKnots,
                                           g_iDashWindSpeedUnit ),
                        getUsrSpeedUnit_Plugin( g_iDashWindSpeedUnit ) );
                    SendSentenceToAllInstruments(
                        OCPN_DBP_STC_TWS2,
                        toUsrSpeed_Plugin(
                            m_NMEA0183->Mwd.WindSpeedKnots,
                            g_iDashWindSpeedUnit ),
                        getUsrSpeedUnit_Plugin( g_iDashWindSpeedUnit ) );
                } /* then NKEbug hunt activated and condition for it
                     detected, drop. */
            }
        }
        else if ( m_NMEA0183->LastSentenceIDReceived == _T("MWV") ) {
            /* NMEA 0183 standard Wind Speed and Angle, in relation to
               the vessel's bow/centerline. */
            if ( m_NMEA0183->Parse() ) {
                if ( m_NMEA0183->Mwv.IsDataValid == NTrue ) {
                    /* MWV windspeed has different units. Form it to knots
                       to fit "toUsrSpeed_Plugin()" */
                    double m_wSpeedFactor = 1.0; //knots ("N")
                    if (m_NMEA0183->Mwv.WindSpeedUnits == _T("K") )
                        m_wSpeedFactor = 0.53995 ; //km/h > knots
                    if (m_NMEA0183->Mwv.WindSpeedUnits == _T("M") )
                        m_wSpeedFactor = 1.94384 ; //m/s > knots
                    if ( m_NMEA0183->Mwv.Reference == _T("R") ) {
                        // Relative (apparent wind)
                        if ( mPriAWA >= 1 ) {
                            mPriAWA = 1;
                            wxString m_awaunit;
                            double m_awaangle;
                            if (m_NMEA0183->Mwv.WindAngle >180) {
                                m_awaunit = L"\u00B0lr"; /* == wind arrow on
                                                            the port side */
                                m_awaangle = 180.0 -
                                    (m_NMEA0183->Mwv.WindAngle - 180.0);
                            }
                            else {
                                m_awaunit = L"\u00B0rl"; /* == wind arrow on
                                                            starboard side */
                                m_awaangle = m_NMEA0183->Mwv.WindAngle;
                            }
                            SendSentenceToAllInstruments(
                                OCPN_DBP_STC_AWA,
                                m_awaangle,
                                m_awaunit);
                            SendSentenceToAllInstruments(
                                OCPN_DBP_STC_AWS,
                                toUsrSpeed_Plugin(
                                    m_NMEA0183->Mwv.WindSpeed *
                                    m_wSpeedFactor,
                                    g_iDashWindSpeedUnit ),
                                getUsrSpeedUnit_Plugin(
                                    g_iDashWindSpeedUnit ) );
                            this->SetNMEASentence_Arm_AWS_Watchdog();
                        }
                    }

                    else if ( m_NMEA0183->Mwv.Reference == _T("T") ) {
                        // Theoretical (aka True)
                        if ( mPriTWA >= 1 ) {
                            mPriTWA = 1;
                            wxString m_twaunit;
                            double m_twaangle;
                            if (m_NMEA0183->Mwv.WindAngle >180) {
                                m_twaunit = L"\u00B0lr"; /* == wind arrow on
                                                            the port side */
                                m_twaangle = 180.0 -
                                    (m_NMEA0183->Mwv.WindAngle - 180.0);
                            }
                            else {
                                m_twaunit = L"\u00B0rl"; /* == wind arrow on
                                                            starboard side */
                                m_twaangle = m_NMEA0183->Mwv.WindAngle;
                            }
                            SendSentenceToAllInstruments(
                                OCPN_DBP_STC_TWA,
                                m_twaangle,
                                m_twaunit);
                            this->SetNMEASentence_Arm_TWD_Watchdog();
                            SendSentenceToAllInstruments(
                                OCPN_DBP_STC_TWS,
                                toUsrSpeed_Plugin(
                                    m_NMEA0183->Mwv.WindSpeed *
                                    m_wSpeedFactor,
                                    g_iDashWindSpeedUnit ),
                                getUsrSpeedUnit_Plugin(
                                    g_iDashWindSpeedUnit ) );
                            SendSentenceToAllInstruments(
                                OCPN_DBP_STC_TWS2,
                                toUsrSpeed_Plugin(
                                    m_NMEA0183->Mwv.WindSpeed *
                                    m_wSpeedFactor,
                                    g_iDashWindSpeedUnit ),
                                getUsrSpeedUnit_Plugin(
                                    g_iDashWindSpeedUnit ) );
                            this->SetNMEASentence_Arm_TWS_Watchdog();
                        }
                    }
                }
            }
        }

        else if (m_NMEA0183->LastSentenceIDReceived == _T("RMB")) {
            if (m_NMEA0183->Parse()) {
                if (m_NMEA0183->Rmb.IsDataValid == NTrue) {
                    if ( !std::isnan(
                             m_NMEA0183->Rmb.BearingToDestinationDegreesTrue)
                         &&
                         (m_NMEA0183->Rmb.BearingToDestinationDegreesTrue
                          < 999.) ) { // empty field
                        SendSentenceToAllInstruments(
                            OCPN_DBP_STC_BRG,
                            m_NMEA0183->Rmb.BearingToDestinationDegreesTrue,
                            m_NMEA0183->Rmb.To);
                        this->SetNMEASentence_Arm_BRG_Watchdog();
                    }
                    if ( !std::isnan(
                             m_NMEA0183->Rmb.RangeToDestinationNauticalMiles)
                         &&
                         (m_NMEA0183->Rmb.RangeToDestinationNauticalMiles
                          < 999.) ) // empty field
                        SendSentenceToAllInstruments(
                            OCPN_DBP_STC_DTW,
                            m_NMEA0183->Rmb.RangeToDestinationNauticalMiles,
                            _T("Nm"));
                    /*
                      Note: there are no consumers in Tactics functions for
                      the below sentence but without it the Dashboard's
                      VMG-instruments remain silent when there is next active
                      waypoint set by the OpenCPN's routing functions.
                      We capture here the sentence send by OpenCPN
                      to the autopilot and  other interested parties like.
                      Since the GitHub issue #1422 https://git.io/JkLrL 
                      has not been recognized and there is no concensus
                      between the stakeholders of the VMG meaning in
                      OpenCPN, this note and the below sentences can be
                      removed */
                    if ( !std::isnan(
                             m_NMEA0183->Rmb.DestinationClosingVelocityKnots)
                         &&
                         (m_NMEA0183->Rmb.DestinationClosingVelocityKnots
                          < 999.) ) { // empty field
                        SendSentenceToAllInstruments(
                            OCPN_DBP_STC_VMG,
                            toUsrSpeed_Plugin(
                                m_NMEA0183->Rmb.DestinationClosingVelocityKnots,
                                g_iDashWindSpeedUnit ),
                            getUsrSpeedUnit_Plugin( g_iDashWindSpeedUnit ) );
                        this->SetNMEASentence_Arm_VMG_Watchdog();
                    } // then valid sentence with VMG information received
                } // then valid data
            } // then sentence parse OK
        } // then last sentence is RMB

        else if ( m_NMEA0183->LastSentenceIDReceived == _T("RMC") ) {
            if ( m_NMEA0183->Parse() ) {
                if ( m_NMEA0183->Rmc.IsDataValid == NTrue ) {
                    if ( mPriPosition >= 4 ) {
                        mPriPosition = 4;
                        double lat, lon;
                        float llt = m_NMEA0183->Rmc.Position.Latitude.Latitude;
                        int lat_deg_int = (int) ( llt / 100 );
                        float lat_deg = lat_deg_int;
                        float lat_min = llt - ( lat_deg * 100 );
                        lat = lat_deg + ( lat_min / 60. );
                        if ( m_NMEA0183->Rmc.Position.Latitude.Northing ==
                             South )
                            lat = -lat;
                        SendSentenceToAllInstruments(
                            OCPN_DBP_STC_LAT,
                            lat,
                            _T("SDMM") );
                        float lln =
                            m_NMEA0183->Rmc.Position.Longitude.Longitude;
                        int lon_deg_int = (int) ( lln / 100 );
                        float lon_deg = lon_deg_int;
                        float lon_min = lln - ( lon_deg * 100 );
                        lon = lon_deg + ( lon_min / 60. );
                        if ( m_NMEA0183->Rmc.Position.Longitude.Easting ==
                             West )
                            lon = -lon;
                        SendSentenceToAllInstruments(
                            OCPN_DBP_STC_LON,
                            lon,
                            _T("SDMM") );
                    }

                    if ( mPriCOGSOG >= 3 ) {
                        mPriCOGSOG = 3;
                        if ( m_NMEA0183->Rmc.SpeedOverGroundKnots < 999. ) {
                            SendSentenceToAllInstruments(
                                OCPN_DBP_STC_SOG,
                                toUsrSpeed_Plugin(
                                    mSOGFilter.filter(
                                        m_NMEA0183->Rmc.SpeedOverGroundKnots ),
                                    g_iDashSpeedUnit ),
                                getUsrSpeedUnit_Plugin( g_iDashSpeedUnit ) );
                        }
                        if ( m_NMEA0183->Rmc.TrackMadeGoodDegreesTrue < 999. ) {
                            SendSentenceToAllInstruments(
                                OCPN_DBP_STC_COG,
                                mCOGFilter.filter(
                                    m_NMEA0183->Rmc.TrackMadeGoodDegreesTrue ),
                                _T("\u00B0") );
                        }
                        if ( m_NMEA0183->Rmc.TrackMadeGoodDegreesTrue < 999.
                             && m_NMEA0183->Rmc.MagneticVariation < 999.) {
                            double dMagneticCOG;
                            if ( m_NMEA0183->Rmc.MagneticVariationDirection ==
                                 East) {
                                dMagneticCOG = mCOGFilter.get() -
                                    m_NMEA0183->Rmc.MagneticVariation;
                                if ( dMagneticCOG < 0.0 )
                                    dMagneticCOG = 360.0 + dMagneticCOG;
                            }
                            else {
                                dMagneticCOG = mCOGFilter.get() +
                                    m_NMEA0183->Rmc.MagneticVariation;
                                if ( dMagneticCOG > 360.0 )
                                    dMagneticCOG = dMagneticCOG - 360.0;
                            }
                            SendSentenceToAllInstruments(
                                OCPN_DBP_STC_MCOG,
                                dMagneticCOG,
                                _T("\u00B0M") );
                        }
                    }

                    if ( mPriVar >= 3 )
                    {
                        /* Any device sending VAR=0.0 can be assumed to not
                           really know what the actual variation is, so in
                           this case we use WMM if available */
                        if ( (!std::isnan( m_NMEA0183->Rmc.MagneticVariation))
                             &&
                             0.0 != m_NMEA0183->Rmc.MagneticVariation ) {
                            mPriVar = 3;
                            if (m_NMEA0183->Rmc.MagneticVariationDirection ==
                                East)
                                mVar = m_NMEA0183->Rmc.MagneticVariation;
                            else if (m_NMEA0183->Rmc.MagneticVariationDirection
                                      == West)
                                mVar = -m_NMEA0183->Rmc.MagneticVariation;
                            mVar_Watchdog = gps_watchdog_timeout_ticks;
                            SendSentenceToAllInstruments(
                                OCPN_DBP_STC_HMV,
                                mVar,
                                _T("\u00B0"));
                        }
                    }

                    if ( mPriDateTime >= 3 ) {
                        mPriDateTime = 3;
                        wxString dt = m_NMEA0183->Rmc.Date +
                            m_NMEA0183->Rmc.UTCTime;
                        mUTCDateTime.ParseFormat( dt.c_str(),
                                                  _T("%d%m%y%H%M%S") );
                        /* Note: in Dashboard, this is a "fake" UTC,
                           just a string representation, memorized,
                           i.e. the parsed value has time zone applied to it
                           within the wxDateTime object, its Epoch value is
                           therefore off by the value defined by the timezone.
                           In order not to break the clock.cpp, we register
                           the offset to the real UTC LL (Epoch) value:
                        */
                        wxDateTime resultingFakeUTC = mUTCDateTime.ToUTC();
                        wxLongLong resEpoch = resultingFakeUTC.GetValue();
                        wxLongLong fakeEpoch = mUTCDateTime.GetValue();
                        mUTCDateTzOffsetLL =
                            fakeEpoch.GetValue() - resEpoch.GetValue();
                        mUTCRealGpsEpoch =
                            fakeEpoch.GetValue() + mUTCDateTzOffsetLL;
                        mGNSSreceivedAtLocalMs = wxGetUTCTimeMillis();
                    }
                }
            }
        }

        else if ( m_NMEA0183->LastSentenceIDReceived == _T("RSA") ) {
            if ( m_NMEA0183->Parse() ) {
                if ( m_NMEA0183->Rsa.IsStarboardDataValid == NTrue ) {
                    SendSentenceToAllInstruments(
                        OCPN_DBP_STC_RSA,
                        m_NMEA0183->Rsa.Starboard,
                        _T("\u00B0") );
                } else if ( m_NMEA0183->Rsa.IsPortDataValid == NTrue ) {
                    SendSentenceToAllInstruments(
                        OCPN_DBP_STC_RSA,
                        -m_NMEA0183->Rsa.Port,
                        _T("\u00B0") );
                }
            }
        }

        else if ( m_NMEA0183->LastSentenceIDReceived == _T("VHW") ) {
            if ( m_NMEA0183->Parse() ) {
                if ( mPriHeadingT >= 2 ) {
                    if ( m_NMEA0183->Vhw.DegreesTrue < 999. ) {
                        mPriHeadingT = 2;
                        SendSentenceToAllInstruments(
                            OCPN_DBP_STC_HDT,
                            m_NMEA0183->Vhw.DegreesTrue,
                            _T("\u00B0T") );
                    }
                }
                if ( mPriHeadingM >= 3 ) {
                    mPriHeadingM = 3;
                    SendSentenceToAllInstruments(
                        OCPN_DBP_STC_HDM,
                        m_NMEA0183->Vhw.DegreesMagnetic,
                        _T("\u00B0M") );
                }
                if ( m_NMEA0183->Vhw.Knots < 999. ) {
                    SendSentenceToAllInstruments(
                        OCPN_DBP_STC_STW,
                        toUsrSpeed_Plugin(
                            m_NMEA0183->Vhw.Knots,
                            g_iDashSpeedUnit ),
                        getUsrSpeedUnit_Plugin( g_iDashSpeedUnit ) );
                    mStW_Watchdog = gps_watchdog_timeout_ticks;
                }
                if ( !std::isnan(m_NMEA0183->Vhw.DegreesMagnetic) )
                    mHDx_Watchdog = gps_watchdog_timeout_ticks;
                if ( !std::isnan(m_NMEA0183->Vhw.DegreesTrue) )
                    mHDT_Watchdog = gps_watchdog_timeout_ticks;
            }
        }

        else if ( m_NMEA0183->LastSentenceIDReceived == _T("VTG") ) {
            if ( m_NMEA0183->Parse() ) {
                if ( mPriCOGSOG >= 2 ) {
                    mPriCOGSOG = 2;
                    /* Special check for unintialized values, as opposed
                       to zero values */
                    if ( m_NMEA0183->Vtg.SpeedKnots < 999. ) {
                        SendSentenceToAllInstruments(
                            OCPN_DBP_STC_SOG,
                            toUsrSpeed_Plugin(
                                mSOGFilter.filter(
                                    m_NMEA0183->Vtg.SpeedKnots),
                                g_iDashSpeedUnit ),
                            getUsrSpeedUnit_Plugin( g_iDashSpeedUnit ) );
                    }
                    // Vtg.SpeedKilometersPerHour;
                    if ( m_NMEA0183->Vtg.TrackDegreesTrue < 999. ) {
                        SendSentenceToAllInstruments(
                            OCPN_DBP_STC_COG,
                            mCOGFilter.filter(
                                m_NMEA0183->Vtg.TrackDegreesTrue),
                            _T("\u00B0") );
                    }
                }
            }
        }
        /* NMEA 0183 Relative (Apparent) Wind Speed and Angle. Wind angle in
           relation to the vessel's heading, and wind speed measured relative
           to the moving vessel. */
        else if ( m_NMEA0183->LastSentenceIDReceived == _T("VWR") ) {
            if ( m_NMEA0183->Parse() ) {
                if ( mPriAWA >= 2 ) {
                    mPriAWA = 2;
                    wxString awaunit;
                    awaunit = ( m_NMEA0183->Vwr.DirectionOfWind ==
                        Left ? L"\u00B0lr" : L"\u00B0rl" );
                    SendSentenceToAllInstruments(
                        OCPN_DBP_STC_AWA,
                        m_NMEA0183->Vwr.WindDirectionMagnitude,
                        awaunit );
                    SendSentenceToAllInstruments(
                        OCPN_DBP_STC_AWS,
                        toUsrSpeed_Plugin(
                            m_NMEA0183->Vwr.WindSpeedKnots,
                            g_iDashWindSpeedUnit ),
                        getUsrSpeedUnit_Plugin( g_iDashWindSpeedUnit ) );
                    this->SetNMEASentence_Arm_AWS_Watchdog();
                }
            }
        }
        /* NMEA 0183 True wind angle in relation to the vessel's heading,
           and true wind speed referenced to the water. True wind is
           the vector sum of the Relative (apparent) wind vector and
           the vessel's velocity vector relative to the water along the
           heading line of the vessel. It represents the wind at the vessel
           if it were stationary relative to the water and heading in
           the same direction. */
        else if ( m_NMEA0183->LastSentenceIDReceived == _T("VWT") ) {
            if ( m_NMEA0183->Parse() ) {
                if ( mPriTWA >= 2 ) {
                    mPriTWA = 2;
                    wxString vwtunit;
                    vwtunit = ( m_NMEA0183->Vwt.DirectionOfWind ==
                        Left ? L"\u00B0lr" : L"\u00B0rl" );
                    SendSentenceToAllInstruments(
                        OCPN_DBP_STC_TWA,
                        m_NMEA0183->Vwt.WindDirectionMagnitude,
                        vwtunit );
                    this->SetNMEASentence_Arm_TWD_Watchdog();
                    SendSentenceToAllInstruments(
                        OCPN_DBP_STC_TWS,
                        toUsrSpeed_Plugin(
                            m_NMEA0183->Vwt.WindSpeedKnots,
                            g_iDashWindSpeedUnit ),
                        getUsrSpeedUnit_Plugin( g_iDashWindSpeedUnit ) );
                    this->SetNMEASentence_Arm_TWS_Watchdog();
                }
            }
        }

        else if (m_NMEA0183->LastSentenceIDReceived == _T("XDR")) {
            //Transducer measurement
            /* XDR Transducer types
             * AngularDisplacementTransducer = 'A',
             * TemperatureTransducer = 'C',
             * LinearDisplacementTransducer = 'D',
             * FrequencyTransducer = 'F',
             * HumidityTransducer = 'H',
             * ForceTransducer = 'N',
             * PressureTransducer = 'P',
             * FlowRateTransducer = 'R',
             * TachometerTransducer = 'T',
             * VolumeTransducer = 'V'
             */
            if (m_NMEA0183->Parse()) {
                wxString xdrunit;
                for (int i = 0; i<m_NMEA0183->Xdr.TransducerCnt; i++) {
                    double xdrdata = m_NMEA0183->Xdr.TransducerInfo[i].Data;
                    // XDR Airtemp
                    if (m_NMEA0183->Xdr.TransducerInfo[i].Type == _T("C")) {
                        double TemperatureValue = xdrdata;
                        wxString TemperatureUnitOfMeasurement =
                            m_NMEA0183->Xdr.TransducerInfo[i].Unit;
                        checkNMEATemperatureDataAndUnit(
                            TemperatureValue,
                            TemperatureUnitOfMeasurement );
                        SendSentenceToAllInstruments(
                            OCPN_DBP_STC_ATMP,
                            TemperatureValue,
                            TemperatureUnitOfMeasurement );
                    }
                    /* NKE style of XDR Airtemp etc. cf. original
                       Tactics Plugin */
                    if (m_NMEA0183->Xdr.TransducerInfo[i].Name ==
                        _T("AirTemp") ||
                        m_NMEA0183->Xdr.TransducerInfo[i].Name ==
                        _T("ENV_OUTAIR_T") ||
                        m_NMEA0183->Xdr.TransducerInfo[i].Name ==
                        _T("ENV_OUTSIDE_T")) {
                        double TemperatureValue = xdrdata;
                        wxString TemperatureUnitOfMeasurement =
                            m_NMEA0183->Xdr.TransducerInfo[i].Unit;
                        checkNMEATemperatureDataAndUnit(
                            TemperatureValue,
                            TemperatureUnitOfMeasurement );
                        SendSentenceToAllInstruments(
                            OCPN_DBP_STC_ATMP,
                            TemperatureValue,
                            TemperatureUnitOfMeasurement );
                    }
                    // XDR Pressure
                    if (m_NMEA0183->Xdr.TransducerInfo[i].Type == _T("P")) {
                        if (m_NMEA0183->Xdr.TransducerInfo[i].Unit ==
                            _T("B")) {
                            xdrdata *= 1000;
                            SendSentenceToAllInstruments(
                                OCPN_DBP_STC_MDA,
                                xdrdata,
                                _T("mBar") );
                        }
                    }
                    // XDR Pitch (=Nose up/down) or Heel (stb/port)
                    if (m_NMEA0183->Xdr.TransducerInfo[i].Type == _T("A")) {
                        if (m_NMEA0183->Xdr.TransducerInfo[i].Name ==
                            _T("PTCH")
                            || m_NMEA0183->Xdr.TransducerInfo[i].Name ==
                            _T("PITCH")) {
                            if (m_NMEA0183->Xdr.TransducerInfo[i].Data > 0) {
                                xdrunit = L"\u00B0u";
                            }
                            else if (m_NMEA0183->Xdr.TransducerInfo[i].Data
                                     < 0) {
                                xdrunit = L"\u00B0d";
                            }
                            else {
                                xdrunit = _T("\u00B0");
                            }
                            SendSentenceToAllInstruments(
                                OCPN_DBP_STC_PITCH,
                                xdrdata,
                                xdrunit);
                        }
                        else if (
                            (m_NMEA0183->Xdr.TransducerInfo[i].Name ==
                             _T("ROLL")) ||
                            (m_NMEA0183->Xdr.TransducerInfo[i].Name ==
                             _T("HEEL")) ||
                            (m_NMEA0183->Xdr.TransducerInfo[i].Name ==
                             _T("Heel Angle")))
                        {
                            if (m_NMEA0183->Xdr.TransducerInfo[i].Data > 0) {
                                xdrunit = L"\u00B0r";
                            }
                            else if (m_NMEA0183->Xdr.TransducerInfo[i].Data
                                     < 0) {
                                xdrunit = L"\u00B0l";
                            }
                            else {
                                xdrunit = _T("\u00B0");
                            }
                            SendSentenceToAllInstruments(
                                OCPN_DBP_STC_HEEL,
                                xdrdata,
                                xdrunit);
                        } 
                        //Nasa style water temp
                        else if (m_NMEA0183->Xdr.TransducerInfo[i].Name ==
                                 _T("ENV_WATER_T")){
                            double TemperatureValue = xdrdata;
                            wxString TemperatureUnitOfMeasurement =
                                m_NMEA0183->Xdr.TransducerInfo[i].Unit;
                            checkNMEATemperatureDataAndUnit(
                                TemperatureValue,
                                TemperatureUnitOfMeasurement );
                            SendSentenceToAllInstruments(
                                OCPN_DBP_STC_ATMP,
                                TemperatureValue,
                                TemperatureUnitOfMeasurement );
                        }
                    }
                } // for XDR transducers
            } // then parse OK
        } // then XDR
        else if (m_NMEA0183->LastSentenceIDReceived == _T("ZDA")) {
            if ( m_NMEA0183->Parse() ) {
                if ( mPriDateTime >= 2 ) {
                    mPriDateTime = 2;
                    wxString dt;
                    dt.Printf( _T("%4d%02d%02d"),
                        m_NMEA0183->Zda.Year,
                        m_NMEA0183->Zda.Month,
                        m_NMEA0183->Zda.Day );
                    dt.Append( m_NMEA0183->Zda.UTCTime );
                    mUTCDateTime.ParseFormat(
                        dt.c_str(),
                        _T("%Y%m%d%H%M%S") );
                    wxDateTime resultingFakeUTC = mUTCDateTime.ToUTC();
                    wxLongLong resEpoch = resultingFakeUTC.GetValue();
                    wxLongLong fakeEpoch = mUTCDateTime.GetValue();
                    mUTCDateTzOffsetLL =
                        fakeEpoch.GetValue() - resEpoch.GetValue();
                    mUTCRealGpsEpoch =
                        fakeEpoch.GetValue() + mUTCDateTzOffsetLL;
                    mGNSSreceivedAtLocalMs = wxGetUTCTimeMillis();
                }
            }
        }
        //      Process an AIVDO message
        else if ( sentence.Mid( 1, 5 ).IsSameAs( _T("AIVDO") ) ) {
            PlugIn_Position_Fix_Ex gpd;
            if ( DecodeSingleVDOMessage(
                     sentence,
                     &gpd,
                     &m_VDO_accumulator) ) {
                if ( !std::isnan( gpd.Lat ) )
                    SendSentenceToAllInstruments(
                        OCPN_DBP_STC_LAT,
                        gpd.Lat,
                        _T("SDMM") );
                if ( !std::isnan( gpd.Lon ) )
                    SendSentenceToAllInstruments(
                        OCPN_DBP_STC_LON,
                        gpd.Lon,
                        _T("SDMM") );
                SendSentenceToAllInstruments(
                    OCPN_DBP_STC_SOG,
                    toUsrSpeed_Plugin(
                        mSOGFilter.filter(gpd.Sog),
                        g_iDashSpeedUnit ),
                    getUsrSpeedUnit_Plugin( g_iDashSpeedUnit ) );
                SendSentenceToAllInstruments(
                    OCPN_DBP_STC_COG,
                    mCOGFilter.filter( gpd.Cog ),
                    _T("\u00B0") );
                if ( !std::isnan(gpd.Hdt) ) {
                    SendSentenceToAllInstruments(
                        OCPN_DBP_STC_HDT,
                        gpd.Hdt,
                        _T("\u00B0T") );
                    mHDT_Watchdog = gps_watchdog_timeout_ticks;
                }
            }
        }
    }
    else { // SignalK - see https://git.io/Je3W0 for supported NMEA-0183 sentences
        if ( type->IsSameAs( "NMEA0183", false ) ) {

            if ( this->m_pSkData->isSubscribedToAllPaths() )
                this->m_pSkData->UpdateNMEA0183PathList( path, key );
            
            if ( sentenceId->CmpNoCase(_T("DBT")) == 0 ) {
                // https://git.io/JeYfB
                if ( path->CmpNoCase(_T("environment.depth.belowTransducer"))
                     == 0 ) {
                    if ( mPriDepth >= 2 ) {
                        mPriDepth = 2;
                        double depth = value + g_dDashDBTOffset;
                        SendSentenceToAllInstruments(
                            OCPN_DBP_STC_DPT,
                            toUsrDistance_Plugin( depth / 1852.0,
                                                  g_iDashDepthUnit ),
                            getUsrDistanceUnit_Plugin( g_iDashDepthUnit ),
                            timestamp );
                    }
                }
            } // DBT
                
            else if ( sentenceId->CmpNoCase(_T("DPT")) == 0 ) {
                // https://git.io/JeYf4
                bool depthvalue = false;
                if ( path->CmpNoCase(_T("environment.depth.belowTransducer"))
                     == 0 ) {
                    if ( !mSiK_DPT_environmentDepthBelowKeel )
                        depthvalue = true;
                }
                else if ( path->CmpNoCase(_T("environment.depth.belowKeel"))
                          == 0 ) {  // depth + offset
                    depthvalue = true;
                    mSiK_DPT_environmentDepthBelowKeel = true; // lock priority
                }
                if ( depthvalue ) {
                    if ( mPriDepth >= 1 ) {
                        mPriDepth = 1;
                        double depth = value + g_dDashDBTOffset;
                        SendSentenceToAllInstruments(
                            OCPN_DBP_STC_DPT,
                            toUsrDistance_Plugin(
                                depth / 1852.0,
                                g_iDashDepthUnit ),
                            getUsrDistanceUnit_Plugin( g_iDashDepthUnit ),
                            timestamp );
                    }
                }
            } // DPT

            else if ( sentenceId->CmpNoCase(_T("GGA")) == 0 ) {
                // https://git.io/JeYWl
                if ( path->CmpNoCase(_T("navigation.gnss.methodQuality"))
                     == 0 ) {
                    if ( valStr->CmpNoCase(_T("GNSS fix")) == 0 ) {
                        mSiK_navigationGnssMethodQuality = 1;
                    }
                    else {
                        mSiK_navigationGnssMethodQuality = 0;
                        mSatsInView = 0;
                        SendSentenceToAllInstruments(
                            OCPN_DBP_STC_SAT,
                            mSatsInView, _T(""),
                            timestamp );
                    }
               }
                else if ( mSiK_navigationGnssMethodQuality > 0 ) {
                    if ( path->CmpNoCase(_T("navigation.position")) == 0 ) {
                        if ( (mPriPosition >= 3) && (key != NULL) ) {
                            // See SetPositionFix() - It rules, even if no fix!
                            mPriPosition = 3;
                            if ( key->CmpNoCase(_T("longitude")) == 0 )
                                // coordinate: https://git.io/JeYry
                                SendSentenceToAllInstruments(
                                    OCPN_DBP_STC_LON,
                                    value, _T("SDMM"),
                                    timestamp );
                            if ( key->CmpNoCase(_T("latitude")) == 0 )
                                SendSentenceToAllInstruments(
                                    OCPN_DBP_STC_LAT,
                                    value, _T("SDMM"),
                                    timestamp );
                        }
                    }
                    else if ( path->CmpNoCase(
                                  _T("navigation.gnss.satellites")) == 0 ) {
                        mSatsInView = value;
                        SendSentenceToAllInstruments(
                            OCPN_DBP_STC_SAT,
                            mSatsInView, _T(""),
                            timestamp );
                        mGPS_Watchdog = gps_watchdog_timeout_ticks;
                    }
                }
            } // GGA

            else if ( sentenceId->CmpNoCase(_T("GLL")) == 0 ) {
                /* Note: Signal K does not send delta is no validy flag set,
                   see https://git.io/JeYQK */
                if ( path->CmpNoCase(_T("navigation.position")) == 0 ) {
                    if ( (mPriPosition >= 2)  && (key != NULL) ) {
                        // See SetPositionFix() - It rules, even if no fix!
                        mPriPosition = 2;
                        if ( key->CmpNoCase(_T("longitude")) == 0 )
                            // coordinate: https://git.io/JeYry
                            SendSentenceToAllInstruments( OCPN_DBP_STC_LON,
                                                          value, _T("SDMM"),
                                                          timestamp );
                        else if ( key->CmpNoCase(_T("latitude")) == 0 )
                            SendSentenceToAllInstruments( OCPN_DBP_STC_LAT,
                                                          value, _T("SDMM"),
                                                          timestamp );
                    }
                }
            } // GLL

            /* Note: Sentence GSV not implemented in Signal K delta, see
               https://git.io/JeYdd - see GGA */

            else if ( sentenceId->CmpNoCase(_T("HDG")) == 0 ) {
                // https://git.io/JeYdxn
                if ( path->CmpNoCase(_T("navigation.headingMagnetic")) == 0 ) {
                    if ( mPriHeadingM >= 1 ) { 
                        if ( !std::isnan( value )) {
                            mPriHeadingM = 1;
                            mHdm = value * RAD_IN_DEG;
                            SendSentenceToAllInstruments( OCPN_DBP_STC_HDM,
                                                          mHdm,_T("\u00B0"),
                                                          timestamp );
                            mHDx_Watchdog = gps_watchdog_timeout_ticks;
                        }
                    }
                }
                else if ( path->CmpNoCase(_T("navigation.magneticVariation"))
                          == 0 ) {
                    if ( mPriVar >= 2 ) {
                        // see comment in NMEA equivalent: not really useful
                        if ( !std::isnan( value )) {
                            if ( value != 0.0 ) {
                                mPriVar = 2;
                                mVar = value * RAD_IN_DEG;
                                SendSentenceToAllInstruments(
                                    OCPN_DBP_STC_HMV,
                                    mVar,
                                    _T("\u00B0"),
                                    timestamp );
                                mVar_Watchdog = gps_watchdog_timeout_ticks;
                            }
                        }
                    }
                }
                if ( !std::isnan( mVar )  && !std::isnan( mHdm ) &&
                     (mPriHeadingT > 3) ) {
                    mPriHeadingT = 4;
                    double heading = mHdm + mVar;
                    if (heading < 0)
                        heading += 360;
                    else if (heading >= 360.0)
                        heading -= 360;
                    SendSentenceToAllInstruments(OCPN_DBP_STC_HDT,
                                                 heading, _T("\u00B0"),
                                                 timestamp );
                    mHDT_Watchdog = gps_watchdog_timeout_ticks;
                }
            } // HDG
        
            else if ( sentenceId->CmpNoCase(_T("HDM")) == 0 ) {
                // https://git.io/JeYdxn
                if ( path->CmpNoCase(_T("navigation.headingMagnetic")) == 0 ) {
                    if ( mPriHeadingM >= 2 ) { 
                        if ( !std::isnan( value )) {
                            mPriHeadingM = 2;
                            mHdm = value * RAD_IN_DEG;
                            SendSentenceToAllInstruments( OCPN_DBP_STC_HDM,
                                                          mHdm, _T("\u00B0M"),
                                                          timestamp );
                            mHDx_Watchdog = gps_watchdog_timeout_ticks;
                        }
                    }
                }
            } // HDM
        
            else if ( sentenceId->CmpNoCase(_T("HDT")) == 0 ) {
                // https://git.io/JeOCC
                if ( path->CmpNoCase(_T("navigation.headingTrue")) == 0 ) {
                    if ( mPriHeadingM >= 1 ) { 
                        if ( !std::isnan( value )) {
                            mPriHeadingM = 1;
                            mHdm = value * RAD_IN_DEG;
                            SendSentenceToAllInstruments( OCPN_DBP_STC_HDT,
                                                          mHdm, _T("\u00B0T"),
                                                          timestamp );
                            mHDT_Watchdog = gps_watchdog_timeout_ticks;
                        }
                    }
                }
            } // HDT

            /* MTA is not implemented in Signal K delta,
               cf. https://git.io/JeYdd - see XDR */

            else if ( sentenceId->CmpNoCase(_T("MDA")) == 0 ) {
                // https://git.io/JeOWL
                if ( path->CmpNoCase(_T("environment.outside.pressure"))
                     == 0 ) {
                    // Note: value from Signal K is SI units Pa, convet to hPa
                    double hPaPressure = value / 100.;
                    if ( (hPaPressure > 800) && (hPaPressure < 1100) ) {
                        SendSentenceToAllInstruments( OCPN_DBP_STC_MDA,
                                                      hPaPressure, _T("hPa"),
                                                      timestamp );
                    } // then valid pressure in hPa
                    /* Note: Dashboard does not deal with other values in MDA
                       as for now so we skip them */
                }
            } // MDA

            else if ( sentenceId->CmpNoCase(_T("MTW")) == 0 ) {
                // https://git.io/JeOwA
                if ( path->CmpNoCase(_T("environment.water.temperature"))
                     == 0 ) {
                    /* Note: value from Signal K is SI units, thus we
                       receive Kelvins */
                    double TemperatureValue = value - CELCIUS_IN_KELVIN; 
                    wxString TemperatureUnitOfMeasurement = _T("C"); // MTW default
                    checkNMEATemperatureDataAndUnit(
                        TemperatureValue, TemperatureUnitOfMeasurement );
                    SendSentenceToAllInstruments(
                        OCPN_DBP_STC_TMP,
                        TemperatureValue,
                        TemperatureUnitOfMeasurement,
                        timestamp );
                }
            } // MTW

            /* MWD is not implemented in Signal K, cf. https://git.io/JeYdd
               (but true wind will be calculated in Tactics) */

            else if ( sentenceId->CmpNoCase(_T("MWV")) == 0 ) {
                // https://git.io/JeOov
                if ( path->CmpNoCase(_T("environment.wind.speedApparent"))
                     == 0 ) {
                    /* Note: value from Signal K is SI units, thus we
                       receive [m/s] */
                    SendSentenceToAllInstruments(
                        OCPN_DBP_STC_AWS,
                        toUsrSpeed_Plugin( value * MS_IN_KNOTS,
                                           g_iDashWindSpeedUnit ),
                        getUsrSpeedUnit_Plugin( g_iDashWindSpeedUnit ),
                        timestamp );
                    this->SetNMEASentence_Arm_AWS_Watchdog();
                }
                else if ( path->CmpNoCase(_T("environment.wind.angleApparent"))
                          == 0 ) {
                    if ( mPriAWA >= 1 ) {
                        mPriAWA = 1;
                        SendSentenceToAllInstruments(
                            OCPN_DBP_STC_AWA,
                            std::abs( value ) * RAD_IN_DEG,
                            ( value < 0 ? L"\u00B0lr" : L"\u00B0rl" ),
                            timestamp );
                    } // AWA priority
                }
                else if ( path->CmpNoCase(_T("environment.wind.speedTrue")) ==
                          0 ) {
                    SendSentenceToAllInstruments(
                        OCPN_DBP_STC_TWS,
                        toUsrSpeed_Plugin( value * MS_IN_KNOTS,
                                           g_iDashWindSpeedUnit ),
                        getUsrSpeedUnit_Plugin( g_iDashWindSpeedUnit ),
                        timestamp );
                    SendSentenceToAllInstruments(
                        OCPN_DBP_STC_TWS2,
                        toUsrSpeed_Plugin( value * MS_IN_KNOTS,
                                           g_iDashWindSpeedUnit ),
                        getUsrSpeedUnit_Plugin( g_iDashWindSpeedUnit ),
                        timestamp );
                    this->SetNMEASentence_Arm_TWS_Watchdog();
                }
                else if ( path->CmpNoCase(_T("environment.wind.angleTrueWater"))
                          == 0 ) {
                    if ( mPriTWA >= 1 ) {
                        mPriTWA = 1;
                        double windTWSDir = std::abs( value ) * RAD_IN_DEG;
                        if ( windTWSDir > 360.0 )
                            windTWSDir -= 360.;
                        SendSentenceToAllInstruments(
                            OCPN_DBP_STC_TWA,
                            windTWSDir,
                            ( value < 0 ? L"\u00B0lr" : L"\u00B0rl" ),
                            timestamp );
                        this->SetNMEASentence_Arm_TWD_Watchdog();
                    } // TWA priority
                }
            } // MWV
        
            else if ( sentenceId->CmpNoCase(_T("RMB")) == 0 ) {
                /* https://git.io/Je3UV
                   See the comment in the same sentence's interpretation above
                   (when coming from OpenCPN): the controversy of having it here
                   is the same: the infamous VMG interpretation of next
                   destination waypoint is not the same as in Tactics,
                   i.e. for the sailing boat performance criteria.
                   It is kept here since it can be considered belonging to
                   Dashboard which needs to serve also the needs of a cruising
                   sailing and motor boats but it can be useful in the off-shore
                   races, too.
                   Dashboard ignores navigation.courseRhumbline.nextPoint, as
                   for now */
                if ( path->CmpNoCase(
                         _T("navigation.courseRhumbline.nextPoint.bearingTrue") )
                     == 0 ) {
                    SendSentenceToAllInstruments(
                        OCPN_DBP_STC_BRG,
                        value * RAD_IN_DEG,
                        _T("\u00B0"), /* as for now, Origin ID not available
                                         from Signal K */
                        timestamp );
                    this->SetNMEASentence_Arm_BRG_Watchdog();
                }
                else if ( path->CmpNoCase(
                   _T("navigation.courseRhumbline.nextPoint.velocityMadeGood"))
                   == 0 ) {
                    /* This is THE carburator for hours of useless "discussions"
                       in forums; comment it out if you don't like it :) */
                    SendSentenceToAllInstruments(
                        OCPN_DBP_STC_VMG,
                        toUsrSpeed_Plugin( value * MS_IN_KNOTS,
                                           g_iDashWindSpeedUnit ),
                        getUsrSpeedUnit_Plugin( g_iDashWindSpeedUnit ),
                        timestamp );
                    this->SetNMEASentence_Arm_VMG_Watchdog();
                }
                else if (
                    path->CmpNoCase(
                   _T("navigation.courseRhumbline.nextPoint.distance"))
                    == 0 ) {
                    SendSentenceToAllInstruments(
                        OCPN_DBP_STC_DTW,
                        value * KM_IN_NM, _T("Nm"),
                        timestamp );
                }
            } // RMB

            else if ( sentenceId->CmpNoCase(_T("RMC")) == 0 ) {
                // https://git.io/Je3T3
                if ( path->CmpNoCase(_T("navigation.position")) == 0 ) {
                    if ( (mPriPosition >= 4) && (key != NULL) ) {
                        // See SetPositionFix() - It rules, even if no fix!
                        mPriPosition = 4;
                        if ( key->CmpNoCase(_T("longitude")) == 0 )
                            // coordinate: https://git.io/JeYry
                            SendSentenceToAllInstruments( OCPN_DBP_STC_LON,
                                                          value, _T("SDMM"),
                                                          timestamp );
                        else if ( key->CmpNoCase(_T("latitude")) == 0 )
                            SendSentenceToAllInstruments( OCPN_DBP_STC_LAT,
                                                          value, _T("SDMM"),
                                                          timestamp );
                    } // selected by (low) priority
                }
                else if ( ( path->CmpNoCase(
                          _T("navigation.courseOverGroundTrue")) == 0 ) ||
                          ( path->CmpNoCase(_T("navigation.speedOverGround"))
                            == 0 ) ||
                          ( path->CmpNoCase(_T("navigation.magneticVariation"))
                            == 0 ) ) {
                    if ( mPriCOGSOG >= 3 ) {
                        if ( path->CmpNoCase(
                             _T("navigation.courseOverGroundTrue")) == 0 ) {
                            mPriCOGSOG = 3;
                            SendSentenceToAllInstruments(
                                OCPN_DBP_STC_COG,
                                mCOGFilter.filter( value * RAD_IN_DEG ),
                                _T("\u00B0"),
                                timestamp );
                        }
                        else if ( path->CmpNoCase(
                                  _T("navigation.speedOverGround")) == 0 ) {
                            mPriCOGSOG = 3;
                            SendSentenceToAllInstruments(
                                OCPN_DBP_STC_SOG,
                                toUsrSpeed_Plugin(
                                    mSOGFilter.filter( value * MS_IN_KNOTS ),
                                    g_iDashSpeedUnit ),
                                getUsrSpeedUnit_Plugin( g_iDashSpeedUnit ),
                                timestamp );
                        }
                    } // mPriCOGSOG
                } // then COGSOG contents
                else if ( path->CmpNoCase(_T("navigation.datetime")) == 0 ) {
                    if ( mPriDateTime >= 3 ) {
                        mPriDateTime = 3;
                        bool parseError;
                        wxDateTime msParsedDateTime =
                            parseRfc3359UTC( valStr, parseError );
                        if ( !parseError ) {
                            mUTCDateTime = msParsedDateTime;
                            wxDateTime resultingFakeUTC = mUTCDateTime.ToUTC();
                            wxLongLong resEpoch = resultingFakeUTC.GetValue();
                            wxLongLong fakeEpoch = mUTCDateTime.GetValue();
                            mUTCDateTzOffsetLL =
                                fakeEpoch.GetValue() - resEpoch.GetValue();
                            mUTCRealGpsEpoch =
                                fakeEpoch.GetValue() + mUTCDateTzOffsetLL;
                            mGNSSreceivedAtLocalMs = wxGetUTCTimeMillis();
                        }
                    } // mPriDateTime
                } // then date/time update received with the above data
            } // RMC

            else if ( sentenceId->CmpNoCase(_T("RSA")) == 0 ) {
                // https://git.io/Je3sA
                if ( path->CmpNoCase(_T("steering.rudderAngle")) == 0 ) {
                    SendSentenceToAllInstruments(
                        OCPN_DBP_STC_RSA,
                        value * RAD_IN_DEG, _T("\u00B0"),
                        timestamp );
                }
            } // RSA

            else if ( sentenceId->CmpNoCase(_T("VHW")) == 0 ) {
                // https://git.io/Je3GE
                if ( path->CmpNoCase(_T("navigation.headingTrue")) == 0 ) {
                    if ( mPriHeadingT >= 2 ) {
                        mPriHeadingT = 2;
                        SendSentenceToAllInstruments(
                            OCPN_DBP_STC_HDT,
                            value * RAD_IN_DEG, _T("\u00B0T"),
                            timestamp );
                        mHDT_Watchdog = gps_watchdog_timeout_ticks;
                    } // priority activation
                }
                else if ( path->CmpNoCase(
                              _T("navigation.headingMagnetic")) == 0 ) {
                    if ( mPriHeadingM >= 3 ) {
                        mPriHeadingM = 3;
                        SendSentenceToAllInstruments(
                            OCPN_DBP_STC_HDM,
                            value * RAD_IN_DEG, _T("\u00B0M"),
                            timestamp );
                        mHDx_Watchdog = gps_watchdog_timeout_ticks;
                    } // priority activation
                }
                else if ( path->CmpNoCase(_T("navigation.speedThroughWater"))
                          == 0 ) {
                    SendSentenceToAllInstruments(
                        OCPN_DBP_STC_STW,
                        toUsrSpeed_Plugin( value * MS_IN_KNOTS,
                                           g_iDashSpeedUnit ),
                        getUsrSpeedUnit_Plugin( g_iDashSpeedUnit ),
                        timestamp );
                    mStW_Watchdog = gps_watchdog_timeout_ticks;
                }
            } // VHW

            else if ( sentenceId->CmpNoCase(_T("VLW")) == 0 ) {
                // https://git.io/JeOrS (was nm here, now meters)
                if ( path->CmpNoCase(_T("navigation.trip.log")) == 0 ) {
                    /* Note: value from Signal K is "as received",
                       i.e. nautical miles */
                    if ( value >= 0.0 )
                        SendSentenceToAllInstruments(
                            OCPN_DBP_STC_VLW1,
                            toUsrDistance_Plugin( (value * M_IN_NM),
                                                  g_iDashDistanceUnit ),
                            getUsrDistanceUnit_Plugin( g_iDashDistanceUnit ),
                            timestamp );
                }
                else if ( path->CmpNoCase(_T("navigation.log")) == 0 ) {
                    if ( value >= 0.0 )
                        SendSentenceToAllInstruments(
                            OCPN_DBP_STC_VLW2,
                            toUsrDistance_Plugin( (value * M_IN_NM),
                                                  g_iDashDistanceUnit ),
                            getUsrDistanceUnit_Plugin( g_iDashDistanceUnit ),
                            timestamp );
                }
            } // VLW

            else if ( sentenceId->CmpNoCase(_T("VTG")) == 0 ) {
                /* https://git.io/Je3Zp for now, Dashboard ignores
                   "navigation.courseOverGroundMagnetic" */
                if ( mPriCOGSOG >= 2 ) {
                    if ( path->CmpNoCase(
                             _T("navigation.speedOverGround")) == 0 ) {
                        mPriCOGSOG = 2;
                        SendSentenceToAllInstruments(
                            OCPN_DBP_STC_SOG,
                            toUsrSpeed_Plugin(
                                mSOGFilter.filter( value * MS_IN_KNOTS ),
                                g_iDashSpeedUnit ),
                            getUsrSpeedUnit_Plugin( g_iDashSpeedUnit ),
                            timestamp );
                    }
                    else if ( path->CmpNoCase(
                              _T("navigation.courseOverGroundTrue")) == 0 ) {
                        mPriCOGSOG = 2;
                        SendSentenceToAllInstruments(
                            OCPN_DBP_STC_COG,
                            mCOGFilter.filter( value * RAD_IN_DEG ),
                            _T("\u00B0"),
                            timestamp );
                    }
                } // priority activation
            } // VTG

            else if ( sentenceId->CmpNoCase(_T("VWR")) == 0 ) { // 
                if ( mPriAWA >= 2 ) {
                    if ( path->CmpNoCase(
                         _T("environment.wind.speedApparent")) == 0 ) {
                        mPriAWA = 2;
                        SendSentenceToAllInstruments(
                            OCPN_DBP_STC_AWS,
                            toUsrSpeed_Plugin(
                                value * MS_IN_KNOTS, g_iDashWindSpeedUnit ),
                            getUsrSpeedUnit_Plugin( g_iDashWindSpeedUnit ),
                            timestamp );
                        this->SetNMEASentence_Arm_AWS_Watchdog();
                    }
                    else if ( path->CmpNoCase(
                              _T("environment.wind.angleApparent")) == 0 ) {
                        mPriAWA = 2;
                        SendSentenceToAllInstruments(
                            OCPN_DBP_STC_AWA,
                            std::abs( value ) * RAD_IN_DEG,
                            ( value < 0 ? L"\u00B0lr" : L"\u00B0rl" ),
                            timestamp );
                    }
                } // prirority activation
            } // VWR

            // VWT not implemented as for now (but Tactics calculates it)

            /* XDR is not implemented in the Signal K TCP delta stream yet
               but it is available from the NMEA stream which the OpenCPN
               can receive from Signal K - or it receives it from the
               multiplexer.
               Therefore it is searched and executed in the above,
               non-SignalK section. */

        } // then NMEA-0183 delta from Signal K

        else {  // Else another data source on SignalK, like NMEA-2000

            if ( this->m_pSkData->isSubscribedToAllPaths() )
                this->m_pSkData->UpdateNMEA2000PathList( path, key );

            this->SendDataToAllPathSubscribers(
                ( key == NULL ? *path : (*path + _T(".") + *key) ),
                ( std::isnan( value ) ? 0.0 : value), L"", timestamp );
            /*
              Send to "classical" (i.e. non-subscribing) instruments and to the
              the Tactics engine the values like they are done with NMEA-0183
              originating data. The OpenCPN fix provided values are not sent.
              We keep 1:1 mapping between SignalK keys https://git.io/Jep2E
              and Dashboard instrument keys.
            */

            bool depthvalue = false;
            if ( path->CmpNoCase(_T("environment.depth.belowTransducer"))
                 == 0 ) {
                if ( !mSiK_DPT_environmentDepthBelowKeel )
                    depthvalue = true;
            }
            else if ( path->CmpNoCase(_T("environment.depth.belowKeel"))
                      == 0 ) {  // depth + offset
                depthvalue = true;
                mSiK_DPT_environmentDepthBelowKeel = true; // lock priority
            }
            if ( depthvalue ) {
                double depth = value + g_dDashDBTOffset;
                SendSentenceToAllInstruments(
                    OCPN_DBP_STC_DPT,
                    toUsrDistance_Plugin( depth / 1852.0, g_iDashDepthUnit ),
                    getUsrDistanceUnit_Plugin( g_iDashDepthUnit ),
                    timestamp );
            }

            else if ( path->CmpNoCase(_T("navigation.gnss.methodQuality"))
                      == 0 ) {
                if ( valStr->CmpNoCase(_T("GNSS fix")) == 0 ) {
                    mSiK_navigationGnssMethodQuality = 1;
                }
                else {
                    mSiK_navigationGnssMethodQuality = 0;
                    mSatsInView = 0;
                    SendSentenceToAllInstruments(
                        OCPN_DBP_STC_SAT,
                        mSatsInView, _T(""),
                        timestamp );
                }
            }

           else if ( path->CmpNoCase(_T("navigation.gnss.satellites"))
                     == 0 ) {
                if ( mSiK_navigationGnssMethodQuality > 0 ) {
                    mSatsInView = value;
                    SendSentenceToAllInstruments(
                        OCPN_DBP_STC_SAT,
                        mSatsInView, _T(""),
                        timestamp );
                    mGPS_Watchdog = gps_watchdog_timeout_ticks;
                }
           }
            
            else if ( path->CmpNoCase(_T("navigation.position")) == 0 ) {
                if ( mSiK_navigationGnssMethodQuality > 0 ) {
                    if ( (mPriPosition >= 3) && (key != NULL) ) {
                        // See SetPositionFix() - It rules, even if no fix!
                        mPriPosition = 3;
                        if ( key->CmpNoCase(_T("longitude")) == 0 )
                            // coordinate: https://git.io/JeYry
                            SendSentenceToAllInstruments( OCPN_DBP_STC_LON,
                                                          value, _T("SDMM"),
                                                          timestamp );
                        if ( key->CmpNoCase(_T("latitude")) == 0 )
                            SendSentenceToAllInstruments( OCPN_DBP_STC_LAT,
                                                          value, _T("SDMM"),
                                                          timestamp );
                    }
                }
            }

            else if ( path->CmpNoCase(_T("navigation.headingMagnetic"))
                      == 0 ) {
                if ( mPriHeadingM >= 1 ) { 
                    if ( !std::isnan( value )) {
                        mPriHeadingM = 1;
                        mHdm = value * RAD_IN_DEG;
                        SendSentenceToAllInstruments( OCPN_DBP_STC_HDM,
                                                      mHdm,_T("\u00B0"),
                                                      timestamp );
                        mHDx_Watchdog = gps_watchdog_timeout_ticks;
                    }
                }
            }

            else if ( path->CmpNoCase(_T("navigation.magneticVariation"))
                      == 0 ) {
                if ( mPriVar >= 2 ) {
                    // see comment in NMEA equivalent: not really useful
                    if ( !std::isnan( value )) {
                        if ( value != 0.0 ) {
                            mPriVar = 2;
                            mVar = value * RAD_IN_DEG;
                            SendSentenceToAllInstruments( OCPN_DBP_STC_HMV,
                                                          mVar,
                                                          _T("\u00B0"),
                                                          timestamp );
                            mVar_Watchdog = gps_watchdog_timeout_ticks;
                        }
                    }
                }
                if ( !std::isnan( mVar )  && !std::isnan( mHdm ) &&
                     (mPriHeadingT > 3) ) {
                    mPriHeadingT = 4;
                    double heading = mHdm + mVar;
                    if (heading < 0)
                        heading += 360;
                    else if (heading >= 360.0)
                        heading -= 360;
                    SendSentenceToAllInstruments(OCPN_DBP_STC_HDT,
                                                 heading, _T("\u00B0"),
                                                 timestamp );
                    mHDT_Watchdog = gps_watchdog_timeout_ticks;
                }
            }
        
            else if ( path->CmpNoCase(_T("navigation.headingTrue")) == 0 ) {
                if ( mPriHeadingM >= 1 ) { 
                    if ( !std::isnan( value )) {
                        mPriHeadingM = 1;
                        mHdm = value * RAD_IN_DEG;
                        SendSentenceToAllInstruments( OCPN_DBP_STC_HDT,
                                                      mHdm, _T("\u00B0T"),
                                                      timestamp );
                        mHDT_Watchdog = gps_watchdog_timeout_ticks;
                    }
                }
            }

            else if ( path->CmpNoCase(_T("environment.outside.pressure"))
                      == 0 ) {
                // Note: value from Signal K is SI units Pa, convet to hPa
                double hPaPressure = value / 100.;
                if ( (hPaPressure > 800) && (hPaPressure < 1100) ) {
                    SendSentenceToAllInstruments( OCPN_DBP_STC_MDA,
                                                  hPaPressure, _T("hPa"),
                                                  timestamp );
                } // then valid pressure in hPa
            }

            else if ( path->CmpNoCase(_T("environment.water.temperature"))
                      == 0 ) {
                /* Note: value from Signal K is SI units, thus we
                   receive Kelvins */
                double TemperatureValue = value - CELCIUS_IN_KELVIN; 
                wxString TemperatureUnitOfMeasurement = _T("C"); // as MTW
                checkNMEATemperatureDataAndUnit(
                    TemperatureValue, TemperatureUnitOfMeasurement );
                SendSentenceToAllInstruments( OCPN_DBP_STC_TMP,
                                              TemperatureValue,
                                              TemperatureUnitOfMeasurement,
                                              timestamp );
            }

            else if ( path->CmpNoCase(_T("environment.wind.speedApparent"))
                      == 0 ) {
                if ( mPriAWA >= 1 ) {
                    mPriAWA = 1;
                    /* Note: value from Signal K is SI units, thus we
                       receive [m/s] */
                    SendSentenceToAllInstruments(
                        OCPN_DBP_STC_AWS,
                        toUsrSpeed_Plugin( value * MS_IN_KNOTS,
                                           g_iDashWindSpeedUnit ),
                        getUsrSpeedUnit_Plugin( g_iDashWindSpeedUnit ),
                        timestamp );
                    this->SetNMEASentence_Arm_AWS_Watchdog();
                } // AWA priority
            }

            else if ( path->CmpNoCase(_T("environment.wind.angleApparent"))
                      == 0 ) {
                if ( mPriAWA >= 1 ) {
                    mPriAWA = 1;
                    SendSentenceToAllInstruments(
                        OCPN_DBP_STC_AWA,
                        std::abs( value ) * RAD_IN_DEG,
                        ( value < 0 ? L"\u00B0lr" : L"\u00B0rl" ),
                        timestamp );
                } // AWA priority
            }

            else if ( path->CmpNoCase(_T("environment.wind.speedTrue"))
                      == 0 ) {
                if ( mPriTWA >= 1 ) {
                    mPriTWA = 1;
                    SendSentenceToAllInstruments(
                        OCPN_DBP_STC_TWS,
                        toUsrSpeed_Plugin( value * MS_IN_KNOTS,
                                           g_iDashWindSpeedUnit ),
                        getUsrSpeedUnit_Plugin( g_iDashWindSpeedUnit ),
                        timestamp );
                    SendSentenceToAllInstruments(
                        OCPN_DBP_STC_TWS2,
                        toUsrSpeed_Plugin( value * MS_IN_KNOTS,
                                           g_iDashWindSpeedUnit ),
                        getUsrSpeedUnit_Plugin( g_iDashWindSpeedUnit ),
                        timestamp );
                    this->SetNMEASentence_Arm_TWS_Watchdog();
                } // TWA priority
            }

            else if ( path->CmpNoCase(_T("environment.wind.angleTrueWater"))
                      == 0 ) {
                if ( mPriTWA >= 1 ) {
                    mPriTWA = 1;
                    double windDegDir = std::abs( value ) * RAD_IN_DEG;
                    if ( windDegDir > 360.0 )
                        windDegDir -= 360.;
                    SendSentenceToAllInstruments(
                        OCPN_DBP_STC_TWA,
                        windDegDir,
                        ( value < 0 ? L"\u00B0lr" : L"\u00B0rl" ),
                        timestamp );
                    this->SetNMEASentence_Arm_TWD_Watchdog();
                } // TWA priority
            }
        
            /* See the comment in the RMB  sentence's interpretation above
               (when coming from OpenCPN): the controversy of having it here
               is the same: the infamous VMG interpretation of next
               destination waypoint is not the same as in Tactics,
               i.e. for the sailing boat performance criteria.
               It is kept here since it can be considered belonging to
               Dashboard which needs to serve also the needs of a cruising
               sailing and motor boats but it can be useful in the off-shore
               races, too.
               Dashboard ignores navigation.courseRhumbline.nextPoint,
               as for now */
            else if ( path->CmpNoCase(
                      _T("navigation.courseRhumbline.nextPoint.bearingTrue"))
                      == 0 ) {
                SendSentenceToAllInstruments(
                    OCPN_DBP_STC_BRG,
                    value * RAD_IN_DEG,
                    _T("\u00B0"), /* as for now, Origin ID not available
                                     from Signal K */
                    timestamp );
                this->SetNMEASentence_Arm_BRG_Watchdog();
            }

            else if ( path->CmpNoCase(
                 _T("navigation.courseRhumbline.nextPoint.velocityMadeGood"))
                      == 0 ) {
                /* This is THE carburator for hours of useless "discussions"
                   in forums; comment it out if you don't like it :) */
                SendSentenceToAllInstruments(
                    OCPN_DBP_STC_VMG,
                    toUsrSpeed_Plugin( value * MS_IN_KNOTS,
                                       g_iDashWindSpeedUnit ),
                    getUsrSpeedUnit_Plugin( g_iDashWindSpeedUnit ),
                    timestamp );
                this->SetNMEASentence_Arm_VMG_Watchdog();
            }

            else if ( path->CmpNoCase(
                      _T("navigation.courseRhumbline.nextPoint.distance"))
                      == 0 ) {
                SendSentenceToAllInstruments(
                    OCPN_DBP_STC_DTW,
                    value * KM_IN_NM, _T("Nm"),
                    timestamp );
            }

            else if ( ( path->CmpNoCase(
                       _T("navigation.courseOverGroundTrue")) == 0 ) ||
                      ( path->CmpNoCase(
                          _T("navigation.speedOverGround")) == 0 ) ) {
                if ( mPriCOGSOG >= 2 ) {
                    if ( path->CmpNoCase(
                             _T("navigation.courseOverGroundTrue")) == 0 ) {
                        mPriCOGSOG = 2;
                        SendSentenceToAllInstruments(
                            OCPN_DBP_STC_COG,
                            mCOGFilter.filter( value * RAD_IN_DEG ),
                            _T("\u00B0"),
                            timestamp );
                    }
                    else if ( path->CmpNoCase(
                                  _T("navigation.speedOverGround")) == 0 ) {
                        mPriCOGSOG = 2;
                        SendSentenceToAllInstruments(
                            OCPN_DBP_STC_SOG,
                            toUsrSpeed_Plugin(
                                mSOGFilter.filter( value * MS_IN_KNOTS ),
                                g_iDashSpeedUnit ),
                            getUsrSpeedUnit_Plugin( g_iDashSpeedUnit ),
                            timestamp );
                    }
                } // mPriCOGSOG
            } // then COGSOG contents

            else if ( path->CmpNoCase(
                      _T("navigation.courseOverGroundMagnetic")) == 0 ) {
                mPriCOGSOG = 3;
                SendSentenceToAllInstruments(
                    OCPN_DBP_STC_MCOG,
                    value * RAD_IN_DEG,
                    _T("\u00B0M"),
                    timestamp );
            }
           
            else if ( path->CmpNoCase(_T("navigation.datetime")) == 0 ) {
                if ( mPriDateTime >= 3 ) {
                    mPriDateTime = 3;
                    bool parseError;
                    wxDateTime msParsedDateTime =
                        parseRfc3359UTC( valStr, parseError );
                    if ( !parseError ) {
                        mUTCDateTime = msParsedDateTime;
                        wxDateTime resultingFakeUTC = mUTCDateTime.ToUTC();
                        wxLongLong resEpoch = resultingFakeUTC.GetValue();
                        wxLongLong fakeEpoch = mUTCDateTime.GetValue();
                        mUTCDateTzOffsetLL =
                            fakeEpoch.GetValue() - resEpoch.GetValue();
                        mUTCRealGpsEpoch =
                            fakeEpoch.GetValue() + mUTCDateTzOffsetLL;
                        mGNSSreceivedAtLocalMs = wxGetUTCTimeMillis();
                    }
                } // mPriDateTime
            } // then date/time update received with the above data

            else if ( path->CmpNoCase(
                      _T("steering.rudderAngle")) == 0 ) {
                SendSentenceToAllInstruments(
                    OCPN_DBP_STC_RSA,
                    value * RAD_IN_DEG, _T("\u00B0"),
                    timestamp );
            }

            else if ( path->CmpNoCase(
                      _T("navigation.speedThroughWater")) == 0 ) {
                SendSentenceToAllInstruments(
                    OCPN_DBP_STC_STW,
                    toUsrSpeed_Plugin(
                        value * MS_IN_KNOTS, g_iDashSpeedUnit ),
                    getUsrSpeedUnit_Plugin( g_iDashSpeedUnit ),
                    timestamp );
                mStW_Watchdog = gps_watchdog_timeout_ticks;
            }

            else if ( path->CmpNoCase(
                      _T("navigation.trip.log")) == 0 ) {
                // Note: value from Signal K is meters here
                if ( value >= 0.0 )
                    SendSentenceToAllInstruments(
                        OCPN_DBP_STC_VLW1,
                        toUsrDistance_Plugin( (value * M_IN_NM),
                                              g_iDashDistanceUnit ),
                        getUsrDistanceUnit_Plugin( g_iDashDistanceUnit ),
                        timestamp );
            }
            else if ( path->CmpNoCase(_T("navigation.log")) == 0 ) {
                if ( value >= 0.0 )
                    SendSentenceToAllInstruments(
                        OCPN_DBP_STC_VLW2,
                        toUsrDistance_Plugin( (value * M_IN_NM),
                                              g_iDashDistanceUnit ),
                        getUsrDistanceUnit_Plugin( g_iDashDistanceUnit ),
                        timestamp );
            }

        } // then non-NMEA-0183 (like NMEA-2000) delta from Signal K
                
    } // else Signal K

}

void dashboard_pi::SetPositionFix( PlugIn_Position_Fix &pfix )
{
    if ( mPriPosition >= 1 ) {
        mPriPosition = 1;
        SendSentenceToAllInstruments( OCPN_DBP_STC_LAT, pfix.Lat, _T("SDMM") );
        SendSentenceToAllInstruments( OCPN_DBP_STC_LON, pfix.Lon, _T("SDMM") );
        mGPS_Watchdog = gps_watchdog_timeout_ticks;
    }
    if ( mPriCOGSOG >= 1 && !std::isnan( pfix.Cog ) && !std::isnan( pfix.Sog ) ) {
        mPriCOGSOG = 1;
        SendSentenceToAllInstruments(
            OCPN_DBP_STC_SOG,
            toUsrSpeed_Plugin( mSOGFilter.filter(pfix.Sog), g_iDashSpeedUnit ),
            getUsrSpeedUnit_Plugin( g_iDashSpeedUnit ) );
        SendSentenceToAllInstruments(
            OCPN_DBP_STC_COG,
            mCOGFilter.filter(pfix.Cog),
            _T("\u00B0") );
        if ( !std::isnan( pfix.Var ) ) {
            double dMagneticCOG = mCOGFilter.get() - pfix.Var;
            if ( dMagneticCOG < 0.0 ) dMagneticCOG = 360.0 + dMagneticCOG;
            if ( dMagneticCOG > 360.0 ) dMagneticCOG = dMagneticCOG - 360.0;
            SendSentenceToAllInstruments( OCPN_DBP_STC_MCOG,
                                          dMagneticCOG ,
                                          _T("\u00B0M") );
        }
    }
    if ( mPriVar >= 1 ) {
        if ( !std::isnan( pfix.Var ) ){
            mPriVar = 1;
            mVar = pfix.Var;
            mVar_Watchdog = gps_watchdog_timeout_ticks;
            SendSentenceToAllInstruments(
                OCPN_DBP_STC_HMV,
                pfix.Var,
                _T("\u00B0") );
        }
    }
    if ( mPriDateTime >= 6 ) { // priority is given to data from GNSS
        mPriDateTime = 6;
        mUTCDateTime.Set( pfix.FixTime );
        mUTCDateTime = mUTCDateTime.ToUTC();
        mUTCDateTzOffsetLL = 0LL;
        mUTCRealGpsEpoch = mUTCDateTime.GetValue().GetValue(); // now the same
        mGNSSreceivedAtLocalMs = wxGetUTCTimeMillis();
    }
    mSatsInView = pfix.nSats;
    SendSentenceToAllInstruments( OCPN_DBP_STC_SAT, mSatsInView, _T("") );

}

void dashboard_pi::SetCursorLatLon( double lat, double lon )
{
    this->TacticsSetCursorLatLon(lat, lon);
    SendSentenceToAllInstruments( OCPN_DBP_STC_PLA, lat, _T("SDMM") );
    SendSentenceToAllInstruments( OCPN_DBP_STC_PLO, lon, _T("SDMM") );

}

void dashboard_pi::SetActiveLegInfo(Plugin_Active_Leg_Info &leg_info) {
    if ( mActiveLegInfo )
       *mActiveLegInfo = leg_info;
}

void dashboard_pi::ClearActiveRouteMessages()
{
    mRouteActivatedName = wxEmptyString;
    mRouteActivatedGUID = wxEmptyString;
    mWpActivatedName = wxEmptyString;
    mWpActivatedGUID = wxEmptyString;
    mWpArrivedName = wxEmptyString;
    mWpArrivedGUID = wxEmptyString;
    mWpArrivedIsSkipped = false;
    mWpArrivedNextName = wxEmptyString;
    mWpArrivedNextGUID = wxEmptyString;
    if ( mActiveLegInfo )
        delete mActiveLegInfo;
    mActiveLegInfo = nullptr;
}

void dashboard_pi::SetPluginMessage(wxString &message_id, wxString &message_body)
{
    wxJSONValue  root;
    // construct a JSON parser
    wxJSONReader reader;
    int numErrors = reader.Parse( message_body, &root );
    if ( numErrors > 0 )
        return;
    
    if ( message_id == _T("WMM_VARIATION_BOAT") ) {

        wxString decl = root[_T("Decl")].AsString();
        double decl_val;
        decl.ToDouble(&decl_val);

        if ( mPriVar >= 4 ) {
            mPriVar = 4;
            mVar = decl_val;
            mVar_Watchdog = gps_watchdog_timeout_ticks;
            SendSentenceToAllInstruments( OCPN_DBP_STC_HMV, mVar, _T("\u00B0") );
        }
    }
    else if ( message_id == _T("OCPN_RTE_ACTIVATED") ) {
        mRouteActivatedName = root[_T("Route_activated")].AsString();  
        mRouteActivatedGUID = root[_T("GUID")].AsString();
        mActiveLegInfo = new Plugin_Active_Leg_Info();
        mActiveLegInfo->wp_name = wxEmptyString;
    }
    else if ( message_id == _T("OCPN_RTE_DEACTIVATED") ) {
        ClearActiveRouteMessages();
    }
    else if ( message_id == _T("OCPN_RTE_ENDED") ) {
        ClearActiveRouteMessages();
    }
    else if ( message_id == _T("OCPN_WPT_ACTIVATED") ) {
        mWpActivatedName = root[_T("WP_activated")].AsString();
        mWpActivatedGUID = root[_T("GUID")].AsString();
    }
    else if ( message_id == _T("OCPN_WPT_ARRIVED") ) {
        if ( root.HasMember("isSkipped") ) {
            mWpArrivedIsSkipped = root[_T("isSkipped")].AsBool();
            mWpArrivedName = root[_T("WP_arrived")].AsString();
            if ( root.HasMember("Next_WP") ) {
                mWpArrivedGUID = wxEmptyString;
            } /* then the GUID of the arrived WP will be overwritten(!),
                 in O <= v5.2 - OK, clear it. */
            else {
                mWpArrivedGUID = root[_T("GUID")].AsString();
            } // else the GUID of the arrived WP will not be overwritten
        } // then there was an active point to which we have arrived
        if ( root.HasMember("Next_WP") ) {
            mWpArrivedNextName = root[_T("Next_WP")].AsString();
            mWpArrivedNextGUID = root[_T("GUID")].AsString();
        } // then there is next waypoint on the route
        else {
            mWpArrivedNextName = wxEmptyString;
            mWpArrivedNextGUID = wxEmptyString;
        }
    }
    else if ( message_id == _T("OpenCPN Config") ) {
        int ocpnMajorVersion = root[_T("OpenCPN Version Major")].AsInt();
        if ( !mBmajorVersion_warning_given ) {
            if ( ocpnMajorVersion != PLUGIN_TARGET_OCPN_VERSION_MAJOR ) {
                wxString message = wxString::Format(
                    _T("%s%d%s%d"),
                    _("This plug-in is intended for OpenCPN v"),
                    PLUGIN_TARGET_OCPN_VERSION_MAJOR,
                    _(".\nYou have OpenCPN v"),
                    ocpnMajorVersion );
                wxMessageDialog dlg(
                    GetOCPNCanvasWindow(), message,
                    _T("dashboard_tactics_pi message"), wxOK );
                (void) dlg.ShowModal();
                mBmajorVersion_warning_given = true;
            }
        }
        if ( !mBminorVersion_warning_given ) {
            int ocpnMinorVersion = root[_T("OpenCPN Version Minor")].AsInt();
            if ( ocpnMinorVersion < PLUGIN_MINIMUM_OCPN_VERSION_MINOR ) {
                wxString message = wxString::Format(
                    _T("%s%d%s%d%s%d%s%d"),
                    _("This plug-in is intended for OpenCPN v"),
                    PLUGIN_TARGET_OCPN_VERSION_MAJOR, _T("."),
                    PLUGIN_MINIMUM_OCPN_VERSION_MINOR,
                    _(" or superior minor version.\nYou have OpenCPN v"),
                    ocpnMajorVersion, _T("."), ocpnMinorVersion );
                wxMessageDialog dlg( GetOCPNCanvasWindow(), message,
                                     _T("dashboard_tactics_pi message"), wxOK );
                (void) dlg.ShowModal();
                mBminorVersion_warning_given = true;
            }
        }
    }
    else if ( message_id == _T("OCPN_OPENGL_CONFIG") ) {
        if ( !this->getTacticsDCmsgShown() ) {
            bool bOpenGLsetupComplete = root[_T("setupComplete")].AsBool();
            if ( !bOpenGLsetupComplete ) {
                wxString message(
                    _("OpenGL appears to be enabled but OpenCPN reports\n") +
                    _("OpenGL setup not being complete.\n") +
                    _("Chart overlay functions in this plug-in may fail.\n") +
                    _("Please study OpenCPN log file for OpenGL error messages.") );
                wxMessageDialog dlg(
                    GetOCPNCanvasWindow(), message, _T("dashboard_tactics_pi message"),
                    wxOK|wxICON_ERROR);
                (void) dlg.ShowModal();
                this->setTacticsDCmsgShownTrue();
            }
        }
    }
}

int dashboard_pi::GetToolbarToolCount( void )
{
    return 1;
}

void dashboard_pi::ShowPreferencesDialog( wxWindow* parent )
{
    DashboardPreferencesDialog *dialog = new DashboardPreferencesDialog(
        parent, wxID_ANY,
        m_ArrayOfDashboardWindow,
        GetCommonName(),
        GetNameVersion(),
        wxDefaultPosition
     );

    if ( dialog->ShowModal() == wxID_OK ) {
        delete g_pFontTitle;
        g_pFontTitle = new wxFont(
            dialog->m_pFontPickerTitle->GetSelectedFont() );
        delete g_pFontData;
        g_pFontData = new wxFont(
            dialog->m_pFontPickerData->GetSelectedFont() );
        delete g_pFontLabel;
        g_pFontLabel = new wxFont(
            dialog->m_pFontPickerLabel->GetSelectedFont() );
        delete g_pFontSmall;
        g_pFontSmall = new wxFont(
            dialog->m_pFontPickerSmall->GetSelectedFont() );

        /* OnClose should handle that for us normally but it does not
           seem to do so - we must save changes first */
        dialog->SaveDashboardConfig();
        m_ArrayOfDashboardWindow.Clear();
        m_ArrayOfDashboardWindow = dialog->m_Config;

        SetApplySaveWinRequest();

        SetToolbarItemState(
            m_toolbar_item_id, GetDashboardWindowShownCount() != 0 );
    }
    dialog->Destroy();
}

void dashboard_pi::SetColorScheme( PI_ColorScheme cs )
{
    m_colorScheme = cs;
    for( size_t i = 0; i < m_ArrayOfDashboardWindow.GetCount(); i++ ) {
        DashboardWindow *dashboard_window =
            m_ArrayOfDashboardWindow.Item( i )->m_pDashboardWindow;
        if ( dashboard_window ) {
            dashboard_window->SetColorScheme( cs );
            dashboard_window->SendColorSchemeToAllJSInstruments( cs );
        }
    }
}

int dashboard_pi::GetDashboardWindowShownCount()
{
    int cnt = 0;

    for( size_t i = 0; i < m_ArrayOfDashboardWindow.GetCount(); i++ ) {
        DashboardWindow *dashboard_window = m_ArrayOfDashboardWindow.Item(
            i )->m_pDashboardWindow;
        if ( dashboard_window ) {
            wxAuiPaneInfo &pane = m_pauimgr->GetPane( dashboard_window );
            if ( pane.IsOk() && pane.IsShown() ) cnt++;
        }
    }
    return cnt;
}


void dashboard_pi::OnToolbarToolCallback( int id )
{
    int cnt = GetDashboardWindowShownCount();

    bool b_anyviz = false;
    for( size_t i = 0; i < m_ArrayOfDashboardWindow.GetCount(); i++ ) {
        DashboardWindowContainer *cont = m_ArrayOfDashboardWindow.Item( i );
        if ( cont->m_bIsVisible ) {
            b_anyviz = true;
            break;
        }
    }

    for( size_t i = 0; i < m_ArrayOfDashboardWindow.GetCount(); i++ ) {
        DashboardWindowContainer *cont = m_ArrayOfDashboardWindow.Item( i );
        DashboardWindow *dashboard_window = cont->m_pDashboardWindow;
        if ( dashboard_window ) {
            wxAuiPaneInfo &pane = m_pauimgr->GetPane( dashboard_window );
            if ( pane.IsOk() ) {
                bool b_reset_pos = false;

#ifdef __WXMSW__
                /* Support MultiMonitor setups which an allow negative
                   window positions. If the requested window title bar does
                   not intersect any installed monitor,then default to
                   simple primary monitor positioning. */
                RECT frame_title_rect;
                frame_title_rect.left = pane.floating_pos.x;
                frame_title_rect.top = pane.floating_pos.y;
                frame_title_rect.right = pane.floating_pos.x +
                    pane.floating_size.x;
                frame_title_rect.bottom = pane.floating_pos.y + 30;
                if ( NULL == MonitorFromRect( &frame_title_rect,
                                              MONITOR_DEFAULTTONULL ) )
                    b_reset_pos = true;
#else
                /*  Make sure drag bar (title bar) of window intersects
                    wxClient Area of screen */
                wxRect window_title_rect;// conservative estimate
                window_title_rect.x = pane.floating_pos.x;
                window_title_rect.y = pane.floating_pos.y;
                window_title_rect.width = pane.floating_size.x;
                window_title_rect.height = 30;
                wxRect ClientRect = wxGetClientDisplayRect();
                // Prevent the new window from being too close to the edge
                ClientRect.Deflate(60, 60);
                if ( !ClientRect.Intersects( window_title_rect ) )
                    b_reset_pos = true;
#endif
                if ( b_reset_pos )
                    pane.FloatingPosition( 50, 50 );
                if ( cnt == 0 )
                    if ( b_anyviz )
                        pane.Show( cont->m_bIsVisible );
                    else {
                        cont->m_bIsVisible = cont->m_bPersVisible;
                        pane.Show( cont->m_bIsVisible );
                    }
                else
                    pane.Show( false );
            }

            /* This patch fixes a bug in wxAUIManager FS#548:
               Dropping a DashBoard Window right on top on the (supposedly
               fixed) chart bar window causes a resize of the chart bar, and
               the Dashboard window assumes some of its properties.
               The Dashboard window is no longer grabbable...
               Workaround:  detect this case, and force the pane to be on
               a different Row so that the display is corrected by toggling
               the dashboard off and back on. */
            if ( ( pane.dock_direction == wxAUI_DOCK_BOTTOM ) &&
                 pane.IsDocked() )
                pane.Row( 2 );
        }
    }
    /* Toggle is handled by the toolbar but we must keep plugin manager
       b_toggle updated to actual status to ensure right status upon
       toolbar rebuild */
    int iToolbarToolCallbackShownWindows = GetDashboardWindowShownCount();
    ( iToolbarToolCallbackShownWindows != 0 ?
        m_bToggledStateVisible = true : m_bToggledStateVisible = false );
    SetToolbarItemState( m_toolbar_item_id, m_bToggledStateVisible );
    this->SetToggledStateVisible( m_bToggledStateVisible );
    m_pauimgr->Update();
}

void dashboard_pi::OnContextMenuItemCallback(int id)
{
    // so far only thes  parent class deals with this event
    this->TacticsOnContextMenuItemCallback(id);
    return;
}

void dashboard_pi::UpdateAuiStatus( void )
{
    /* This method is called by OpenCPN (pluginmanager.cpp) after the PlugIn
       is initialized and the frame has done its initial layout, possibly
       from a saved wxAuiManager "Perspective" (see also OCPN_AUIManager.cpp,
       v5.0 knows type "Dashboard" but not "DashT"... what can one do...). 
       It is a chance for the PlugIn to syncronize itself internally with the
       state of any Panes that were added to the frame in the PlugIn ctor. */

     for( size_t i = 0; i < m_ArrayOfDashboardWindow.GetCount(); i++ ) {
        DashboardWindowContainer *cont = m_ArrayOfDashboardWindow.Item( i );
        wxAuiPaneInfo &pane = m_pauimgr->GetPane( cont->m_pDashboardWindow );
        // Initialize visible state as perspective is loaded now
        cont->m_bIsVisible = ( pane.IsOk() && pane.IsShown() );

    }
    m_pauimgr->Update();

    /* We use this callback here to keep the context menu selection in sync
       with the window state */
    int iUpdateAuiShownWindows = GetDashboardWindowShownCount();
    ( iUpdateAuiShownWindows != 0 ?
        m_bToggledStateVisible = true : m_bToggledStateVisible = false );
    SetToolbarItemState( m_toolbar_item_id, m_bToggledStateVisible );
    this->SetToggledStateVisible( m_bToggledStateVisible );
}

void dashboard_pi::ApplyConfig(
    bool init
    )
{
    wxArrayOfDashboard replacedDashboards;
    replacedDashboards.Clear();
    wxArrayOfDashboard addedDashboards;
    addedDashboards.Clear();
    // Reverse order to allow deleting using indexed container
    for ( size_t i = m_ArrayOfDashboardWindow.GetCount(); i > 0; i-- ) {
        DashboardWindowContainer *cont = m_ArrayOfDashboardWindow.Item( i - 1 );
        if ( cont->m_bIsDeleted ) {
            if ( cont->m_pDashboardWindow ) {
                m_pauimgr->DetachPane( cont->m_pDashboardWindow );
                cont->m_pDashboardWindow->Close( true );
                cont->m_pDashboardWindow->Destroy();
            }
            m_ArrayOfDashboardWindow.Remove( cont );
            delete cont;
        }
    } // For loop array searching for deleted Dashboards
    
    for ( size_t i = 0; i < m_ArrayOfDashboardWindow.GetCount(); i++ ) {
        DashboardWindowContainer *cont = m_ArrayOfDashboardWindow.Item( i );
        int orient =
            ( cont->m_sOrientation == _T("V") ? wxVERTICAL : wxHORIZONTAL );
        DashboardWindowContainer *newcont =
            new DashboardWindowContainer( cont );
        /*
          Prepare a new window pane with instruments if first time
          or if a floating replacement is needed
        */
        wxAuiPaneInfo p_cont;
        bool wIsDocked = false;
        if ( newcont->m_pDashboardWindow ) {
            p_cont = m_pauimgr->GetPane( newcont->m_pDashboardWindow );
            if ( p_cont.IsOk() ) {
                if ( p_cont.IsDocked() ) {
                    wIsDocked = true;
                } // then window is in a pane which is docked
            } // then a valid pane
        } // then this is non-init (run-time) and there is a window pane
        bool addpane = false;
        bool rebuildpane = false; // either or but not both
        if ( init ) {
            addpane = true;
        } // in the init we need always a new pane, pro-forma
        else {
            if ( cont->m_pDashboardWindow ) {
                if ( !cont->m_pDashboardWindow->isInstrumentListEqual(
                        newcont->m_aInstrumentList ) ) {
                    addpane = true;
                } /* then a change in instruments replacement window is
                     needed, needs a pane */
                else {
                    p_cont = m_pauimgr->GetPane( cont->m_pDashboardWindow );
                    if ( !p_cont.IsOk() ) {
                        addpane = true;
                    } /* then there is no pane for this window, create one
                         (with a replacement window) */
                    else {
                        if ( wIsDocked ) {
                            if ( !newcont->m_bIsDocked ) {
                                cont->m_bIsDocked = true;
                                newcont->m_bIsDocked = true;
                                rebuildpane = true;
                            } /* Has been just docked, a rerarrangement is
                                 needed (ov50 some cases) */
                        } /* then there is a pane, unmodified instuments,
                             docked */
                        else {
                            if ( newcont->m_bIsDocked ) {
                                cont->m_bIsDocked = false;
                                newcont->m_bIsDocked = false;
                            } /* from docked to undocked, just register,
                                 no need for rearrangement (ov50) */
                            int orientNow=
                                cont->m_pDashboardWindow->GetSizerOrientation();
                            if ( (orientNow == wxHORIZONTAL) &&
                                 (newcont->m_sOrientation == _T("V")) ) {
                                addpane = true;
                                orient = wxVERTICAL;
                            } // then orientation change request to vertical
                            if ( (orientNow == wxVERTICAL) &&
                                 (newcont->m_sOrientation == _T("H")) ) {
                                addpane = true;
                                orient = wxHORIZONTAL;
                            } // then orientation change request to horiz.
                        } /* else there is a pane, unmodified instruments,
                             floating */
                    } // else pane is OK
                } // else there is no change in the instrument list
            } // then there is an instrument dashboard window
            else {
                addpane = true;
            } // else there is no instrument window
        } // else not init, study run-time dashboard window

        bool NewDashboardCreated = false;
        if ( addpane ) {
            newcont->m_pDashboardWindow = new DashboardWindow(
                GetOCPNCanvasWindow(), wxID_ANY,
                m_pauimgr, this, orient, (init ? cont : newcont),
                wxPanelNameStr,   /* note, ov51 commit https://git.io/JfoVy,
                                     _requires_ "panel" (as Dashboard) */
                this->m_pSkData );
            newcont->m_pDashboardWindow->Show( false );
            newcont->m_pDashboardWindow->SetInstrumentList(
                newcont->m_aInstrumentList, newcont->m_aInstrumentIDs, init );
            if ( !init )
                newcont->m_sName = MakeName();
            NewDashboardCreated = true;
        } // then a pane will be added, create a window for it.
        /*
          Position of the frame, initial or existing.
        */
        bool vertical = true;
        if ( orient == wxHORIZONTAL )
            vertical = false;

        wxPoint position;
        if ( wIsDocked )
            position = wxDefaultPosition;
        else {
            position = m_pluginFrame->GetPosition();
            position.x += 100;
            position.y += 100;
        }
        if ( !init && NewDashboardCreated ) {
            if ( newcont->m_pDashboardWindow ) {
                if ( p_cont.IsOk() ) {
                    if ( !wIsDocked )
                        position = p_cont.floating_pos;
                } /* then let's study if we can put the window in its
                     original position */
            } // then there is a window in this pane
        } // then this is a run-time call
        /*
          The logic for creating a new window pane is as follows
          init: always create a new window pane
          otherwise: create only if change in the contents, orientation, etc.
        */
        wxAuiPaneInfo p;
        if ( addpane ) {
            wxSize sz = newcont->m_pDashboardWindow->GetMinSize();
            // Mac has a little trouble with initial Layout() sizing...
#ifdef __WXOSX__
            if (sz.x == 0)
                sz.IncTo( wxSize( 160, 388) );
#endif
            p = wxAuiPaneInfo().Name( newcont->m_sName ).Caption(
                newcont->m_sCaption ).CaptionVisible(
                    false ).TopDockable( !vertical ).BottomDockable(
                        !vertical ).LeftDockable( false ).RightDockable(
                            vertical ).MinSize( sz ).BestSize(
                                sz ).FloatingSize( sz ).FloatingPosition(
                                    position ).Float().Show(
                                        false ).Gripper(false) ;
        } /* then it was necessary to add new pane for init
             or replacement resizing */
        else {
            m_pauimgr->GetPane( newcont->m_pDashboardWindow ).Caption(
                newcont->m_sCaption ).Show( newcont->m_bIsVisible );
        } /* else is a non-init run on an existing and unmodified
             pane, keep it */
        if ( addpane && !init ) {
            addedDashboards.Add( newcont );
            replacedDashboards.Add( cont );
            newcont->m_bPersVisible = cont->m_bIsVisible;
            if ( cont->m_pDashboardWindow ) {
                m_pauimgr->GetPane(
                    cont->m_pDashboardWindow ).FloatingPosition(
                        position ).Float(); // undock if docked
                m_pauimgr->DetachPane( cont->m_pDashboardWindow );
                (void ) cont->m_pDashboardWindow->Close( false );
                m_pauimgr->Update();
                if ( !cont->m_pDashboardWindow->Destroy() ) {
                    wxLogMessage(
                        "dashboard_tactics_pi: INFO: rearranged window pane "
                        "failed in Destroy()." );
                } // then window pane added to a list for later deletion
            } /* then this is an existing window in an existing window pane,
                 replaced with a new one */
            m_pauimgr->AddPane( newcont->m_pDashboardWindow, p, position);
            newcont->m_pDashboardWindow->Show( newcont->m_bIsVisible );
            m_pauimgr->GetPane( newcont->m_pDashboardWindow ).Show(
                newcont->m_bIsVisible );
            m_pauimgr->Update();
        } /* then we have created a pane and it is a replacement of an
             excisting pane: detach/destroy the one it replaces */
        else {
            if ( init ) {
                m_pauimgr->AddPane(
                    newcont->m_pDashboardWindow, p, position);
                newcont->m_pDashboardWindow->Show( newcont->m_bIsVisible );
                newcont->m_bPersVisible = newcont->m_bIsVisible;
                m_pauimgr->GetPane( newcont->m_pDashboardWindow ).Show(
                    newcont->m_bIsVisible );
                cont->m_pDashboardWindow = newcont->m_pDashboardWindow;
                if ( wIsDocked ) {
                    cont->m_bIsDocked = true;  /* Memo ov50: never comes
                                                  here in Init() - docked
                                                  pane is not recognized as
                                                  such ! */
                } /* Then was created as docked, however the container
                     constructor defaults to floating */
                m_pauimgr->Update();
            } // then a brand new window, register it
            else {
                m_pauimgr->GetPane( cont->m_pDashboardWindow ).Show(
                    newcont->m_bIsVisible ).Caption( newcont->m_sCaption );
                if ( rebuildpane ) {
                    if ( wIsDocked ) {
                        cont->m_bIsDocked = true;
                    } // was docked and rebuilt
                }
                m_pauimgr->Update();
                if ( NewDashboardCreated ) {
                    newcont->m_pDashboardWindow->Close();
                    newcont->m_pDashboardWindow->Destroy();
                    newcont->m_pDashboardWindow = nullptr;
                } // then, just in case, garbage collection
            } // no need to do a replacement or to create a new window
        } // else brand new pane or no action
    }  // for dashboard window containers remaining after deletions

    for( size_t i = 0; i < replacedDashboards.GetCount(); i++ ) {
        m_ArrayOfDashboardWindow.Remove( replacedDashboards.Item( i ) );
    }
    for( size_t i = 0; i < addedDashboards.GetCount(); i++ ) {
        m_ArrayOfDashboardWindow.Add( addedDashboards.Item( i ) );
    }

    this->TacticsApplyConfig();

    m_pauimgr->Update();
    mSOGFilter.setFC(g_iDashSOGDamp ? 1.0 / (2.0*g_iDashSOGDamp) : 0.0);
    mCOGFilter.setFC(g_iDashCOGDamp ? 1.0 / (2.0*g_iDashCOGDamp) : 0.0);
    mCOGFilter.setType(IIRFILTER_TYPE_DEG);
}

void dashboard_pi::PopulateContextMenu( wxMenu* menu )
{
    for( size_t i = 0; i < m_ArrayOfDashboardWindow.GetCount(); i++ ) {
        DashboardWindowContainer *cont = m_ArrayOfDashboardWindow.Item( i );
        wxMenuItem* item = menu->AppendCheckItem( i+1, cont->m_sCaption );
        item->Check( cont->m_bIsVisible );
    }
}

void dashboard_pi::ShowDashboard( size_t id, bool visible )
{
    if ( id < m_ArrayOfDashboardWindow.GetCount() ) {
        DashboardWindowContainer *cont = m_ArrayOfDashboardWindow.Item( id );
        m_pauimgr->GetPane( cont->m_pDashboardWindow ).Show( visible );
        cont->m_bIsVisible = visible;
        cont->m_bPersVisible = visible;
        m_pauimgr->Update();
    }
}

bool dashboard_pi::RenderOverlay(wxDC &dc, PlugIn_ViewPort *vp)
{
    return this->TacticsRenderOverlay( dc, vp );
}
bool dashboard_pi::RenderGLOverlay(wxGLContext *pcontext, PlugIn_ViewPort *vp)
{
    return this->TacticsRenderGLOverlay( pcontext, vp );
}
