/*
 * Copyright (c) 2026
 *
 * SPDX-License-Identifier: GPL-2.0-only
 */

#include "ble-error-model.h"

#include "ble-spectrum-value-helper.h"
#include "ble-utils.h"

#include "ns3/double.h"
#include "ns3/log.h"

#include <algorithm>
#include <cmath>

namespace ns3
{

NS_LOG_COMPONENT_DEFINE("BleErrorModel");

namespace ble
{

NS_OBJECT_ENSURE_REGISTERED(BleErrorModel);

TypeId
BleErrorModel::GetTypeId()
{
    static TypeId tid =
        TypeId("ns3::ble::BleErrorModel")
            .SetParent<Object>()
            .SetGroupName("Ble")
            .AddConstructor<BleErrorModel>()
            .AddAttribute("ImplementationLoss",
                          "Loss applied to every PHY mode, in dB, representing the gap between "
                          "the modelled receiver and an ideal non-coherent GFSK receiver.",
                          DoubleValue(0.0),
                          MakeDoubleAccessor(&BleErrorModel::m_implementationLossDb),
                          MakeDoubleChecker<double>(0.0, 20.0))
            .AddAttribute("CodedS2ImplementationLoss",
                          "Additional loss of the LE Coded S=2 PHY, in dB.",
                          DoubleValue(0.0),
                          MakeDoubleAccessor(&BleErrorModel::m_codedS2ImplementationLossDb),
                          MakeDoubleChecker<double>(0.0, 20.0))
            .AddAttribute("CodedS8ImplementationLoss",
                          "Additional loss of the LE Coded S=8 PHY, in dB. The default of 2 dB "
                          "makes the modelled sensitivity match the published figures of common "
                          "radios, whose pattern mapper does not reach the coding gain of an "
                          "ideal code of the same rate.",
                          DoubleValue(2.0),
                          MakeDoubleAccessor(&BleErrorModel::m_codedS8ImplementationLossDb),
                          MakeDoubleChecker<double>(0.0, 20.0));
    return tid;
}

BleErrorModel::BleErrorModel()
    : m_implementationLossDb(0.0),
      m_codedS2ImplementationLossDb(0.0),
      m_codedS8ImplementationLossDb(2.0)
{
    NS_LOG_FUNCTION(this);
}

BleErrorModel::~BleErrorModel()
{
    NS_LOG_FUNCTION(this);
}

double
BleErrorModel::GetImplementationLossDb(BlePhyMode mode) const
{
    double loss = m_implementationLossDb;
    if (mode == BlePhyMode::LE_CODED_S2)
    {
        loss += m_codedS2ImplementationLossDb;
    }
    else if (mode == BlePhyMode::LE_CODED_S8)
    {
        loss += m_codedS8ImplementationLossDb;
    }
    return loss;
}

double
BleErrorModel::GetEbNo(double sinr, BlePhyMode mode) const
{
    if (sinr <= 0.0)
    {
        return 0.0;
    }
    // the signal to noise ratio is measured in the receiver bandwidth, which is one symbol rate
    // wide; spreading the same energy over fewer information bits raises the energy per bit
    const double bandwidthOverBitRate =
        BleSpectrumValueHelper::GetNoiseBandwidthHz(mode) / GetBitRate(mode);
    const double loss = std::pow(10.0, GetImplementationLossDb(mode) / 10.0);
    return sinr * bandwidthOverBitRate / loss;
}

double
BleErrorModel::GetBitErrorRate(double sinr, BlePhyMode mode) const
{
    const double ebNo = GetEbNo(sinr, mode);
    if (ebNo <= 0.0)
    {
        return 0.5;
    }
    // non-coherent binary frequency shift keying
    const double ber = 0.5 * std::exp(-0.5 * ebNo);
    return std::clamp(ber, 0.0, 0.5);
}

double
BleErrorModel::GetChunkSuccessRate(double sinr, BlePhyMode mode, uint64_t nbits) const
{
    if (nbits == 0)
    {
        return 1.0;
    }
    const double ber = GetBitErrorRate(sinr, mode);
    if (ber <= 0.0)
    {
        return 1.0;
    }
    if (ber >= 0.5)
    {
        return 0.0;
    }
    // computed in the logarithmic domain so that long packets do not underflow
    const double logSuccess = static_cast<double>(nbits) * std::log1p(-ber);
    return std::exp(logSuccess);
}

} // namespace ble
} // namespace ns3
