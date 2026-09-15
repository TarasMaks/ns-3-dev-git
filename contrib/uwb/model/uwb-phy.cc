/*
 * Copyright (c) 2026
 *
 * SPDX-License-Identifier: GPL-2.0-only
 */

#include "uwb-phy.h"

#include "uwb-spectrum-value-helper.h"

#include "ns3/abort.h"
#include "ns3/antenna-model.h"
#include "ns3/boolean.h"
#include "ns3/double.h"
#include "ns3/enum.h"
#include "ns3/isotropic-antenna-model.h"
#include "ns3/log.h"
#include "ns3/simulator.h"
#include "ns3/uinteger.h"

#include <cmath>

namespace ns3
{

NS_LOG_COMPONENT_DEFINE("UwbPhy");

namespace uwb
{

const char*
UwbPhyStateName(UwbPhyState state)
{
    switch (state)
    {
    case UwbPhyState::OFF:
        return "OFF";
    case UwbPhyState::IDLE:
        return "IDLE";
    case UwbPhyState::SYNC:
        return "SYNC";
    case UwbPhyState::RX:
        return "RX";
    case UwbPhyState::TX:
        return "TX";
    default:
        return "invalid";
    }
}

NS_OBJECT_ENSURE_REGISTERED(UwbPhy);

TypeId
UwbPhy::GetTypeId()
{
    static TypeId tid =
        TypeId("ns3::uwb::UwbPhy")
            .SetParent<SpectrumPhy>()
            .SetGroupName("Uwb")
            .AddConstructor<UwbPhy>()
            .AddAttribute("RfChannel",
                          "The UWB channel the radio is tuned to. Channels 3 and 9 are the ones "
                          "the standard makes mandatory; most products use 5 and 9.",
                          UintegerValue(5),
                          MakeUintegerAccessor(&UwbPhy::SetRfChannel, &UwbPhy::GetRfChannel),
                          MakeUintegerChecker<uint8_t>(FIRST_CHANNEL, LAST_CHANNEL))
            .AddAttribute("DataRate",
                          "The data rate of the payload.",
                          EnumValue(UwbDataRate::RATE_6M81),
                          MakeEnumAccessor<UwbDataRate>(&UwbPhy::SetDataRate, &UwbPhy::GetDataRate),
                          MakeEnumChecker(UwbDataRate::RATE_110K,
                                          "110kbps",
                                          UwbDataRate::RATE_850K,
                                          "850kbps",
                                          UwbDataRate::RATE_6M81,
                                          "6.81Mbps",
                                          UwbDataRate::RATE_27M24,
                                          "27.24Mbps"))
            .AddAttribute("Prf",
                          "The mean pulse repetition frequency of the preamble.",
                          EnumValue(UwbPrf::PRF_64M),
                          MakeEnumAccessor<UwbPrf>(&UwbPhy::SetPrf, &UwbPhy::GetPrf),
                          MakeEnumChecker(UwbPrf::PRF_16M, "PRF16", UwbPrf::PRF_64M, "PRF64"))
            .AddAttribute("PreambleSymbols",
                          "The number of preamble symbols. Longer preambles buy acquisition "
                          "margin, which is what long range configurations spend their air time "
                          "on: 1024 or 2048 symbols rather than the 64 or 128 of a short range "
                          "link.",
                          UintegerValue(128),
                          MakeUintegerAccessor(&UwbPhy::SetPreambleSymbols,
                                               &UwbPhy::GetPreambleSymbols),
                          MakeUintegerChecker<uint32_t>(16, 4096))
            .AddAttribute("PreambleCode",
                          "The preamble code the radio transmits and correlates against. Two "
                          "links on the same channel with different codes interfere far less "
                          "than two with the same code.",
                          UintegerValue(9),
                          MakeUintegerAccessor(&UwbPhy::SetPreambleCode, &UwbPhy::GetPreambleCode),
                          MakeUintegerChecker<uint8_t>(1, 24))
            .AddAttribute("TxPower",
                          "The total transmit power, in dBm. The default is what the -41.3 "
                          "dBm/MHz regulatory density allows across a 499.2 MHz channel.",
                          DoubleValue(-14.3),
                          MakeDoubleAccessor(&UwbPhy::SetTxPowerDbm, &UwbPhy::GetTxPowerDbm),
                          MakeDoubleChecker<double>(-60.0, 10.0))
            .AddAttribute("RxEnabled",
                          "Whether the radio listens.",
                          BooleanValue(true),
                          MakeBooleanAccessor(&UwbPhy::SetRxEnabled, &UwbPhy::IsRxEnabled),
                          MakeBooleanChecker())
            .AddAttribute("AntennaDelay",
                          "What is left of the antenna delay of this radio after calibration. A "
                          "real radio has several hundred nanoseconds of it between the "
                          "timestamping unit and the antenna, and firmware calibrates that away; "
                          "what remains is a constant range bias, and this is the knob for it. "
                          "Each end of a two-way exchange contributes its own, so a value of one "
                          "hundred picoseconds at both ends inflates every range by six "
                          "centimetres.",
                          TimeValue(Time(0)),
                          MakeTimeAccessor(&UwbPhy::m_antennaDelay),
                          MakeTimeChecker())
            .AddAttribute("RangingNoiseFactor",
                          "How far the leading edge detector of a real radio sits above the "
                          "Cramer-Rao bound for time of arrival estimation. The bound assumes a "
                          "matched filter on a known pulse in white noise; a real detector "
                          "searches for the first path in a multipath channel with a finite "
                          "accumulator, and lands several times short of it.",
                          DoubleValue(6.0),
                          MakeDoubleAccessor(&UwbPhy::m_rangingNoiseFactor),
                          MakeDoubleChecker<double>(1.0, 100.0))
            .AddAttribute("RangingNoiseFloor",
                          "The timestamp error a calibrated radio still shows when the signal is "
                          "strong. Thirty picoseconds is about a centimetre, which is the "
                          "standard deviation reported for static two-way ranging at short "
                          "range.",
                          TimeValue(PicoSeconds(30)),
                          MakeTimeAccessor(&UwbPhy::m_rangingNoiseFloor),
                          MakeTimeChecker())
            .AddTraceSource("PhyTxBegin",
                            "A frame starts being transmitted.",
                            MakeTraceSourceAccessor(&UwbPhy::m_phyTxBeginTrace),
                            "ns3::uwb::UwbPhy::TxBeginTracedCallback")
            .AddTraceSource("PhyTxEnd",
                            "A frame has been transmitted.",
                            MakeTraceSourceAccessor(&UwbPhy::m_phyTxEndTrace),
                            "ns3::uwb::UwbPhy::TxEndTracedCallback")
            .AddTraceSource("PhyRxBegin",
                            "The correlator has locked onto the preamble of a frame.",
                            MakeTraceSourceAccessor(&UwbPhy::m_phyRxBeginTrace),
                            "ns3::uwb::UwbPhy::TxBeginTracedCallback")
            .AddTraceSource("PhyRxEnd",
                            "A frame has been received without error.",
                            MakeTraceSourceAccessor(&UwbPhy::m_phyRxEndTrace),
                            "ns3::uwb::UwbPhy::RxEndTracedCallback")
            .AddTraceSource("PhyRxDrop",
                            "A frame has been dropped, with the reason.",
                            MakeTraceSourceAccessor(&UwbPhy::m_phyRxDropTrace),
                            "ns3::uwb::UwbPhy::RxDropTracedCallback")
            .AddTraceSource("State",
                            "The state of the radio has changed.",
                            MakeTraceSourceAccessor(&UwbPhy::m_stateTrace),
                            "ns3::uwb::UwbPhy::StateTracedCallback");
    return tid;
}

UwbPhy::UwbPhy()
    : m_preambleCode(9),
      m_txPowerDbm(-14.3),
      m_rxEnabled(true),
      m_antennaDelay(Time(0)),
      m_rangingNoiseFactor(6.0),
      m_rangingNoiseFloor(PicoSeconds(30)),
      m_state(UwbPhyState::IDLE),
      m_noiseW(0.0),
      m_rxStart(Time(0)),
      m_lastTxTimestamp(0)
{
    NS_LOG_FUNCTION(this);
    m_errorModel = CreateObject<UwbErrorModel>();
    m_clock = CreateObject<UwbClockModel>();
    m_antenna = CreateObject<IsotropicAntennaModel>();
    m_random = CreateObject<UniformRandomVariable>();
    m_rangingNoise = CreateObject<NormalRandomVariable>();
    m_rangingNoise->SetAttribute("Mean", DoubleValue(0.0));
    m_rangingNoise->SetAttribute("Variance", DoubleValue(1.0));
    UpdateNoise();
}

UwbPhy::~UwbPhy()
{
    NS_LOG_FUNCTION(this);
}

void
UwbPhy::DoDispose()
{
    NS_LOG_FUNCTION(this);
    m_startTxEvent.Cancel();
    m_endTxEvent.Cancel();
    m_endShrEvent.Cancel();
    m_endRxEvent.Cancel();
    m_interference.Clear();
    m_device = nullptr;
    m_mobility = nullptr;
    m_channel = nullptr;
    m_antenna = nullptr;
    m_errorModel = nullptr;
    m_clock = nullptr;
    m_noisePsd = nullptr;
    m_receiveOkCallback.Nullify();
    m_receiveErrorCallback.Nullify();
    m_txEndCallback.Nullify();
    SpectrumPhy::DoDispose();
}

void
UwbPhy::DoInitialize()
{
    NS_LOG_FUNCTION(this);
    UpdateNoise();
    SpectrumPhy::DoInitialize();
}

/* ---------------------------------------------------------------------------------------- */
/* SpectrumPhy                                                                                */
/* ---------------------------------------------------------------------------------------- */

void
UwbPhy::SetDevice(Ptr<NetDevice> device)
{
    m_device = device;
}

Ptr<NetDevice>
UwbPhy::GetDevice() const
{
    return m_device;
}

void
UwbPhy::SetMobility(Ptr<MobilityModel> mobility)
{
    m_mobility = mobility;
}

Ptr<MobilityModel>
UwbPhy::GetMobility() const
{
    return m_mobility;
}

void
UwbPhy::SetChannel(Ptr<SpectrumChannel> channel)
{
    m_channel = channel;
}

Ptr<const SpectrumModel>
UwbPhy::GetRxSpectrumModel() const
{
    return UwbSpectrumValueHelper::GetSpectrumModel();
}

Ptr<Object>
UwbPhy::GetAntenna() const
{
    return m_antenna;
}

/* ---------------------------------------------------------------------------------------- */
/* Configuration                                                                              */
/* ---------------------------------------------------------------------------------------- */

void
UwbPhy::SetConfig(const UwbPhyConfig& config)
{
    NS_LOG_FUNCTION(this << config.ToString());
    m_config = config;
    UpdateNoise();
}

const UwbPhyConfig&
UwbPhy::GetConfig() const
{
    return m_config;
}

void
UwbPhy::SetRfChannel(uint8_t channel)
{
    NS_ABORT_MSG_IF(channel < FIRST_CHANNEL || channel > LAST_CHANNEL,
                    "Channel " << +channel << " is outside the range this model supports");
    m_config.channel = channel;
    UpdateNoise();
}

uint8_t
UwbPhy::GetRfChannel() const
{
    return m_config.channel;
}

void
UwbPhy::SetDataRate(UwbDataRate rate)
{
    m_config.dataRate = rate;
}

UwbDataRate
UwbPhy::GetDataRate() const
{
    return m_config.dataRate;
}

void
UwbPhy::SetPrf(UwbPrf prf)
{
    m_config.prf = prf;
}

UwbPrf
UwbPhy::GetPrf() const
{
    return m_config.prf;
}

void
UwbPhy::SetPreambleSymbols(uint32_t symbols)
{
    m_config.preambleSymbols = symbols;
}

uint32_t
UwbPhy::GetPreambleSymbols() const
{
    return m_config.preambleSymbols;
}

void
UwbPhy::SetPreambleCode(uint8_t code)
{
    m_preambleCode = code;
}

uint8_t
UwbPhy::GetPreambleCode() const
{
    return m_preambleCode;
}

void
UwbPhy::SetTxPowerDbm(double txPowerDbm)
{
    m_txPowerDbm = txPowerDbm;
}

double
UwbPhy::GetTxPowerDbm() const
{
    return m_txPowerDbm;
}

void
UwbPhy::SetTxPowerToRegulatoryLimit()
{
    m_txPowerDbm = GetRegulatoryTxPowerDbm(m_config.channel);
}

double
UwbPhy::GetPowerDensityDbmPerMhz() const
{
    return UwbSpectrumValueHelper::GetPowerDensityDbmPerMhz(m_txPowerDbm, m_config.channel);
}

void
UwbPhy::SetRxEnabled(bool enabled)
{
    NS_LOG_FUNCTION(this << enabled);
    m_rxEnabled = enabled;
    if (!enabled && (m_state == UwbPhyState::IDLE))
    {
        ChangeState(UwbPhyState::OFF);
    }
    else if (enabled && (m_state == UwbPhyState::OFF))
    {
        ChangeState(UwbPhyState::IDLE);
    }
}

bool
UwbPhy::IsRxEnabled() const
{
    return m_rxEnabled;
}

UwbPhyState
UwbPhy::GetState() const
{
    return m_state;
}

bool
UwbPhy::IsBusy() const
{
    return m_state == UwbPhyState::TX || m_state == UwbPhyState::SYNC || m_state == UwbPhyState::RX;
}

void
UwbPhy::SetErrorModel(Ptr<UwbErrorModel> errorModel)
{
    m_errorModel = errorModel;
    UpdateNoise();
}

Ptr<UwbErrorModel>
UwbPhy::GetErrorModel() const
{
    return m_errorModel;
}

void
UwbPhy::SetClockModel(Ptr<UwbClockModel> clock)
{
    m_clock = clock;
}

Ptr<UwbClockModel>
UwbPhy::GetClockModel() const
{
    return m_clock;
}

void
UwbPhy::SetReceiveOkCallback(ReceiveOkCallback callback)
{
    m_receiveOkCallback = callback;
}

void
UwbPhy::SetReceiveErrorCallback(ReceiveErrorCallback callback)
{
    m_receiveErrorCallback = callback;
}

void
UwbPhy::SetTxEndCallback(TxEndCallback callback)
{
    m_txEndCallback = callback;
}

int64_t
UwbPhy::AssignStreams(int64_t stream)
{
    m_random->SetStream(stream);
    m_rangingNoise->SetStream(stream + 1);
    return 2;
}

void
UwbPhy::ChangeState(UwbPhyState state)
{
    if (state == m_state)
    {
        return;
    }
    const auto previous = m_state;
    m_state = state;
    m_stateTrace(previous, state);
}

void
UwbPhy::UpdateNoise()
{
    if (!m_errorModel)
    {
        return;
    }
    m_noisePsd =
        UwbSpectrumValueHelper::CreateNoisePowerSpectralDensity(m_errorModel->GetNoiseFigureDb());
    m_noiseW = UwbSpectrumValueHelper::GetBandPower(m_noisePsd, m_config.channel);
}

/* ---------------------------------------------------------------------------------------- */
/* Timing                                                                                     */
/* ---------------------------------------------------------------------------------------- */

Time
UwbPhy::CalculateTxDuration(uint32_t psduOctets) const
{
    return GetFrameDuration(m_config, psduOctets);
}

Time
UwbPhy::GetShrDuration() const
{
    return uwb::GetShrDuration(m_config);
}

double
UwbPhy::GetSensitivityDbm() const
{
    return m_errorModel->GetSensitivityDbm(m_config);
}

double
UwbPhy::GetNoisePowerW() const
{
    return m_noiseW;
}

double
UwbPhy::GetRssiDbm() const
{
    const double powerW = m_interference.GetTotalPowerW(Simulator::Now()) + m_noiseW;
    return (powerW > 0.0) ? 10.0 * std::log10(powerW) + 30.0 : -200.0;
}

uint64_t
UwbPhy::GetTxTimestampFor(Time markerAt) const
{
    return m_clock->GetLocalTicks(markerAt - m_antennaDelay);
}

uint64_t
UwbPhy::GetLastTxTimestamp() const
{
    return m_lastTxTimestamp;
}

/* ---------------------------------------------------------------------------------------- */
/* Transmission                                                                               */
/* ---------------------------------------------------------------------------------------- */

bool
UwbPhy::StartTx(Ptr<Packet> packet, bool ranging)
{
    NS_LOG_FUNCTION(this << packet << ranging);
    if (m_state == UwbPhyState::TX || m_startTxEvent.IsPending())
    {
        NS_LOG_DEBUG("Already transmitting, the request is refused");
        return false;
    }
    DoStartTx(packet, ranging);
    return true;
}

bool
UwbPhy::ScheduleTx(Ptr<Packet> packet, Time markerAt, bool ranging)
{
    NS_LOG_FUNCTION(this << packet << markerAt << ranging);
    const Time start = markerAt - GetShrDuration();
    if (start < Simulator::Now())
    {
        NS_LOG_WARN("A transmission was scheduled for " << markerAt << ", which has passed");
        return false;
    }
    if (m_state == UwbPhyState::TX || m_startTxEvent.IsPending())
    {
        NS_LOG_DEBUG("Already transmitting, the request is refused");
        return false;
    }
    m_startTxEvent =
        Simulator::Schedule(start - Simulator::Now(), &UwbPhy::DoStartTx, this, packet, ranging);
    return true;
}

void
UwbPhy::DoStartTx(Ptr<Packet> packet, bool ranging)
{
    NS_LOG_FUNCTION(this << packet << ranging);
    NS_ABORT_MSG_IF(!m_channel, "The PHY is not attached to a spectrum channel");

    // a transmission abandons whatever the receiver was doing, as a half duplex radio must
    if (m_state == UwbPhyState::SYNC || m_state == UwbPhyState::RX)
    {
        m_endShrEvent.Cancel();
        m_endRxEvent.Cancel();
        m_phyRxDropTrace(nullptr, 0.0, "tx-while-rx");
    }
    ChangeState(UwbPhyState::TX);

    const Time shr = GetShrDuration();
    const Time duration = CalculateTxDuration(packet->GetSize());
    m_lastTxTimestamp = GetTxTimestampFor(Simulator::Now() + shr);

    auto params = Create<UwbSpectrumSignalParameters>();
    params->duration = duration;
    params->txPhy = this;
    params->txAntenna = m_antenna;
    params->psd =
        UwbSpectrumValueHelper::CreateTxPowerSpectralDensity(m_txPowerDbm, m_config.channel);
    params->packet = packet->Copy();
    params->config = m_config;
    params->preambleCode = m_preambleCode;
    params->shrDuration = shr;
    params->ranging = ranging;

    m_phyTxBeginTrace(packet, m_config.channel, m_txPowerDbm);
    m_channel->StartTx(params);
    m_endTxEvent = Simulator::Schedule(duration, &UwbPhy::EndTx, this, packet);
}

void
UwbPhy::EndTx(Ptr<const Packet> packet)
{
    NS_LOG_FUNCTION(this << packet);
    ChangeState(m_rxEnabled ? UwbPhyState::IDLE : UwbPhyState::OFF);
    m_phyTxEndTrace(packet, m_lastTxTimestamp);
    if (!m_txEndCallback.IsNull())
    {
        m_txEndCallback(packet, m_lastTxTimestamp);
    }
}

/* ---------------------------------------------------------------------------------------- */
/* Reception                                                                                  */
/* ---------------------------------------------------------------------------------------- */

void
UwbPhy::StartRx(Ptr<SpectrumSignalParameters> params)
{
    NS_LOG_FUNCTION(this << params);
    const Time now = Simulator::Now();
    m_interference.Cleanup(now - Seconds(1));

    auto uwbParams = DynamicCast<UwbSpectrumSignalParameters>(params);
    // every signal, UWB or not, delivers whatever of its power falls inside the channel
    const double rxPowerW = UwbSpectrumValueHelper::GetBandPower(params->psd, m_config.channel);

    // a UWB signal built on a different preamble code is suppressed by the correlator, which is
    // what lets many links share a channel; anything else arrives whole
    double effectivePowerW = rxPowerW;
    if (uwbParams && uwbParams->preambleCode != m_preambleCode)
    {
        effectivePowerW *= std::pow(10.0, -m_errorModel->GetCodeRejectionDb() / 10.0);
    }
    m_interference.AddSignal(now, now + params->duration, effectivePowerW);

    if (!uwbParams)
    {
        NS_LOG_DEBUG("A signal that is not UWB adds " << rxPowerW << " W to the interference");
        return;
    }

    const double rxPowerDbm = (rxPowerW > 0.0) ? 10.0 * std::log10(rxPowerW) + 30.0 : -200.0;

    if (!m_rxEnabled || m_state == UwbPhyState::OFF)
    {
        NS_LOG_DEBUG("The radio is not listening");
        return;
    }
    if (m_state == UwbPhyState::TX)
    {
        m_phyRxDropTrace(uwbParams->packet, rxPowerDbm, "busy-tx");
        return;
    }
    if (m_state == UwbPhyState::SYNC || m_state == UwbPhyState::RX)
    {
        m_phyRxDropTrace(uwbParams->packet, rxPowerDbm, "busy-rx");
        return;
    }
    if (uwbParams->config.channel != m_config.channel)
    {
        NS_LOG_DEBUG("The signal is on channel " << +uwbParams->config.channel
                                                 << ", the radio is on " << +m_config.channel);
        return;
    }
    if (uwbParams->preambleCode != m_preambleCode)
    {
        NS_LOG_DEBUG("The signal uses preamble code " << +uwbParams->preambleCode
                                                      << ", the radio expects " << +m_preambleCode);
        return;
    }
    if (uwbParams->config.prf != m_config.prf ||
        uwbParams->config.preambleSymbols != m_config.preambleSymbols)
    {
        m_phyRxDropTrace(uwbParams->packet, rxPowerDbm, "preamble-mismatch");
        return;
    }

    ChangeState(UwbPhyState::SYNC);
    m_rxStart = now;
    m_endShrEvent = Simulator::Schedule(uwbParams->shrDuration,
                                        &UwbPhy::EndShr,
                                        this,
                                        uwbParams->packet,
                                        rxPowerW,
                                        uwbParams);
}

void
UwbPhy::EndShr(Ptr<Packet> packet, double rxPowerW, Ptr<UwbSpectrumSignalParameters> params)
{
    NS_LOG_FUNCTION(this << packet << rxPowerW);
    const Time now = Simulator::Now();
    const double rxPowerDbm = (rxPowerW > 0.0) ? 10.0 * std::log10(rxPowerW) + 30.0 : -200.0;

    // the correlator integrates over the whole synchronisation header, so what decides the lock
    // is the average signal to interference and noise ratio across it
    const double sinr = m_interference.GetAverageSinr(rxPowerW, m_noiseW, m_rxStart, now);
    if (!m_errorModel->IsPreambleAcquired(sinr, params->config))
    {
        NS_LOG_DEBUG("The preamble did not clear the acquisition threshold");
        ChangeState(m_rxEnabled ? UwbPhyState::IDLE : UwbPhyState::OFF);
        m_phyRxDropTrace(packet, rxPowerDbm, "preamble-not-acquired");
        return;
    }

    // the marker is the first pulse after the synchronisation header, so it falls now; the
    // receiver has to estimate it, and the error it makes is the accuracy of the range
    const double sigmaS = GetTimestampSigmaS(sinr);
    const double errorS = sigmaS * m_rangingNoise->GetValue();

    UwbRxInfo info;
    info.rxPowerDbm = rxPowerDbm;
    info.sinrDb = (sinr > 0.0) ? 10.0 * std::log10(sinr) : -200.0;
    info.ebNoDb = m_errorModel->GetEbNoDb(sinr, params->config);
    info.arrival = now;
    info.timestampErrorS = errorS;
    info.rxTimestamp = m_clock->GetLocalTicks(now + m_antennaDelay + Seconds(errorS));
    info.config = params->config;
    info.preambleCode = params->preambleCode;
    info.ranging = params->ranging;

    ChangeState(UwbPhyState::RX);
    m_phyRxBeginTrace(packet, params->config.channel, rxPowerDbm);

    const Time remaining = params->duration - params->shrDuration;
    m_endRxEvent = Simulator::Schedule(remaining, &UwbPhy::EndRx, this, packet, info, now);
}

void
UwbPhy::EndRx(Ptr<Packet> packet, UwbRxInfo info, Time payloadStart)
{
    NS_LOG_FUNCTION(this << packet);
    ChangeState(m_rxEnabled ? UwbPhyState::IDLE : UwbPhyState::OFF);

    const double rxPowerW = std::pow(10.0, (info.rxPowerDbm - 30.0) / 10.0);

    // the header and the payload are not sent at the same rate, so they are weighed separately
    UwbPhyConfig phrConfig = info.config;
    phrConfig.dataRate = GetPhrDataRate(info.config.dataRate);
    const Time phrEnd = payloadStart + GetPhrDuration(info.config);

    const double phrSuccess = m_interference.CalculateSuccessRate(m_errorModel,
                                                                  rxPowerW,
                                                                  m_noiseW,
                                                                  payloadStart,
                                                                  phrEnd,
                                                                  phrConfig,
                                                                  PHR_BITS);
    const double psduSuccess =
        m_interference.CalculateSuccessRate(m_errorModel,
                                            rxPowerW,
                                            m_noiseW,
                                            phrEnd,
                                            Simulator::Now(),
                                            info.config,
                                            static_cast<uint64_t>(packet->GetSize()) * 8);

    const double success = phrSuccess * psduSuccess;
    NS_LOG_DEBUG("Reception of " << packet->GetSize() << " octets at " << info.rxPowerDbm
                                 << " dBm, Eb/N0 " << info.ebNoDb << " dB, success " << success);

    if (m_random->GetValue() > success)
    {
        m_phyRxDropTrace(packet, info.rxPowerDbm, (phrSuccess < psduSuccess) ? "phr-error"
                                                                            : "payload-error");
        if (!m_receiveErrorCallback.IsNull())
        {
            m_receiveErrorCallback(packet, info);
        }
        return;
    }

    m_phyRxEndTrace(packet, info);
    if (!m_receiveOkCallback.IsNull())
    {
        m_receiveOkCallback(packet, info);
    }
}

double
UwbPhy::GetTimestampSigmaS(double sinr) const
{
    if (sinr <= 0.0)
    {
        return m_rangingNoiseFloor.GetSeconds();
    }

    // the root mean square bandwidth of a flat spectrum of width B is B divided by the square
    // root of twelve
    const double bandwidthHz = UwbSpectrumValueHelper::GetNoiseBandwidthHz(m_config.channel);
    const double betaHz = bandwidthHz / std::sqrt(12.0);

    // the preamble correlator raises the signal to noise ratio the estimator works from
    const double gain = std::pow(10.0, m_errorModel->GetAcquisitionGainDb(m_config) / 10.0);
    const double postCorrelationSnr = sinr * gain;

    const double bound = 1.0 / (2.0 * M_PI * betaHz * std::sqrt(2.0 * postCorrelationSnr));
    const double achieved = m_rangingNoiseFactor * bound;
    const double floorS = m_rangingNoiseFloor.GetSeconds();
    return std::sqrt(achieved * achieved + floorS * floorS);
}

} // namespace uwb
} // namespace ns3
