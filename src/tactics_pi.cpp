/***************************************************************************
* $Id: tactics_pi.cpp, v1.0 2016/06/07 tomBigSpeedy Exp $
*
* Project:  OpenCPN
* Purpose:  tactics Plugin
* Author:   Thomas Rauch
*       (Inspired by original work from Jean-Eudes Onfray)
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

#include "tactics_pi.h"
using namespace std;



extern int g_iDashSpeedMax;
extern int g_iDashCOGDamp;
extern int g_iDashSpeedUnit;
extern int g_iDashSOGDamp;
extern int g_iDashDepthUnit;
extern int g_iDashDistanceUnit;  //0="Nautical miles", 1="Statute miles", 2="Kilometers", 3="Meters"
extern int g_iDashWindSpeedUnit; //0="Kts", 1="mph", 2="km/h", 3="m/s"
extern int g_iUTCOffset;

bool g_bTacticsImportChecked;
double g_dalphaDeltCoG;
double g_dalphaLaylinedDampFactor;
double g_dLeewayFactor;
double g_dfixedLeeway;
double g_dalpha_currdir;
int g_iMinLaylineWidth;
int g_iMaxLaylineWidth;
double g_dLaylineLengthonChart;
Polar* BoatPolar;
bool g_bDisplayLaylinesOnChart;
bool g_bDisplayCurrentOnChart;
wxString g_path_to_PolarFile;
wxString g_path_to_PolarLookupOutputFile;
PlugIn_Route *m_pRoute = NULL;
PlugIn_Waypoint *m_pMark = NULL;
wxString g_sMarkGUID = L"\u2191TacticsWP";
double g_dmark_lat = NAN;
double g_dmark_lon = NAN;
double g_dcur_lat = NAN;
double g_dcur_lon = NAN;
double g_dheel[6][5];
bool g_bUseHeelSensor;
bool g_bUseFixedLeeway;
bool g_bManHeelInput;
bool g_bCorrectSTWwithLeeway;  //if true STW is corrected with Leeway (in case Leeway is available)
bool g_bCorrectAWwithHeel;    //if true, AWS/AWA will be corrected with Heel-Angle
bool g_bForceTrueWindCalculation;    //if true, NMEA Data for TWS,TWA,TWD is not used, but the plugin calculated data is used
bool g_bUseSOGforTWCalc; //if true, use SOG instead of STW to calculate TWS,TWA,TWD
bool g_bShowWindbarbOnChart;
bool g_bShowPolarOnChart;
bool g_bPersistentChartPolarAnimation; // If true, continue timer based functions to animate performance on the chart
bool g_bExpPerfData01;
bool g_bExpPerfData02;
bool g_bExpPerfData03;
bool g_bExpPerfData04;
bool g_bExpPerfData05;
bool g_bNKE_TrueWindTableBug;//variable for NKE TrueWindTable-Bugfix
wxString g_sCMGSynonym, g_sVMGSynonym;
wxString g_sDataExportSeparator;
bool     g_bDataExportUTC;
bool     g_bDataExportClockticks;
AvgWind* AverageWind;

int g_iDbgRes_Polar_Status;

wxString tactics_pi::get_sCMGSynonym(void) {return g_sCMGSynonym;};
wxString tactics_pi::get_sVMGSynonym(void) {return g_sVMGSynonym;};
//---------------------------------------------------------------------------------------------------------
//
//          Tactics Performance instruments and functions for Dashboard plug-in
//
//---------------------------------------------------------------------------------------------------------

tactics_pi::tactics_pi( void )
{
    m_hostplugin = NULL;
    m_hostplugin_pconfig = NULL;
    b_tactics_dc_message_shown = false;

    // please keep the below in same order than in class definition to ease the maintenance
    mHdt = NAN;
    mStW = NAN;
    mStWnocorr = NAN;
    mSOG = NAN;
    mCOG = NAN;
    mlat = NAN;
    mlon = NAN;
    mheel = NAN;
    msensorheel = NAN;
    mLeeway = NAN;
    m_calcTWA = NAN;
    m_calcTWD = NAN;
    m_calcTWS = NAN;
    mHeelUnit = _T("");
    mAWAUnit = _T("");
    mAWSUnit = _T("");
    mAWA = NAN;
    mAWAnocorr = NAN;
    mAWS = NAN;
    mAWSnocorr = NAN;
    mTWA = NAN;
    mTWD = NAN;
    mTWS = NAN;
    m_bTrueWindAngle_available = false;
    m_bTrueWindSpeed_available = false;
    m_bTrueWindDirection_available = false;
    m_bTrueWind_available = false;
    m_bLaylinesIsVisible = false;
    m_bLaylinesIsVisibleSavedState = false;
    m_bDisplayCurrentOnChart = false;
    m_bDisplayCurrentOnChartSavedState = false;
    m_bShowWindbarbOnChart = false;
    m_bShowWindbarbOnChartSavedState = false;
    m_bShowPolarOnChart = false;
    m_bShowPolarOnChartSavedState = false;
    m_bPersistentChartPolarAnimation = false;
    m_LeewayOK = false;
    m_bNKE_TrueWindTableBug = false;
    m_VWR_AWA = 10;
    alpha_currspd = 0.2;  //smoothing constant for current speed
    alpha_CogHdt = 0.1; // smoothing constant for diff. btw. Cog & Hdt
    m_ExpSmoothCurrSpd = NAN;
    m_ExpSmoothCurrDir = NAN;
    m_ExpSmoothSog = NAN;
    m_ExpSmoothSinCurrDir = NAN;
    m_ExpSmoothCosCurrDir = NAN;
    m_ExpSmoothDiffCogHdt = NAN;
    m_LaylineDegRange = 0;
    for (int i = 0; i < COGRANGE; i++) m_COGRange[i] = NAN;
    m_ExpSmoothDegRange = 0;
    m_LaylineSmoothedCog = NAN;
    m_ExpSmoothSinCog = NAN;
    m_ExpSmoothCosCog = NAN;
    m_SmoothedpredCog = NAN;
    m_ExpSmoothSinpredCog = NAN;
    m_ExpSmoothCospredCog = NAN;
    m_ExpSmcur_tacklinedir = NAN;
    m_ExpSmtarget_tacklinedir = NAN;
    m_ExpSmoothSincur_tacklinedir = NAN;
    m_ExpSmoothCoscur_tacklinedir = NAN;
    m_ExpSmoothSintarget_tacklinedir = NAN;
    m_ExpSmoothCostarget_tacklinedir = NAN;

    // Performance variables
    mPolarTargetSpeed = NAN;
    mPredictedHdG = NAN;
    mPredictedCoG = NAN;
    mPredictedSoG = NAN;
    mPercentTargetVMGupwind = NAN;
    mPercentTargetVMGdownwind = NAN;
    tvmg.TargetAngle = 0.0;
    tvmg.TargetSpeed = 0.0;
    tcmg.TargetAngle = 0.0;
    tcmg.TargetSpeed = 0.0;
    mVMGGain = NAN;
    mCMGGain = NAN;
    mVMGoptAngle = NAN;
    mCMGoptAngle = NAN;
    mBRG = NAN;
    mBRGnocorr = NAN;
    vpoints[0].x = 0;
    vpoints[0].y = 0;
    vpoints[1] = vpoints[0];
    vpoints[2] = vpoints[0];
    tackpoints[0] = vpoints[0];
    tackpoints[1] = vpoints[0];
    tackpoints[2] = vpoints[0];
    m_CurrentDirection = NAN;

    mSinCurrDir = new DoubleExpSmooth(g_dalpha_currdir);
    mCosCurrDir = new DoubleExpSmooth(g_dalpha_currdir);
    mExpSmoothCurrSpd = new ExpSmooth(alpha_currspd);
    mExpSmoothSog = new DoubleExpSmooth(0.4);
    mExpSmSinCog = new DoubleExpSmooth(
        g_dalphaLaylinedDampFactor);//prev. ExpSmooth(...
    mExpSmCosCog = new DoubleExpSmooth(
        g_dalphaLaylinedDampFactor );//prev. ExpSmooth(...
    mExpSmSinpredCog = new DoubleExpSmooth(
        g_dalphaLaylinedDampFactor );
    mExpSmCospredCog = new DoubleExpSmooth(
        g_dalphaLaylinedDampFactor );
    mExpSmSincur_tacklinedir = new DoubleExpSmooth(
        g_dalphaLaylinedDampFactor );
    mExpSmCoscur_tacklinedir = new DoubleExpSmooth(
        g_dalphaLaylinedDampFactor );
    mExpSmSintarget_tacklinedir = new DoubleExpSmooth(
        g_dalphaLaylinedDampFactor );
    mExpSmCostarget_tacklinedir = new DoubleExpSmooth(
        g_dalphaLaylinedDampFactor );

    mExpSmDegRange = new ExpSmooth(g_dalphaDeltCoG);
    mExpSmDegRange->SetInitVal(g_iMinLaylineWidth);
    mExpSmDiffCogHdt = new ExpSmooth(alpha_CogHdt);
    mExpSmDiffCogHdt->SetInitVal(0);

    b_tactics_dc_message_shown = false;
    m_bToggledStateVisible = false;
    m_bToggledStateVisibleDefined = false;
    m_iDbgRes_TW_Calc_AWS_STC = DBGRES_AWS_STC_UNKNOWN;
    m_iDbgRes_TW_Calc_Force = DBGRES_FORCE_UNKNOWN;
    m_iDbgRes_TW_Calc_AWS = DBGRES_MVAL_UNKNOWN;
    m_iDbgRes_TW_Calc_AWA = DBGRES_MVAL_UNKNOWN;
    m_iDbgRes_TW_Calc_AWAUnit = DBGRES_MVAL_UNKNOWN;
    m_iDbgRes_TW_Calc_Hdt = DBGRES_MVAL_UNKNOWN;
    m_iDbgRes_TW_Calc_SOG = DBGRES_MVAL_UNKNOWN;
    m_iDbgRes_TW_Calc_StW = DBGRES_MVAL_UNKNOWN;
    m_iDbgRes_TW_Calc_Lau = DBGRES_EXEC_UNKNOWN;
    m_iDbgRes_TW_Calc_Exe = DBGRES_EXEC_UNKNOWN;
    g_iDbgRes_Polar_Status = DBGRES_POLAR_UNKNOWN;
}

tactics_pi::~tactics_pi( void )
{
    return;
}

int tactics_pi::TacticsInit( opencpn_plugin *hostplugin, wxFileConfig *pConf )
{
    m_hostplugin = hostplugin;
    m_hostplugin_pconfig = pConf;
    m_hostplugin_config_path = pConf->GetPath();
    m_this_config_path = m_hostplugin_config_path + _T("/Tactics");

    SetNMEASentence_Arm_BRG_Watchdog();
    SetNMEASentence_Arm_TWD_Watchdog();
    SetNMEASentence_Arm_TWS_Watchdog();
    SetNMEASentence_Arm_AWS_Watchdog();
    SetNMEASentence_Arm_VMG_Watchdog();

    (void) DeleteSingleWaypoint (g_sMarkGUID);

    this->TacticsLoadConfig();
    this->TacticsApplyConfig();

    AverageWind = new AvgWind();
    // Context menue for making marks
    m_pmenu = new wxMenu();
    // this is a dummy menu required by Windows as parent to item created
    wxMenuItem *pmi = new wxMenuItem(m_pmenu, -1, _T("Set ") + g_sMarkGUID);
    int miid = AddCanvasContextMenuItem(pmi, m_hostplugin);
    SetCanvasContextMenuItemViz(miid, true);

    return (WANTS_CURSOR_LATLON |
            WANTS_TOOLBAR_CALLBACK |
            INSTALLS_TOOLBAR_TOOL |
            WANTS_PREFERENCES |
            WANTS_CONFIG |
            WANTS_NMEA_SENTENCES |
            WANTS_NMEA_EVENTS |
            USES_AUI_MANAGER |
            WANTS_PLUGIN_MESSAGING |
            WANTS_OPENGL_OVERLAY_CALLBACK |
            WANTS_OVERLAY_CALLBACK
        );
}

void tactics_pi::OnAvgWindUpdTimer_Tactics()
{
    if ( !std::isnan(mTWD) )
        AverageWind->CalcAvgWindDir(mTWD);
}

void tactics_pi::SetToggledStateVisible( bool isvisible )
{
    if ( !m_bToggledStateVisibleDefined ) {
        m_bToggledStateVisibleDefined = true;
        m_bToggledStateVisible = isvisible;
        if ( !isvisible ) {
            if ( !m_bPersistentChartPolarAnimation ) {
                m_bLaylinesIsVisibleSavedState = m_bLaylinesIsVisible;
                m_bLaylinesIsVisible = false;
                m_bDisplayCurrentOnChartSavedState = m_bDisplayCurrentOnChart;
                m_bDisplayCurrentOnChart = false;
                m_bShowWindbarbOnChartSavedState = m_bShowWindbarbOnChart;
                m_bShowWindbarbOnChart = false;
                m_bShowPolarOnChartSavedState = m_bShowPolarOnChart;
                m_bShowPolarOnChart = false;
            } // then don't show chart animation when not visible
        } // then started as not visible
    } // then this is the first ever state definition
    if ( m_bToggledStateVisible == isvisible )
        return;
    m_bToggledStateVisible = isvisible;
    if ( isvisible ) {
        if ( !m_bPersistentChartPolarAnimation ) {
            m_bLaylinesIsVisible = m_bLaylinesIsVisibleSavedState;
            m_bDisplayCurrentOnChart = m_bDisplayCurrentOnChartSavedState;
            m_bShowWindbarbOnChart = m_bShowWindbarbOnChartSavedState;
            m_bShowPolarOnChart = m_bShowPolarOnChartSavedState;
        } // if non persistent animation was disabled
    } // actions from invisible to visible
    else {
        if ( !m_bPersistentChartPolarAnimation ) {
            m_bLaylinesIsVisibleSavedState = m_bLaylinesIsVisible;
            m_bLaylinesIsVisible = false;
            m_bDisplayCurrentOnChartSavedState = m_bDisplayCurrentOnChart;
            m_bDisplayCurrentOnChart = false;
            m_bShowWindbarbOnChartSavedState = m_bShowWindbarbOnChart;
            m_bShowWindbarbOnChart = false;
            m_bShowPolarOnChartSavedState = m_bShowPolarOnChart;
            m_bShowPolarOnChart = false;
        } // if non persistent animation is required
    } // else actions from visible to invisible
    return;
}

bool tactics_pi::TacticsDeInit()
{
	this->TacticsSaveConfig();

    if (m_pRoute){
		m_pRoute->pWaypointList->DeleteContents(true);
		DeletePlugInRoute(m_pRoute->m_GUID);
	}
    (void) DeleteSingleWaypoint (g_sMarkGUID);

	return true;
}
void tactics_pi::TacticsNotify()
{
    mBRG_Watchdog--;
    if (mBRG_Watchdog <= 0) {
      SendSentenceToAllInstruments(OCPN_DBP_STC_BRG, NAN, _T("\u00B0"));
      SetNMEASentence_Arm_BRG_Watchdog();
    }
    mTWD_Watchdog--;
    if (mTWD_Watchdog <= 0) {
      std::unique_lock<std::mutex> lck( mtxTWD,std::defer_lock );
      if ( lck.try_lock() ) {
          mTWD = NAN;
          mTWA = NAN;
          lck.unlock();
          SendSentenceToAllInstruments(OCPN_DBP_STC_TWD, NAN, _T("\u00B0"));
          SendSentenceToAllInstruments(OCPN_DBP_STC_TWA, NAN, _T("\u00B0"));
          SetNMEASentence_Arm_TWD_Watchdog();
      }// then not on-going calculations using mTWD or mTWA
    }
    mTWS_Watchdog--;
    if (mTWS_Watchdog <= 0) {
        std::unique_lock<std::mutex> lck( mtxTWS,std::defer_lock );
        if ( lck.try_lock() ) {
            mTWS = NAN;
            lck.unlock();
            SendSentenceToAllInstruments(OCPN_DBP_STC_TWS, NAN, _T(""));
            SetNMEASentence_Arm_TWS_Watchdog();
        } // then no on-going calculations using mTWS
    }
    mAWS_Watchdog--;
    if (mAWS_Watchdog <= 0) {
        std::unique_lock<std::mutex> lck( mtxAWS,std::defer_lock );
        if ( lck.try_lock() ) {
            mAWS = NAN;
            lck.unlock();
            SendSentenceToAllInstruments(OCPN_DBP_STC_AWS, NAN, _T(""));
            SetNMEASentence_Arm_AWS_Watchdog();
        } // then no ongoig calculations using mAWS
    }

    mVMG_Watchdog--;
    if (mVMG_Watchdog <= 0) {
      SendSentenceToAllInstruments(OCPN_DBP_STC_VMG, NAN, _T(""));
      SetNMEASentence_Arm_VMG_Watchdog();
    }

    this->ExportPerformanceData();

}

bool tactics_pi::TacticsLoadConfig()
{
    wxFileConfig *pConf = (wxFileConfig *) m_hostplugin_pconfig;
	if (!pConf)
        return false;

    this->m_this_config_path =
        this->m_hostplugin_config_path + _T("/Tactics");
    pConf->SetPath( this->m_this_config_path );
    if (!this->LoadConfig_CheckTacticsPlugin( pConf )) {
        /* If not imported from the tactics_pi
         standalone Tactics Plugin, we must have our own
         settings in ini-file by now: */
        pConf->SetPath( this->m_this_config_path );
        bool bUserDecision = g_bTacticsImportChecked;
        this->LoadTacticsPluginBasePart( pConf );
        pConf->SetPath( this->m_this_config_path + _T("/Performance"));
        this->LoadTacticsPluginPerformancePart( pConf );
        g_bTacticsImportChecked = bUserDecision;
    } // then load from this plugin's sub-group
    else {
        bool bUserDecision = g_bTacticsImportChecked;
        this->ImportStandaloneTacticsSettings ( pConf );
        g_bTacticsImportChecked = bUserDecision;
    } // else load from the tactics_pi plugin's group
    /* Unlike the tactics_pi plugin, Dashboard  not absolutely require
       to have polar-file - it may be that the user is not
       interested in performance part. Yet. We can ask that later. */
    BoatPolar = new Polar(this);
    if ( g_path_to_PolarFile != _T("NULL") ) {
        BoatPolar->loadPolar( g_path_to_PolarFile );
        if ( g_path_to_PolarLookupOutputFile != _T("NULL") ) {
            BoatPolar->saveLookupTable( g_path_to_PolarLookupOutputFile );
        }
    }
    else {
        BoatPolar->loadPolar(_T("NULL"));
    }

    return true;
}
/*
 Check and swap to original TacticsPlugin group if the user wish to import from there, return false if no import
*/
bool tactics_pi::LoadConfig_CheckTacticsPlugin( wxFileConfig *pConf )
{
    bool bCheckIt = false;
    pConf->SetPath( this->m_this_config_path );
   if (!pConf->Exists(_T("TacticsImportChecked"))) {
        g_bTacticsImportChecked = false;
        bCheckIt = true;
    } // then this must be the first run...
    else {
        pConf->Read(_T("TacticsImportChecked"), &g_bTacticsImportChecked, false);
        if (!g_bTacticsImportChecked)
            bCheckIt = true;
    } /* else check is done only once, normally, unless
         TacticsImportChecked is altered manually to false */
    if (!bCheckIt)
        return false;
    if (!this->StandaloneTacticsSettingsExists ( pConf ))
        return false;
    wxString message(
        _("Import existing Tactics plugin settings into Dashboard's integrated Tactics settings? (Cancel=later)"));
    wxMessageDialog *dlg = new wxMessageDialog(
        GetOCPNCanvasWindow(), message, _T("Dashboard configuration choice"), wxYES_NO|wxCANCEL);
    int choice = dlg->ShowModal();
    if ( choice == wxID_YES ) {

        g_bTacticsImportChecked = true;
        return true;
    } // then import
    else {
        if (choice == wxID_NO ) {
            g_bTacticsImportChecked = true;
            return false;
        } // then do not import, attempt to import from local
        else {
            g_bTacticsImportChecked = false;
            return false;
        } // else not sure (cancel): not now, but will ask again
    } // else no or cancel
}
void tactics_pi::ImportStandaloneTacticsSettings ( wxFileConfig *pConf )
{
    // Here we suppose that both tactics_pi and this Dashboard implementation are identical
    pConf->SetPath( "/PlugIns/Tactics" );
    this->LoadTacticsPluginBasePart( pConf );
    pConf->SetPath( "/PlugIns/Tactics/Performance" );
    this->LoadTacticsPluginPerformancePart( pConf );
}

bool tactics_pi::StandaloneTacticsSettingsExists ( wxFileConfig *pConf )
{
    pConf->SetPath( "/PlugIns/Tactics/Performance" );
    if (pConf->Exists(_T("PolarFile")))
        return true;
    return false;
}

/*
  Load tactics_pi settings from the Tactics base group,
  underneath the group given in pConf object (you must set it).
*/
void tactics_pi::LoadTacticsPluginBasePart ( wxFileConfig *pConf )
{
    pConf->Read(_T("CurrentDampingFactor"), &g_dalpha_currdir, 0.008);
    pConf->Read(_T("LaylineDampingFactor"), &g_dalphaLaylinedDampFactor, 0.15);
    pConf->Read(_T("LaylineLenghtonChart"), &g_dLaylineLengthonChart, 10.0);
    pConf->Read(_T("MinLaylineWidth"), &g_iMinLaylineWidth, 2);
    pConf->Read(_T("MaxLaylineWidth"), &g_iMaxLaylineWidth, 30);
    pConf->Read(_T("LaylineWidthDampingFactor"), &g_dalphaDeltCoG, 0.25);
    pConf->Read(_T("ShowLaylinesOnChart"), &g_bDisplayLaylinesOnChart, false);
    m_bLaylinesIsVisible = g_bDisplayLaylinesOnChart;
    pConf->Read(_T("ShowCurrentOnChart"), &g_bDisplayCurrentOnChart, false);
    m_bDisplayCurrentOnChart = g_bDisplayCurrentOnChart;
    pConf->Read(_T("CMGSynonym"), &g_sCMGSynonym, _T("CMG"));
    pConf->Read(_T("VMGSynonym"), &g_sVMGSynonym, _T("VMG"));
    pConf->Read(_T("DataExportSeparator"), &g_sDataExportSeparator, _(";"));
    pConf->Read(_T("TacticsImportChecked"), &g_bTacticsImportChecked, false);
    pConf->Read(_T("DataExportUTC-ISO8601"), &g_bDataExportUTC, 0);
    pConf->Read(_T("DataExportClockticks"), &g_bDataExportClockticks,0);
}
/*
  Import tactics_pi settings from the Tactics Performance Group,
  underneath the group given in pConf object (you must set it)
*/
void tactics_pi::LoadTacticsPluginPerformancePart ( wxFileConfig *pConf )
{
    pConf->Read(_T("PolarFile"), &g_path_to_PolarFile, _T("NULL"));
    pConf->Read(_T("PolarLookupTableOutputFile"), &g_path_to_PolarLookupOutputFile, _T("NULL"));
    pConf->Read(_T("BoatLeewayFactor"), &g_dLeewayFactor, 10);
    pConf->Read(_T("fixedLeeway"), &g_dfixedLeeway, 30);
    pConf->Read(_T("UseHeelSensor"), &g_bUseHeelSensor, true);
    pConf->Read(_T("UseFixedLeeway"), &g_bUseFixedLeeway, false);
    pConf->Read(_T("UseManHeelInput"), &g_bManHeelInput, false);
    pConf->Read(_T("Heel_5kn_45Degree"), &g_dheel[1][1], 5);
    pConf->Read(_T("Heel_5kn_90Degree"), &g_dheel[1][2], 8);
    pConf->Read(_T("Heel_5kn_135Degree"), &g_dheel[1][3], 5);
    pConf->Read(_T("Heel_10kn_45Degree"), &g_dheel[2][1], 8);
    pConf->Read(_T("Heel_10kn_90Degree"), &g_dheel[2][2], 10);
    pConf->Read(_T("Heel_10kn_135Degree"), &g_dheel[2][3], 11);
    pConf->Read(_T("Heel_15kn_45Degree"), &g_dheel[3][1], 25);
    pConf->Read(_T("Heel_15kn_90Degree"), &g_dheel[3][2], 20);
    pConf->Read(_T("Heel_15kn_135Degree"), &g_dheel[3][3], 13);
    pConf->Read(_T("Heel_20kn_45Degree"), &g_dheel[4][1], 20);
    pConf->Read(_T("Heel_20kn_90Degree"), &g_dheel[4][2], 16);
    pConf->Read(_T("Heel_20kn_135Degree"), &g_dheel[4][3], 15);
    pConf->Read(_T("Heel_25kn_45Degree"), &g_dheel[5][1], 25);
    pConf->Read(_T("Heel_25kn_90Degree"), &g_dheel[5][2], 20);
    pConf->Read(_T("Heel_25kn_135Degree"), &g_dheel[5][3], 20);
    pConf->Read(_T("UseManHeelInput"), &g_bManHeelInput, false);
    pConf->Read(_T("CorrectSTWwithLeeway"), &g_bCorrectSTWwithLeeway, false);  //if true, STW is corrected with Leeway (in case Leeway is available)
    pConf->Read(_T("CorrectAWwithHeel"), &g_bCorrectAWwithHeel, false);    //if true, AWS/AWA are corrected with Heel-Angle
    pConf->Read(_T("ForceTrueWindCalculation"), &g_bForceTrueWindCalculation, false);    //if true, NMEA Data for TWS,TWA,TWD is not used, but the plugin calculated data is used
    pConf->Read(_T("ShowWindbarbOnChart"), &g_bShowWindbarbOnChart, false);
    m_bShowWindbarbOnChart = g_bShowWindbarbOnChart;
    pConf->Read(_T("ShowPolarOnChart"), &g_bShowPolarOnChart, false);
    m_bShowPolarOnChart = g_bShowPolarOnChart;
    pConf->Read(_T("PersistentChartPolarAnimation"), &g_bPersistentChartPolarAnimation, true);
    m_bPersistentChartPolarAnimation = g_bPersistentChartPolarAnimation;
    pConf->Read(_T("UseSOGforTWCalc"), &g_bUseSOGforTWCalc, true);
    pConf->Read(_T("ExpPolarSpeed"), &g_bExpPerfData01, false);
    pConf->Read(_T("ExpCourseOtherTack"), &g_bExpPerfData02, false);
    pConf->Read(_T("ExpTargetVMG"), &g_bExpPerfData03, false);
    pConf->Read(_T("ExpVMG_CMG_Diff_Gain"), &g_bExpPerfData04, false);
    pConf->Read(_T("ExpCurrent"), &g_bExpPerfData05, false);
    pConf->Read(_T("NKE_TrueWindTableBug"), &g_bNKE_TrueWindTableBug, false);
    m_bNKE_TrueWindTableBug = g_bNKE_TrueWindTableBug;
}
void tactics_pi::TacticsApplyConfig(void)
{

    if (!(BoatPolar == NULL)) {
        if (g_path_to_PolarFile != _T("NULL")) {
            BoatPolar->loadPolar(g_path_to_PolarFile);
            if ( g_path_to_PolarLookupOutputFile != _T("NULL") ) {
                BoatPolar->saveLookupTable( g_path_to_PolarLookupOutputFile );
            }
        }
        else {
            BoatPolar->loadPolar(_T("NULL"));
        }
    }
    m_bDisplayCurrentOnChart = g_bDisplayCurrentOnChart;
    m_bShowWindbarbOnChart = g_bShowWindbarbOnChart;
    m_bShowPolarOnChart = g_bShowPolarOnChart;
    m_bPersistentChartPolarAnimation = g_bPersistentChartPolarAnimation;
    return;
}

