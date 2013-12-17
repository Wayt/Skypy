#include "Skypy.h"
#include <iostream>
#include "Session.h"

Skypy::Skypy() : _stopEvent(false), _sessionAddMutex(), _sessionAddList(),
    _sessionDelMutex(), _sessionDelList(), _sessionMap(),
    _networkMgr()
{

}

void Skypy::onStartup()
{
    std::cout << "*****************************************" << std::endl;
    std::cout << "*     _____ _                           *" << std::endl;
    std::cout << "*    /  ___| |                          *" << std::endl;
    std::cout << "*    \\ `--.| | ___   _ _ __  _   _      *" << std::endl;
    std::cout << "*     `--. \\ |/ / | | | '_ \\| | | |     *" << std::endl;
    std::cout << "*    /\\__/ /   <| |_| | |_) | |_| |     *" << std::endl;
    std::cout << "*    \\____/|_|\\_\\\\__, | .__/ \\__, |     *" << std::endl;
    std::cout << "*                 __/ | |     __/ |     *" << std::endl;
    std::cout << "*                |___/|_|    |___/      *" << std::endl;
    std::cout << "*                        Coupyright <3  *" << std::endl;
    std::cout << "*****************************************" << std::endl << std::endl;

    std::cout << ">> Starting network ..." << std::endl;
    _networkMgr.startNetwork(5000, 10);
}

void Skypy::onShutdown()
{
    std::cout << "Stopping Skypy server..." << std::endl;
}

void Skypy::update(uint32 diff)
{
    _processAddSession();
    _updateSessions(diff);
    _processDelSession();
}

void Skypy::addSession(Session* sess)
{
    Mutex::ScopLock lock(_sessionAddMutex);
    _sessionAddList.push_back(sess);
}

void Skypy::delSession(Session* sess)
{
    Mutex::ScopLock lock(_sessionDelMutex);
    _sessionDelList.push_back(sess);
}

void Skypy::_processAddSession()
{
    Mutex::ScopLock lock(_sessionAddMutex);
    for (std::list<Session*>::const_iterator itr = _sessionAddList.begin();
            itr != _sessionAddList.end(); ++itr)
        _sessionMap[(*itr)->getId()] = *itr;
    _sessionAddList.clear();
}

void Skypy::_processDelSession()
{
    Mutex::ScopLock lock(_sessionDelMutex);
    for (std::list<Session*>::const_iterator itr = _sessionDelList.begin();
            itr != _sessionDelList.end(); ++itr)
    {
        _sessionMap.erase((*itr)->getId());
        delete *itr;
    }
    _sessionDelList.clear();
}

void Skypy::_updateSessions(uint32 diff)
{
    for (std::map<uint32, Session*>::const_iterator itr = _sessionMap.begin();
            itr != _sessionMap.end(); ++itr)
        if (!itr->second->isLogout())
            itr->second->update(diff);
}