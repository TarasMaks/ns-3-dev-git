/*
 * Copyright (c) 2026
 *
 * SPDX-License-Identifier: GPL-2.0-only
 */

#ifndef BLE_PHY_H
#define BLE_PHY_H

#include "ble-constants.h"
#include "ble-error-model.h"
#include "ble-interference-helper.h"

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

namespace ble
{

/**
 * @ingroup ble
 * The states of a BLE physical layer.
 *
 * A BLE radio never listens continuously: the Link Layer opens a receive window before an event
 * and closes it afterwards, so a PHY that is neither transmitting nor demodulating is idle only
 * if its receive window is open, and off otherwise.
 */
enum class BlePhyState : uint8_t
{
    OFF = 0, //!< the radio is not listening and not transmitting
    IDLE,    //!< the receive window is open and no packet is being demodulated
    RX,      //!< a packet is being demodulated
    TX,      //!< a packet is being transmitted
};

/**
 * @ingroup ble
 * @param state a PHY state
 * @return a printable name
 */
const char* BlePhyStateName(BlePhyState state);

/**
 * @ingroup ble
 * The physical layer of a BLE device.
 *
 * The PHY transmits and receives over a spectrum channel, so BLE signals interfere with every
 * other 2.4 GHz technology of ns-3 that uses the spectrum framework. It implements the four LE
 * PHYs of Bluetooth Core Specification 5.x: LE 1M, LE 2M, LE Coded S=2 and LE Coded S=8.
 *
 * A packet is demodulated when its power reaches the sensitivity of the receiver, when it
 * arrives on the channel the PHY is tuned to, when it carries the access address the PHY is
 * listening for and when the PHY is idle with its receive window open. Whether it is delivered
 * to the Link Layer then depends on the signal to interference and noise ratio over the
 * reception, evaluated by the error model.
 */
class BlePhy : public SpectrumPhy
{
  public:
    /// Signature of the callback invoked when a packet is received without error.
    using ReceiveOkCallback = Callback<void, Ptr<Packet>, double, uint8_t>;
    /// Signature of the callback invoked when a reception fails.
    using ReceiveErrorCallback = Callback<void, Ptr<const Packet>, double>;
    /// Signature of the callback invoked when a transmission ends.
    using TxEndCallback = Callback<void>;

    /// @return the TypeId
    static TypeId GetTypeId();

    BlePhy();
    ~BlePhy() override;

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
     * @param mode the PHY mode used to transmit and to receive
     */
    void SetPhyMode(BlePhyMode mode);
    /// @return the PHY mode
    BlePhyMode GetPhyMode() const;

    /**
     * @param txPowerDbm the transmit power, in dBm
     */
    void SetTxPowerDbm(double txPowerDbm);
    /// @return the transmit power, in dBm
    double GetTxPowerDbm() const;

    /**
     * Tune the radio to a channel.
     * @param channel the logical channel index (0-39)
     */
    void SetRfChannel(uint8_t channel);
    /// @return the logical channel index the radio is tuned to
    uint8_t GetRfChannel() const;

    /**
     * @param accessAddress the access address the receiver correlates on
     */
    void SetAccessAddress(uint32_t accessAddress);
    /// @return the access address the receiver correlates on
    uint32_t GetAccessAddress() const;

    /**
     * Open or close the receive window.
     * @param enabled whether the radio listens
     */
    void SetRxEnabled(bool enabled);
    /// @return whether the receive window is open
    bool IsRxEnabled() const;

    /// @return the state of the radio
    BlePhyState GetState() const;

    /**
     * Transmit a packet on the channel the radio is tuned to.
     *
     * @param packet the packet, made of the PDU header, the payload and the CRC
     * @return true if the transmission started, false if the radio was busy
     */
    bool StartTx(Ptr<Packet> packet);

    /**
     * The duration of a packet on the air, including the preamble, the access address, the
     * header, the payload and the CRC (Vol 6, Part B, Section 2.1).
     *
     * @param pduOctets the number of octets of the PDU, that is its header and its payload
     * @param mode the PHY mode
     * @return the duration
     */
    static Time CalculateTxDuration(uint32_t pduOctets, BlePhyMode mode);

    /**
     * The number of information bits a reception carries, used to weigh the error model.
     *
     * @param pduOctets the number of octets of the PDU
     * @return the number of bits of the PDU and of the CRC
     */
    static uint64_t CalculatePduBits(uint32_t pduOctets);

