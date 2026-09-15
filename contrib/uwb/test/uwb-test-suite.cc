/*
 * Copyright (c) 2026
 *
 * SPDX-License-Identifier: GPL-2.0-only
 */

#include "ns3/double.h"
#include "ns3/mobility-module.h"
#include "ns3/propagation-module.h"
#include "ns3/simulator.h"
#include "ns3/spectrum-module.h"
#include "ns3/test.h"
#include "ns3/uwb-phy.h"
#include "ns3/uwb-clock-model.h"
#include "ns3/uwb-error-model.h"
#include "ns3/uwb-spectrum-value-helper.h"
#include "ns3/uwb-utils.h"

#include <cmath>
#include <set>
#include <vector>

using namespace ns3;
using namespace ns3::uwb;

/**
 * @ingroup uwb
 * @ingroup tests
 * @brief Check the channel plan and the timing of the HRP UWB physical layer.
 */
class UwbTimingTestCase : public TestCase
{
  public:
    UwbTimingTestCase()
        : TestCase("Channel plan and frame timing")
    {
    }

  private:
    void DoRun() override
    {
        // every centre frequency is a multiple of the 499.2 MHz chip rate, which is how the
        // channel raster of the standard is defined
        for (uint8_t channel = FIRST_CHANNEL; channel <= LAST_CHANNEL; ++channel)
        {
            const double frequency = ChannelToFrequencyMhz(channel);
            const double multiple = frequency / 499.2;
            NS_TEST_EXPECT_MSG_EQ_TOL(multiple,
                                      std::round(multiple),
                                      1e-9,
                                      "Channel " << +channel
                                                 << " must sit on the 499.2 MHz raster");
        }
        // the two channels every product uses
        NS_TEST_EXPECT_MSG_EQ_TOL(ChannelToFrequencyMhz(5), 6489.6, 1e-9, "Channel 5");
        NS_TEST_EXPECT_MSG_EQ_TOL(ChannelToFrequencyMhz(9), 7987.2, 1e-9, "Channel 9");
        NS_TEST_EXPECT_MSG_EQ_TOL(ChannelToBandwidthMhz(5), 499.2, 1e-9, "Channel 5 bandwidth");
        NS_TEST_EXPECT_MSG_EQ(IsMandatoryChannel(9), true, "Channel 9 is mandatory");

        // a preamble symbol is the code length times its spreading, in chips
        NS_TEST_EXPECT_MSG_EQ_TOL(GetPreambleSymbolDurationS(UwbPrf::PRF_16M) * 1e9,
                                  993.59,
                                  0.01,
                                  "Preamble symbol at a 15.6 MHz pulse repetition frequency");
        NS_TEST_EXPECT_MSG_EQ_TOL(GetPreambleSymbolDurationS(UwbPrf::PRF_64M) * 1e9,
                                  1017.63,
                                  0.01,
                                  "Preamble symbol at a 62.4 MHz pulse repetition frequency");

        // the nominal data rates come out of the symbol durations once the Reed-Solomon code is
        // taken into account, since a symbol carries one coded bit and the code passes 55 of
        // every 63
        NS_TEST_EXPECT_MSG_EQ_TOL(GetBitRate(UwbDataRate::RATE_850K) / 1e3,
                                  850.0,
                                  2.0,
                                  "Nominal rate of the 0.85 Mb/s mode");
        NS_TEST_EXPECT_MSG_EQ_TOL(GetBitRate(UwbDataRate::RATE_6M81) / 1e6,
                                  6.81,
                                  0.01,
                                  "Nominal rate of the 6.81 Mb/s mode");
        NS_TEST_EXPECT_MSG_EQ_TOL(GetBitRate(UwbDataRate::RATE_27M24) / 1e6,
                                  27.24,
                                  0.01,
                                  "Nominal rate of the 27.24 Mb/s mode");

        // the Reed-Solomon code adds 48 parity bits to every block of 330 information bits
        NS_TEST_EXPECT_MSG_EQ(GetReedSolomonCodedBits(0), 0u, "An empty payload codes to nothing");
        NS_TEST_EXPECT_MSG_EQ(GetReedSolomonCodedBits(1), 8u + 48u, "One octet needs one block");
        NS_TEST_EXPECT_MSG_EQ(GetReedSolomonCodedBits(127),
                              1016u + 4u * 48u,
                              "A 127 octet payload needs four blocks");

        // a whole frame at the settings most products use
        UwbPhyConfig config;
        config.channel = 5;
        config.dataRate = UwbDataRate::RATE_6M81;
        config.prf = UwbPrf::PRF_64M;
        config.preambleSymbols = 128;
        NS_TEST_EXPECT_MSG_EQ(config.GetSfdSymbols(), SFD_SYMBOLS_SHORT, "Short delimiter");
        const double shrUs = GetShrDuration(config).GetNanoSeconds() / 1000.0;
        const double phrUs = GetPhrDuration(config).GetNanoSeconds() / 1000.0;
        const double psduUs = GetPsduDuration(config, 127).GetNanoSeconds() / 1000.0;
        NS_TEST_EXPECT_MSG_EQ_TOL(shrUs, 138.40, 0.05, "Synchronisation header");
        NS_TEST_EXPECT_MSG_EQ_TOL(phrUs, 19.49, 0.05, "Physical layer header");
        NS_TEST_EXPECT_MSG_EQ_TOL(psduUs, 154.88, 0.05, "Payload of 127 octets");
        NS_TEST_EXPECT_MSG_EQ_TOL(GetFrameDuration(config, 127).GetNanoSeconds() / 1000.0,
                                  shrUs + phrUs + psduUs,
                                  0.01,
                                  "A frame is its three parts");

        // the long range mode uses the long delimiter and is far slower
        UwbPhyConfig slow = config;
        slow.dataRate = UwbDataRate::RATE_110K;
        slow.preambleSymbols = 1024;
        NS_TEST_EXPECT_MSG_EQ(slow.GetSfdSymbols(), SFD_SYMBOLS_LONG, "Long delimiter");
        NS_TEST_EXPECT_MSG_GT(GetFrameDuration(slow, 127).GetSeconds(),
                              10 * GetFrameDuration(config, 127).GetSeconds(),
                              "The long range mode takes far longer to send the same payload");

        // the processing gain is what lets a receiver work below the noise floor of its channel
        NS_TEST_EXPECT_MSG_EQ_TOL(GetProcessingGainDb(config),
                                  18.65,
                                  0.05,
                                  "Processing gain of the 6.81 Mb/s mode on a 499.2 MHz channel");
        NS_TEST_EXPECT_MSG_GT(GetProcessingGainDb(slow),
                              35.0,
                              "The long range mode has far more processing gain");

        // the regulatory density limit caps the power a whole channel may carry
        NS_TEST_EXPECT_MSG_EQ_TOL(GetRegulatoryTxPowerDbm(5),
                                  -14.32,
                                  0.05,
                                  "Total power allowed over a 499.2 MHz channel");
    }
};

