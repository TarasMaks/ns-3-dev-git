.. highlight:: cpp

Bluetooth Low Energy (ble)
--------------------------

The ``ble`` contrib module models Bluetooth Low Energy: the physical layer and the Link Layer of
Bluetooth Core Specification 5.x, Volume 6, Parts A and B. It is built on the spectrum framework,
so BLE signals interfere with the other 2.4 GHz technologies of ns-3 and coexistence can be
simulated directly. See ``contrib/ble/README.md`` for the full description, the example list and
the table of results the model reproduces.

Model Description
*****************

The module has three layers.

``BlePhy`` is a ``SpectrumPhy`` implementing the four LE PHYs, LE 1M, LE 2M, LE Coded S=2 and
LE Coded S=8. It computes on-air packet durations from the preamble, access address, PDU and CRC
of each PHY, accumulates the interference of every overlapping signal per megahertz, and resolves
a reception into a packet error probability chunk by chunk as the interference changes. Because a
BLE radio listens only when the Link Layer opens a receive window, the PHY has an explicit
receive window rather than listening continuously.

``BleErrorModel`` turns a signal to interference and noise ratio into a bit error rate using the
expression for non-coherent binary frequency shift keying, with the energy per information bit
derived from the receiver bandwidth and the information rate of the mode.

``BleLinkLayer`` implements advertising, scanning, initiating and connections, including the
random delay added to every advertising event, both channel selection algorithms, the one-bit
stop and wait protocol with its sequence numbers and more data bit, peripheral latency, the
supervision timeout and L2CAP fragmentation.

``BleNetDevice`` presents the pair as a normal ``NetDevice``, and ``BleHelper`` installs devices,
attaches them to a shared spectrum channel and writes Wireshark captures.

Usage
*****

::

  BleHelper ble;
  ble.SetChannel(BleHelper::CreateChannel("ns3::LogDistancePropagationLossModel"));
  ble.SetPhyAttribute("PhyMode", EnumValue(BlePhyMode::LE_CODED_S8));
  NetDeviceContainer devices = ble.Install(nodes);
  BleHelper::ConnectStatically(devices.Get(0), devices.Get(1), MilliSeconds(30), Seconds(1));

Validation
**********

The ``ble`` test suite checks the CRC and the whitening against their mathematical invariants,
both channel selection algorithms against the properties a connection depends on, the header
serialization, the on-air packet durations against the durations published for the four LE PHYs,
and the receiver sensitivities against the figures published for a common BLE radio. The
``ble-connection`` suite runs connections end to end and checks reliable delivery, fragmentation
and reassembly, the throughput against its analytical maximum, and discovery through advertising
and scanning. The examples are run by ``test.py``.
