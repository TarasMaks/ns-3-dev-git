/*
 * Copyright (c) 2026
 *
 * SPDX-License-Identifier: GPL-2.0-only
 */

#include "ns3/ble-error-model.h"
#include "ns3/ble-headers.h"
#include "ns3/ble-phy.h"
#include "ns3/ble-spectrum-value-helper.h"
#include "ns3/ble-utils.h"
#include "ns3/packet.h"
#include "ns3/test.h"

#include <cmath>
#include <set>
#include <vector>

using namespace ns3;
using namespace ns3::ble;

/**
 * @ingroup ble
 * @ingroup tests
 * @brief Check the CRC-24 and the data whitening against their mathematical invariants.
 */
class BleCrcWhiteningTestCase : public TestCase
{
  public:
    BleCrcWhiteningTestCase()
        : TestCase("CRC-24 and whitening invariants")
    {
    }

  private:
    void DoRun() override
    {
        // the CRC of an all-zero message with a zero register stays zero
        std::vector<uint8_t> zeros(32, 0);
        NS_TEST_EXPECT_MSG_EQ(Crc24(0, zeros), 0u, "CRC of zeros with a zero register");

        // a CRC is linear over GF(2) when the register starts at zero
        std::vector<uint8_t> a{0x12, 0x34, 0x56, 0x78, 0x9A, 0xBC};
        std::vector<uint8_t> b{0xDE, 0xAD, 0xBE, 0xEF, 0x01, 0x02};
        std::vector<uint8_t> x(a.size());
        for (std::size_t i = 0; i < a.size(); ++i)
        {
            x[i] = a[i] ^ b[i];
        }
        NS_TEST_EXPECT_MSG_EQ(Crc24(0, x),
                              Crc24(0, a) ^ Crc24(0, b),
                              "The CRC is linear when the register starts at zero");

        // a single bit flip anywhere changes the CRC
        for (std::size_t octet = 0; octet < a.size(); ++octet)
        {
            for (uint8_t bit = 0; bit < 8; ++bit)
            {
                auto flipped = a;
                flipped[octet] ^= static_cast<uint8_t>(1U << bit);
                NS_TEST_EXPECT_MSG_NE(Crc24(ADV_CRC_INIT, flipped),
                                      Crc24(ADV_CRC_INIT, a),
                                      "A single bit flip must change the CRC");
            }
        }

        // whitening is its own inverse and depends on the channel
        for (uint8_t channel = 0; channel < N_RF_CHANNELS; ++channel)
        {
            std::vector<uint8_t> data{0x00, 0xFF, 0x55, 0xAA, 0x0F, 0xF0, 0x12, 0x34};
            const auto original = data;
            Whiten(channel, data);
            NS_TEST_EXPECT_MSG_EQ(data == original, false, "Whitening must change the data");
            Whiten(channel, data);
            NS_TEST_EXPECT_MSG_EQ(data == original, true, "Whitening twice must restore the data");
        }
        std::vector<uint8_t> d1(16, 0);
        std::vector<uint8_t> d2(16, 0);
        Whiten(10, d1);
        Whiten(20, d2);
        NS_TEST_EXPECT_MSG_EQ(d1 == d2,
                              false,
                              "Different channels must give different whitening sequences");
    }
};

/**
 * @ingroup ble
 * @ingroup tests
 * @brief Check the channel maps and both channel selection algorithms.
 */
class BleChannelSelectionTestCase : public TestCase
{
  public:
    BleChannelSelectionTestCase()
        : TestCase("Channel map and channel selection algorithms #1 and #2")
    {
    }

