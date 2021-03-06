//
//  CRTConnTcp.cpp
//  dyncRTConnector
//
//  Created by hp on 12/11/15.
//  Copyright (c) 2015 hp. All rights reserved.
//

#include <string.h>
#include "CRTConnTcp.h"
#include "rtklog.h"
#include "RTMsgCommon.h"

CRTConnTcp::CRTConnTcp()
{

}

CRTConnTcp::~CRTConnTcp()
{

}

int CRTConnTcp::DoProcessData(const char* pData, int nLen)
{
    if (nLen==0) {
        return 0;
    }
    SIGNALMSG m_smsg;
    MEETMSG m_mmsg;
    std::string sstr(pData, nLen), err("");
    m_smsg.GetMsg(sstr, err);
    if (err.length() > 0) {
        LE("%s err:%s\n", __FUNCTION__, err.c_str());
        if (err.compare(INVALID_JSON_PARAMS)==0) {
            // Here means the msg not received totally
            return -1;
        } else {
            // Here means the msg error
            return nLen;
        }
    }
    if (m_smsg._scont.length()>0) {
        m_mmsg.GetMsg(m_smsg._scont, err);
        if (err.length() > 0) {
            LE("%s err:%s\n", __FUNCTION__, err.c_str());
            if (err.compare(INVALID_JSON_PARAMS)==0) {
                // Here means the msg not received totally
                return -1;
            } else {
                // Here means the msg error
                return nLen;
            }
        }
    }
    if (m_smsg._stype == SIGNALTYPE::keepalive) {
        if (m_mmsg._from.length()>0) {
            OnKeepAlive(m_mmsg._from.c_str());
        } else {
            LE("keepalive params errors\n");
        }
    } else if (m_smsg._stype == SIGNALTYPE::login) {
        if (m_mmsg._from.length()>0 && m_mmsg._pass.length()>0 && m_mmsg._nname.length()>0) {
            OnLogin(m_mmsg._from.c_str(), m_mmsg._pass.c_str(), m_mmsg._nname.c_str());
        } else {
            LE("login params errors\n");
        }
    } else if (m_smsg._stype == SIGNALTYPE::sndmsg) {
        if (m_mmsg._from.length()>0) {
            const char* pContentDump = strdup(m_mmsg.ToJson().c_str());
            OnSndMsg(m_mmsg._mtype, m_mmsg._mseq, m_mmsg._from.c_str(), pContentDump, (int)strlen(pContentDump));
            free((void*)pContentDump);
            pContentDump = NULL;
        } else {
            LE("sndmsg params errors\n");
        }
    } else if (m_smsg._stype == SIGNALTYPE::getmsg) {
        if (m_mmsg._from.length()>0) {
            OnGetMsg(m_mmsg._mtype, m_mmsg._mseq, m_mmsg._from.c_str());
        } else {
            LE("getmsg params errors\n");
        }
    } else if (m_smsg._stype == SIGNALTYPE::logout) {
        if (m_mmsg._from.length()>0) {
            OnLogout(m_mmsg._from.c_str());
        } else {
            LE("logout params errors\n");
        }
    } else {
        LE("parse signal msg params error\n");
    }
    return nLen;
}


char* CRTConnTcp::GenerateResponse(int code, const std::string&query, const char*pData, int nLen, int&outLen)
{
    return (char*)"";
}


void CRTConnTcp::SendResponse(int code, const std::string&query, const char*pData, int nLen)
{

}

void CRTConnTcp::SendResponse(int code, const std::string&strContent)
{
    OnResponse(strContent.c_str(), (int)strContent.length());
}
