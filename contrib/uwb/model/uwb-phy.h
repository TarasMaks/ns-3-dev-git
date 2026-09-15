/*
 * Copyright (c) 2026
 *
 * SPDX-License-Identifier: GPL-2.0-only
 */

#ifndef UWB_PHY_H
#define UWB_PHY_H

#include "uwb-clock-model.h"
#include "uwb-error-model.h"
#include "uwb-interference-helper.h"
#include "uwb-spectrum-signal-parameters.h"

#include "ns3/event-id.h"
#include "ns3/mobility-model.h"
#include "ns3/net-device.h"
#include "ns3/packet.h"
#include "ns3/spectrum-channel.h"
#include "ns3/spectrum-phy.h"
#include "ns3/traced-callback.h"

namespace ns3
{

class AntennaModel;

namespace uwb
{

/**
 * @ingroup uwb
 * The states of a UWB physical layer.
 *
 * A UWB frame is acquired before it is demodulated, and the two take measurably different
 * amounts of time: the preamble of a long range configuration lasts longer than the payload it
 * precedes. The radio therefore has a state for each.
 */
enum class UwbPhyState : uint8_t
{
    OFF = 0, //!< the radio is neither listening nor transmitting
    IDLE,    //!< the radio is listening and has not locked onto anything
    SYNC,    //!< the radio is correlating against the preamble of a frame
    RX,      //!< the radio is demodulating the payload of a frame
    TX,      //!< the radio is transmitting
};

/**
 * @ingroup uwb
 * @param state a PHY state
 * @return a printable name
 */
const char* UwbPhyStateName(UwbPhyState state);

/**
 * @ingroup uwb
 * What a receiver learned about a frame it took in.
 */
struct UwbRxInfo
{
    double rxPowerDbm{0.0};     //!< the power of the signal in the channel, in dBm
    double sinrDb{0.0};         //!< the signal to interference and noise ratio, in dB
    double ebNoDb{0.0};         //!< the energy per bit over noise density, in dB
    Time arrival{Time(0)};      //!< the instant of simulated time at which the marker arrived
    uint64_t rxTimestamp{0};    //!< the reading of the local counter at the marker
    double timestampErrorS{0.0}; //!< the error the leading edge estimate made, in seconds
    UwbPhyConfig config;        //!< the configuration the frame was sent with
    uint8_t preambleCode{9};    //!< the preamble code of the frame
    bool ranging{false};        //!< whether the sender asked for the arrival to be timestamped
};

/**
 * @ingroup uwb
 * The physical layer of a UWB device.
 *
 * The PHY transmits and receives over a spectrum channel, so a UWB signal interferes with
 * everything else in the band, and everything else interferes with it. Because a UWB channel is
 * 499.2 MHz wide and sits at 6.5 or 8 GHz, that includes the 5 GHz and 6 GHz Wi-Fi models of
 * ns-3 when they share a MultiModelSpectrumChannel.
 *
 * Receiving a frame has two stages, as it does in hardware. First the correlator has to lock
 * onto the preamble, which accumulates over the whole synchronisation header and either clears
 * the acquisition threshold or does not. Then the payload is demodulated, and whether it
 * survives depends on the signal to interference and noise ratio over its duration. A frame
 * whose preamble the receiver missed is not a frame with errors; it is a frame the receiver
 * never knew about, and the model distinguishes the two.
 *
 * Every frame carries a marker, the first pulse of the physical layer header, which is the
 * instant both ends of a ranging exchange refer to. The transmitter knows its marker exactly,
 * because it launches the frame on a counter edge. The receiver has to estimate it from a
 * signal below the noise floor, and the error it makes is the reason UWB ranging is accurate to
 * centimetres rather than to millimetres. The model draws that error from the Cramer-Rao bound
 * for time of arrival estimation,
 *
 *     sigma = 1 / (2 pi beta sqrt(2 SNR)),
 *
 * where beta is the root mean square bandwidth of the channel and SNR is the signal to noise
 * ratio after the preamble correlator, scaled by a factor for the gap between a real leading
 * edge detector and the bound, and floored at the residual a calibrated radio still shows at
 * close range.
 *
 * A transmission can be scheduled rather than started, which is what real ranging firmware
 * does: the marker is placed at a counter value chosen in advance, so the transmitter can put
 * its own transmit timestamp inside the frame it is about to send. ScheduleTx does that.
 */
class UwbPhy : public SpectrumPhy
{
  public:
    /// Signature of the callback invoked when a frame is received without error.
    using ReceiveOkCallback = Callback<void, Ptr<Packet>, const UwbRxInfo&>;
    /// Signature of the callback invoked when a reception fails.
    using ReceiveErrorCallback = Callback<void, Ptr<const Packet>, const UwbRxInfo&>;
    /// Signature of the callback invoked when a transmission ends, with the marker timestamp.
    using TxEndCallback = Callback<void, Ptr<const Packet>, uint64_t>;

    /// @return the TypeId
    static TypeId GetTypeId();

    UwbPhy();
    ~UwbPhy() override;

