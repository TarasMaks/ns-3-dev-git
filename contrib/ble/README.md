# ble: a Bluetooth Low Energy model for ns-3

`ble` is a contrib module that models Bluetooth Low Energy, covering the physical layer and the
Link Layer of Bluetooth Core Specification 5.x (Volume 6, Parts A and B). It is built on the
ns-3 spectrum framework, so BLE signals share the 2.4 GHz band with every other ns-3 technology
that uses that framework, and coexistence with Wi-Fi can be simulated directly.

## What is modelled

**Physical layer** (`BlePhy`, a `SpectrumPhy`)

- The four LE PHYs: LE 1M, LE 2M, LE Coded S=2 and LE Coded S=8.
- All 40 RF channels, with the mapping between logical channel indices and centre frequencies,
  including the three primary advertising channels at 2402, 2426 and 2480 MHz.
- On-air packet durations computed from the preamble, access address, PDU and CRC of each PHY,
  including the separate coding of FEC block 1 and FEC block 2 of the LE Coded PHY.
- A transmit spectrum whose adjacent channel leakage respects the transmitter spectrum mask.
- Interference from every overlapping signal, BLE or not, accumulated per 1 MHz bin and resolved
  into a packet error probability chunk by chunk as the interference changes.
- A state machine with an explicit receive window, because a BLE radio listens only when the
  Link Layer opens one.

**Error model** (`BleErrorModel`)

The LE PHYs modulate with GFSK at a modulation index near 0.5, so the model uses the bit error
rate of non-coherent binary frequency shift keying, `BER = 0.5 exp(-0.5 Eb/N0)`, and derives the
energy per information bit from the signal to noise ratio measured in the receiver bandwidth.
The coded PHYs gain 3 dB (S=2) and 9 dB (S=8) from their lower information rate, less a 2 dB
implementation loss on S=8 whose pattern mapper does not reach the coding gain of an ideal code
of the same rate.

**Link Layer** (`BleLinkLayer`)

- Advertising events over the enabled primary advertising channels, with the random delay of up
  to 10 ms that the specification adds to every event.
- Passive and active scanning, with a scan window inside a scan interval and channel rotation.
- Connection establishment through `CONNECT_IND`, and a static setup path that skips discovery
  when a study only cares about the connection.
- Connection events anchored one connection interval apart, hopping over the data channels with
  channel selection algorithm #1 or #2.
- The one-bit stop and wait protocol of the specification: every packet carries a sequence number
  and the number its sender expects next, packets are retransmitted until acknowledged, and the
  more data bit extends an event while either side still has something to send.
- Peripheral latency, supervision timeout, and adaptive frequency hopping through a settable
  channel map.
- L2CAP fragmentation and reassembly of payloads larger than one PDU.

**Device and helper** (`BleNetDevice`, `BleHelper`)

A normal `NetDevice` so applications, the Internet stack and 6LoWPAN can run on top, with a
default MTU of 1280 octets as RFC 7668 requires for IPv6 over BLE, plus a helper that installs
devices, wires them to a shared spectrum channel and can write Wireshark captures.

## Building and running

```bash
./ns3 configure --enable-examples --enable-tests
./ns3 build
./test.py -s ble                # unit tests of the utilities, PHY and error model
./test.py -s ble-connection     # end to end tests of the connection procedures
./ns3 run "ble-connection-throughput --phy=2M"
```

## Examples

| Example | What it shows |
|---|---|
| `ble-connection-throughput` | Throughput of one connection against the analytical maximum, swept over PHY mode, connection interval and PDU payload |
| `ble-range-phy-modes` | Delivery ratio against distance for the four PHYs, next to the sensitivity and the free space range each of them implies |
| `ble-advertising-discovery` | How long a scanner takes to find advertisers, against the advertising interval and the scan duty cycle |
| `ble-piconet` | One central serving several peripherals, with the anchor points of the connections spread over the interval |
| `ble-wifi-coexistence` | A BLE connection and a saturated Wi-Fi link on the same spectrum channel, with and without adaptive frequency hopping |

## Results the model reproduces

Every figure below is checked by the test suite or printed by an example.

| Quantity | Model | Reference |
|---|---|---|
| Longest packet, LE 1M / 2M / S=2 / S=8 | 2120 / 1064 / 4542 / 17040 us | The durations published for the four LE PHYs with a 255-octet PDU |
| Receiver sensitivity, LE 1M / 2M / S=2 / S=8 | -95 / -92 / -98 / -102 dBm | Nordic nRF52840 datasheet: -96 / -93 / -99 / -103 dBm, all within about 1 dB |
| Sensitivity against the requirement | 25 dB of margin or more | The Bluetooth Core Specification requires -70 dBm |
| Relative sensitivity of the PHYs | LE 2M costs 3 dB, S=2 gains 3 dB, S=8 gains 7 dB | The same datasheet figures |
| Maximum application throughput, LE 1M | 814 kb/s analytical, 772 kb/s simulated | The figure usually quoted for a connection using the Data Length Extension |
| Maximum application throughput, LE 2M | 1442 kb/s analytical, about 1.37 Mb/s simulated | The figure usually quoted for LE 2M |
| Free space range at 0 dBm, LE 1M / S=8 | 565 m / 1269 m | The coded PHY was introduced to extend range, and delivers it |
| BLE next to a saturated Wi-Fi link | 37 % of its throughput, fully recovered by removing the ten overlapping channels | Why adaptive frequency hopping exists |
| Noise power in 1 MHz with an 8 dB noise figure | -106 dBm | Thermal noise, -174 dBm/Hz |

## Modelling choices worth knowing

- **Conservative connection events.** The central refuses to start an exchange that could not
  finish before the next anchor point, and sizes that check for a full-length answer even when
  the peripheral is only going to acknowledge. A fraction of each connection interval therefore
  goes unused, which is why the simulated throughput sits at about 95 % of the analytical bound.
- **CRC and whitening are computed but do not decide anything.** Packets succeed or fail from the
  signal to interference and noise ratio, as in the rest of ns-3. The CRC and the whitening are
  implemented so that captures and traces carry the right values, and the test suite checks them
  against their mathematical invariants: the CRC is linear over GF(2) with a zero register and
  every single bit flip changes it, and whitening is its own inverse and differs per channel.
- **Channel selection algorithm #2** is implemented with the structure the specification
  describes, three rounds of its bit permutation and its multiply, add and modulo operation. The
  properties the simulation depends on are tested: the output is deterministic, always a usable
  channel, and spread over the whole channel map.
- **One radio per connection.** A device holds one connection, as its single radio allows. A
  central serving several peripherals is modelled with one device per connection on the same
  node, which is what the piconet example does.
- **Access addresses** are drawn to satisfy the constraints of the specification: distance from
  the advertising access address, no more than six equal consecutive bits, not four equal octets,
  at most 24 transitions, and at least two transitions in the six most significant bits.

## Not modelled

Extended and periodic advertising, LE Audio and isochronous channels, encryption and pairing,
connection parameter update and PHY update procedures beyond the control PDU definitions,
multiple connections on one radio with a real scheduler, sleep clock accuracy and the receive
window widening it causes, and the 2 Mb/s advertising extensions.

## References

- Bluetooth Core Specification 5.x, Volume 6, Parts A and B: the LE physical layer and the Link
  Layer, including the MCS timings, the channel selection algorithms, the CRC and whitening, and
  the receiver sensitivity requirement.
- RFC 7668, *IPv6 over Bluetooth Low Energy*, for the L2CAP MTU the device defaults to.
- Nordic Semiconductor nRF52840 product specification, for the published receiver sensitivities
  the error model is calibrated against.
