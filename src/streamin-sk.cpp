/**************************************************************************
* $Id: streamout.cpp, v1.0 2019/08/08 DarthVader Exp $
*
* Project:  OpenCPN
* Purpose:  Tactics Plugin
* Author:   Petri Makijarvi
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
#include <limits>
#ifndef WX_PRECOMP
#include <wx/wx.h>
#endif
#include <wx/wfstream.h> 
#include <wx/fileconf.h>
#include <wx/socket.h>
#include <wx/sckstrm.h>

#include "wx/jsonreader.h"
#include "plugin_ids.h"

#include "dashboard_pi.h"
#include "streamin-sk.h"

extern int g_iDashWindSpeedUnit;
extern int g_iDashSpeedUnit;
extern int g_iSpeedFormat;

wxBEGIN_EVENT_TABLE (TacticsInstrument_StreamInSkSingle, DashboardInstrument)
   EVT_THREAD (myID_THREAD_SK_IN, TacticsInstrument_StreamInSkSingle::OnThreadUpdate)
   EVT_TIMER (myID_TICK_SK_IN, TacticsInstrument_StreamInSkSingle::OnStreamInSkUpdTimer)
   EVT_CLOSE (TacticsInstrument_StreamInSkSingle::OnClose)
wxEND_EVENT_TABLE ()

// ----------------------------------------------------------------
//
//    TacticsInstrument_StreamInSkSingle
//
//----------------------------------------------------------------
TacticsInstrument_StreamInSkSingle::TacticsInstrument_StreamInSkSingle(
    DashboardWindow *pparent, wxWindowID id, wxString title, unsigned long long cap_flag, wxString format,
    std::mutex &mtxNofStreamInSk, int &nofStreamInSk, wxString &echoStreamerInSkShow, wxString confdir)
	:DashboardInstrument(pparent, id, title, cap_flag)
{
    m_frame = this;
    m_pparent = pparent;
    std::unique_lock<std::mutex> lck_mtxNofStreamInSk( mtxNofStreamInSk);
    nofStreamInSk++;
    m_mtxNofStreamInSk = &mtxNofStreamInSk;
    m_nofStreamInSk = &nofStreamInSk;
    m_echoStreamerInSkShow = &echoStreamerInSkShow;
    m_state = SSKM_STATE_UNKNOWN;
    m_timer = NULL;

    wxString emptyStr = wxEmptyString;
    emptyStr = emptyStr.wc_str();

    m_data  = emptyStr;
    if ( nofStreamInSk > 1) {
        m_timer = new wxTimer( this, myID_TICK_SK_IN );
        m_timer->Start( SSKM_TICK_COUNT, wxTIMER_CONTINUOUS );
        m_state = SSKM_STATE_DISPLAYRELAY;
        m_data = echoStreamerInSkShow;
        return;
    } // check against case that there is multiple disaplays and this is just slave
    m_state = SSKM_STATE_INIT;
    m_data += L"\u2013 \u2013 \u2013";
    echoStreamerInSkShow = m_data;
    m_format = format;
    m_DataHeight = 0;
    m_confdir = confdir;
    m_pconfig = GetOCPNConfigObject();
    m_configFileName = wxEmptyString;

    m_source = emptyStr;
    m_connectionRetry = 0;
    m_timestamps = emptyStr;
    m_stamp = true;
    m_verbosity = 0;

    m_configured = LoadConfig();

    if ( !m_configured )
        return;
    m_state = SSKM_STATE_CONFIGURED;

    m_stateComm = SKTM_STATE_UNKNOWN;
    m_cmdThreadStop = false;
    m_updatesSent = 0;
    m_startGraceCnt = 0;
    m_threadMsg = emptyStr;
    if ( CreateThread() != wxTHREAD_NO_ERROR ) {
        if ( m_verbosity > 0)
            wxLogMessage ("dashboard_tactics_pi: Delta Streamer FAILED : Signal K Delta Streamer : could not create communication thread.");
        m_state = SSKM_STATE_FAIL;
        return;
    } // will not talk
    m_thread = GetThread();
    m_thread->SetPriority( ((wxPRIORITY_MAX * 5) / 10) );
    if ( m_thread->Run() != wxTHREAD_NO_ERROR ) {
        if ( m_verbosity > 0)
            wxLogMessage ("dashboard_tactics_pi: Delta Streamer FAILED : Signal K Delta Streamer: cannot run communication thread.");
        m_state = SSKM_STATE_FAIL;
        return;
    }
    m_timer = new wxTimer( this, myID_TICK_SK_IN );
    m_timer->Start( SSKM_TICK_COUNT, wxTIMER_CONTINUOUS );
    m_state = SSKM_STATE_READY;
}
/***********************************************************************************

************************************************************************************/
TacticsInstrument_StreamInSkSingle::~TacticsInstrument_StreamInSkSingle()
{
    std::unique_lock<std::mutex> lck_mtxNofStreamInSk( *m_mtxNofStreamInSk);
    (*m_nofStreamInSk)--;
    if ( m_state == SSKM_STATE_DISPLAYRELAY )
        return;
    if ( this->m_timer != NULL ) {
        this->m_timer->Stop();
        delete this->m_timer;
    }
    m_data = L"\u2013 HALT \u2013";
    *m_echoStreamerInSkShow = m_data;
    SaveConfig();
    if ( GetThread() ) {
        if ( m_thread->IsRunning() ) {
            m_cmdThreadStop = true;
            m_socket.InterruptWait();
            m_thread->Wait();
        }
    }
}
/***********************************************************************************

************************************************************************************/
wxSize TacticsInstrument_StreamInSkSingle::GetSize(int orient, wxSize hint)
{
	wxClientDC dc(this);
	int w;
	dc.GetTextExtent(m_title, &w, &m_TitleHeight, 0, 0, g_pFontTitle);
	dc.GetTextExtent(_T("000"), &w, &m_DataHeight, 0, 0, g_pFontData);

	if (orient == wxHORIZONTAL) {
		return wxSize(DefaultWidth, wxMax(hint.y, m_TitleHeight + m_DataHeight));
	}
	else {
		return wxSize(wxMax(hint.x, DefaultWidth), m_TitleHeight + m_DataHeight);
	}
}
/***********************************************************************************

************************************************************************************/
void TacticsInstrument_StreamInSkSingle::sLL(long long cnt, wxString &retString)
{
    wxLongLong wxLL = cnt;
    wxString sBuffer = wxLL.ToString();
    retString = sBuffer.wc_str();
}
/***********************************************************************************

************************************************************************************/
void TacticsInstrument_StreamInSkSingle::Draw(wxGCDC* dc)
{
	wxColour cl;
#ifdef __WXMSW__
	wxBitmap tbm(dc->GetSize().x, m_DataHeight, -1);
	wxMemoryDC tdc(tbm);
	wxColour c2;
	GetGlobalColor(_T("DASHB"), &c2);
	tdc.SetBackground(c2);
	tdc.Clear();

	tdc.SetFont(*g_pFontData);
	GetGlobalColor(_T("DASHF"), &cl);
	tdc.SetTextForeground(cl);

	tdc.DrawText(m_data, 10, 0);

	tdc.SelectObject(wxNullBitmap);

	dc->DrawBitmap(tbm, 0, m_TitleHeight, false);
#else
	dc->SetFont(*g_pFontData);
	GetGlobalColor(_T("DASHF"), &cl);
	dc->SetTextForeground(cl);

	dc->DrawText(m_data, 10, m_TitleHeight);

#endif

}
/***********************************************************************************

************************************************************************************/
void TacticsInstrument_StreamInSkSingle::OnClose( wxCloseEvent &evt )
{
    if ( m_state == SSKM_STATE_DISPLAYRELAY )
        return;
    if ( m_verbosity > 1)
        wxLogMessage ("dashboard_tactics_pi: OnClose() : received CloseEvent, waiting on comm. thread.");
    if ( GetThread() ) {
        if ( m_thread->IsRunning() ) {
            m_cmdThreadStop = true;
            m_socket.InterruptWait();
            m_thread->Wait();
        }
    }
    Destroy();
}
/***********************************************************************************

************************************************************************************/
wxThread::ExitCode TacticsInstrument_StreamInSkSingle::Entry( )
{
#define giveUpConnectionRetry100ms(__MS_100__) int waitMilliSeconds = 0; \
    while ( (!m_thread->TestDestroy()) && (waitMilliSeconds < (m_connectionRetry * __MS_100__ * 100)) ) { \
    wxMilliSleep( 100 ); \
    waitMilliSeconds += 100; \
}
#define __NOT_STOP_THREAD__ (!m_thread->TestDestroy() && !m_cmdThreadStop)
#define __STOP_THREAD__ (m_thread->TestDestroy() || m_cmdThreadStop)

    m_stateComm = SKTM_STATE_INIT;

    // wxSocketBase::Initialize(); // note: for eventual unit test, not for production
    m_socket.SetTimeout( m_connectionRetry );
    m_socket.SetFlags( wxSOCKET_BLOCK );
    wxIPV4address  *address = new wxIPV4address();
    wxThreadEvent   event( wxEVT_THREAD, myID_THREAD_SK_IN );

    wxUniChar separator = 0x3a;
    address->Hostname(m_source.BeforeFirst(separator));
    address->Service(m_source.AfterFirst(separator));
    if ( m_verbosity > 1) {
        m_threadMsg = wxString::Format(
            "dashboard_tactics_pi: Signal K Delta Streamer : SKTM_STATE_INIT : (%s:%s)",
            m_source.BeforeFirst(separator), m_source.AfterFirst(separator));
        wxQueueEvent( m_frame, event.Clone() );
    }

    wxString sCnxPrg[3];
    sCnxPrg[0] = L"\u2013 \u2013 \u2190";
    sCnxPrg[1] = L"\u2013 \u2190 \u2013";
    sCnxPrg[2] = L"\u2190 \u2013 \u2013";
    int iCnxPrg = 2;

    wxString header = wxEmptyString;
    header += "GET ";
    header += " HTTP/1.1\r\n";

    header += "Host: ";
    header += m_source;
    header += "\r\n";	

    header += "User-Agent: OpenCPN/5.0\r\n";
    header += "Accept: */*\r\n";

    header += "Content-Type: application/x-www-form-urlencoded";
    header += "\r\n";
    header += "Connection: keep-alive";
    header += "\r\n";
    header += "Content-Length: "; // from this starts the dynamic part

    m_stateComm = SKTM_STATE_CONNECTING;
    int prevStateComm = m_stateComm;

    while (__NOT_STOP_THREAD__) {

        if ( (m_stateComm == SKTM_STATE_CONNECTING) || (m_stateComm == SKTM_STATE_ERROR) ) {
            wxString connectionErr = wxEmptyString;
            giveUpConnectionRetry100ms(5);
            if ( __NOT_STOP_THREAD__ ) {
                ( (iCnxPrg >= 2) ? iCnxPrg = 0 : iCnxPrg++ );
                m_data = sCnxPrg[iCnxPrg];
                *m_echoStreamerInSkShow = m_data;
                if ( !m_socket.Connect( *address, false ) ) {
                    if ( !m_socket.WaitOnConnect() ) {
                        connectionErr += _T(" (timeout)");
                    }
                    else {
                        if ( !m_socket.IsConnected() ) {
                            connectionErr += _T(" (refused by peer)");
                        }
                    }
                }
                if ( __NOT_STOP_THREAD__) {
                    if ( connectionErr.IsEmpty() ) {
                        m_stateComm = SKTM_STATE_WAITING;
                        if ( prevStateComm != m_stateComm ) {
                            if ( m_verbosity > 1) {
                                m_threadMsg = _T("dashboard_tactics_pi: SignalK Delta Streamer : SKTM_STATE_WAITING");
                                wxQueueEvent( m_frame, event.Clone() );
                            }
                        }
                        prevStateComm = m_stateComm;
                    }
                    else {
                        m_stateComm = SKTM_STATE_ERROR;
                        if ( prevStateComm != m_stateComm ) {
                            if ( m_verbosity > 1) {
                                m_threadMsg = _T("dashboard_tactics_pi: SignalK Delta Streamer : SKTM_STATE_ERROR");
                                m_threadMsg += connectionErr;
                                wxQueueEvent( m_frame, event.Clone() );
                            }
                        }
                        prevStateComm = m_stateComm;
                        giveUpConnectionRetry100ms(5);
                    } // else error state wait until attempting again
                } // then thread does not need to terminate
            } // then thread does not need to terminate
        } // then need to attempt to connect()
        
        if ( m_stateComm == SKTM_STATE_WAITING ) {

            m_data = sCnxPrg[iCnxPrg];
            *m_echoStreamerInSkShow = m_data;
            
            wxString sData = wxEmptyString; // no payload this time

            if (__STOP_THREAD__)
                        break;
                
            wxString sHdrOut = header;
            sHdrOut += wxString::Format("%d", sData.Len() );
            sHdrOut += "\r\n";
            sHdrOut += "\r\n";
            sHdrOut += sData;
            wxScopedCharBuffer scb = sHdrOut.mb_str();
            size_t len = scb.length();
            
            m_socket.Write( scb.data(), len );

            if ( m_verbosity > 4) {
                m_threadMsg = wxString::Format("dashboard_tactics_pi: streamin-sk: written to socket:\n%s", sHdrOut);
                wxQueueEvent( m_frame, event.Clone() );
            } // for big time debugging only, use tail -f opencpn.log | grep dashboard_tactics_pi
            
            if ( m_socket.Error() ) {
                m_stateComm = SKTM_STATE_ERROR;
            }
            else {

                while ( __NOT_STOP_THREAD__ && (m_stateComm == SKTM_STATE_WAITING) ) {
                    bool readAvailable = false;
                    bool timeOut = false;
                    char c;
                    ( m_socket.Peek(&c,1).LastCount()==0 ? readAvailable = false : readAvailable = true );
                    if ( !readAvailable ) {
                        wxLongLong startWait = wxGetUTCTimeMillis();
                        readAvailable = m_socket.WaitForRead( );
                        if ( !readAvailable) {
                            wxLongLong endWait = wxGetUTCTimeMillis();
                            if ( (endWait.GetValue() - startWait.GetValue()) >= ( m_connectionRetry * 1000 ) ) {
                                m_socket.Close();
                                m_stateComm = SKTM_STATE_ERROR;
                                timeOut = true;
                            }
                        }
                    }
                    if ( (__STOP_THREAD__) || timeOut )
                        break;

                    if ( readAvailable ) {

                        wxSocketInputStream streamin( (wxSocketBase &)m_socket );
                        wxLongLong wxllNowMs;
                        long long  msNow;
                        bool syncerror = false;
                        m_stateComm = SKTM_STATE_READY;
                        if ( m_verbosity > 1) {
                            m_threadMsg = _T("dashboard_tactics_pi: SignalK Delta Streamer : SKTM_STATE_READY");
                            wxQueueEvent( m_frame, event.Clone() );
                        }

                        while ( __NOT_STOP_THREAD__ && !syncerror ) {

                            try {
                                wxJSONValue  root;
                                wxJSONReader reader;
                                int numErrors = reader.Parse( (wxInputStream &) streamin, &root );
                                wxllNowMs = wxGetUTCTimeMillis();
                                msNow = wxllNowMs.GetValue();
                                if ( numErrors > 0 )  {
                                    if ( m_verbosity > 1 ) {
                                        const wxArrayString& errors = reader.GetErrors();
                                        for (int i = 0; ((i < numErrors) && ( i < 10 ) ); i++) {
                                            m_threadMsg = wxString::Format(
                                                "dashboard_tactics_pi: ERROR - parsing errors in the streaming (%d):\n%s\n",
                                                i, errors[i] );
                                            wxQueueEvent( m_frame, event.Clone() );
                                            wxMilliSleep(20);
                                        }
                                    }
                                    throw 1; // lost sync, it is quite useless to continue
                                } // then issues in the parsing
                                bool hasUpdates = root.HasMember( "updates" );
                                if ( hasUpdates) {
                                    wxJSONValue updates = root["updates"];
                                    if ( !updates.IsArray() ) throw 100;
                                    int asize = updates.Size();
                                    if ( asize == 0 ) throw 105;
                                    for ( int i = 0; i < asize; i++ ) {
                                        if ( !updates[i].HasMember( "source" ) ) throw (i+1)*100000 + 110;
                                        wxJSONValue source = updates[i]["source"];
                                        if ( !source.HasMember( "type" ) ) throw (i+1)*100000 + 115;
                                        wxString type = source["type"].AsString();
                                        wxString sentence = wxEmptyString;
                                        wxString talker = wxEmptyString;
                                        wxString src = wxEmptyString;
                                        int pgn = 0;
                                        if ( type.IsSameAs("NMEA0183", false) ) {
                                            if ( !source.HasMember( "sentence" ) ) throw (i+1)*100000 + 1000;
                                            sentence = source["sentence"].AsString();
                                            if ( !source.HasMember( "talker" ) ) throw (i+1)*100000 + 1005;
                                            talker = source["talker"].AsString();
                                        }
                                        else if ( type.IsSameAs("NMEA2000", false) ) {
                                            if ( !source.HasMember( "src" ) ) throw (i+1)*100000 + 2000;
                                            src = source["src"].AsString();
                                            if ( !source.HasMember( "pgn" ) ) throw (i+1)*100000 + 2005;
                                            pgn = source["pgn"].AsInt();
                                        }
                                        else throw (i+1)*10000 + 3000;
                                        if ( !updates[i].HasMember( "timestamp" ) ) throw (i+1)*100000 + 4000;
                                        wxString timestamp = updates[i]["timestamp"].AsString();
                                        if ( !m_stamp ) {
                                            wxDateTime utc0; // Signal K https://tools.ietf.org/html/rfc3339
                                            if (utc0.ParseISOCombined( timestamp.BeforeLast('.') ) ) { // rfc3359 not understood
                                                utc0 = utc0.FromTimezone(wxDateTime::UTC); // (w/ DST):the "Z", still no ms
                                                wxllNowMs = utc0.GetValue();
                                                msNow = wxllNowMs.GetValue();
                                                wxString millis = timestamp.AfterLast('.');
                                                millis = millis.BeforeLast('Z');
                                                long long retMs;
                                                (void) millis.ToLongLong( &retMs );
                                                msNow += retMs;
                                                if ( m_verbosity > 4) {
                                                    wxString msNowLLString = wxEmptyString;
                                                    sLL ( msNow, msNowLLString );
                                                    m_threadMsg = wxString::Format(
                                                        "dashboard_tactics_pi: Signal K timestamp (%s) msNow (%s)",
                                                        timestamp, msNowLLString );
                                                    wxQueueEvent( m_frame, event.Clone() );
                                                } // then slowing down with the indirect debug log
                                            } // then conversion OK
                                            else {
                                                if ( m_verbosity > 3) {
                                                    m_threadMsg = wxString::Format(
                                                        "dashboard_tactics_pi: ERROR Signal K JSON updates: timestamp %s?",
                                                        timestamp);
                                                    wxQueueEvent( m_frame, event.Clone() );
                                                } // then a serious problem but let's throttle a bit
                                            } // else a problem which may be recurrant and filling log quickly
                                        } // then use server's timestamp
                                        if ( !updates[i].HasMember( "values" ) ) throw (i+1)*100000 + 5000;
                                        wxJSONValue values = updates[i]["values"];
                                        if ( !values.IsArray() ) throw (i+1)*100000 + 5005;
                                        int vsize = values.Size();
                                        if ( vsize < 0 ) throw (i+1)*10000 + 5010;
                                        for ( int v = 0; v < vsize; v++ ) {
                                            if ( !values[v].HasMember( "path" )) throw (i+1)*100000 + (v+1)*10000 + 5015;
                                            wxString path = values[v]["path"].AsString();
                                            if ( !values[v].HasMember( "value" )) throw (i+1)*100000 + (v+1)*10000 + 5020;
                                            double   value = 0.0;
                                            int      valInt = 0;
                                            wxString valStr = wxEmptyString;
                                            wxJSONValue valueset = values[v]["value"];
                                            if ( !valueset.IsObject() ) {
                                                if ( valueset.IsDouble() ) {
                                                    value = valueset.AsDouble();
                                                }
                                                else if ( valueset.IsInt() ) {
                                                    valInt = valueset.AsInt();
                                                    value = static_cast<double>(valInt);
                                                }
                                                valStr = valueset.AsString();
                                                m_pparent->SetUpdateSignalK (
                                                    &type, &sentence, &talker, &src, pgn, &path, value, &valStr, msNow );
                                                if ( m_verbosity > 3) {
                                                    m_threadMsg = wxString::Format(
                                                        "dashboard_tactics_pi: Signal K type (%s) sentence (%s) talker (%s) "
                                                        "src (%s) pgn (%d) timestamp (%s) path (%s) value (%f), valStr (%s)",
                                                        type, sentence, talker, src, pgn, timestamp, path, value, valStr);
                                                    wxQueueEvent( m_frame, event.Clone() );
                                                } // then slowing down with the indirect debug log
                                            }
                                            else {
                                                wxArrayString names = valueset.GetMemberNames();
                                                for ( int n = 0; (size_t) n < names.GetCount(); n++ ) {
                                                    wxString key = names[n];
                                                    if ( !valueset.HasMember( key ) ) throw (n+1)*1000000 + (i+1)*100000 + (v+1)*10000 + 5025;
                                                    if ( !valueset[ key ].IsArray() ) {
                                                        if ( valueset[ key ].IsDouble() ) {
                                                            value = valueset[ key ].AsDouble();
                                                        }
                                                        else if ( valueset[ key ].IsInt() ) {
                                                            valInt = valueset[ key ].AsInt();
                                                            value = static_cast<double>(valInt);
                                                        }
                                                        valStr = valueset[ key ].AsString();
                                                    }
                                                    else {
                                                        if ( valueset[ key ].Size() > 0 ) {
                                                            if ( valueset[ key ][0].IsDouble() ) {
                                                                value = valueset[ key ][0].AsDouble();
                                                            }
                                                            else if ( valueset[ key ][0].IsInt() ) {
                                                                valInt = valueset[ key ][0].AsInt();
                                                                value = static_cast<double>(valInt);
                                                            }
                                                            valStr = valueset[ key ][0].AsString();
                                                        } 
                                                    }
                                                    m_pparent->SetUpdateSignalK (
                                                        &type, &sentence, &talker, &src, pgn, &path, value, &valStr, msNow, &key );
                                                    if ( m_verbosity > 3) {
                                                        m_threadMsg = wxString::Format(
                                                            "dashboard_tactics_pi: Signal K type (%s) sentence (%s) talker (%s) "
                                                            "src (%s) pgn (%d) timestamp (%s) path (%s) key (%s) value (%f), valStr (%s)",
                                                            type, sentence, talker, src, pgn, timestamp, path, key, value, valStr);
                                                        wxQueueEvent( m_frame, event.Clone() );
                                                    } // then slowing down with the indirect debug log
                                                } 
                                            }
                                        
                                            m_updatesSent += 1;
                                        
                                        } // for items in the array of values
                                    } // for items in the array of updates
                                } // then has updates
                            }
                            catch (int x) {
                                if ( x == 1 ) {
                                    syncerror = true;
                                    m_stateComm = SKTM_STATE_ERROR;
                                    m_socket.Close();
                                    if ( m_verbosity > 1 ) {
                                        m_threadMsg = wxString::Format(
                                            "dashboard_tactics_pi: ERROR Signal K JSON updates: sync lost, waiting...");
                                        wxQueueEvent( m_frame, event.Clone() );
                                    }
                                    wxMilliSleep( 100 );
                                    break;
                                } // then we've probably lost the connection - sync error anyway, difficult to catch, try anyway
                                else {
                                    if ( m_verbosity > 1 ) {
                                    m_threadMsg = wxString::Format(
                                        "dashboard_tactics_pi: ERROR Signal K JSON updates: exception number %d.", x );
                                    wxQueueEvent( m_frame, event.Clone() );
                                    }
                                } // else a fatal error - this is probably a repeating event if it occurs, filling the logs if no filter
                            } // catch errors in reading
                        } // while continuously parsing from the socket input stream
                    } // then poked that a read() is possible
                } // while waiting on the socket for read()
            } // else socket is not in error after writing HTTP header into it
        } // then connected to the socket, waiting for transaction
    } // while not to be stopped / destroyed
        
    
    m_socket.Close();
    // wxSocketBase::Shutdown();  // note: for eventual unit test, not for production
    delete address;

    return (wxThread::ExitCode)0;
    
}
/***********************************************************************************

************************************************************************************/
void TacticsInstrument_StreamInSkSingle::OnThreadUpdate( wxThreadEvent &evt )
{
    if ( !m_threadMsg.IsEmpty() )         // NOTE: not to slow down the thread
        wxLogMessage ("%s", m_threadMsg); // w/ high debug rate can't catch them all!
    m_threadMsg = wxEmptyString;
}
/***********************************************************************************

************************************************************************************/
void TacticsInstrument_StreamInSkSingle::OnStreamInSkUpdTimer( wxTimerEvent &evt )
{
    if ( m_state == SSKM_STATE_DISPLAYRELAY ) {
        m_data = *m_echoStreamerInSkShow;
        return;
    }
    if ( m_thread->TestDestroy() || m_cmdThreadStop )
        return;
    if ( m_thread->IsAlive() ) {
        if ( m_thread->IsPaused() ) {
            if ( m_verbosity > 3) {
                wxLogMessage("dashboard_tactics_pi: DEBUG : OnStreamInSkUpdTime : m_thread IsPaused()");
                } // for big time debugging only, use tail -f opencpn.log | grep dashboard_tactics_pi
            m_thread->Resume();
        }
    }
    if ( m_startGraceCnt >= SSKM_START_GRACE_COUNT ) {
        if ( (m_stateComm == SKTM_STATE_READY) || (m_stateComm == SKTM_STATE_WAITING) ) {
            m_data = wxString::Format("%3d  [1/s]", m_updatesSent);
            *m_echoStreamerInSkShow = m_data;
        }
    }
    else {
        m_startGraceCnt ++;
        if ( (m_stateComm == SKTM_STATE_READY) && (m_updatesSent > 0) ) {
            m_data = ( (m_startGraceCnt % 2) == 0? L"  \u2665" : L"" );
            *m_echoStreamerInSkShow = m_data;
        }
    }
    m_updatesSent = 0;
}
/***********************************************************************************

************************************************************************************/
bool TacticsInstrument_StreamInSkSingle::LoadConfig()
{
    if ( *m_nofStreamInSk > 1 )
        return true;
    
    wxFileConfig *pConf = m_pconfig;
    
    if (!pConf)
        return false;
    pConf->SetPath(_T("/PlugIns/Dashboard/Tactics/SteaminSk/"));
    pConf->Read(_T("ConfigFile"), &m_configFileName, "streamin-sk.json");
    wxString s = wxFileName::GetPathSeparator();
    wxString confPath = m_confdir + m_configFileName;
    if ( !wxFileExists( confPath ) ) {
        wxString tmplPath  = *GetpSharedDataLocation();
        tmplPath += _T("plugins") + s + _T("dashboard_tactics_pi") + s + _T("data") + s + _T("streamin_sk_template.json");
        if ( !wxFileExists( tmplPath ) ) {
            wxLogMessage ("dashboard_tactics_pi: ERROR - missing template %s", tmplPath);
            m_data = L"\u2013 No template. \u2013";
            *m_echoStreamerInSkShow = m_data;
            return false;
        }
        bool ret = wxCopyFile ( tmplPath, confPath ); 
        if ( !ret ) {
            wxLogMessage ("dashboard_tactics_pi: ERROR - cannot copy template %s to %s", tmplPath, confPath);
            m_data = L"\u2013 ConfigFile? \u2013";
            *m_echoStreamerInSkShow = m_data;
            return false;
        }
    }

    try {
        wxFileInputStream jsonStream( confPath );
        wxJSONValue  root;
        wxJSONReader reader;
        int numErrors = reader.Parse( jsonStream, &root );

        if ( numErrors > 0 )  {
            const wxArrayString& errors = reader.GetErrors();
            wxMessageBox(
                _("Signal K Steamer configuration file parsing error, see log file."));
            for (int i = 0; ( ((size_t) i < errors.GetCount()) && ( i < 10 ) ); i++) {
                wxLogMessage(
                    "dashboard_tactics_pi: ERROR - parsing errors in the configuration file: %s", errors.Item(i) );
            }
            return false;
        }

        if ( !root.HasMember("streaminsk") ) throw 100;
        if ( !root["streaminsk"].HasMember("source") ) throw 101;
        m_source += root["streaminsk"]["source"].AsString();
        wxUniChar separator = 0x3a;
        if ( m_source.Find( separator ) == wxNOT_FOUND ) {
            wxLogMessage(
                "dashboard_tactics_pi: ERROR - Signal K source config file missing ':' in 'source', now : %s", m_source);
            return false;
        }
        if ( !root["streaminsk"].HasMember("api") ) throw 102;
        m_api += root["streaminsk"]["api"].AsString();
        if ( !root["streaminsk"].HasMember("connectionretry") ) throw 103;
        m_connectionRetry = root["streaminsk"]["connectionretry"].AsInt();
        if ( m_connectionRetry <=0)
            m_connectionRetry = 1; // cannot be <=0 ; used to throttle the thread
        if ( !root["streaminsk"].HasMember("timestamps") ) throw 104;
        m_timestamps += root["streaminsk"]["timestamps"].AsString();
        if ( m_timestamps.IsSameAs( _T("server"), false ) )
            m_stamp = false;
        if ( !root["streaminsk"].HasMember("verbosity") ) throw 105;
        m_verbosity = root["streaminsk"]["verbosity"].AsInt();

        if ( m_verbosity > 1 ) {
            wxLogMessage( "dashboard_tactics_pi: Signal K source       = \"%s\"",  m_source );
            wxLogMessage( "dashboard_tactics_pi: Signal K API version  = \"%s\"",  m_api );
            wxLogMessage( "dashboard_tactics_pi: Signal K conn.retry   = %d s.",   m_connectionRetry );
            wxLogMessage( "dashboard_tactics_pi: Signal K timestamps   = \"%s\"",  m_timestamps );
            wxLogMessage( "dashboard_tactics_pi: Streamer verbosity    = %d",      m_verbosity );
        }

    }
    catch (int x) {
        if ( (x >= 100) && (x < 200) ) {
            wxLogMessage ("dashboard_tactics_pi: JSON file %s parsing exception: missing expected item %d in 'streaminsk'",
                          confPath, (x - 100) );
        }
        wxMessageBox(_("Signal K Steamer configuration file parsing error, see log file."));

        return false;
        
    } // A JSON file can have errors which make this old JSON code to break

    return true;
}
/***********************************************************************************

************************************************************************************/
void TacticsInstrument_StreamInSkSingle::SaveConfig()
{
    if ( *m_nofStreamInSk > 1 )
        return;
    
    wxFileConfig *pConf = m_pconfig;
    
    if (!pConf)
        return;

    pConf->SetPath(_T("/PlugIns/Dashboard/Tactics/SteaminSk/"));
    pConf->Write(_T("ConfigFile"),m_configFileName);

    return;
}

/***********************************************************************************

************************************************************************************/
void TacticsInstrument_StreamInSkSingle::SetNMEASentence(wxString& delta)
{
    return;
}