  private:
    void DoRun() override
    {
        // a default map uses all 37 data channels and survives a serialization round trip
        BleChannelMap full;
        NS_TEST_EXPECT_MSG_EQ(+full.GetNUsedChannels(), +N_DATA_CHANNELS, "A default map is full");
        uint8_t octets[5];
        full.Serialize(octets);
        BleChannelMap restored(octets);
        NS_TEST_EXPECT_MSG_EQ(+restored.GetNUsedChannels(),
                              +N_DATA_CHANNELS,
                              "A full map survives a serialization round trip");

        BleChannelMap sparse;
        for (uint8_t channel = 0; channel < N_DATA_CHANNELS; ++channel)
        {
            sparse.SetUsed(channel, channel % 3 == 0);
        }
        const uint8_t expectedUsed = 13; // channels 0, 3, ..., 36
        NS_TEST_EXPECT_MSG_EQ(+sparse.GetNUsedChannels(), +expectedUsed, "Sparse map size");
        sparse.Serialize(octets);
        BleChannelMap sparseRestored(octets);
        NS_TEST_EXPECT_MSG_EQ(sparseRestored.GetUsedChannels() == sparse.GetUsedChannels(),
                              true,
                              "A sparse map survives a serialization round trip");

        // algorithm #1 visits every channel of a full map within 37 events, for every hop
        // increment, because 37 is prime and the increment is smaller than it
        for (uint8_t hop = HOP_INCREMENT_MIN; hop <= HOP_INCREMENT_MAX; ++hop)
        {
            BleChannelSelectionAlgorithm1 csa1(hop);
            std::set<uint8_t> visited;
            for (uint8_t event = 0; event < N_DATA_CHANNELS; ++event)
            {
                visited.insert(csa1.NextChannel(full));
            }
            NS_TEST_EXPECT_MSG_EQ(visited.size(),
                                  std::size_t{N_DATA_CHANNELS},
                                  "Algorithm #1 with hop " << +hop << " must visit every channel");
        }

        // with a sparse map every selected channel is a used channel
        for (uint8_t hop = HOP_INCREMENT_MIN; hop <= HOP_INCREMENT_MAX; ++hop)
        {
            BleChannelSelectionAlgorithm1 csa1(hop);
            for (uint16_t event = 0; event < 500; ++event)
            {
                const uint8_t channel = csa1.NextChannel(sparse);
                NS_TEST_EXPECT_MSG_EQ(sparse.IsUsed(channel),
                                      true,
                                      "Algorithm #1 must remap into the used channels");
            }
        }

        // algorithm #2 is deterministic, stays in range and uses the whole map
        BleChannelSelectionAlgorithm2 csa2(0x8E89BED6);
        std::set<uint8_t> visited;
        for (uint16_t event = 0; event < 2000; ++event)
        {
            const uint8_t channel = csa2.GetChannel(full, event);
            NS_TEST_EXPECT_MSG_LT(+channel, +N_DATA_CHANNELS, "Algorithm #2 must stay in range");
            NS_TEST_EXPECT_MSG_EQ(+channel,
                                  +csa2.GetChannel(full, event),
                                  "Algorithm #2 must be deterministic");
            visited.insert(channel);
        }
        NS_TEST_EXPECT_MSG_EQ(visited.size(),
                              std::size_t{N_DATA_CHANNELS},
                              "Algorithm #2 must use every channel of a full map");

        for (uint16_t event = 0; event < 2000; ++event)
        {
            NS_TEST_EXPECT_MSG_EQ(sparse.IsUsed(csa2.GetChannel(sparse, event)),
                                  true,
                                  "Algorithm #2 must remap into the used channels");
        }

        // the channel identifier is derived from the access address
        BleChannelSelectionAlgorithm2 other(0x12345678);
        NS_TEST_EXPECT_MSG_EQ(other.GetChannelIdentifier(),
                              static_cast<uint16_t>(0x1234 ^ 0x5678),
                              "The channel identifier is the exclusive-or of the address halves");
    }
};

/**
 * @ingroup ble
 * @ingroup tests
 * @brief Check the RF channel mapping and the access address constraints.
 */
class BleChannelMappingTestCase : public TestCase
{
  public:
    BleChannelMappingTestCase()
        : TestCase("RF channel mapping and access address constraints")
    {
    }

