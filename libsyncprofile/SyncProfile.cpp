/*
 * This file is part of buteo-syncfw package
 *
 * Copyright (C) 2010 Nokia Corporation and/or its subsidiary(-ies).
 *
 * Contact: Sateesh Kavuri <sateesh.kavuri@nokia.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA
 *
 */

#include "SyncProfile.h"
#include "ProfileEngineDefs.h"
#include "LogMacros.h"
#include <QDomDocument>

namespace Buteo {

// Private implementation class for SyncProfile.
class SyncProfilePrivate
{
public:
    SyncProfilePrivate();

    SyncProfilePrivate(const SyncProfilePrivate &aSource);

    ~SyncProfilePrivate();

    SyncLog *iLog;

    SyncSchedule iSchedule;

    QList<int> mSyncRetryIntervals;

    int mSyncRetryCount;
};

}

using namespace Buteo;

SyncProfilePrivate::SyncProfilePrivate()
:   iLog(0),
    mSyncRetryCount(0)
{
    mSyncRetryIntervals.append(0);
}

SyncProfilePrivate::SyncProfilePrivate(const SyncProfilePrivate &aSource)
:   iLog(0),
    iSchedule(aSource.iSchedule)
{
    if (aSource.iLog != 0)
    {
        iLog = new SyncLog(*aSource.iLog);
    } // no else
}

SyncProfilePrivate::~SyncProfilePrivate()
{
    delete iLog;
    iLog = 0;
}

SyncProfile::SyncProfile(const QString &aName)
:   Profile(aName, Profile::TYPE_SYNC),
    d_ptr(new SyncProfilePrivate())
{
}

SyncProfile::SyncProfile(const QDomElement &aRoot)
:   Profile(aRoot),
    d_ptr(new SyncProfilePrivate())
{
    QDomElement schedule = aRoot.firstChildElement(TAG_SCHEDULE);
    if (!schedule.isNull())
    {
        d_ptr->iSchedule = SyncSchedule(schedule);
    } // no else
    QDomElement retriesElement = aRoot.firstChildElement(TAG_ERROR_ATTEMPTS);
    if (!retriesElement.isNull())
    {
        QDomElement timeElement  = retriesElement.firstChildElement(TAG_ATTEMPT_DELAY);
        while (!timeElement.isNull()) {
            bool ok = false;
            int parsedTime = timeElement.attribute(ATTR_VALUE, "-1").toInt(&ok);
            if ( ok && ( parsedTime > 0 ))
            {
                d_ptr->mSyncRetryIntervals.append(parsedTime);
            }
            timeElement = timeElement.nextSiblingElement(TAG_ATTEMPT_DELAY);
        }
    }
}

SyncProfile::SyncProfile(const SyncProfile &aSource)
:   Profile(aSource),
    d_ptr(new SyncProfilePrivate(*aSource.d_ptr))
{
}

SyncProfile::~SyncProfile()
{
    delete d_ptr;
    d_ptr = 0;
}

SyncProfile *SyncProfile::clone() const
{
    return new SyncProfile(*this);
}

QDomElement SyncProfile::toXml(QDomDocument &aDoc, bool aLocalOnly) const
{
    QDomElement root = Profile::toXml(aDoc, aLocalOnly);
    QDomElement schedule = d_ptr->iSchedule.toXml(aDoc);
    if (!schedule.isNull())
    {
        root.appendChild(schedule);
    } // no else
    if (d_ptr->mSyncRetryIntervals.count()>1)
    {
        QDomElement retries = aDoc.createElement(TAG_ERROR_ATTEMPTS);
        for ( int i = 1 ;  i < d_ptr->mSyncRetryIntervals.count(); i++)
        {
            QDomElement retryInterval = aDoc.createElement(TAG_ATTEMPT_DELAY);
            retryInterval.setAttribute(ATTR_VALUE,d_ptr->mSyncRetryIntervals.at(i));
            retries.appendChild(retryInterval);
        }
        root.appendChild(retries);
    }

    return root;
}

