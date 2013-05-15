/* * This file is part of Harmattan Organiser GUI *
 *
 * Copyright (C) 2009-2010 Nokia Corporation and/or its subsidiary(-ies).
 * All rights reserved.
 *
 * Contact: Andrey Moiseenko <andrey.moiseenko@nokia.com>
 *
 * This software, including documentation, is protected by copyright
 * controlled by Nokia Corporation. All rights are reserved. Copying,
 * including reproducing, storing, adapting or translating, any or all of
 * this material requires the prior written consent of Nokia Corporation.
 * This material also contains confidential information which may not be
 * disclosed to others without the prior written consent of Nokia.
 */


#include "defaultinvitationplugin.h"
#include <extendedcalendar.h>
#include <QDebug>
#include <icalformat.h>
#include <QTimer>

#ifdef MKCAL_FOR_MEEGO
#include <qmailaccount.h>
#include <qmailstore.h>
#include <qmailaddress.h>
#include <qmailcodec.h>
#include "transmitemail.h"
#endif

using namespace KCalCore;

const QString name("DefaultInvitationPlugin");

//@cond PRIVATE
class DefaultInvitationPlugin::Private
{
public:
  Private() : mStore( 0 ), mDefaultAccount( 0 ), mInit ( false ),
  mErrorCode( ServiceInterface::ErrorOk )
  {
  }
  ~Private() {
    uninit();
  }

  void init() {

    if ( !mInit ) {
      mStore = QMailStore::instance();
      Q_ASSERT(mStore);
      QMailAccountKey byDefault = QMailAccountKey::status( QMailAccount::PreferredSender );
      QMailAccountIdList accounts = mStore->queryAccounts(byDefault);
      if (!accounts.count()) {
        qWarning() << "Default account was not found!";
        //        delete mStore;
        mStore = 0;
        mErrorCode = ServiceInterface::ErrorNoAccount;
        return;
      }
      if (accounts.count() > 1) {
        qWarning( "There are more than one default account, using first" );
      }
      mDefaultAccount =  new QMailAccount( accounts.first() );
      mInit = true;
    }
  }

  QString defaultAddress() {

    if ( mInit ) {
      return mDefaultAccount->fromAddress().address();
    } else {
      return QString();
    }
  }

  void uninit() {

    delete mStore;
    mStore = 0;
    delete mDefaultAccount;
    mDefaultAccount = 0;
    mInit = false;
  }

  bool sendMail( const QStringList &recipients, const QString &subject,
                 const QString &body, const QString &attachment ) {

    if ( !mInit ) {
      return false;
    }

    // Build a message
    QMailMessage message;
    // Setup account which should be used to send a message
    message.setParentAccountId( mDefaultAccount->id() );
    // Get the outbox folder ID
    QMailFolderId folderId = mDefaultAccount->standardFolder(QMailFolder::OutboxFolder);
    if (!folderId.isValid()) {
        folderId = QMailFolder::LocalStorageFolderId;
    }
    // Put message to standard outbox folder fo that account
    message.setParentFolderId( folderId );

    // Setup message status
    message.setStatus(QMailMessage::Outbox, true);
    message.setStatus(QMailMessage::Outgoing, true);
    message.setStatus(QMailMessage::ContentAvailable, true);
    message.setStatus(QMailMessage::PartialContentAvailable, true);
    message.setStatus(QMailMessage::Read, true);
    message.setDate(QMailTimeStamp(QDateTime::currentDateTime()));

    // Define recipeint's address
    QList<QMailAddress> addresses;
    foreach( const QString &mail, recipients) {
      addresses.append( QMailAddress( mail ) );
    }
    message.setTo( addresses );
    // Define from address
    message.setFrom( mDefaultAccount->fromAddress() );
    // Define subject
    message.setSubject( subject );
    message.setMessageType(QMailMessage::Email);
    message.setMultipartType(QMailMessagePartContainerFwd::MultipartAlternative);

    // Create the MIME part representing the message body
    QMailMessagePart bodyPart = QMailMessagePart::fromData(
        body,
        QMailMessageContentDisposition(QMailMessageContentDisposition::None),
        QMailMessageContentType("text/plain; charset=\"utf-8\""),
        QMailMessageBody::QuotedPrintable);
    bodyPart.removeHeaderField("Content-Disposition");

    // Create the calendar MIME part
    QMailMessagePart calendarPart = QMailMessagePart::fromData(
        attachment,
        QMailMessageContentDisposition(QMailMessageContentDisposition::None),
        QMailMessageContentType("text/calendar; charset=\"utf-8\"; method=REQUEST"),
        QMailMessageBody::Base64);
    calendarPart.removeHeaderField("Content-Disposition");
    calendarPart.appendHeaderField("Content-Class","urn:content-classes:calendarmessage");

    message.appendPart(bodyPart);
    message.appendPart(calendarPart);

    // send (to outbox)
    if (!mStore->addMessage(&message))
      return false;

    // initiate transmission
    QSharedPointer<QMailTransmitAction> action ( new QMailTransmitAction() );
    if ( action ) {
      action->transmitMessages( mDefaultAccount->id() );
    } else {
      qDebug( "No MailTransmistAction" );
      return false;
    }

    return true;
  }

  QMailStore *mStore;
  QMailAccount *mDefaultAccount;
  bool mInit;
  ServiceInterface::ErrorCode mErrorCode;
};


