#include <QMessageBox>
#include <QRegExpValidator>
#include <QHostInfo>

#include "mainwindow.h"
#include "networkmgr.h"

#include "packet.hpp"
#include "opcodemgr.h"

#include "audiomanager.h"
#include "widgetaddcontactwindow.h"

#include <iostream>

#include "sipPacket.hpp"
#include "clientmgr.h"

#include <QNetworkInterface>

MainWindow::MainWindow(QMainWindow *parent) :
    QMainWindow(parent),
    _widgets(new QStackedWidget(this)),
    _loginForm(new WidgetLogin(this)),
    _contactForm(new WidgetContactsList(this)),
    _menuBar(new QMenuBar(this)),
    _configWindow(new WidgetConfigWindow(this))
{
    setCentralWidget(_widgets);
    _widgets->addWidget(_loginForm);
    _widgets->addWidget(_contactForm);
    _loginForm->initialize();
    _widgets->setCurrentWidget(_loginForm);

    setWindowTitle("Skypy");

    sNetworkMgr->setMainWindow(this);

    setMenuBar(_menuBar);
    QMenu* menu = _menuBar->addMenu("Menu");
    menu->addAction("Settings", this, SLOT(_handleSettings()));
    menu->addAction("Logout", this, SLOT(_handleLogout()));

    // Init audio
    int index = AudioStream::deviceIndex(sClientMgr->settings().value("inputDevice", "").toString());
    if (index != -1)
    {
        if (!sAudioManager->setInputDevice(index, AudioSample::MONO, AudioSample::FREQ_48000))
            std::cout << "FAIL INIT AUDIO INPUT: " << sAudioManager->errorText().toStdString() << std::endl;
    }
    else
        if (!sAudioManager->setInputDevice(DEFAULT_DEVICE, AudioSample::MONO, AudioSample::FREQ_48000))
            std::cout << "FAIL INIT AUDIO INPUT: " << sAudioManager->errorText().toStdString() << std::endl;

    index = AudioStream::deviceIndex(sClientMgr->settings().value("outputDevice", "").toString());
    if (index != -1)
    {
        if (!sAudioManager->setOutputDevice(index, AudioSample::MONO, AudioSample::FREQ_48000))
            std::cout << "FAIL INIT AUDIO OUPUT: " << sAudioManager->errorText().toStdString() << std::endl;
    }
    else
        if (!sAudioManager->setOutputDevice(DEFAULT_DEVICE, AudioSample::MONO, AudioSample::FREQ_48000))
            std::cout << "FAIL INIT AUDIO OUPUT: " << sAudioManager->errorText().toStdString() << std::endl;
}

MainWindow::~MainWindow()
{
}

void MainWindow::handleRequireAuth(QString const& localAddr)
{
    Packet pkt(CMSG_AUTH);
    pkt << _loginForm->getEmailText();
    pkt << _loginForm->getPasswordText();
    pkt << localAddr;
    sNetworkMgr->tcpSendPacket(pkt);
    std::cout << "AUTH SENDED" << std::endl;
    std::cout << "LOCAL IP: " << localAddr.toStdString() << std::endl;
    pkt.dumpHex();
}

bool MainWindow::handleAuthResult(Packet& pkt)
{
    quint8 result;
    pkt >> result;

    std::cout << "AUTH RESULT: " << quint32(result) << std::endl;

    if (result == 0)
    {
        _loginForm->unload();
        _contactForm->initialize();
        _widgets->setCurrentWidget(_contactForm);
    }
    else
        QMessageBox::information(this, "Authentification", "Fail to authenticate");

    return (result == 0);
}

void MainWindow::handleServerConnectionLost(QAbstractSocket::SocketError e, QString const& msg)
{
    (void)e;
    _contactForm->unload();
    _loginForm->initialize();
    setWindowTitle("Skypy");
    _widgets->setCurrentWidget(_loginForm);
    QMessageBox::information(this, "Connection error", "Error: " + msg);
}

