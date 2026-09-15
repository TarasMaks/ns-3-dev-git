/*
 * Copyright (c) 2026
 *
 * SPDX-License-Identifier: GPL-2.0-only
 */

#include "ns3/ble-helper.h"
#include "ns3/ble-link-layer.h"
#include "ns3/ble-net-device.h"
#include "ns3/ble-phy.h"
#include "ns3/constant-position-mobility-model.h"
#include "ns3/enum.h"
#include "ns3/mobility-helper.h"
#include "ns3/node-container.h"
#include "ns3/packet.h"
#include "ns3/simulator.h"
#include "ns3/test.h"
#include "ns3/uinteger.h"

#include <vector>

using namespace ns3;
using namespace ns3::ble;

namespace
{

/**
 * Build two nodes a short distance apart, each with a BLE device on a shared channel.
 *
 * @param devices filled with the two devices
 * @param nodes filled with the two nodes
 * @param mode the PHY mode of both devices
 * @param distance the distance between the nodes, in metres
 * @param maxPduPayload the largest data channel PDU payload
 */
void
BuildPair(NetDeviceContainer& devices,
          NodeContainer& nodes,
          BlePhyMode mode,
          double distance,
          uint8_t maxPduPayload = DATA_PDU_PAYLOAD_MAX,
          uint32_t queueSize = 100)
{
    nodes.Create(2);
    MobilityHelper mobility;
    auto positions = CreateObject<ListPositionAllocator>();
    positions->Add(Vector(0.0, 0.0, 0.0));
    positions->Add(Vector(distance, 0.0, 0.0));
    mobility.SetPositionAllocator(positions);
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(nodes);

    BleHelper ble;
    ble.SetChannel(BleHelper::CreateChannel("ns3::FriisPropagationLossModel"));
    ble.SetPhyAttribute("PhyMode", EnumValue(mode));
    ble.SetLinkLayerAttribute("MaxPduPayload", UintegerValue(maxPduPayload));
    ble.SetLinkLayerAttribute("QueueSize", UintegerValue(queueSize));
    devices = ble.Install(nodes);
    BleHelper::AssignStreams(devices, 1);
}

/// Counts the advertising reports a scanner produces.
struct AdvCounter
{
    uint32_t reports{0};      //!< reports seen
    Mac48Address lastAddress; //!< the advertiser of the last report

    /**
     * @param report the report
     */
    void Count(BleAdvReport report)
    {
        ++reports;
        lastAddress = report.address;
    }
};

/// Counts the payloads a device receives and remembers their total size.
struct Sink
{
    uint32_t packets{0}; //!< payloads received
    uint64_t bytes{0};   //!< octets received

    /**
     * @param packet the payload
     * @param from the address it came from
     */
    void Receive(Ptr<Packet> packet, Mac48Address from)
    {
        ++packets;
        bytes += packet->GetSize();
    }
};

} // namespace

/**
 * @ingroup ble
 * @ingroup tests
 * @brief Check that a connection carries payloads reliably in both directions.
 */
class BleConnectionTestCase : public TestCase
{
  public:
    BleConnectionTestCase()
        : TestCase("A connection carries payloads in both directions")
    {
    }

  private:
    void DoRun() override
    {
        NetDeviceContainer devices;
        NodeContainer nodes;
        BuildPair(devices, nodes, BlePhyMode::LE_1M, 1.0);

        auto central = DynamicCast<BleNetDevice>(devices.Get(0));
        auto peripheral = DynamicCast<BleNetDevice>(devices.Get(1));
        BleHelper::ConnectStatically(central, peripheral, MilliSeconds(20), MilliSeconds(10));

        Sink centralSink;
        Sink peripheralSink;
        central->GetLinkLayer()->SetReceiveCallback(MakeCallback(&Sink::Receive, &centralSink));
        peripheral->GetLinkLayer()->SetReceiveCallback(
            MakeCallback(&Sink::Receive, &peripheralSink));

        // twenty payloads in each direction, small enough to fit one PDU each
        const uint32_t count = 20;
        const uint32_t payloadSize = 100;
        for (uint32_t i = 0; i < count; ++i)
        {
            Simulator::Schedule(MilliSeconds(20 + i), [&, i]() {
                central->GetLinkLayer()->Enqueue(Create<Packet>(payloadSize),
                                                 peripheral->GetLinkLayer()->GetAddress());
                peripheral->GetLinkLayer()->Enqueue(Create<Packet>(payloadSize),
                                                    central->GetLinkLayer()->GetAddress());
            });
        }

        Simulator::Stop(Seconds(3));
        Simulator::Run();

        NS_TEST_EXPECT_MSG_EQ(central->GetLinkLayer()->IsConnected(),
                              true,
                              "The central must still be connected");
        NS_TEST_EXPECT_MSG_EQ(peripheral->GetLinkLayer()->IsConnected(),
                              true,
                              "The peripheral must still be connected");
        NS_TEST_EXPECT_MSG_EQ(static_cast<uint32_t>(central->GetLinkLayer()->GetRole()),
                              static_cast<uint32_t>(BleRole::CENTRAL),
                              "The first device must be the central");
        NS_TEST_EXPECT_MSG_EQ(static_cast<uint32_t>(peripheral->GetLinkLayer()->GetRole()),
                              static_cast<uint32_t>(BleRole::PERIPHERAL),
                              "The second device must be the peripheral");

        NS_TEST_EXPECT_MSG_EQ(peripheralSink.packets,
                              count,
                              "Every payload sent by the central must arrive");
        NS_TEST_EXPECT_MSG_EQ(centralSink.packets,
                              count,
                              "Every payload sent by the peripheral must arrive");
        NS_TEST_EXPECT_MSG_EQ(peripheralSink.bytes,
                              static_cast<uint64_t>(count) * payloadSize,
                              "Payloads must arrive intact");
        NS_TEST_EXPECT_MSG_EQ(centralSink.bytes,
                              static_cast<uint64_t>(count) * payloadSize,
                              "Payloads must arrive intact");
        Simulator::Destroy();
    }
};