/**
 * @ingroup uwb
 * @ingroup tests
 * @brief Check the timestamp resolution, the frame check sequence and trilateration.
 */
class UwbUtilityTestCase : public TestCase
{
  public:
    UwbUtilityTestCase()
        : TestCase("Timestamps, frame check sequence and trilateration")
    {
    }

  private:
    void DoRun() override
    {
        NS_TEST_ASSERT_MSG_EQ(EnableUwbTimeResolution(),
                              true,
                              "The module runs the simulator clock at femtoseconds");

        // the timestamp resolution of a real radio is a chip divided by 128, a little under
        // five millimetres of propagation
        NS_TEST_EXPECT_MSG_EQ_TOL(TIMESTAMP_RESOLUTION_S * 1e12,
                                  15.65,
                                  0.01,
                                  "Timestamp resolution in picoseconds");
        NS_TEST_EXPECT_MSG_EQ_TOL(TIMESTAMP_RESOLUTION_S * SPEED_OF_LIGHT * 1000.0,
                                  4.69,
                                  0.01,
                                  "Timestamp resolution in millimetres");

        // distance and propagation delay are the same quantity in different units
        for (const double distance : {0.1, 1.0, 10.0, 100.0})
        {
            const Time flight = DistanceToTimeOfFlight(distance);
            // a femtosecond of clock resolution is 0.3 micrometres of range, so the round
            // trip is exact to well inside a micrometre
            NS_TEST_EXPECT_MSG_EQ_TOL(TimeOfFlightToDistance(flight),
                                      distance,
                                      1e-6,
                                      "A distance survives a round trip through its delay");
        }
        // light travels almost exactly thirty centimetres in a nanosecond
        NS_TEST_EXPECT_MSG_EQ_TOL(TimeOfFlightToDistance(NanoSeconds(1)),
                                  0.2998,
                                  0.001,
                                  "One nanosecond of flight");

        // quantising never moves a time by more than half a unit
        for (const double nanoseconds : {0.0, 1.0, 3.7, 1234.5})
        {
            const Time exact = Seconds(nanoseconds * 1e-9);
            const Time rounded = QuantiseToTimestamp(exact);
            NS_TEST_EXPECT_MSG_LT_OR_EQ(std::abs((rounded - exact).GetSeconds()),
                                        TIMESTAMP_RESOLUTION_S / 2 + 1e-15,
                                        "Quantising moves a time by at most half a unit");
        }

        // the frame check sequence changes with every single bit flip
        std::vector<uint8_t> frame{0x41, 0x88, 0x01, 0xCD, 0xAB, 0x34, 0x12, 0x78, 0x56};
        const uint16_t fcs = Fcs16(frame);
        for (std::size_t octet = 0; octet < frame.size(); ++octet)
        {
            for (uint8_t bit = 0; bit < 8; ++bit)
            {
                auto flipped = frame;
                flipped[octet] ^= static_cast<uint8_t>(1U << bit);
                NS_TEST_EXPECT_MSG_NE(Fcs16(flipped), fcs, "A bit flip must change the sequence");
            }
        }
        // it is linear over GF(2), as every cyclic redundancy check with a zero register is
        std::vector<uint8_t> other{0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99};
        std::vector<uint8_t> combined(frame.size());
        for (std::size_t i = 0; i < frame.size(); ++i)
        {
            combined[i] = frame[i] ^ other[i];
        }
        NS_TEST_EXPECT_MSG_EQ(Fcs16(combined),
                              static_cast<uint16_t>(Fcs16(frame) ^ Fcs16(other)),
                              "The frame check sequence is linear");

        // trilateration recovers a position from exact ranges
        const std::vector<Vector> anchors{Vector(0, 0, 0),
                                          Vector(20, 0, 0),
                                          Vector(20, 20, 0),
                                          Vector(0, 20, 0)};
        const Vector truth(7.0, 13.0, 0.0);
        std::vector<double> ranges;
        for (const auto& anchor : anchors)
        {
            ranges.push_back(std::sqrt(std::pow(anchor.x - truth.x, 2) +
                                       std::pow(anchor.y - truth.y, 2) +
                                       std::pow(anchor.z - truth.z, 2)));
        }
        Vector estimate;
        NS_TEST_EXPECT_MSG_EQ(Trilaterate(anchors, ranges, false, estimate),
                              true,
                              "Four anchors in a plane must give a position");
        NS_TEST_EXPECT_MSG_EQ_TOL(estimate.x, truth.x, 1e-6, "Trilaterated x");
        NS_TEST_EXPECT_MSG_EQ_TOL(estimate.y, truth.y, 1e-6, "Trilaterated y");

        // it degrades gracefully rather than exploding when the ranges carry error
        std::vector<double> noisy = ranges;
        noisy[0] += 0.10;
        noisy[1] -= 0.10;
        noisy[2] += 0.05;
        noisy[3] -= 0.05;
        NS_TEST_EXPECT_MSG_EQ(Trilaterate(anchors, noisy, false, estimate), true, "Noisy ranges");
        const double error = std::sqrt(std::pow(estimate.x - truth.x, 2) +
                                       std::pow(estimate.y - truth.y, 2));
        NS_TEST_EXPECT_MSG_LT(error, 0.30, "Ten centimetre range errors stay a position error");

        // collinear anchors cannot fix a position in a plane
        const std::vector<Vector> collinear{Vector(0, 0, 0), Vector(10, 0, 0), Vector(20, 0, 0)};
        const std::vector<double> collinearRanges{10.0, 5.0, 10.0};
        NS_TEST_EXPECT_MSG_EQ(Trilaterate(collinear, collinearRanges, false, estimate),
                              false,
                              "Collinear anchors must be refused");
    }
};

