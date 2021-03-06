//
//  MRTRoomManager.cpp
//  dyncRTMeeting
//
//  Created by hp on 12/9/15.
//  Copyright (c) 2015 hp. All rights reserved.
//

#include "MRTRoomManager.h"
#include <assert.h>
#include <algorithm>
#include <json/json.h>
#include "md5.h"
#include "md5digest.h"
#include "OS.h"
#include "rtklog.h"
#include "MRTMeetingRoom.h"
#include "StatusCode.h"
#include "RTHttpMsg.h"
#include "RTUtils.hpp"
#include "RTZKClient.hpp"


void MRTRoomManager::HandleOptRoom(TRANSMSG& tmsg, MEETMSG& mmsg)
{
    switch (mmsg._cmd) {
        case MEETCMD::enter:
        {
            EnterRoom(tmsg, mmsg);
        }
            break;
        case MEETCMD::leave:
        {
            LeaveRoom(tmsg, mmsg);
        }
            break;
        default:
            LE("HandleOptRoom cmd [%d] not handle now\n", mmsg._cmd);
            break;
    }
}


void MRTRoomManager::HandleOptRoomWithData(int cmd, TRANSMSG& tmsg, MEETMSG& mmsg, std::string& data)
{
    switch (cmd) {
        case M_HTTP_CMD_GET_MEETING_INFO:
        {

        }
            break;
        case M_HTTP_CMD_GET_MEMBER_LIST:
        {
            OnGetMemberList(tmsg, mmsg, data);
        }
            break;
        default:
            LE("HandleOptRoomWithData cmd [%d] not handle now\n", mmsg._cmd);
            break;
    }
}


void MRTRoomManager::HandleDcommRoom(TRANSMSG& tmsg, MEETMSG& mmsg)
{
    if (mmsg._room.length()==0) {
        LE("invalid params error\n");
        //
        return;
    }
    std::string users(""), resp("");
    mmsg._ntime = OS::Milliseconds();
    MeetingRoomMapIt it = m_meetingRoomMap.find(mmsg._room);
    if (it != m_meetingRoomMap.end()) {
        switch (mmsg._action) {
            case DCOMMACTION::msend:
            {
                switch (mmsg._tags) {
                    case SENDTAGS::sendtags_talk:
                    {
                        if (mmsg._to.at(0)=='a') {
                            //to all
                            //@Eric
                            //* 1, Check GetMembersStatus is Waitting
                            // Need to save msg to msgList for other member(not in room) when Got members form http.
                            rtc::scoped_refptr<MRTMeetingRoom> meetingRoom = it->second;
                            if (meetingRoom->GetGetMembersStatus()!=MRTMeetingRoom::GetMembersStatus::GMS_DONE) {
                                //store message
                                meetingRoom->AddWaitingMsgToList(1, 1, tmsg, mmsg);
                                return;
                            }
                            if (!meetingRoom->GetRoomMemberJson(mmsg._from, users)) {
                                //LI("==>HandleDcommRoom from:%s, to users:%s\n", mmsg._from.c_str(), users.c_str());
                                GenericResponse(tmsg, mmsg, MESSAGETYPE::request, SIGNALTYPE::sndmsg, RTCommCode::_ok, users, resp);
                                SendTransferData(resp, (int)resp.length());
                                LI("#user:%s, #msg:%s\n", mmsg._from.c_str(), resp.c_str());

                                if (m_pHttpSvrConn) {
                                    m_pHttpSvrConn->HttpInsertMeetingMsg(mmsg._pass.c_str(), mmsg._room.c_str(), "0", it->second->GetSessionId().c_str(), mmsg._cont.c_str(), mmsg._from.c_str());
                                }
                                return;
                            } else {
                                LE("==>HandleDcommRoom no member in room, users:%s\n", users.c_str());
                                // no member in room
                            }
                        } else if (mmsg._to.at(0)=='u') {
                            //to userlist
                        }
                    }
                        break;
                    case SENDTAGS::sendtags_call:
                    {
                        if (mmsg._to.at(0)=='a') {
                            //to all
                            //@Eric
                            //* 1, Check GetMembersStatus is Waitting
                            // Need to save msg to msgList for other member(not in room) when Got members form http.
                            rtc::scoped_refptr<MRTMeetingRoom> meetingRoom = it->second;
                            if (meetingRoom->GetGetMembersStatus()!=MRTMeetingRoom::GetMembersStatus::GMS_DONE) {
                                //store message
                                meetingRoom->AddWaitingMsgToList(1, 1, tmsg, mmsg);
                                return;
                            }
                            if (!meetingRoom->GetRoomMemberJson(mmsg._from, users)) {
                                //LI("==>HandleDcommRoom from:%s, to users:%s\n", mmsg._from.c_str(), users.c_str());
                                GenericResponse(tmsg, mmsg, MESSAGETYPE::request, SIGNALTYPE::sndmsg, RTCommCode::_ok, users, resp);
                                SendTransferData(resp, (int)resp.length());
                                LI("#user:%s, #msg:%s\n", mmsg._from.c_str(), resp.c_str());
                                return;
                            } else {
                                LE("==>HandleDcommRoom no member in room, users:%s\n", users.c_str());
                                // no member in room
                            }
                        } else if (mmsg._to.at(0)=='u') {
                            //to userlist
                        }
                    }
                        break;

                    case SENDTAGS::sendtags_enter:
                    {

                    }
                        break;
                    case SENDTAGS::sendtags_leave:
                    {

                    }
                        break;
                    default:
                    {

                    }
                        break;
                }
            }
                break;
            case DCOMMACTION::dsett:
            {

            }
                break;
            case DCOMMACTION::share:
            {

            }
                break;

            default:
            {

            }
                break;
        }
    } else {

    }
}

