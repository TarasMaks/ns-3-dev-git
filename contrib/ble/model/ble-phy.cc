/*
 * Copyright (c) 2026
 *
 * SPDX-License-Identifier: GPL-2.0-only
 */

#include "ble-phy.h"

#include "ble-spectrum-signal-parameters.h"
#include "ble-spectrum-value-helper.h"
#include "ble-utils.h"

#include "ns3/antenna-model.h"
#include "ns3/boolean.h"
#include "ns3/double.h"
#include "ns3/enum.h"
#include "ns3/isotropic-antenna-model.h"
#include "ns3/log.h"
#include "ns3/pointer.h"
#include "ns3/simulator.h"
#include "ns3/uinteger.h"

#include <cmath>

namespace ns3
{

NS_LOG_COMPONENT_DEFINE("BlePhy");

namespace ble
{

NS_OBJECT_ENSURE_REGISTERED(BlePhy);

const char*
BlePhyStateName(BlePhyState state)
{
    switch (state)
    {
    case BlePhyState::OFF:
        return "OFF";
    case BlePhyState::IDLE:
        return "IDLE";
    case BlePhyState::RX:
        return "RX";
    case BlePhyState::TX:
        return "TX";
    default:
        return "invalid";
    }
}

TypeId
BlePhy::GetTypeId()
{
    static TypeId tid =
        TypeId("ns3::ble::BlePhy")
            .SetParent<SpectrumPhy>()
            .SetGroupName("Ble")
            .AddConstructor<BlePhy>()
            .AddAttribute("PhyMode",
                          "The LE PHY used to transmit and to receive.",
                          EnumValue(BlePhyMode::LE_1M),
                          MakeEnumAccessor<BlePhyMode>(&BlePhy::SetPhyMode, &BlePhy::GetPhyMode),
                          MakeEnumChecker(BlePhyMode::LE_1M,
                                          "LE_1M",
                                          BlePhyMode::LE_2M,
                                          "LE_2M",
                                          BlePhyMode::LE_CODED_S2,
                                          "LE_CODED_S2",
                                          BlePhyMode::LE_CODED_S8,
                                          "LE_CODED_S8"))
            .AddAttribute("TxPower",
                          "The transmit power, in dBm. The specification allows -20 dBm to "
                          "+10 dBm for LE power class 2 and up to +20 dBm for class 1.",
                          DoubleValue(0.0),
                          MakeDoubleAccessor(&BlePhy::SetTxPowerDbm, &BlePhy::GetTxPowerDbm),
                          MakeDoubleChecker<double>(-40.0, 20.0))
            .AddAttribute("NoiseFigure",
                          "The noise figure of the receiver, in dB.",
                          DoubleValue(8.0),
                          MakeDoubleAccessor(&BlePhy::m_noiseFigureDb),
                          MakeDoubleChecker<double>(0.0, 30.0))
            .AddAttribute("RxSensitivity",
                          "The power below which a packet is not demodulated at all, in dBm. "
                          "Packets above it may still fail, depending on the signal to noise "
                          "ratio.",
                          DoubleValue(-105.0),
                          MakeDoubleAccessor(&BlePhy::m_rxSensitivityDbm),
                          MakeDoubleChecker<double>(-130.0, 0.0))
            .AddAttribute("ErrorModel",
                          "The model that turns signal to noise ratios into bit errors.",
                          PointerValue(),
                          MakePointerAccessor(&BlePhy::SetErrorModel, &BlePhy::GetErrorModel),
                          MakePointerChecker<BleErrorModel>())
            .AddTraceSource("PhyTxBegin",
                            "A packet starts being transmitted.",
                            MakeTraceSourceAccessor(&BlePhy::m_phyTxBeginTrace),
                            "ns3::ble::BlePhy::PacketChannelPowerTracedCallback")
            .AddTraceSource("PhyTxEnd",
                            "A packet finishes being transmitted.",
                            MakeTraceSourceAccessor(&BlePhy::m_phyTxEndTrace),
                            "ns3::Packet::TracedCallback")
            .AddTraceSource("PhyRxBegin",
                            "A packet starts being demodulated.",
                            MakeTraceSourceAccessor(&BlePhy::m_phyRxBeginTrace),
                            "ns3::ble::BlePhy::PacketChannelPowerTracedCallback")
            .AddTraceSource("PhyRxEnd",
                            "A packet is received without error.",
                            MakeTraceSourceAccessor(&BlePhy::m_phyRxEndTrace),
                            "ns3::ble::BlePhy::PacketPowerTracedCallback")
            .AddTraceSource("PhyRxDrop",
                            "A packet is dropped by the physical layer.",
                            MakeTraceSourceAccessor(&BlePhy::m_phyRxDropTrace),
                            "ns3::ble::BlePhy::PacketPowerReasonTracedCallback")
            .AddTraceSource("State",
                            "The state of the radio changes.",
                            MakeTraceSourceAccessor(&BlePhy::m_stateTrace),
                            "ns3::ble::BlePhy::StateTracedCallback");
    return tid;
}

BlePhy::BlePhy()
    : m_mode(BlePhyMode::LE_1M),
      m_rfChannel(ADV_CHANNEL_37),
      m_accessAddress(ADV_ACCESS_ADDRESS),
      m_txPowerDbm(0.0),
      m_noiseFigureDb(8.0),
      m_rxSensitivityDbm(-105.0),
      m_rxEnabled(false),
      m_state(BlePhyState::OFF),
      m_rxPowerW(0.0)
{
    NS_LOG_FUNCTION(this);
    m_antenna = CreateObject<IsotropicAntennaModel>();
    m_errorModel = CreateObject<BleErrorModel>();
    m_random = CreateObject<UniformRandomVariable>();
    UpdateNoise();
}

BlePhy::~BlePhy()
{
    NS_LOG_FUNCTION(this);
}

void
BlePhy::DoInitialize()
{
    NS_LOG_FUNCTION(this);
    UpdateNoise();
    SpectrumPhy::DoInitialize();
}

void
BlePhy::DoDispose()
{
    NS_LOG_FUNCTION(this);
    m_endTxEvent.Cancel();
    m_endRxEvent.Cancel();
    m_device = nullptr;
    m_mobility = nullptr;
    m_channel = nullptr;
    m_antenna = nullptr;
    m_errorModel = nullptr;
    m_noisePsd = nullptr;
    m_random = nullptr;
    m_receiveOkCallback.Nullify();
    m_receiveErrorCallback.Nullify();
    m_txEndCallback.Nullify();
    m_interference.Clear();
    SpectrumPhy::DoDispose();
}

void
BlePhy::SetDevice(Ptr<NetDevice> device)
{
    m_device = device;
}

Ptr<NetDevice>
BlePhy::GetDevice() const
{
    return m_device;
}

void
BlePhy::SetMobility(Ptr<MobilityModel> mobility)
{
    m_mobility = mobility;
}

Ptr<MobilityModel>
BlePhy::GetMobility() const
{
    return m_mobility;
}

void
BlePhy::SetChannel(Ptr<SpectrumChannel> channel)
{
    NS_LOG_FUNCTION(this << channel);
    m_channel = channel;
    if (m_channel)
    {
        m_channel->AddRx(this);
    }
}

Ptr<const SpectrumModel>
BlePhy::GetRxSpectrumModel() const
{
    return BleSpectrumValueHelper::GetSpectrumModel();
}

Ptr<Object>
BlePhy::GetAntenna() const
{
    return m_antenna;
}

void
BlePhy::SetPhyMode(BlePhyMode mode)
{
    NS_LOG_FUNCTION(this << BlePhyModeName(mode));
    m_mode = mode;
    UpdateNoise();
}

BlePhyMode
BlePhy::GetPhyMode() const
{
    return m_mode;
}

void
BlePhy::SetTxPowerDbm(double txPowerDbm)
{
    m_txPowerDbm = txPowerDbm;
}

double
BlePhy::GetTxPowerDbm() const
{
    return m_txPowerDbm;
}

void
BlePhy::SetRfChannel(uint8_t channel)
{
    NS_LOG_FUNCTION(this << +channel);
    NS_ABORT_MSG_IF(channel >= N_RF_CHANNELS, "Invalid channel " << +channel);
    if (channel == m_rfChannel)
    {
        return;
    }
    // retuning aborts any reception in progress, as a real radio would
    if (m_state == BlePhyState::RX)
    {
        m_endRxEvent.Cancel();
        m_phyRxDropTrace(nullptr, 0.0, "channel-switch");
        const auto previous = m_state;
        m_state = m_rxEnabled ? BlePhyState::IDLE : BlePhyState::OFF;
        m_stateTrace(previous, m_state);
    }
    m_rfChannel = channel;
}

uint8_t
BlePhy::GetRfChannel() const
{
    return m_rfChannel;
}

void
BlePhy::SetAccessAddress(uint32_t accessAddress)
{
    m_accessAddress = accessAddress;
}

uint32_t
BlePhy::GetAccessAddress() const
{
    return m_accessAddress;
}

void
BlePhy::SetRxEnabled(bool enabled)
{
    NS_LOG_FUNCTION(this << enabled);
    if (enabled == m_rxEnabled)
    {
        return;
    }
    m_rxEnabled = enabled;
    const auto previous = m_state;
    if (!enabled)
    {
        if (m_state == BlePhyState::RX)
        {
            m_endRxEvent.Cancel();
            m_phyRxDropTrace(nullptr, 0.0, "rx-window-closed");
        }
        if (m_state != BlePhyState::TX)
        {
            m_state = BlePhyState::OFF;
        }
    }
    else if (m_state == BlePhyState::OFF)
    {
        m_state = BlePhyState::IDLE;
    }
    if (previous != m_state)
    {
        m_stateTrace(previous, m_state);
    }
}

bool
BlePhy::IsRxEnabled() const
{
    return m_rxEnabled;
}

BlePhyState
BlePhy::GetState() const
{
    return m_state;
}

void
BlePhy::SetErrorModel(Ptr<BleErrorModel> errorModel)
{
    if (errorModel)
    {
        m_errorModel = errorModel;
    }
}

Ptr<BleErrorModel>
BlePhy::GetErrorModel() const
{
    return m_errorModel;
}

void
BlePhy::SetReceiveOkCallback(ReceiveOkCallback callback)
{
    m_receiveOkCallback = callback;
}

void
BlePhy::SetReceiveErrorCallback(ReceiveErrorCallback callback)
{
    m_receiveErrorCallback = callback;
}

void
BlePhy::SetTxEndCallback(TxEndCallback callback)
{
    m_txEndCallback = callback;
}

int64_t
BlePhy::AssignStreams(int64_t stream)
{
    m_random->SetStream(stream);
    return 1;
}

void
BlePhy::UpdateNoise()
{
    m_noisePsd = BleSpectrumValueHelper::CreateNoisePowerSpectralDensity(m_noiseFigureDb);
}

double
BlePhy::GetNoisePowerW() const
{
    if (!m_noisePsd)
    {
        return 0.0;
    }
    return BleSpectrumValueHelper::GetBandPower(m_noisePsd, m_rfChannel, m_mode);
}

double
BlePhy::GetRssiDbm() const
{
    const double powerW = m_interference.GetTotalPowerW(Simulator::Now()) + GetNoisePowerW();
    return (powerW > 0.0) ? 10.0 * std::log10(powerW) + 30.0 : -200.0;
}

Time
BlePhy::CalculateTxDuration(uint32_t pduOctets, BlePhyMode mode)
{
    const double symbolRate = GetSymbolRate(mode);
    double symbols = 0.0;
    if (!IsCoded(mode))
    {
        // preamble, access address, PDU and CRC, one symbol per bit
        symbols = ((mode == BlePhyMode::LE_2M) ? PREAMBLE_SYMBOLS_2M : PREAMBLE_SYMBOLS_1M) +
                  ACCESS_ADDRESS_OCTETS * 8 + pduOctets * 8 + CRC_OCTETS * 8;
    }
    else
    {
        // the preamble is sent uncoded, the access address, the coding indicator and TERM1 are
        // always coded at S=8 and the PDU, the CRC and TERM2 use the spreading of the mode
        const uint32_t spreading = GetSymbolsPerBit(mode);
        const double fec1Bits = ACCESS_ADDRESS_OCTETS * 8 + CODING_INDICATOR_BITS + TERMINATOR_BITS;
        const double fec2Bits = pduOctets * 8 + CRC_OCTETS * 8 + TERMINATOR_BITS;
        symbols =
            PREAMBLE_SYMBOLS_CODED + fec1Bits * CODED_FEC1_SYMBOLS_PER_BIT + fec2Bits * spreading;
    }
    return Seconds(symbols / symbolRate);
}

uint64_t
BlePhy::CalculatePduBits(uint32_t pduOctets)
{
    // the error model is applied to the bits protected by the CRC
    return static_cast<uint64_t>(pduOctets + CRC_OCTETS) * 8;
}

double
BlePhy::CalculateSensitivityDbm(BlePhyMode mode) const
{
    const double noiseW = BleSpectrumValueHelper::GetBandPower(m_noisePsd, m_rfChannel, mode);
    if (noiseW <= 0.0)
    {
        return 0.0;
    }
    const uint64_t bits = CalculatePduBits(PDU_HEADER_OCTETS + REFERENCE_SENSITIVITY_PAYLOAD);
    // bisect on the received power that gives the reference packet error rate
    double lowDbm = -140.0;
    double highDbm = 20.0;
    for (uint32_t i = 0; i < 200; ++i)
    {
        const double middleDbm = 0.5 * (lowDbm + highDbm);
        const double signalW = std::pow(10.0, (middleDbm - 30.0) / 10.0);
        const double per = 1.0 - m_errorModel->GetChunkSuccessRate(signalW / noiseW, mode, bits);
        if (per > REFERENCE_SENSITIVITY_PER)
        {
            lowDbm = middleDbm;
        }
        else
        {
            highDbm = middleDbm;
        }
    }
    return 0.5 * (lowDbm + highDbm);
}

bool
BlePhy::StartTx(Ptr<Packet> packet)
{
    NS_LOG_FUNCTION(this << packet << +m_rfChannel << BlePhyModeName(m_mode));
    NS_ABORT_MSG_IF(!m_channel, "The PHY is not attached to a spectrum channel");
    if (m_state == BlePhyState::TX)
    {
        NS_LOG_DEBUG("Already transmitting, the request is refused");
        return false;
    }

    // a transmission aborts any reception in progress
    if (m_state == BlePhyState::RX)
    {
        m_endRxEvent.Cancel();
        m_phyRxDropTrace(nullptr, 0.0, "tx-while-rx");
    }

    const auto previous = m_state;
    m_state = BlePhyState::TX;
    m_stateTrace(previous, m_state);

    const Time duration = CalculateTxDuration(packet->GetSize() - CRC_OCTETS, m_mode);
    auto params = Create<BleSpectrumSignalParameters>();
    params->duration = duration;
    params->txPhy = this;
    params->txAntenna = m_antenna;
    params->psd =
        BleSpectrumValueHelper::CreateTxPowerSpectralDensity(m_txPowerDbm, m_rfChannel, m_mode);
    params->packet = packet->Copy();
    params->mode = m_mode;
    params->channel = m_rfChannel;
    params->accessAddress = m_accessAddress;

    m_phyTxBeginTrace(packet, m_rfChannel, m_txPowerDbm);
    m_channel->StartTx(params);
    m_endTxEvent = Simulator::Schedule(duration, &BlePhy::EndTx, this);
    return true;
}

void
BlePhy::EndTx()
{
    NS_LOG_FUNCTION(this);
    const auto previous = m_state;
    m_state = m_rxEnabled ? BlePhyState::IDLE : BlePhyState::OFF;
    m_stateTrace(previous, m_state);
    m_phyTxEndTrace(nullptr);
    if (!m_txEndCallback.IsNull())
    {
        m_txEndCallback();
    }
}

void
BlePhy::StartRx(Ptr<SpectrumSignalParameters> params)
{
    NS_LOG_FUNCTION(this << params);
    const Time now = Simulator::Now();
    m_interference.Cleanup(now - Seconds(1));

    auto bleParams = DynamicCast<BleSpectrumSignalParameters>(params);
    // every signal, whether it is BLE or not, contributes to the interference in the band
    const double rxPowerW = BleSpectrumValueHelper::GetBandPower(params->psd, m_rfChannel, m_mode);
    m_interference.AddSignal(now, now + params->duration, rxPowerW);

    if (!bleParams)
    {
        NS_LOG_DEBUG("A non-BLE signal of " << rxPowerW << " W adds to the interference");
        return;
    }

    const double rxPowerDbm = (rxPowerW > 0.0) ? 10.0 * std::log10(rxPowerW) + 30.0 : -200.0;

    if (!m_rxEnabled || m_state == BlePhyState::OFF)
    {
        NS_LOG_DEBUG("The receive window is closed");
        return;
    }
    if (m_state == BlePhyState::TX)
    {
        m_phyRxDropTrace(bleParams->packet, rxPowerDbm, "busy-tx");
        return;
    }
    if (m_state == BlePhyState::RX)
    {
        m_phyRxDropTrace(bleParams->packet, rxPowerDbm, "busy-rx");
        return;
    }
    if (bleParams->channel != m_rfChannel)
    {
        NS_LOG_DEBUG("The signal is on channel " << +bleParams->channel << ", the radio is on "
                                                 << +m_rfChannel);
        return;
    }
    if (bleParams->mode != m_mode)
    {
        m_phyRxDropTrace(bleParams->packet, rxPowerDbm, "phy-mode-mismatch");
        return;
    }
    if (bleParams->accessAddress != m_accessAddress)
    {
        NS_LOG_DEBUG("The access address does not match");
        return;
    }
    if (rxPowerDbm < m_rxSensitivityDbm)
    {
        m_phyRxDropTrace(bleParams->packet, rxPowerDbm, "below-sensitivity");
        return;
    }

    const auto previous = m_state;
    m_state = BlePhyState::RX;
    m_stateTrace(previous, m_state);
    m_rxStart = now;
    m_rxPowerW = rxPowerW;
    m_phyRxBeginTrace(bleParams->packet, bleParams->channel, rxPowerDbm);
    m_endRxEvent =
        Simulator::Schedule(params->duration, &BlePhy::EndRx, this, bleParams->packet, rxPowerW);
}

void
BlePhy::EndRx(Ptr<Packet> packet, double rxPowerW)
{
    NS_LOG_FUNCTION(this << packet << rxPowerW);
    const Time now = Simulator::Now();
    const double rxPowerDbm = (rxPowerW > 0.0) ? 10.0 * std::log10(rxPowerW) + 30.0 : -200.0;

    const auto previous = m_state;
    m_state = m_rxEnabled ? BlePhyState::IDLE : BlePhyState::OFF;
    m_stateTrace(previous, m_state);

    const uint32_t pduOctets = packet->GetSize() - CRC_OCTETS;
    const uint64_t bits = CalculatePduBits(pduOctets);
    const double success = m_interference.CalculateSuccessRate(m_errorModel,
                                                               rxPowerW,
                                                               GetNoisePowerW(),
                                                               m_rxStart,
                                                               now,
                                                               m_mode,
                                                               bits);
    if (m_random->GetValue() < success)
    {
        NS_LOG_DEBUG("Received a packet of " << pduOctets << " octets at " << rxPowerDbm
                                             << " dBm, success probability " << success);
        m_phyRxEndTrace(packet, rxPowerDbm);
        if (!m_receiveOkCallback.IsNull())
        {
            m_receiveOkCallback(packet, rxPowerDbm, m_rfChannel);
        }
        return;
    }

    NS_LOG_DEBUG("A packet was corrupted, success probability " << success);
    m_phyRxDropTrace(packet, rxPowerDbm, "crc-error");
    if (!m_receiveErrorCallback.IsNull())
    {
        m_receiveErrorCallback(packet, rxPowerDbm);
    }
}

} // namespace ble
} // namespace ns3
