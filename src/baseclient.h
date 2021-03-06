/*
 * Copyright (C) 2016 Roman Shchekin aka mrqtros.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 3.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef BASECLIENT_H_
#define BASECLIENT_H_

#include <atomic>
#include <deque>
#include <map>
#include <memory>
#include <string>
#include <core/net/http/request.h>
#include <core/net/uri.h>

#include <QJsonDocument>
#include <QString>
#include <QList>
#include <QDebug>
#include <QMap>
#include <QByteArray>

#include <QLoggingCategory>

#include "config.h"
#include "course.h"

Q_DECLARE_LOGGING_CATEGORY(BaseCli)

/**
 * Provide a nice way to access the HTTP API.
 *
 * We don't want our scope's code to be mixed together with HTTP and JSON handling.
 */
class BaseClient
{
public:
    BaseClient(Config::Ptr config);

    virtual ~BaseClient() = default;

    virtual QList<Course> courses(const QString& query) = 0;
    virtual const QString baseApiUrl() const = 0;
    virtual const QString name() const = 0;
    virtual const QMap<QByteArray, QByteArray> customHeaders() const;

    /**
     * Cancel any pending queries (this method can be called from a different thread)
     */
    virtual void cancel();

    virtual Config::Ptr config();

protected:
    virtual void get(const core::net::Uri::Path &path, const core::net::Uri::QueryParameters &parameters, QByteArray &result);
    /**
     * Progress callback that allows the query to cancel pending HTTP requests.
     */
    core::net::http::Request::Progress::Next progress_report(const core::net::http::Request::Progress& progress);

    /**
     * Hang onto the configuration information
     */
    Config::Ptr m_config;

    /**
     * Thread-safe cancelled flag
     */
    std::atomic<bool> m_cancelled;
};

#endif // BASECLIENT_H_