void MRTRoomManager::EnterRoom(TRANSMSG& tmsg, MEETMSG& mmsg)
{
    std::string users(""), resp("");
    mmsg._ntime = OS::Milliseconds();
    //@Eric
    //* 1, Find Room
    MeetingRoomMapIt it = m_meetingRoomMap.find(mmsg._room);
    if (it == m_meetingRoomMap.end()) { // meeting not exists
        LE("==>EnterRoom % Room %s not exists, new it\n", mmsg._room.c_str(), __FUNCTION__);
        m_meetingRoomMap.insert(make_pair(mmsg._room, new rtc::RefCountedObject<MRTMeetingRoom>(mmsg._room, "")));
        it = m_meetingRoomMap.find(mmsg._room);

        //@Eric
        //* Get room member list from httpSvr.
        //* Set GetMembersStatus to Waitting.
        if (m_pHttpSvrConn) {
            m_pHttpSvrConn->HttpGetMeetingMemberList(tmsg, mmsg);
            it->second->SetGetMembersStatus(MRTMeetingRoom::GMS_WAITTING);
        } else {
            LE("==>EnterRoom m_pHttpSvrConn is NULL\n");
            assert(false);
        }
    }

    //@Eric
    //* 2, Add member to RoomList.
    LI("==>EnterRoom User %s to Room %s\n", mmsg._from.c_str(), mmsg._room.c_str());
    it->second->AddMemberToRoom(mmsg._from, MRTMeetingRoom::MemberStatus::MS_INMEETING);
    AddUserMeetingRoomId(mmsg._from, mmsg._room);
    int online = it->second->GetMeetingMemberNumber();
    if (online==1) {
        if (m_pHttpSvrConn) {
            m_pHttpSvrConn->HttpInsertSessionMeetingInfo(mmsg._pass.c_str(), mmsg._room.c_str(), it->second->GetSessionId().c_str(), "0", "0", "1");
            m_pHttpSvrConn->HttpUpdateSessionMeetingStatus(mmsg._pass.c_str(), it->second->GetSessionId().c_str(), "1");
        }
    }
    if (m_pHttpSvrConn) {
        char mem[4] = {0};
        sprintf(mem, "%d", online);
        m_pHttpSvrConn->HttpUpdateSessionMeetingNumber(mmsg._pass.c_str(), it->second->GetSessionId().c_str(), mem, mmsg._room.c_str());
    }

    //@Eric
    //* 3, Notify all members, i'm online.
    if (!it->second->GetAllRoomMemberJson(users)) {
        if (users.length()>0) {
            LI("==>EnterRoom notify users :%s i am in room, online mem:%d\n", users.c_str(), online);
            //if (it->second->GetGetMembersStatus()!=MRTMeetingRoom::GetMembersStatus::GMS_DONE) {
            //    //store message
            //    LI("==>HandleDcommRoom GetGetMembersStatus WAITING...\n");
            //    it->second->AddWaitingMsgToList(1, 1, tmsg, mmsg);
            //} else {
                mmsg._nmem = online;
                mmsg._cont = mmsg._from;
                mmsg._tags = SENDTAGS::sendtags_enter;
                GenericResponse(tmsg, mmsg, MESSAGETYPE::request, SIGNALTYPE::sndmsg, RTCommCode::_ok, users, resp);
                SendTransferData(resp, (int)resp.length());
            //}
        }
    }

    //@Eric
    //* 5, Enter room done!
    // Need send back event to client?
    //////////////////////////

}