/**
 * @ingroup uwb
 * @ingroup tests
 * @brief Check the spectrum model and the power spectral densities built on it.
 */
class UwbSpectrumTestCase : public TestCase
{
  public:
    UwbSpectrumTestCase()
        : TestCase("Spectrum model, transmit mask and regulatory limit")
    {
    }

  private:
    /// @param psd a power spectral density
    /// @return the power it carries over the whole model, in W
    static double IntegratePsd(Ptr<const SpectrumValue> psd)
    {
        double total = 0.0;
        auto value = psd->ConstValuesBegin();
        auto band = psd->ConstBandsBegin();
        while (value != psd->ConstValuesEnd())
        {
            total += (*value) * (band->fh - band->fl);
            ++value;
            ++band;
        }
        return total;
    }

    void DoRun() override
    {
        // the grid exists so that no channel edge has to be approximated: a bin is a fifth of
        // the nominal channel, and every centre frequency is a multiple of the channel width
        const double binWidth = UwbSpectrumValueHelper::GetBinWidthHz();
        NS_TEST_EXPECT_MSG_EQ_TOL(binWidth * 5.0,
                                  NOMINAL_BANDWIDTH_MHZ * 1e6,
                                  1.0,
                                  "Five bins make one nominal channel");

        auto model = UwbSpectrumValueHelper::GetSpectrumModel();
        NS_TEST_EXPECT_MSG_EQ(model->GetNumBands(),
                              UwbSpectrumValueHelper::GetNBins(),
                              "The spectrum model has the advertised number of bins");

        for (uint8_t channel = FIRST_CHANNEL; channel <= LAST_CHANNEL; ++channel)
        {
            const double centreHz = ChannelToFrequencyMhz(channel) * 1e6;
            // the centre of every channel falls on the centre of a bin
            const double bins = (centreHz - model->Begin()->fc) / binWidth;
            NS_TEST_EXPECT_MSG_EQ_TOL(bins,
                                      std::round(bins),
                                      1e-6,
                                      "Channel " << +channel << " lands on the grid");

            const double txPowerDbm = GetRegulatoryTxPowerDbm(channel);
            auto psd = UwbSpectrumValueHelper::CreateTxPowerSpectralDensity(txPowerDbm, channel);

            // whatever shape the mask has, it must not invent power
            const double totalDbm = 10.0 * std::log10(IntegratePsd(psd) * 1000.0);
            NS_TEST_EXPECT_MSG_EQ_TOL(totalDbm,
                                      txPowerDbm,
                                      0.01,
                                      "Channel " << +channel << " radiates what it was given");

            // nearly all of it lands inside the channel, the rest in the skirts
            const double inBandDbm =
                10.0 * std::log10(UwbSpectrumValueHelper::GetBandPower(psd, channel) * 1000.0);
            NS_TEST_EXPECT_MSG_LT(inBandDbm, totalDbm + 1e-6, "The band holds no more than all");
            NS_TEST_EXPECT_MSG_GT(inBandDbm,
                                  totalDbm - 0.1,
                                  "Channel " << +channel << " keeps its power in its band");

            // the density is the figure regulators cap, and it is the same on every channel
            NS_TEST_EXPECT_MSG_EQ_TOL(
                UwbSpectrumValueHelper::GetPowerDensityDbmPerMhz(txPowerDbm, channel),
                REGULATORY_EIRP_DBM_PER_MHZ,
                0.01,
                "Channel " << +channel << " sits on the regulatory density");
        }

        // a signal on one channel stays out of a channel far away from it
        auto psd5 = UwbSpectrumValueHelper::CreateTxPowerSpectralDensity(0.0, 5);
        const double leakedIntoNine = UwbSpectrumValueHelper::GetBandPower(psd5, 9);
        NS_TEST_EXPECT_MSG_LT(leakedIntoNine,
                              1e-9,
                              "A transmission on channel 5 does not reach channel 9");

        // the noise floor of a 499.2 MHz receiver is what the thermal figure says it is
        auto noise = UwbSpectrumValueHelper::CreateNoisePowerSpectralDensity(6.0);
        const double noiseDbm =
            10.0 * std::log10(UwbSpectrumValueHelper::GetBandPower(noise, 5) * 1000.0);
        NS_TEST_EXPECT_MSG_EQ_TOL(noiseDbm,
                                  UwbErrorModel::GetThermalNoiseDbm(499.2e6, 6.0),
                                  0.01,
                                  "The noise in the channel matches the thermal figure");
        NS_TEST_EXPECT_MSG_EQ_TOL(noiseDbm, -81.0, 0.1, "A UWB receiver takes in -81 dBm of noise");
    }
};

