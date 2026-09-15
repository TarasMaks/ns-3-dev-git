/*
 * Copyright (c) 2026
 *
 * SPDX-License-Identifier: GPL-2.0-only
 */

#ifndef UWB_MAC_H
#define UWB_MAC_H

#include "uwb-headers.h"
#include "uwb-phy.h"
#include "uwb-ranging.h"

#include "ns3/event-id.h"
#include "ns3/mac16-address.h"
#include "ns3/object.h"
#include "ns3/packet.h"
#include "ns3/random-variable-stream.h"
#include "ns3/traced-callback.h"

#include <deque>

namespace ns3
{
namespace uwb
{

/**
 * @ingroup uwb
 * What one ranging exchange produced.
 */
struct UwbRangingResult
{
    Mac16Address peer;                                //!< the device at the other end
    UwbRangingMethod method{UwbRangingMethod::DS_TWR}; //!< the scheme that was used
    uint8_t session{0};       //!< the identifier of the exchange
    bool valid{false};        //!< false when the exchange did not complete
    double rangeMetres{0.0};  //!< the distance that was measured
    double rxPowerDbm{0.0};   //!< the power of the last frame of the exchange
    double sinrDb{0.0};       //!< the signal to interference and noise ratio of that frame
    Time replyDelay{Time(0)}; //!< how long the responder held the exchange
    Time when{Time(0)};       //!< when the exchange completed
    bool measuredHere{false}; //!< true if this device did the arithmetic, false if it was told
};

/**
 * @ingroup uwb
 * The medium access layer of a UWB device: frames, acknowledgements and ranging exchanges.
 *
 * A UWB radio cannot listen before it talks the way a narrowband radio does. Its own signal
 * sits below the noise floor, so carrier sense would have nothing to sense, and IEEE Std
 * 802.15.4 accordingly lets UWB devices use unslotted ALOHA. This MAC does the same: it
 * transmits when its own radio is free and backs off by a random interval when it is not.
 *
 * On top of that it runs the ranging exchanges of IEEE Std 802.15.4z.
 *
 * In a single-sided exchange the initiator sends a poll and the responder answers with the two
 * counter readings it took, which is all the initiator needs; two frames, and an error that
 * grows with the turnaround and the difference between the two crystals.
 *
 * In a double-sided exchange the initiator adds a third frame carrying its own three readings,
 * which gives the responder two round trips to combine, and the crystal errors cancel. The
 * responder therefore holds the answer, and by default sends it back in a short report so that
 * both ends know the range, as products generally do.
 *
 * Every reply is a delayed transmission, scheduled at a counter value chosen when the reply is
 * built. That is not an optimisation: it is what lets a device put its own transmit timestamp
 * inside the frame it is about to send, and no double-sided exchange works without it. The
 * turnaround can never be shorter than the preamble of the reply, because the marker a
 * timestamp refers to is the first pulse after the preamble has finished.
 */
class UwbMac : public Object
{
  public:
    /// Signature of the callback invoked when a data frame is delivered.
    using ReceiveCallback = Callback<void, Ptr<Packet>, Mac16Address>;
    /// Signature of the callback invoked when a ranging exchange finishes.
    using RangingResultCallback = Callback<void, const UwbRangingResult&>;
    /// Signature of the callback invoked when a blink is heard, with its sender, the
    /// identifier the sender put on it and what the receiver learned about the frame.
    using BlinkCallback = Callback<void, Mac16Address, uint8_t, const UwbRxInfo&>;

    /// @return the TypeId
    static TypeId GetTypeId();

    UwbMac();
    ~UwbMac() override;

    /// @param phy the radio this MAC drives
    void SetPhy(Ptr<UwbPhy> phy);
    /// @return the radio this MAC drives
    Ptr<UwbPhy> GetPhy() const;

    /// @param address the short address of this device
    void SetAddress(Mac16Address address);
    /// @return the short address of this device
    Mac16Address GetAddress() const;

    /// @param panId the personal area network this device belongs to
    void SetPanId(uint16_t panId);
    /// @return the personal area network this device belongs to
    uint16_t GetPanId() const;

    /**
     * Queue a data frame.
     *
     * @param packet the payload
     * @param destination the short address to send it to
     * @return true if it was queued, false if the queue is full
     */
    bool Enqueue(Ptr<Packet> packet, Mac16Address destination);

    /**
     * Start a ranging exchange with a peer.
     *
     * @param peer the device to range against
     * @param method the scheme to use; time difference of arrival is not a two-way exchange and
     *               is refused here, use SendBlink instead
     * @return true if the exchange was started, false if one is already running
     */
    bool StartRanging(Mac16Address peer, UwbRangingMethod method);

    /**
     * Send a blink, the single frame a tag transmits for anchors to timestamp when the
     * infrastructure, rather than the tag, is working out where it is.
     *
     * @return true if the blink was sent or queued
     */
    bool SendBlink();

    /// @return true if a ranging exchange is in progress
    bool IsRanging() const;

    /// @param callback invoked with the payload and the sender of every data frame
    void SetReceiveCallback(ReceiveCallback callback);

    /// @param callback invoked whenever a ranging exchange finishes, successfully or not
    void SetRangingResultCallback(RangingResultCallback callback);

    /// @param callback invoked with the sender, the identifier and the arrival of every blink
    void SetBlinkCallback(BlinkCallback callback);

    /// @param delay how long this device waits before answering a poll
    void SetResponseDelay(Time delay);
    /// @return how long this device waits before answering a poll
    Time GetResponseDelay() const;