bool tactics_pi::TacticsSaveConfig()
{
    wxFileConfig *pConf = (wxFileConfig *) m_hostplugin_pconfig;
    if (!pConf)
        return false;
    pConf->SetPath( this->m_this_config_path );
    SaveTacticsPluginBasePart ( pConf );
    pConf->SetPath( this->m_this_config_path + _T("/Performance"));
    SaveTacticsPluginPerformancePart ( pConf );
    return true;
}
/*
  Save tactics_pi settings into the Tactics base group,
  underneath the group given in pConf object (you have to set it).
*/
void tactics_pi::SaveTacticsPluginBasePart ( wxFileConfig *pConf )
{
    pConf->Write(_T("CurrentDampingFactor"), g_dalpha_currdir);
    pConf->Write(_T("LaylineDampingFactor"), g_dalphaLaylinedDampFactor);
    pConf->Write(_T("LaylineLenghtonChart"), g_dLaylineLengthonChart);
    pConf->Write(_T("MinLaylineWidth"), g_iMinLaylineWidth);
    pConf->Write(_T("MaxLaylineWidth"), g_iMaxLaylineWidth);
    pConf->Write(_T("LaylineWidthDampingFactor"), g_dalphaDeltCoG);
    pConf->Write(_T("ShowLaylinesOnChart"), g_bDisplayLaylinesOnChart);
    pConf->Write(_T("ShowCurrentOnChart"), g_bDisplayCurrentOnChart);
    pConf->Write(_T("CMGSynonym"), g_sCMGSynonym);
    pConf->Write(_T("VMGSynonym"), g_sVMGSynonym);
    pConf->Write(_T("DataExportSeparator"), g_sDataExportSeparator);
    pConf->Write(_T("DataExportUTC-ISO8601"), g_bDataExportUTC);
    pConf->Write(_T("DataExportClockticks"), g_bDataExportClockticks);
    pConf->Write(_T("TacticsImportChecked"), g_bTacticsImportChecked);
}
/*
  Save tactics_pi settings into the Tactics Performance Group,
  underneath the group given in pConf object (you have to set it).
*/
void tactics_pi::SaveTacticsPluginPerformancePart ( wxFileConfig *pConf )
{
    pConf->Write(_T("PolarFile"), g_path_to_PolarFile);
    pConf->Write(_T("BoatLeewayFactor"), g_dLeewayFactor);
    pConf->Write(_T("fixedLeeway"), g_dfixedLeeway);
    pConf->Write(_T("UseHeelSensor"), g_bUseHeelSensor);
    pConf->Write(_T("UseFixedLeeway"), g_bUseFixedLeeway);
    pConf->Write(_T("UseManHeelInput"), g_bManHeelInput);
    pConf->Write(_T("CorrectSTWwithLeeway"), g_bCorrectSTWwithLeeway);
    pConf->Write(_T("CorrectAWwithHeel"), g_bCorrectAWwithHeel);
    pConf->Write(_T("ForceTrueWindCalculation"), g_bForceTrueWindCalculation);
    pConf->Write(_T("UseSOGforTWCalc"), g_bUseSOGforTWCalc);
    pConf->Write(_T("ShowWindbarbOnChart"), g_bShowWindbarbOnChart);
    pConf->Write(_T("ShowPolarOnChart"), g_bShowPolarOnChart);
    pConf->Write(_T("PersistentChartPolarAnimation"), g_bPersistentChartPolarAnimation);
    pConf->Write(_T("Heel_5kn_45Degree"), g_dheel[1][1]);
    pConf->Write(_T("Heel_5kn_90Degree"), g_dheel[1][2]);
    pConf->Write(_T("Heel_5kn_135Degree"), g_dheel[1][3]);
    pConf->Write(_T("Heel_10kn_45Degree"), g_dheel[2][1]);
    pConf->Write(_T("Heel_10kn_90Degree"), g_dheel[2][2]);
    pConf->Write(_T("Heel_10kn_135Degree"), g_dheel[2][3]);
    pConf->Write(_T("Heel_15kn_45Degree"), g_dheel[3][1]);
    pConf->Write(_T("Heel_15kn_90Degree"), g_dheel[3][2]);
    pConf->Write(_T("Heel_15kn_135Degree"), g_dheel[3][3]);
    pConf->Write(_T("Heel_20kn_45Degree"), g_dheel[4][1]);
    pConf->Write(_T("Heel_20kn_90Degree"), g_dheel[4][2]);
    pConf->Write(_T("Heel_20kn_135Degree"), g_dheel[4][3]);
    pConf->Write(_T("Heel_25kn_45Degree"), g_dheel[5][1]);
    pConf->Write(_T("Heel_25kn_90Degree"), g_dheel[5][2]);
    pConf->Write(_T("Heel_25kn_135Degree"), g_dheel[5][3]);
    pConf->Write(_T("ExpPolarSpeed"), g_bExpPerfData01);
    pConf->Write(_T("ExpCourseOtherTack"), g_bExpPerfData02);
    pConf->Write(_T("ExpTargetVMG"), g_bExpPerfData03);
    pConf->Write(_T("ExpVMG_CMG_Diff_Gain"), g_bExpPerfData04);
    pConf->Write(_T("ExpCurrent"), g_bExpPerfData05);
    pConf->Write(_T("NKE_TrueWindTableBug"), g_bNKE_TrueWindTableBug);
}


/*********************************************************************
Taken from cutil
*********************************************************************/
inline int myCCW(wxRealPoint p0, wxRealPoint p1, wxRealPoint p2) {
	double dx1, dx2;
	double dy1, dy2;

	dx1 = p1.x - p0.x; dx2 = p2.x - p0.x;
	dy1 = p1.y - p0.y; dy2 = p2.y - p0.y;

	/* This is basically a slope comparison: we don't do divisions because

	* of divide by zero possibilities with pure horizontal and pure
	* vertical lines.
	*/
	return ((dx1 * dy2 > dy1 * dx2) ? 1 : -1);

}
/*********************************************************************
returns true if we have a line intersection.
Taken from cutil, but with double variables
**********************************************************************/
inline bool IsLineIntersect(wxRealPoint p1, wxRealPoint p2, wxRealPoint p3, wxRealPoint p4)
{
	return (((myCCW(p1, p2, p3) * myCCW(p1, p2, p4)) <= 0)
		&& ((myCCW(p3, p4, p1) * myCCW(p3, p4, p2) <= 0)));

}
/*********************************************************************
calculate Line intersection between 2 lines, each described by 2 points
return lat/lon of intersection point
basic calculation:
int p1[] = { -4,  5, 1 };
int p2[] = { -2, -5, 1 };
int p3[] = { -6,  2, 1 };
int p4[] = {  5,  4, 1 };
int l1[3], l2[3], s[3];
double sch[2];
l1[0] = p1[1] * p2[2] - p1[2] * p2[1];
l1[1] = p1[2] * p2[0] - p1[0] * p2[2];
l1[2] = p1[0] * p2[1] - p1[1] * p2[0];
l2[0] = p3[1] * p4[2] - p3[2] * p4[1];
l2[1] = p3[2] * p4[0] - p3[0] * p4[2];
l2[2] = p3[0] * p4[1] - p3[1] * p4[0];
s[0] = l1[1] * l2[2] - l1[2] * l2[1];
s[1] = l1[2] * l2[0] - l1[0] * l2[2];
s[2] = l1[0] * l2[1] - l1[1] * l2[0];
sch[0] = (double)s[0] / (double)s[2];
sch[1] = (double)s[1] / (double)s[2];
**********************************************************************/
wxRealPoint GetLineIntersection(wxRealPoint line1point1, wxRealPoint line1point2, wxRealPoint line2point1, wxRealPoint line2point2)
{
	wxRealPoint intersect;
	intersect.x = -999.;
	intersect.y = -999.;
	if (IsLineIntersect(line1point1, line1point2, line2point1, line2point2)){
		double line1[3], line2[3], s[3];
		line1[0] = line1point1.y * 1. - 1. * line1point2.y;
		line1[1] = 1. * line1point2.x - line1point1.x * 1.;
		line1[2] = line1point1.x * line1point2.y - line1point1.y * line1point2.x;
		line2[0] = line2point1.y * 1. - 1. * line2point2.y;
		line2[1] = 1. * line2point2.x - line2point1.x * 1.;
		line2[2] = line2point1.x * line2point2.y - line2point1.y * line2point2.x;
		s[0] = line1[1] * line2[2] - line1[2] * line2[1];
		s[1] = line1[2] * line2[0] - line1[0] * line2[2];
		s[2] = line1[0] * line2[1] - line1[1] * line2[0];
		intersect.x = s[0] / s[2];
		intersect.y = s[1] / s[2];
	}
	return intersect;
}
/**********************************************************************
Function calculates the time to sail for a given distance, TWA and TWS,
based on the polar data
returns NaN if no polar data or if it is not valid
***********************************************************************/
double CalcPolarTimeToMark(double distance, double twa, double tws)
{
    if ( !BoatPolar->isValid() ) {
        if ( g_iDbgRes_Polar_Status != DBGRES_POLAR_INVALID ) {
            wxLogMessage ("dashboard_tactics_pi: >>> Missing or invalid Polar file: no Performance data, Laylines, Polar graphs available. <<<");
            g_iDbgRes_Polar_Status = DBGRES_POLAR_INVALID;
        } // then debug print
        return NAN;
    } // then no valid polar
    g_iDbgRes_Polar_Status = DBGRES_POLAR_VALID;
	double pspd = BoatPolar->GetPolarSpeed(twa, tws);
	return distance / pspd;
}
/**********************************************************************
Function returns the (smaller) TWA of a given TWD and Course.
Used for Target-CMG calculation.
It covers the 359 - 0 degree problem
e.g. : TWD = 350, ctm = 10; the TWA is returned as 20 degrees
(and not 340 if we'd do a simple TWD - ctm)
***********************************************************************/
double getMarkTWA(double twd, double ctm)
{
	double val, twa;
	if (twd > 180)
	{
		val = twd - 180;
		if (ctm < val)
			twa = 360 - twd + ctm;
		else
			twa = twd > ctm ? twd - ctm : ctm - twd;
	}
	else
	{
		val = twd + 180;
		if (ctm > val)
			twa = 360 - ctm + twd;
		else
			twa = twd > ctm ? twd - ctm : ctm - twd;
	}
	return twa;
}
/**********************************************************************
Function returns the (smaller) degree range of 2 angular values
on the compass rose (without sign)
It covers the 359 - 0 degree problem
e.g. : max = 350, min = 10; the rage is returned as 20 degrees
(and not 340 if we'd do a simple max - min)
**********************************************************************/
double getDegRange(double max, double min)
{
	double val, range;
	if (max > 180)
	{
		val = max - 180;
		if (min < val)
			range = 360 - max + min;
		else
			range = max > min ? max - min : min - max;
	}
	else
	{
		val = max + 180;
		if (min > val)
			range = 360 - min + max;
		else
			range = max > min ? max - min : min - max;
	}
	return range;
}
/**********************************************************************
Function returns the (smaller) signed degree range of 2 angular values
on the compass rose (clockwise is +)
It covers the 359 - 0 degree problem
e.g. : fromAngle = 350, toAngle = 10; the range is returned as +20 degrees
(and not 340 if we'd do a simple fromAngle - toAngle)
***********************************************************************/
double getSignedDegRange(double fromAngle, double toAngle)
{
	double val, range;
	if (fromAngle > 180)
	{
		val = fromAngle - 180;
		if (toAngle < val)
			range = 360 - fromAngle + toAngle;
		else
			range = toAngle - fromAngle;
	}
	else
	{
		val = fromAngle + 180;
		if (toAngle > val)
			range = -(360 - toAngle + fromAngle);
		else
			range = toAngle - fromAngle;
	}
	return range;
}


/**********************************************************************
Draw the OpenGL overlay
Called by Plugin Manager on main system process cycle
**********************************************************************/
bool tactics_pi::TacticsRenderGLOverlay(wxGLContext *pcontext, PlugIn_ViewPort *vp)
{
	b_tactics_dc_message_shown = false; // show message box if RenderOverlay() is called again
#ifndef __linux__
    if ( !pcontext->IsOK() )
        return false;
#endif // __linux__
	if (m_bLaylinesIsVisible || m_bDisplayCurrentOnChart || m_bShowWindbarbOnChart || m_bShowPolarOnChart){
		glPushAttrib(GL_COLOR_BUFFER_BIT | GL_LINE_BIT | GL_ENABLE_BIT | GL_POLYGON_BIT | GL_HINT_BIT);
		glEnable(GL_LINE_SMOOTH);
		glEnable(GL_BLEND);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		glHint(GL_LINE_SMOOTH_HINT, GL_NICEST);
		glPushMatrix();
		this->DoRenderLaylineGLOverlay(pcontext, vp);
		this->DoRenderCurrentGLOverlay(pcontext, vp);
		glPopMatrix();
		glPopAttrib();
	}
	return true;
}

bool tactics_pi::TacticsRenderOverlay(wxDC &dc, PlugIn_ViewPort *vp)
{
	if (b_tactics_dc_message_shown == false) {
        b_tactics_dc_message_shown = true;

		wxString message(_("You have to turn on OpenGL to use chart overlay "));
		wxMessageDialog dlg(GetOCPNCanvasWindow(), message, _T("dashboard_tactics_pi message"), wxOK);
		dlg.ShowModal();
	}
	return false;
}

/********************************************************************
Draw the OpenGL Layline overlay
*********************************************************************/
void tactics_pi::DoRenderCurrentGLOverlay(wxGLContext *pcontext, PlugIn_ViewPort *vp)
{
    if (m_bDisplayCurrentOnChart && !std::isnan(mlat) && !std::isnan(mlon) && !std::isnan(m_CurrentDirection)) {
        //draw the current on the chart here
        /*
         *           0
         *          /\
         *         /  \
         *        /    \
         *     6 /_  X _\ 1
         *        5|  |2
         *         |__|
         *        4    3
         */
        wxPoint boat;
        GetCanvasPixLL(vp, &boat, mlat, mlon);

        double m_radius;
        double zoomradius = 160.0 + vp->view_scale_ppm * 100.0;
        (zoomradius > 400.0 ? m_radius = 400.0 : m_radius = zoomradius);
        wxRealPoint currpoints[7];
        double currval = m_CurrentDirection;
        double rotate = vp->rotation;
        double currvalue = ((currval - 90.) * M_PI / 180) + rotate;

        currpoints[0].x = boat.x + (m_radius * .40 * cos(currvalue));
        currpoints[0].y = boat.y + (m_radius * .40 * sin(currvalue));
        currpoints[1].x = boat.x + (m_radius * .18 * cos(currvalue + 1.5));
        currpoints[1].y = boat.y + (m_radius * .18 * sin(currvalue + 1.5));
        currpoints[2].x = boat.x + (m_radius * .10 * cos(currvalue + 1.5));
        currpoints[2].y = boat.y + (m_radius * .10 * sin(currvalue + 1.5));

        currpoints[3].x = boat.x + (m_radius * .3 * cos(currvalue + 2.8));
        currpoints[3].y = boat.y + (m_radius * .3 * sin(currvalue + 2.8));
        currpoints[4].x = boat.x + (m_radius * .3 * cos(currvalue - 2.8));
        currpoints[4].y = boat.y + (m_radius * .3 * sin(currvalue - 2.8));

        currpoints[5].x = boat.x + (m_radius * .10 * cos(currvalue - 1.5));
        currpoints[5].y = boat.y + (m_radius * .10 * sin(currvalue - 1.5));
        currpoints[6].x = boat.x + (m_radius * .18 * cos(currvalue - 1.5));
        currpoints[6].y = boat.y + (m_radius * .18 * sin(currvalue - 1.5));
        //below 0.2 knots the current generally gets inaccurate, as the error level gets too high.
        // as a hint, fade the current transparency below 0.25 knots...
        //        int curr_trans = 164;
        //        if (m_ExpSmoothCurrSpd <= 0.20) {
        //          fading : 0.2 * 5 = 1 --> we set full transp. >=0.2
        //          curr_trans = m_ExpSmoothCurrSpd * 5 * curr_trans;
        //          if (curr_trans < 50) curr_trans = 50;  //lower limit
        //        }
        //        GLubyte red(7), green(107), blue(183), alpha(curr_trans);
        //        glColor4ub(7, 107, 183, curr_trans);                  // red, green, blue,  alpha

        //GLubyte red(7), green(107), blue(183), alpha(164);
        glColor4ub(7, 107, 183, 164); // red, green, blue,  alpha
        //        glLineWidth(2);
        //      glBegin(GL_POLYGON | GL_LINES);
        glBegin(GL_POLYGON);
        glVertex2d(currpoints[0].x, currpoints[0].y);
        glVertex2d(currpoints[1].x, currpoints[1].y);
        glVertex2d(currpoints[2].x, currpoints[2].y);
        glVertex2d(currpoints[3].x, currpoints[3].y);
        glVertex2d(currpoints[4].x, currpoints[4].y);
        glVertex2d(currpoints[5].x, currpoints[5].y);
        glVertex2d(currpoints[6].x, currpoints[6].y);
        glEnd();
    }
}