/**
 * @ingroup uwb
 * @ingroup tests
 * @brief Check the error model against the published receiver sensitivities.
 */
class UwbErrorModelTestCase : public TestCase
{
  public:
    UwbErrorModelTestCase()
        : TestCase("Sensitivity, processing gain and acquisition")
    {
    }

  private:
    void DoRun() override
    {
        auto errorModel = CreateObject<UwbErrorModel>();
        UwbPhyConfig config;
        config.channel = 5;
        config.prf = UwbPrf::PRF_64M;
        config.preambleSymbols = 128;

        // the two anchors are the point of the calibration, so they have to land exactly
        config.dataRate = UwbDataRate::RATE_110K;
        config.preambleSymbols = 1024;
        NS_TEST_EXPECT_MSG_EQ_TOL(errorModel->GetSensitivityDbm(config),
                                  PUBLISHED_SENSITIVITY_110K_DBM,
                                  0.01,
                                  "The long range mode reproduces its published sensitivity");

        config.dataRate = UwbDataRate::RATE_6M81;
        config.preambleSymbols = 128;
        NS_TEST_EXPECT_MSG_EQ_TOL(errorModel->GetSensitivityDbm(config),
                                  PUBLISHED_SENSITIVITY_6M81_DBM,
                                  0.01,
                                  "The common mode reproduces its published sensitivity");

        // the interpolated rates fall between them, in order, with no crossing
        double previous = -1000.0;
        for (const auto rate : {UwbDataRate::RATE_110K,
                                UwbDataRate::RATE_850K,
                                UwbDataRate::RATE_6M81,
                                UwbDataRate::RATE_27M24})
        {
            config.dataRate = rate;
            const double sensitivity = errorModel->GetSensitivityDbm(config);
            NS_TEST_EXPECT_MSG_GT(sensitivity,
                                  previous,
                                  "A faster mode is never more sensitive than a slower one");
            previous = sensitivity;
        }

        config.dataRate = UwbDataRate::RATE_850K;
        NS_TEST_EXPECT_MSG_EQ_TOL(errorModel->GetSensitivityDbm(config),
                                  -99.0,
                                  0.1,
                                  "0.85 Mb/s lands where the interpolation puts it");
        config.dataRate = UwbDataRate::RATE_27M24;
        NS_TEST_EXPECT_MSG_EQ_TOL(errorModel->GetSensitivityDbm(config),
                                  -89.0,
                                  0.1,
                                  "27.24 Mb/s lands where the interpolation puts it");

        // processing gain is the whole reason a signal under the noise floor is readable
        config.dataRate = UwbDataRate::RATE_6M81;
        NS_TEST_EXPECT_MSG_EQ_TOL(GetProcessingGainDb(config),
                                  18.65,
                                  0.01,
                                  "The common mode spreads by 18.65 dB");

        // at its sensitivity the signal is far below the noise in the channel
        const double noiseDbm = UwbErrorModel::GetThermalNoiseDbm(499.2e6, 6.0);
        const double sinrDb = errorModel->GetSensitivityDbm(config) - noiseDbm;
        NS_TEST_EXPECT_MSG_LT(sinrDb, -10.0, "A UWB receiver works well below the noise floor");

        // the bit error rate has to fall as the signal improves, and the frame with it
        double previousBer = 1.0;
        for (const double sinrDbStep : {-20.0, -15.0, -12.0, -9.0, -6.0})
        {
            const double sinr = std::pow(10.0, sinrDbStep / 10.0);
            const double ber = errorModel->GetBitErrorRate(sinr, config);
            NS_TEST_EXPECT_MSG_LT(ber, previousBer, "More signal is never more errors");
            previousBer = ber;
        }
        NS_TEST_EXPECT_MSG_EQ_TOL(errorModel->GetChunkSuccessRate(0.0, config, 1016),
                                  0.0,
                                  1e-9,
                                  "No signal decodes nothing");
        NS_TEST_EXPECT_MSG_EQ(errorModel->GetChunkSuccessRate(1e6, config, 1016) > 0.999,
                              true,
                              "An enormous signal decodes everything");

        // the sensitivity is defined at one per cent, so the frame has to fail that often
        const double sensitivityW =
            std::pow(10.0, (errorModel->GetSensitivityDbm(config) - 30.0) / 10.0);
        const double noiseW = std::pow(10.0, (noiseDbm - 30.0) / 10.0);
        NS_TEST_EXPECT_MSG_EQ_TOL(
            errorModel->GetChunkSuccessRate(sensitivityW / noiseW, config, 127 * 8),
            1.0 - SENSITIVITY_PER,
            0.002,
            "The reference frame fails one time in a hundred at the sensitivity");

        // a longer preamble buys acquisition margin and nothing else
        config.preambleSymbols = 128;
        const double shortGain = errorModel->GetAcquisitionGainDb(config);
        config.preambleSymbols = 1024;
        const double longGain = errorModel->GetAcquisitionGainDb(config);
        NS_TEST_EXPECT_MSG_EQ_TOL(longGain - shortGain,
                                  10.0 * std::log10(8.0),
                                  0.01,
                                  "Eight times the preamble is nine dB of correlator gain");
        NS_TEST_EXPECT_MSG_LT(errorModel->GetAcquisitionThresholdDbm(config),
                              errorModel->GetSensitivityDbm(config),
                              "At a long preamble the payload, not acquisition, sets the range");

        // the long range mode is where the preamble length earns its air time. Its payload
        // reaches twelve dB further than the fast mode does, so a preamble short enough to be
        // harmless at 6.81 Mb/s is what runs out first at 0.11 Mb/s
        config.dataRate = UwbDataRate::RATE_110K;
        config.prf = UwbPrf::PRF_16M;
        config.preambleSymbols = 1024;
        NS_TEST_EXPECT_MSG_LT(errorModel->GetAcquisitionThresholdDbm(config),
                              errorModel->GetSensitivityDbm(config),
                              "A thousand symbol preamble carries the long range mode");

        config.preambleSymbols = 16;
        NS_TEST_EXPECT_MSG_GT(errorModel->GetAcquisitionThresholdDbm(config),
                              errorModel->GetSensitivityDbm(config),
                              "A sixteen symbol preamble throws that range away");
    }
};