/**
 * @ingroup ble
 * @ingroup tests
 * @brief Check that payloads larger than a PDU are fragmented and reassembled.
 */
class BleFragmentationTestCase : public TestCase
{
  public:
    BleFragmentationTestCase()
        : TestCase("Payloads larger than a PDU are fragmented and reassembled")
    {
    }

  private:
    void DoRun() override
    {
        // the smallest PDU payload the specification allows, so that even modest payloads are
        // split over several PDUs
        NetDeviceContainer devices;
        NodeContainer nodes;
        BuildPair(devices, nodes, BlePhyMode::LE_1M, 1.0, DATA_PDU_PAYLOAD_MIN_MAX);

        auto central = DynamicCast<BleNetDevice>(devices.Get(0));
        auto peripheral = DynamicCast<BleNetDevice>(devices.Get(1));
        BleHelper::ConnectStatically(central, peripheral, MilliSeconds(20), MilliSeconds(10));

        Sink sink;
        peripheral->GetLinkLayer()->SetReceiveCallback(MakeCallback(&Sink::Receive, &sink));

        // 500 octets need twenty PDUs of 27 octets once the L2CAP header is added
        const std::vector<uint32_t> sizes{1, 27, 100, 500, 1280};
        uint64_t expectedBytes = 0;
        for (std::size_t i = 0; i < sizes.size(); ++i)
        {
            expectedBytes += sizes[i];
            Simulator::Schedule(MilliSeconds(20 + 10 * i), [&, i]() {
                central->GetLinkLayer()->Enqueue(Create<Packet>(sizes[i]),
                                                 peripheral->GetLinkLayer()->GetAddress());
            });
        }

        Simulator::Stop(Seconds(15));
        Simulator::Run();

        NS_TEST_EXPECT_MSG_EQ(sink.packets,
                              static_cast<uint32_t>(sizes.size()),
                              "Every payload must be reassembled exactly once");
        NS_TEST_EXPECT_MSG_EQ(sink.bytes,
                              expectedBytes,
                              "Reassembled payloads must have their original sizes");
        Simulator::Destroy();
    }
};

/**
 * @ingroup ble
 * @ingroup tests
 * @brief Check the throughput of a saturated connection against the analytical maximum.
 */
class BleThroughputTestCase : public TestCase
{
  public:
    BleThroughputTestCase()
        : TestCase("Connection throughput against the analytical maximum")
    {
    }

  private:
    /**
     * The largest application throughput a connection can carry, obtained by filling connection
     * events with exchanges of a full data PDU answered by an empty one.
     *
     * @param mode the PHY mode
     * @param payload the data channel PDU payload, in octets
     * @return the throughput in kb/s
     */
    static double AnalyticalMaxKbps(BlePhyMode mode, uint32_t payload)
    {
        const double dataUs =
            BlePhy::CalculateTxDuration(PDU_HEADER_OCTETS + payload, mode).GetNanoSeconds() /
            1000.0;
        const double emptyUs =
            BlePhy::CalculateTxDuration(PDU_HEADER_OCTETS, mode).GetNanoSeconds() / 1000.0;
        const double exchangeUs = dataUs + T_IFS_US + emptyUs + T_IFS_US;
        // the L2CAP header of every payload is not application data
        return (payload * 8.0) / exchangeUs * 1000.0;
    }

