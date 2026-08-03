//
// Fulcrum - A fast & nimble SPV Server for Bitcoin Cash
// Copyright (C) 2019-2026 Calin A. Culianu <calin.culianu@gmail.com>
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program (see LICENSE.txt).  If not, see
// <https://www.gnu.org/licenses/>.
//
#include "SlipstreamClient.h"

#include "Json/Json.h"
#include "Util.h"

#include <QEventLoop>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QTimer>
#include <QUrl>
#include <QVariantMap>

#include <algorithm>

namespace SlipstreamClient {
namespace {

struct HttpJsonResult {
    bool ok = false;
    int httpStatus = 0;
    QByteArray body;
    QString error;
};

HttpJsonResult httpPostJson(const QUrl &url, const QByteArray &payload, int timeoutSecs,
                            const QString &bearerToken = {})
{
    HttpJsonResult ret;
    if (!url.isValid() || url.scheme().isEmpty() || url.host().isEmpty()) {
        ret.error = QStringLiteral("invalid URL: %1").arg(url.toString());
        return ret;
    }

    QNetworkAccessManager nam;
    QNetworkRequest req(url);
    req.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
    if (!bearerToken.isEmpty())
        req.setRawHeader(QByteArrayLiteral("Authorization"),
                         QByteArrayLiteral("Bearer ") + bearerToken.toUtf8());
#if QT_VERSION >= QT_VERSION_CHECK(5, 15, 0)
    req.setTransferTimeout(std::max(1, timeoutSecs) * 1000);
#endif

    QEventLoop loop;
    QTimer timer;
    timer.setSingleShot(true);
    QObject::connect(&timer, &QTimer::timeout, &loop, &QEventLoop::quit);

    QNetworkReply *reply = nam.post(req, payload);
    QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);

    timer.start(std::max(1, timeoutSecs) * 1000);
    loop.exec();

    if (!reply->isFinished()) {
        reply->abort();
        if (!reply->isFinished())
            loop.exec();
        ret.error = QStringLiteral("request timed out after %1s").arg(timeoutSecs);
        delete reply;
        return ret;
    }

    const auto netErr = reply->error();
    const QString netErrStr = reply->errorString();
    ret.body = reply->readAll();
    ret.httpStatus = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    delete reply;

    if (netErr != QNetworkReply::NoError) {
        if (!ret.body.isEmpty())
            ret.error = QStringLiteral("%1 (HTTP %2): %3")
                            .arg(netErrStr).arg(ret.httpStatus)
                            .arg(QString::fromUtf8(ret.body.left(200)));
        else
            ret.error = QStringLiteral("%1 (HTTP %2)").arg(netErrStr).arg(ret.httpStatus);
        return ret;
    }

    ret.ok = true;
    return ret;
}

} // namespace

DecisionResult shouldUseSlipstream(const QString &decisionUrl, const QString &apiToken,
                                   const QByteArray &txHex, const QString &txId,
                                   double feeRateSatsPerVByte, int timeoutSecs)
{
    DecisionResult ret;

    const QString urlStr = decisionUrl.trimmed();
    if (urlStr.isEmpty()) {
        ret.message = QStringLiteral("slipstream decision URL is empty");
        return ret;
    }
    if (apiToken.trimmed().isEmpty()) {
        ret.message = QStringLiteral("slipstream API token is empty");
        return ret;
    }
    if (!(feeRateSatsPerVByte > 0.0)) {
        ret.message = QStringLiteral("fee_rate must be a positive decimal sat/vB");
        return ret;
    }

    QVariantMap body;
    body.insert(QStringLiteral("tx_hex"), QString::fromLatin1(txHex));
    body.insert(QStringLiteral("tx_id"), txId);
    body.insert(QStringLiteral("fee_rate"), feeRateSatsPerVByte);

    const auto http = httpPostJson(QUrl(urlStr), Json::toUtf8(body, true), timeoutSecs, apiToken);
    if (!http.ok) {
        ret.message = QStringLiteral("should_use_slipstream: %1").arg(http.error);
        return ret;
    }

    try {
        const QVariant parsed = Json::parseUtf8(http.body, Json::ParseOption::AcceptAnyValue);
        if (!parsed.canConvert<QVariantMap>()) {
            ret.message = QStringLiteral("should_use_slipstream: unexpected JSON type");
            return ret;
        }
        const QVariantMap root = parsed.toMap();
        const QVariant dataVar = root.value(QStringLiteral("data"));
        if (!dataVar.canConvert<QVariantMap>()) {
            ret.message = QStringLiteral("should_use_slipstream: response missing data object");
            return ret;
        }
        const QVariant v = dataVar.toMap().value(QStringLiteral("should_use_slipstream"));
        if (!v.isValid() || v.type() != QVariant::Bool) {
            ret.message = QStringLiteral("should_use_slipstream: data.should_use_slipstream missing or not bool");
            return ret;
        }
        ret.ok = true;
        ret.shouldUse = v.toBool();
        return ret;
    } catch (const std::exception &e) {
        ret.message = QStringLiteral("should_use_slipstream: invalid JSON (%1)")
                          .arg(QString::fromUtf8(e.what()));
        return ret;
    }
}

} // namespace SlipstreamClient
