/*
 * Copyright (c) 2026
 *
 * SPDX-License-Identifier: GPL-2.0-only
 */

#include "ble-spectrum-value-helper.h"

#include "ble-utils.h"

#include "ns3/abort.h"
#include "ns3/log.h"

#include <cmath>

namespace ns3
{

NS_LOG_COMPONENT_DEFINE("BleSpectrumValueHelper");

namespace ble
{

namespace
{

/// Lowest centre frequency of the spectrum model, in Hz.
constexpr double FIRST_BIN_CENTRE_HZ = 2400.0e6;
/// Width of a bin of the spectrum model, in Hz.
constexpr double BIN_WIDTH_HZ = 1.0e6;
/// Number of bins, covering 2400 MHz to 2484 MHz.
constexpr uint32_t N_BINS = 85;
/// Boltzmann constant multiplied by the reference temperature of 290 K, in W/Hz.
constexpr double KT_W_PER_HZ = 1.38064852e-23 * 290.0;

/**
 * @param channel the logical channel index
 * @return the index of the bin holding the centre frequency of the channel
 */
uint32_t
ChannelToCentreBin(uint8_t channel)
{
    const double frequencyMhz = ChannelToFrequencyMhz(channel);
    const auto bin = static_cast<uint32_t>(std::lround(frequencyMhz - 2400.0));
    NS_ABORT_MSG_IF(bin >= N_BINS, "Channel " << +channel << " falls outside the spectrum model");
    return bin;
}

/**
 * The fraction of the transmitted power that falls in each 1 MHz bin around the centre
 * frequency. The first entry is the centre bin and the following entries are the bins on each
 * side. The shape approximates the GFSK spectrum of the LE PHYs and integrates to one.
 *
 * @param mode the PHY mode
 * @return the fractions, from the centre outwards
 */
const std::vector<double>&
GetPowerShape(BlePhyMode mode)
{
    // One symbol per microsecond: the main lobe fits in a single 1 MHz bin. The fraction two
    // bins away sets the adjacent channel leakage, which the transmitter spectrum mask of the
    // specification caps at -20 dBc; real radios are well inside it, so 0.002 is used.
    static const std::vector<double> narrow{0.900, 0.048, 0.002};
    // Two symbols per microsecond: the main lobe is twice as wide, so more energy legitimately
    // falls in the neighbouring bins.
    static const std::vector<double> wide{0.620, 0.180, 0.010};
    return (mode == BlePhyMode::LE_2M) ? wide : narrow;
}

/// Global spectrum model, built once and shared by every BLE PHY.
Ptr<SpectrumModel> g_bleSpectrumModel;

} // namespace

Ptr<const SpectrumModel>
BleSpectrumValueHelper::GetSpectrumModel()
{
    if (!g_bleSpectrumModel)
    {
        Bands bands;
        for (uint32_t i = 0; i < N_BINS; ++i)
        {
            BandInfo band;
            band.fc = FIRST_BIN_CENTRE_HZ + i * BIN_WIDTH_HZ;
            band.fl = band.fc - BIN_WIDTH_HZ / 2;
            band.fh = band.fc + BIN_WIDTH_HZ / 2;
            bands.push_back(band);
        }
        g_bleSpectrumModel = Create<SpectrumModel>(bands);
    }
    return g_bleSpectrumModel;
}

Ptr<SpectrumValue>
BleSpectrumValueHelper::CreateTxPowerSpectralDensity(double txPowerDbm,
                                                     uint8_t channel,
                                                     BlePhyMode mode)
{
    auto psd = Create<SpectrumValue>(GetSpectrumModel());
    const double txPowerW = std::pow(10.0, (txPowerDbm - 30.0) / 10.0);
    const uint32_t centre = ChannelToCentreBin(channel);
    const auto& shape = GetPowerShape(mode);
    for (std::size_t offset = 0; offset < shape.size(); ++offset)
    {
        const double fraction = shape[offset];
        if (offset == 0)
        {
            (*psd)[centre] = txPowerW * fraction / BIN_WIDTH_HZ;
            continue;
        }
        if (centre >= offset)
        {
            (*psd)[centre - offset] = txPowerW * fraction / BIN_WIDTH_HZ;
        }
        if (centre + offset < N_BINS)
        {
            (*psd)[centre + offset] = txPowerW * fraction / BIN_WIDTH_HZ;
        }
    }
    return psd;
}

Ptr<SpectrumValue>
BleSpectrumValueHelper::CreateNoisePowerSpectralDensity(double noiseFigureDb)
{
    auto psd = Create<SpectrumValue>(GetSpectrumModel());
    const double noiseFigure = std::pow(10.0, noiseFigureDb / 10.0);
    const double noisePsd = KT_W_PER_HZ * noiseFigure;
    for (uint32_t i = 0; i < N_BINS; ++i)
    {
        (*psd)[i] = noisePsd;
    }
    return psd;
}

Ptr<SpectrumValue>
BleSpectrumValueHelper::CreateRfFilter(uint8_t channel, BlePhyMode mode)
{
    auto filter = Create<SpectrumValue>(GetSpectrumModel());
    const uint32_t centre = ChannelToCentreBin(channel);
    // the receiver bandwidth is one symbol rate wide, so one bin for 1 Msym/s and the centre
    // bin plus its two neighbours, halved at the edges, for 2 Msym/s
    (*filter)[centre] = 1.0;
    if (mode == BlePhyMode::LE_2M)
    {
        if (centre >= 1)
        {
            (*filter)[centre - 1] = 0.5;
        }
        if (centre + 1 < N_BINS)
        {
            (*filter)[centre + 1] = 0.5;
        }
    }
    return filter;
}

double
BleSpectrumValueHelper::GetBandPower(Ptr<const SpectrumValue> psd, uint8_t channel, BlePhyMode mode)
{
    if (!psd)
    {
        return 0.0;
    }
    auto filter = CreateRfFilter(channel, mode);
    double power = 0.0;
    auto psdIt = psd->ConstValuesBegin();
    auto filterIt = filter->ConstValuesBegin();
    auto bandIt = psd->ConstBandsBegin();
    while (psdIt != psd->ConstValuesEnd() && filterIt != filter->ConstValuesEnd())
    {
        power += (*psdIt) * (*filterIt) * (bandIt->fh - bandIt->fl);
        ++psdIt;
        ++filterIt;
        ++bandIt;
    }
    return power;
}

double
BleSpectrumValueHelper::GetNoiseBandwidthHz(BlePhyMode mode)
{
    return GetSymbolRate(mode);
}

} // namespace ble
} // namespace ns3
