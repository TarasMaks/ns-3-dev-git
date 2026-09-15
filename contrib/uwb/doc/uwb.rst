.. highlight:: cpp

Ultra-Wideband (uwb)
--------------------

The ``uwb`` contrib module models Ultra-Wideband: the High Rate Pulse repetition frequency (HRP)
UWB physical layer of IEEE Std 802.15.4, Clause 15, and the ranging procedures of IEEE Std
802.15.4z. See ``contrib/uwb/README.md`` for the full description, the example list and the
results the model reproduces.

ns-3 has neither of these. There is no UWB physical layer in the tree, and there is no notion of
ranging at all: nothing else in ns-3 measures a distance from a propagation delay. Both are built
here on the spectrum framework, with two things changed underneath that the rest of this section
explains first, because nothing else in the module makes sense without them.

Model Description
*****************

Time resolution
+++++++++++++++

ns-3 keeps time in nanoseconds by default. A nanosecond is thirty centimetres of propagation, so
every ranging measurement would collapse onto that grid and the module would report quantisation
noise and nothing else. The module therefore raises the simulator resolution to femtoseconds,
which is 0.3 micrometres, comfortably below the 15.65 ps (4.69 mm) grid that the radios
themselves quantise to.

The cost is the span of a simulation: a 64-bit count of femtoseconds runs out after about 2.5
hours of simulated time, which is far longer than any ranging scenario needs. ``UwbHelper`` sets
the resolution in its constructor, and a script that builds the objects by hand calls
``EnableUwbTimeResolution()`` before anything else creates a ``Time``.

Device clocks
+++++++++++++

``UwbClockModel`` is the crystal of one device: a 40-bit counter running at 63.8976 GHz, which is
128 times the 499.2 MHz chip rate, with a frequency error in parts per million and an optional
slow drift. It is the same counter a real transceiver has, wrap included, so it turns over about
every 17.2 seconds and differences have to be taken modulo its width.

Nothing in the module reads a global clock. That is deliberate. A 20 ppm crystal, which is an
ordinary part and the most IEEE Std 802.15.4 permits, gains or loses 20 microseconds every
second; over the 300 microseconds a responder typically takes to turn a ranging frame around, two
devices 40 ppm apart accumulate 12 ns of disagreement, which is 3.6 metres of apparent range.
That single number is why single-sided two-way ranging is nearly useless without correction and
why the double-sided scheme exists, and a model in which every device could read a perfect clock
could not show it.

Physical layer
++++++++++++++

``UwbPhy`` is a ``SpectrumPhy`` occupying a real 499.2 MHz channel, so a UWB signal interferes
with everything else in the band and everything else interferes with it, including the 5 GHz and
6 GHz Wi-Fi models when they share a ``MultiModelSpectrumChannel``. The spectrum model covers
3.1 to 10.7 GHz with a resolution of 99.84 MHz, a fifth of the nominal channel width, chosen so
that every UWB centre frequency lands exactly on the grid and no channel edge has to be
approximated.

Receiving a frame has two stages, as it does in hardware. The correlator first has to lock onto
the preamble, accumulating the code length multiplied by the number of preamble symbols and
either clearing the acquisition threshold or not; only then is the payload demodulated, and
whether it survives depends on the signal to interference and noise ratio over its duration. A
frame whose preamble the receiver missed is not a frame with errors, it is a frame the receiver
never knew about, and the model distinguishes the two.

``UwbErrorModel`` turns the signal to interference and noise ratio, measured across the whole
channel, into a bit error rate. The channel is thousands of times wider than the information
rate, so the processing gain ``10 log10(B / R)`` is what makes a signal well below the noise
floor readable: 18.65 dB at 6.81 Mb/s on a nominal channel. The model then applies the bit error
rate of coherent binary phase shift keying, which is the polarity of a burst position modulation
symbol, to an effective energy per bit that also carries a per-rate term.

That term is the net of the coding gain of Clause 15 and the implementation loss of a real
receiver, and it is solved at run time rather than assumed: two published sensitivities of the
Qorvo DW1000, -105 dBm at 0.11 Mb/s and -93 dBm at 6.81 Mb/s, both at 1 % packet error rate on a
127 octet frame, fix it exactly, and the remaining two rates sit on the straight line those
anchors define against the logarithm of the data rate. The test suite checks that the anchors
come back.

Timestamps and ranging
++++++++++++++++++++++

Every frame carries a marker, the first pulse of the physical layer header, and it is the instant
both ends of a ranging exchange refer to. The transmitter knows its marker exactly, because it
launches the frame on a counter edge. ``UwbPhy::ScheduleTx`` is the delayed transmission that
places a marker at a chosen counter value, which is what lets a device put its own transmit
timestamp inside the frame it is about to send; no double-sided exchange works without it.

The receiver has to estimate the marker from a signal below the noise floor, and the error it
makes is the reason UWB ranging is accurate to centimetres rather than to millimetres. The model
draws that error from the Cramer-Rao bound for time of arrival estimation,

.. math::

   \sigma = \frac{1}{2 \pi \beta \sqrt{2\,\mathrm{SNR}}}

where :math:`\beta` is the root mean square bandwidth of the channel and the signal to noise
ratio is the one after the preamble correlator, scaled by a factor for the gap between a real
leading edge detector and the bound and floored at the residual a calibrated radio still shows at
close range.