/*********************************************************************
Draw the OpenGL Layline overlay
**********************************************************************/
void tactics_pi::DoRenderLaylineGLOverlay(wxGLContext *pcontext, PlugIn_ViewPort *vp)
{
	wxPoint  mark_center;
	wxPoint boat;
	if (!std::isnan(mlat) && !std::isnan(mlon)) {
		GetCanvasPixLL(vp, &vpoints[0], mlat, mlon);
		boat = vpoints[0];

        /*********************************************************************
		Draw wind barb on boat position
***********************************************************************/
        //mTWD=NAN caught in subroutines
		DrawWindBarb(boat, vp);
		DrawPolar(vp, boat, mTWD);
	}
	if (!GetSingleWaypoint(g_sMarkGUID, m_pMark))
        m_pMark = NULL;
	if (m_pMark){
		/*********************************************************************
		Draw wind barb on mark position
        **********************************************************************/
		GetCanvasPixLL(vp, &mark_center, m_pMark->m_lat, m_pMark->m_lon);
		DrawWindBarb(mark_center, vp);
		/*********************************************************************
		Draw direct line from boat to mark as soon as mark is dropped. Reduces problems to find it ...
**********************************************************************/
		glColor4ub(255, 128, 0, 168); //orange
		glLineWidth(2);
		glBegin(GL_LINES);
		glVertex2d(boat.x, boat.y);
		glVertex2d(mark_center.x, mark_center.y);
		glEnd();

	}

	if ( !m_bLaylinesIsVisible )
        return;
    
    std::unique_lock<std::mutex> lckmTWDmTWA( mtxTWD ); // shared mutex mTWD and mTWA
    std::unique_lock<std::mutex> lckmTWS( mtxTWS );
    std::unique_lock<std::mutex> lckmHdt( mtxHdt );
    std::unique_lock<std::mutex> lckmAWA( mtxAWS,std::defer_lock ); // shares mutex with AWS,mAWAUnit
    if (!std::isnan(mlat) && !std::isnan(mlon) && !std::isnan(mCOG) && !std::isnan(mHdt) &&
        !std::isnan(mStW) && !std::isnan(mTWS) && !std::isnan(mTWA)) {
        if (std::isnan(m_LaylineSmoothedCog)) m_LaylineSmoothedCog = mCOG;
        if (std::isnan(mLeeway)) mLeeway = 0.0;
        /**********************************************************************
			Draw the boat laylines, independent from the g_sMarkGUID
			The first (foreward) layline is on the COG pointer
        ***********************************************************************/
        wxString curTack = mAWAUnit;
        wxString targetTack = _T("");
        //it shows '<deg>L'= wind from left = port tack or '<deg>R'=wind from right = starboard tack
        //we're on port tack, so vertical layline is red
        if (curTack == _T("\u00B0lr")) {
            //GLubyte red(204), green(41), blue(41), alpha(128);
            glColor4ub(204, 41, 41, 128);                 	// red, green, blue,  alpha
            targetTack = _T("R");
        }
        else if (curTack == _T("\u00B0rl"))  {// we're on starboard tack, so vertical layline is green
            //GLubyte red(0), green(200), blue(0), alpha(128);
            glColor4ub(0, 200, 0, 128);                 	// red, green, blue,  alpha
            targetTack = _T("L");
        }
        double tmplat1, tmplon1, tmplat2, tmplon2;
        PositionBearingDistanceMercator_Plugin(mlat, mlon, m_LaylineSmoothedCog - m_ExpSmoothDegRange / 2., g_dLaylineLengthonChart, &tmplat1, &tmplon1);
        GetCanvasPixLL(vp, &vpoints[1], tmplat1, tmplon1);
        PositionBearingDistanceMercator_Plugin(mlat, mlon, m_LaylineSmoothedCog + m_ExpSmoothDegRange / 2., g_dLaylineLengthonChart, &tmplat2, &tmplon2);
        GetCanvasPixLL(vp, &vpoints[2], tmplat2, tmplon2);
        glBegin(GL_TRIANGLES);
        glVertex2d(vpoints[0].x, vpoints[0].y);
        glVertex2d(vpoints[1].x, vpoints[1].y);
        glVertex2d(vpoints[2].x, vpoints[2].y);
        glEnd();
        /*********************************************************************
			Calculate and draw  the second boat layline (for other tack)
			---------------------------------------------------------
			Approach : we're drawing the first layline on COG, but TWA is based on boat heading (Hdt).
			To calculate the layline of the other tack, sum up
			diff_btw_Cog_and_Hdt (now we're on Hdt)
			+ 2 x TWA
			+ Leeway
			---------
			= predictedHdt
			+ current_angle
			======================
			= newCog (on other tack)
			Calculation of (sea) current angle :
			1. from actual pos. calculate the endpoint of predictedHdt (out: predictedLatHdt, predictedLonHdt),
			assuming same StW on other tack
			2. at that point apply current : startpoint predictedLatHdt, predictedLonHdt + bearing + speed; out : predictedLatCog, predictedLonCog
			3. calculate angle (and speed) from curr pos to predictedLatCog, predictedLonCog; out : newCog + newSOG
        ************************************************************************/
        double  diffCogHdt;
        double tws_kts = fromUsrSpeed_Plugin(mTWS, g_iDashWindSpeedUnit);
        double stw_kts = fromUsrSpeed_Plugin(mStW, g_iDashSpeedUnit);
        double currspd_kts = std::isnan(m_ExpSmoothCurrSpd) ? 0.0 : fromUsrSpeed_Plugin(m_ExpSmoothCurrSpd, g_iDashSpeedUnit);
        double currdir = std::isnan(m_CurrentDirection) ? 0.0 : m_CurrentDirection;
        diffCogHdt = getDegRange(mCOG, mHdt);
        //avoid second "jumping" layline; Smooth predicted Cog as well
        mExpSmDiffCogHdt->SetAlpha(alpha_CogHdt);
        m_ExpSmoothDiffCogHdt = mExpSmDiffCogHdt->GetSmoothVal((diffCogHdt < 0 ? -diffCogHdt : diffCogHdt));
        if (targetTack == _T("R")){ // currently wind is from port ...now
            mPredictedHdG = m_LaylineSmoothedCog - m_ExpSmoothDiffCogHdt - 2 * mTWA - fabs(mLeeway); //Leeway is signed
            //GLubyte red(0), green(200), blue(0), alpha(128);
            glColor4ub(0, 200, 0, 128);                 	// red, green, blue,  alpha
        }
        else if (targetTack == _T("L")){ //currently wind from starboard
            mPredictedHdG = m_LaylineSmoothedCog + m_ExpSmoothDiffCogHdt + 2 * mTWA + fabs(mLeeway); //Leeway is signed
            //GLubyte red(204), green(41), blue(41), alpha(128);
            glColor4ub(204, 41, 41, 128);                 	// red, green, blue,  alpha
        }
        else {
            mPredictedHdG = (mTWA < 10) ? 180 : 0;
        }
        if (mPredictedHdG < 0) mPredictedHdG += 360;
        if (mPredictedHdG >= 360) mPredictedHdG -= 360;
        double predictedLatHdt, predictedLonHdt, predictedLatCog, predictedLonCog;
        double  predictedSog;
        //apply current on predicted Heading (mPredictedCoG)
        PositionBearingDistanceMercator_Plugin(mlat, mlon, mPredictedHdG, stw_kts, &predictedLatHdt, &predictedLonHdt);
        PositionBearingDistanceMercator_Plugin(predictedLatHdt, predictedLonHdt, currdir, currspd_kts, &predictedLatCog, &predictedLonCog);
        DistanceBearingMercator_Plugin(predictedLatCog, predictedLonCog, mlat, mlon, &mPredictedCoG, &predictedSog);
        //double exp. smoothing of mPredictedCoG
        double myrad = (90 - mPredictedCoG)*M_PI / 180.;
        mExpSmSinpredCog->SetAlpha(g_dalphaLaylinedDampFactor);
        mExpSmCospredCog->SetAlpha(g_dalphaLaylinedDampFactor);
        m_ExpSmoothSinpredCog = mExpSmSinpredCog->GetSmoothVal(sin(myrad));
        m_ExpSmoothCospredCog = mExpSmCospredCog->GetSmoothVal(cos(myrad));
        m_SmoothedpredCog = (int)(90. - (atan2(m_ExpSmoothSinpredCog, m_ExpSmoothCospredCog)*180. / M_PI) + 360.) % 360;

        tackpoints[0] = vpoints[0];
        double tmplat3, tmplon3, tmplat4, tmplon4;
        PositionBearingDistanceMercator_Plugin(mlat, mlon, m_SmoothedpredCog - m_ExpSmoothDegRange / 2., g_dLaylineLengthonChart, &tmplat3, &tmplon3);
        GetCanvasPixLL(vp, &tackpoints[1], tmplat3, tmplon3);
        PositionBearingDistanceMercator_Plugin(mlat, mlon, m_SmoothedpredCog + m_ExpSmoothDegRange / 2., g_dLaylineLengthonChart, &tmplat4, &tmplon4);
        GetCanvasPixLL(vp, &tackpoints[2], tmplat4, tmplon4);
        glBegin(GL_TRIANGLES);
        glVertex2d(tackpoints[0].x, tackpoints[0].y);
        glVertex2d(tackpoints[1].x, tackpoints[1].y);
        glVertex2d(tackpoints[2].x, tackpoints[2].y);
        glEnd();

        //            wxLogMessage("mlat=%f, mlon=%f,currspd=%f,predictedCoG=%f, mTWA=%f,mLeeway=%f, g_iDashSpeedUnit=%d", mlat, mlon, currspd_kts, mPredictedCoG, mTWA, mLeeway,g_iDashSpeedUnit);
        //wxLogMessage("tackpoints[0].x=%d, tackpoints[0].y=%d,tackpoints[1].x=%d, tackpoints[1].y=%d,tackpoints[2].x=%d, tackpoints[2].y=%d", tackpoints[0].x, tackpoints[0].y, tackpoints[1].x, tackpoints[1].y, tackpoints[2].x, tackpoints[2].y);
        //wxString GUID = g_sMarkGUID;

        //if (!GetSingleWaypoint(g_sMarkGUID, m_pMark))
        //     m_pMark = NULL;
        if (m_pMark)
        {
            if ( !BoatPolar->isValid() ) {
                if ( g_iDbgRes_Polar_Status != DBGRES_POLAR_INVALID ) {
                    wxLogMessage (
                        "dashboard_tactics_pi: >>> Missing or invalid Polar file: no Performance data, Laylines, Polar graphs available. <<<");
                    g_iDbgRes_Polar_Status = DBGRES_POLAR_INVALID;
                } // then debug print
                return;
            } // then no polar or is invalid
            g_iDbgRes_Polar_Status = DBGRES_POLAR_VALID;
 
            /*********************************************************************
				Draw the laylines btw. mark and boat with max 1 tack
				Additionally calculate if sailing the directline to
                 mark is faster.
				This direct line calculation is based on the theoretical
                  polardata for the TWA when on 'course to mark',
				TWS and 'distance to mark'
				Idea:
				* we draw the VMG laylines on mark and boat position
				* currently (for simplicity) I'm drawing/calculating
                 the laylines up- AND downwind ! Room for improvement ...
                 * per layline pair ..
                 * check, if they intersect, if yes
                 * calculate time to sail
                 * calculate time to sail on direct line, based on
                   polar data
                   * either draw layline or direct line
                   ***********************************************************************/
            //calculate Course to Mark = CTM
            double CTM, DistToMark, directLineTimeToMark, directLineTWA;
            DistanceBearingMercator_Plugin(m_pMark->m_lat, m_pMark->m_lon, mlat, mlon, &CTM, &DistToMark);
            //calc time-to-mark on direct line, versus opt. TWA and intersection
            directLineTWA = getMarkTWA(mTWD, CTM);
            directLineTimeToMark = CalcPolarTimeToMark(DistToMark, directLineTWA, tws_kts);
            if (std::isnan(directLineTimeToMark)) directLineTimeToMark = 99999;
            //use target VMG calculation for laylines-to-mark
            tvmg = BoatPolar->Calc_TargetVMG(directLineTWA, tws_kts); // directLineTWA <= 90deg--> upwind, >90 --> downwind
            //optional : use target CMG calculation for laylines-to-mark
            // tvmg = BoatPolar->Calc_TargetCMG(mTWS,mTWD,CTM); // directLineTWA <= 90deg --> upwind, >90 --> downwind
            double sigCTM_TWA = getSignedDegRange(CTM, mTWD);
            double cur_tacklinedir=0, target_tacklinedir=0;
            if (!std::isnan(tvmg.TargetAngle))
            {
                if (curTack == _T("\u00B0lr")){
                    cur_tacklinedir = mTWD - tvmg.TargetAngle - fabs(mLeeway);  //- m_ExpSmoothDiffCogHdt
                    target_tacklinedir = mTWD + tvmg.TargetAngle + fabs(mLeeway);//+ m_ExpSmoothDiffCogHdt
                }
                else{
                    cur_tacklinedir = mTWD + tvmg.TargetAngle + fabs(mLeeway);//+ m_ExpSmoothDiffCogHdt
                    target_tacklinedir = mTWD - tvmg.TargetAngle - fabs(mLeeway);//- m_ExpSmoothDiffCogHdt
                }
                while (cur_tacklinedir < 0) cur_tacklinedir += 360;
                while (cur_tacklinedir > 359) cur_tacklinedir -= 360;
                while (target_tacklinedir < 0) target_tacklinedir += 360;
                while (target_tacklinedir > 359) target_tacklinedir -= 360;
                double lat, lon, curlat, curlon, act_sog;
                wxRealPoint m_end, m_end2, c_end, c_end2;
                //apply current on foreward layline
                PositionBearingDistanceMercator_Plugin(mlat, mlon, cur_tacklinedir, stw_kts, &lat, &lon);
                PositionBearingDistanceMercator_Plugin(lat, lon, currdir, currspd_kts, &curlat, &curlon);
                DistanceBearingMercator_Plugin(curlat, curlon, mlat, mlon, &cur_tacklinedir, &act_sog);
                // smooth cur_tacklinedir, continue whith smoothed value
                if (wxIsNaN(m_ExpSmcur_tacklinedir)) m_ExpSmcur_tacklinedir = cur_tacklinedir;
 
                double myrad2 = (90 - cur_tacklinedir)*M_PI / 180.;
                mExpSmSincur_tacklinedir->SetAlpha(g_dalphaLaylinedDampFactor);
                mExpSmCoscur_tacklinedir->SetAlpha(g_dalphaLaylinedDampFactor);
                m_ExpSmoothSincur_tacklinedir = mExpSmSincur_tacklinedir->GetSmoothVal(sin(myrad2));
                m_ExpSmoothCoscur_tacklinedir = mExpSmCoscur_tacklinedir->GetSmoothVal(cos(myrad2));
                m_ExpSmcur_tacklinedir = (int)(90. - (atan2(m_ExpSmoothSincur_tacklinedir, m_ExpSmoothCoscur_tacklinedir)*180. / M_PI) + 360.) % 360;

                //cur_tacklinedir=local_bearing(curlat, curlon, mlat, mlon);
                //apply current on mark layline
                PositionBearingDistanceMercator_Plugin(mlat, mlon, target_tacklinedir, stw_kts, &lat, &lon);
                PositionBearingDistanceMercator_Plugin(lat, lon, currdir, currspd_kts, &curlat, &curlon);
                DistanceBearingMercator_Plugin(curlat, curlon, mlat, mlon, &target_tacklinedir, &act_sog);
                //smooth target_tacklinedir, continue whith smoothed value
                if (wxIsNaN(m_ExpSmtarget_tacklinedir)) m_ExpSmtarget_tacklinedir = target_tacklinedir;

                double myrad3 = (90 - target_tacklinedir)*M_PI / 180.;
                mExpSmSintarget_tacklinedir->SetAlpha(g_dalphaLaylinedDampFactor);
                mExpSmCostarget_tacklinedir->SetAlpha(g_dalphaLaylinedDampFactor);
                m_ExpSmoothSintarget_tacklinedir = mExpSmSintarget_tacklinedir->GetSmoothVal(sin(myrad3));
                m_ExpSmoothCostarget_tacklinedir = mExpSmCostarget_tacklinedir->GetSmoothVal(cos(myrad3));
                m_ExpSmtarget_tacklinedir = (int)(90. - (atan2(m_ExpSmoothSintarget_tacklinedir, m_ExpSmoothCostarget_tacklinedir)*180. / M_PI) + 360.) % 360;

                //target_tacklinedir=local_bearing(curlat, curlon, mlat, mlon);
                //double cur_tacklinedir2 = cur_tacklinedir > 180 ? cur_tacklinedir - 180 : cur_tacklinedir + 180;
                double cur_tacklinedir2 = m_ExpSmcur_tacklinedir > 180 ? m_ExpSmcur_tacklinedir - 180 : m_ExpSmcur_tacklinedir + 180;

                //Mark : get an end of the current dir
                PositionBearingDistanceMercator_Plugin(m_pMark->m_lat, m_pMark->m_lon, m_ExpSmcur_tacklinedir, DistToMark * 2, &m_end.y, &m_end.x);
                //Mark : get the second end of the same line on the opposite direction
                PositionBearingDistanceMercator_Plugin(m_pMark->m_lat, m_pMark->m_lon, cur_tacklinedir2, DistToMark * 2, &m_end2.y, &m_end2.x);

                //double boat_fwTVMGDir = target_tacklinedir > 180 ? target_tacklinedir - 180 : target_tacklinedir + 180;
                double boat_fwTVMGDir = m_ExpSmtarget_tacklinedir > 180 ? m_ExpSmtarget_tacklinedir - 180 : m_ExpSmtarget_tacklinedir + 180;
                //Boat : get an end of the predicted layline
                PositionBearingDistanceMercator_Plugin(mlat, mlon, boat_fwTVMGDir, DistToMark * 2, &c_end.y, &c_end.x);
                //Boat : get the second end of the same line on the opposite direction
                PositionBearingDistanceMercator_Plugin(mlat, mlon, m_ExpSmtarget_tacklinedir, DistToMark * 2, &c_end2.y, &c_end2.x);

                // see if we have an intersection of the 2 laylines
                wxRealPoint intersection_pos;
                intersection_pos = GetLineIntersection(c_end, c_end2, m_end, m_end2);

                if (intersection_pos.x > 0 && intersection_pos.y > 0) {
                    //calc time-to-mark on direct line, versus opt. TWA and intersection
                    // now calc time-to-mark via intersection
                    double TimeToMarkwithIntersect, tempCTM, DistToMarkwInt, dist1, dist2;
                    //distance btw. Boat and intersection
                    DistanceBearingMercator_Plugin(mlat, mlon, intersection_pos.y, intersection_pos.x, &tempCTM, &dist1);
                    //dist1 = local_distance(mlat, mlon, intersection_pos.y, intersection_pos.x);

                    //distance btw. Intersection - Mark
                    DistanceBearingMercator_Plugin(intersection_pos.y, intersection_pos.x, m_pMark->m_lat, m_pMark->m_lon, &tempCTM, &dist2);
                    //dist2 = local_distance(intersection_pos.y, intersection_pos.x, m_pMark->m_lat, m_pMark->m_lon);

                    //Total distance as sum of dist1 + dist2
                    //Note : current is NOT yet taken into account here !
                    DistToMarkwInt = dist1 + dist2;
                    TimeToMarkwithIntersect = CalcPolarTimeToMark(DistToMarkwInt, tvmg.TargetAngle, tws_kts);
                    if (std::isnan(TimeToMarkwithIntersect))TimeToMarkwithIntersect = 99999;
                    if (TimeToMarkwithIntersect > 0 && directLineTimeToMark > 0){
                        //only draw the laylines with intersection, if they are faster than the direct course
                        if (TimeToMarkwithIntersect < directLineTimeToMark){
                            if (curTack == _T("\u00B0lr"))
                                glColor4ub(255, 0, 0, 255);
                            else
                                glColor4ub(0, 200, 0, 255);
                            glLineWidth(2);
                            wxPoint inter;
                            GetCanvasPixLL(vp, &inter, intersection_pos.y, intersection_pos.x);
                            glBegin(GL_LINES); // intersect from forward layline --> target VMG --> mark
                            glVertex2d(boat.x, boat.y); // from boat with target VMG-Angle sailing forward  to intersection
                            glVertex2d(inter.x, inter.y);
                            if (curTack == _T("\u00B0lr"))
                                glColor4ub(0, 200, 0, 255);
                            else
                                glColor4ub(255, 0, 0, 255);
                            glVertex2d(inter.x, inter.y); // from intersection with target VMG-Angle to mark
                            glVertex2d(mark_center.x, mark_center.y);
                            glEnd();
                        }
                        else{ // otherwise highlight the direct line
                            if (sigCTM_TWA < 0)
                                glColor4ub(255, 0, 0, 255);
                            else
                                glColor4ub(0, 200, 0, 255);
                            glLineWidth(2);
                            glBegin(GL_LINES);
                            glVertex2d(boat.x, boat.y);
                            glVertex2d(mark_center.x, mark_center.y);
                            glEnd();
                        }
                    }
                }
                else { //no intersection at all
                    if (directLineTimeToMark < 99999.){ //but direct line may be valid
                        if (sigCTM_TWA <0)
                            glColor4ub(255, 0, 0, 255);
                        else
                            glColor4ub(0, 200, 0, 255);
                        glLineWidth(2);
                        glBegin(GL_LINES);
                        glVertex2d(boat.x, boat.y);
                        glVertex2d(mark_center.x, mark_center.y);
                        glEnd();
                    }
                    else { //no intersection and no valid direct line
                        // convert from coordinates to screen values
                        wxPoint cogend, mark_end;
                        GetCanvasPixLL(vp, &mark_end, m_end.y, m_end.x);
                        GetCanvasPixLL(vp, &cogend, c_end.y, c_end.x);
                        if (curTack == _T("\u00B0lr"))glColor4ub(255, 0, 0, 255);
                        else  glColor4ub(0, 200, 0, 255);
                        glLineWidth(2);
                        glLineStipple(4, 0xAAAA);
                        glEnable(GL_LINE_STIPPLE);
                        glBegin(GL_LINES); // intersect from forward layline --> target VMG --> mark
                        glVertex2d(boat.x, boat.y); // from boat with target VMG-Angle sailing forward  to intersection
                        glVertex2d(cogend.x, cogend.y);
                        if (curTack == _T("\u00B0lr"))glColor4ub(0, 200, 0, 255);
                        else  glColor4ub(255, 0, 0, 255);
                        glVertex2d(mark_end.x, mark_end.y); // from intersection with target VMG-Angle to mark
                        glVertex2d(mark_center.x, mark_center.y);
                        glEnd();
                        glDisable(GL_LINE_STIPPLE);
                    }
                }
            }
            double target_tacklinedir2 = m_ExpSmtarget_tacklinedir > 180 ? m_ExpSmtarget_tacklinedir - 180 : m_ExpSmtarget_tacklinedir + 180;
            wxRealPoint pm_end, pm_end2, pc_end, pc_end2;

            //Mark : get an end of the predicted layline
            PositionBearingDistanceMercator_Plugin(m_pMark->m_lat, m_pMark->m_lon, m_ExpSmtarget_tacklinedir, DistToMark * 2, &pm_end.y, &pm_end.x);
            //Mark : get the second end of the same predicted layline on the opposite direction
            PositionBearingDistanceMercator_Plugin(m_pMark->m_lat, m_pMark->m_lon, target_tacklinedir2, DistToMark * 2, &pm_end2.y, &pm_end2.x);
            //double boat_tckTVMGDir = cur_tacklinedir > 180 ? cur_tacklinedir - 180 : cur_tacklinedir + 180;
            double boat_tckTVMGDir = m_ExpSmcur_tacklinedir > 180 ? m_ExpSmcur_tacklinedir - 180 : m_ExpSmcur_tacklinedir + 180;
            //Boat : get an end of the predicted layline
            PositionBearingDistanceMercator_Plugin(mlat, mlon, boat_tckTVMGDir, DistToMark * 2, &pc_end.y, &pc_end.x);
            //Boat : get the second end of the same predicted layline on the opposite direction
            PositionBearingDistanceMercator_Plugin(mlat, mlon, m_ExpSmcur_tacklinedir, DistToMark * 2, &pc_end2.y, &pc_end2.x);

            // see if we have an intersection of the 2 laylines
            wxRealPoint pIntersection_pos;
            pIntersection_pos = GetLineIntersection(pc_end, pc_end2, pm_end, pm_end2);

            if (pIntersection_pos.x > 0 && pIntersection_pos.y > 0){
                //calc time-to-mark on direct line, versus opt. TWA and intersection
                // now calc time-to-mark via intersection
                double TimeToMarkwInt, tempCTM, DistToMarkwInt, dist1, dist2;
                //distance btw. boat and intersection
                DistanceBearingMercator_Plugin(mlat, mlon, pIntersection_pos.y, pIntersection_pos.x, &tempCTM, &dist1);
                //dist1 = local_distance(mlat, mlon, pIntersection_pos.y, pIntersection_pos.x);

                //distance btw. Intersection - Mark
                DistanceBearingMercator_Plugin(pIntersection_pos.y, pIntersection_pos.x, m_pMark->m_lat, m_pMark->m_lon, &tempCTM, &dist2);
                //dist2 = local_distance(pIntersection_pos.y, pIntersection_pos.x, m_pMark->m_lat, m_pMark->m_lon);
                //Total distance as sum of dist1 + dist2
                DistToMarkwInt = dist1 + dist2;
                TimeToMarkwInt = CalcPolarTimeToMark(DistToMarkwInt, tvmg.TargetAngle, tws_kts);
                if (std::isnan(TimeToMarkwInt))TimeToMarkwInt = 99999;
                if (TimeToMarkwInt > 0 && directLineTimeToMark > 0){
                    //only draw the laylines with intersection, if they are faster than the direct course
                    if (TimeToMarkwInt < directLineTimeToMark){
                        if (curTack == _T("\u00B0lr"))
                            glColor4ub(0, 200, 0, 255);
                        else
                            glColor4ub(255, 0, 0, 255);
                        glLineWidth(2);
                        wxPoint pinter;
                        GetCanvasPixLL(vp, &pinter, pIntersection_pos.y, pIntersection_pos.x);

                        glBegin(GL_LINES); // intersect from target layline --> target other tack VMG --> mark
                        glVertex2d(boat.x, boat.y);   //from boat to intersection with Target VMG-Angle, but sailing on other tack
                        glVertex2d(pinter.x, pinter.y);
                        if (curTack == _T("\u00B0lr"))
                            glColor4ub(255, 0, 0, 255);
                        else
                            glColor4ub(0, 200, 0, 255);
                        glVertex2d(pinter.x, pinter.y);//from intersection to mark with Target VMG-Angle, but sailing on other tack
                        glVertex2d(mark_center.x, mark_center.y);
                        glEnd();
                    }
                    else { // otherwise highlight the direct line
                        if (sigCTM_TWA <0)
                            glColor4ub(255, 0, 0, 255);
                        else
                            glColor4ub(0, 200, 0, 255);
                        glLineWidth(2);
                        glBegin(GL_LINES);
                        glVertex2d(boat.x, boat.y);
                        glVertex2d(mark_center.x, mark_center.y);
                        glEnd();
                    }
                }
            }
            else { //no intersection ...
                if (directLineTimeToMark < 99999.){ //but direct line may be valid
                    if (sigCTM_TWA <0)
                        glColor4ub(255, 0, 0, 255);
                    else
                        glColor4ub(0, 200, 0, 255);
                    glLineWidth(2);
                    glBegin(GL_LINES);
                    glVertex2d(boat.x, boat.y);
                    glVertex2d(mark_center.x, mark_center.y);
                    glEnd();
                }
                else{
                    wxPoint pcogend, pmarkend;
                    GetCanvasPixLL(vp, &pmarkend, pm_end.y, pm_end.x);
                    GetCanvasPixLL(vp, &pcogend, pc_end.y, pc_end.x);

                    if (curTack == _T("\u00B0lr"))glColor4ub(0, 200, 0, 255);
                    else  glColor4ub(255, 0, 0, 255);
                    glLineWidth(2);
                    glLineStipple(4, 0xAAAA);
                    glEnable(GL_LINE_STIPPLE);
                    glBegin(GL_LINES); // intersect from target layline --> target other tack VMG --> mark
                    glVertex2d(boat.x, boat.y);   //from boat to intersection with Target VMG-Angle, but sailing on other tack
                    glVertex2d(pcogend.x, pcogend.y);
                    if (curTack == _T("\u00B0lr"))glColor4ub(255, 0, 0, 255);
                    else glColor4ub(0, 200, 0, 255);
                    glVertex2d(pmarkend.x, pmarkend.y);//from intersection to mark with Target VMG-Angle, but sailing on other tack
                    glVertex2d(mark_center.x, mark_center.y);
                    glEnd();
                    glDisable(GL_LINE_STIPPLE);
                }
            }
        }
    }
}


/**********************************************************************
Draw the OpenGL Polar on the ships position overlay
Polar is normalized (always same size)
What should be drawn:
* the actual polar curve for the actual TWS
* 0/360� point (directly upwind)
* the rest of the polar currently in 2� steps
***********************************************************************/
#define STEPS  180 //72