    void DoRun() override
    {
        struct Expectation
        {
            BlePhyMode mode;
            double publishedKbps; //!< widely quoted maximum application throughput
        };

        // the figures usually quoted for a connection using the Data Length Extension
        const std::vector<Expectation> expectations{
            {BlePhyMode::LE_1M, 814.0},
            {BlePhyMode::LE_2M, 1442.0},
        };

        for (const auto& expectation : expectations)
        {
            const double analytical = AnalyticalMaxKbps(expectation.mode, DATA_PDU_PAYLOAD_MAX);
            NS_TEST_EXPECT_MSG_EQ_TOL(analytical,
                                      expectation.publishedKbps,
                                      15.0,
                                      "The analytical maximum of "
                                          << BlePhyModeName(expectation.mode)
                                          << " must match the published figure");

            NetDeviceContainer devices;
            NodeContainer nodes;
            // the queue must stay full for the whole measurement, otherwise the connection
            // idles and the average throughput reflects the offered load instead of the limit
            BuildPair(devices, nodes, expectation.mode, 1.0, DATA_PDU_PAYLOAD_MAX, 20000);
            auto central = DynamicCast<BleNetDevice>(devices.Get(0));
            auto peripheral = DynamicCast<BleNetDevice>(devices.Get(1));
            // a long connection interval lets many exchanges fit in one event
            BleHelper::ConnectStatically(central, peripheral, MilliSeconds(100), MilliSeconds(10));

            Sink sink;
            peripheral->GetLinkLayer()->SetReceiveCallback(MakeCallback(&Sink::Receive, &sink));

            // keep the queue full for the whole run
            const uint32_t payloadSize = DATA_PDU_PAYLOAD_MAX - L2CAP_HEADER_OCTETS;
            const uint32_t offered = 20000;
            for (uint32_t i = 0; i < offered; ++i)
            {
                const bool queued =
                    central->GetLinkLayer()->Enqueue(Create<Packet>(payloadSize),
                                                     peripheral->GetLinkLayer()->GetAddress());
                NS_TEST_EXPECT_MSG_EQ(queued, true, "The queue must accept the offered payload");
            }

            const Time stop = Seconds(2);
            Simulator::Stop(stop);
            Simulator::Run();

            const double measuredKbps =
                sink.bytes * 8.0 / (stop - MilliSeconds(10)).GetSeconds() / 1000.0;
            NS_TEST_EXPECT_MSG_GT(central->GetLinkLayer()->GetQueueSize(),
                                  0u,
                                  "The queue must still hold payloads at the end of the run");
            // The central refuses to start an exchange that could not finish before the next
            // anchor point, and it sizes that check for a full-length answer, so a fraction of
            // each connection interval is left unused.
            NS_TEST_EXPECT_MSG_GT(measuredKbps,
                                  0.90 * analytical,
                                  "The measured throughput of "
                                      << BlePhyModeName(expectation.mode)
                                      << " must approach the analytical maximum");
            NS_TEST_EXPECT_MSG_LT_OR_EQ(measuredKbps,
                                        analytical * 1.02,
                                        "The measured throughput cannot exceed the maximum");
            Simulator::Destroy();
        }
    }
};

/**
 * @ingroup ble
 * @ingroup tests
 * @brief Check that advertising is heard by a scanner and leads to a connection.
 */
class BleDiscoveryTestCase : public TestCase
{
  public:
    BleDiscoveryTestCase()
        : TestCase("Advertising, scanning and connection establishment")
    {
    }

  private:
    void DoRun() override
    {
        NetDeviceContainer devices;
        NodeContainer nodes;
        BuildPair(devices, nodes, BlePhyMode::LE_1M, 2.0);
        auto advertiser = DynamicCast<BleNetDevice>(devices.Get(0));
        auto scanner = DynamicCast<BleNetDevice>(devices.Get(1));

        advertiser->GetLinkLayer()->SetAttribute("AdvInterval", TimeValue(MilliSeconds(50)));
        scanner->GetLinkLayer()->SetAttribute("ScanInterval", TimeValue(MilliSeconds(50)));
        scanner->GetLinkLayer()->SetAttribute("ScanWindow", TimeValue(MilliSeconds(50)));

        AdvCounter counter;
        scanner->GetLinkLayer()->TraceConnectWithoutContext(
            "AdvReport",
            MakeCallback(&AdvCounter::Count, &counter));

        advertiser->Initialize();
        scanner->Initialize();
        Simulator::Schedule(MilliSeconds(1),
                            &BleLinkLayer::StartAdvertising,
                            advertiser->GetLinkLayer());
        Simulator::Schedule(MilliSeconds(1), &BleLinkLayer::StartScanning, scanner->GetLinkLayer());

        Simulator::Stop(Seconds(2));
        Simulator::Run();
        NS_TEST_EXPECT_MSG_GT(counter.reports,
                              5u,
                              "A scanner must hear a nearby advertiser repeatedly");
        NS_TEST_EXPECT_MSG_EQ(counter.lastAddress,
                              advertiser->GetLinkLayer()->GetAddress(),
                              "The reports must name the advertiser");
        Simulator::Destroy();
    }
};

/**
 * @ingroup ble
 * @ingroup tests
 * @brief Test suite of the BLE connection procedures.
 */
class BleConnectionTestSuite : public TestSuite
{
  public:
    BleConnectionTestSuite()
        : TestSuite("ble-connection", Type::UNIT)
    {
        AddTestCase(new BleConnectionTestCase, Duration::QUICK);
        AddTestCase(new BleFragmentationTestCase, Duration::QUICK);
        AddTestCase(new BleThroughputTestCase, Duration::QUICK);
        AddTestCase(new BleDiscoveryTestCase, Duration::QUICK);
    }
};

static BleConnectionTestSuite g_bleConnectionTestSuite; //!< the test suite