    // inherited from SpectrumPhy
    void SetDevice(Ptr<NetDevice> device) override;
    Ptr<NetDevice> GetDevice() const override;
    void SetMobility(Ptr<MobilityModel> mobility) override;
    Ptr<MobilityModel> GetMobility() const override;
    void SetChannel(Ptr<SpectrumChannel> channel) override;
    Ptr<const SpectrumModel> GetRxSpectrumModel() const override;
    Ptr<Object> GetAntenna() const override;
    void StartRx(Ptr<SpectrumSignalParameters> params) override;

    /**
     * @param config the channel, data rate, pulse repetition frequency and preamble length the
     *               radio transmits and receives with
     */
    void SetConfig(const UwbPhyConfig& config);
    /// @return the link configuration
    const UwbPhyConfig& GetConfig() const;

    /// @param channel the channel index (1 to 15)
    void SetRfChannel(uint8_t channel);
    /// @return the channel index the radio is tuned to
    uint8_t GetRfChannel() const;

    /// @param rate the data rate of the payload
    void SetDataRate(UwbDataRate rate);
    /// @return the data rate of the payload
    UwbDataRate GetDataRate() const;

    /// @param prf the mean pulse repetition frequency
    void SetPrf(UwbPrf prf);
    /// @return the mean pulse repetition frequency
    UwbPrf GetPrf() const;

    /// @param symbols the number of preamble symbols
    void SetPreambleSymbols(uint32_t symbols);
    /// @return the number of preamble symbols
    uint32_t GetPreambleSymbols() const;

    /// @param code the preamble code index (1 to 24)
    void SetPreambleCode(uint8_t code);
    /// @return the preamble code index
    uint8_t GetPreambleCode() const;

    /// @param txPowerDbm the total transmit power, in dBm
    void SetTxPowerDbm(double txPowerDbm);
    /// @return the total transmit power, in dBm
    double GetTxPowerDbm() const;

    /**
     * Set the transmit power to the highest the regulatory density limit allows on the channel
     * the radio is tuned to, which is what a UWB product does.
     */
    void SetTxPowerToRegulatoryLimit();

    /// @return the mean power density of a transmission, in dBm per megahertz
    double GetPowerDensityDbmPerMhz() const;

    /// @param enabled whether the radio listens
    void SetRxEnabled(bool enabled);
    /// @return whether the radio listens
    bool IsRxEnabled() const;

    /// @return the state of the radio
    UwbPhyState GetState() const;
    /// @return true if the radio is transmitting, acquiring or demodulating
    bool IsBusy() const;

    /**
     * Transmit a frame now.
     *
     * @param packet the frame, including its frame check sequence
     * @param ranging whether the receiver should timestamp the arrival
     * @return true if the transmission started, false if the radio was busy
     */
    bool StartTx(Ptr<Packet> packet, bool ranging = false);

    /**
     * Transmit a frame so that its marker falls at a chosen instant, which lets the sender put
     * its own transmit timestamp inside the frame. This is the delayed transmission that
     * ranging firmware relies on.
     *
     * @param packet the frame, including its frame check sequence
     * @param markerAt the instant of simulated time the marker should fall at
     * @param ranging whether the receiver should timestamp the arrival
     * @return true if the transmission was scheduled, false if the instant has passed or the
     *         radio is busy
     */
    bool ScheduleTx(Ptr<Packet> packet, Time markerAt, bool ranging = true);

    /**
     * The counter reading a transmission whose marker falls at an instant would report,
     * including the antenna delay of this radio.
     *
     * @param markerAt the instant of simulated time
     * @return the counter reading, in device time units
     */
    uint64_t GetTxTimestampFor(Time markerAt) const;

    /// @return the counter reading of the marker of the last frame transmitted
    uint64_t GetLastTxTimestamp() const;

    /**
     * The duration of a frame on the air.
     *
     * @param psduOctets the payload length, in octets, including the frame check sequence
     * @return the duration
     */
    Time CalculateTxDuration(uint32_t psduOctets) const;

    /// @return the duration of the synchronisation header, which precedes the marker
    Time GetShrDuration() const;

    /// @return the sensitivity of the receiver for a reference frame, in dBm
    double GetSensitivityDbm() const;

    /// @return the noise power in the channel, in watt
    double GetNoisePowerW() const;

    /// @return the total power currently present in the channel, in dBm
    double GetRssiDbm() const;

    /// @param callback invoked with the frame and what the receiver learned about it
    void SetReceiveOkCallback(ReceiveOkCallback callback);

    /// @param callback invoked with the frame and what the receiver learned about it
    void SetReceiveErrorCallback(ReceiveErrorCallback callback);

    /// @param callback invoked when a transmission ends
    void SetTxEndCallback(TxEndCallback callback);

    /// @param errorModel the error model used to turn signal to noise ratios into errors
    void SetErrorModel(Ptr<UwbErrorModel> errorModel);
    /// @return the error model
    Ptr<UwbErrorModel> GetErrorModel() const;

    /// @param clock the crystal of this device
    void SetClockModel(Ptr<UwbClockModel> clock);
    /// @return the crystal of this device
    Ptr<UwbClockModel> GetClockModel() const;

