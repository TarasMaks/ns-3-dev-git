/*
 * Copyright (c) 2026
 *
 * SPDX-License-Identifier: GPL-2.0-only
 */

#ifndef UWB_TDOA_ENGINE_H
#define UWB_TDOA_ENGINE_H

#include "uwb-mac.h"

#include "ns3/event-id.h"
#include "ns3/object.h"
#include "ns3/random-variable-stream.h"
#include "ns3/traced-callback.h"
#include "ns3/vector.h"

#include <map>
#include <vector>

namespace ns3
{
namespace uwb
{

/**
 * @ingroup uwb
 * The infrastructure side of time difference of arrival: it collects the instants at which one
 * blink reached each anchor and works out where the tag was.
 *
 * Two-way ranging asks the tag to take part in a conversation, which costs the tag air time and
 * battery and limits how many tags a channel can carry. Time difference of arrival asks nothing
 * of the tag beyond a single frame: the anchors hear it, compare notes, and the tag never knows
 * it was located. A room can hold far more tags that way, which is why large installations use
 * it.
 *
 * What it costs instead is time. Each anchor timestamps the blink on its own crystal, and the
 * anchors have to be brought onto a common time base before the differences mean anything. The
 * engine does that conversion exactly, which stands in for an ideal wired synchronisation, and
 * then adds whatever residual error SyncError describes. That residual is not a detail: light
 * travels thirty centimetres in a nanosecond, so a nanosecond of disagreement between two
 * anchors moves the answer by about that much, and it does not matter how good the radios are.
 *
 * Anchors are assumed to be at known positions, as they are in practice, having been surveyed
 * when the system was installed. Four are needed to fix a position in a plane and five to
 * resolve height, one more than trilateration needs, because the distance to the reference
 * anchor is unknown and has to be solved for alongside the position.
 */
class UwbTdoaEngine : public Object
{
  public:
    /// Signature of the callback invoked when a tag has been located.
    using PositionCallback = Callback<void, Mac16Address, Vector, bool>;

    /// @return the TypeId
    static TypeId GetTypeId();

    UwbTdoaEngine();
    ~UwbTdoaEngine() override;

    /**
     * Register an anchor. The engine listens to the blinks it hears.
     *
     * @param anchor the MAC of the anchor
     * @param position where the anchor is, as surveyed
     */
    void AddAnchor(Ptr<UwbMac> anchor, Vector position);

    /// @return the number of registered anchors
    std::size_t GetNAnchors() const;

    /// @param callback invoked with the tag, the position found and whether it is trustworthy
    void SetPositionCallback(PositionCallback callback);

    /// @param solveHeight whether the vertical coordinate is estimated as well
    void SetSolveHeight(bool solveHeight);

    /**
     * Assign the streams of the random variables of this object.
     * @param stream the first stream index to use
     * @return the number of streams used
     */
    int64_t AssignStreams(int64_t stream);

  protected:
    void DoDispose() override;

  private:
    /// One anchor.
    struct Anchor
    {
        Ptr<UwbMac> mac;          //!< the MAC of the anchor
        Vector position;          //!< where it is
        Time syncOffset{Time(0)}; //!< what its clock is out by after synchronisation
    };

    /// The reports gathered for one blink.
    struct Collection
    {
        std::map<std::size_t, double> arrivals; //!< anchor index to arrival time, in seconds
        EventId solve;                          //!< fires once the window has closed
    };

    /// A blink is identified by who sent it and what they numbered it.
    using BlinkKey = std::pair<Mac16Address, uint8_t>;

    /**
     * An anchor heard a blink.
     *
     * @param index which anchor
     * @param tag who sent the blink
     * @param session the identifier the tag put on it
     * @param info what the anchor learned about the frame
     */
    void OnBlink(std::size_t index, Mac16Address tag, uint8_t session, const UwbRxInfo& info);

    /**
     * The window for one blink has closed, so work out where the tag was.
     *
     * @param key which blink
     */
    void Solve(BlinkKey key);

    std::vector<Anchor> m_anchors;             //!< the anchors
    std::map<BlinkKey, Collection> m_pending;  //!< the blinks still being collected
    Time m_collectionWindow;                   //!< how long to wait for the anchors to report
    Time m_syncError;                          //!< the residual disagreement between anchors
    bool m_solveHeight;                        //!< whether height is estimated

    PositionCallback m_positionCallback;   //!< invoked once a tag has been located
    Ptr<NormalRandomVariable> m_syncNoise; //!< draws the residual offset of each anchor

    /// Traced when a tag is located, with the tag, the position and whether it is trustworthy.
    TracedCallback<Mac16Address, Vector, bool> m_positionTrace;
};

} // namespace uwb
} // namespace ns3

#endif /* UWB_TDOA_ENGINE_H */