void MainWindow::handleContactLogin(Packet& pkt)
{
    quint32 count;
    pkt >> count;
    std::cout << "RECEIV CONTACT COUNT: " << count << std::endl;
    for (quint32 i = 0; i < count; ++i)
    {
        quint32 id;
        QString name;
        QString email;
        QString ipPublic, ipPrivate;
        pkt >> id;
        pkt >> name;
        pkt >> email;
        pkt >> ipPublic;
        pkt >> ipPrivate;

        ContactInfo* info = sClientMgr->findContact(id);
        if (!info)
            continue;

        info->setPublicIp(ipPublic);
        info->setPrivateIp(ipPrivate);
        info->setOnline(true);

        _contactForm->loginContact(info);
    }
}

void MainWindow::handleContactLogout(Packet& pkt)
{
    quint32 count;
    pkt >> count;
    for (quint32 i = 0; i < count; ++i)
    {
        quint32 id;
        pkt >> id;

        ContactInfo* info = sClientMgr->findContact(id);
        if (!info)
            continue;
        info->setPublicIp("");
        info->setPrivateIp("");
        info->setOnline(false);

        _contactForm->logoutContact(id);
    }
}

void MainWindow::handleChatText(Packet &pkt)
{
    quint32 from;
    QString msg;
    pkt >> from;
    pkt >> msg;
    std::cout << "RECEIV MSG FROM: " << from << " - " << msg.toStdString() << std::endl;
    _contactForm->addMessageFrom(from, msg);
}

void MainWindow::handleSearchContactResult(Packet &pkt)
{
    quint32 count;
    pkt >> count;

    _contactForm->getContactWindow()->getResultListWidget()->clear();
    for (quint32 i = 0; i < count; ++i)
    {
        QString name;
        QString email;
        quint8 online;
        pkt >> name >> email >> online;
        ContactInfo* info = new ContactInfo(_contactForm->getContactWindow()->getResultListWidget(), 0, name, email, online != 0);
        info->setText(name + " (" + email + ")");
        _contactForm->getContactWindow()->addResult(info);
    }
}

void MainWindow::handleAddContactRequest(Packet &pkt)
{
    quint32 reqId;
    QString name;
    QString email;
    pkt >> reqId >> name >> email;

    Notification* notif = new Notification(_contactForm->getNotificationWidget(), NOTIF_CONTACT_REQUEST, "New contact request from " + name + " (" + email + ")", reqId);
    notif->setTextInfo(name + " (" + email + ")");
    _contactForm->addNotification(notif);
}

void MainWindow::handleSipRep(Packet &pkt)
{
  sNetworkMgr->handleSipRep(pkt);
}

void MainWindow::handleSipRequest(Packet &pkt)
{
  sNetworkMgr->handleSipRequest(pkt);
}

void MainWindow::handlesipResponse(SipRespond const& resp)
{
    if (resp.getCmd() == "INVITE")
    {
        _contactForm->handleCallResponse(resp);
    }
    else if (resp.getCmd() == "BYE")
        _contactForm->handleByeResponse(resp);
}

void MainWindow::handleCallRequest(SipRequest const& request)
{
    ContactInfo* sender = sClientMgr->findContact(request.getSenderId());
    if (!sender || sClientMgr->hasActiveCall() || sClientMgr->hasCallRequest())
    {

        SipRespond Rep(604, request);
        sNetworkMgr->tcpSendPacket(Rep.getPacket());
        return;
    }

    _contactForm->handleCallRequest(sender, request);
}

void MainWindow::handleByeRequest(const SipRequest &request)
{
    ContactInfo* sender = sClientMgr->findContact(request.getSenderId());
    if (!sender || !sClientMgr->hasActiveCallWith(request.getSenderId()))
    {

        SipRespond Rep(604, request);
        sNetworkMgr->tcpSendPacket(Rep.getPacket());
        return;
    }

    _contactForm->handleByeRequest(sender, request);

}

void MainWindow::handleAccountInfo(Packet& pkt)
{
    quint32 id;
    QString name, email, publicIp;
    pkt >> id >> name >> email >> publicIp;
    std::cout << "RECEIV id: " << id << " - name: " << name.toStdString() << " - email: " << email.toStdString() << " - publicIp: " << publicIp.toStdString() << std::endl;
    setWindowTitle("Skypy - " + name + " (" + email + ")");
    sClientMgr->setAccountInfo(id, name, email);
    sClientMgr->setPublicIp(publicIp);
}