void MRTRoomManager::LeaveRoom(TRANSMSG& tmsg, MEETMSG& mmsg)
{
    std::string users(""), resp("");
    mmsg._ntime = OS::Milliseconds();
    //@Eric
    //* 1, Remove myself in Room MemberList;
    //
    MeetingRoomMapIt it = m_meetingRoomMap.find(mmsg._room);
    if (it != m_meetingRoomMap.end()) {
        if (it->second) {
            it->second->DelMemberFmMeeting(mmsg._from);
            DelUserMeetingRoomId(mmsg._from);
        } else {
            return;
        }
    } else {
        // not find room
        return;
    }
    //@Eric
    //* 2, Notify Other members
    //
    int online = it->second->GetMeetingMemberNumber();

    if (!it->second->GetAllRoomMemberJson(users)) {
        if (users.length()>0) {
            //if (it->second->GetGetMembersStatus()!=MRTMeetingRoom::GetMembersStatus::GMS_DONE) {
                //store message
            //    LI("==>HandleDcommRoom GetGetMembersStatus WAITING...\n");
            //    it->second->AddWaitingMsgToList(1, 1, tmsg, mmsg);
            //} else {
                mmsg._cont = mmsg._from;
                mmsg._nmem = online;
                mmsg._tags = SENDTAGS::sendtags_leave;
                GenericResponse(tmsg, mmsg, MESSAGETYPE::request, SIGNALTYPE::sndmsg, RTCommCode::_ok, users, resp);
                SendTransferData(resp, (int)resp.length());
            //}
        }
    }
    LI("==>LeaveRoom notify users :%s i am out room, online mem:%d\n", users.c_str(), online);

    //@Eric
    //* 3, Check size of Room MemberList offline is 0?
    // If 0, maybe release this room?
    // If GetMembersStatus is Waitting, need waitting for http response!!!
    if (it->second->GetGetMembersStatus()==MRTMeetingRoom::GetMembersStatus::GMS_WAITTING) {
        //send wait event
    }
    if (m_pHttpSvrConn) {
        char mem[4] = {0};
        sprintf(mem, "%d", online);
        m_pHttpSvrConn->HttpUpdateSessionMeetingNumber(mmsg._pass.c_str(), it->second->GetSessionId().c_str(), mem, mmsg._room.c_str());
    }
    if (online==0) {
        if (m_pHttpSvrConn) {
            m_pHttpSvrConn->HttpUpdateSessionMeetingEndtime(mmsg._pass.c_str(), it->second->GetSessionId().c_str());
            m_pHttpSvrConn->HttpUpdateSessionMeetingStatus(mmsg._pass.c_str(), it->second->GetSessionId().c_str(), "2");
        }
    }
    it->second->ResetSessionId();

    //@Eric
    //* 4, Leave room done!
    // Need send back event to client?

}



/////////////////////////////////////////////////////////////

bool MRTRoomManager::Init(const std::string& msgQueueIp, unsigned short msgQueuePort, const std::string& httpIp, unsigned short httpPort, const std::string& httpHost)
{
    if (m_pRoomDispatcher==NULL)
        m_pRoomDispatcher = new MRTRoomDispatcher();
    return ConnectMsgQueue(msgQueueIp, msgQueuePort) && ConnectHttpSvrConn(httpIp, httpPort, httpHost);
}

