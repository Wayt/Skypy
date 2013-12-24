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

MainWindow::MainWindow(QMainWindow *parent) :
    QMainWindow(parent),
    _widgets(new QStackedWidget(this)),
    _loginForm(new WidgetLoginForm(this)),
    _contactForm(new WidgetContactsList(this))
{
    setCentralWidget(_widgets);
    _widgets->addWidget(_loginForm);
    _widgets->addWidget(_contactForm);
    _loginForm->initialize();
    _widgets->setCurrentWidget(_loginForm);

    sNetworkMgr->setMainWindow(this);

    // Init audio
    if (!sAudioManager->setInputDevice(DEFAULT_DEVICE, AudioSample::MONO, AudioSample::FREQ_48000))
        std::cout << "FAIL INIT AUDIO INPUT: " << sAudioManager->errorText().toStdString() << std::endl;
    if (!sAudioManager->setOutputDevice(DEFAULT_DEVICE, AudioSample::MONO, AudioSample::FREQ_48000))
        std::cout << "FAIL INIT AUDIO OUPUT: " << sAudioManager->errorText().toStdString() << std::endl;
}

MainWindow::~MainWindow()
{
}

void MainWindow::handleRequireAuth()
{
    Packet pkt(CMSG_AUTH);
    pkt << _loginForm->getEmailText();
    pkt << _loginForm->getPasswordText();
    sNetworkMgr->tcpSendPacket(pkt);
    std::cout << "AUTH SENDED" << std::endl;
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
    setWindowTitle("");
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
        pkt >> id;
        pkt >> name;
        pkt >> email;
        ContactInfo* info = new ContactInfo(_contactForm->getContactListWidget(), id, name, email);
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

    ContactInfo* info = new ContactInfo(NULL, reqId, name, email, false);
    info->setText(name + " (" + email + ")");

    Notification* notif = new Notification(_contactForm->getNotificationWidget(), NOTIF_CONTACT_REQUEST, "New contact request from " + info->text(), info);
    _contactForm->getNotificationWidget()->addItem(notif);
}

void MainWindow::handleSipRep(Packet &pkt)
{
  sNetworkMgr->handleSipRep(pkt);
}

void MainWindow::handleSipRequest(Packet &pkt)
{
  sNetworkMgr->handleSipRequest(pkt);
}

void MainWindow::handlesipResponse(SipRequest const* req, SipRespond const* resp)
{
    if (req->getCmd() == "INVITE")
    {
        switch (resp->getCode())
        {
        case 100: // Forward de l'appel
            _contactForm->addMessageFrom(req->getDestId(), "Send call request ...", true);
            break;
        case 404: // Contact non connecte
            _contactForm->addMessageFrom(req->getDestId(), req->getDestEmail() + " isn't online", true);
            break;
        case 180: // Ca sonne
            _contactForm->addMessageFrom(req->getDestId(), "Ringing ...", true);
            break;
        case 200: // Ca a decrocher
            _contactForm->addMessageFrom(req->getDestId(), "Call accepted", true);
            if (sAudioManager->start())
            {
                sNetworkMgr->setCallPeerAddr(QHostAddress(resp->getDestIp()), resp->getDestPort());
                sNetworkMgr->runCall();
            }
            break;
        case 603: // Refuse
            _contactForm->addMessageFrom(req->getDestId(), "Call refused", true);
            break;
        }
    }
}

void MainWindow::handleCallRequest(SipRequest const& request)
{
    QMessageBox::StandardButton reply;
    reply = QMessageBox::question(this, "Incomming call", "Accept call from " + request.getSenderEmail() + " ?",
                              QMessageBox::Yes | QMessageBox::No);
    switch (reply)
    {
        case QMessageBox::Yes:
        case QMessageBox::No:
        {
            QHostInfo info = QHostInfo::fromName( QHostInfo::localHostName()  );
            QList<QHostAddress> ips = info.addresses();
            if (reply == QMessageBox::Yes && ips.size() > 0)
            {
                for (quint32 selfPort = AUDIO_PORT; selfPort < AUDIO_PORT + 200; ++selfPort)
                    if (sNetworkMgr->setCallHostAddr(ips[0], selfPort))
                    {
                        if (sAudioManager->start())
                        {
                            sNetworkMgr->setCallPeerAddr(QHostAddress(request.getSenderIp()), request.getSenderPort());
                            sNetworkMgr->runCall();
                            std::cout << "CALL ACCEPTED, LISTEN ON " << ips[0].toString().toStdString() << ":" << selfPort << std::endl;
                            SipRespond Rep(200, "INVITE", request.getSenderEmail(), request.getSenderId(), request.getSenderIp(), request.getSenderPort(),
                                           request.getDestEmail(), request.getDestId(), request.getDestIp(), selfPort);
                            sNetworkMgr->tcpSendPacket(Rep.getPacket());
                        }
                        else // Should send error
                            std::cout << "FAIL TO START AUDIO" << std::endl;
                        break;
                    }

                //if (sAudioManager->start())
                  //{
                    //sNetworkMgr->setCallPeerAddr(QHostAddress(host), AUDIO_PORT + ((_peerId % 2) == 0 ? 1 : 0));
                    //sNetworkMgr->runCall);
                    //std::cout << "RUN CALL" << std::endl;

                  //}
                //else
                  //std::cerr << "FAIL START: " << sAudioManager->errorText().toStdString() << std::endl;
            }
            else
            {
                SipRespond Rep(603, "INVITE", request.getSenderEmail(), request.getSenderId(), request.getSenderIp(), request.getSenderPort(),
                                           request.getDestEmail(), request.getDestId(), request.getDestIp(), 0);
                sNetworkMgr->tcpSendPacket(Rep.getPacket());
                std::cout << "SEND CALL REFUSED" << std::endl;
            }
            break;
        }
    }
}

void MainWindow::handleAccountInfo(Packet& pkt)
{
    quint32 id;
    QString name, email;
    pkt >> id >> name >> email;
    setWindowTitle(name + " (" + email + ")");
    sClientMgr->setAccountInfo(id, name, email);

}