void SyncProfile::setName(const QString &aName)
{
  // sets the name in the super class Profile.
  Profile::setName(aName);
  // Here we also must set name for the log associated to this profile, 
  // as that is only set during log construction. Changing SyncProfile name does not 
  // reflect there
  if (d_ptr->iLog)
    d_ptr->iLog->setProfileName(aName);
}

void SyncProfile::setName(const QStringList &aKeys)
{
  // sets the name in the super class Profile.

  Profile::setName(aKeys);
  // Here we also must set name for the log associated to this profile, 
  // as that is only set during log construction. Changing SyncProfile name does not 
  // reflect there
  if (d_ptr->iLog)
    d_ptr->iLog->setProfileName(Profile::name());
}


QDateTime SyncProfile::lastSyncTime() const
{
    QDateTime lastSync;

    if (d_ptr->iLog != 0 && d_ptr->iLog->lastResults() != 0)
    {
        return d_ptr->iLog->lastResults()->syncTime();
    } // no else

    return lastSync;
}

QDateTime SyncProfile::nextSyncTime() const
{
    QDateTime nextSync;

    if ( ( syncType() == SYNC_SCHEDULED ) && ( d_ptr->mSyncRetryCount == 0 ) )
    {
        nextSync = d_ptr->iSchedule.nextSyncTime(lastSyncTime());
    }
    else
    {
        // Manual sync mode. Returned time for next sync is null.
        if ( ( d_ptr->mSyncRetryIntervals.count() > 0 ) && ( d_ptr->mSyncRetryCount > 0 ) && ( d_ptr->mSyncRetryCount < d_ptr->mSyncRetryIntervals.count( ) ) )
        {
            nextSync = QDateTime::currentDateTime().addSecs(d_ptr->mSyncRetryIntervals.at(d_ptr->mSyncRetryCount) * 60);
        }
    }

    return nextSync;
}

const SyncResults *SyncProfile::lastResults() const
{
    if (d_ptr->iLog != 0)
    {
        return d_ptr->iLog->lastResults();
    }
    else
    {
        return 0;
    }
}

const SyncLog *SyncProfile::log() const
{
    return d_ptr->iLog;
}

void SyncProfile::setLog(SyncLog *aLog)
{
    delete d_ptr->iLog;
    d_ptr->iLog = aLog;
}

void SyncProfile::addResults(const SyncResults &aResults)
{
    if (0 == d_ptr->iLog)
    {
        d_ptr->iLog = new SyncLog(name());
    } // no else

    d_ptr->iLog->addResults(aResults);
}

SyncProfile::SyncType SyncProfile::syncType() const
{
    return boolKey(KEY_SYNC_SCHEDULED) ? SYNC_SCHEDULED : SYNC_MANUAL;
}

void SyncProfile::setSyncType(SyncType aType)
{
    setBoolKey(KEY_SYNC_SCHEDULED, aType == SYNC_SCHEDULED);
}

SyncSchedule SyncProfile::syncSchedule() const
{
    return d_ptr->iSchedule;
}

void SyncProfile::setSyncSchedule(const SyncSchedule &aSchedule)
{
    d_ptr->iSchedule = aSchedule;
}

QStringList SyncProfile::storageBackendNames() const
{
    QStringList enabledStorageBackends;
    QStringList storageNames = subProfileNames(Profile::TYPE_STORAGE);

    foreach (QString storage, storageNames)
    {
        const Profile *p = subProfile(storage, Profile::TYPE_STORAGE);
        if (p->isEnabled())
        {
            // Get backend name from the storage profile. If the backend name
            // is not defined, use profile name as the backend name.
            enabledStorageBackends.append(p->key(KEY_BACKEND, p->name()));
        } // no else
    }

    return enabledStorageBackends;
}

QString SyncProfile::serviceName() const
{
    QStringList serviceNameList = subProfileNames(Profile::TYPE_SERVICE);

    if (serviceNameList.isEmpty()) 
	    return QString();
    
    return serviceNameList.first();
}