bool MRTRoomManager::ConnectMsgQueue(const std::string& msgQueueIp, unsigned short msgQueuePort)
{
    m_pMsgQueueSession = new MRTTransferSession(TRANSFERMODULE::mmsgqueue);
    m_pMsgQueueSession->Init();
    // conn to msgqueue
    while(!m_pMsgQueueSession->Connect(msgQueueIp, msgQueuePort)) {
        LI("connecting to msgqueue server %s:%u waiting...\n", msgQueueIp.c_str(), msgQueuePort);
        usleep(100*1000);
    }
    LI("%s port:%u, socketFD:%d\n", __FUNCTION__, msgQueuePort,  m_pMsgQueueSession->GetSocket()->GetSocketFD());
    m_pMsgQueueSession->EstablishConnection();
    return true;
}

bool MRTRoomManager::TryConnectMsgQueue(const std::string& msgQueueIp, unsigned short msgQueuePort)
{
    Assert(m_pMsgQueueSession==nullptr);

    m_pMsgQueueSession = new MRTTransferSession(TRANSFERMODULE::mmsgqueue);
    m_pMsgQueueSession->Init();

    bool ok = false;
    int times;
    // conn to msgqueue
    do{
        ok = m_pMsgQueueSession->Connect(msgQueueIp, msgQueuePort);
        usleep(1000*1000);
        LI("try %d times to connect to msgqueue server %s:%u waiting...\n", times, msgQueueIp.c_str(), msgQueuePort);
    }while(!ok && times++ < 5);

    if (ok) {
        LI("%s port:%u, socketFD:%d\n", __FUNCTION__, msgQueuePort,  m_pMsgQueueSession->GetSocket()->GetSocketFD());
        m_pMsgQueueSession->EstablishConnection();
        return true;
    } else {
        if (m_pRoomDispatcher)
            m_pRoomDispatcher->Signal(Task::kIdleEvent);
        return false;
    }
}

bool MRTRoomManager::ConnectHttpSvrConn(const std::string& httpIp, unsigned short httpPort, const std::string& httpHost)
{
    if (!m_pHttpSvrConn) {
        m_pHttpSvrConn = new rtc::RefCountedObject<MRTHttpSvrConn>();
        m_pHttpSvrConn->SetHttpHost(httpIp, httpPort, httpHost);
    }
    return true;
}

void MRTRoomManager::SendTransferData(const std::string strData, int nLen)
{
    if (m_pMsgQueueSession) {
        m_pMsgQueueSession->SendTransferData(strData.c_str(), nLen);
    } else {
        LE("%s m_pMsgQueueSession %d NULL error\n", __FUNCTION__, __LINE__);
    }
}

void MRTRoomManager::CheckMembers()
{
#if 0
    MeetingRoomMapIt it = m_meetingRoomMap.begin();
    for (; it!=m_meetingRoomMap.end(); it++) {
        LI("meetingRoom roomMember:%d, online:%d\n", it->second->GetRoomMemberNumber(), it->second->GetMeetingMemberNumber());
    }
#endif
}

void MRTRoomManager::SyncHttpRequest()
{

}

void MRTRoomManager::ClearSessionLost(const std::string& uid, const std::string& token, const std::string& connector)
{
    if (m_meetingRoomMap.size()==0) {
        return;
    }
    const std::string roomid = GetUserMeetingRoomId(uid);
    if (roomid.length()==0) {
        return;
    }
    MeetingRoomMapIt it = m_meetingRoomMap.find(roomid);
    if (it==m_meetingRoomMap.end()) {
        return;
    }

    it->second->DelMemberFmMeeting(uid);
    DelUserMeetingRoomId(uid);
    int online = it->second->GetMeetingMemberNumber();
    char strOnline[4] = {0};
    std::string pubid(""), users(""), resp(""), cont("");
    sprintf(strOnline, "%d", online);

    // notify member having room uid leaving
    users = "";
    resp = "";
    it->second->GetRoomMemberJson(uid, users);
    cont = uid;
    GenericConnLostResponse(uid, token, roomid, connector, SENDTAGS::sendtags_leave, online, cont, users, resp);
    SendTransferData(resp, (int)resp.length());
    if (it->second->GetGetMembersStatus()!=MRTMeetingRoom::GetMembersStatus::GMS_DONE) {
        //store message
        LI("==>ClearSessionLost GetGetMembersStatus WAITING...\n");
        //it->second->AddWaitingMsgToList(1, 1, resp);
    } else {

    }
    //LI("ClearSessionLost msg resp:%s\n", resp.c_str());

    if (m_pHttpSvrConn) {
        m_pHttpSvrConn->HttpUpdateSessionMeetingNumber(token.c_str(), it->second->GetSessionId().c_str(), strOnline, it->second->GetRoomId().c_str());
    }
    if (online==0) {
        if (m_pHttpSvrConn) {
            m_pHttpSvrConn->HttpUpdateSessionMeetingEndtime(token.c_str(), it->second->GetSessionId().c_str());
            m_pHttpSvrConn->HttpUpdateSessionMeetingStatus(token.c_str(), it->second->GetSessionId().c_str(), "2");
        }
    }
    it->second->ResetSessionId();

}