    /**
     * The sensitivity of the receiver, that is the power at which a reference packet of 37
     * octets of payload is received with the packet error rate the specification uses to
     * define it.
     *
     * @param mode the PHY mode
     * @return the sensitivity, in dBm
     */
    double CalculateSensitivityDbm(BlePhyMode mode) const;

    /// @return the noise power in the receiver bandwidth, in watt
    double GetNoisePowerW() const;

    /// @return the total power currently present in the receiver bandwidth, in dBm
    double GetRssiDbm() const;

    /**
     * @param callback invoked with the packet, its received power in dBm and its channel when a
     *                 reception succeeds
     */
    void SetReceiveOkCallback(ReceiveOkCallback callback);

    /**
     * @param callback invoked with the packet and its received power in dBm when a reception
     *                 fails
     */
    void SetReceiveErrorCallback(ReceiveErrorCallback callback);

    /**
     * @param callback invoked when a transmission ends
     */
    void SetTxEndCallback(TxEndCallback callback);

    /**
     * @param errorModel the error model used to turn signal to noise ratios into errors
     */
    void SetErrorModel(Ptr<BleErrorModel> errorModel);
    /// @return the error model
    Ptr<BleErrorModel> GetErrorModel() const;

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
    /// Complete a transmission.
    void EndTx();

    /**
     * Complete a reception and decide whether the packet is delivered.
     * @param packet the packet
     * @param rxPowerW the power of the signal, in watt
     */
    void EndRx(Ptr<Packet> packet, double rxPowerW);

    /// Rebuild the noise power spectral density after a change of noise figure or mode.
    void UpdateNoise();

    Ptr<NetDevice> m_device;         //!< the device this PHY belongs to
    Ptr<MobilityModel> m_mobility;   //!< the mobility model of the node
    Ptr<SpectrumChannel> m_channel;  //!< the spectrum channel
    Ptr<AntennaModel> m_antenna;     //!< the antenna model
    Ptr<BleErrorModel> m_errorModel; //!< the error model

    BlePhyMode m_mode;         //!< the PHY mode
    uint8_t m_rfChannel;       //!< the logical channel the radio is tuned to
    uint32_t m_accessAddress;  //!< the access address the receiver correlates on
    double m_txPowerDbm;       //!< the transmit power
    double m_noiseFigureDb;    //!< the receiver noise figure
    double m_rxSensitivityDbm; //!< the power below which a packet is not demodulated
    bool m_rxEnabled;          //!< whether the receive window is open

    BlePhyState m_state;                  //!< the state of the radio
    BleInterferenceHelper m_interference; //!< the signals seen by the receiver
    Ptr<SpectrumValue> m_noisePsd;        //!< the noise power spectral density
    Time m_rxStart;                       //!< when the current reception started
    double m_rxPowerW;                    //!< the power of the current reception
    EventId m_endTxEvent;                 //!< the event that ends the current transmission
    EventId m_endRxEvent;                 //!< the event that ends the current reception

    ReceiveOkCallback m_receiveOkCallback;       //!< invoked on a successful reception
    ReceiveErrorCallback m_receiveErrorCallback; //!< invoked on a failed reception
    TxEndCallback m_txEndCallback;               //!< invoked when a transmission ends

    Ptr<UniformRandomVariable> m_random; //!< used to draw the outcome of a reception

    /// Traced when a transmission starts, with the packet, the channel and the power in dBm.
    TracedCallback<Ptr<const Packet>, uint8_t, double> m_phyTxBeginTrace;
    /// Traced when a transmission ends.
    TracedCallback<Ptr<const Packet>> m_phyTxEndTrace;
    /// Traced when a reception starts, with the packet, the channel and the power in dBm.
    TracedCallback<Ptr<const Packet>, uint8_t, double> m_phyRxBeginTrace;
    /// Traced when a reception succeeds, with the packet and the power in dBm.
    TracedCallback<Ptr<const Packet>, double> m_phyRxEndTrace;
    /// Traced when a reception fails, with the packet, the power in dBm and the reason.
    TracedCallback<Ptr<const Packet>, double, std::string> m_phyRxDropTrace;
    /// Traced on every state change, with the previous and the new state.
    TracedCallback<BlePhyState, BlePhyState> m_stateTrace;
};

} // namespace ble
} // namespace ns3

#endif /* BLE_PHY_H */