void tactics_pi::DrawPolar(PlugIn_ViewPort *vp, wxPoint pp, double PolarAngle)
{
    if ( !m_bShowPolarOnChart )
        return;
    if ( !BoatPolar->isValid() ) {
        if ( g_iDbgRes_Polar_Status != DBGRES_POLAR_INVALID ) {
            wxLogMessage ("dashboard_tactics_pi: >>> Missing or invalid Polar file: no Performance data, Laylines, Polar graphs available. <<<");
            g_iDbgRes_Polar_Status = DBGRES_POLAR_INVALID;
        } // then debug print
        return;
    } // then no polar or it is not valid
    g_iDbgRes_Polar_Status = DBGRES_POLAR_VALID;
    
    std::unique_lock<std::mutex> lckmTWS( mtxTWS );
    std::unique_lock<std::mutex> lckmTWD( mtxTWD );
    std::unique_lock<std::mutex> lckmBRG( mtxBRG );
	if (!std::isnan(mTWS) && !std::isnan(mTWD) && !std::isnan(mBRG)){
		glColor4ub(0, 0, 255, 192);	// red, green, blue,  alpha (byte values)
		double polval[STEPS];
		double max = 0;
		double rotate = vp->rotation;
		int i;
		if (mTWS > 0){
			TargetxMG vmg_up = BoatPolar->GetTargetVMGUpwind(mTWS);
			TargetxMG vmg_dn = BoatPolar->GetTargetVMGDownwind(mTWS);
			TargetxMG CmGMax, CmGMin;
			BoatPolar->Calc_TargetCMG2(mTWS, mTWD, mBRG, &CmGMax, &CmGMin);  //CmGMax = the higher value, CmGMin the lower cmg value

			for (i = 0; i < STEPS / 2; i++){ //0...179
				polval[i] = BoatPolar->GetPolarSpeed(i * 2 + 1, mTWS); //polar data is 1...180 !!! i*2 : draw in 2� steps
				polval[STEPS - 1 - i] = polval[i];
				//if (std::isnan(polval[i])) polval[i] = polval[STEPS-1 - i] = 0.0;
				if (polval[i]>max) max = polval[i];
			}
			wxPoint currpoints[STEPS];
			double rad, anglevalue;
			for (i = 0; i < STEPS; i++){
				anglevalue = deg2rad(PolarAngle + i * 2) + deg2rad(0. - ANGLE_OFFSET); //i*2 : draw in 2� steps
				rad = 81 * polval[i] / max;
				currpoints[i].x = pp.x + (rad * cos(anglevalue));
				currpoints[i].y = pp.y + (rad * sin(anglevalue));
			}
			glLineWidth(1);
			glBegin(GL_LINES);

			if (std::isnan(polval[0])){ //always draw the 0� point (directly upwind)
				currpoints[0].x = pp.x;
				currpoints[0].y = pp.y;
			}
			glVertex2d(currpoints[0].x, currpoints[0].y);

			for (i = 1; i < STEPS; i++){
				if (!std::isnan(polval[i])){  //only draw, if we have a real data value (NAN is init status, w/o data)
					glVertex2d(currpoints[i].x, currpoints[i].y);
					glVertex2d(currpoints[i].x, currpoints[i].y);
				}
			}
			glVertex2d(currpoints[0].x, currpoints[0].y); //close the curve

			//dc->DrawPolygon(STEPS, currpoints, 0, 0);
			glEnd();
			//draw Target-VMG Angles now
			if (!std::isnan(vmg_up.TargetAngle)){
				rad = 81 * BoatPolar->GetPolarSpeed(vmg_up.TargetAngle, mTWS) / max;
				DrawTargetAngle(vp, pp, PolarAngle + vmg_up.TargetAngle, _T("BLUE3"), 1, rad);
				DrawTargetAngle(vp, pp, PolarAngle - vmg_up.TargetAngle, _T("BLUE3"), 1, rad);
			}
			if (!std::isnan(vmg_dn.TargetAngle)){
				rad = 81 * BoatPolar->GetPolarSpeed(vmg_dn.TargetAngle, mTWS) / max;
				DrawTargetAngle(vp, pp, PolarAngle + vmg_dn.TargetAngle, _T("BLUE3"), 1, rad);
				DrawTargetAngle(vp, pp, PolarAngle - vmg_dn.TargetAngle, _T("BLUE3"), 1, rad);
			}
			if (!std::isnan(CmGMax.TargetAngle)){
				rad = 81 * BoatPolar->GetPolarSpeed(CmGMax.TargetAngle, mTWS) / max;
				DrawTargetAngle(vp, pp, PolarAngle + CmGMax.TargetAngle, _T("URED"), 2, rad);
			}
			if (!std::isnan(CmGMin.TargetAngle)){
				rad = 81 * BoatPolar->GetPolarSpeed(CmGMin.TargetAngle, mTWS) / max;
				DrawTargetAngle(vp, pp, PolarAngle + CmGMin.TargetAngle, _T("URED"), 1, rad);
			}
			//Hdt line
			if (!std::isnan(mHdt)){
				wxPoint hdt;
				anglevalue = deg2rad(mHdt) + deg2rad(0. - ANGLE_OFFSET) + rotate;
				rad = 81 * 1.1;
				hdt.x = pp.x + (rad * cos(anglevalue));
				hdt.y = pp.y + (rad * sin(anglevalue));
				glColor4ub(0, 0, 255, 255);	// red, green, blue,  alpha (byte values)
				glLineWidth(3);
				glBegin(GL_LINES);
				glVertex2d(pp.x, pp.y);
				glVertex2d(hdt.x, hdt.y);
				glEnd();
			}
		}
	}
}
/***********************************************************************
Draw pointers for the optimum target VMG- and CMG Angle (if bearing is available)
************************************************************************/
void tactics_pi::DrawTargetAngle(PlugIn_ViewPort *vp, wxPoint pp, double Angle, wxString color, int size, double rad){
	//  if (TargetAngle > 0){
	double rotate = vp->rotation;
	//    double value = deg2rad(PolarAngle + TargetAngle) + deg2rad(0 - ANGLE_OFFSET) + rotate;
	//    double value1 = deg2rad(PolarAngle + 5 + TargetAngle) + deg2rad(0 - ANGLE_OFFSET) + rotate;
	//    double value2 = deg2rad(PolarAngle - 5 + TargetAngle) + deg2rad(0 - ANGLE_OFFSET) + rotate;
	double sizefactor, widthfactor;
	if (size == 1) {
		sizefactor = 1.05;
		widthfactor = 1.05;
	}
	else{
		sizefactor = 1.12;
		widthfactor = 1.4;
	}
	double value = deg2rad(Angle) + deg2rad(0 - ANGLE_OFFSET) + rotate;
	double value1 = deg2rad(Angle + 5 * widthfactor) + deg2rad(0 - ANGLE_OFFSET) + rotate;
	double value2 = deg2rad(Angle - 5 * widthfactor) + deg2rad(0 - ANGLE_OFFSET) + rotate;

	/*
     *           0
     *          /\
     *         /  \
     *        /    \
     *     2 /_ __ _\ 1
     *
     *           X
     */
	wxPoint points[4];
	points[0].x = pp.x + (rad * 0.95 * cos(value));
	points[0].y = pp.y + (rad * 0.95 * sin(value));
	points[1].x = pp.x + (rad * 1.15 * sizefactor * cos(value1));
	points[1].y = pp.y + (rad * 1.15 * sizefactor * sin(value1));
	points[2].x = pp.x + (rad * 1.15 * sizefactor * cos(value2));
	points[2].y = pp.y + (rad * 1.15 * sizefactor * sin(value2));
	/*    points[1].x = pp.x + (rad * 1.15 * cos(value1));
          points[1].y = pp.y + (rad * 1.15 * sin(value1));
          points[2].x = pp.x + (rad * 1.15 * cos(value2));
          points[2].y = pp.y + (rad * 1.15 * sin(value2));*/
	if (color == _T("BLUE3")) glColor4ub(0, 0, 255, 128);
	else if (color == _T("URED")) glColor4ub(255, 0, 0, 128);
	else glColor4ub(255, 128, 0, 168);

	glLineWidth(1);
	glBegin(GL_TRIANGLES);
	glVertex2d(points[0].x, points[0].y);
	glVertex2d(points[1].x, points[1].y);
	glVertex2d(points[2].x, points[2].y);
	glEnd();

	//}
	//  }
}
/***********************************************************************
Toggle Layline Render overlay
************************************************************************/
void tactics_pi::ToggleLaylineRender()
{
	m_bLaylinesIsVisible = m_bLaylinesIsVisible ? false : true;
    g_bDisplayLaylinesOnChart = m_bLaylinesIsVisible;

}
void tactics_pi::ToggleCurrentRender()
{
	m_bDisplayCurrentOnChart = m_bDisplayCurrentOnChart ? false : true;
    g_bDisplayCurrentOnChart = m_bDisplayCurrentOnChart;
}
void tactics_pi::TogglePolarRender()
{
	m_bShowPolarOnChart = m_bShowPolarOnChart ? false : true;
    g_bShowPolarOnChart = m_bShowPolarOnChart;
}
void tactics_pi::ToggleWindbarbRender()
{
	m_bShowWindbarbOnChart = m_bShowWindbarbOnChart ? false : true;
    g_bShowWindbarbOnChart = m_bShowWindbarbOnChart;
}

/**********************************************************************
 **********************************************************************/
bool tactics_pi::GetLaylineVisibility()
{
	return m_bLaylinesIsVisible;
}
bool tactics_pi::GetCurrentVisibility()
{
	return m_bDisplayCurrentOnChart;
}
bool tactics_pi::GetWindbarbVisibility()
{
	return m_bShowWindbarbOnChart;
}
bool tactics_pi::GetPolarVisibility()
{
	return m_bShowPolarOnChart;
}

/*******************************************************************
 Calculate degree-range for laylines
 Do some exponential smoothing on degree range of COGs and  COG itself
*******************************************************************/
void tactics_pi::CalculateLaylineDegreeRange(void)
{
    if (!std::isnan(mCOG)){
        if (mCOG != m_COGRange[0]){
            if (std::isnan(m_ExpSmoothSinCog)) m_ExpSmoothSinCog = 0;
            if (std::isnan(m_ExpSmoothCosCog)) m_ExpSmoothCosCog = 0;


            double mincog = 360, maxcog = 0;
            for (int i = 0; i < COGRANGE; i++){
                if (!std::isnan(m_COGRange[i])){
                    mincog = wxMin(mincog, m_COGRange[i]);
                    maxcog = wxMax(maxcog, m_COGRange[i]);
                }
            }
            m_LaylineDegRange = getDegRange(maxcog, mincog);
            for (int i = 0; i < COGRANGE - 1; i++)
                m_COGRange[i + 1] = m_COGRange[i];
            m_COGRange[0] = mCOG;
            if (m_LaylineDegRange < g_iMinLaylineWidth){
                m_LaylineDegRange = g_iMinLaylineWidth;
            }
            else if (m_LaylineDegRange > g_iMaxLaylineWidth){
                m_LaylineDegRange = g_iMaxLaylineWidth;
            }

            //shifting
            double rad = (90 - mCOG)*M_PI / 180.;
            mExpSmSinCog->SetAlpha(g_dalphaLaylinedDampFactor);
            mExpSmCosCog->SetAlpha(g_dalphaLaylinedDampFactor);
            m_ExpSmoothSinCog = mExpSmSinCog->GetSmoothVal(sin(rad));
            m_ExpSmoothCosCog = mExpSmCosCog->GetSmoothVal(cos(rad));

            m_LaylineSmoothedCog = (int)(90. - (atan2( m_ExpSmoothSinCog, m_ExpSmoothCosCog)*180. / M_PI) + 360.) % 360;

            mExpSmDegRange->SetAlpha(g_dalphaDeltCoG);
            m_ExpSmoothDegRange =
                mExpSmDegRange->GetSmoothVal(m_LaylineDegRange);
        }
    }
}

/**********************************************************************
Draw the OpenGL Windbarb on the ships position overlay
Basics taken from tackandlay_pi and adopted
***********************************************************************/
void tactics_pi::DrawWindBarb(wxPoint pp, PlugIn_ViewPort *vp)
{
    if (!m_bShowWindbarbOnChart)
        return;
    std::unique_lock<std::mutex> lckmTWS( mtxTWS );
    std::unique_lock<std::mutex> lckmTWD( mtxTWD );
    if (!std::isnan(mTWD) && !std::isnan(mTWS)){
        if (mTWD >= 0 && mTWD < 360){
            glColor4ub(0, 0, 255, 192);	// red, green, blue,  alpha (byte values)
            double rad_angle;
            double shaft_x, shaft_y;
            double barb_0_x, barb_0_y, barb_1_x, barb_1_y;
            double barb_2_x, barb_2_y;
            double barb_length_0_x, barb_length_0_y, barb_length_1_x, barb_length_1_y;
            double barb_length_2_x, barb_length_2_y;
            double barb_3_x, barb_3_y, barb_4_x, barb_4_y, barb_length_3_x, barb_length_3_y, barb_length_4_x, barb_length_4_y;
            double tws_kts = fromUsrSpeed_Plugin(mTWS, g_iDashWindSpeedUnit);

            double barb_length[50] = {
                0, 0, 0, 0, 0,    //  0 knots
                0, 0, 0, 0, 5,    //  5 knots
                0, 0, 0, 0, 10,    // 10 knots
                0, 0, 0, 5, 10,    // 15 knots
                0, 0, 0, 10, 10,   // 20 knots
                0, 0, 5, 10, 10,   // 25 knots
                0, 0, 10, 10, 10,  // 30 knots
                0, 5, 10, 10, 10,  // 35 knots
                0, 10, 10, 10, 10,  // 40 knots
                5, 10, 10, 10, 10  // 45 knots
            };

            int p = 0;
            if (tws_kts < 3.)
                p = 0;
            else if (tws_kts >= 3. && tws_kts < 8.)
                p = 1;
            else if (tws_kts >= 8. && tws_kts < 13.)
                p = 2;
            else if (tws_kts >= 13. && tws_kts < 18.)
                p = 3;
            else if (tws_kts >= 18. && tws_kts < 23.)
                p = 4;
            else if (tws_kts >= 23. && tws_kts < 28.)
                p = 5;
            else if (tws_kts >= 28. && tws_kts < 33.)
                p = 6;
            else if (tws_kts >= 33. && tws_kts < 38.)
                p = 7;
            else if (tws_kts >= 38. && tws_kts < 43.)
                p = 8;
            else if (tws_kts >= 43. && tws_kts < 48.)
                p = 9;
            else if (tws_kts >= 48.)
                p = 9;
            //wxLogMessage("mTWS=%.2f --> p=%d", mTWS, p);
            p = 5 * p;

            double rotate = vp->rotation;
            rad_angle = ((mTWD - 90.) * M_PI / 180) + rotate;

            shaft_x = cos(rad_angle) * 90;
            shaft_y = sin(rad_angle) * 90;

            barb_0_x = pp.x + .6 * shaft_x;
            barb_0_y = (pp.y + .6 * shaft_y);
            barb_1_x = pp.x + .7 * shaft_x;
            barb_1_y = (pp.y + .7 * shaft_y);
            barb_2_x = pp.x + .8 * shaft_x;
            barb_2_y = (pp.y + .8 * shaft_y);
            barb_3_x = pp.x + .9 * shaft_x;
            barb_3_y = (pp.y + .9 * shaft_y);
            barb_4_x = pp.x + shaft_x;
            barb_4_y = (pp.y + shaft_y);

            barb_length_0_x = cos(rad_angle + M_PI / 4) * barb_length[p] * 3;
            barb_length_0_y = sin(rad_angle + M_PI / 4) * barb_length[p] * 3;
            barb_length_1_x = cos(rad_angle + M_PI / 4) * barb_length[p + 1] * 3;
            barb_length_1_y = sin(rad_angle + M_PI / 4) * barb_length[p + 1] * 3;
            barb_length_2_x = cos(rad_angle + M_PI / 4) * barb_length[p + 2] * 3;
            barb_length_2_y = sin(rad_angle + M_PI / 4) * barb_length[p + 2] * 3;
            barb_length_3_x = cos(rad_angle + M_PI / 4) * barb_length[p + 3] * 3;
            barb_length_3_y = sin(rad_angle + M_PI / 4) * barb_length[p + 3] * 3;
            barb_length_4_x = cos(rad_angle + M_PI / 4) * barb_length[p + 4] * 3;
            barb_length_4_y = sin(rad_angle + M_PI / 4) * barb_length[p + 4] * 3;

            glLineWidth(2);
            glBegin(GL_LINES);
            glVertex2d(pp.x, pp.y);
            glVertex2d(pp.x + shaft_x, pp.y + shaft_y);
            glVertex2d(barb_0_x, barb_0_y);
            glVertex2d(barb_0_x + barb_length_0_x, barb_0_y + barb_length_0_y);
            glVertex2d(barb_1_x, barb_1_y);
            glVertex2d(barb_1_x + barb_length_1_x, barb_1_y + barb_length_1_y);
            glVertex2d(barb_2_x, barb_2_y);
            glVertex2d(barb_2_x + barb_length_2_x, barb_2_y + barb_length_2_y);
            glVertex2d(barb_3_x, barb_3_y);
            glVertex2d(barb_3_x + barb_length_3_x, barb_3_y + barb_length_3_y);
            glVertex2d(barb_4_x, barb_4_y);
            glVertex2d(barb_4_x + barb_length_4_x, barb_4_y + barb_length_4_y);
            glEnd();
        }
    }
}

/********************************************************************
Before the derived class which implements the abstract method
SendSentenceToAllInstruments() actually sends the sentences to
the instrumetns in its windows, it needs to call this method to
correct the values if Tactics performance functions are activated.
According to the settings and the sentence, it may return an
corrected value (cf. the documentation for operation principles).
true - yes, pass sentence with corrected values/units to instruments
false - no, continue processing, we've changed nothing
*********************************************************************/
bool tactics_pi::SendSentenceToAllInstruments_PerformanceCorrections(
        unsigned long long st, double &value, wxString &unit )
{

    if (st == OCPN_DBP_STC_AWS){
        mAWS = value;
        mAWSnocorr = NAN;
        mAWSUnit = unit;
        /* Correct AWS with heel if global variable set and heel
           is available. The correction only makes sense if one
           uses a heel sensor.
           AWS_corrected =
           AWS_measured * cos(AWA_measured) / cos(AWA_corrected) */
        if (g_bCorrectAWwithHeel == true && g_bUseHeelSensor &&
            !std::isnan(mheel) && !std::isnan(value)) {
            mAWSnocorr = value;
            mAWS = value / cos(mheel*M_PI / 180.);
            value = mAWS;
            return true;
        }
    }
    if (st == OCPN_DBP_STC_STW){
        mStW = value;
        mStWnocorr = NAN;
        /* Correct STW with Leeway if global variable set and heel
           is available. The correction only makes sense if one
           uses a heel sensor. */
        if (g_bCorrectSTWwithLeeway == true && g_bUseHeelSensor &&
            !std::isnan(mLeeway) && !std::isnan(mheel)) {
            mStWnocorr = value;
            mStW = value / cos(mLeeway *M_PI / 180.0);
            value = mStW;
            return true;
        }
    }
    if (st == OCPN_DBP_STC_BRG){
        mBRG = value;
        mBRGnocorr = NAN;
        if (m_pMark && !std::isnan(mlat) && !std::isnan(mlon)) {
            double dist;
            double newvalue = value;
            mBRGnocorr = value;
            DistanceBearingMercator_Plugin(m_pMark->m_lat,
                                           m_pMark->m_lon, mlat, mlon,
                                           &newvalue, &dist);
            mBRG = newvalue;
            value = mBRG;
            unit = g_sMarkGUID;
            return true;
        }
    }
    if (st == OCPN_DBP_STC_AWA){
        mAWA = value;
        mAWAnocorr = NAN;
        mAWAUnit = unit;
        if (g_bCorrectAWwithHeel == true && g_bUseHeelSensor &&
            !std::isnan(mLeeway) && !std::isnan(mheel)){
            /* Correct AWA with heel if global variable set and heel
               is available. Correction only makes sense with heel
               sensor is available */
            double tan_awa = tan(value * M_PI / 180.);
            double awa_heel;
            mAWAnocorr = value;
            if (std::isnan(tan_awa))
                awa_heel = value;
            else
            {
                double cos_heel = cos(mheel * M_PI / 180.);
                awa_heel = atan(tan_awa / cos_heel) *180. / M_PI;
                if (value >= 0.0){
                    if (value > 90.0)
                        awa_heel += 180.0;
                }
                else{
                    if (value < -90.0)
                        awa_heel -= 180.0;
                }
            }
            mAWA = awa_heel;
            value = mAWA;
            return true;
        }
    }

    return false;
}

/********************************************************************
This method is method is to use to pass calculated performance
sentences from Tactics instruments and functions to other
instruments and/or between the Tactics instruments.
An example client of these sentences is streaming/archiving and
statistical calculation instruments.
*********************************************************************/
void tactics_pi::SendPerfSentenceToAllInstruments(
    unsigned long long st, double value, wxString unit, long long timestamp ) {
    // use the shortcut to instruments, i.e. not making callbacks to this module
    pSendSentenceToAllInstruments( st, value, unit, timestamp );
}

/********************************************************************
This method is method is to use to pass streamed-in Signal K update
into the NMEA interpretation method of the implementating (derived)
class.
*********************************************************************/
void tactics_pi::SetUpdateSignalK(
        wxString *type, wxString *sentenceId, wxString *talker, wxString *src, int pgn,
        wxString *path, double value, wxString *valStr, long long timestamp, wxString *key )
{
    wxString noNMEA = wxEmptyString;
    SetNMEASentence( noNMEA, type, sentenceId, talker, src, pgn, path, value, valStr, timestamp, key );
}

/********************************************************************
Before the derived class which implements the abstract method
SendSentenceToAllInstruments() actually sends the sentences to
the instrumetns in its windows, it needs to call this method to
check that if the sentence is needed to in true wind calculations.
false - no, do not launch the true wind calculations, continue
true - yes, use  (and only call true wind calculations if this true)
*********************************************************************/

