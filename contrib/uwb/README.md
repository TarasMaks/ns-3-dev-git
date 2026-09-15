# UWB module for ns-3

An Ultra-Wideband model: the High Rate Pulse repetition frequency (HRP) UWB physical layer of
IEEE Std 802.15.4, and the ranging procedures of IEEE Std 802.15.4z.

ns-3 has neither of these. It has no UWB physical layer, and it has no notion of ranging at all:
nothing in the tree measures a distance from a propagation delay, because nothing needs to. Both
are built here from what ns-3 does provide, the spectrum framework and the event scheduler, with
two things changed underneath.

## What had to be built rather than reused

**The clock runs at femtoseconds.** ns-3 keeps time in nanoseconds by default, and a nanosecond
is thirty centimetres of propagation. A ranging model on that grid would report nothing but
quantisation noise. The module raises the simulator resolution to femtoseconds, which is 0.3
micrometres, comfortably below the 15.65 ps (4.69 mm) grid the radios themselves work on. The
cost is the span of a simulation: a 64-bit count of femtoseconds runs out after about 2.5 hours,
which is far longer than any ranging scenario needs. `UwbHelper` does this for you; a script that
builds the pieces by hand calls `EnableUwbTimeResolution()` before anything else.

**Every device has its own crystal.** `UwbClockModel` is a 40-bit counter running at 63.8976 GHz
with a frequency error in parts per million, exactly like the counter in a real transceiver, wrap
included. Nothing in the module reads a global clock. This is the whole point: a model in which
two devices agreed about time would make single-sided ranging look as good as double-sided, which
is the one thing a UWB model must not do.

## What is in it

| | |
|---|---|
| `UwbPhy` | A `SpectrumPhy` on a real 499.2 MHz channel. Acquires the preamble, then demodulates the payload, and timestamps the marker. |
| `UwbErrorModel` | BPM-BPSK with processing gain, calibrated against published receiver sensitivities. |
| `UwbClockModel` | The crystal: frequency offset, slow drift, and the 40-bit counter. |
| `UwbMac` | Unslotted ALOHA, acknowledged data frames, and the ranging state machines. |
| `UwbTdoaEngine` | The infrastructure side of time difference of arrival. |
| `UwbNetDevice`, `UwbHelper` | The ordinary ns-3 plumbing. |

Channels 1 to 15 of the standard are modelled, with their real centre frequencies and
bandwidths; data rates 0.11, 0.85, 6.81 and 27.24 Mb/s; pulse repetition frequencies of 15.6 and
62.4 MHz; preambles of 16 to 4096 symbols; and the -41.3 dBm/MHz regulatory density that makes a
technology with half a gigahertz of bandwidth nevertheless short range.

## How the physical layer is calibrated

The error model takes the signal to interference and noise ratio measured across the whole
channel, converts it to energy per information bit through the processing gain
`10 log10(B / R)`, and applies the bit error rate of coherent binary phase shift keying,
`BER = 0.5 erfc(sqrt(Eb/N0))`, to an effective `Eb/N0` that carries one more per-rate term. That
term is the net of the coding gain of Clause 15 and the implementation loss of a real receiver,
and it is not guessed: it is solved at run time from two published sensitivities of the Qorvo
DW1000, -105 dBm at 0.11 Mb/s and -93 dBm at 6.81 Mb/s, both at 1 % packet error rate on a 127
octet frame. The two remaining rates sit on the straight line those anchors define against the
logarithm of the data rate, which yields -99 dBm at 0.85 Mb/s and -89 dBm at 27.24 Mb/s. The test
suite checks that the anchors come back exactly.

Dropping from 6.81 Mb/s to 0.11 Mb/s is 18.1 dB of data rate but buys only 12 dB of sensitivity,
so the long range mode is about 6 dB less efficient per bit than the fast one. That is a property
of real hardware, not of the code: a 127 octet frame at 0.11 Mb/s lasts about ten milliseconds,
over which crystal drift and the receiver tracking loops cost more than the extra coding returns.
A model that assumed the long range mode was ideal would overstate UWB range by a factor of two,
so the loss is kept.

Acquisition is modelled separately from demodulation, because in UWB they give out at different
points. The correlator accumulates the code length multiplied by the number of preamble symbols,
and the result has to clear a threshold before the payload is looked at. At the preamble lengths
products use this leaves several dB of margin; shorten the preamble and it becomes the limit
instead, which is exactly why long range configurations use long preambles.

## How a range is measured

Every frame carries a marker, the first pulse of the physical layer header, which is the instant
both ends refer to. The transmitter knows its marker exactly, because it launches the frame on a
counter edge; `UwbPhy::ScheduleTx` is the delayed transmission that lets a device put its own
transmit timestamp inside the frame it is about to send, and no double-sided exchange works
without it.