/**
 * @ingroup uwb
 * @ingroup tests
 * @brief Check the crystal: counter arithmetic, frequency offset and the counter wrap.
 */
class UwbClockTestCase : public TestCase
{
  public:
    UwbClockTestCase()
        : TestCase("Device clock, frequency offset and counter wrap")
    {
    }

  private:
    void DoRun() override
    {
        EnableUwbTimeResolution();

        // the counter runs at 128 times the chip rate, so one tick is the timestamp resolution
        NS_TEST_EXPECT_MSG_EQ_TOL(1.0 / DTU_RATE_HZ,
                                  TIMESTAMP_RESOLUTION_S,
                                  1e-18,
                                  "One tick is one timestamp unit");
        NS_TEST_EXPECT_MSG_EQ(UwbClockModel::TimeToTicks(Seconds(1)),
                              static_cast<uint64_t>(std::llround(DTU_RATE_HZ)),
                              "A second is 63.8976 billion ticks");

        // a duration survives a trip through the counter
        for (const auto& duration : {MicroSeconds(1), MicroSeconds(300), MilliSeconds(17)})
        {
            const uint64_t ticks = UwbClockModel::TimeToTicks(duration);
            const Time back = UwbClockModel::TicksToTime(ticks);
            NS_TEST_EXPECT_MSG_LT(std::abs((back - duration).GetSeconds()),
                                  TIMESTAMP_RESOLUTION_S,
                                  "A duration survives the counter to within one tick");
        }

        // a perfect crystal reads simulated time
        auto perfect = CreateObject<UwbClockModel>();
        NS_TEST_EXPECT_MSG_EQ(perfect->GetLocalTicks(Seconds(1)),
                              UwbClockModel::TimeToTicks(Seconds(1)) % DTU_COUNTER_MODULUS,
                              "A crystal with no error reads simulated time");

        // a crystal that is twenty parts per million fast gains twenty microseconds a second
        auto fast = CreateObject<UwbClockModel>();
        fast->SetFrequencyOffsetPpm(20.0);
        const Time gained = fast->GetLocalTime(Seconds(1)) - Seconds(1);
        NS_TEST_EXPECT_MSG_EQ_TOL(gained.GetSeconds(),
                                  20e-6,
                                  1e-12,
                                  "Twenty parts per million is twenty microseconds a second");
        NS_TEST_EXPECT_MSG_LT(std::abs((fast->LocalToGlobal(fast->GetLocalTime(Seconds(3))) -
                                        Seconds(3))
                                           .GetSeconds()),
                              1e-12,
                              "The local clock inverts back to simulated time");

        // this is the number that makes the ranging schemes differ: two crystals forty parts
        // per million apart disagree by twelve nanoseconds over a three hundred microsecond
        // turnaround, which is three and a half metres of apparent range
        auto slow = CreateObject<UwbClockModel>();
        slow->SetFrequencyOffsetPpm(-20.0);
        const Time reply = MicroSeconds(300);
        const Time disagreement = fast->GetLocalTime(reply) - slow->GetLocalTime(reply);
        NS_TEST_EXPECT_MSG_EQ_TOL(disagreement.GetSeconds() * 1e9,
                                  12.0,
                                  0.01,
                                  "Forty parts per million is twelve nanoseconds in a turnaround");
        NS_TEST_EXPECT_MSG_EQ_TOL(TimeOfFlightToDistance(disagreement) / 2.0,
                                  1.8,
                                  0.01,
                                  "Which a single sided exchange turns into 1.8 metres of error");

        // the counter is forty bits wide, so differences have to be taken modulo its width
        NS_TEST_EXPECT_MSG_EQ(UwbClockModel::TicksDifference(100, 40), 60u, "A plain difference");
        NS_TEST_EXPECT_MSG_EQ(UwbClockModel::TicksDifference(40, DTU_COUNTER_MODULUS - 60),
                              100u,
                              "A difference across the wrap of the counter");
        const Time wrapPeriod = UwbClockModel::TicksToTime(DTU_COUNTER_MODULUS);
        NS_TEST_EXPECT_MSG_EQ_TOL(wrapPeriod.GetSeconds(),
                                  17.2,
                                  0.05,
                                  "The counter wraps every 17.2 seconds");

        // a drifting crystal is slower than its nominal rate to begin with and faster later
        auto drifting = CreateObject<UwbClockModel>();
        drifting->SetFrequencyOffsetPpm(0.0);
        drifting->SetAttribute("Drift", DoubleValue(0.1));
        NS_TEST_EXPECT_MSG_EQ_TOL(drifting->GetInstantaneousOffsetPpm(Seconds(10)),
                                  1.0,
                                  1e-9,
                                  "A tenth of a part per million a second for ten seconds");
        NS_TEST_EXPECT_MSG_EQ_TOL((drifting->GetLocalTime(Seconds(10)) - Seconds(10)).GetSeconds(),
                                  10.0 * 0.5e-6,
                                  1e-12,
                                  "Over the interval the drift acts at half its final value");
    }
};

