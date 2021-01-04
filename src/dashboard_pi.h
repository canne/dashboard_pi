/******************************************************************************
 * $Id: dashboard_pi.h, v1.0 2010/08/05 SethDart Exp $
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

#ifndef _DASHBOARDPI_H_
#define _DASHBOARDPI_H_

#include "wx/wxprec.h"

#ifndef  WX_PRECOMP
#include "wx/wx.h"
#endif //precompiled headers

#include <wx/notebook.h>
#include <wx/fileconf.h>
#include <wx/listctrl.h>
#include <wx/imaglist.h>
#include <wx/spinctrl.h>
#include <wx/aui/aui.h>
#include <wx/fontpicker.h>
#include <wx/unichar.h>

#include "ocpn_plugin.h"

#include "tactics_pi.h"

#include "nmea0183/nmea0183.h"

#include "gps.h"

#include "iirfilter.h"

#include "DashboardWindowContainer.h"

#define DASHBOARD_TOOL_POSITION -1          // Request default positioning of toolbar tool

#define gps_watchdog_timeout_ticks  10


//----------------------------------------------------------------------------------------------------------
//    The PlugIn Class Definition
//----------------------------------------------------------------------------------------------------------


class dashboard_pi : public tactics_pi, wxTimer, opencpn_plugin_117
{
public:
    dashboard_pi(void *ppimgr);
    ~dashboard_pi(void);

    //    The required PlugIn Methods
    int Init(void);
    bool DeInit(void);

    void ResetAllSourcePriorities();
    void Notify();

    int GetAPIVersionMajor();
    int GetAPIVersionMinor();
    int GetPlugInVersionMajor();
    int GetPlugInVersionMinor();
    int GetPlugInVersionPatch();
    wxBitmap *GetPlugInBitmap();
    wxString GetCommonName();
    wxString GetShortDescription();
    wxString GetLongDescription();

    wxString GetStandardPath();
    // implementation of parent classes methods (w/ call-backs)
    void OnContextMenuItemCallback(int id);
    long long checkTimestamp( long long timestamp );
    void SendSentenceToAllInstruments(
        unsigned long long st, double value, wxString unit, long long timestamp=0LL);
    void pSendSentenceToAllInstruments(
        unsigned long long st, double value, wxString unit, long long timestamp=0LL);
    bool getIsUntrustedLocalTime(void) { return mUntrustedLocalTime; };
    wxDateTime getGNSSuTCDateTime(void) { return mUTCDateTime; };
    wxLongLong getGNSSreceivedAtLocalMs(void) { return mGNSSreceivedAtLocalMs; };
    void SendDataToAllPathSubscribers(
        wxString path, double value, wxString unit, long long timestamp );
    void callAllRegisteredGLRenderers(
        wxGLContext* pcontext, PlugIn_ViewPort* vp,
        wxString className = wxEmptyString );
    bool RenderOverlay(wxDC &dc, PlugIn_ViewPort *vp);
    bool RenderGLOverlay(wxGLContext *pcontext, PlugIn_ViewPort *vp);
    void OnAvgWindUpdTimer(wxTimerEvent& event);
    void OnAuiRender( wxAuiManagerEvent& event );
    wxString GetActiveRouteName() { return mRouteActivatedName; };
    wxString GetActiveRouteGUID() { return mRouteActivatedGUID; };
    wxString GetWpActivatedName() { return mWpActivatedName; };
    wxString GetWpActivatedGUID() { return mWpActivatedGUID; };
    bool GetWpArrivedIsSkipped() { return mWpArrivedIsSkipped; };
    wxString GetWpArrivedName() { return mWpArrivedName; };
    wxString GetWpArrivedGUID() { return mWpArrivedGUID; };
    wxString GetWpArrivedNextName() { return mWpArrivedNextName; };
    wxString GetWpArrivedNextGUID() { return mWpArrivedNextGUID; };
    Plugin_Active_Leg_Info* GetActiveLegInfoPtr() {
        return mActiveLegInfo; };

    //    The optional method overrides
    void SetNMEASentence(wxString &sentence);
    void SetNMEASentence(
        wxString& sentence, wxString* type=NULL, wxString* sentenceId=NULL, wxString* talker=NULL,
        wxString* src=NULL, int pgn=0, wxString* path=NULL, double value=NAN, wxString* valStr=NULL,
        long long timestamp=0LL, wxString* key=NULL);
    void SetPositionFix(PlugIn_Position_Fix &pfix);
    void SetCursorLatLon(double lat, double lon);
    int GetToolbarToolCount(void);
    void OnToolbarToolCallback(int id);
    void ShowPreferencesDialog( wxWindow* parent );
    void SetColorScheme(PI_ColorScheme cs);
    void OnPaneClose( wxAuiManagerEvent& event );
    void UpdateAuiStatus(void);
    bool SaveConfig(void);
    void PopulateContextMenu( wxMenu* menu );
    void ShowDashboard( size_t id, bool visible );
    int GetToolbarItemId(){ return m_toolbar_item_id; }
    int GetDashboardWindowShownCount();
    void SetActiveLegInfo(Plugin_Active_Leg_Info& leg_info);
    void SetPluginMessage(wxString& message_id, wxString& message_body);
    wxWindow *pGetPluginFrame(void) { return m_pluginFrame; }
    void ApplyConfig( bool init=false );
    void SetApplySaveWinRequest(void) { mApS_Watchcat = 1; }
    //#define APPLYSAVEWININIT       mApS_Watchcat=-1; // no OnAuiRender() at init (fast, but will hit later on docked items)
#define APPLYSAVEWINREQUESTED  mApS_Watchcat==1
#define APPLYSAVEWINRUNNING    mApS_Watchcat!=0
#define APPLYSAVEWINSERVED     mApS_Watchcat=0
#define APPLYSAVEWININIT       APPLYSAVEWINSERVED    // OnAuiRender() capture handled at init, slower/reliable docked items)

    int                m_nofStreamOut;
    std::mutex         m_mtxNofStreamOut;
    wxString           m_echoStreamerShow;
    int                m_nofStreamInSk;
    std::mutex         m_mtxNofStreamInSk;
    wxString           m_echoStreamerInSkShow;
    PI_ColorScheme     m_colorScheme;
    
private:
    bool LoadConfig(void);
    void LoadDashboardBasePart( wxFileConfig* pConf );
    void SaveDashboardBasePart( wxFileConfig* pConf );
    void LoadDashboardInstruments( wxFileConfig* pConf );
    void SaveDashboardInstruments( wxFileConfig* pConf );
    void LoadColorSettings( wxFileConfig* pConf );
    void LoadFontSettings( wxFileConfig* pConf );
    void SaveColorSettings( wxFileConfig* pConf );
    void SaveFontSettings( wxFileConfig* pConf );
    wxString GetCommonNameVersion(void);  
    wxString GetNameVersion(void);

    void SendSatInfoToAllInstruments(int cnt, int seq, SAT_INFO sats[4]);
    void SendUtcTimeToAllInstruments( wxDateTime value );

    bool                 m_bToggledStateVisible;
    int                  m_iPlugInRequirements;
    wxWindow            *m_pluginFrame;
    static const char   *s_common_name;
    wxTimer             *m_avgWindUpdTimer;
    wxFileConfig        *m_pconfig;
    wxAuiManager        *m_pauimgr;
    int                  m_toolbar_item_id;

    wxArrayOfDashboard   m_ArrayOfDashboardWindow;
    int                  m_show_id;
    int                  m_hide_id;

    NMEA0183            *m_NMEA0183;
    short                mPriPosition;
    short                mPriCOGSOG;
    short                mPriHeadingM;
    short                mPriHeadingT;
    short                mPriVar;
    short                mPriDateTime;
    short                mPriAWA;
    short                mPriTWA;
    short                mPriDepth;
    double               mVar;
    wxString             mRouteActivatedName;
    wxString             mRouteActivatedGUID;
    wxString             mWpActivatedName;
    wxString             mWpActivatedGUID;
    wxString             mWpArrivedName;
    wxString             mWpArrivedGUID;
    bool                 mWpArrivedIsSkipped;
    wxString             mWpArrivedNextName;
    wxString             mWpArrivedNextGUID;
    Plugin_Active_Leg_Info *mActiveLegInfo;
    // FFU
    double               mSatsInView;
    double               mHdm;
    wxDateTime           mUTCDateTime;
    wxLongLong           mGNSSreceivedAtLocalMs;
    long int             mGNSSvsLocalTimeDeltaS;
    bool                 mUntrustedLocalTime;
    bool                 mLogUntrustedLocalTimeNotify;
    int                  m_config_version;
    wxString             m_VDO_accumulator;
    int                  mSrc_Watchdog;
    int                  mHDx_Watchdog;
    int                  mHDT_Watchdog;
    int                  mGPS_Watchdog;
    int                  mVar_Watchdog;
    int                  mStW_Watchdog;
    int                  mSiK_Watchdog;
    int                  mTim_Watchdog;
    bool                 mSiK_DPT_environmentDepthBelowKeel;
    int                  mSiK_navigationGnssMethodQuality;
    int                  mApS_Watchcat;
    bool                 mBmajorVersion_warning_given;
    bool                 mBminorVersion_warning_given;

    iirfilter            mSOGFilter;
    iirfilter            mCOGFilter;

protected:
    DECLARE_EVENT_TABLE();
    void ClearActiveRouteMessages();
};
  

enum
{
    ID_DASHBOARD_WINDOW
};

enum
{
    ID_DASH_PREFS = 999,
    ID_DASH_VERTICAL,
    ID_DASH_HORIZONTAL,
    ID_DASH_UNDOCK
};

#endif