    /// @return the number of frames waiting to be sent
    std::size_t GetQueueLength() const;

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
    /// One queued data frame.
    struct Pending
    {
        Ptr<Packet> packet;        //!< the payload
        Mac16Address destination;  //!< where it is going
        uint8_t sequence{0};       //!< its sequence number
        uint32_t attempts{0};      //!< how many times it has been sent
    };

    /// What is known about an exchange in progress.
    struct Session
    {
        bool active{false};                                //!< whether the exchange is running
        Mac16Address peer;                                 //!< the device at the other end
        UwbRangingMethod method{UwbRangingMethod::DS_TWR}; //!< the scheme in use
        uint8_t id{0};                                     //!< the identifier of the exchange
        UwbTwrTimestamps stamps;                           //!< the readings taken so far
        Time replyDelay{Time(0)};                          //!< the turnaround that was used
        double rxPowerDbm{0.0};                            //!< power of the last frame
        double sinrDb{0.0};                                //!< quality of the last frame
        EventId timeout;                                   //!< gives up if the peer goes quiet
    };

    /// Try to put the frame at the head of the queue on the air.
    void TrySend();

    /// Wait a random interval and try again.
    void Backoff();

    /**
     * Build a frame and hand it to the radio now.
     *
     * @param payload the payload, which may be empty
     * @param destination where the frame is going
     * @param type the frame type
     * @param sequence the sequence number
     * @param ackRequest whether the frame asks to be acknowledged
     * @param ranging whether the receiver should timestamp the arrival
     * @return true if the radio took it
     */
    bool SendFrame(Ptr<Packet> payload,
                   Mac16Address destination,
                   UwbFrameType type,
                   uint8_t sequence,
                   bool ackRequest,
                   bool ranging);

    /**
     * Build a frame and hand it to the radio to be sent so that its marker falls at a chosen
     * instant, which is what lets the frame carry its own transmit timestamp.
     *
     * @param payload the payload
     * @param destination where the frame is going
     * @param markerAt when the marker should fall
     * @return true if the radio took it
     */
    bool ScheduleFrame(Ptr<Packet> payload, Mac16Address destination, Time markerAt);

    /**
     * The earliest instant at which this device could place the marker of a reply, given that
     * the preamble has to go out first.
     *
     * @return the instant
     */
    Time GetReplyMarkerTime() const;

    /**
     * A frame arrived without error.
     *
     * @param packet the frame
     * @param info what the receiver learned about it
     */
    void ReceiveOk(Ptr<Packet> packet, const UwbRxInfo& info);

    /**
     * Handle a ranging message.
     *
     * @param packet the payload, with the MAC header already removed
     * @param source who sent it
     * @param info what the receiver learned about the frame
     */
    void HandleRanging(Ptr<Packet> packet, Mac16Address source, const UwbRxInfo& info);

    /// Give up on the exchange this device started.
    void InitiatorTimeout();

    /// Give up on the exchange this device is answering.
    void ResponderTimeout();

    /// The acknowledgement of the frame at the head of the queue did not arrive.
    void AckTimeout();

    /**
     * Announce a result and close the exchange it belongs to.
     *
     * @param result what was measured
     */
    void Report(const UwbRangingResult& result);

    Ptr<UwbPhy> m_phy;         //!< the radio
    Mac16Address m_address;    //!< the short address of this device
    uint16_t m_panId;          //!< the personal area network

    Time m_responseDelay;   //!< how long this device waits before answering a poll
    Time m_rangingTimeout;  //!< how long an exchange may stall before it is abandoned
    Time m_ackTimeout;      //!< how long to wait for an acknowledgement
    Time m_backoffMinimum;  //!< shortest wait before retrying a busy radio
    Time m_backoffMaximum;  //!< longest wait before retrying a busy radio
    uint32_t m_maxRetries;  //!< how many times a data frame is sent before it is dropped
    uint32_t m_queueLimit;  //!< how many data frames may wait
    bool m_ackEnabled;      //!< whether data frames ask to be acknowledged
    bool m_reportRange;     //!< whether the responder tells the initiator what it measured

    std::deque<Pending> m_queue; //!< the data frames waiting to be sent
    uint8_t m_sequence;          //!< the next sequence number
    uint8_t m_session;           //!< the next exchange identifier
    Session m_initiator;         //!< the exchange this device started
    Session m_responder;         //!< the exchange this device is answering
    bool m_awaitingAck;          //!< whether the head of the queue is waiting to be acknowledged

    EventId m_sendEvent;    //!< the next attempt to send
    EventId m_ackEvent;     //!< gives up on an acknowledgement

    ReceiveCallback m_receiveCallback;             //!< invoked on every data frame
    RangingResultCallback m_rangingResultCallback; //!< invoked on every ranging result
    BlinkCallback m_blinkCallback;                 //!< invoked on every blink heard

    Ptr<UniformRandomVariable> m_backoff; //!< draws the wait before retrying a busy radio

    /// Traced when a ranging exchange finishes, with what it produced.
    TracedCallback<const UwbRangingResult&> m_rangingTrace;
    /// Traced when a frame is handed to the radio, with the frame and its destination.
    TracedCallback<Ptr<const Packet>, Mac16Address> m_txTrace;
    /// Traced when a data frame is delivered, with the frame and its source.
    TracedCallback<Ptr<const Packet>, Mac16Address> m_rxTrace;
    /// Traced when a data frame is given up on, with the frame and its destination.
    TracedCallback<Ptr<const Packet>, Mac16Address> m_dropTrace;
};

} // namespace uwb
} // namespace ns3

#endif /* UWB_MAC_H */