/**
 * @ingroup uwb
 * @ingroup tests
 * @brief Check a link end to end: the power that arrives, the range it reaches and the
 *        timestamps it produces.
 */
class UwbPhyLinkTestCase : public TestCase
{
  public:
    UwbPhyLinkTestCase()
        : TestCase("Link budget, range and arrival timestamps")
    {
    }

  private:
    /// One receiver outcome.
    struct Outcome
    {
        uint32_t ok{0};           //!< frames delivered
        uint32_t failed{0};       //!< frames that arrived but did not decode
        double lastRxPowerDbm{0}; //!< power of the last frame that arrived
        Time lastArrival{Time(0)};   //!< when the marker of the last good frame arrived
        uint64_t lastRxTimestamp{0}; //!< what the receiver made of that marker
    };

    Outcome m_outcome; //!< what the receiver saw

    /// @param packet the frame
    /// @param info what the receiver learned
    void RxOk(Ptr<Packet> packet, const UwbRxInfo& info)
    {
        ++m_outcome.ok;
        m_outcome.lastRxPowerDbm = info.rxPowerDbm;
        m_outcome.lastArrival = info.arrival;
        m_outcome.lastRxTimestamp = info.rxTimestamp;
    }

    /// @param packet the frame
    /// @param info what the receiver learned
    void RxError(Ptr<const Packet> packet, const UwbRxInfo& info)
    {
        ++m_outcome.failed;
        m_outcome.lastRxPowerDbm = info.rxPowerDbm;
    }

    /**
     * Build a pair of radios a fixed distance apart on a spectrum channel.
     *
     * @param distance the separation, in metres
     * @param config the configuration both radios use
     * @param sender filled in with the transmitting radio
     * @param receiver filled in with the receiving radio
     */
    void BuildLink(double distance,
                   const UwbPhyConfig& config,
                   Ptr<UwbPhy>& sender,
                   Ptr<UwbPhy>& receiver)
    {
        auto channel = CreateObject<MultiModelSpectrumChannel>();
        auto friis = CreateObject<FriisPropagationLossModel>();
        // ns-3 evaluates a propagation loss model at the frequency it is configured with, not
        // at the frequency of the signal, so it has to be told where the channel sits
        friis->SetFrequency(ChannelToFrequencyMhz(config.channel) * 1e6);
        channel->AddPropagationLossModel(friis);
        channel->SetPropagationDelayModel(CreateObject<ConstantSpeedPropagationDelayModel>());

        auto senderMobility = CreateObject<ConstantPositionMobilityModel>();
        auto receiverMobility = CreateObject<ConstantPositionMobilityModel>();
        senderMobility->SetPosition(Vector(0, 0, 0));
        receiverMobility->SetPosition(Vector(distance, 0, 0));

        sender = CreateObject<UwbPhy>();
        receiver = CreateObject<UwbPhy>();
        sender->SetConfig(config);
        receiver->SetConfig(config);
        sender->SetTxPowerToRegulatoryLimit();
        receiver->SetTxPowerToRegulatoryLimit();
        sender->SetChannel(channel);
        receiver->SetChannel(channel);
        sender->SetMobility(senderMobility);
        receiver->SetMobility(receiverMobility);
        channel->AddRx(sender);
        channel->AddRx(receiver);
        sender->AssignStreams(1);
        receiver->AssignStreams(11);
        receiver->SetReceiveOkCallback(MakeCallback(&UwbPhyLinkTestCase::RxOk, this));
        receiver->SetReceiveErrorCallback(MakeCallback(&UwbPhyLinkTestCase::RxError, this));
    }

    /**
     * Send a number of frames one after another and report what arrived.
     *
     * @param distance the separation, in metres
     * @param config the configuration both radios use
     * @param frames how many frames to send
     * @return what the receiver saw
     */
    Outcome RunLink(double distance, const UwbPhyConfig& config, uint32_t frames)
    {
        m_outcome = Outcome{};
        Ptr<UwbPhy> sender;
        Ptr<UwbPhy> receiver;
        BuildLink(distance, config, sender, receiver);

        const Time spacing = sender->CalculateTxDuration(MAX_PSDU_OCTETS) + MilliSeconds(1);
        for (uint32_t i = 0; i < frames; ++i)
        {
            Simulator::Schedule(spacing * (i + 1), [sender]() {
                sender->StartTx(Create<Packet>(MAX_PSDU_OCTETS), true);
            });
        }
        Simulator::Stop(spacing * (frames + 2));
        Simulator::Run();
        Simulator::Destroy();
        return m_outcome;
    }