bool tactics_pi::SendSentenceToAllInstruments_LaunchTrueWindCalculations(
        unsigned long long st, double value )
{
    // Let's collect information about instruments providing true wind
    if ( (st == OCPN_DBP_STC_TWA) )
        m_bTrueWindAngle_available = true;
    if ( (st == OCPN_DBP_STC_TWS) )
        m_bTrueWindSpeed_available = true;
    if ( (st == OCPN_DBP_STC_TWD) ) {
        if ( std::isnan(value) || (value == 0.0) )
                m_bTrueWindDirection_available = false;
    }
    if ( m_bTrueWindAngle_available && m_bTrueWindSpeed_available && m_bTrueWindDirection_available )
        m_bTrueWind_available = true;
    // Here's the logic depending of the data and settings
    if ( st != OCPN_DBP_STC_AWS ) { // this is the data we're waiting
        if ( m_iDbgRes_TW_Calc_AWS_STC == DBGRES_AWS_STC_UNKNOWN ) {
            wxLogMessage ("dashboard_tactics_pi: Tactics true wind calculations: waiting on an AWS sentence.");
            m_iDbgRes_TW_Calc_AWS_STC = DBGRES_AWS_STC_WAIT;
        }
        return false;
    } // then no AWS sentence            
    if ( std::isnan(value) ) { // but it can be sent by the AWS watchdog
        if ( ( m_iDbgRes_TW_Calc_AWS_STC == DBGRES_AWS_STC_WAIT ) || ( m_iDbgRes_TW_Calc_AWS_STC == DBGRES_AWS_STC_AVAILABLE ) ) {
            wxLogMessage ("dashboard_tactics_pi: Tactics true wind calculations: invalid AWS or timeout on it.");
            m_iDbgRes_TW_Calc_AWS_STC = DBGRES_AWS_STC_AVAILABLE_INVALID;
        }
        m_iDbgRes_TW_Calc_Lau = DBGRES_EXEC_FALSE;
        return false; // in this case no TW calculations, move on
    } // then invalid contents
    if ( ( m_iDbgRes_TW_Calc_AWS_STC == DBGRES_AWS_STC_WAIT ) || ( m_iDbgRes_TW_Calc_AWS_STC == DBGRES_AWS_STC_AVAILABLE_INVALID ) ) {
        wxLogMessage ("dashboard_tactics_pi: Tactics true wind calculations: a valid AWS received, now (%f).", value);
        m_iDbgRes_TW_Calc_AWS_STC = DBGRES_AWS_STC_AVAILABLE;
    } // then AWS sentence with valid contents
    
    if ( g_bForceTrueWindCalculation ) {
        if ( m_bTrueWind_available ) { // Force TW calc. selection no effect
            if ( ( m_iDbgRes_TW_Calc_Force != DBGRES_FORCE_SELECTED_TW_AVAILABLE ) ) {
                wxLogMessage ("dashboard_tactics_pi: Tactics true wind calculations: TW data available but forced calculation requested.");
                m_iDbgRes_TW_Calc_Force = DBGRES_FORCE_SELECTED_TW_AVAILABLE;
            } // then debug messsage out
        } // the true wind is available from instruments
        else {
            if ( m_bTrueWindAngle_available && m_bTrueWindSpeed_available && !m_bTrueWindDirection_available ) {
                if ( ( m_iDbgRes_TW_Calc_Force != DBGRES_FORCE_SELECTED_NO_TWD_AVAILABLE ) ) {
                    wxLogMessage (
                        "dashboard_tactics_pi: Tactics true wind calculations: Forced calculation requested, no TWD, but has TWA/TWS.");
                    m_iDbgRes_TW_Calc_Force = DBGRES_FORCE_SELECTED_NO_TWD_AVAILABLE;
                }
            } // then true wind available but not true wind direction, perhaps we are moored
            else {
                if ( ( m_iDbgRes_TW_Calc_Force != DBGRES_FORCE_SELECTED_NO_TW_AVAILABLE ) ) {
                    wxLogMessage (
                        "dashboard_tactics_pi: Tactics true wind calculations: Forced calculation requested but no TW data as for now..");
                    m_iDbgRes_TW_Calc_Force = DBGRES_FORCE_SELECTED_NO_TW_AVAILABLE;
                } // no debug message yet
            } // else true wind not available in general
        } // else no true wind available
    } // then force true wind calculations
    else {
        if ( m_bTrueWind_available ) { // TW calculations not forced and there is TW available from instruments
            if ( ( m_iDbgRes_TW_Calc_Force != DBGRES_FORCE_NOT_SELECTED_TW_AVAILABLE ) ) {
                wxLogMessage ("dashboard_tactics_pi: Tactics true wind calculations: TW data available and no forced calculation requested.");
                m_iDbgRes_TW_Calc_Force = DBGRES_FORCE_NOT_SELECTED_TW_AVAILABLE;
            } // no debug message yet
            m_iDbgRes_TW_Calc_Lau = DBGRES_EXEC_FALSE;
            return false;
        } // the true wind is available from instruments
        else {
            if ( ( m_iDbgRes_TW_Calc_Force != DBGRES_FORCE_NOT_SELECTED_NO_TW_AVAILABLE ) ) {
                wxLogMessage ("dashboard_tactics_pi: Tactics true wind calculations: No forced calculation req., no TW data: normal proceeding.");
                m_iDbgRes_TW_Calc_Force = DBGRES_FORCE_NOT_SELECTED_NO_TW_AVAILABLE;
            }  // no debug message yet
        } // else no true wind available
    } // else user is not asking  Tactics true wind calculation
            
    // Let's check the parameters needed
    std::unique_lock<std::mutex> lckmAWAmAWS( mtxAWS );
    std::unique_lock<std::mutex> lckmHdt( mtxHdt );
    
    if ( std::isnan(mAWA) ) {
        if ( ( m_iDbgRes_TW_Calc_AWA != DBGRES_MVAL_INVALID) ) {
            wxLogMessage ("dashboard_tactics_pi: Tactics true wind calculations: Tactics has no valid internal AWA value.");
            m_iDbgRes_TW_Calc_AWA = DBGRES_MVAL_INVALID;
        } // then debug print
        m_iDbgRes_TW_Calc_Lau = DBGRES_EXEC_FALSE;
        return false;
    } // then NaN AWA
    else {
        if ( mAWA >= 0.0 ) {
            if ( ( m_iDbgRes_TW_Calc_AWA != DBGRES_MVAL_AVAILABLE) ) {
                wxLogMessage ("dashboard_tactics_pi: Tactics true wind calculations: Tactics has now an internal AWA value, (%f).", mAWA);
                m_iDbgRes_TW_Calc_AWA = DBGRES_MVAL_AVAILABLE;
            } // then debug print
        } // then valid data above zero
        else {
            if ( ( m_iDbgRes_TW_Calc_AWA != DBGRES_MVAL_IS_NEG) ) {
                wxLogMessage ("dashboard_tactics_pi: Tactics true wind calculations: Tactics has AWA value but it is negative (%f).", mAWA);
                m_iDbgRes_TW_Calc_AWA = DBGRES_MVAL_IS_NEG;
            } // then debug print
            m_iDbgRes_TW_Calc_Lau = DBGRES_EXEC_FALSE;
            return false;
        } // else data is zero
    } // else no NaN AWA

    if ( std::isnan(mAWS) ) {
        if ( ( m_iDbgRes_TW_Calc_AWS != DBGRES_MVAL_INVALID) ) {
            wxLogMessage ("dashboard_tactics_pi: Tactics true wind calculations: Tactics has no valid intenal AWS value.");
            m_iDbgRes_TW_Calc_AWS = DBGRES_MVAL_INVALID;
        } // then debug print
        m_iDbgRes_TW_Calc_Lau = DBGRES_EXEC_FALSE;
        return false;
    } // then NaN AWS
    else {
        if ( mAWS >= 0.0 ) {
            if ( ( m_iDbgRes_TW_Calc_AWS != DBGRES_MVAL_AVAILABLE) ) {
                wxLogMessage ("dashboard_tactics_pi: Tactics true wind calculations: Tactics has a valid internal AWS value (%f).", mAWS);
                m_iDbgRes_TW_Calc_AWS = DBGRES_MVAL_AVAILABLE;
            } // then debug print
        } // then valid data above zero
        else {
            if ( ( m_iDbgRes_TW_Calc_AWS != DBGRES_MVAL_IS_NEG) ) {
                wxLogMessage ("dashboard_tactics_pi: Tactics true wind calculations: Tactics has a negative internal AWS (%f).", mAWS);
                m_iDbgRes_TW_Calc_AWS = DBGRES_MVAL_IS_NEG;
            } // then debug print
            m_iDbgRes_TW_Calc_Lau = DBGRES_EXEC_FALSE;
            return false;
        } // else data is zero
    } // else no NaN AWS

    if ( std::isnan(mHdt) ) {
        if ( ( m_iDbgRes_TW_Calc_Hdt != DBGRES_MVAL_INVALID) ) {
            wxLogMessage ("dashboard_tactics_pi: Tactics true wind calculations: Tactics has no valid internal true heading value.");
            m_iDbgRes_TW_Calc_Hdt = DBGRES_MVAL_INVALID;
        } // then debug print
        m_calcTWD = NAN; // that invalidates also this
        m_iDbgRes_TW_Calc_Lau = DBGRES_EXEC_FALSE;
        return false;
    } // then NaN Hdt
    else {
        if ( ( m_iDbgRes_TW_Calc_Hdt != DBGRES_MVAL_AVAILABLE) ) {
            wxLogMessage ("dashboard_tactics_pi: Tactics true wind calculations: Tactics has an internal true heading value, now (%f).", mHdt);
            m_iDbgRes_TW_Calc_Hdt = DBGRES_MVAL_AVAILABLE;
        } // then debug print
    } // else no NaN Hdt

    if ( (mAWAUnit != _T("")) ) {
        if ( ( m_iDbgRes_TW_Calc_AWAUnit != DBGRES_MVAL_AVAILABLE ) ) {
            wxLogMessage ("dashboard_tactics_pi: Tactics true wind calculations: AWA unit is availabe, now (%s).", mAWAUnit);
            m_iDbgRes_TW_Calc_AWAUnit = DBGRES_MVAL_AVAILABLE;
        } // then debug print
    } // then valid AWA unit
    else {
        if ( ( m_iDbgRes_TW_Calc_AWAUnit != DBGRES_MVAL_IS_ZERO) ) {
                wxLogMessage ("dashboard_tactics_pi: Tactics true wind calculations: Tactics has internal AWA unit but it is empty.");
                m_iDbgRes_TW_Calc_AWAUnit = DBGRES_MVAL_IS_ZERO;
        } // then debug print
        m_iDbgRes_TW_Calc_Lau = DBGRES_EXEC_FALSE;
        return false;
    } // else unit is null string


    // Let's check that we have speed (water or ground available) - we need those for everything else but moored TWD
    if ( !( g_bForceTrueWindCalculation && m_bTrueWindAngle_available && m_bTrueWindSpeed_available && !m_bTrueWindDirection_available ) ) {
        if ( g_bUseSOGforTWCalc ) {
            if ( std::isnan(mSOG) ) {
                if ( ( m_iDbgRes_TW_Calc_SOG != DBGRES_MVAL_INVALID) ) {
                    wxLogMessage (
                        "dashboard_tactics_pi: Tactics true wind calculations: SOG calculations requested but Tactics has no valid internal SOG value.");
                    m_iDbgRes_TW_Calc_SOG = DBGRES_MVAL_INVALID;
                } // then debug print
                m_iDbgRes_TW_Calc_Lau = DBGRES_EXEC_FALSE;
                return false;
            } // then NaN SOG
            else {
                if ( mSOG > 0.0 ) {
                    if ( ( m_iDbgRes_TW_Calc_SOG != DBGRES_MVAL_AVAILABLE) ) {
                        wxLogMessage (
                            "dashboard_tactics_pi: Tactics true wind calculations: SOG calc. requested, a valid internal SOG value, now (%f).", mSOG);
                        m_iDbgRes_TW_Calc_SOG = DBGRES_MVAL_AVAILABLE;
                    } // then debug print
                } // then valid data above zero
                else {
                    if ( ( m_iDbgRes_TW_Calc_SOG != DBGRES_MVAL_IS_ZERO) ) {
                        wxLogMessage ("dashboard_tactics_pi: Tactics true wind calculations: SOG calc.request. but value is 0 or negative (%f).", mSOG);
                        m_iDbgRes_TW_Calc_SOG = DBGRES_MVAL_IS_ZERO;
                    } // then debug print
                    m_iDbgRes_TW_Calc_Lau = DBGRES_EXEC_FALSE;
                    return false;
                } // else data is zero
            } // else no NaN SOG
        } // then use SOG
        else {
            if ( std::isnan(mStW) ) {
                if ( ( m_iDbgRes_TW_Calc_StW != DBGRES_MVAL_INVALID) ) {
                    wxLogMessage (
                        "dashboard_tactics_pi: Tactics true wind calculations: StW calculations requested but Tactics has no valid StW value.");
                    m_iDbgRes_TW_Calc_StW = DBGRES_MVAL_INVALID;
                } // then debug print
                m_iDbgRes_TW_Calc_Lau = DBGRES_EXEC_FALSE;
                return false;
            } // then NaN StW
            else {
                if ( mStW > 0.0 ) {
                    if ( ( m_iDbgRes_TW_Calc_StW != DBGRES_MVAL_AVAILABLE) ) {
                        wxLogMessage (
                            "dashboard_tactics_pi: Tactics true wind calculations: StW calc. requested, a valid internal StW value, now (%f).", mStW);
                        m_iDbgRes_TW_Calc_StW = DBGRES_MVAL_AVAILABLE;
                    } // then debug print
                } // then valid data above zero
                else {
                    if ( ( m_iDbgRes_TW_Calc_StW != DBGRES_MVAL_IS_ZERO) ) {
                        wxLogMessage ("dashboard_tactics_pi: Tactics true wind calculations: StW calc. request but value is 0 or neg. (%f)", mStW);
                        m_iDbgRes_TW_Calc_StW = DBGRES_MVAL_IS_ZERO;
                    } // then debug print
                    m_iDbgRes_TW_Calc_Lau = DBGRES_EXEC_FALSE;
                    return false;
                } // else data is zero
            } // else STW is not NaN
        } // else STW speed calculations
    } // then not TWD calculations in moored conditions where TWA/TWS are known
        
    // all OK!
    if ( ( m_iDbgRes_TW_Calc_Lau != DBGRES_EXEC_TRUE) ) {
        if ( m_iDbgRes_TW_Calc_Exe == DBGRES_EXEC_TRUE )
            wxLogMessage ("dashboard_tactics_pi: Tactics true wind calculations: checks OK, execution of algorithm will continue.", mStW);
        else
            wxLogMessage ("dashboard_tactics_pi: Tactics true wind calculations: checks OK, execution of algorithm will be launched.", mStW);
        m_iDbgRes_TW_Calc_Lau = DBGRES_EXEC_TRUE;
    } // then debug print
    return true;
}

/********************************************************************
Likewise to SendSentenceToAllInstruments_PerformanceCorrections(),
this method is to be called from the derived class which implements
SendSentenceToAllInstruments() - if Tactics is requested to calculate
the true wind, and it is available and
values up to date and send those values to instrument windows as well.
*********************************************************************/
bool tactics_pi::SendSentenceToAllInstruments_GetCalculatedTrueWind(
    unsigned long long st, double value, wxString unit,
    unsigned long long &st_twa, double &value_twa, wxString &unit_twa,
    unsigned long long &st_tws, unsigned long long &st_tws2, double &value_tws, wxString &unit_tws,
    unsigned long long &st_twd, double &value_twd, wxString &unit_twd,
    long long &calctimestamp
    )
{
    double spdval;

    if ( st != OCPN_DBP_STC_AWS )
        return false;

    /* Sometimes useful to debug why it does not enter below */
    /*
    wxLogMessage ( "tactics_pi::SendSentenceToAllInstruments_GetCalculatedTrueWind() - g_bUseSOGforTWCalc %s, mSOG %f, mStW %f .",
                   (g_bUseSOGforTWCalc?"true":"false"), (std::isnan(mSOG)?999.99:mSOG), (std::isnan(mStW)?999.99:mStW) );
    */

    // Let's first check if we've been asked to calculate the TWD value in moored conditions, where TWA and TWS are provided
    if ( g_bForceTrueWindCalculation && m_bTrueWindAngle_available && m_bTrueWindSpeed_available && !m_bTrueWindDirection_available ) {

        double stillval = 0.0; // supposed to be moored, otherwise normal calculations can be done
        if ( !std::isnan(mSOG) )
            stillval = mSOG;
        else if ( !std::isnan(mStW) )
            stillval = mStW;

        if ( stillval < 0.2 ) {
        
            if ( std::isnan(mTWA) || std::isnan(mTWS) || !(mAWAUnit != _T("")) || std::isnan(mHdt) ) {
                m_calcTWD = NAN;
                m_iDbgRes_TW_Calc_Exe = DBGRES_EXEC_FALSE;
                return false;
            } // then (still or again) invalid values, cannot progress
            // Allright, now we can start the calculations
            std::unique_lock<std::mutex> lckmAWSmAWA( mtxAWS ); //
            std::unique_lock<std::mutex> lckmHdt( mtxHdt );
            std::unique_lock<std::mutex> lckmTWD( mtxTWD );
            mTWD = (mAWAUnit == _T("\u00B0rl")) ? mHdt + mTWA : mHdt - mTWA;
            if (mTWD >= 360) mTWD -= 360;
            if (mTWD < 0) mTWD += 360;
            m_calcTWD = mTWD;
            st_twa = OCPN_DBP_STC_TWA;
            value_twa = mTWA;
            unit_twa = mAWAUnit;
            st_tws = OCPN_DBP_STC_TWS;
            st_tws2 = OCPN_DBP_STC_TWS2;
            value_tws = mTWS;
            unit_tws = mAWSUnit;
            st_twd = OCPN_DBP_STC_TWD;
            value_twd = mTWD;
            unit_twd = _T("\u00B0T");
            wxLongLong wxllNowMs = wxGetUTCTimeMillis();
            calctimestamp = wxllNowMs.GetValue();
            if ( ( m_iDbgRes_TW_Calc_Exe != DBGRES_EXEC_TWDONLY_TRUE ) ) {
                wxLogMessage (
                    "dashboard_tactics_pi: Tactics true wind calculations: standstill TWD only requested, "
                    "returning now TWA %f '%s', TWS %f '%s, TWD %f '%s'.",
                    (std::isnan(value_twa)?999.99:value_twa), unit_twa,
                    (std::isnan(value_tws)?999.99:value_tws), unit_tws,
                    (std::isnan(value_twd)?999.99:value_twd), unit_twd );
                m_iDbgRes_TW_Calc_Exe = DBGRES_EXEC_TWDONLY_TRUE;
            } // then debug print
            return true;
        } // then we are at standstill, or almost; got TWS/TWA but no TWD; calculate it from TWA/HDT if available
        
    } // then force calculate TWD forced in moored conditions when TWA and TWS are known
    
    if ( g_bUseSOGforTWCalc ) {
        if ( std::isnan(mSOG) ) {
            m_iDbgRes_TW_Calc_Exe = DBGRES_EXEC_FALSE;
            return false;
        } // then mSOG is not valid
    } // then use SOG, check if value
    else {
        if ( std::isnan(mStW) ) {
            m_iDbgRes_TW_Calc_Exe = DBGRES_EXEC_FALSE;
            return false;
        } // then mStW is not valid
    } // else use StW, check if value
    spdval = (g_bUseSOGforTWCalc) ? mSOG : mStW ;

    //  Calculate TWS (from AWS and StW/SOG)
    std::unique_lock<std::mutex> lckmAWSmAWA( mtxAWS ); //
    std::unique_lock<std::mutex> lckmHdt( mtxHdt );
    /* The below is the single most important debugging tool for this method! We may need it again */
    /*
    wxLogMessage ( "tactics_pi::SendSentenceToAllInstruments_GetCalculatedTrueWind() - mStW %f, mSOG %f, spdval %f, m_bTrueWind_available %s, g_bForceTrueWindCalculation %s, mAWA %f, mAWS %f, mAWAUnit '%s', mHdt %f .",
                   (std::isnan(mStW)?999.99:mStW), (std::isnan(mSOG)?999.99:mSOG), (std::isnan(spdval)?999.99:spdval),
                   (m_bTrueWind_available?"true":"false"),(g_bForceTrueWindCalculation?"true":"false"),
                   (std::isnan(mAWA)?999.99:mAWA), (std::isnan(mAWS)?999.99:mAWS),
                   mAWAUnit, (std::isnan(mHdt)?999.99:mHdt) );
    */
    // only start calculating if we have a full set of valid data!
    if ( std::isnan(mAWA) || std::isnan(mAWS) || std::isnan(mHdt) ) {
        m_calcTWS = NAN;
        m_calcTWD = NAN;
        m_calcTWA = NAN;
        m_iDbgRes_TW_Calc_Exe = DBGRES_EXEC_FALSE;
        return false;
    } // then (still or again) invalid values, cannot progress
    if ( (m_bTrueWind_available && !g_bForceTrueWindCalculation) ||
         !(mAWA >= 0.0) || !(mAWS >= 0.0) || !(spdval > 0.0) || !(mAWAUnit != _T("")) ) {
        m_calcTWS = NAN;
        m_calcTWD = NAN;
        m_calcTWA = NAN; 
        m_iDbgRes_TW_Calc_Exe = DBGRES_EXEC_FALSE;
        return false;
    } // then calculation is not wanted/usable or valid values but zero value
    //we have to do the calculation in knots
    double aws_kts = fromUsrSpeed_Plugin(mAWS,
                                         g_iDashWindSpeedUnit);
    if ( std::isnan( aws_kts ) ) {
        m_calcTWS = NAN;
        m_calcTWD = NAN;
        m_calcTWA = NAN;
        m_iDbgRes_TW_Calc_Exe = DBGRES_EXEC_FALSE;
        return false;
    } // then the AWS kts conversion by O failed, returning NaN, no speed value, cannot continue
    spdval = fromUsrSpeed_Plugin(spdval, g_iDashSpeedUnit);
    if ( std::isnan( spdval ) ) {
        m_calcTWS = NAN;
        m_calcTWD = NAN;
        m_calcTWA = NAN;
        m_iDbgRes_TW_Calc_Exe = DBGRES_EXEC_FALSE;
        return false;
    } // then the speed kts conversion by O failed, returning NaN, no speed value, cannot continue
    // Allright, now we can start the calculations
    std::unique_lock<std::mutex> lckmTWAmTWS( mtxTWS );
    std::unique_lock<std::mutex> lckmTWD( mtxTWD );
    mTWA = 0;
    mTWD = 0.;
    if (mAWA < 180.) {
        mTWA = 90. - (180. / M_PI*atan((aws_kts*cos(mAWA*M_PI / 180.) - spdval) / (aws_kts*sin(mAWA*M_PI / 180.))));
    }
    else if (mAWA > 180.) {
        mTWA = 360. - (90. - (180. / M_PI*atan((aws_kts*cos((180. - (mAWA - 180.))*M_PI / 180.) - spdval) / (aws_kts*sin((180. - (mAWA - 180.))*M_PI / 180.)))));
    }
    else {
        mTWA = 180.;
    }
    mTWS = sqrt(pow((aws_kts*cos(mAWA*M_PI / 180.)) - spdval, 2) + pow(aws_kts*sin(mAWA*M_PI / 180.), 2));
    /* ToDo: adding leeway needs to be reviewed, as the direction
       of the bow is still based on the magnetic compass,
       no matter if leeway or not ...
       if (!std::isnan(mLeeway) && g_bUseHeelSensor) { //correct TWD with Leeway if heel is available. Makes only sense with heel sensor
       mTWD = (mAWAUnit == _T("\u00B0rl")) ? mHdt + mTWA + mLeeway : mHdt - mTWA + mLeeway;
       }
       else*/
    mTWD = (mAWAUnit == _T("\u00B0rl")) ? mHdt + mTWA : mHdt - mTWA;
    //endif
    if (mTWD >= 360) mTWD -= 360;
    if (mTWD < 0) mTWD += 360;
    //convert mTWS back to user wind speed settings
    mTWS = toUsrSpeed_Plugin(mTWS, g_iDashWindSpeedUnit);
    m_calcTWS = mTWS;
    m_calcTWD = mTWD;
    m_calcTWA = mTWA;
    if (mAWSUnit == _T(""))
        mAWSUnit = mAWAUnit;
    mTWD_Watchdog = twd_watchdog_timeout_ticks;
    st_twa = OCPN_DBP_STC_TWA;
    value_twa = mTWA;
    unit_twa = mAWAUnit;
    st_tws = OCPN_DBP_STC_TWS;
    st_tws2 = OCPN_DBP_STC_TWS2;
    value_tws = mTWS;
    unit_tws = mAWSUnit;
    st_twd = OCPN_DBP_STC_TWD;
    value_twd = mTWD;
    unit_twd = _T("\u00B0T");
    wxLongLong wxllNowMs = wxGetUTCTimeMillis();
    calctimestamp = wxllNowMs.GetValue();
    if ( ( m_iDbgRes_TW_Calc_Exe != DBGRES_EXEC_TRUE ) ) {
        wxLogMessage (
            "dashboard_tactics_pi: Tactics true wind calculations: algorithm is running and returning now TWA %f '%s', TWS %f '%s, TWD %f '%s'.",
            (std::isnan(value_twa)?999.99:value_twa), unit_twa,
            (std::isnan(value_tws)?999.99:value_tws), unit_tws,
            (std::isnan(value_twd)?999.99:value_twd), unit_twd );
        m_iDbgRes_TW_Calc_Exe = DBGRES_EXEC_TRUE;
    } // then debug print
    return true;
}