``uwb-ranging.h`` holds the estimators themselves, as free functions that take the six counter
readings an exchange produces: ``SolveSsTwrRange`` for the two-message scheme,
``SolveDsTwrRange`` for the three-message one, and ``SolveTdoa`` for the hyperbolic system that
arrival differences give.

Medium access and ranging exchanges
+++++++++++++++++++++++++++++++++++

``UwbMac`` uses unslotted ALOHA, because a UWB signal sits below the noise floor and a carrier
sense would have nothing to hear; IEEE Std 802.15.4 permits exactly this for UWB devices. It
carries acknowledged data frames with the MAC header and frame check sequence of the standard,
and it runs the ranging state machines: poll and response for a single-sided exchange, poll,
response and final for a double-sided one, with an optional report frame that tells the initiator
what the responder computed, and a blink for time difference of arrival.

``UwbTdoaEngine`` is the infrastructure side of that last scheme. It collects the counter
readings each anchor took, resolves them against the wrap of the 40-bit counter, brings them onto
a common time base and solves the hyperbolic system. A ``SyncError`` attribute adds whatever
residual disagreement the infrastructure has left between the anchor clocks, which is not a
detail: light travels thirty centimetres in a nanosecond, so a nanosecond of disagreement moves
the answer by about that much however good the radios are.

Scope and Limitations
*********************

The propagation is whatever loss model the script installs, so the multipath that dominates real
indoor UWB accuracy is not modelled unless the script brings it. The leading edge error comes
from a bound that assumes a clean channel, so the centimetres this module reports are line of
sight centimetres; a real non-line-of-sight measurement is biased long by the wall it went
through, and that bias is not here.

The pulse shape is a flat mask with skirts rather than the root raised cosine of the standard.
Scrambling, the Reed-Solomon and convolutional codes, and the preamble codes themselves are
accounted for in timing and in gain but are not simulated bit by bit: frames carry their real
lengths and their real durations, and the frame check sequence is computed for real, but the
coding is a gain figure rather than an encoder. Two overlapping UWB signals with different
preamble codes are separated by a fixed rejection figure rather than by their actual cross
correlation.

Channel 0, which sits in the sub-gigahertz band, and the Low Rate Pulse repetition frequency
(LRP) physical layer are not modelled.

Usage
*****

``UwbHelper`` builds a radio, a crystal, a medium access layer and a network device on every
node, wires them together and attaches the radio to a shared spectrum channel::

    EnableUwbTimeResolution();

    UwbHelper uwb;
    uwb.SetChannelNumber(5);
    uwb.SetDataRate(UwbDataRate::RATE_6M81);
    uwb.SetPreambleSymbols(128);
    uwb.SetChannel(UwbHelper::CreateChannel(5));
    NetDeviceContainer devices = uwb.Install(nodes);
    uwb.AssignStreams(devices, 1);

    auto initiator = DynamicCast<UwbNetDevice>(devices.Get(0));
    initiator->SetRangingResultCallback(MakeCallback(&OnRange));
    initiator->StartRanging(Mac16Address::ConvertFrom(devices.Get(1)->GetAddress()),
                            UwbRangingMethod::DS_TWR);

``UwbHelper::CreateChannel`` exists for a reason worth knowing. ns-3 evaluates a propagation loss
model at the frequency the model is configured with rather than at the frequency of the signal,
so a Friis model left at its default would compute the loss of a 5.15 GHz link for a channel
sitting at 6.5 or 8 GHz, and every received power would be two or three dB out. The helper sets
the frequency from the channel in use. It also gives every device a crystal offset drawn from a
random variable, 20 parts per million wide by default.

Helpers
+++++++

``UwbHelper::CreateTdoaEngine`` registers a set of installed devices as anchors of a
``UwbTdoaEngine``, taking each anchor position from the mobility model of its node. Packet
captures are written with ``EnablePcap`` in the IEEE 802.15.4 link type, so Wireshark decodes the
MAC headers directly.

Examples
++++++++

Five examples live in ``contrib/uwb/examples``:
``uwb-ranging-comparison`` measures the two two-way schemes side by side against crystal offset
and turnaround; ``uwb-link-budget`` finds the range of each data rate and shows where the
preamble rather than the payload becomes the limit; ``uwb-positioning`` walks a tag through a
room and locates it both ways; ``uwb-tag-capacity`` fills a channel with tags; and
``uwb-wifi-coexistence`` shares 6.5 GHz with a Wi-Fi 6E network.

Validation
**********

``./test.py -s uwb`` runs eleven test cases. They check the channel plan against the 499.2 MHz
raster and the frame timings against the symbol durations of Clause 15; that the transmit mask
conserves power and sits on the regulatory density on every channel; that the modelled
sensitivities reproduce the two published anchors exactly and that the interpolated rates stay
ordered; that the device counter converts, wraps and resolves correctly and that two crystals 40
ppm apart disagree by 12 ns over a 300 microsecond turnaround; that a link delivers the free
space power budget and reaches the distance the sensitivity implies; that the arrival timestamps
of a ten metre link are unbiased with a spread of a few centimetres; that a strong interferer on
the same preamble code destroys a reception while the same interferer on a different code does
not; that the estimators recover an exact range between perfect clocks and that the single-sided
error matches its closed form while the double-sided one cancels; that a full over-the-air
exchange reproduces both of those; and that four synchronised anchors place a tag inside a metre
while a nanosecond of clock error costs several times that.