DefaultInvitationPlugin::DefaultInvitationPlugin(): d( new Private() )
{
}

DefaultInvitationPlugin::~DefaultInvitationPlugin()
{
  delete d;
}


bool DefaultInvitationPlugin::sendInvitation(const QString &accountId, const QString &notebookUid, const Incidence::Ptr &invitation, const QString &body)
{

  Q_UNUSED( body );
  Q_UNUSED( accountId );
  Q_UNUSED( notebookUid );

  d->mErrorCode = ServiceInterface::ErrorOk;

  Attendee::List attendees = invitation->attendees();
  if ( attendees.size() == 0 ) {
    qDebug("No attendees");
    return false;
  }

  d->init();

  ICalFormat icf;
  QString ical =  icf.createScheduleMessage( invitation, iTIPPublish ) ;

  QStringList emails;
  foreach (const Attendee::Ptr &att, attendees) {
    emails.append( att->email() );
  }

  const QString &description = invitation->description();
  const bool res = d->sendMail( emails, invitation->summary(), description, ical );

//  d->uninit();
  return res;
}

bool DefaultInvitationPlugin::sendUpdate(const QString &accountId, const Incidence::Ptr &invitation, const QString &body)
{
  //Same as sendResponse
  return sendResponse(accountId, invitation, body);
}

bool DefaultInvitationPlugin::sendResponse(const QString &accountId, const Incidence::Ptr &invitation, const QString &body)
{
  Q_UNUSED(accountId);    // not needed by this plugin: we use the default account

  d->mErrorCode = ServiceInterface::ErrorOk;

  d->init();
  // Is there an organizer?
  Person::Ptr organizer = invitation->organizer();
  if (organizer->isEmpty() || organizer->email().isEmpty()) { // we do not have an organizer
    qWarning() << "sendResponse() called with wrong invitation: there is no organizer!";
    return false;
  }

  // Check: Am I one of the attendees? Had the organizer requested RSVP from me?
  Attendee::Ptr me = invitation->attendeeByMail( d->defaultAddress() );
  if (me == 0 || !me->RSVP()) {
    qWarning() << "sendResponse() called with wrong invitation: we are not invited or no response is expected.";
    return false;
  }

  ICalFormat icf;
  QString ical =  icf.createScheduleMessage( invitation, iTIPReply) ;
  //    QString base64Data = ical;

  bool res = d->sendMail(QStringList( organizer->email() ), invitation->summary(), body, ical);

//  d->uninit();
  return res;

}

QString DefaultInvitationPlugin::pluginName() const
{
  d->mErrorCode = ServiceInterface::ErrorOk;
  return name;
}

QString DefaultInvitationPlugin::icon() const
{
  return QString();
}

QString DefaultInvitationPlugin::uiName() const
{
  return QLatin1String("Default");
}

bool DefaultInvitationPlugin::multiCalendar() const
{
  d->mErrorCode = ServiceInterface::ErrorNotSupported;
  return false;
}

QString DefaultInvitationPlugin::emailAddress(const mKCal::Notebook::Ptr &notebook)
{
  Q_UNUSED( notebook );
  d->init();
  QString email ( d->defaultAddress() );
//  d->uninit();
  return email;
}

QString DefaultInvitationPlugin::displayName(const mKCal::Notebook::Ptr &notebook) const
{
  Q_UNUSED( notebook );
  return QString();
}

bool DefaultInvitationPlugin::downloadAttachment(const mKCal::Notebook::Ptr &notebook, const QString &uri, const QString &path)
{
  Q_UNUSED( notebook );
  Q_UNUSED( uri );
  Q_UNUSED( path );
  d->mErrorCode = ServiceInterface::ErrorNotSupported;
  return false;

}

bool DefaultInvitationPlugin::deleteAttachment(const mKCal::Notebook::Ptr &notebook, const Incidence::Ptr &incidence, const QString &uri)
{
  Q_UNUSED( notebook );
  Q_UNUSED( incidence );
  Q_UNUSED( uri );
  d->mErrorCode = ServiceInterface::ErrorNotSupported;
  return false;

}

bool DefaultInvitationPlugin::shareNotebook(const mKCal::Notebook::Ptr &notebook, const QStringList &sharedWith)
{
  Q_UNUSED( notebook );
  Q_UNUSED( sharedWith );
  d->mErrorCode = ServiceInterface::ErrorNotSupported;
  return false;
}

QStringList DefaultInvitationPlugin::sharedWith(const mKCal::Notebook::Ptr &notebook)
{
  Q_UNUSED( notebook );
  d->mErrorCode = ServiceInterface::ErrorNotSupported;
  return QStringList();
}

QString DefaultInvitationPlugin::DefaultInvitationPlugin::serviceName() const
{
  d->mErrorCode = ServiceInterface::ErrorOk;
  return name;
}


QString DefaultInvitationPlugin::defaultNotebook() const
{
  d->mErrorCode = ServiceInterface::ErrorNotSupported;
  return QString();
}

bool DefaultInvitationPlugin::checkProductId(const QString &prodId) const
{
  Q_UNUSED(prodId);
  d->mErrorCode = ServiceInterface::ErrorNotSupported;
  return false;
}

ServiceInterface::ErrorCode DefaultInvitationPlugin::error() const
{
  return d->mErrorCode;
}


Q_EXPORT_PLUGIN2(defaultinvitationplugin, DefaultInvitationPlugin);