    void DoRun() override
    {
        EnableUwbTimeResolution();

        UwbPhyConfig config;
        config.channel = 5;
        config.dataRate = UwbDataRate::RATE_6M81;
        config.prf = UwbPrf::PRF_64M;
        config.preambleSymbols = 128;

        // a link of ten metres is comfortable for the mode products use
        const double distance = 10.0;
        auto outcome = RunLink(distance, config, 20);
        NS_TEST_EXPECT_MSG_EQ(outcome.ok, 20u, "A ten metre link loses nothing");

        // the power that arrives is the free space budget, with nothing invented on the way
        const double fsplDb = 20.0 * std::log10(4.0 * M_PI * distance *
                                                ChannelToFrequencyMhz(config.channel) * 1e6 /
                                                SPEED_OF_LIGHT);
        NS_TEST_EXPECT_MSG_EQ_TOL(outcome.lastRxPowerDbm,
                                  GetRegulatoryTxPowerDbm(config.channel) - fsplDb,
                                  0.05,
                                  "The received power is the free space link budget");

        // a UWB transmitter is a very quiet one: the regulatory density leaves it two
        // hundredths of a milliwatt, which is why ten metres is a normal range
        NS_TEST_EXPECT_MSG_LT(GetRegulatoryTxPowerDbm(config.channel),
                              -14.0,
                              "The regulatory limit keeps a UWB transmitter under -14 dBm");

        // far beyond the sensitivity nothing arrives at all
        outcome = RunLink(400.0, config, 10);
        NS_TEST_EXPECT_MSG_EQ(outcome.ok, 0u, "Four hundred metres is out of reach");

        // the long range mode reaches much further, which is the whole point of it
        config.dataRate = UwbDataRate::RATE_110K;
        config.preambleSymbols = 1024;
        outcome = RunLink(100.0, config, 5);
        NS_TEST_EXPECT_MSG_EQ(outcome.ok, 5u, "A hundred metres is comfortable at 0.11 Mb/s");
    }
};

/**
 * @ingroup uwb
 * @ingroup tests
 * @brief Check what a picosecond timestamp is worth: the arrival of a marker, the counter
 *        reading it produces and the range that follows from it.
 */
class UwbTimestampTestCase : public TestCase
{
  public:
    UwbTimestampTestCase()
        : TestCase("Marker timestamps and the range they measure")
    {
    }

  private:
    std::vector<double> m_measured; //!< the range each frame measured, in metres
    uint64_t m_txTimestamp{0};      //!< the marker of the frame in flight

    /// @param packet the frame
    /// @param info what the receiver learned
    void RxOk(Ptr<Packet> packet, const UwbRxInfo& info)
    {
        const uint64_t elapsed = UwbClockModel::TicksDifference(info.rxTimestamp, m_txTimestamp);
        m_measured.push_back(TimeOfFlightToDistance(UwbClockModel::TicksToTime(elapsed)));
    }

    void DoRun() override
    {
        EnableUwbTimeResolution();

        UwbPhyConfig config;
        config.channel = 5;
        config.dataRate = UwbDataRate::RATE_6M81;
        config.prf = UwbPrf::PRF_64M;
        config.preambleSymbols = 128;

        const double distance = 10.0;
        auto channel = CreateObject<MultiModelSpectrumChannel>();
        auto friis = CreateObject<FriisPropagationLossModel>();
        friis->SetFrequency(ChannelToFrequencyMhz(config.channel) * 1e6);
        channel->AddPropagationLossModel(friis);
        channel->SetPropagationDelayModel(CreateObject<ConstantSpeedPropagationDelayModel>());

        auto senderMobility = CreateObject<ConstantPositionMobilityModel>();
        auto receiverMobility = CreateObject<ConstantPositionMobilityModel>();
        senderMobility->SetPosition(Vector(0, 0, 0));
        receiverMobility->SetPosition(Vector(distance, 0, 0));

        auto sender = CreateObject<UwbPhy>();
        auto receiver = CreateObject<UwbPhy>();
        for (auto& phy : {sender, receiver})
        {
            phy->SetConfig(config);
            phy->SetTxPowerToRegulatoryLimit();
            phy->SetChannel(channel);
        }
        sender->SetMobility(senderMobility);
        receiver->SetMobility(receiverMobility);
        channel->AddRx(sender);
        channel->AddRx(receiver);
        sender->AssignStreams(21);
        receiver->AssignStreams(31);
        receiver->SetReceiveOkCallback(MakeCallback(&UwbTimestampTestCase::RxOk, this));

        // both radios keep perfect time here, so what is left in the measurement is the error
        // the receiver makes estimating the leading edge of the signal
        const uint32_t frames = 200;
        const Time spacing = sender->CalculateTxDuration(MAX_PSDU_OCTETS) + MilliSeconds(1);
        for (uint32_t i = 0; i < frames; ++i)
        {
            Simulator::Schedule(spacing * (i + 1), [this, sender]() {
                const Time marker = Simulator::Now() + sender->GetShrDuration();
                m_txTimestamp = sender->GetTxTimestampFor(marker);
                sender->StartTx(Create<Packet>(MAX_PSDU_OCTETS), true);
            });
        }
        Simulator::Stop(spacing * (frames + 2));
        Simulator::Run();
        Simulator::Destroy();

        NS_TEST_ASSERT_MSG_EQ(m_measured.size(), frames, "Every frame was measured");

        double mean = 0.0;
        for (const double range : m_measured)
        {
            mean += range / frames;
        }
        double variance = 0.0;
        for (const double range : m_measured)
        {
            variance += (range - mean) * (range - mean) / frames;
        }
        const double sigma = std::sqrt(variance);

        // a one way measurement between two perfect clocks has no bias, only the leading edge
        // error, so the average has to sit on the truth
        NS_TEST_EXPECT_MSG_EQ_TOL(mean, distance, 0.02, "A one way measurement has no bias");

        // and the spread has to be the centimetres that UWB is known for, neither the
        // millimetres of the raw timestamp grid nor the metres of a narrowband radio
        NS_TEST_EXPECT_MSG_GT(sigma, 0.001, "The leading edge estimate is not exact");
        NS_TEST_EXPECT_MSG_LT(sigma, 0.10, "Ten metres is measured to a few centimetres");

        // no single measurement strays far
        for (const double range : m_measured)
        {
            NS_TEST_EXPECT_MSG_LT(std::abs(range - distance),
                                  0.50,
                                  "No measurement strays half a metre");
        }
    }
};