const Profile *SyncProfile::serviceProfile() const
{
    QList<const Profile*> subProfiles = allSubProfiles();
    foreach (const Profile *p, subProfiles)
    {
        if (p->type() == TYPE_SERVICE)
        {
            return p;
        } // no else
    }

    return 0;
}

Profile *SyncProfile::serviceProfile()
{
    QList<Profile*> subProfiles = allSubProfiles();
    foreach (Profile *p, subProfiles)
    {
        if (p->type() == TYPE_SERVICE)
        {
            return p;
        } // no else
    }

    return 0;
}

const Profile *SyncProfile::clientProfile() const
{
    QList<const Profile*> subProfiles = allSubProfiles();
    foreach (const Profile *p, subProfiles)
    {
        if (p->type() == TYPE_CLIENT)
        {
            return p;
        } // no else
    }

    return 0;
}

Profile *SyncProfile::clientProfile()
{
    QList<Profile*> subProfiles = allSubProfiles();
    foreach (Profile *p, subProfiles)
    {
        if (p->type() == TYPE_CLIENT)
        {
            return p;
        } // no else
    }

    return 0;
}

const Profile *SyncProfile::serverProfile() const
{
    QList<const Profile*> subProfiles = allSubProfiles();
    foreach (const Profile *p, subProfiles)
    {
        if (p->type() == TYPE_SERVER)
        {
            return p;
        } // no else
    }

    return 0;
}

Profile *SyncProfile::serverProfile()
{
    QList<Profile*> subProfiles = allSubProfiles();
    foreach (Profile *p, subProfiles)
    {
        if (p->type() == TYPE_SERVER)
        {
            return p;
        } // no else
    }

    return 0;
}

QList<const Profile*> SyncProfile::storageProfiles() const
{
    QList<const Profile*> storages;
    QList<const Profile*> subProfiles = allSubProfiles();
    foreach (const Profile *p, subProfiles)
    {
        if (p->type() == TYPE_STORAGE)
        {
            storages.append(p);
        } // no else
    }

    return storages;
}

QList<Profile*> SyncProfile::storageProfilesNonConst()
{
    QList<Profile*> storages;
    QList<Profile*> subProfiles = allSubProfiles();
    foreach (Profile *p, subProfiles)
    {
        if (p->type() == TYPE_STORAGE)
        {
            storages.append(p);
        } // no else
    }

    return storages;
}

SyncProfile::DestinationType SyncProfile::destinationType() const
{
    DestinationType type = DESTINATION_TYPE_UNDEFINED;
    QString typeStr;

    const Profile *service = serviceProfile();
    if (service)
    {
        typeStr = service->key(KEY_DESTINATION_TYPE);
    } // no else

    if (typeStr == VALUE_ONLINE)
    {
        type = DESTINATION_TYPE_ONLINE;
    }
    else if (typeStr == VALUE_DEVICE)
    {
        type = DESTINATION_TYPE_DEVICE;
    }
    else
    {
        type = DESTINATION_TYPE_UNDEFINED;
    }

    return type;
}

SyncProfile::SyncDirection SyncProfile::syncDirection() const
{
    SyncDirection dir = SYNC_DIRECTION_UNDEFINED;
    QString dirStr;

    const Profile *client = clientProfile();
    if (client)
    {
        dirStr = client->key(KEY_SYNC_DIRECTION);
    } // no else

    if (dirStr == VALUE_TWO_WAY)
    {
        dir = SYNC_DIRECTION_TWO_WAY;
    }
    else if (dirStr == VALUE_FROM_REMOTE)
    {
        dir = SYNC_DIRECTION_FROM_REMOTE;
    }
    else if (dirStr == VALUE_TO_REMOTE)
    {
        dir = SYNC_DIRECTION_TO_REMOTE;
    }
    else
    {
        dir = SYNC_DIRECTION_UNDEFINED;
    }

    return dir;
}

