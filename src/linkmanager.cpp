#include <QList>
#include <QApplication>
#include "linkmanager.h"
#include <iostream>

#include <QDebug>

LinkManager* LinkManager::instance()
{
    static LinkManager* _instance = 0;
    if(_instance == 0) {
        _instance = new LinkManager();

        /* Set the application as parent to ensure that this object
         * will be destroyed when the main application exits */
        _instance->setParent(qApp);
    }
    return _instance;
}

/**
 *   Private singleton constructor
 *
 * This class implements the singleton design pattern and has therefore only a private constructor.
 **/
LinkManager::LinkManager()
{
    links = QList<LinkInterface*>();
    protocolLinks = QMap<ProtocolInterface*, LinkInterface*>();
}

LinkManager::~LinkManager()
{
    disconnectAll();
    foreach (LinkInterface* link, links)
    {
        if(link) link->deleteLater();
    }
}

void LinkManager::add(LinkInterface* link)
{
    if (!link || links.contains(link))
        return;

    connect(link, SIGNAL(destroyed(QObject*)), this, SLOT(removeLink(QObject*)));
    links.append(link);
    linkTypesMap.insertMulti(link->getLinkType(), link);
    emit newLink(link);
}

void LinkManager::addProtocol(LinkInterface* link, ProtocolInterface* protocol)
{
    // Connect link to protocol
    // the protocol will receive new bytes from the link
    if(!link || !protocol) return;

    QList<LinkInterface*> linkList = protocolLinks.values(protocol);

    // If protocol has not been added before (list length == 0)
    // OR if link has not been added to protocol, add
    if ((linkList.length() > 0 && !linkList.contains(link)) || linkList.length() == 0)
    {
        // Protocol is new, add
        connect(link, SIGNAL(bytesReceived(LinkInterface*, QByteArray)), protocol, SLOT(receiveBytes(LinkInterface*, QByteArray)));

//        qDebug() << link->getPortName();
        // Store the connection information in the protocol links map
        protocolLinks.insertMulti(protocol, link);
    }
}

QList<LinkInterface *> LinkManager::getLinksForProtocol(ProtocolInterface* protocol)
{
    return protocolLinks.values(protocol);
}

QList<LinkInterface *> LinkManager::getLinksForType(const int linkType)
{
    return linkTypesMap.values(linkType);
}

bool LinkManager::connectAll()
{
    bool allConnected = true;

    foreach (LinkInterface* link, links)
    {
        if(!link) {}
        else if(!link->connect()) allConnected = false;
    }

    return allConnected;
}

bool LinkManager::disconnectAll()
{
    bool allDisconnected = true;

    foreach (LinkInterface* link, links)
    {
        //static int i=0;
        if(!link) {}
        else if(!link->disconnect()) allDisconnected = false;
    }

    return allDisconnected;
}

bool LinkManager::connectLink(LinkInterface* link)
{
    if(!link) return false;
    return link->connect();
}

bool LinkManager::disconnectLink(LinkInterface* link)
{
    if(!link) return false;
    return link->disconnect();
}

void LinkManager::removeLink(QObject* link)
{
    LinkInterface* linkInterface = dynamic_cast<LinkInterface*>(link);
    if (linkInterface)
    {
        removeLink(linkInterface);
    }
}

bool LinkManager::removeLink(LinkInterface* link)
{
    if (!link)
        return false;

    for (int i=0; i < QList<LinkInterface*>(links).size(); i++) {
        if (link == links.at(i)) {
            linkTypesMap.remove(linkTypesMap.key(link));
            links.removeAt(i); //remove from link list
        }
    }
    // Remove link from protocol map
    foreach (ProtocolInterface* proto, protocolLinks.keys(link))  {
        protocolLinks.remove(proto, link);
    }

    // Emit removal of link
    emit linkRemoved(link);

    return true;
}

/**
 * The access time is linear in the number of links.
 *
 * @param id link identifier to search for
 * @return A pointer to the link or NULL if not found
 */
LinkInterface* LinkManager::getLinkForId(int id)
{
    foreach (LinkInterface* link, links)
    {
        if (link->getId() == id) return link;
    }
    return NULL;
}

/**
 *
 */
const QList<LinkInterface*> LinkManager::getLinks()
{
    return QList<LinkInterface*>(links);
}