The receiver has to estimate the marker from a signal below the noise floor. The error it makes
is drawn from the Cramer-Rao bound for time of arrival estimation,
`sigma = 1 / (2 pi beta sqrt(2 SNR))`, where `beta` is the root mean square bandwidth of the
channel and the signal to noise ratio is the one after the preamble correlator, scaled by a
factor for the gap between a real leading edge detector and the bound and floored at the residual
a calibrated radio still shows at close range. At ten metres on channel 5 that comes out at about
a centimetre, which is what published static ranging measurements report.

## Running it

    ./ns3 configure --enable-examples --enable-tests
    ./ns3 build
    ./test.py -s uwb

    ./ns3 run uwb-ranging-comparison    # why the third frame is worth sending
    ./ns3 run uwb-link-budget           # how far it reaches, and what the preamble buys
    ./ns3 run uwb-positioning           # a tag in a room, located two ways
    ./ns3 run uwb-tag-capacity          # how many tags one channel carries
    ./ns3 run uwb-wifi-coexistence      # sharing 6.5 GHz with Wi-Fi 6E

A minimal script:

```cpp
EnableUwbTimeResolution();            // before anything creates a Time

UwbHelper uwb;
uwb.SetChannelNumber(5);
uwb.SetDataRate(UwbDataRate::RATE_6M81);
uwb.SetChannel(UwbHelper::CreateChannel(5));
NetDeviceContainer devices = uwb.Install(nodes);

auto initiator = DynamicCast<UwbNetDevice>(devices.Get(0));
initiator->SetRangingResultCallback(MakeCallback(&OnRange));
initiator->StartRanging(Mac16Address::ConvertFrom(devices.Get(1)->GetAddress()),
                        UwbRangingMethod::DS_TWR);
```

`UwbHelper::CreateChannel` exists because ns-3 evaluates a propagation loss model at the
frequency the model is configured with rather than at the frequency of the signal. A Friis model
left at its default would compute the loss of a 5.15 GHz link for a channel sitting at 6.5 GHz,
and every received power would be two dB out. The helper sets it from the channel in use.

## What the examples show

**The third frame.** Two devices ten metres apart, ordinary 20 ppm crystals, a 340 microsecond
turnaround. Single-sided ranging reports 12.04 m; double-sided ranging reports 10.00 m. The
single-sided error is the crystal offset multiplied by half the turnaround and nothing else: it
does not shrink with distance, it does not average away, and it scales exactly with both the
crystal spread and the reply delay. The double-sided estimator cancels it to first order, leaving
a bias under a millimetre.

**Range.** On channel 5 with isotropic antennas and free space propagation, 0.11 Mb/s reaches
about 154 m, 0.85 Mb/s about 77 m, 6.81 Mb/s about 39 m and 27.24 Mb/s about 24 m. Six dB of
sensitivity is twice the range, so each step down the rates roughly doubles the reach. At the
lower pulse repetition frequency a short preamble throws most of that away.

**Positioning.** A tag walking a circuit inside six anchors is located to about 3 cm by two-way
ranging and trilateration, and to about 3 cm by blinking once and letting the anchors compare
arrival times, provided their clocks agree. One nanosecond of residual disagreement between the
anchors moves the answer by about 40 cm. With only four anchors the arrival-difference solution
has blind spots where the system is nearly singular and the answer wanders by metres, which is
why installations never use the minimum.

**Capacity.** A two-way fix costs four frames for every anchor it talks to; a blink costs one
frame however many anchors are listening. On one channel at 6.81 Mb/s, ten tags ranging ten times
a second against six anchors already push the ALOHA success rate down to a third, while a hundred
tags blinking at the same rate still get located more than four times in five.

**Coexistence.** UWB channel 5 covers 6240 to 6739 MHz, and Wi-Fi 6E channel 103 sits inside it.
The processing gain of 18.65 dB is a great deal but not thirty, and a Wi-Fi radio transmits about
thirty dB more power than a UWB one is allowed to, so at close range it buries the link entirely:
the UWB pair needs the Wi-Fi pair to be roughly eighty metres away before it works again. The
other direction is not symmetric at all. The Wi-Fi throughput does not move, because a
transmitter held to -41.3 dBm per megahertz puts less power into an 80 MHz Wi-Fi channel than
that channel's own noise floor.

## Limits

The propagation is whatever loss model the script installs, so the multipath that dominates real
indoor UWB accuracy is not modelled unless the script brings it. The leading edge error is drawn
from a bound that assumes a clean channel, which means the centimetres this module reports are
line of sight centimetres; a real non-line-of-sight measurement is biased long by a wall, and
that bias is not here. The pulse shape is a flat mask with skirts rather than the root raised
cosine of the standard. Scrambling, the Reed-Solomon and convolutional codes, and the preamble
codes themselves are accounted for in timing and in gain but not simulated bit by bit; frames
carry their real lengths and their real durations, and the frame check sequence is computed for
real, but the coding is a gain figure rather than an encoder.