quint32 SyncProfile::syncOnChangeAfter() const
{
    quint32 syncOnChangeAfterTime = -1;

    const Profile *client = clientProfile();
    if (client)
    {
        QString time = client->key(KEY_SOC_AFTER);
        if(!time.isEmpty())
        {
            bool ok = false;
            syncOnChangeAfterTime = time.toUInt(&ok);
            if(false == ok)
            {
                syncOnChangeAfterTime = -1;
            }
        }
    }
    LOG_DEBUG("Sync on change after time from profile :" << syncOnChangeAfterTime);
    return syncOnChangeAfterTime;
}

void SyncProfile::setSyncDirection(SyncDirection aDirection)
{
    QString dirStr;

    switch (aDirection)
    {
    case SYNC_DIRECTION_TWO_WAY:
        dirStr = VALUE_TWO_WAY;
        break;

    case SYNC_DIRECTION_FROM_REMOTE:
        dirStr = VALUE_FROM_REMOTE;
        break;

    case SYNC_DIRECTION_TO_REMOTE:
        dirStr = VALUE_TO_REMOTE;
        break;

    case SYNC_DIRECTION_UNDEFINED:
    default:
        // Value string is left as null. Key gets deleted when the value is set.
        break;
    }

    Profile *client = clientProfile();
    if (client)
    {
        client->setKey(KEY_SYNC_DIRECTION, dirStr);
    }
    else
    {
        LOG_WARNING("Profile" << name() << "has no client profile");
        LOG_WARNING("Failed to set sync direction");
    }
}

SyncProfile::ConflictResolutionPolicy SyncProfile::conflictResolutionPolicy() const
{
    ConflictResolutionPolicy policy = CR_POLICY_UNDEFINED;
    QString policyStr;

    const Profile *client = clientProfile();
    if (client)
    {
        policyStr = client->key(KEY_CONFLICT_RESOLUTION_POLICY);
    } // no else

    if (policyStr == VALUE_PREFER_REMOTE)
    {
        policy = CR_POLICY_PREFER_REMOTE_CHANGES;
    }
    else if (policyStr == VALUE_PREFER_LOCAL)
    {
        policy = CR_POLICY_PREFER_LOCAL_CHANGES;
    }
    else
    {
        policy = CR_POLICY_UNDEFINED;
    }

    return policy;
}

void SyncProfile::setConflictResolutionPolicy(ConflictResolutionPolicy aPolicy)
{
    QString policyStr;

    switch (aPolicy)
    {
    case CR_POLICY_PREFER_REMOTE_CHANGES:
        policyStr = VALUE_PREFER_REMOTE;
        break;

    case CR_POLICY_PREFER_LOCAL_CHANGES:
        policyStr = VALUE_PREFER_LOCAL;
        break;

    case CR_POLICY_UNDEFINED:
    default:
        // Value string is left as null. Key gets deleted when the value is set.
        break;
    }

    Profile *client = clientProfile();
    if (client)
    {
        client->setKey(KEY_CONFLICT_RESOLUTION_POLICY, policyStr);
    }
    else
    {
        LOG_WARNING("Profile" << name() << "has no client profile");
        LOG_WARNING("Failed to set conflict resolution policy");
    }
}

int SyncProfile::syncRetryAttemptsCount() const
{
    return d_ptr->mSyncRetryIntervals.count();
}

int SyncProfile::syncCurrentAttempt() const
{
    return d_ptr->mSyncRetryCount;
}

int SyncProfile::syncRetryDelay(int aRetry) const
{
    if ( ( aRetry >=0 ) && ( aRetry < d_ptr->mSyncRetryIntervals.count() ) )
        return d_ptr->mSyncRetryIntervals.at(aRetry);
    return -1;
}

void SyncProfile::setSyncRetryAttemp(int aRetry)
{
    d_ptr->mSyncRetryCount = aRetry;
}

bool SyncProfile::needNextAttempt() const
{
    return d_ptr->mSyncRetryCount < d_ptr->mSyncRetryIntervals.count();
}

void SyncProfile::setNextAttempt()
{
    d_ptr->mSyncRetryCount++;
}

void SyncProfile::resetAttempts()
{
    d_ptr->mSyncRetryCount=0;
}