  private:
    void DoRun() override
    {
        // the three advertising channels sit at the documented frequencies
        NS_TEST_EXPECT_MSG_EQ(ChannelToFrequencyMhz(ADV_CHANNEL_37), 2402.0, "Channel 37");
        NS_TEST_EXPECT_MSG_EQ(ChannelToFrequencyMhz(ADV_CHANNEL_38), 2426.0, "Channel 38");
        NS_TEST_EXPECT_MSG_EQ(ChannelToFrequencyMhz(ADV_CHANNEL_39), 2480.0, "Channel 39");
        // the data channels fill the gaps between them
        NS_TEST_EXPECT_MSG_EQ(ChannelToFrequencyMhz(0), 2404.0, "Data channel 0");
        NS_TEST_EXPECT_MSG_EQ(ChannelToFrequencyMhz(10), 2424.0, "Data channel 10");
        NS_TEST_EXPECT_MSG_EQ(ChannelToFrequencyMhz(11), 2428.0, "Data channel 11");
        NS_TEST_EXPECT_MSG_EQ(ChannelToFrequencyMhz(36), 2478.0, "Data channel 36");

        // every logical channel maps to a distinct RF channel
        std::set<uint8_t> rfChannels;
        for (uint8_t channel = 0; channel < N_RF_CHANNELS; ++channel)
        {
            rfChannels.insert(ChannelToRfChannel(channel));
        }
        NS_TEST_EXPECT_MSG_EQ(rfChannels.size(),
                              std::size_t{N_RF_CHANNELS},
                              "The 40 channels must map to 40 distinct RF channels");

        // the advertising access address itself is not a valid connection access address
        NS_TEST_EXPECT_MSG_EQ(IsValidAccessAddress(ADV_ACCESS_ADDRESS),
                              false,
                              "The advertising access address is not a valid connection address");
        NS_TEST_EXPECT_MSG_EQ(IsValidAccessAddress(0x00000000),
                              false,
                              "An all-zero access address is invalid");
        NS_TEST_EXPECT_MSG_EQ(IsValidAccessAddress(0xAAAAAAAA),
                              false,
                              "An access address with four equal octets is invalid");

        // generated addresses satisfy every constraint
        auto random = CreateObject<UniformRandomVariable>();
        random->SetStream(1);
        for (uint32_t i = 0; i < 200; ++i)
        {
            const uint32_t accessAddress = GenerateAccessAddress(random);
            NS_TEST_EXPECT_MSG_EQ(IsValidAccessAddress(accessAddress),
                                  true,
                                  "A generated access address must be valid");
        }
    }
};

/**
 * @ingroup ble
 * @ingroup tests
 * @brief Check that every header survives a serialization round trip.
 */
class BleHeaderTestCase : public TestCase
{
  public:
    BleHeaderTestCase()
        : TestCase("Header serialization round trips")
    {
    }

