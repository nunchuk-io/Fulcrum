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
#pragma once

#include <QByteArray>
#include <QString>

/// Synchronous HTTP helper for the Nunchuk slipstream routing decision.
/// Intended to be called from a worker thread (e.g. ThreadPool work lambda).
namespace SlipstreamClient {

/// Nunchuk decision API result.
///   POST <decisionUrl>
///   Authorization: Bearer <apiToken>
///   Content-Type: application/json
///   Body: { "tx_hex": "<hex>", "tx_id": "<hex>", "fee_rate": <positive decimal sat/vB> }
///   Response 200 JSON:
///     { "data": { "status": "<string>", "message": "<string>",
///                 "should_use_slipstream": <bool> } }
///   If data.status == "error", Fulcrum fails the broadcast with data.message as the client error text.
/// When should_use_slipstream is true, the decision API itself performs the Slipstream
/// broadcast; Fulcrum only needs to echo the txid to the Electrum client.
struct DecisionResult {
    bool ok = false;           ///< HTTP/parse succeeded
    bool shouldUse = false;    ///< meaningful only when ok==true
    QString message;           ///< error detail when !ok
};

/// Ask Nunchuk whether this broadcast was (or should be) handled via Slipstream.
DecisionResult shouldUseSlipstream(const QString &decisionUrl, const QString &apiToken,
                                   const QByteArray &txHex, const QString &txId,
                                   double feeRateSatsPerVByte, int timeoutSecs);

} // namespace SlipstreamClient