void MainWindow::handleJoinChatGroup(Packet& pkt)
{
    quint32 chatId;
    quint32 memberCount;

    pkt >> chatId;
    pkt >> memberCount;

    _contactForm->createChatGroup(chatId);
    std::cout << "JOIN CHAT GROUP, COUNT: " << memberCount << std::endl;
    for (quint32 i = 0; i < memberCount; ++i)
    {
        quint32 id;
        QString name, email, ipPublic, ipPrivate;
        quint8 online;
        pkt >> id >> name >> email >> ipPublic >> ipPrivate >> online;
        if (id == sClientMgr->getAccountId())
            continue;
        WidgetChatTab::PeerInfo* peer = new WidgetChatTab::PeerInfo();
        peer->peerId = id;
        peer->peerName = name;
        peer->peerEmail = email;
        peer->peerPublicIp = ipPublic;
        peer->peerPrivateIp = ipPrivate;
        peer->online = online;
        _contactForm->chatGroupMemberJoin(chatId, peer);
    }
}

void MainWindow::handleChatGroupAddMember(Packet& pkt)
{
    quint32 chatId, id;
    QString name, email, ipPublic, ipPrivate;
    quint8 online;
    pkt >> chatId >> id >> name >> email >> ipPublic >> ipPrivate >> online;

    if (id == sClientMgr->getAccountId())
        return;

    WidgetChatTab::PeerInfo* peer = new WidgetChatTab::PeerInfo();
    peer->peerId = id;
    peer->peerName = name;
    peer->peerEmail = email;
    peer->peerPublicIp = ipPublic;
    peer->peerPrivateIp = ipPrivate;
    peer->online = online;
    _contactForm->chatGroupMemberJoin(chatId, peer);
}

void MainWindow::handleGroupChatText(Packet& pkt)
{
    quint32 chatId, fromId;
    QString msg;

    pkt >> chatId >> fromId >> msg;
    _contactForm->addChatGroupMessageFrom(chatId, fromId, msg);
}


void MainWindow::handleChatGroupUpdateMember(Packet& pkt)
{
    quint32 chatId, id;
    QString name, email, publicIp, privateIp;
    bool online;

    pkt >> chatId >> id >> name >> email >> publicIp >> privateIp >> online;

    if (id == sClientMgr->getAccountId())
        return;

    WidgetChatTab::PeerInfo peer;
    peer.peerId = id;
    peer.peerName = name;
    peer.peerEmail = email;
    peer.peerPublicIp = publicIp;
    peer.peerPrivateIp = privateIp;
    peer.online = online;
    _contactForm->chatGroupMemberUpdate(chatId, peer);
}

void MainWindow::handleContactList(Packet& pkt)
{
    quint32 count;
    pkt >> count;
    std::cout << "RECEIV CONTACT COUNT: " << count << std::endl;
    for (quint32 i = 0; i < count; ++i)
    {
        quint32 id;
        QString name;
        QString email;
        QString ipPublic, ipPrivate;
        quint8 online;
        pkt >> id;
        pkt >> name;
        pkt >> email;
        pkt >> ipPublic;
        pkt >> ipPrivate;
        pkt >> online;

        ContactInfo* info = sClientMgr->findContact(id);
        if (info)
        {
            info->setPublicIp(ipPublic);
            info->setPrivateIp(ipPrivate);
            info->setOnline(online != 0);
        }
        else
        {
            info = new ContactInfo(_contactForm->getContactListWidget(), id, name, email, online != 0, ipPublic, ipPrivate);
            _contactForm->addContact(info);
        }
    }
}

void MainWindow::handleChatGroupDelMember(Packet& pkt)
{
    quint32 chatId, id;
    pkt >> chatId >> id;

    if (id == sClientMgr->getAccountId())
        return;

    _contactForm->chatGroupRemoveMember(chatId, id);
}

void MainWindow::_handleLogout()
{
    sNetworkMgr->closeTcpConnection();
    _contactForm->unload();
    _loginForm->initialize();
    setWindowTitle("Skypy");
    _widgets->setCurrentWidget(_loginForm);
}

void MainWindow::_handleSettings()
{
    _configWindow->show();
}