  private:
    void DoRun() override
    {
        {
            BleAdvHeader in;
            in.SetPduType(BleAdvPduType::CONNECT_IND);
            in.SetChSel(true);
            in.SetTxAdd(true);
            in.SetRxAdd(false);
            in.SetLength(34);
            Packet packet;
            packet.AddHeader(in);
            BleAdvHeader out;
            packet.RemoveHeader(out);
            NS_TEST_EXPECT_MSG_EQ(static_cast<uint32_t>(out.GetPduType()),
                                  static_cast<uint32_t>(in.GetPduType()),
                                  "adv PDU type");
            NS_TEST_EXPECT_MSG_EQ(out.GetChSel(), true, "adv ChSel");
            NS_TEST_EXPECT_MSG_EQ(out.GetTxAdd(), true, "adv TxAdd");
            NS_TEST_EXPECT_MSG_EQ(out.GetRxAdd(), false, "adv RxAdd");
            NS_TEST_EXPECT_MSG_EQ(+out.GetLength(), 34, "adv length");
            NS_TEST_EXPECT_MSG_EQ(in.GetSerializedSize(), PDU_HEADER_OCTETS, "adv header size");
        }
        {
            BleDataHeader in;
            in.SetLlid(BleLlid::DATA_START);
            in.SetNesn(true);
            in.SetSn(false);
            in.SetMd(true);
            in.SetLength(251);
            Packet packet;
            packet.AddHeader(in);
            BleDataHeader out;
            packet.RemoveHeader(out);
            NS_TEST_EXPECT_MSG_EQ(static_cast<uint32_t>(out.GetLlid()),
                                  static_cast<uint32_t>(BleLlid::DATA_START),
                                  "data LLID");
            NS_TEST_EXPECT_MSG_EQ(out.GetNesn(), true, "data NESN");
            NS_TEST_EXPECT_MSG_EQ(out.GetSn(), false, "data SN");
            NS_TEST_EXPECT_MSG_EQ(out.GetMd(), true, "data MD");
            NS_TEST_EXPECT_MSG_EQ(+out.GetLength(), 251, "data length");
            NS_TEST_EXPECT_MSG_EQ(in.GetSerializedSize(), PDU_HEADER_OCTETS, "data header size");
        }
        {
            BleConnectIndHeader in;
            in.SetInitA(Mac48Address("00:11:22:33:44:55"));
            in.SetAdvA(Mac48Address("66:77:88:99:AA:BB"));
            in.SetAccessAddress(0x50655DAB);
            in.SetCrcInit(0x123456);
            in.SetWinSize(3);
            in.SetWinOffset(7);
            in.SetInterval(80);
            in.SetLatency(4);
            in.SetTimeout(500);
            in.SetHopIncrement(11);
            in.SetSca(5);
            BleChannelMap map;
            map.SetUsed(5, false);
            map.SetUsed(17, false);
            in.SetChannelMap(map);
            Packet packet;
            packet.AddHeader(in);
            BleConnectIndHeader out;
            packet.RemoveHeader(out);
            NS_TEST_EXPECT_MSG_EQ(out.GetInitA(), in.GetInitA(), "InitA");
            NS_TEST_EXPECT_MSG_EQ(out.GetAdvA(), in.GetAdvA(), "AdvA");
            NS_TEST_EXPECT_MSG_EQ(out.GetAccessAddress(), 0x50655DABu, "access address");
            NS_TEST_EXPECT_MSG_EQ(out.GetCrcInit(), 0x123456u, "CRC init");
            NS_TEST_EXPECT_MSG_EQ(+out.GetWinSize(), 3, "window size");
            NS_TEST_EXPECT_MSG_EQ(out.GetWinOffset(), 7, "window offset");
            NS_TEST_EXPECT_MSG_EQ(out.GetInterval(), 80, "interval");
            NS_TEST_EXPECT_MSG_EQ(out.GetLatency(), 4, "latency");
            NS_TEST_EXPECT_MSG_EQ(out.GetTimeout(), 500, "timeout");
            NS_TEST_EXPECT_MSG_EQ(+out.GetHopIncrement(), 11, "hop increment");
            NS_TEST_EXPECT_MSG_EQ(+out.GetSca(), 5, "sleep clock accuracy");
            NS_TEST_EXPECT_MSG_EQ(out.GetChannelMap().IsUsed(5), false, "channel 5 disabled");
            NS_TEST_EXPECT_MSG_EQ(out.GetChannelMap().IsUsed(17), false, "channel 17 disabled");
            NS_TEST_EXPECT_MSG_EQ(+out.GetChannelMap().GetNUsedChannels(), 35, "used channels");
            // the LLData field of a CONNECT_IND PDU is 22 octets after the two addresses
            NS_TEST_EXPECT_MSG_EQ(in.GetSerializedSize(), 34u, "CONNECT_IND payload size");
        }
        {
            BleTwoAddressHeader in;
            in.SetSourceAddress(Mac48Address("01:02:03:04:05:06"));
            in.SetAdvA(Mac48Address("0A:0B:0C:0D:0E:0F"));
            Packet packet;
            packet.AddHeader(in);
            BleTwoAddressHeader out;
            packet.RemoveHeader(out);
            NS_TEST_EXPECT_MSG_EQ(out.GetSourceAddress(), in.GetSourceAddress(), "source address");
            NS_TEST_EXPECT_MSG_EQ(out.GetAdvA(), in.GetAdvA(), "advertiser address");
            NS_TEST_EXPECT_MSG_EQ(in.GetSerializedSize(), 12u, "SCAN_REQ payload size");
        }
        {
            BleLlControlHeader in;
            in.SetOpcode(BleLlControlOpcode::LL_PHY_UPDATE_IND);
            in.SetParameters({0x04, 0x04});
            Packet packet;
            packet.AddHeader(in);
            BleLlControlHeader out;
            packet.RemoveHeader(out);
            NS_TEST_EXPECT_MSG_EQ(static_cast<uint32_t>(out.GetOpcode()),
                                  static_cast<uint32_t>(in.GetOpcode()),
                                  "control opcode");
            NS_TEST_EXPECT_MSG_EQ(out.GetParameters() == in.GetParameters(),
                                  true,
                                  "control parameters");
        }
        {
            Packet packet(20);
            BleCrcTrailer in;
            in.SetCrc(0xABCDEF);
            packet.AddTrailer(in);
            NS_TEST_EXPECT_MSG_EQ(packet.GetSize(), 23u, "a CRC adds three octets");
            BleCrcTrailer out;
            packet.RemoveTrailer(out);
            NS_TEST_EXPECT_MSG_EQ(out.GetCrc(), 0xABCDEFu, "CRC round trip");
            NS_TEST_EXPECT_MSG_EQ(packet.GetSize(), 20u, "removing the CRC restores the size");
        }
    }
};

