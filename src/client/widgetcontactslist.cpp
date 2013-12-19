#include "widgetcontactslist.h"
#include <QtGlobal>
#include <iostream>
#include "widgetchatwindow.h"

WidgetContactsList::WidgetContactsList(QWidget *parent) :
    QWidget(parent),
    Ui::WidgetContactsList(),
    _contactMap(),
    _chatWindow(new WidgetChatWindow(this))
{
    setupUi(this);
    QObject::connect(_contactList, SIGNAL(itemDoubleClicked(QListWidgetItem*)), this, SLOT(handleContactDoubleClick(QListWidgetItem*)));
}

void WidgetContactsList::initialize()
{
    _contactMap.clear();
    _contactList->clear();
}

void WidgetContactsList::unload()
{
    _chatWindow->hide();
}

void WidgetContactsList::loginContact(ContactInfo* info)
{
    _contactList->addItem(info);
    _contactMap[info->getId()] = info;
}

void WidgetContactsList::logoutContact(quint32 id)
{
    ContactInfo* info = findContact(id);
    if (!info)
        return;

    _contactList->removeItemWidget(info);
    _contactMap.erase(id);
    delete info;
}

void WidgetContactsList::handleContactDoubleClick(QListWidgetItem* item)
{
    ContactInfo* contact = dynamic_cast<ContactInfo*>(item);
    std::cout << "CLICKED ON " << contact->getEmail().toStdString() << std::endl;

    if (contact)
    {
        _chatWindow->addChatTab(contact, true);
        _chatWindow->show();
    }
}


ContactInfo* WidgetContactsList::findContact(quint32 id)
{
    for (std::map<quint32, ContactInfo*>::const_iterator itr = _contactMap.begin();
         itr != _contactMap.end(); ++itr)
        std::cout << "ID: " << itr->first << " - " << itr->second->getEmail().toStdString() << std::endl;
    std::map<quint32, ContactInfo*>::const_iterator itr = _contactMap.find(id);
    if (itr == _contactMap.end())
        return NULL;
    return itr->second;
}


void WidgetContactsList::addMessageFrom(quint32 id, QString const& msg)
{
    ContactInfo* info = findContact(id);
    if (!info)
        return;

    std::cout << "MSG FROM: " << info->getEmail().toStdString() << " - " << msg.toStdString() << std::endl;
    _chatWindow->addMessageFrom(info, msg);
}