/**
 * @ingroup uwb
 * @ingroup tests
 * @brief Check how a UWB receiver holds up against another transmitter in the same channel.
 */
class UwbInterferenceTestCase : public TestCase
{
  public:
    UwbInterferenceTestCase()
        : TestCase("Processing gain and preamble code rejection under interference")
    {
    }

  private:
    uint32_t m_ok{0}; //!< frames delivered

    /// @param packet the frame
    /// @param info what the receiver learned
    void RxOk(Ptr<Packet> packet, const UwbRxInfo& info)
    {
        ++m_ok;
    }

    /**
     * Send frames from a wanted transmitter while a second one, much closer, talks over them.
     *
     * @param interfererCode the preamble code the interferer uses
     * @param frames how many frames to send
     * @return how many frames were delivered
     */
    uint32_t RunWithInterferer(uint8_t interfererCode, uint32_t frames)
    {
        m_ok = 0;
        UwbPhyConfig config;
        config.channel = 5;
        config.dataRate = UwbDataRate::RATE_6M81;
        config.prf = UwbPrf::PRF_64M;
        config.preambleSymbols = 128;

        auto channel = CreateObject<MultiModelSpectrumChannel>();
        auto friis = CreateObject<FriisPropagationLossModel>();
        friis->SetFrequency(ChannelToFrequencyMhz(config.channel) * 1e6);
        channel->AddPropagationLossModel(friis);
        channel->SetPropagationDelayModel(CreateObject<ConstantSpeedPropagationDelayModel>());

        // the wanted transmitter is twenty metres away and the interferer two, so it arrives
        // twenty dB stronger
        const std::vector<Vector> positions{Vector(20, 0, 0), Vector(2, 0, 0), Vector(0, 0, 0)};
        std::vector<Ptr<UwbPhy>> phys;
        for (const auto& position : positions)
        {
            auto mobility = CreateObject<ConstantPositionMobilityModel>();
            mobility->SetPosition(position);
            auto phy = CreateObject<UwbPhy>();
            phy->SetConfig(config);
            phy->SetTxPowerToRegulatoryLimit();
            phy->SetChannel(channel);
            phy->SetMobility(mobility);
            channel->AddRx(phy);
            phys.push_back(phy);
        }
        auto wanted = phys[0];
        auto interferer = phys[1];
        auto receiver = phys[2];
        interferer->SetPreambleCode(interfererCode);
        wanted->AssignStreams(41);
        interferer->AssignStreams(51);
        receiver->AssignStreams(61);
        receiver->SetReceiveOkCallback(MakeCallback(&UwbInterferenceTestCase::RxOk, this));

        const Time spacing = wanted->CalculateTxDuration(MAX_PSDU_OCTETS) + MilliSeconds(1);
        for (uint32_t i = 0; i < frames; ++i)
        {
            const Time at = spacing * (i + 1);
            Simulator::Schedule(at, [wanted]() {
                wanted->StartTx(Create<Packet>(MAX_PSDU_OCTETS), false);
            });
            // the interferer starts once the wanted preamble is safely acquired, so what is
            // being tested is the payload and not the lock
            Simulator::Schedule(at + wanted->GetShrDuration() + MicroSeconds(1), [interferer]() {
                interferer->StartTx(Create<Packet>(MAX_PSDU_OCTETS), false);
            });
        }
        Simulator::Stop(spacing * (frames + 2));
        Simulator::Run();
        Simulator::Destroy();
        return m_ok;
    }

    void DoRun() override
    {
        EnableUwbTimeResolution();

        // an interferer twenty dB stronger on the same preamble code buries the signal: even
        // the processing gain of a 499.2 MHz channel cannot recover that
        NS_TEST_EXPECT_MSG_EQ(RunWithInterferer(9, 20),
                              0u,
                              "A much stronger transmitter on the same code drowns the link");

        // the same interferer on a different code is suppressed by the correlator, and what is
        // left the processing gain absorbs. This is how a room full of UWB tags works at all
        NS_TEST_EXPECT_MSG_GT(RunWithInterferer(10, 20),
                              17u,
                              "A different preamble code lets the link survive it");
    }
};

/**
 * @ingroup uwb
 * @ingroup tests
 * @brief Test suite of the UWB model.
 */
class UwbTestSuite : public TestSuite
{
  public:
    UwbTestSuite()
        : TestSuite("uwb", Type::UNIT)
    {
        AddTestCase(new UwbTimingTestCase, Duration::QUICK);
        AddTestCase(new UwbUtilityTestCase, Duration::QUICK);
        AddTestCase(new UwbSpectrumTestCase, Duration::QUICK);
        AddTestCase(new UwbErrorModelTestCase, Duration::QUICK);
        AddTestCase(new UwbClockTestCase, Duration::QUICK);
        AddTestCase(new UwbPhyLinkTestCase, Duration::QUICK);
        AddTestCase(new UwbTimestampTestCase, Duration::QUICK);
        AddTestCase(new UwbInterferenceTestCase, Duration::QUICK);
    }
};

static UwbTestSuite g_uwbTestSuite; //!< the test suite