/**
 * @ingroup ble
 * @ingroup tests
 * @brief Check the on-air packet durations against the values published for the four LE PHYs.
 */
class BlePacketDurationTestCase : public TestCase
{
  public:
    BlePacketDurationTestCase()
        : TestCase("On-air packet durations of the four LE PHYs")
    {
    }

  private:
    void DoRun() override
    {
        // The longest packet of each PHY carries a 255-octet PDU, that is a two-octet header and
        // a 251-octet payload extended by the Data Length Extension plus four octets of MIC.
        // The resulting durations are the ones quoted for the LE PHYs.
        const uint32_t maxPdu = PDU_HEADER_OCTETS + 255;

        struct Expectation
        {
            BlePhyMode mode;
            double maxDurationUs;
            double emptyPduUs;
        };

        // an empty PDU carries only the two-octet header
        const std::vector<Expectation> expectations{
            {BlePhyMode::LE_1M, 2120.0, 80.0},
            {BlePhyMode::LE_2M, 1064.0, 44.0},
            {BlePhyMode::LE_CODED_S2, 4542.0, 462.0},
            {BlePhyMode::LE_CODED_S8, 17040.0, 720.0},
        };
        for (const auto& expectation : expectations)
        {
            const double maxUs =
                BlePhy::CalculateTxDuration(maxPdu, expectation.mode).GetNanoSeconds() / 1000.0;
            NS_TEST_EXPECT_MSG_EQ_TOL(maxUs,
                                      expectation.maxDurationUs,
                                      0.01,
                                      "Longest packet of " << BlePhyModeName(expectation.mode));
            const double emptyUs =
                BlePhy::CalculateTxDuration(PDU_HEADER_OCTETS, expectation.mode).GetNanoSeconds() /
                1000.0;
            NS_TEST_EXPECT_MSG_EQ_TOL(emptyUs,
                                      expectation.emptyPduUs,
                                      0.01,
                                      "Empty packet of " << BlePhyModeName(expectation.mode));
        }

        // The uncoded PHYs send one bit per symbol, so doubling the symbol rate halves the
        // duration of everything but the preamble, which is twice as long on LE 2M.
        const double oneM = BlePhy::CalculateTxDuration(maxPdu, BlePhyMode::LE_1M).GetDouble();
        const double twoM = BlePhy::CalculateTxDuration(maxPdu, BlePhyMode::LE_2M).GetDouble();
        NS_TEST_EXPECT_MSG_LT(twoM, oneM, "LE 2M must be faster than LE 1M");

        // The coded PHYs spread each bit over two or eight symbols, so the payload takes four
        // times longer at S=8 than at S=2.
        const double s2 = BlePhy::CalculateTxDuration(maxPdu, BlePhyMode::LE_CODED_S2).GetDouble();
        const double s8 = BlePhy::CalculateTxDuration(maxPdu, BlePhyMode::LE_CODED_S8).GetDouble();
        const double preambleAndFec1Us = 80.0 + 296.0;
        const double s2PayloadUs = s2 / 1000.0 - preambleAndFec1Us;
        const double s8PayloadUs = s8 / 1000.0 - preambleAndFec1Us;
        NS_TEST_EXPECT_MSG_EQ_TOL(s8PayloadUs / s2PayloadUs,
                                  4.0,
                                  0.001,
                                  "S=8 spreads the payload four times more than S=2");

        // The number of bits the error model is applied to covers the PDU and the CRC.
        NS_TEST_EXPECT_MSG_EQ(BlePhy::CalculatePduBits(PDU_HEADER_OCTETS + 27),
                              (2 + 27 + 3) * 8,
                              "Protected bits of a 27-octet payload");
    }
};

/**
 * @ingroup ble
 * @ingroup tests
 * @brief Check the error model against the receiver sensitivities of real radios and of the
 *        specification.
 */