void MRTRoomManager::ClearMsgQueueSession(const std::string& sid)
{
    // the object MsgQueue RTTransferSession will be delete in Task
    // here just make the pointer to NULL
    m_pMsgQueueSession = NULL;
    LI("RTRoomManager::ClearMsgQueueSession m_pMsgQueueSession=NULL sid:%s\n", sid.c_str());
}


bool MRTRoomManager::ClearAll()
{
    if (m_pRoomDispatcher)
        m_pRoomDispatcher->Signal(Task::kKillEvent);
    return true;
}

void MRTRoomManager::ProcessTickEvent(const char*pData, int nLen)
{
    Assert(m_pMsgQueueSession);
    if (m_pMsgQueueSession->GetConnectingStatus()==0) {
        bool ok = false;
        int times = 0;
        do{
            ok = m_pMsgQueueSession->Connect();
            usleep(2000*1000);
        }while(!ok && ++times < 5);
        if (m_pRoomDispatcher)
            m_pRoomDispatcher->Signal(Task::kIdleEvent);
    } else if (m_pMsgQueueSession->GetConnectingStatus() == 1) {
        m_pMsgQueueSession->EstablishConnection();
    }
}



////////////////////////////////////////////////////
/////////////////private////////////////////////////
////////////////////////////////////////////////////

int MRTRoomManager::ChangeToJson(const std::string from, std::string& users)
{
    TOJSONUSER touser;
    touser._us.push_front(from);
    users = touser.ToJson();
    return 0;
}

void MRTRoomManager::SendWaitingMsgs(MeetingRoomMapIt mit)
{
    //TODO:
    MRTMeetingRoom::WaitingMsgsList pWml = mit->second->GetWaitingMsgs();
    if (pWml.size()==0) {
        return;
    }
    //LI("===>>>MRTRoomManager::SendWaitingMsgs size:%d, roomid:%s\n", (int)pWml.size(), mit->second->GetRoomId().c_str());
    std::string roomUsers(""), resp("");
    mit->second->GetAllRoomMemberJson(roomUsers);
    MRTMeetingRoom::WaitingMsgsList::iterator wit = pWml.begin();
    for (; wit!=pWml.end(); wit++) {
        MRTMeetingRoom::WaitingMsg wm = *wit;
        GenericResponse(wm._wtmsg, wm._wmsg, MESSAGETYPE::request, SIGNALTYPE::sndmsg, RTCommCode::_ok, roomUsers, resp);
        SendTransferData(resp.c_str(), (int)resp.length());
    }
    pWml.clear();
}

void MRTRoomManager::OnGetMemberList(TRANSMSG& tmsg, MEETMSG& mmsg, std::string& data)
{
    //@Eric
    //* 1, Update room member list.
    MeetingRoomMapIt it = m_meetingRoomMap.find(mmsg._room);
    if (it != m_meetingRoomMap.end()) {
        MEETINGMEMBERLIST memList;
        std::string err;
        memList.GetMsg(data, err);
        if (err.length()==0) {
            it->second->UpdateMemberList(memList._uslist);
        } else {
            LE("OnGetMemberList error:%s, mmsg._roomid:%s\n", err.c_str(), mmsg._room.c_str());
            assert(false);
        }
    } else {
        // not find room
        return;
    }

    //@Eric
    //* 2, Set GetMembersStatus to Done, and notify other members(not in room) meeting is opened.
    it->second->SetGetMembersStatus(MRTMeetingRoom::GetMembersStatus::GMS_DONE);
    //LI("====>>>>OnGetMemberList SendWaitingMsgs\n");
    SendWaitingMsgs(it);

    //@Eric
    //* 3, Check msgs list, and send to notify other members(not in room)

}