/********************************************************************
set system variables
*********************************************************************/
void tactics_pi::SetCalcVariables(
    unsigned long long st, double value, wxString unit)
{
    std::unique_lock<std::mutex> lckmAWA( mtxAWS,std::defer_lock ); // shares mutex with AWS
    std::unique_lock<std::mutex> lckmAWS( mtxAWS,std::defer_lock );
    std::unique_lock<std::mutex> lckmTWA( mtxTWD,std::defer_lock ); // shares mutex with TWD
    std::unique_lock<std::mutex> lckmTWS( mtxTWS,std::defer_lock );
    std::unique_lock<std::mutex> lckmTWD( mtxTWD,std::defer_lock );
    std::unique_lock<std::mutex> lckmHdt( mtxHdt,std::defer_lock );
    std::unique_lock<std::mutex> lckmBRG( mtxBRG,std::defer_lock );
    switch (st) {
    case OCPN_DBP_STC_AWA:
        if ( lckmAWA.try_lock() ) {
            mAWA = value;
            mAWAUnit = unit;
        } // then no on-going calculations
        break;
    case OCPN_DBP_STC_AWS:
        if ( lckmAWS.try_lock() ) {
            mAWS = value;
            mAWSUnit = unit;
        } // then no on-going calculations
        break;
    case  OCPN_DBP_STC_TWA:
        if ( lckmTWA.try_lock() ) {
            if (g_bForceTrueWindCalculation && !std::isnan(m_calcTWA)){ //otherwise we distribute the original O TWA
                mTWA = m_calcTWA;
            }
            else
                mTWA = value;
        } // then no on-going calculations
        break;
    case  OCPN_DBP_STC_TWS:
        if ( lckmTWS.try_lock() ) {
            if (g_bForceTrueWindCalculation && !std::isnan(m_calcTWS)){ //otherwise we distribute the original O TWS
            mTWS = m_calcTWS;
            }
            else
                mTWS = value;
        } // then no on-going calculations
        break;
    case  OCPN_DBP_STC_TWD:
        if ( lckmTWD.try_lock() ) {
            if (g_bForceTrueWindCalculation && !std::isnan(m_calcTWD)){ //otherwise we distribute the original O TWD
                mTWD = m_calcTWD;
            }
            else
                mTWD = value;
        } // then no on-going calculations
        break;
    case OCPN_DBP_STC_STW:
        mStW = value;
        break;
    case OCPN_DBP_STC_HDT:
        // lock this variable against derived class watchdog provoked NaN if calculations are on-going
        if ( lckmHdt.try_lock() )
            mHdt = value;
        break;
    case OCPN_DBP_STC_HEEL:
        if (g_bUseHeelSensor){
            mheel = value;
            mHeelUnit = unit;
        }
        msensorheel = value; //TR TEMP for testing !
        break;
    case OCPN_DBP_STC_COG:
        mCOG = value;
        break;
    case OCPN_DBP_STC_SOG:
        mSOG = value;
        break;
    case OCPN_DBP_STC_LAT:
        mlat = value;
        break;
    case OCPN_DBP_STC_LON:
        mlon = value;
        break;
    case OCPN_DBP_STC_CURRDIR:
        m_CurrentDirection = value;
        break;
    case OCPN_DBP_STC_CURRSPD:
        m_ExpSmoothCurrSpd = value;
        break;
    case OCPN_DBP_STC_BRG:
        // lock this variable against derived class watchdog provoked NaN if calculations are on-going
        if ( lckmBRG.try_lock() )
            mBRG = value;
        break;
    default:
        break;
    }
    if (g_bManHeelInput){
        mHeelUnit = (mAWAUnit == _T("\u00B0lr")) ? _T("\u00B0r") : _T("\u00B0l");
        g_dheel[0][0] = g_dheel[1][0] = g_dheel[2][0] = g_dheel[3][0] = g_dheel[4][0] = g_dheel[5][0] = g_dheel[0][1] = g_dheel[0][2] = g_dheel[0][3] = g_dheel[0][4] = 0.0;
        if (std::isnan(mTWS)) mTWS = 0;
        if (std::isnan(mTWA)) mTWA = 0;
        int twsmin = (int)(mTWS / 5);
        int twsmax = twsmin + 1;
        int twamin = (int)(mTWA / 45);
        int twamax = twamin + 1;
        double tws1 = twsmin*5.;
        double tws2 = twsmax * 5.;
        double twa1 = twamin * 45.;
        double twa2 = twamax * 45.;


        double twsfact = (mTWS - tws1) / (tws2 - tws1);
        double twafact = (mTWA - twa1) / (twa2 - twa1);
        double heel1 = g_dheel[twsmin][twamin] + twsfact*(g_dheel[twsmax][twamin] - g_dheel[twsmin][twamin]);
        double heel2 = g_dheel[twsmin][twamax] + twsfact*(g_dheel[twsmax][twamax] - g_dheel[twsmin][twamax]);

        mheel = heel1 + twafact*(heel2 - heel1);
        if (mHeelUnit == _T("\u00B0l")) mheel = -mheel;
    }
    if (!std::isnan(mLeeway)){
        if (mLeeway >= -90 && mLeeway <= 90)
            m_LeewayOK = true;
    }
}

/********************************************************************
Likewise to SendSentenceToAllInstruments_PerformanceCorrections(),
this method is to be called from the derived class which implements
SendSentenceToAllInstruments() - if Tactics is requested to calculate
the the leeway from heel we need to send those values to instruments
if the return value is true.
*********************************************************************/
bool tactics_pi::SendSentenceToAllInstruments_GetCalculatedLeeway(
    unsigned long long &st_leeway, double &value_leeway,
    wxString &unit_leeway, long long &calctimestamp)
{
    bool calculatedLeeway = false;

    if (g_bUseFixedLeeway){
        mHeelUnit =
            (mAWAUnit == _T("\u00B0lr")) ? _T("\u00B0r") : _T("\u00B0l");
        mLeeway = g_dfixedLeeway;
        if (std::isnan(mheel)) mheel = 0;

        if (mHeelUnit == _T("\u00B0l") && mLeeway > 0) mLeeway = -mLeeway;
        if (mHeelUnit == _T("\u00B0r") && mLeeway < 0) mLeeway = -mLeeway;
        calculatedLeeway = true;
    }

    else {//g_bUseHeelSensor or g_bManHeelInput

        // only start calculating if we have a full set of data
        if (!std::isnan(mheel) && !std::isnan(mStW)) {
            double stwvalue;
            ( std::isnan(mStWnocorr) ? stwvalue = mStW : stwvalue = mStWnocorr );
            double stw_kts = fromUsrSpeed_Plugin(stwvalue, g_iDashSpeedUnit);

            // calculate Leeway based on Heel
            if (mheel == 0)
                mLeeway = 0;
            else if (mStW == 0)
                mLeeway = g_dfixedLeeway;
            else
                mLeeway = (g_dLeewayFactor*mheel) / (stw_kts*stw_kts);
            if (mLeeway > g_dfixedLeeway) mLeeway = g_dfixedLeeway;
            if (mLeeway < -g_dfixedLeeway) mLeeway = -g_dfixedLeeway;
            //22.04TR : auf neg. Werte prufen !!!
            mHeelUnit = (mheel < 0) ? _T("\u00B0l") : _T("\u00B0r");
            calculatedLeeway = true;
        }
    }
    if ( calculatedLeeway ) {
        st_leeway = OCPN_DBP_STC_LEEWAY;
        value_leeway = mLeeway;
        unit_leeway = mHeelUnit;
        wxLongLong wxllNowMs = wxGetUTCTimeMillis();
        calctimestamp = wxllNowMs.GetValue();
        return true;
    }
    return false;
}

/**********************************************************************
Likewise to SendSentenceToAllInstruments_PerformanceCorrections(),
this method is to be called from the derived class which implements
SendSentenceToAllInstruments() - if Tactics is requested to calculate
the the leeway from heel we need to send those values to instruments
if the return value is true:
Calculate Current with DES
using COG/SOG-Vector + HDT/STW-Vector
from the heel sensor we get the boat drift due to heel = Leeway
the whole drift angle btw. COG and HDT is a mixture of Leeway  + current
first we apply leeway to HDT, and get CRS = Course through water
The remaining diff btw. CRS & COG is current
based on actual position, we calculate lat/lon of the endpoints of both vectors  (COG/HDT = direction, SOG/STW = length)
then we take lat/lon from the endpoints of these vectors and calculate current between them with length (=speed) and direction
return the endpoint of SOG,COG

ToDo :
we're already calculating in "user speed data", but the exp. smoothing is done on the value itself.
On unit-change, the smoothed data values need to be converted from "old" to "new"
************************************************************************/
bool tactics_pi::SendSentenceToAllInstruments_GetCalculatedCurrent(
    unsigned long long st, double value, wxString unit,
    unsigned long long &st_currdir, double &value_currdir,
    wxString &unit_currdir,
    unsigned long long &st_currspd, double &value_currspd,
    wxString &unit_currspd, long long &calctimestamp)
{

    //don't calculate on ALL incoming sentences ...
    if (st == OCPN_DBP_STC_HDT) {

        // ... and only start calculating if we have a full set of data
        std::unique_lock<std::mutex> lckmHdt( mtxHdt );

        if (!std::isnan(mheel) && m_LeewayOK && !std::isnan(mCOG) &&
            !std::isnan(mSOG) && !std::isnan(mStW) && !std::isnan(mHdt) &&
            !std::isnan(mlat) && !std::isnan(mlon) && !std::isnan(mLeeway)) {

            double COGlon, COGlat;
            //we have to do the calculation in knots ...
            double sog_kts, stw_kts;
            sog_kts = fromUsrSpeed_Plugin(mSOG, g_iDashSpeedUnit);
            if ( g_bCorrectSTWwithLeeway )
                stw_kts = fromUsrSpeed_Plugin(mStW, g_iDashSpeedUnit);
            else
                ( std::isnan( mStWnocorr ) ?
                  stw_kts = fromUsrSpeed_Plugin(mStW, g_iDashSpeedUnit) :
                  stw_kts = fromUsrSpeed_Plugin(mStWnocorr, g_iDashSpeedUnit) );
            //calculate endpoint of COG/SOG
            PositionBearingDistanceMercator_Plugin(
                mlat, mlon, mCOG, sog_kts, &COGlat, &COGlon);

            //------------------------------------
            //correct HDT with Leeway
            //------------------------------------

            /*         ^ Hdt, STW
             * Wind   /
             * -->   /        Leeway
             *      /
             *     /----------> CRS, STW (stw_corr or stw)
             *     \
             *      \        Current
             *       \ COG,SOG
             *        V
             */

            // if wind is from port, heel & mLeeway will be positive (to starboard), adding degrees on the compass rose
            //  CRS = Hdt + Leeway
            //  if wind is from starboard, heel/mLeeway are negative (to port), mLeeway has to be substracted from Hdt
            //   As mLeeway is a signed double, so we can generally define : CRS = Hdt + mLeeway
            double CourseThroughWater = mHdt + mLeeway;
            if (CourseThroughWater >= 360) CourseThroughWater -= 360;
            if (CourseThroughWater < 0) CourseThroughWater += 360;
            double CRSlat, CRSlon;
            //calculate endpoint of StW/KdW;
            PositionBearingDistanceMercator_Plugin(mlat, mlon, CourseThroughWater, stw_kts, &CRSlat, &CRSlon);

            //calculate the Current vector with brg & speed from the 2 endpoints above
            double currdir = 0, currspd = 0;
            //currdir = local_bearing(StWlat, StWlon, COGlat, COGlon );
            //currspd = local_distance(COGlat, COGlon, StWlat, StWlon);
            DistanceBearingMercator_Plugin(COGlat, COGlon, CRSlat, CRSlon, &currdir, &currspd);
            // double exponential smoothing on currdir / currspd
            if (currspd < 0) currspd = 0;
            if (std::isnan(m_ExpSmoothCurrSpd))
                m_ExpSmoothCurrSpd = currspd;
            if (std::isnan(m_ExpSmoothCurrDir))
                m_ExpSmoothCurrDir = currdir;

            double currdir_tan = currdir;
            mExpSmoothCurrSpd->SetAlpha(alpha_currspd);
            m_ExpSmoothCurrSpd = mExpSmoothCurrSpd->GetSmoothVal(currspd);

            double rad = (90 - currdir_tan)*M_PI / 180.;
            mSinCurrDir->SetAlpha(g_dalpha_currdir);
            mCosCurrDir->SetAlpha(g_dalpha_currdir);
            m_ExpSmoothSinCurrDir = mSinCurrDir->GetSmoothVal(sin(rad));
            m_ExpSmoothCosCurrDir = mCosCurrDir->GetSmoothVal(cos(rad));
            m_CurrentDirection = (90. - (atan2(m_ExpSmoothSinCurrDir, m_ExpSmoothCosCurrDir)*180. / M_PI) + 360.);
            while (m_CurrentDirection >= 360) m_CurrentDirection -= 360;
            st_currdir = OCPN_DBP_STC_CURRDIR;
            value_currdir = m_CurrentDirection;
            unit_currdir = _T("\u00B0");
            st_currspd = OCPN_DBP_STC_CURRSPD;
            value_currspd = toUsrSpeed_Plugin(
                m_ExpSmoothCurrSpd, g_iDashSpeedUnit);
            unit_currspd = getUsrSpeedUnit_Plugin(g_iDashSpeedUnit);
            wxLongLong wxllNowMs = wxGetUTCTimeMillis();
            calctimestamp = wxllNowMs.GetValue();
            return true;
        }
        else{
            m_CurrentDirection = NAN;
            m_ExpSmoothCurrSpd = NAN;
        }
    }
    return false;
}

void tactics_pi::TacticsSetCursorLatLon(double lat, double lon)
{
	g_dcur_lat = lat;
	g_dcur_lon = lon;
}

/**********************************************************************
Set MARK Position
***********************************************************************/
void tactics_pi::TacticsOnContextMenuItemCallback(int id)
{
    m_pMark = new PlugIn_Waypoint(
        g_dcur_lat, g_dcur_lon, _T("activepoint"), g_sMarkGUID, g_sMarkGUID);
    g_dmark_lat = m_pMark->m_lat;
    g_dmark_lon = m_pMark->m_lon;
    AddSingleWaypoint(m_pMark, false);
    m_pMark->m_CreateTime = wxDateTime::Now().GetTm();
}

/***********************************************************************
Central routine to calculate the polar based performance data,
independent of the use of any instrument or setting
***********************************************************************/
void tactics_pi::CalculatePerformanceData(void)
{
    if ( !BoatPolar->isValid() ) {
        if ( g_iDbgRes_Polar_Status != DBGRES_POLAR_INVALID ) {
            wxLogMessage ("dashboard_tactics_pi: >>> Missing or invalid Polar file: no Performance data, Laylines, Polar graphs available. <<<");
            g_iDbgRes_Polar_Status = DBGRES_POLAR_INVALID;
        } // then debug print
        return;
    } // then no polar or it is not valid
    g_iDbgRes_Polar_Status = DBGRES_POLAR_VALID;
    
    std::unique_lock<std::mutex> lckmTWAmTWS( mtxTWS ); // lock both TWA and TWS
    std::unique_lock<std::mutex> lckmAWAmAWS( mtxAWS ); // shares mutex with AWS
    std::unique_lock<std::mutex> lckmBRG( mtxBRG );
    std::unique_lock<std::mutex> lckmHdt( mtxHdt );
    if (std::isnan(mTWA) || std::isnan(mTWS)) {
        return;
    }

    mPolarTargetSpeed = BoatPolar->GetPolarSpeed(mTWA, mTWS);

    //transfer targetangle dependent on AWA, not TWA
    if (mAWA <= 90)
        tvmg = BoatPolar->Calc_TargetVMG(60, mTWS);
    else
        tvmg = BoatPolar->Calc_TargetVMG(120, mTWS);

    // get Target VMG Angle from Polar
    //tvmg = BoatPolar->Calc_TargetVMG(mTWA, mTWS);
    if (tvmg.TargetSpeed > 0 && !std::isnan(mStW)) {
        double VMG = BoatPolar->Calc_VMG(mTWA, mStW);
        mPercentTargetVMGupwind = mPercentTargetVMGdownwind = 0;
        if (mTWA < 90){
            mPercentTargetVMGupwind = fabs(VMG / tvmg.TargetSpeed * 100.);
        }
        if (mTWA > 90){
            mPercentTargetVMGdownwind = fabs(VMG / tvmg.TargetSpeed * 100.);
        }
        //mVMGGain = 100.0 - mStW/tvmg.TargetSpeed  * 100.;
        mVMGGain = 100.0 - VMG / tvmg.TargetSpeed  * 100.;
    }
    else
    {
        mPercentTargetVMGupwind = mPercentTargetVMGdownwind = 0;
        mVMGGain = 0;
    }
    if (tvmg.TargetAngle >= 0 && tvmg.TargetAngle < 360) {
        mVMGoptAngle = getSignedDegRange(mTWA, tvmg.TargetAngle);
    }
    else
        mVMGoptAngle = 0;

    if (mBRG >= 0 && !std::isnan(mHdt) && !std::isnan(mStW) && !std::isnan(mTWD)){
        tcmg = BoatPolar->Calc_TargetCMG(mTWS, mTWD, mBRG);
        double actcmg = BoatPolar->Calc_CMG(mHdt, mStW, mBRG);
        // mCMGGain = (tcmg.TargetSpeed >0) ? (100.0 - mStW / tcmg.TargetSpeed *100.) : 0.0;
        mCMGGain = (tcmg.TargetSpeed >0) ? (100.0 - actcmg / tcmg.TargetSpeed *100.) : 0.0;
        if (tcmg.TargetAngle >= 0 && tcmg.TargetAngle < 360) {
            mCMGoptAngle = getSignedDegRange(mTWA, tcmg.TargetAngle);
        }
        else
            mCMGoptAngle = 0;

    }
    lckmTWAmTWS.unlock();
    lckmAWAmAWS.unlock();
    lckmBRG.unlock();
    lckmHdt.unlock();
    CalculatePredictedCourse();
}

/**********************************************************************
Calculates the predicted course/speed on the other tack and stores it
in the variables mPredictedCoG, mPredictedSoG
**********************************************************************/
void tactics_pi::CalculatePredictedCourse(void)
{
    std::unique_lock<std::mutex> lckmTWAmTWS( mtxTWS ); // lock both TWA and TWS
    std::unique_lock<std::mutex> lckmAWAmAWS( mtxAWS ); // shares mutex with AWS
    std::unique_lock<std::mutex> lckmBRG( mtxBRG );
    std::unique_lock<std::mutex> lckmHdt( mtxHdt );
    double predictedKdW; //==predicted Course Through Water
    if (!std::isnan(mStW) && !std::isnan(mHdt) && !std::isnan(mTWA) && !std::isnan(mlat) && !std::isnan(mlon) && !std::isnan(mLeeway) && !std::isnan(m_CurrentDirection) && !std::isnan(m_ExpSmoothCurrSpd)){
      //New: with BearingCompass in Head-Up mode = Hdt
      double Leeway = (mHeelUnit == _T("\u00B0lr")) ? -mLeeway : mLeeway;
      //todo : assuming TWAunit = AWAunit ...
      if (mAWAUnit == _T("\u00B0lr")){ //currently wind is from port, target is from starboard ...
        predictedKdW = mHdt - 2 * mTWA - Leeway;
      }
      else if (mAWAUnit == _T("\u00B0rl")){ //so, currently wind from starboard
        predictedKdW = mHdt + 2 * mTWA - Leeway;
      }
      else {
        predictedKdW = (mTWA < 10) ? 180 : 0; // should never happen, but is this correct ???
      }
      if (predictedKdW >= 360) predictedKdW -= 360;
      if (predictedKdW < 0) predictedKdW += 360;
      double predictedLatHdt, predictedLonHdt, predictedLatCog, predictedLonCog;
      //double predictedCoG;
      //standard triangle calculation to get predicted CoG / SoG
      //get endpoint from boat-position by applying  KdW, StW
      PositionBearingDistanceMercator_Plugin(mlat, mlon, predictedKdW, mStW, &predictedLatHdt, &predictedLonHdt);
      //wxLogMessage(_T("Step1: m_lat=%f,m_lon=%f, predictedKdW=%f,m_StW=%f --> predictedLatHdt=%f,predictedLonHdt=%f\n"), m_lat, m_lon, predictedKdW, m_StW, predictedLatHdt, predictedLonHdt);
      //apply surface current with direction & speed to endpoint from above
      PositionBearingDistanceMercator_Plugin(predictedLatHdt, predictedLonHdt, m_CurrentDirection, m_ExpSmoothCurrSpd, &predictedLatCog, &predictedLonCog);
      //wxLogMessage(_T("Step2: predictedLatHdt=%f,predictedLonHdt=%f, m_CurrDir=%f,m_CurrSpeed=%f --> predictedLatCog=%f,predictedLonCog=%f\n"), predictedLatHdt, predictedLonHdt, m_CurrDir, m_CurrSpeed, predictedLatCog, predictedLonCog);
      //now get predicted CoG & SoG as difference between the 2 endpoints (coordinates) from above
      DistanceBearingMercator_Plugin(predictedLatCog, predictedLonCog, mlat, mlon, &mPredictedCoG, &mPredictedSoG);
    }
}

/*********************************************************************
NMEA $PNKEP (NKE style) performance data
**********************************************************************/
void tactics_pi::ExportPerformanceData(void)
{
	//PolarTargetSpeed
	if (g_bExpPerfData01 && !std::isnan(mPolarTargetSpeed)){
		createPNKEP_NMEA(1, mPolarTargetSpeed, mPolarTargetSpeed  * 1.852, 0, 0);
	}
	//todo : extract mPredictedCoG calculation from layline.calc and add to CalculatePerformanceData
	if (g_bExpPerfData02 && !std::isnan(mPredictedCoG)){
		createPNKEP_NMEA(2, mPredictedCoG, 0, 0, 0); // course (CoG) on other tack
	}
	//Target VMG angle, act. VMG % upwind, act. VMG % downwind
	if (g_bExpPerfData03 && !std::isnan(tvmg.TargetAngle) && tvmg.TargetSpeed > 0){
		createPNKEP_NMEA(3, tvmg.TargetAngle, mPercentTargetVMGupwind, mPercentTargetVMGdownwind, 0);
	}
	//Gain VMG de 0 � 999%, Angle pour optimiser le VMG de 0 � 359�,Gain CMG de 0 � 999%,Angle pour optimiser le CMG de 0 � 359�
	if (g_bExpPerfData04)
		createPNKEP_NMEA(4, mCMGoptAngle, mCMGGain, mVMGoptAngle, mVMGGain);
	//current direction, current speed kts, current speed in km/h,
	if (g_bExpPerfData05 && !std::isnan(m_CurrentDirection) && !std::isnan(m_ExpSmoothCurrSpd)){
		createPNKEP_NMEA(5, m_CurrentDirection, m_ExpSmoothCurrSpd, m_ExpSmoothCurrSpd  * 1.852, 0);
	}
}

void tactics_pi::createPNKEP_NMEA(int sentence, double data1, double data2, double data3, double data4)
{
	wxString nmeastr = "";
	switch (sentence)
	{
	case 0:
		//strcpy(nmeastr, "$PNKEPA,");
		break;
	case 1:
		nmeastr = _T("$PNKEP,01,") + wxString::Format("%.2f,N,", data1) + wxString::Format("%.2f,K", data2);
		break;
	case 2:
		/*course on next tack(code PNKEP02)
		$PNKEP, 02, x.x*hh<CR><LF>
		\ Cap sur bord Oppos� / prochain bord de 0 � 359�*/
		nmeastr = _T("$PNKEP,02,") + wxString::Format("%.1f", data1);
		break;
	case 3:
		/*    $PNKEP, 03, x.x, x.x, x.x*hh<CR><LF>
		|    |     \ performance downwind from 0 to 99 %
		|     \ performance upwind from 0 to 99 %
		\ opt.VMG angle  0 � 359�  */
		nmeastr = _T("$PNKEP,03,") + wxString::Format("%.1f,", data1) + wxString::Format("%.1f,", data2) + wxString::Format("%.1f", data3);
		break;
	case 4:
		/*Calculates the gain for VMG & CMG and stores it in the variables
		mVMGGain, mCMGGain,mVMGoptAngle,mCMGoptAngle
		Gain is the percentage btw. the current boat speed mStW value and Target-VMG/CMG
		Question : shouldn't we compare act.VMG with Target-VMG ? To be investigated ...
		$PNKEP, 04, x.x, x.x, x.x, x.x*hh<CR><LF>
		|    |    |    \ Gain VMG de 0 � 999 %
		|    |     \ Angle pour optimiser le VMG de 0 � 359�
		|    \ Gain CMG de 0 � 999 %
		\ Angle pour optimiser le CMG de 0 � 359�*/
		nmeastr = _T("$PNKEP,04,") + wxString::Format("%.1f,", data1) + wxString::Format("%.1f,", data2) + wxString::Format("%.1f,", data3) + wxString::Format("%.1f", data4);
		break;
	case 5:
		nmeastr = _T("$PNKEP,05,") + wxString::Format("%.1f,", data1) + wxString::Format("%.2f,N,", data2) + wxString::Format("%.2f,K", data3);
		break;
	default:
		nmeastr = _T("");
		break;
	}
	if (nmeastr != "")
		SendNMEASentence(nmeastr);
}
/***********************************************************************
Put the created NMEA record ot O's NMEA data stream
Taken from nmeaconverter_pi.
All credits to Pavel !
***********************************************************************/
void tactics_pi::SendNMEASentence(wxString sentence)
{
	wxString Checksum = ComputeChecksum(sentence);
	sentence = sentence.Append(wxT("*"));
	sentence = sentence.Append(Checksum);
	sentence = sentence.Append(wxT("\r\n"));
	//wxLogMessage(sentence);
	PushNMEABuffer(sentence);
}
/**********************************************************************
Calculate the checksum of the created NMEA record.
Taken from nmeaconverter_pi.
All credits to Pavel !
***********************************************************************/
wxString tactics_pi::ComputeChecksum(wxString sentence)
{
	unsigned char calculated_checksum = 0;
	for (wxString::const_iterator i = sentence.begin() + 1; i != sentence.end() && *i != '*'; ++i)
		calculated_checksum ^= static_cast<unsigned char> (*i);

	return(wxString::Format(wxT("%02X"), calculated_checksum));
}