class BleSensitivityTestCase : public TestCase
{
  public:
    BleSensitivityTestCase()
        : TestCase("Receiver sensitivity of the four LE PHYs")
    {
    }

  private:
    void DoRun() override
    {
        auto phy = CreateObject<BlePhy>();
        phy->SetRfChannel(ADV_CHANNEL_37);

        // Published sensitivities of a common BLE radio (Nordic nRF52840 datasheet), which the
        // model reproduces within about one decibel with its default noise figure of 8 dB.
        struct Expectation
        {
            BlePhyMode mode;
            double publishedDbm;
        };

        const std::vector<Expectation> expectations{
            {BlePhyMode::LE_1M, -96.0},
            {BlePhyMode::LE_2M, -93.0},
            {BlePhyMode::LE_CODED_S2, -99.0},
            {BlePhyMode::LE_CODED_S8, -103.0},
        };
        double previousDbm = 0.0;
        for (const auto& expectation : expectations)
        {
            const double sensitivityDbm = phy->CalculateSensitivityDbm(expectation.mode);
            NS_TEST_EXPECT_MSG_EQ_TOL(sensitivityDbm,
                                      expectation.publishedDbm,
                                      1.5,
                                      "Sensitivity of " << BlePhyModeName(expectation.mode));
            // every mode must comfortably exceed the minimum the specification requires
            NS_TEST_EXPECT_MSG_LT(sensitivityDbm,
                                  REFERENCE_SENSITIVITY_DBM - 20.0,
                                  "The modelled receiver must beat the required sensitivity");
            previousDbm = sensitivityDbm;
        }
        (void)previousDbm;

        // LE 2M pays three decibels for its wider bandwidth, the coded PHYs gain from their
        // lower information rate
        const double oneM = phy->CalculateSensitivityDbm(BlePhyMode::LE_1M);
        const double twoM = phy->CalculateSensitivityDbm(BlePhyMode::LE_2M);
        const double s2 = phy->CalculateSensitivityDbm(BlePhyMode::LE_CODED_S2);
        const double s8 = phy->CalculateSensitivityDbm(BlePhyMode::LE_CODED_S8);
        NS_TEST_EXPECT_MSG_EQ_TOL(twoM - oneM, 3.0, 0.2, "LE 2M costs three decibels");
        NS_TEST_EXPECT_MSG_EQ_TOL(oneM - s2, 3.0, 0.2, "LE Coded S=2 gains three decibels");
        NS_TEST_EXPECT_MSG_EQ_TOL(oneM - s8, 7.0, 0.2, "LE Coded S=8 gains seven decibels");

        // the bit error rate falls as the signal to noise ratio grows and saturates at one half
        auto errorModel = CreateObject<BleErrorModel>();
        double previousBer = 0.5;
        for (double snrDb = -10.0; snrDb <= 30.0; snrDb += 1.0)
        {
            const double snr = std::pow(10.0, snrDb / 10.0);
            const double ber = errorModel->GetBitErrorRate(snr, BlePhyMode::LE_1M);
            NS_TEST_EXPECT_MSG_LT_OR_EQ(ber, previousBer, "The bit error rate must not increase");
            NS_TEST_EXPECT_MSG_LT_OR_EQ(ber, 0.5, "The bit error rate is at most one half");
            NS_TEST_EXPECT_MSG_GT_OR_EQ(ber, 0.0, "The bit error rate is not negative");
            previousBer = ber;
        }
        NS_TEST_EXPECT_MSG_EQ(errorModel->GetChunkSuccessRate(0.0, BlePhyMode::LE_1M, 100) < 1e-6,
                              true,
                              "A packet cannot survive a vanishing signal to noise ratio");
        NS_TEST_EXPECT_MSG_EQ_TOL(errorModel->GetChunkSuccessRate(1e6, BlePhyMode::LE_1M, 1000),
                                  1.0,
                                  1e-9,
                                  "A very strong signal is received without error");
    }
};

/**
 * @ingroup ble
 * @ingroup tests
 * @brief Check the spectrum model and the power spectral densities.
 */
class BleSpectrumTestCase : public TestCase
{
  public:
    BleSpectrumTestCase()
        : TestCase("Spectrum model and power spectral densities")
    {
    }