void MRTRoomManager::GenericResponse(TRANSMSG tmsg, MEETMSG mmsg, MESSAGETYPE msgtype, SIGNALTYPE stype, int code, const std::string& tos, std::string& response)
{
    switch (mmsg._cmd) {
        case MEETCMD::enter:
        case MEETCMD::leave:
        case MEETCMD::dcomm:
            ResponseSndMsg(tmsg, mmsg, msgtype, stype, code, tos, response);
            break;
        default:
            LE("mmsg._cmd error:%d !!!\n", mmsg._cmd);
            break;
    }
}

void MRTRoomManager::GenericConnLostResponse(const std::string& uid, const std::string& token, const std::string& roomid, const std::string& connector, SENDTAGS tags, int nmem, const std::string& cont, const std::string& tos, std::string& response)
{
    //LI("GenericConnLostResponse cont:%s\n", cont.c_str());
    MEETMSG meetmsg;
    long long seq = GenericTransSeq();
    meetmsg._mtype = MSGTYPE::meeting;
    meetmsg._messagetype = MESSAGETYPE::request;
    meetmsg._signaltype = SIGNALTYPE::sndmsg;
    meetmsg._cmd = MEETCMD::leave;
    meetmsg._action = DCOMMACTION::msend;
    meetmsg._tags = tags;
    meetmsg._type = SENDTYPE::msg;
    meetmsg._mseq = 0;
    meetmsg._from = uid;
    meetmsg._room = roomid;
    meetmsg._to = "";
    meetmsg._cont = cont;
    meetmsg._pass = token;
    meetmsg._code = RTCommCode::_ok;
    meetmsg._nname = "";
    meetmsg._rname = "";
    meetmsg._nmem = nmem;
    meetmsg._ntime = OS::Milliseconds();

    QUEUEMSG qmsg;
    qmsg._flag = 0;
    qmsg._touser = tos;
    qmsg._connector = connector;//which connector come from
    qmsg._content = meetmsg.ToJson();

    TRANSFERMSG trmsg;
    trmsg._action = TRANSFERACTION::req;
    trmsg._fmodule = TRANSFERMODULE::mmeeting;
    trmsg._type = TRANSFERTYPE::queue;
    trmsg._trans_seq = seq;
    trmsg._trans_seq_ack = 0;
    trmsg._valid = 1;
    trmsg._content = qmsg.ToJson();
    response = trmsg.ToJson();
}

void MRTRoomManager::ResponseSndMsg(TRANSMSG tmsg, MEETMSG mmsg, MESSAGETYPE msgtype, SIGNALTYPE stype, int code, const std::string& tos,  std::string& response)
{
    MEETMSG meetmsg;
    long long seq = GenericTransSeq();
    meetmsg._mtype = mmsg._mtype;
    meetmsg._messagetype = msgtype;
    meetmsg._signaltype = stype;
    meetmsg._cmd = mmsg._cmd;
    meetmsg._action = mmsg._action;
    meetmsg._tags = mmsg._tags;
    meetmsg._type = mmsg._type;
    meetmsg._mseq = mmsg._mseq;
    meetmsg._from = mmsg._from;
    meetmsg._room = mmsg._room;
    meetmsg._to = "";
    meetmsg._cont = mmsg._cont;
    meetmsg._pass = mmsg._pass;
    meetmsg._code = code;
    meetmsg._nname = mmsg._nname;
    meetmsg._rname = mmsg._rname;
    meetmsg._nmem = mmsg._nmem;
    meetmsg._ntime = mmsg._ntime;

    QUEUEMSG qmsg;
    qmsg._flag = 0;
    qmsg._touser = tos;
    qmsg._connector = tmsg._connector;//which connector come from
    qmsg._content = meetmsg.ToJson();

    TRANSFERMSG trmsg;
    trmsg._action = TRANSFERACTION::req;
    trmsg._fmodule = TRANSFERMODULE::mmeeting;
    trmsg._type = TRANSFERTYPE::queue;
    trmsg._trans_seq = seq;
    trmsg._trans_seq_ack = 0;
    trmsg._valid = 1;
    trmsg._content = qmsg.ToJson();
    response = trmsg.ToJson();
}