/**********************************************************************
NKE has a bug in its TWS calculation with activated(!) "True Wind Tables"
in combination of Multigraphic instrument and HR wind sensor. This almost
duplicates the TWS values delivered in MWD & VWT and destroys the Wind
history view, showing weird peaks. It is particularly annoying when @anchor
and trying to record windspeeds ...
It seems to happen only when
* VWR sentence --> AWA changes from "Left" to "Right" through 0� AWA
* with low AWA values like 0...4 degrees
NMEA stream looks like this:
                 $IIMWD,,,349,M,6.6,N,3.4,M*29                 6.6N TWS
                 $IIVWT, 3, R, 7.1, N, 3.7, M, 13.1, K * 63
                 $IIVWR, 0, R, 7.9, N, 4.1, M, 14.6, K * 6F    it seems to begin with 0�AWA ...
                 $IIMWD, , , 346, M, 15.3, N, 7.9, M * 18      jump from 6.6 to 15.3 TWS ...
                 $IIVWT, 7, R, 15.3, N, 7.9, M, 28.3, K * 56   VWT also has 15.3 TWS
                 $IIVWR, 1, L, 8.6, N, 4.4, M, 15.9, K * 7B     1�AWA
                 $IIMWD, , , 345, M, 15.8, N, 8.1, M * 17       still 15.8 TWS
                 $IIVWT, 1, L, 8.6, N, 4.4, M, 15.9, K * 7D     VWT now back to 8.6 TWS
                 $IIVWR, 0, R, 9.0, N, 4.6, M, 16.7, K * 6C     passing 0�AWA again
                 $IIMWD, , , 335, M, 17.6, N, 9.1, M * 1D       jump to 17.6 TWS
                 $IIVWT, 8, R, 17.6, N, 9.1, M, 32.6, K * 56    still 17.6 TWS
                 $IIVWR, 4, R, 9.1, N, 4.7, M, 16.9, K * 66     VWR increasing to 4�AWA...
                 $IIMWD, , , 335, M, 9.0, N, 4.6, M*2E          and back to normal, 9 TWS
Trying to catch this here, return true if detected and if the check
is explictly requested in the ini-file. The implementation of
the SetNMEASentence in the child class can then simply drop this data.
***********************************************************************/
bool tactics_pi::SetNMEASentenceMWD_NKEbug(double SentenceWindSpeedKnots)
{
    if (!m_bNKE_TrueWindTableBug)
        return false;
    if (std::isnan(SentenceWindSpeedKnots))
        return false;
    if (std::isnan(mTWS))
        return false;
    if ((m_VWR_AWA < 8.0) && (SentenceWindSpeedKnots > mTWS*1.7)){
     wxLogMessage(
            "dashboard_tactics: NKE MWD-Sentence filtered, see m_bNKE_TrueWindTableBug setting. MWD-Spd=%f,mTWS=%f,VWR_AWA=%f,NKE_BUG=%d",
            SentenceWindSpeedKnots, mTWS, m_VWR_AWA,
            m_bNKE_TrueWindTableBug);
        return true;
    } // then conditions explained in the above description have been met
    return false;
}

//----------------------------------------------------------------
//    Tactics Preference Dialogs Implementation
//    porting note: as virtual parent for child dialog which will
//                  create and deal with buttons outside the tabs.
//----------------------------------------------------------------

TacticsPreferencesDialog::TacticsPreferencesDialog(
    wxWindow *parent, wxWindowID id, const wxString derivtitle, wxPoint pos ) :
	wxDialog(
        parent, id, derivtitle, pos, wxDefaultSize ,
        wxDEFAULT_DIALOG_STYLE | wxMAXIMIZE_BOX | wxMINIMIZE_BOX | wxRESIZE_BORDER)
{
    m_itemNotebook = NULL;
    return;
}

void TacticsPreferencesDialog::TacticsPreferencesInit( wxNotebook *itemNotebook, int border_size )
{
    m_itemNotebook = itemNotebook;
    m_border_size = border_size;
}

void TacticsPreferencesDialog::TacticsPreferencesPanel()
{
    if (m_itemNotebook == NULL)
        return;

    wxScrolledWindow *itemPanelNotebook03 = new wxScrolledWindow(m_itemNotebook, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxTAB_TRAVERSAL | wxVSCROLL);

	int scrollRate = 5;
#ifdef __OCPN__ANDROID__
	scrollRate = 1;
#endif
	itemPanelNotebook03->SetScrollRate(0, scrollRate);
	//itemNotebook->Layout();

	wxBoxSizer* itemBoxSizer06 = new wxBoxSizer(wxVERTICAL);
	itemPanelNotebook03->SetSizer(itemBoxSizer06);
	m_itemNotebook->AddPage(itemPanelNotebook03, _(L"\u2191Tactics Performance Parameters"));
	//****************************************************************************************************
	wxStaticBox* itemStaticBox05 = new wxStaticBox(itemPanelNotebook03, wxID_ANY, _("Laylines"));
	wxStaticBoxSizer* itemStaticBoxSizer05 = new wxStaticBoxSizer(itemStaticBox05, wxHORIZONTAL);
	itemBoxSizer06->Add(itemStaticBoxSizer05, 0, wxEXPAND | wxALL, m_border_size);

	wxFlexGridSizer *itemFlexGridSizer05 = new wxFlexGridSizer(2);
	itemFlexGridSizer05->AddGrowableCol(1);
	itemStaticBoxSizer05->Add(itemFlexGridSizer05, 1, wxEXPAND | wxALL, 0);
	wxString s;
    //---Layline damping factor -----------------
    wxStaticText* itemStaticText18 = new wxStaticText(itemPanelNotebook03, wxID_ANY, _("Layline damping factor [0.025-1]:  "),
      wxDefaultPosition, wxDefaultSize, 0);
    itemStaticText18->SetToolTip(_("The layline damping factor determines how fast the  laylines react on your course changes, i.e. your COG changes.\n Low values mean high damping."));
    itemFlexGridSizer05->Add(itemStaticText18, 0, wxEXPAND | wxALL, m_border_size);
    m_alphaLaylineDampFactor = new wxSpinCtrlDouble(itemPanelNotebook03, wxID_ANY, wxEmptyString, wxDefaultPosition, wxSize(60, -1), wxSP_ARROW_KEYS, 0.025, 1, g_dalphaLaylinedDampFactor, 0.001);
    itemFlexGridSizer05->Add(m_alphaLaylineDampFactor, 0, wxALIGN_LEFT, 0);
    m_alphaLaylineDampFactor->SetValue(g_dalphaLaylinedDampFactor);
    m_alphaLaylineDampFactor->SetToolTip(_("The layline damping factor determines how fast the  laylines react on your course changes, i.e. your COG changes.\n Low values mean high damping."));

    //--------------------
	wxStaticText* itemStaticText20 = new wxStaticText(itemPanelNotebook03, wxID_ANY, _("Layline width damping factor [0.025-1]:  "),
		wxDefaultPosition, wxDefaultSize, 0);
	itemStaticText20->SetToolTip(_("The width of the boat laylines is based on the yawing of the boat (vertical axis), i.e. your COG changes.\nThe idea is to display the COG range where you're sailing to.\n Low values mean high damping."));
	itemFlexGridSizer05->Add(itemStaticText20, 0, wxEXPAND | wxALL, m_border_size);
	m_alphaDeltCoG = new wxSpinCtrlDouble(itemPanelNotebook03, wxID_ANY, wxEmptyString, wxDefaultPosition, wxSize(60, -1), wxSP_ARROW_KEYS, 0.025, 1, g_dalphaDeltCoG, 0.001);
	itemFlexGridSizer05->Add(m_alphaDeltCoG, 0, wxALIGN_LEFT, 0);
	m_alphaDeltCoG->SetValue(g_dalphaDeltCoG);
	m_alphaDeltCoG->SetToolTip(_("Width of the boat laylines is based on the yawing of the boat (vertical axis), i.e. your COG changes.\nThe idea is to display the range where you're sailing to.\n Low values mean high damping."));

	//--------------------
	wxStaticText* itemStaticText19 = new wxStaticText(itemPanelNotebook03, wxID_ANY, _("Layline length on Chart [nm]:  "),
		wxDefaultPosition, wxDefaultSize, 0);
	itemStaticText19->SetToolTip(_("Length of the boat laylines in [nm]"));

	itemFlexGridSizer05->Add(itemStaticText19, 0, wxEXPAND | wxALL, m_border_size);
	m_pLaylineLength = new wxSpinCtrlDouble(itemPanelNotebook03, wxID_ANY, wxEmptyString, wxDefaultPosition, wxSize(60, -1), wxSP_ARROW_KEYS, 0.0, 20.0, g_dLaylineLengthonChart, 0.1);
	itemFlexGridSizer05->Add(m_pLaylineLength, 0, wxALIGN_LEFT | wxALL, 0);
	m_pLaylineLength->SetValue(g_dLaylineLengthonChart);
	m_pLaylineLength->SetToolTip(_("Length of the boat laylines in [nm]"));
	//--------------------
	wxStaticText* itemStaticText21 = new wxStaticText(itemPanelNotebook03, wxID_ANY, _("Min. Layline Width [\u00B0]:"),
		wxDefaultPosition, wxDefaultSize, 0);
	itemStaticText21->SetToolTip(_("Min. width of boat laylines in degrees."));
	itemFlexGridSizer05->Add(itemStaticText21, 0, wxEXPAND | wxALL, m_border_size);
	m_minLayLineWidth = new wxSpinCtrl(itemPanelNotebook03, wxID_ANY, wxEmptyString, wxDefaultPosition, wxSize(60, -1), wxSP_ARROW_KEYS, 0, 20, g_iMinLaylineWidth);
	m_minLayLineWidth->SetToolTip(_("Min. width of boat laylines in degrees."));
	itemFlexGridSizer05->Add(m_minLayLineWidth, 0, wxALIGN_LEFT | wxALL, 0);

	//--------------------
	wxStaticText* itemStaticText22 = new wxStaticText(itemPanelNotebook03, wxID_ANY, _("Max. Layline Width [\u00B0]:"),
		wxDefaultPosition, wxDefaultSize, 0);
	itemStaticText22->SetToolTip(_("Max. width of boat laylines in degrees."));
	itemFlexGridSizer05->Add(itemStaticText22, 0, wxEXPAND | wxALL, m_border_size);
	m_maxLayLineWidth = new wxSpinCtrl(itemPanelNotebook03, wxID_ANY, wxEmptyString, wxDefaultPosition, wxSize(60, -1), wxSP_ARROW_KEYS, 0, 30, g_iMaxLaylineWidth);
	m_maxLayLineWidth->SetToolTip(_("Max. width of boat laylines in degrees."));
	itemFlexGridSizer05->Add(m_maxLayLineWidth, 0, wxALIGN_LEFT | wxALL, 0);
	//****************************************************************************************************
	wxStaticBox* itemStaticBox06 = new wxStaticBox(itemPanelNotebook03, wxID_ANY, _("Leeway"));
	wxStaticBoxSizer* itemStaticBoxSizer06 = new wxStaticBoxSizer(itemStaticBox06, wxHORIZONTAL);
	itemBoxSizer06->Add(itemStaticBoxSizer06, 0, wxEXPAND | wxALL, m_border_size);
	wxFlexGridSizer *itemFlexGridSizer06 = new wxFlexGridSizer(2);
	itemFlexGridSizer06->AddGrowableCol(1);
	itemStaticBoxSizer06->Add(itemFlexGridSizer06, 1, wxEXPAND | wxALL, 0);


	//--------------------
	wxStaticText* itemStaticText23a = new wxStaticText(itemPanelNotebook03, wxID_ANY, _("Boat's Leeway factor [0-20]:  "), wxDefaultPosition, wxDefaultSize, 0);
	itemStaticText23a->SetToolTip(_("Leeway='Drift' of boat due to heel/wind influence\nLow values mean high performance of hull\nLeeway = (LeewayFactor * Heel) / STW\u00B2;")); //�
	itemFlexGridSizer06->Add(itemStaticText23a, 0, wxEXPAND | wxALL, m_border_size);
	m_LeewayFactor = new wxSpinCtrlDouble(itemPanelNotebook03, wxID_ANY, wxEmptyString, wxDefaultPosition, wxSize(60, -1), wxSP_ARROW_KEYS, 0, 20, g_dLeewayFactor, 0.01);
	m_LeewayFactor->SetToolTip(_("Leeway='Drift' of boat due to heel/wind influence\nLow values mean high performance of hull\nLeeway = (LeewayFactor * Heel) / STW\u00B2;"));

	itemFlexGridSizer06->Add(m_LeewayFactor, 0, wxALIGN_LEFT | wxALL, 0);
	m_LeewayFactor->SetValue(g_dLeewayFactor);
	//--------------------
	m_ButtonUseHeelSensor = new wxRadioButton(itemPanelNotebook03, wxID_ANY, _("Use Heel Sensor"), wxDefaultPosition, wxDefaultSize, wxRB_GROUP);
	itemFlexGridSizer06->Add(m_ButtonUseHeelSensor, 0, wxALL, 5);
	m_ButtonUseHeelSensor->SetValue(g_bUseHeelSensor);
	m_ButtonUseHeelSensor->SetToolTip(_("Use the internal heel sensor if available\nImportant for the correct calculation of the surface current."));
	wxStaticText* itemStaticText23b = new wxStaticText(itemPanelNotebook03, wxID_ANY, _(""), wxDefaultPosition, wxDefaultSize, 0);
	itemFlexGridSizer06->Add(itemStaticText23b, 0, wxEXPAND | wxALL, m_border_size);
	//--------------------
	m_ButtonFixedLeeway = new wxRadioButton(itemPanelNotebook03, wxID_ANY, _("fixed/max Leeway [\u00B0]:"), wxDefaultPosition, wxDefaultSize, 0);
	itemFlexGridSizer06->Add(m_ButtonFixedLeeway, 0, wxALL, 5);
	m_ButtonFixedLeeway->SetValue(g_bUseFixedLeeway);
	m_ButtonFixedLeeway->SetToolTip(_("Dual purpose !\nIf Radiobutton is NOT set, then it's used to limit Leeway to a max value.\n If Radiobutton is set, then it fixes Leeway to this constant value."));

	m_ButtonFixedLeeway->Connect(wxEVT_COMMAND_RADIOBUTTON_SELECTED, wxCommandEventHandler(TacticsPreferencesDialog::OnManualHeelUpdate), NULL, this);

	m_fixedLeeway = new wxSpinCtrlDouble(itemPanelNotebook03, wxID_ANY, wxEmptyString, wxDefaultPosition, wxSize(60, -1), wxSP_ARROW_KEYS, 0, 30, g_dfixedLeeway, 0.01);
	itemFlexGridSizer06->Add(m_fixedLeeway, 0, wxALIGN_LEFT, 0);
	m_fixedLeeway->SetValue(g_dfixedLeeway);
	//--------------------
	m_ButtonHeelInput = new wxRadioButton(itemPanelNotebook03, wxID_ANY, _("manual Heel input:"), wxDefaultPosition, wxDefaultSize, 0);
	itemFlexGridSizer06->Add(m_ButtonHeelInput, 0, wxALL, 5);
	m_ButtonHeelInput->SetValue(g_bManHeelInput);
	m_ButtonHeelInput->SetToolTip(_("If no heel sensor is available, you can create a manual 'heel polar' here.\nJust read/enter the data from a mechanical heel sensor (e.g. on compass).\nUse True Wind Speed & Angle only !\nTake care: motoring w/o sails&heel will show wrong current data !!!"));

	m_ButtonHeelInput->Connect(wxEVT_COMMAND_RADIOBUTTON_SELECTED, wxCommandEventHandler(TacticsPreferencesDialog::OnManualHeelUpdate), NULL, this);

	wxStaticText* itemStaticText23c = new wxStaticText(itemPanelNotebook03, wxID_ANY, _(""), wxDefaultPosition, wxDefaultSize, 0);
	itemFlexGridSizer06->Add(itemStaticText23c, 0, wxEXPAND | wxALL, m_border_size);
	//****************************************************************************************************
	wxStaticBox* itemStaticBox07 = new wxStaticBox(itemPanelNotebook03, wxID_ANY, _("Heel"));
	wxStaticBoxSizer* itemStaticBoxSizer07 = new wxStaticBoxSizer(itemStaticBox07, wxHORIZONTAL);
	itemBoxSizer06->Add(itemStaticBoxSizer07, 0, wxEXPAND | wxALL, m_border_size);
	wxFlexGridSizer *itemFlexGridSizer07 = new wxFlexGridSizer(4);
	itemStaticBoxSizer07->Add(itemFlexGridSizer07, 1, wxEXPAND | wxALL, 0);

	//--------------------
	wxStaticText* itemStaticText23T0 = new wxStaticText(itemPanelNotebook03, wxID_ANY, _("TWS/TWA "), wxDefaultPosition, wxDefaultSize, 0);
	itemFlexGridSizer07->Add(itemStaticText23T0, 0, wxEXPAND | wxALL, m_border_size);
	wxStaticText* itemStaticText23T1 = new wxStaticText(itemPanelNotebook03, wxID_ANY, _(" 45\u00B0"), wxDefaultPosition, wxDefaultSize, 0);
	itemFlexGridSizer07->Add(itemStaticText23T1, 0, wxALIGN_CENTER | wxALL, m_border_size);
	wxStaticText* itemStaticText23T2 = new wxStaticText(itemPanelNotebook03, wxID_ANY, _(" 90\u00B0"), wxDefaultPosition, wxDefaultSize, 0);
	itemFlexGridSizer07->Add(itemStaticText23T2, 0, wxALIGN_CENTER | wxALL, m_border_size);
	wxStaticText* itemStaticText23T3 = new wxStaticText(itemPanelNotebook03, wxID_ANY, _("135\u00B0"), wxDefaultPosition, wxDefaultSize, 0);
	itemFlexGridSizer07->Add(itemStaticText23T3, 0, wxALIGN_CENTER | wxALL, m_border_size);
	//--------------------
	wxStaticText* itemStaticText23ws5 = new wxStaticText(itemPanelNotebook03, wxID_ANY, _("5 kn"), wxDefaultPosition, wxDefaultSize, 0);
	itemFlexGridSizer07->Add(itemStaticText23ws5, 0, wxEXPAND | wxALL, m_border_size);
	m_heel5_45 = new wxSpinCtrlDouble(itemPanelNotebook03, wxID_ANY, wxEmptyString, wxDefaultPosition, wxSize(60, -1), wxSP_ARROW_KEYS, 0, 60, g_dheel[1][1], 0.1);
	itemFlexGridSizer07->Add(m_heel5_45, 0, wxALIGN_LEFT, 0);
	m_heel5_45->SetValue(g_dheel[1][1]);
	//--------------------
	m_heel5_90 = new wxSpinCtrlDouble(itemPanelNotebook03, wxID_ANY, wxEmptyString, wxDefaultPosition, wxSize(60, -1), wxSP_ARROW_KEYS, 0, 60, g_dheel[1][2], 0.1);
	itemFlexGridSizer07->Add(m_heel5_90, 0, wxALIGN_LEFT, 0);
	m_heel5_90->SetValue(g_dheel[1][2]);
	//--------------------
	m_heel5_135 = new wxSpinCtrlDouble(itemPanelNotebook03, wxID_ANY, wxEmptyString, wxDefaultPosition, wxSize(60, -1), wxSP_ARROW_KEYS, 0, 60, g_dheel[1][3], 0.1);
	itemFlexGridSizer07->Add(m_heel5_135, 0, wxALIGN_LEFT, 0);
	m_heel5_135->SetValue(g_dheel[1][3]);
	//--------------------
	wxStaticText* itemStaticText23ws10 = new wxStaticText(itemPanelNotebook03, wxID_ANY, _("10 kn"), wxDefaultPosition, wxDefaultSize, 0);
	itemFlexGridSizer07->Add(itemStaticText23ws10, 0, wxEXPAND | wxALL, m_border_size);
	m_heel10_45 = new wxSpinCtrlDouble(itemPanelNotebook03, wxID_ANY, wxEmptyString, wxDefaultPosition, wxSize(60, -1), wxSP_ARROW_KEYS, 0, 60, g_dheel[2][1], 0.1);
	itemFlexGridSizer07->Add(m_heel10_45, 0, wxALIGN_LEFT, 0);
	m_heel10_45->SetValue(g_dheel[2][1]);
	//--------------------
	m_heel10_90 = new wxSpinCtrlDouble(itemPanelNotebook03, wxID_ANY, wxEmptyString, wxDefaultPosition, wxSize(60, -1), wxSP_ARROW_KEYS, 0, 60, g_dheel[2][2], 0.1);
	itemFlexGridSizer07->Add(m_heel10_90, 0, wxALIGN_LEFT, 0);
	m_heel10_90->SetValue(g_dheel[2][2]);
	//--------------------
	m_heel10_135 = new wxSpinCtrlDouble(itemPanelNotebook03, wxID_ANY, wxEmptyString, wxDefaultPosition, wxSize(60, -1), wxSP_ARROW_KEYS, 0, 60, g_dheel[2][3], 0.1);
	itemFlexGridSizer07->Add(m_heel10_135, 0, wxALIGN_LEFT, 0);
	m_heel10_135->SetValue(g_dheel[2][3]);
	//--------------------
	wxStaticText* itemStaticText23ws15 = new wxStaticText(itemPanelNotebook03, wxID_ANY, _("15 kn"), wxDefaultPosition, wxDefaultSize, 0);
	itemFlexGridSizer07->Add(itemStaticText23ws15, 0, wxEXPAND | wxALL, m_border_size);

	m_heel15_45 = new wxSpinCtrlDouble(itemPanelNotebook03, wxID_ANY, wxEmptyString, wxDefaultPosition, wxSize(60, -1), wxSP_ARROW_KEYS, 0, 60, g_dheel[3][1], 0.1);
	itemFlexGridSizer07->Add(m_heel15_45, 0, wxALIGN_LEFT, 0);
	m_heel15_45->SetValue(g_dheel[3][1]);
	//--------------------
	m_heel15_90 = new wxSpinCtrlDouble(itemPanelNotebook03, wxID_ANY, wxEmptyString, wxDefaultPosition, wxSize(60, -1), wxSP_ARROW_KEYS, 0, 60, g_dheel[3][2], 0.1);
	itemFlexGridSizer07->Add(m_heel15_90, 0, wxALIGN_LEFT, 0);
	m_heel15_90->SetValue(g_dheel[3][2]);
	//--------------------
	m_heel15_135 = new wxSpinCtrlDouble(itemPanelNotebook03, wxID_ANY, wxEmptyString, wxDefaultPosition, wxSize(60, -1), wxSP_ARROW_KEYS, 0, 60, g_dheel[3][3], 0.1);
	itemFlexGridSizer07->Add(m_heel15_135, 0, wxALIGN_LEFT, 0);
	m_heel15_135->SetValue(g_dheel[3][3]);
	//--------------------
	wxStaticText* itemStaticText23ws20 = new wxStaticText(itemPanelNotebook03, wxID_ANY, _("20 kn"), wxDefaultPosition, wxDefaultSize, 0);
	itemFlexGridSizer07->Add(itemStaticText23ws20, 0, wxEXPAND | wxALL, m_border_size);
	m_heel20_45 = new wxSpinCtrlDouble(itemPanelNotebook03, wxID_ANY, wxEmptyString, wxDefaultPosition, wxSize(60, -1), wxSP_ARROW_KEYS, 0, 60, g_dheel[4][1], 0.1);
	itemFlexGridSizer07->Add(m_heel20_45, 0, wxALIGN_LEFT, 0);
	m_heel20_45->SetValue(g_dheel[4][1]);
	//--------------------
	m_heel20_90 = new wxSpinCtrlDouble(itemPanelNotebook03, wxID_ANY, wxEmptyString, wxDefaultPosition, wxSize(60, -1), wxSP_ARROW_KEYS, 0, 60, g_dheel[4][2], 0.1);
	itemFlexGridSizer07->Add(m_heel20_90, 0, wxALIGN_LEFT, 0);
	m_heel20_90->SetValue(g_dheel[4][2]);
	//--------------------
	m_heel20_135 = new wxSpinCtrlDouble(itemPanelNotebook03, wxID_ANY, wxEmptyString, wxDefaultPosition, wxSize(60, -1), wxSP_ARROW_KEYS, 0, 60, g_dheel[4][3], 0.1);
	itemFlexGridSizer07->Add(m_heel20_135, 0, wxALIGN_LEFT, 0);
	m_heel20_135->SetValue(g_dheel[4][3]);
	//--------------------
	wxStaticText* itemStaticText23ws25 = new wxStaticText(itemPanelNotebook03, wxID_ANY, _("25 kn"), wxDefaultPosition, wxDefaultSize, 0);
	itemFlexGridSizer07->Add(itemStaticText23ws25, 0, wxEXPAND | wxALL, m_border_size);
	m_heel25_45 = new wxSpinCtrlDouble(itemPanelNotebook03, wxID_ANY, wxEmptyString, wxDefaultPosition, wxSize(60, -1), wxSP_ARROW_KEYS, 0, 60, g_dheel[5][1], 0.1);
	itemFlexGridSizer07->Add(m_heel25_45, 0, wxALIGN_LEFT, 0);
	m_heel25_45->SetValue(g_dheel[5][1]);
	//--------------------
	m_heel25_90 = new wxSpinCtrlDouble(itemPanelNotebook03, wxID_ANY, wxEmptyString, wxDefaultPosition, wxSize(60, -1), wxSP_ARROW_KEYS, 0, 60, g_dheel[5][2], 0.1);
	itemFlexGridSizer07->Add(m_heel25_90, 0, wxALIGN_LEFT, 0);
	m_heel25_90->SetValue(g_dheel[5][2]);
	//--------------------
	m_heel25_135 = new wxSpinCtrlDouble(itemPanelNotebook03, wxID_ANY, wxEmptyString, wxDefaultPosition, wxSize(60, -1), wxSP_ARROW_KEYS, 0, 60, g_dheel[5][3], 0.1);
	itemFlexGridSizer07->Add(m_heel25_135, 0, wxALIGN_LEFT, 0);
	m_heel25_135->SetValue(g_dheel[5][3]);

	//****************************************************************************************************
	wxStaticBox* itemStaticBox08 = new wxStaticBox(itemPanelNotebook03, wxID_ANY, _("Current"));
	wxStaticBoxSizer* itemStaticBoxSizer08 = new wxStaticBoxSizer(itemStaticBox08, wxHORIZONTAL);
	itemBoxSizer06->Add(itemStaticBoxSizer08, 0, wxEXPAND | wxALL, m_border_size);
	wxFlexGridSizer *itemFlexGridSizer08 = new wxFlexGridSizer(2);
	itemFlexGridSizer08->AddGrowableCol(1);
	itemStaticBoxSizer08->Add(itemFlexGridSizer08, 1, wxEXPAND | wxALL, 0);

	//--------------------
	//
	wxStaticText* itemStaticText24 = new wxStaticText(itemPanelNotebook03, wxID_ANY, _("Current damping factor [1-400]:  "), wxDefaultPosition, wxDefaultSize, 0);
	itemStaticText24->SetToolTip(_("Stabilizes the surface current 'arrow' in the chart overlay, bearing compass and also the numerical instruments\nLow values mean high damping"));
	itemFlexGridSizer08->Add(itemStaticText24, 0, wxEXPAND | wxALL, m_border_size);
	m_AlphaCurrDir = new wxSpinCtrl(itemPanelNotebook03, wxID_ANY, wxEmptyString, wxDefaultPosition, wxSize(60, -1), wxSP_ARROW_KEYS | wxSP_WRAP, 1, 400, g_dalpha_currdir * 1000);
	itemFlexGridSizer08->Add(m_AlphaCurrDir, 0, wxALIGN_LEFT, 0);
	m_AlphaCurrDir->SetValue(g_dalpha_currdir * 1000);
	m_AlphaCurrDir->SetToolTip(_("Stabilizes the surface current 'arrow' in the chart overlay, bearing compass and also the numerical instruments\nLow values mean high damping"));
	//--------------------
	m_CurrentOnChart = new wxCheckBox(itemPanelNotebook03, wxID_ANY, _("Display Current on Chart (OpenGL)"));
	itemFlexGridSizer08->Add(m_CurrentOnChart, 0, wxEXPAND, 5);
	m_CurrentOnChart->SetValue(g_bDisplayCurrentOnChart);
	m_CurrentOnChart->SetToolTip(_("The default on program startup"));
	//****************************************************************************************************
	wxStaticBox* itemStaticBox10 = new wxStaticBox(itemPanelNotebook03, wxID_ANY, _("True Wind"));
	wxStaticBoxSizer* itemStaticBoxSizer10 = new wxStaticBoxSizer(itemStaticBox10, wxHORIZONTAL);
	itemBoxSizer06->Add(itemStaticBoxSizer10, 0, wxEXPAND | wxALL, m_border_size);
	wxFlexGridSizer *itemFlexGridSizer10 = new wxFlexGridSizer(2);
	itemFlexGridSizer10->AddGrowableCol(1);
	itemStaticBoxSizer10->Add(itemFlexGridSizer10, 1, wxEXPAND | wxALL, 0);

	//--------------------
	m_CorrectSTWwithLeeway = new wxCheckBox(itemPanelNotebook03, wxID_ANY, _("Correct STW with Leeway"));
	itemFlexGridSizer10->Add(m_CorrectSTWwithLeeway, 0, wxEXPAND, 5);
	m_CorrectSTWwithLeeway->SetValue(g_bCorrectSTWwithLeeway);
	m_CorrectSTWwithLeeway->SetToolTip(_("Apply a correction to your log speed throughout the plugin based on the calculated Leeway and Current.\nOnly makes sense with a real heel sensor.\nMake sure your instruments do not already apply this correction !"));
	//--------------------
	m_CorrectAWwithHeel = new wxCheckBox(itemPanelNotebook03, wxID_ANY, _("Correct AWS/AWA with Heel"));
	itemFlexGridSizer10->Add(m_CorrectAWwithHeel, 0, wxEXPAND, 5);
	m_CorrectAWwithHeel->SetValue(g_bCorrectAWwithHeel);
	m_CorrectAWwithHeel->SetToolTip(_("Use with care, this is normally done by the instruments themselves as soon as you have an integrated, original equipment heel sensor"));

	m_CorrectAWwithHeel->Connect(wxEVT_COMMAND_CHECKBOX_CLICKED, wxCommandEventHandler(TacticsPreferencesDialog::OnAWSAWACorrectionUpdated), NULL, this);
	//--------------------
	m_ForceTrueWindCalculation = new wxCheckBox(itemPanelNotebook03, wxID_ANY, _("Force True Wind Calculation"));
	itemFlexGridSizer10->Add(m_ForceTrueWindCalculation, 0, wxEXPAND, 5);
	m_ForceTrueWindCalculation->SetValue(g_bForceTrueWindCalculation);
	m_ForceTrueWindCalculation->SetToolTip(_("Internally calculates True Wind data (TWS,TWA,TWD) and uses it within the whole plugin even if there is True Wind data available via NMEA."));

	//--------------------
	m_UseSOGforTWCalc = new wxCheckBox(itemPanelNotebook03, wxID_ANY, _("Use SOG instead of STW for True Wind Calc."));
	itemFlexGridSizer10->Add(m_UseSOGforTWCalc, 0, wxEXPAND, 5);
	m_UseSOGforTWCalc->SetValue(g_bUseSOGforTWCalc);
	m_UseSOGforTWCalc->SetToolTip(_("Recommended. As True Wind blows over the earth surface, we should calc. it with Speed Over Ground.\nEliminates the influence of currents."));

	m_ShowWindbarbOnChart = new wxCheckBox(itemPanelNotebook03, wxID_ANY, _("Show Wind Barb on Chart (OpenGL)"));
	itemFlexGridSizer10->Add(m_ShowWindbarbOnChart, 0, wxEXPAND, 5);
	m_ShowWindbarbOnChart->SetValue(g_bShowWindbarbOnChart);
	m_ShowWindbarbOnChart->SetToolTip(_("The default on program startup"));

	//****************************************************************************************************
	wxStaticBox* itemStaticBox09 = new wxStaticBox(itemPanelNotebook03, wxID_ANY, _(L"Polar - NOTE: \u2191Tactics instruments need your boat's polars!"));
	wxStaticBoxSizer* itemStaticBoxSizer09 = new wxStaticBoxSizer(itemStaticBox09, wxHORIZONTAL);
	itemBoxSizer06->Add(itemStaticBoxSizer09, 0, wxEXPAND | wxALL, m_border_size);
	wxFlexGridSizer *itemFlexGridSizer09 = new wxFlexGridSizer(2);
	itemFlexGridSizer09->AddGrowableCol(1);
	itemStaticBoxSizer09->Add(itemFlexGridSizer09, 1, wxEXPAND | wxALL, 0);

	wxStaticText* itemStaticText30 = new wxStaticText(itemPanelNotebook03, wxID_ANY, _("Polar file:"), wxDefaultPosition, wxDefaultSize, 0);
	itemFlexGridSizer09->Add(itemStaticText30, 0, wxEXPAND | wxALL, m_border_size);

	m_pTextCtrlPolar = new wxTextCtrl(itemPanelNotebook03, wxID_ANY, g_path_to_PolarFile, wxDefaultPosition, wxDefaultSize);
	itemFlexGridSizer09->Add(m_pTextCtrlPolar, 0, wxALIGN_LEFT | wxEXPAND | wxALL, m_border_size);

	m_buttonLoadPolar = new wxButton(itemPanelNotebook03, wxID_ANY, _("Load"), wxDefaultPosition, wxDefaultSize, 0);
	itemFlexGridSizer09->Add(m_buttonLoadPolar, 0, wxALIGN_LEFT | wxALL, 5);
    m_buttonLoadPolar->Connect(
        wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler(TacticsPreferencesDialog::SelectPolarFile), NULL, this);

	m_ShowPolarOnChart = new wxCheckBox(itemPanelNotebook03, wxID_ANY, _("Show Polar on Chart (OpenGL)"));
	itemFlexGridSizer09->Add(m_ShowPolarOnChart, 0, wxEXPAND, 5);
	m_ShowPolarOnChart->SetValue(g_bShowPolarOnChart);
	m_ShowPolarOnChart->SetToolTip(_("The default on program startup"));
	//****************************************************************************************************
	wxStaticBox* itemStaticBoxExpData = new wxStaticBox(itemPanelNotebook03, wxID_ANY, _("Export NMEA Performance Data"));
	wxStaticBoxSizer* itemStaticBoxSizerExpData = new wxStaticBoxSizer(itemStaticBoxExpData, wxHORIZONTAL);
	itemBoxSizer06->Add(itemStaticBoxSizerExpData, 0, wxEXPAND | wxALL, m_border_size);
	wxFlexGridSizer *itemFlexGridSizerExpData = new wxFlexGridSizer(2);
	itemFlexGridSizerExpData->AddGrowableCol(1);
	itemStaticBoxSizerExpData->Add(itemFlexGridSizerExpData, 1, wxEXPAND | wxALL, 0);
	//-------------------- Radiobutton(s) for different instrument systems -----------
	m_ButtonExpNKE = new wxRadioButton(itemPanelNotebook03, wxID_ANY, _("NKE format ($PNKEP)"), wxDefaultPosition, wxDefaultSize, wxRB_GROUP);
	itemFlexGridSizerExpData->Add(m_ButtonExpNKE, 0, wxALL, 5);
	m_ButtonExpNKE->SetValue(true); // fixed value for now
	m_ButtonExpNKE->SetToolTip(_("Currently only set up for NKE instruments. Exports a predefined set of up to 5 NMEA records which are 'known' by NKE instruments and can be displayed there.\nRead the manual how to set up the interface connection !"));

	wxStaticText* itemStaticTextDummy = new wxStaticText(itemPanelNotebook03, wxID_ANY, _(""), wxDefaultPosition, wxDefaultSize, 0);
	itemFlexGridSizerExpData->Add(itemStaticTextDummy, 0, wxEXPAND | wxALL, m_border_size);
	//--------------------
	//--------------------
	m_ExpPerfData01 = new wxCheckBox(itemPanelNotebook03, wxID_ANY, _("Target Polar Speed"));
	itemFlexGridSizerExpData->Add(m_ExpPerfData01, 0, wxEXPAND, 5);
	m_ExpPerfData01->SetValue(g_bExpPerfData01);
	//--------------------
	m_ExpPerfData02 = new wxCheckBox(itemPanelNotebook03, wxID_ANY, _("CoG on other Tack"));
	itemFlexGridSizerExpData->Add(m_ExpPerfData02, 0, wxEXPAND, 5);
	m_ExpPerfData02->SetValue(g_bExpPerfData02);
	//--------------------
	m_ExpPerfData03 = new wxCheckBox(itemPanelNotebook03, wxID_ANY, _("Target-") + g_sVMGSynonym + _(" angle + Perf. %"));
	itemFlexGridSizerExpData->Add(m_ExpPerfData03, 0, wxEXPAND, 5);
	m_ExpPerfData03->SetValue(g_bExpPerfData03);
	//--------------------
	m_ExpPerfData04 = new wxCheckBox(itemPanelNotebook03, wxID_ANY, _("Diff. angle to Target-") + g_sVMGSynonym + _T("/-") + g_sCMGSynonym + _(" + corresp.gain"));
	itemFlexGridSizerExpData->Add(m_ExpPerfData04, 0, wxEXPAND, 5);
	m_ExpPerfData04->SetValue(g_bExpPerfData04);
	//--------------------
	m_ExpPerfData05 = new wxCheckBox(itemPanelNotebook03, wxID_ANY, _("Current Direction + Speed"));
	itemFlexGridSizerExpData->Add(m_ExpPerfData05, 0, wxEXPAND, 5);
	m_ExpPerfData05->SetValue(g_bExpPerfData05);
	//--------------------
    //****************************************************************************************************
    //****************************************************************************************************
    wxStaticBox* itemStaticBoxFileExp = new wxStaticBox(itemPanelNotebook03, wxID_ANY, _("Export Data to File"));
    wxStaticBoxSizer* itemStaticBoxSizerFileExp = new wxStaticBoxSizer(itemStaticBoxFileExp, wxHORIZONTAL);
    itemBoxSizer06->Add(itemStaticBoxSizerFileExp, 0, wxEXPAND | wxALL, m_border_size);
    wxFlexGridSizer *itemFlexGridSizerFileExp = new wxFlexGridSizer(2);
    itemFlexGridSizerFileExp->AddGrowableCol(1);
    itemStaticBoxSizerFileExp->Add(itemFlexGridSizerFileExp, 1, wxEXPAND | wxALL, 0);
    //    wxStaticText* itemStaticTextDummy2 = new wxStaticText(itemPanelNotebook03, wxID_ANY, _(""), wxDefaultPosition, wxDefaultSize, 0);
    //    itemFlexGridSizerFileExp->Add(itemStaticTextDummy2, 0, wxEXPAND | wxALL, m_border_size);
    //--------------------
    //--------------------
    m_ExpFileData01 = new wxCheckBox(itemPanelNotebook03, wxID_ANY, _("Prepend Clockticks"));
    itemFlexGridSizerFileExp->Add(m_ExpFileData01, 0, wxEXPAND, 5);
    m_ExpFileData01->SetValue(g_bDataExportClockticks);
    m_ExpFileData01->SetToolTip(_("Adds Clockticks to the data exports of BaroHistory, PolarPerformance and WindHistory"));
	//--------------------
    //--------------------
    m_ExpFileData02 = new wxCheckBox(itemPanelNotebook03, wxID_ANY, _("Prepend UTC Timestamp"));
    itemFlexGridSizerFileExp->Add(m_ExpFileData02, 0, wxEXPAND, 5);
    m_ExpFileData02->SetValue(g_bDataExportUTC);
    m_ExpFileData02->SetToolTip(_("Adds ISO8601 UTC-Date&Time to the data exports of BaroHistory, PolarPerformance and WindHistory"));

    wxStaticText* itemStaticText31 = new wxStaticText(itemPanelNotebook03, wxID_ANY, _("Data Separator :"), wxDefaultPosition, wxDefaultSize, 0);
    itemFlexGridSizerFileExp->Add(itemStaticText31, 0, wxEXPAND | wxALL, m_border_size);
    m_pDataExportSeparator = new wxTextCtrl(itemPanelNotebook03, wxID_ANY, g_sDataExportSeparator, wxDefaultPosition, wxSize(30, -1), wxTE_LEFT);
    itemFlexGridSizerFileExp->Add(m_pDataExportSeparator, 0, wxALL, m_border_size);
    m_pDataExportSeparator->SetToolTip(_("Sets the separator for the data exports of BaroHistory, PolarPerformance and WindHistory;"));

    //****************************************************************************************************
	m_PersistentChartPolarAnimation = new wxCheckBox(itemPanelNotebook03, wxID_ANY, _("Persistent Chart Perf. Animations"));
	itemFlexGridSizerExpData->Add(m_PersistentChartPolarAnimation, 0, wxEXPAND, 5);
    m_PersistentChartPolarAnimation->SetValue(g_bPersistentChartPolarAnimation);

    return;
}