    /**
     * Assign the streams of the random variables of this object.
     * @param stream the first stream index to use
     * @return the number of streams used
     */
    int64_t AssignStreams(int64_t stream);

  protected:
    void DoDispose() override;
    void DoInitialize() override;

  private:
    /// Put the frame on the channel, once any scheduling delay has elapsed.
    /// @param packet the frame
    /// @param ranging whether the receiver should timestamp the arrival
    void DoStartTx(Ptr<Packet> packet, bool ranging);

    /// Complete a transmission.
    /// @param packet the frame
    void EndTx(Ptr<const Packet> packet);

    /**
     * Decide whether the correlator locked onto the preamble of a frame.
     *
     * @param packet the frame
     * @param rxPowerW the power of the signal, in watt
     * @param params the signal parameters
     */
    void EndShr(Ptr<Packet> packet, double rxPowerW, Ptr<UwbSpectrumSignalParameters> params);

    /**
     * Complete a reception and decide whether the frame is delivered.
     *
     * @param packet the frame
     * @param info what the receiver learned during acquisition
     * @param payloadStart when the payload started
     */
    void EndRx(Ptr<Packet> packet, UwbRxInfo info, Time payloadStart);

    /**
     * The standard deviation of the leading edge estimate at a given post-correlation signal to
     * noise ratio.
     *
     * @param sinr the signal to interference and noise ratio in the channel, as a linear ratio
     * @return the standard deviation, in seconds
     */
    double GetTimestampSigmaS(double sinr) const;

    /// Move to a state and fire the state trace.
    /// @param state the new state
    void ChangeState(UwbPhyState state);

    /// Rebuild the noise power spectral density after a change of channel or noise figure.
    void UpdateNoise();

    Ptr<NetDevice> m_device;         //!< the device this PHY belongs to
    Ptr<MobilityModel> m_mobility;   //!< the mobility model of the node
    Ptr<SpectrumChannel> m_channel;  //!< the spectrum channel
    Ptr<AntennaModel> m_antenna;     //!< the antenna model
    Ptr<UwbErrorModel> m_errorModel; //!< the error model
    Ptr<UwbClockModel> m_clock;      //!< the crystal of this device

    UwbPhyConfig m_config;   //!< the link configuration
    uint8_t m_preambleCode;  //!< the preamble code the radio uses
    double m_txPowerDbm;     //!< the total transmit power
    bool m_rxEnabled;        //!< whether the radio listens
    Time m_antennaDelay;     //!< the delay between the antenna and the timestamping unit
    double m_rangingNoiseFactor; //!< how far a real leading edge detector sits above the bound
    Time m_rangingNoiseFloor;    //!< the residual timestamp error of a calibrated radio

    UwbPhyState m_state;                  //!< the state of the radio
    UwbInterferenceHelper m_interference; //!< the signals seen by the receiver
    Ptr<SpectrumValue> m_noisePsd;        //!< the noise power spectral density
    double m_noiseW;                      //!< the noise power in the channel
    Time m_rxStart;                       //!< when the current reception started
    uint64_t m_lastTxTimestamp;           //!< the marker of the last frame transmitted
    EventId m_startTxEvent;               //!< the event that starts a scheduled transmission
    EventId m_endTxEvent;                 //!< the event that ends the current transmission
    EventId m_endShrEvent;                //!< the event that ends the current acquisition
    EventId m_endRxEvent;                 //!< the event that ends the current reception

    ReceiveOkCallback m_receiveOkCallback;       //!< invoked on a successful reception
    ReceiveErrorCallback m_receiveErrorCallback; //!< invoked on a failed reception
    TxEndCallback m_txEndCallback;               //!< invoked when a transmission ends

    Ptr<UniformRandomVariable> m_random;   //!< used to draw the outcome of a reception
    Ptr<NormalRandomVariable> m_rangingNoise; //!< used to draw the leading edge error

    /// Traced when a transmission starts, with the frame, the channel and the power in dBm.
    TracedCallback<Ptr<const Packet>, uint8_t, double> m_phyTxBeginTrace;
    /// Traced when a transmission ends, with the frame and the marker timestamp.
    TracedCallback<Ptr<const Packet>, uint64_t> m_phyTxEndTrace;
    /// Traced when the correlator locks onto a preamble, with the frame and the power in dBm.
    TracedCallback<Ptr<const Packet>, uint8_t, double> m_phyRxBeginTrace;
    /// Traced when a reception succeeds, with the frame and what the receiver learned.
    TracedCallback<Ptr<const Packet>, const UwbRxInfo&> m_phyRxEndTrace;
    /// Traced when a reception fails, with the frame, the power in dBm and the reason.
    TracedCallback<Ptr<const Packet>, double, std::string> m_phyRxDropTrace;
    /// Traced on every state change, with the previous and the new state.
    TracedCallback<UwbPhyState, UwbPhyState> m_stateTrace;
};

} // namespace uwb
} // namespace ns3

#endif /* UWB_PHY_H */