  private:
    void DoRun() override
    {
        // the transmitted power is conserved by the power spectral density
        for (uint8_t channel :
             {ADV_CHANNEL_37, ADV_CHANNEL_38, ADV_CHANNEL_39, uint8_t{0}, uint8_t{18}, uint8_t{36}})
        {
            for (auto mode : {BlePhyMode::LE_1M, BlePhyMode::LE_2M, BlePhyMode::LE_CODED_S8})
            {
                auto psd = BleSpectrumValueHelper::CreateTxPowerSpectralDensity(0.0, channel, mode);
                double totalW = 0.0;
                auto valueIt = psd->ConstValuesBegin();
                auto bandIt = psd->ConstBandsBegin();
                while (valueIt != psd->ConstValuesEnd())
                {
                    totalW += (*valueIt) * (bandIt->fh - bandIt->fl);
                    ++valueIt;
                    ++bandIt;
                }
                // 0 dBm is one milliwatt
                NS_TEST_EXPECT_MSG_EQ_TOL(totalW,
                                          1e-3,
                                          1e-5,
                                          "The density of channel " << +channel << " must carry "
                                                                    << "the transmitted power");
            }
        }

        // A receiver collects most of the power of a transmitter on its own channel and very
        // little of the power of a neighbour. Data channels 0, 1 and 2 are two megahertz apart,
        // unlike data channels 10 and 11, which straddle advertising channel 38.
        auto onChannel =
            BleSpectrumValueHelper::CreateTxPowerSpectralDensity(0.0, 0, BlePhyMode::LE_1M);
        const double own = BleSpectrumValueHelper::GetBandPower(onChannel, 0, BlePhyMode::LE_1M);
        const double adjacent =
            BleSpectrumValueHelper::GetBandPower(onChannel, 1, BlePhyMode::LE_1M);
        const double far = BleSpectrumValueHelper::GetBandPower(onChannel, 2, BlePhyMode::LE_1M);
        NS_TEST_EXPECT_MSG_GT(own, 10 * adjacent, "Most power lands on the transmitted channel");
        NS_TEST_EXPECT_MSG_GT(adjacent, far, "Adjacent channel leakage falls with distance");
        // The leakage two megahertz away stays within the transmitter spectrum mask of the
        // specification, which allows at most -20 dBc there. Decibels relative to the carrier
        // are measured against the whole transmitted power, one milliwatt here, not against the
        // fraction a one megahertz receiver collects.
        const double totalTxW = 1e-3;
        NS_TEST_EXPECT_MSG_LT_OR_EQ(10.0 * std::log10(adjacent / totalTxW),
                                    -20.0,
                                    "Adjacent channel leakage must respect the spectrum mask");

        // the noise power grows with the receiver bandwidth
        auto noise = BleSpectrumValueHelper::CreateNoisePowerSpectralDensity(8.0);
        const double noise1M = BleSpectrumValueHelper::GetBandPower(noise, 10, BlePhyMode::LE_1M);
        const double noise2M = BleSpectrumValueHelper::GetBandPower(noise, 10, BlePhyMode::LE_2M);
        NS_TEST_EXPECT_MSG_EQ_TOL(noise2M / noise1M,
                                  2.0,
                                  0.01,
                                  "LE 2M collects twice the noise of LE 1M");
        // thermal noise in one megahertz with an eight decibel noise figure
        const double noise1MDbm = 10.0 * std::log10(noise1M) + 30.0;
        NS_TEST_EXPECT_MSG_EQ_TOL(noise1MDbm, -106.0, 0.5, "Noise power in one megahertz");
    }
};

/**
 * @ingroup ble
 * @ingroup tests
 * @brief Test suite of the BLE model.
 */
class BleTestSuite : public TestSuite
{
  public:
    BleTestSuite()
        : TestSuite("ble", Type::UNIT)
    {
        AddTestCase(new BleCrcWhiteningTestCase, Duration::QUICK);
        AddTestCase(new BleChannelSelectionTestCase, Duration::QUICK);
        AddTestCase(new BleChannelMappingTestCase, Duration::QUICK);
        AddTestCase(new BleHeaderTestCase, Duration::QUICK);
        AddTestCase(new BlePacketDurationTestCase, Duration::QUICK);
        AddTestCase(new BleSensitivityTestCase, Duration::QUICK);
        AddTestCase(new BleSpectrumTestCase, Duration::QUICK);
    }
};

static BleTestSuite g_bleTestSuite; //!< the test suite