void TacticsPreferencesDialog::OnManualHeelUpdate(wxCommandEvent& event)
{
	if (m_ButtonFixedLeeway->GetValue() || m_ButtonHeelInput->GetValue()){
		if (m_CorrectAWwithHeel->IsChecked()){
			wxMessageBox(_("This will also disable the AWA/AWS correction."));
			m_CorrectAWwithHeel->SetValue(false);
		}
	}
}

void TacticsPreferencesDialog::OnAWSAWACorrectionUpdated(wxCommandEvent& event)
{ // check if heel is available
	//...
	if (!m_ButtonUseHeelSensor->GetValue()){
		wxMessageBox(_("This option makes only sense with a real heel sensor."));
		m_CorrectAWwithHeel->SetValue(false);
	}
	else{
		//display warning
		if (m_CorrectAWwithHeel->IsChecked()){
			wxMessageBox(_("Make sure your instruments do not internally correct AWS / AWA with heel.\nThis may result in wrong wind data."));
		}
	}
}

void TacticsPreferencesDialog::SelectPolarFile(wxCommandEvent& event)
{
	wxFileDialog fdlg(
        GetOCPNCanvasWindow(), _("Select a Polar-File"), _T(""));
	if (fdlg.ShowModal() == wxID_CANCEL)
        return;
	g_path_to_PolarFile = fdlg.GetPath();
	BoatPolar->loadPolar(g_path_to_PolarFile);
	if (m_pTextCtrlPolar)
        m_pTextCtrlPolar->SetValue(g_path_to_PolarFile);
}

void TacticsPreferencesDialog::SaveTacticsConfig()
{
    g_dLeewayFactor = m_LeewayFactor->GetValue();
    g_dfixedLeeway = m_fixedLeeway->GetValue();

    g_dalpha_currdir = (double)m_AlphaCurrDir->GetValue() / 1000.0;
    //    g_dalpha_currdir = m_AlphaCurrDir->GetValue();
    g_dalphaDeltCoG = m_alphaDeltCoG->GetValue();
    g_dalphaLaylinedDampFactor = m_alphaLaylineDampFactor->GetValue();
    g_dLaylineLengthonChart = m_pLaylineLength->GetValue();
    g_iMinLaylineWidth = m_minLayLineWidth->GetValue();
    g_iMaxLaylineWidth = m_maxLayLineWidth->GetValue();
    g_bDisplayCurrentOnChart = m_CurrentOnChart->GetValue();
    g_dheel[1][1] = m_heel5_45->GetValue();
    g_dheel[1][2] = m_heel5_90->GetValue();
    g_dheel[1][3] = m_heel5_135->GetValue();
    g_dheel[2][1] = m_heel10_45->GetValue();
    g_dheel[2][2] = m_heel10_90->GetValue();
    g_dheel[2][3] = m_heel10_135->GetValue();
    g_dheel[3][1] = m_heel15_45->GetValue();
    g_dheel[3][2] = m_heel15_90->GetValue();
    g_dheel[3][3] = m_heel15_135->GetValue();
    g_dheel[4][1] = m_heel20_45->GetValue();
    g_dheel[4][2] = m_heel20_90->GetValue();
    g_dheel[4][3] = m_heel20_135->GetValue();
    g_dheel[5][1] = m_heel25_45->GetValue();
    g_dheel[5][2] = m_heel25_90->GetValue();
    g_dheel[5][3] = m_heel25_135->GetValue();

    g_bUseHeelSensor = m_ButtonUseHeelSensor->GetValue();
    g_bUseFixedLeeway = m_ButtonFixedLeeway->GetValue();
    g_bManHeelInput = m_ButtonHeelInput->GetValue();
    g_path_to_PolarFile = m_pTextCtrlPolar->GetValue();
    g_bCorrectSTWwithLeeway = m_CorrectSTWwithLeeway->GetValue();
    g_bCorrectAWwithHeel = m_CorrectAWwithHeel->GetValue();
    g_bForceTrueWindCalculation = m_ForceTrueWindCalculation->GetValue();
    g_bUseSOGforTWCalc = m_UseSOGforTWCalc->GetValue();
    g_bShowWindbarbOnChart = m_ShowWindbarbOnChart->GetValue();
    g_bShowPolarOnChart = m_ShowPolarOnChart->GetValue();
    g_bExpPerfData01 = m_ExpPerfData01->GetValue();
    g_bExpPerfData02 = m_ExpPerfData02->GetValue();
    g_bExpPerfData03 = m_ExpPerfData03->GetValue();
    g_bExpPerfData04 = m_ExpPerfData04->GetValue();
    g_bExpPerfData05 = m_ExpPerfData05->GetValue();
    g_bPersistentChartPolarAnimation =
        m_PersistentChartPolarAnimation->GetValue();
    g_bDataExportClockticks= m_ExpFileData01->GetValue();
    g_bDataExportUTC =  m_ExpFileData02->GetValue();
    g_sDataExportSeparator = m_pDataExportSeparator->GetValue();
}

//----------------------------------------------------------------
//    Tactics Window Implementation
//    porting note: as virtual parent for child window class
//----------------------------------------------------------------

TacticsWindow::TacticsWindow (
    wxWindow *pparent, wxWindowID id,
    tactics_pi *tactics, const wxString derivtitle ) :
    wxWindow(
        pparent, id, wxDefaultPosition, wxDefaultSize,
        wxBORDER_NONE || wxFULL_REPAINT_ON_RESIZE, derivtitle )
{
    m_plugin = tactics;
    return;
}

TacticsWindow::~TacticsWindow()
{
    return;
}

void TacticsWindow::InsertTacticsIntoContextMenu ( wxMenu *contextMenu )
{
    wxMenuItem* btnShowLaylines = contextMenu->AppendCheckItem(
        ID_DASH_LAYLINE, _(L"\u2191Show Laylines"));
    btnShowLaylines->Check(m_plugin->GetLaylineVisibility());

    wxMenuItem* btnShowCurrent = contextMenu->AppendCheckItem(
        ID_DASH_CURRENT, _(L"\u2191Show Current"));
    btnShowCurrent->Check(m_plugin->GetCurrentVisibility());

	wxMenuItem* btnShowWindbarb = contextMenu->AppendCheckItem(
        ID_DASH_WINDBARB, _(L"\u2191Show Windbarb"));
	btnShowWindbarb->Check(m_plugin->GetWindbarbVisibility());

	wxMenuItem* btnShowPolar = contextMenu->AppendCheckItem(
        ID_DASH_POLAR, _(L"\u2191Show Polar"));
	btnShowPolar->Check(m_plugin->GetPolarVisibility());

    return;
}

void TacticsWindow::TacticsInContextMenuAction ( const int eventId )
{
    bool toggled = false;
	switch (eventId){
	case ID_DASH_LAYLINE: {
		m_plugin->ToggleLaylineRender();
        toggled = true;
        break;
	}
	case ID_DASH_CURRENT: {
		m_plugin->ToggleCurrentRender();
        toggled = true;
        break;
	}
	case ID_DASH_POLAR: {
		m_plugin->TogglePolarRender();
        toggled = true;
        break;
	}
	case ID_DASH_WINDBARB: {
		m_plugin->ToggleWindbarbRender();
        toggled = true;
        break;
	}
	}
    /* Port note: Parent cannot save for child,
       child cannot save for parent. */
    if ( toggled )
       m_plugin->SaveConfig();

	return;

}
void TacticsWindow::SendPerfSentenceToAllInstruments(
    unsigned long long st, double value, wxString unit, long long timestamp )
{
    m_plugin->SendSentenceToAllInstruments( st, value, unit, timestamp );
}
void TacticsWindow::SetUpdateSignalK(
        wxString *type, wxString *sentenceId, wxString *talker, wxString *src, int pgn,
        wxString *path, double value, wxString *valStr, long long timestamp, wxString *key )
{
    m_plugin->SetUpdateSignalK( type, sentenceId, talker, src, pgn, path, value, valStr, timestamp, key );
}
