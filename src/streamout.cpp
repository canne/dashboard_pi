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

#include "streamout.h"
#include "ocpn_plugin.h"
#include "wx/jsonreader.h"
#include "plugin_ids.h"

extern int g_iDashWindSpeedUnit;
extern int g_iDashSpeedUnit;
extern int g_iSpeedFormat;

wxBEGIN_EVENT_TABLE (TacticsInstrument_StreamoutSingle, DashboardInstrument)
   EVT_THREAD (myID_THREAD_IFLXAPI, TacticsInstrument_StreamoutSingle::OnThreadUpdate)
   EVT_TIMER (myID_TICK_IFLXAPI, TacticsInstrument_StreamoutSingle::OnStreamOutUpdTimer)
   EVT_CLOSE (TacticsInstrument_StreamoutSingle::OnClose)
wxEND_EVENT_TABLE ()

// ----------------------------------------------------------------
//
//    TacticsInstrument_StreamoutSingle
//
//----------------------------------------------------------------
TacticsInstrument_StreamoutSingle::TacticsInstrument_StreamoutSingle(
    wxWindow *pparent, wxWindowID id, wxString title, unsigned long long cap_flag, wxString format,
    std::mutex &mtxNofStreamOut, int &nofStreamOut, wxString &echoStreamerShow, wxString confdir)
	:DashboardInstrument(pparent, id, title, cap_flag)
{
    m_frame = this;
    std::unique_lock<std::mutex> lck_mtxNofStreamOut( mtxNofStreamOut);
    nofStreamOut++;
    m_mtxNofStreamOut = &mtxNofStreamOut;
    m_nofStreamOut = &nofStreamOut;
    m_echoStreamerShow = &echoStreamerShow;
    m_state = SSSM_STATE_UNKNOWN;
    m_timer = NULL;

    wxString emptyStr = wxEmptyString;
    emptyStr = emptyStr.wc_str();

    m_data  = emptyStr;
    if ( nofStreamOut > 1) {
        wxString checkHalt = echoStreamerShow.wc_str();
        wxString isHalt = L"\u2013 HALT \u2013";
        if ( !checkHalt.IsSameAs( isHalt ) ) {
            m_state = SSSM_STATE_DISPLAYRELAY;
            m_data = echoStreamerShow;
            return;
        } // check against case that there is halted slave displays and this is a restart
    }
    m_state = SSSM_STATE_INIT;
    m_data += L"\u2013 \u2013 \u2013";
    echoStreamerShow = m_data;
    m_format = format;
    m_DataHeight = 0;
    m_confdir = confdir;
    m_pconfig = GetOCPNConfigObject();
    m_configFileName = wxEmptyString;

    m_target = emptyStr;
    m_targetAsFilePath = emptyStr;
    m_linesPerWrite = 0;
    m_api = emptyStr;
    m_org = emptyStr;
    m_bucket = emptyStr;
    m_precision = emptyStr;
    m_token = emptyStr;
    m_connectionRetry = 0;
    m_timestamps = emptyStr;
    m_stamp = true;
    m_verbosity = 0;

    m_configured = LoadConfig();

    if ( !m_configured )
        return;
    m_state = SSSM_STATE_CONFIGURED;

    m_pushedInFifo    = 0LL;
    m_poppedFromFifo  = 0LL;
    m_writtenToOutput = 0LL;
    m_stateFifoOverFlow = STSM_FIFO_OFW_UNKNOWN;
    std::unique_lock<std::mutex> init_m_mtxQLine( m_mtxQLine, std::defer_lock );
    m_stateComm = STSM_STATE_UNKNOWN;
    m_cmdThreadStop = false;
    m_threadMsg = emptyStr;
    if ( CreateThread() != wxTHREAD_NO_ERROR ) {
        if ( m_verbosity > 0)
            wxLogMessage ("dashboard_tactics_pi: DB Streamer FAILED : Influx DB Streamer : could not create communication thread.");
        m_state = SSSM_STATE_FAIL;
        return;
    } // will not talk
    m_thread = GetThread();
    m_thread->SetPriority( ((wxPRIORITY_MAX * 9) / 10) );
    if ( m_thread->Run() != wxTHREAD_NO_ERROR ) {
        if ( m_verbosity > 0)
            wxLogMessage ("dashboard_tactics_pi: DB Streamer FAILED : Influx DB Streamer: cannot run communication thread.");
        m_state = SSSM_STATE_FAIL;
        return;
    }
    m_timer = new wxTimer( this, myID_TICK_IFLXAPI );
    m_timer->Start( SSSM_TICK_COUNT, wxTIMER_CONTINUOUS );
    m_state = SSSM_STATE_READY;
}
/***********************************************************************************

************************************************************************************/
TacticsInstrument_StreamoutSingle::~TacticsInstrument_StreamoutSingle()
{
    std::unique_lock<std::mutex> lck_mtxNofStreamOut( *m_mtxNofStreamOut);
    (*m_nofStreamOut)--;
    if ( m_state == SSSM_STATE_DISPLAYRELAY )
        return;
    if ( this->m_timer != NULL ) {
        this->m_timer->Stop();
        delete this->m_timer;
    }
    m_data = L"\u2013 HALT \u2013";
    *m_echoStreamerShow = m_data;
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
wxSize TacticsInstrument_StreamoutSingle::GetSize(int orient, wxSize hint)
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
void TacticsInstrument_StreamoutSingle::Draw(wxGCDC* dc)
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
bool TacticsInstrument_StreamoutSingle::GetSchema(
    unsigned long long st, long long msNow, sentenceSchema &schema)
{
    for ( unsigned int i = 0; i < vSchema.size(); i++ ) {
        if ( vSchema[i].st == st ) {
            schema = vSchema[i];
            if ( !schema.bStore )
                return false;
            if ( schema.iInterval == 0 ) {
                schema.lastTimeStamp = msNow;
                vSchema[i].lastTimeStamp = msNow;
                return true;
            }
            long long timeLapse = (msNow - schema.lastTimeStamp) / 1000;
            if ( static_cast<long long>(vSchema[i].iInterval) <= timeLapse ) {
                schema.lastTimeStamp = msNow;
                vSchema[i].lastTimeStamp = msNow;
                return true;
            }
        } 
    }
    return false;
}
/***********************************************************************************

************************************************************************************/
void TacticsInstrument_StreamoutSingle::sLL(long long cnt, wxString &retString)
{
    wxLongLong wxLL = cnt;
    wxString sBuffer = wxLL.ToString();
    retString = sBuffer.wc_str();
}
/***********************************************************************************

************************************************************************************/
void TacticsInstrument_StreamoutSingle::SetData(unsigned long long st, double data, wxString unit, long long timestamp)
{
    wxLongLong wxllNowMs = wxGetUTCTimeMillis();

    if ( m_state == SSSM_STATE_DISPLAYRELAY ) {
        m_data = *m_echoStreamerShow;
        return;
    }

    if (std::isnan(data))
        return;

    if ( !( (m_state == SSSM_STATE_READY) && m_configured ) )
        return;
    
    sentenceSchema schema;
    long long msNow = ( timestamp == 0 ? wxllNowMs.GetValue() : timestamp );
    if ( !GetSchema( st, msNow, schema ) )
        return;
    
    lineProtocol line;
    
    line.measurement = schema.sMeasurement;
    if ( !schema.sProp1.IsEmpty() ) {
        line.tag_key1 = _T("prop1");
        line.tag_value1 = schema.sProp1;
        if ( !schema.sProp2.IsEmpty() ) {
            line.tag_key2 = _T("prop2");
            line.tag_value2 = schema.sProp2;
            if ( !schema.sProp3.IsEmpty() ) {
                line.tag_key3 = _T("prop3");
                line.tag_value3 = schema.sProp3;
            }
        }
    }
    line.field_key1 = schema.sField1;
    line.field_value1 = wxString::Format( "%f", data );
    if ( m_stamp )
        line.timestamp = wxString::Format( "%lld", msNow );

    std::unique_lock<std::mutex> lckmQline( m_mtxQLine );
    if ( m_stateComm == STSM_STATE_READY ) {
        long long pushDelta = m_pushedInFifo - m_poppedFromFifo;
        if ( m_stateFifoOverFlow == STSM_FIFO_OFW_NOT_BLOCKING ) {
            if ( pushDelta <= STSM_MAX_UNWRITTEN_FIFO_ELEMENTS_BLOCKING ) {
                qLine.push( line );
                m_pushedInFifo = m_pushedInFifo + 1LL;
            } // then FIFO has not been filled up yet to the limit
            else {
                m_stateFifoOverFlow = STSM_FIFO_OFW_BLOCKING;
                if ( m_verbosity > 0) {
                    wxString sPushedInFifo; sLL( m_pushedInFifo, sPushedInFifo );
                    wxString sPoppedFromFifo; sLL( m_poppedFromFifo, sPoppedFromFifo );
                    wxString sBlockingLimit; sLL( STSM_MAX_UNWRITTEN_FIFO_ELEMENTS_BLOCKING, sBlockingLimit );
                    wxString sPushDelta; sLL( pushDelta, sPushDelta );
                    wxLogMessage(
                        "dashboard_tactics_pi: SetData() : FIFO overflow (%s >= %s (limit)),\npushed: %s, popped: %s",
                        sPushDelta, sBlockingLimit, sPushedInFifo, sPoppedFromFifo);
                }
            } // else comm.thread is dead, connection lost or there is too much data in FIFO
        } // then the last information was that there was room in the FIFO
        else {
            if ( pushDelta <= STSM_MAX_UNWRITTEN_FIFO_ELEMENTS_UNBLOCKING ) {
                m_stateFifoOverFlow = STSM_FIFO_OFW_NOT_BLOCKING;
                if ( m_verbosity > 0) {
                    wxString sPushedInFifo; sLL( m_pushedInFifo, sPushedInFifo );
                    wxString sPoppedFromFifo; sLL( m_poppedFromFifo, sPoppedFromFifo );
                    wxString sPushDelta; sLL( pushDelta, sPushDelta );
                    wxString sUnblockingLimit; sLL( STSM_MAX_UNWRITTEN_FIFO_ELEMENTS_UNBLOCKING, sUnblockingLimit );
                    wxLogMessage(
                        "dashboard_tactics_pi: SetData() : FIFO back writable (%s <= %s (limit)),\npushed: %s, popped: %s",
                        sPushDelta, sUnblockingLimit, sPushedInFifo, sPoppedFromFifo);
                }
                qLine.push( line );
                m_pushedInFifo = m_pushedInFifo + 1LL;
            } // then FIFO is emptying and has gone under the threshold
        } // else there has been a FIFO overflow, let's check if we've passed under the threshold
    } // then the communication thread is alive and has connection with the server

}
/***********************************************************************************

************************************************************************************/
void TacticsInstrument_StreamoutSingle::OnClose( wxCloseEvent &evt )
{
    if ( m_state == SSSM_STATE_DISPLAYRELAY )
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
wxThread::ExitCode TacticsInstrument_StreamoutSingle::Entry( )
{
#define giveUpConnectionRetry100ms(__MS_100__) int waitMilliSeconds = 0; \
    while ( (!m_thread->TestDestroy()) && (waitMilliSeconds < (m_connectionRetry * __MS_100__ * 100)) ) { \
    wxMilliSleep( 100 ); \
    waitMilliSeconds += 100; \
}
#define __NOT_STOP_THREAD__ (!m_thread->TestDestroy() && !m_cmdThreadStop)
#define __STOP_THREAD__ (m_thread->TestDestroy() || m_cmdThreadStop)
#define __CNTROUT__ sLL (  m_writtenToOutput, sWrittenToOutput ); \
                    m_data = sWrittenToOutput; *m_echoStreamerShow = m_data
#define __INCR_CNTROUT__ m_writtenToOutput = m_writtenToOutput + 1LL; \
                         sLL (  m_writtenToOutput, sWrittenToOutput ); \
                         m_data = sWrittenToOutput; *m_echoStreamerShow = m_data


    m_stateComm = STSM_STATE_INIT;
    m_stateFifoOverFlow = STSM_FIFO_OFW_NOT_BLOCKING;

    wxIPV4address  *address = NULL;
    wxFile         *file    = NULL;
    bool            fileOp  = true;
    wxString        sWrittenToOutput = wxEmptyString;
    size_t          wFdOut  = 0;
    wxThreadEvent event( wxEVT_THREAD, myID_THREAD_IFLXAPI );

    if ( m_targetAsFilePath.IsEmpty() ) { 
        // wxSocketBase::Initialize();  // note: for eventual unit test, not for production
        m_socket.SetTimeout( m_connectionRetry );
        m_socket.SetFlags( wxSOCKET_BLOCK );
        address = new wxIPV4address();
        wxUniChar separator = 0x3a;
        address->Hostname(m_target.BeforeFirst(separator));
        address->Service(m_target.AfterFirst(separator));
        if ( m_verbosity > 1) {
            m_threadMsg = wxString::Format(
                "dashboard_tactics_pi: DB Streamer : STSM_STATE_INIT : (%s:%s)",
                m_target.BeforeFirst(separator), m_target.AfterFirst(separator));
            wxQueueEvent( m_frame, event.Clone() );
        }
        fileOp = false;
    } // then HTTP API
    else {
        file = new wxFile( m_targetAsFilePath, wxFile::write );
        if ( !file->IsOpened() ) {
            if ( m_verbosity > 0) {
                m_threadMsg = wxString::Format(
                    "dashboard_tactics_pi: ERROR : DB Streamer : opening file for writing : %s",
                    m_targetAsFilePath);
                wxQueueEvent( m_frame, event.Clone() );
            } // then failed to open the file for writing
            wxMilliSleep( 1000 );
            return (wxThread::ExitCode)0;
        } // then the file opening did not quite succeeded
    } // else file based Line Protocol 

    wxString sCnxPrg[3];
    sCnxPrg[0] = L"\u2192 \u2013 \u2013";
    sCnxPrg[1] = L"\u2013 \u2192 \u2013";
    sCnxPrg[2] = L"\u2013 \u2013 \u2192";
    int iCnxPrg = 2;

    wxString header = wxEmptyString;
    header += "POST ";
    header += "/api/";
    header += m_api;
    header += "/write?org=";
    header += m_org;
    header += "&bucket=";
    header += m_bucket;
    header += "&precision=";
    header += m_precision;
    header += " HTTP/1.1\r\n";

    header += "Host: ";
    header += m_target;
    header += "\r\n";	

    header += "User-Agent: OpenCPN/5.0\r\n";
    header += "Accept: */*\r\n";

    header += "Authorization: Token ";
    header += m_token;
    header += "\r\n";
    header += "Content-Type: application/x-www-form-urlencoded";
    header += "\r\n";
    header += "Connection: keep-alive";
    header += "\r\n";
    header += "Content-Length: "; // from this starts the dynamic part

    std::unique_lock<std::mutex> mtxQLine( m_mtxQLine, std::defer_lock );
    m_writtenToOutput = 0LL;

    if ( fileOp )
        m_stateComm = STSM_STATE_READY;
    else
        m_stateComm = STSM_STATE_CONNECTING;
    int prevStateComm = m_stateComm;

    while (__NOT_STOP_THREAD__) {

        if ( (m_stateComm == STSM_STATE_CONNECTING) || (m_stateComm == STSM_STATE_ERROR) ) {
            wxString connectionErr = wxEmptyString;
            giveUpConnectionRetry100ms(5);
            if ( __NOT_STOP_THREAD__ ) {
                ( (iCnxPrg >= 2) ? iCnxPrg = 0 : iCnxPrg++ );
                m_data = sCnxPrg[iCnxPrg];
                *m_echoStreamerShow = m_data;
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
                        m_stateComm = STSM_STATE_READY;
                        if ( prevStateComm != m_stateComm ) {
                            if ( m_verbosity > 1) {
                                m_threadMsg = _T("dashboard_tactics_pi: DB Streamer : STSM_STATE_READY");
                                wxQueueEvent( m_frame, event.Clone() );
                            }
                        }
                        prevStateComm = m_stateComm;
                    }
                    else {
                        m_stateComm = STSM_STATE_ERROR;
                        if ( prevStateComm != m_stateComm ) {
                            if ( m_verbosity > 1) {
                                m_threadMsg = _T("dashboard_tactics_pi: DB Streamer : STSM_STATE_ERROR");
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
        
        if ( m_stateComm == STSM_STATE_READY ) {
            
            wxString sData = wxEmptyString;
            int linesPrepared = 0;        
            __CNTROUT__;

            while ( (linesPrepared < m_linesPerWrite) && __NOT_STOP_THREAD__ ) {
                
                mtxQLine.lock();
                if (qLine.empty()) {
                    mtxQLine.unlock();
                    wxMilliSleep( 100 );
                    if (__STOP_THREAD__) {
                            break;
                    }
                } // no data to be sent
                else {
                    lineProtocol lineOut = qLine.front();
                    qLine.pop();
                    m_poppedFromFifo = m_poppedFromFifo + 1LL;
                    mtxQLine.unlock();

                    if ( linesPrepared > 0 ) {
                        if ( !fileOp )
                        sData += "\n";
                    }
                    sData += lineOut.measurement;
                    if ( !lineOut.tag_key1.IsEmpty() ) {
                        sData += ",";
                        sData += lineOut.tag_key1;
                        sData += "=";
                        sData += lineOut.tag_value1;
                        if ( !lineOut.tag_key2.IsEmpty() ) {
                            sData += ",";
                            sData += lineOut.tag_key2;
                            sData += "=";
                            sData += lineOut.tag_value2;
                            if ( !lineOut.tag_key3.IsEmpty() ) {
                                sData += ",";
                                sData += lineOut.tag_key3;
                                sData += "=";
                                sData += lineOut.tag_value3;
                            }
                        }
                    }
                    sData += " ";
                    sData += lineOut.field_key1;
                    sData += "=";
                    sData += lineOut.field_value1; // all data is expected to be float
                    if ( !lineOut.field_key2.IsEmpty() ) {
                        sData += ",";
                        sData += lineOut.field_key2;
                        sData += "=";             // if later on we save units as strings : += "\"";
                        sData += lineOut.field_value2;
                        // if later on we save units as string:      sData += "\"";
                        if ( !lineOut.field_key3.IsEmpty() ) {
                            sData += ",";
                            sData += lineOut.field_key3;
                            sData += "=";         // see above the comment about strings
                            sData += lineOut.field_value3;
                            // see above the comment about stings:   sData += "\"";
                        }
                    }
                    if ( !lineOut.timestamp.IsEmpty() ) {
                        sData += " ";
                        sData += lineOut.timestamp;
                    }
                    if ( fileOp )
                        sData += "\n";
                    linesPrepared++;
                    
                    if (__STOP_THREAD__)
                        break;
                } // else there is valid data in the queue
            } // while number of lines to prepare

            if (__STOP_THREAD__)
                        break;
                
            if ( fileOp ) {
                
                file->Write( sData );
                wFdOut += sData.Len();
                if ( wFdOut >= 4096 ) {
                    file->Flush();
                    wFdOut = 0;
                } // then let's flush the buffer every 2kB (if the fsync() is available in the OS)
                
                __INCR_CNTROUT__;
                
                wxMilliSleep(100);
                    
            } // then write to file
            else {
                    
                wxString sHdrOut = header;
                sHdrOut += wxString::Format("%d", sData.Len() );
                sHdrOut += "\r\n";
                sHdrOut += "\r\n";
                sHdrOut += sData;
                wxScopedCharBuffer scb = sHdrOut.mb_str();
                size_t len = scb.length();

                m_socket.Write( scb.data(), len );

                if ( m_verbosity > 4) {
                    m_threadMsg = wxString::Format("dashboard_tactics_pi: streamout: written to socket:\n%s", sHdrOut);
                    wxQueueEvent( m_frame, event.Clone() );
                } // for big time debugging only, use tail -f opencpn.log | grep dashboard_tactics_pi

                if ( !m_socket.Error() ) {

                    __INCR_CNTROUT__;

                    bool readAvailable = false;
                    bool timeOut = false;
                    while ( __NOT_STOP_THREAD__ && !readAvailable ) {
                        char c;
                        ( m_socket.Peek(&c,1).LastCount()==0 ? readAvailable = false : readAvailable = true );
                        if ( !readAvailable) {
                            wxLongLong startWait = wxGetUTCTimeMillis();
                            readAvailable = m_socket.WaitForRead( );
                            if ( !readAvailable) {
                                wxLongLong endWait = wxGetUTCTimeMillis();
                                if ( (endWait.GetValue() - startWait.GetValue()) >= ( m_connectionRetry * 1000 ) ) {
                                    m_socket.Close();
                                    m_stateComm = STSM_STATE_ERROR;
                                    timeOut = true;
                                }
                            }
                       }
                        if ( (__STOP_THREAD__) || timeOut )
                            break;
                    }
                    if ( (__STOP_THREAD__) || timeOut )
                        break;
                    if ( readAvailable ) {
                        wxCharBuffer buf(100);
                        m_socket.Read( buf.data(), 100 );
                        wxString sBuf = buf;
                        bool sBufError = true;
                        if ( sBuf.Contains("HTTP/1.1 204") )
                            sBufError = false;
                        unsigned int lostReadCount = std::numeric_limits<unsigned int>::max();
                        int lostLoop100s = 0;
                        while ( lostReadCount > 0 ) {
                            wxCharBuffer lostbuf(100);
                            m_socket.Read( lostbuf.data(), 100 );
                            lostReadCount = m_socket.LastReadCount();
                            lostLoop100s += lostReadCount;
                        }
                        if ( sBufError ) {
                            if ( m_verbosity > 2) {
                                m_threadMsg = wxString::Format(
                                    "dashboard_tactics_pi: DB Streamer : Data Rejected (%s) : %s",
                                    sBuf, sData);
                                wxQueueEvent( m_frame, event.Clone() );
                            }
                        } // then error from the DB write API
                    } // then read buffer of the socket contains some data
                    else {
                        m_stateComm = STSM_STATE_ERROR;
                        m_socket.Close();
                        if ( m_verbosity > 1) {
                            m_threadMsg = _T("dashboard_tactics_pi: DB Streamer : Error - no data received from server.");
                            wxQueueEvent( m_frame, event.Clone() );
                        }
                        giveUpConnectionRetry100ms(5);
                    } // else no data in the socket buffer
                } // then no error in the socket write
                else {
                    m_stateComm = STSM_STATE_ERROR;
                    m_socket.Close();
                    if ( m_verbosity > 1) {
                        m_threadMsg = _T("dashboard_tactics_pi: DB Streamer : socket Write() error.");
                        wxQueueEvent( m_frame, event.Clone() );
                    }
                    giveUpConnectionRetry100ms(5);
                } // else error in the socket Write()
            } // else network (socket) operation
        } // else data available to be sent
    } // while not to be stopped / destroyed
        
    if ( fileOp ) {
        if ( file->IsOpened() ) {
            (void) file->Flush();
            wxMilliSleep( 20 );
            (void) file->Close();
        }
        delete file;
    } // then close file operations
    else {
        m_socket.Close();
        // wxSocketBase::Shutdown();  // note: for eventual unit test, not for production
        delete address;
    } // else socket operation termination
    
    return (wxThread::ExitCode)0;
    
}
/***********************************************************************************

************************************************************************************/
void TacticsInstrument_StreamoutSingle::OnThreadUpdate( wxThreadEvent &evt )
{
    wxLogMessage ("%s", m_threadMsg);
    m_threadMsg = wxEmptyString;
}
/***********************************************************************************

************************************************************************************/
void TacticsInstrument_StreamoutSingle::OnStreamOutUpdTimer( wxTimerEvent &evt )
{
    if ( m_thread->TestDestroy() || m_cmdThreadStop )
        return;
    if ( m_thread->IsAlive() ) {
        if ( m_thread->IsPaused() ) {
            if ( m_verbosity > 3) {
                wxLogMessage("dashboard_tactics_pi: DEBUG : OnStreamOutUpdTime : m_thread IsPaused()");
                } // for big time debugging only, use tail -f opencpn.log | grep dashboard_tactics_pi
            m_thread->Resume();
        }
    }
    if ( m_verbosity > 3 ) {
        std::unique_lock<std::mutex> lckmQline( m_mtxQLine );
        long long pushDelta = m_pushedInFifo - m_poppedFromFifo;
        wxString sPushedInFifo; sLL( m_pushedInFifo, sPushedInFifo );
        wxString sPoppedFromFifo; sLL( m_poppedFromFifo, sPoppedFromFifo );
        lckmQline.unlock();
        wxString sBlockingLimit; sLL( STSM_MAX_UNWRITTEN_FIFO_ELEMENTS_BLOCKING, sBlockingLimit );
        wxString sUnblockingLimit; sLL( STSM_MAX_UNWRITTEN_FIFO_ELEMENTS_UNBLOCKING, sUnblockingLimit );
        wxString sPushDelta; sLL( pushDelta, sPushDelta );
        wxLogMessage(
            "dashboard_tactics_pi: DEBUG : OnStreamOutUpdTime : FIFO : (In %s Out %s Delta %s Block %s Unblock %s)",
            sPushedInFifo, sPoppedFromFifo, sPushDelta, sBlockingLimit, sUnblockingLimit );
    } // for BIG time debugging only, use tail -f opencpn.log | grep dashboard_tactics_pi
}
/***********************************************************************************

************************************************************************************/
bool TacticsInstrument_StreamoutSingle::LoadConfig()
{
    if ( *m_nofStreamOut > 1 )
        return true;
    
    wxFileConfig *pConf = m_pconfig;
    
    if (!pConf)
        return false;
    pConf->SetPath(_T("/PlugIns/Dashboard/Tactics/Streamout/"));
    pConf->Read(_T("ConfigFile"), &m_configFileName, "streamout.json");
    wxString s = wxFileName::GetPathSeparator();
    wxString confPath = m_confdir + m_configFileName;
    if ( !wxFileExists( confPath ) ) {
        wxString tmplPath  = *GetpSharedDataLocation();
        tmplPath += _T("plugins") + s + _T("dashboard_tactics_pi") + s + _T("data") + s + _T("streamout_template.json");
        if ( !wxFileExists( tmplPath ) ) {
            wxLogMessage ("dashboard_tactics_pi: ERROR - missing template %s", tmplPath);
            m_data = L"\u2013 No template. \u2013";
            *m_echoStreamerShow = m_data;
            return false;
        }
        bool ret = wxCopyFile ( tmplPath, confPath ); 
        if ( !ret ) {
            wxLogMessage ("dashboard_tactics_pi: ERROR - cannot copy template %s to %s", tmplPath, confPath);
            m_data = L"\u2013 ConfigFile? \u2013";
            *m_echoStreamerShow = m_data;
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
            wxMessageBox(_("InfluxDB Steamer configuration file parsing error, see log file."));
            for (int i = 0; ( ((size_t) i < errors.GetCount()) && ( i < 10 ) ); i++) {
                wxLogMessage ("dashboard_tactics_pi: ERROR - parsing errors in the configuration file: %s", errors.Item(i) );
            }
            return false;
        }

        if ( !root.HasMember("influxdb") ) throw 100;
        if ( !root["influxdb"].HasMember("target") ) throw 101;
        m_target += root["influxdb"]["target"].AsString();
        wxUniChar separator = 0x3a;
        if ( m_target.Find( separator ) == wxNOT_FOUND ) {
            m_targetAsFilePath = m_confdir + m_target;
            if ( wxFileExists( m_targetAsFilePath ) ) {
                wxDateTime bupTime = wxDateTime::Now();
                wxString sBupTime = bupTime.Format("_bup_%F_%H-%M-%S");
                wxString sTargetBackupName = m_target + sBupTime;
                wxString sTargetBackupPath = m_confdir + sTargetBackupName;
                bool ret = wxRenameFile ( m_targetAsFilePath, sTargetBackupPath );
                if ( !ret ) {
                    wxLogMessage ("dashboard_tactics_pi: ERROR - cannot rename data %s to %s",
                                  m_targetAsFilePath, sTargetBackupPath);
                    m_data = L"\u2013 ERR:DataFileBackup \u2013";
                    *m_echoStreamerShow = m_data;
                    return false;
                } // then could not rename the original datafile as a backup file
            } // else the target data file exists already, we do not want to append, rename
        } // else this is not HTTP API but a file name

        if ( !root["influxdb"].HasMember("api") ) throw 102;
        m_api += root["influxdb"]["api"].AsString();
        if ( !root["influxdb"].HasMember("org") ) throw 103;
        m_org += root["influxdb"]["org"].AsString();
        if ( !root["influxdb"].HasMember("bucket") ) throw 104;
        m_bucket += root["influxdb"]["bucket"].AsString();
        if ( !root["influxdb"].HasMember("precision") ) throw 105;
        m_precision += root["influxdb"]["precision"].AsString();
        if ( !root["influxdb"].HasMember("token") ) throw 106;
        m_token += root["influxdb"]["token"].AsString();

        if ( !root.HasMember("streamer") ) throw 200;
        if ( !root["streamer"].HasMember("connectionretry") ) throw 201;
        m_connectionRetry = root["streamer"]["connectionretry"].AsInt();
        if ( m_connectionRetry <=0)
            m_connectionRetry = 1; // cannot be <=0 ; used to throttle the thread
        if ( !root["streamer"].HasMember("linesperwrite") ) throw 202;
        m_linesPerWrite = root["streamer"]["linesperwrite"].AsInt();
        if ( m_linesPerWrite <=0)
            m_linesPerWrite = 1;
        if ( !root["streamer"].HasMember("timestamps") ) throw 203;
        m_timestamps += root["streamer"]["timestamps"].AsString();
        if ( m_timestamps.IsSameAs( _T("db"), false ) )
            m_stamp = false;
        if ( !root["streamer"].HasMember("verbosity") ) throw 204;
        m_verbosity = root["streamer"]["verbosity"].AsInt();

        if ( m_verbosity > 1 ) {
            wxLogMessage( "dashboard_tactics_pi: InfluxDB API server   = \"%s\"",  m_target );
            wxLogMessage( "dashboard_tactics_pi: InfluxDB API version  = \"%s\"",  m_api );
            wxLogMessage( "dashboard_tactics_pi: InfluxDB organization = \"%s\"",  m_org );
            wxLogMessage( "dashboard_tactics_pi: InfluxDB bucket       = \"%s\"",  m_bucket );
            wxLogMessage( "dashboard_tactics_pi: InfluxDB precision    = \"%s\"",  m_precision );
            wxLogMessage( "dashboard_tactics_pi: InfluxDB token        =\n\"%s\"", m_token );
            wxLogMessage( "dashboard_tactics_pi: InfluxDB conn.retry   = %d s.",   m_connectionRetry );
            wxLogMessage( "dashboard_tactics_pi: InfluxDB lines/write  = %d",      m_linesPerWrite );
            wxLogMessage( "dashboard_tactics_pi: InfluxDB timestamps   = \"%s\"",  m_timestamps );
            wxLogMessage( "dashboard_tactics_pi: Streamer verbosity    = %d",      m_verbosity );
        }

        if ( !root.HasMember("dbschema") ) throw 300;
        wxJSONValue dbSchemas = root["dbschema"];
        if ( !dbSchemas.IsArray() ) {
            wxLogMessage ("dashboard_tactics_pi: JSON file %s parsing exception: 'dbschema' is not an array.");
            throw 300;
        }
        int asize = dbSchemas.Size();
        if ( asize <= 0 ) {
            wxLogMessage ("dashboard_tactics_pi: JSON file %s parsing exception: 'dbschema' is an array but it is empty.");
            throw 300;
        }
        for ( int i = 0; i < asize; i++ ) {

            sentenceSchema schema;

            if ( !dbSchemas[i].HasMember("sentence") ) throw ( 10000 + (i * 100) + 1 );
            schema.stc = dbSchemas[i]["sentence"].AsString();

            if ( !dbSchemas[i].HasMember("mask") ) throw ( 10000 + (i * 100) + 2 );
            int mask = dbSchemas[i]["mask"].AsInt();
            schema.st = 1ULL << mask;
            
            if ( !dbSchemas[i].HasMember("store") ) throw ( 10000 + (i * 100) + 3 );
            schema.bStore = dbSchemas[i]["store"].AsBool();

            schema.lastTimeStamp = 0LL;

            if ( !dbSchemas[i].HasMember("interval") ) throw ( 10000 + (i * 100) + 4 );
            int iInterval = dbSchemas[i]["interval"].AsInt();
            if ( iInterval < 0 )
                iInterval = 0;
            schema.iInterval = iInterval;

            if ( !dbSchemas[i].HasMember("measurement") ) throw ( 10000 + (i * 100) + 5 );
            schema.sMeasurement = dbSchemas[i]["measurement"].AsString();

            if ( !dbSchemas[i].HasMember("prop1") ) throw ( 10000 + (i * 100) + 6 );
            schema.sProp1 = dbSchemas[i]["prop1"].AsString();

            if ( !dbSchemas[i].HasMember("prop2") ) throw ( 10000 + (i * 100) + 7 );
            schema.sProp2 = dbSchemas[i]["prop2"].AsString();

            if ( !dbSchemas[i].HasMember("prop3") ) throw ( 10000 + (i * 100) + 8 );
            schema.sProp3 = dbSchemas[i]["prop3"].AsString();

            if ( !dbSchemas[i].HasMember("field1") ) throw ( 10000 + (i * 100) + 9 );
            schema.sField1 = dbSchemas[i]["field1"].AsString();

            if ( !dbSchemas[i].HasMember("field2") ) throw ( 10000 + (i * 100) + 10 );
            schema.sField2 = dbSchemas[i]["field2"].AsString();

            if ( !dbSchemas[i].HasMember("field3") ) throw ( 10000 + (i * 100) + 11 );
            schema.sField3 = dbSchemas[i]["field3"].AsString();

            vSchema.push_back ( schema );
            
        } // while array has sentence schemas defined
    }
    catch (int x) {
        if ( (x >= 100) && (x < 200) ) {
            wxLogMessage ("dashboard_tactics_pi: JSON file %s parsing exception: missing expected item %d in 'influxdb'",
                          confPath, (x - 100) );
        }
        else if ( (x >= 200) && (x < 300) ) {
            wxLogMessage ("dashboard_tactics_pi: JSON file %s parsing exception: missing expected item %d in 'streamer'",
                          confPath, (x - 200) );
        }
        else if ( (x >= 300) && (x < 400) ) {
            wxLogMessage ("dashboard_tactics_pi: JSON file %s parsing exception: missing expected item %d in 'dbschema'",
                          confPath, (x - 300) );
        }
        else {
            wxLogMessage ("dashboard_tactics_pi: JSON file %s parsing exception: missing item in 'dbschema' array %d (10000=ignore, 100's=sentence number, 1's=index in sentence)",
                          confPath, x );
        }
        wxMessageBox(_("InfluxDB Steamer configuration file parsing error, see log file."));

        return false;
        
    } // A JSON file can have errors which make this old JSON code to break

    return true;
}
/***********************************************************************************

************************************************************************************/
void TacticsInstrument_StreamoutSingle::SaveConfig()
{
    if ( *m_nofStreamOut > 1 )
        return;
    
    wxFileConfig *pConf = m_pconfig;
    
    if (!pConf)
        return;

    pConf->SetPath(_T("/PlugIns/Dashboard/Tactics/Streamout/"));
    pConf->Write(_T("ConfigFile"),m_configFileName);

    return;
}
