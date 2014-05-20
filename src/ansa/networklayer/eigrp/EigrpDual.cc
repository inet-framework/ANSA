//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU Lesser General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
// 
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Lesser General Public License for more details.
// 
// You should have received a copy of the GNU Lesser General Public License
// along with this program.  If not, see http://www.gnu.org/licenses/.
// 

#include "EigrpDual.h"
#include "EigrpMetricHelper.h"

#include <omnetpp.h>

#define EIGRP_DUAL_DEBUG

namespace eigrpDual
{
// User messages
const char *userMsgs[] =
{
    // RECV_UPDATE
    "received Update",
    // RECV_QUERY
    "received Query",
    // RECV_REPLY
    "received Reply",
    // NEIGHBOR_DOWN
    "neighbor down",
    // INTERFACE_DOWN
    "interface down",
    //INTERFACE_UP
    "interface up",
    //LOST_ROUTE
    "route deletion",
};
};  // end of namespace eigrpDual

void EigrpDual::invalidateRoute(EigrpRouteSource<IPv4Address> *routeSrc)
{
    if (routeSrc->isValid())
    {
        routeSrc->setValid(false);
        ev << "DUAL: invalidate route via " << routeSrc->getNextHop() << " in TT" << endl;
    }
}

/**
 * @param event Type of input event
 * @param source Data from messages are stored into source. If the source is new,
 *        then it is not inserted into route.
 * @param route Contains actual metric and RD. Dij remains unchanged.
 */
void EigrpDual::processEvent(DualEvent event, EigrpRouteSource<IPv4Address> *source, int neighborId, bool isSourceNew)
{
    EigrpRoute<IPv4Address> *route = source->getRouteInfo();

    if (event == NEIGHBOR_DOWN || event == INTERFACE_DOWN)
        source->setUnreachableMetric(); // Must be there

    ev << "DUAL: " << eigrpDual::userMsgs[event];
    ev << " for route " << route->getRouteAddress() << " via " << source->getNextHop();
    ev << " (" << source->getMetric() << "/" << source->getRd() << ")" << endl;

    switch (route->getQueryOrigin())
    {
    case 0: // active state
        processQo0(event, source, route, neighborId, isSourceNew);
        break;

    case 1:
        if (route->getReplyStatusSum() == 0)
        { // passive state
            processQo1Passive(event, source, route, neighborId, isSourceNew);
        }
        else
        { // active state
            processQo1Active(event, source, route, neighborId, isSourceNew);
        }
        break;

    case 2: // active state
        processQo2(event, source, route, neighborId, isSourceNew);
        break;

    case 3: // active state
        processQo3(event, source, route, neighborId, isSourceNew);
        break;

    default:
        //DUAL detected invalid state of route
        ASSERT(false);
        break;
    }
}

//
// States of DUAL
//

void EigrpDual::processQo0(DualEvent event, EigrpRouteSource<IPv4Address> *source, EigrpRoute<IPv4Address> *route, int neighborId, bool isSourceNew)
{
    uint64_t dmin;
    bool hasReplyStatus;

    switch (event)
    {
    case INTERFACE_UP:
        processTransition7(event, source, route, neighborId);
        break;

    case RECV_UPDATE:
        processTransition7(event, source, route, neighborId);
        break;

    case RECV_QUERY:
        if (source->isSuccessor())
            processTransition5(event, source, route, neighborId);
        else
            processTransition6(event, source, route, neighborId);
        break;

    case RECV_REPLY:
    case NEIGHBOR_DOWN:
    case INTERFACE_DOWN:
        if ((hasReplyStatus = route->unsetReplyStatus(neighborId)) == true)
            EV << "     Clear handle, reply status summary = " << route->getReplyStatusSum() << endl;

        if (route->getReplyStatusSum() == 0)
        { // As last reply from neighbor
            // Check FC with FDij(t)
            if (pdm->hasFeasibleSuccessor(route, dmin))
                processTransition14(event, source, route, dmin, neighborId);
            else
                processTransition11(event, source, route, dmin, neighborId);
        }
        else if (hasReplyStatus)
        { // As not last reply from neighbor
            processTransition8(event, source, route, neighborId, isSourceNew);
        }
        // else do nothing (connected source)
        break;

    case LOST_ROUTE:
        // do nothing
        break;

    default:
        ev << "DUAL received invalid input event num. " << event << " in active state 0" << endl;
        ASSERT(false);
        break;
    }
}

void EigrpDual::processQo1Passive(DualEvent event, EigrpRouteSource<IPv4Address> *source, EigrpRoute<IPv4Address> *route, int neighborId, bool isSourceNew)
{
    uint64_t dmin;

    switch (event)
    {
    case LOST_ROUTE:
    case INTERFACE_UP:
    case RECV_UPDATE:
        if (event == LOST_ROUTE) // Force loss of all sources of the route
            route->setFd(0);

        if (pdm->hasFeasibleSuccessor(route, dmin))
            processTransition2(event, source, route, dmin, neighborId);
        else
            processTransition4(event, source, route, dmin, neighborId);
        break;

    case NEIGHBOR_DOWN:
    case INTERFACE_DOWN:
        if (pdm->hasFeasibleSuccessor(route, dmin))
            processTransition2(event, source, route, dmin, neighborId);
        else
            processTransition4(event, source, route, dmin, neighborId);
        break;

    case RECV_QUERY:
        if (route->getNumSucc() == 0 || pdm->hasFeasibleSuccessor(route, dmin))
        { // No successor to the destination or FC satisfied
            if (source->isSuccessor())
                processTransition2(event, source, route, dmin, neighborId);
            else
                processTransition1(event, source, route, dmin, neighborId);
        }
        else
        {
            if (source->isSuccessor())
                processTransition3(event, source, route, dmin, neighborId);
            else
                processTransition4(event, source, route, dmin, neighborId);
        }
        break;

    default:
        ASSERT(false);
        ev << "DUAL received invalid input event in passive state 0, skipped" << endl;
        break;
    }
}

void EigrpDual::processQo1Active(DualEvent event, EigrpRouteSource<IPv4Address> *source, EigrpRoute<IPv4Address> *route, int neighborId, bool isSourceNew)
{
    bool hasReplyStatus;

    switch (event)
    {
    case INTERFACE_UP:
        processTransition17(event, source, route, neighborId);
        break;

    case RECV_UPDATE:
        if (source->isSuccessor())
            processTransition9(event, source, route, neighborId);
        else
            processTransition17(event, source, route, neighborId);
        break;

    case RECV_QUERY:
        if (source->isSuccessor())
            processTransition5(event, source, route, neighborId);
        else
            processTransition6(event, source, route, neighborId);
        break;

    case RECV_REPLY:
        if ((hasReplyStatus = route->unsetReplyStatus(neighborId)) == true)
            EV << "     Clear handle, reply status summary = " << route->getReplyStatusSum() << endl;

        if (route->getReplyStatusSum() == 0) // Last reply
            processTransition15(event, source, route, neighborId);
        else if (hasReplyStatus) // Not last reply
            processTransition18(event, source, route, neighborId, isSourceNew);
        // else do nothing
        break;

    case NEIGHBOR_DOWN:
    case INTERFACE_DOWN:
        if ((hasReplyStatus = route->unsetReplyStatus(neighborId)) == true)
            EV << "     Clear handle, reply status summary = " << route->getReplyStatusSum() << endl;

        // Transition 9 should take precedence over transition 15 (fail of link
        // to S (as last reply) versus fail of link to not S (as last reply)
        // TODO ověřit!
        if (source->isSuccessor())
            processTransition9(event, source, route, neighborId);

        if (route->getReplyStatusSum() == 0)
        { // As last reply from neighbor
            processTransition15(event, source, route, neighborId);
        }
        else if (hasReplyStatus)
        { // As not last reply from neighbor
            processTransition18(event, source, route, neighborId, isSourceNew);
        }
        // else do nothing
        break;

    case LOST_ROUTE:
        // do nothing
        break;

    default:
        ASSERT(false);
        ev << "DUAL received invalid input event in active state 1, skipped" << endl;
        break;
    }
}

void EigrpDual::processQo2(DualEvent event, EigrpRouteSource<IPv4Address> *source, EigrpRoute<IPv4Address> *route, int neighborId, bool isSourceNew)
{
    uint64_t dmin;
    bool hasReplyStatus;

    switch (event)
    {
    case INTERFACE_UP:
        processTransition7(event, source, route, neighborId);
        break;

    case RECV_UPDATE:
        processTransition7(event, source, route, neighborId);
        break;

    case RECV_QUERY:
        if (!source->isSuccessor())
            processTransition6(event, source, route, neighborId);

        else
        {
            ASSERT(false);
            // do nothing (DUAL can not receive Query form S when it is in active state oij=2)
        }
        break;

    case RECV_REPLY:
    case NEIGHBOR_DOWN:
    case INTERFACE_DOWN:
        if ((hasReplyStatus = route->unsetReplyStatus(neighborId)) == true)
            EV << "     Clear handle, reply status summary = " << route->getReplyStatusSum() << endl;

        if (route->getReplyStatusSum() == 0)
        { // As last reply from neighbor
            // Check FC with FDij(t)
            if (pdm->hasFeasibleSuccessor(route, dmin))
                processTransition16(event, source, route, dmin, neighborId);
            else
                processTransition12(event, source, route, dmin, neighborId);
        }
        else if (hasReplyStatus)
        { // As not last reply from neighbor
            processTransition8(event, source, route, neighborId, isSourceNew);
        }
        // else do nothing (connected source)
        break;

    case LOST_ROUTE:
        // do nothing
        break;

    default:
        ASSERT(false);
        ev << "DUAL received invalid input event in active state 2, skipped" << endl;
        break;
    }
}

void EigrpDual::processQo3(DualEvent event, EigrpRouteSource<IPv4Address> *source, EigrpRoute<IPv4Address> *route, int neighborId, bool isSourceNew)
{
    bool hasReplyStatus;

    switch (event)
    {
    case INTERFACE_UP:
        processTransition17(event, source, route, neighborId);
        break;

    case RECV_UPDATE:
        if (source->isSuccessor())
            processTransition10(event, source, route, neighborId);
        else
            processTransition17(event, source, route, neighborId);
        break;

    case RECV_QUERY:
        if (!source->isSuccessor())
            processTransition6(event, source, route, neighborId);
        else
        {
            ASSERT(false);
            // do nothing (DUAL can not receive Query form S when it is in active state oij=3)
        }
        break;

    case RECV_REPLY:
        if ((hasReplyStatus = route->unsetReplyStatus(neighborId)) == true)
            EV << "     Clear handle, reply status summary = " << route->getReplyStatusSum() << endl;

        if (route->getReplyStatusSum() == 0) // Last reply
            processTransition13(event, source, route, neighborId);
        else if (hasReplyStatus) // Not last reply
            processTransition18(event, source, route, neighborId, isSourceNew);
        // else do nothing
        break;

    case NEIGHBOR_DOWN:
    case INTERFACE_DOWN:
        if ((hasReplyStatus = route->unsetReplyStatus(neighborId)) == true)
            EV << "     Clear handle, reply status summary = " << route->getReplyStatusSum() << endl;

        // Transition 10 should take precedence over transition 13 (fail of link
        // to S (as last reply) versus fail of link to not S (as last reply)
        // TODO ověřit!
        if (source->isSuccessor())
            processTransition10(event, source, route, neighborId);

        if (route->getReplyStatusSum() == 0)
        { // As last reply from neighbor
            processTransition13(event, source, route, neighborId);
        }
        else if (hasReplyStatus)
        { // As not last reply from neighbor
            processTransition18(event, source, route, neighborId, isSourceNew);
        }
        // else do nothing
        break;

    case LOST_ROUTE:
        // do nothing
        break;

    default:
        ASSERT(false);
        ev << "DUAL received invalid input event in active state 3, skipped" << endl;
        break;
    }
}

//
// Transitions of DUAL
//

void EigrpDual::processTransition1(int event, EigrpRouteSource<IPv4Address> *source, EigrpRoute<IPv4Address> *route, uint64_t dmin, int neighborId)
{
    EV << "DUAL: transit from oij=1 (passive) to oij=1 (passive) by transition 1" << endl;

    EigrpRouteSource<IPv4Address> *successor = pdm->getBestSuccessor(route);
    if (successor == NULL)
    { // There is no successor, reply with unreachable route
        pdm->sendReply(route, neighborId, source, false, true);
    }
    else
        pdm->sendReply(route, neighborId, successor);

    // Route will be removed after router receives Ack from neighbor for Reply
    if (source->isUnreachable())
        pdm->setDelayedRemove(neighborId, source);
}

void EigrpDual::processTransition2(int event, EigrpRouteSource<IPv4Address> *source, EigrpRoute<IPv4Address> *route, uint64_t dmin, int neighborId)
{
    EV << "DUAL: transit from oij=1 (passive) to oij=1 (passive) by transition 2" << endl;

    EigrpRouteSource<IPv4Address> *successor;
    uint64_t oldDij;    // Dij before the event
    bool rtableChanged = false;

    oldDij = route->getDij();

    // Find successors and update route in TT and RT
    successor = pdm->updateRoute(route, dmin, &rtableChanged);

    if (event == RECV_QUERY)
    {
        ASSERT(successor != NULL);
        if (neighborId == successor->getNexthopId()) // Poison Reverse
            pdm->sendReply(route, neighborId, successor, true);
        else
            pdm->sendReply(route, neighborId, successor);
        // When Reply is sent, remove unreachable route after receiving Ack (According to Cisco EIGRP 10.0)
        if (source->isUnreachable())
            pdm->setDelayedRemove(neighborId, source);
    }
    else
    { // When Reply is not sent, remove unreachable route immediately
        if (source->isUnreachable())
            invalidateRoute(source);
    }

    // Send Update about new Successor
    if (successor != NULL && pdm->hasNeighborForUpdate(successor))
    {
        if (rtableChanged)
            pdm->sendUpdate(IEigrpPdm::UNSPEC_RECEIVER, route, successor, true, "RT changed");
        // Nedavno zmeneno: pokud nastane zmena metriky a cesta neni connected, pak se uplatni Split Horizon (ne Poison Reverse), sem nepatri zmena z inf na non inf hodnotu (to pokryje zmena tabulky)
        else if (route->getDij() != oldDij)
        {
            bool forcePoisonRev = successor->getNexthopId() == IEigrpPdm::CONNECTED_ROUTE;
            pdm->sendUpdate(IEigrpPdm::UNSPEC_RECEIVER, route, successor, forcePoisonRev, "metric changed");
        }
    }
}

void EigrpDual::processTransition3(int event, EigrpRouteSource<IPv4Address> *source, EigrpRoute<IPv4Address> *route, uint64_t dmin, int neighborId)
{
    EV << "DUAL: transit from oij=1 (passive) to oij=3 (active) by transition 3" << endl;
    // Note: source is successor

    int numPeers = 0, numStubs = 0;
    bool gotoActive;
    route->setQueryOrigin(3);

    // Actualize distance of route and FD in TT
    // route->setDij(source->getMetric()); // not there (in transition to passive state DUAL can not detect change of Dij)
    route->setRdPar(source->getMetricParams());
    route->setFd(route->getDij());

    // Send Query with actual distance via successor to all peers
    gotoActive = pdm->setReplyStatusTable(route, source, false, &numPeers, &numStubs);
    EV << "DUAL: peers = " << numPeers << ", stubs = " << numStubs << endl;

    if (gotoActive)
    {
        pdm->sendQuery(IEigrpPdm::UNSPEC_RECEIVER, route, source);
    }
    else
    { // Diffusion computation can not be performed, go back to passive state
        processTransition13(event, source, route, neighborId);
        return;
    }

    // Do not remove source
}

void EigrpDual::processTransition4(int event, EigrpRouteSource<IPv4Address> *source, EigrpRoute<IPv4Address> *route, uint64_t dmin, int neighborId)
{
    EV << "DUAL: transit from oij=1 (passive) to oij=1 (active) by transition 4" << endl;

    int numPeers = 0, numStubs = 0;
    bool gotoActive;
    EigrpRouteSource<IPv4Address> *oldSuccessor;

    route->setQueryOrigin(1);

    // Get old successor
    oldSuccessor = pdm->getBestSuccessor(route);
    // Old successor may not be null
    if (oldSuccessor == NULL) oldSuccessor = source;

    // Actualize distance of route in TT
    if (event == LOST_ROUTE)    // Set distance and router RD to inf
        route->setUnreachable();
    else
    {
        route->setDij(oldSuccessor->getMetric());
        route->setRdPar(oldSuccessor->getMetricParams());
    }
    // Actualize FD in TT
    route->setFd(route->getDij());

    // Send Reply with RDij if input event is Query
    if (event == RECV_QUERY)
        pdm->sendReply(route, neighborId, oldSuccessor);

    // Start own diffusion computation
    gotoActive = pdm->setReplyStatusTable(route, source, true, &numPeers, &numStubs);
    EV << "DUAL: peers = " << numPeers << ", stubs = " << numStubs << endl;
    if (gotoActive)
    {
        pdm->sendQuery(IEigrpPdm::UNSPEC_RECEIVER, route, oldSuccessor, true);
    }
    else
    { // Go to passive state
        processTransition15(event, source, route, neighborId);
        return;
    }

    // Do not remove source
}

void EigrpDual::processTransition5(int event, EigrpRouteSource<IPv4Address> *source, EigrpRoute<IPv4Address> *route, int neighborId)
{
    EV << "DUAL: transit from oij=" << route->getQueryOrigin() << " (active) to oij=2 (active) by transition 5" << endl;

    route->setQueryOrigin(2);

    // Do not delete source
}

void EigrpDual::processTransition6(int event, EigrpRouteSource<IPv4Address> *source, EigrpRoute<IPv4Address> *route, int neighborId)
{
    EV << "DUAL: transit from oij=" << route->getQueryOrigin() << " (active) to oij=" << route->getQueryOrigin() << " (active) by transition 6" << endl;

    EigrpRouteSource<IPv4Address> *oldSuccessor = pdm->getBestSuccessor(route);
    ASSERT(oldSuccessor != NULL);   // Old successor must be available until transition to passive state
    /*if (oldSuccessor == NULL) // Send route with unreachable distance (not Poison Reverse)
        pdm->sendReply(route, neighborId, source, false, true);
    else*/

    if (source->isSuccessor()) // Send route with unreachable distance to old Successor (Poison Reverse)
        pdm->sendReply(route, neighborId, oldSuccessor, true);
    else
        pdm->sendReply(route, neighborId, oldSuccessor);

    // Do not remove unreachable route (this will be done after transition to passive state)
}

void EigrpDual::processTransition7(int event, EigrpRouteSource<IPv4Address> *source, EigrpRoute<IPv4Address> *route, int neighborId)
{
    EV << "DUAL: transit from oij=" << route->getQueryOrigin() << " (active) to oij=" << route->getQueryOrigin() << " (active) by transition 7" << endl;

    // Do not actualize Dij of route by new distance via successor (in transition to passive state DUAL can not detect change of Dij)
    // route->setDij(source->getMetric());

    // Do not remove unreachable route (this will be done after transition to passive state)
}

void EigrpDual::processTransition8(int event, EigrpRouteSource<IPv4Address> *source, EigrpRoute<IPv4Address> *route, int neighborId, bool isSourceNew)
{
    EV << "DUAL: transit from oij=" << route->getQueryOrigin() << " (active) to oij=" << route->getQueryOrigin() << " (active) by transition 8" << endl;

    if (source->isUnreachable() && isSourceNew)
        invalidateRoute(source);
}

void EigrpDual::processTransition9(int event, EigrpRouteSource<IPv4Address> *source, EigrpRoute<IPv4Address> *route, int neighborId)
{
    EV << "DUAL: transit from oij=1 (active) to oij=0 (active) by transition 9" << endl;

    route->setQueryOrigin(0);

    // Actualize Dij of route
    route->setDij(source->getMetric());

    if (route->getReplyStatusSum() == 0)
    { // Reply status table is empty, go to passive state or start new diffusing computation
        uint64_t dmin;
        if (pdm->hasFeasibleSuccessor(route, dmin))
            processTransition14(event, source, route, dmin, neighborId);
        else
            processTransition11(event, source, route, dmin, neighborId);
    }

    // Do not remove unreachable route (this will be done after transition to passive state)
}

void EigrpDual::processTransition10(int event, EigrpRouteSource<IPv4Address> *source, EigrpRoute<IPv4Address> *route, int neighborId)
{
    EV << "DUAL: transit from oij=3 (active) to oij=2 (active) by transition 10" << endl;

    route->setQueryOrigin(2);

    // Actualize Dij of route
    route->setDij(source->getMetric());

    if (route->getReplyStatusSum() == 0)
    { // Reply status table is empty, go to passive state or start new diffusing computation
        uint64_t dmin;
        if (pdm->hasFeasibleSuccessor(route, dmin))
            processTransition16(event, source, route, dmin, neighborId);
        else
            processTransition12(event, source, route, dmin, neighborId);
    }

    // Do not remove unreachable route (this will be done after transition to passive state)
}

void EigrpDual::processTransition11(int event, EigrpRouteSource<IPv4Address> *source, EigrpRoute<IPv4Address> *route, uint64_t dmin, int neighborId)
{
    EV << "DUAL: transit from oij=0 (active) to oij=1 (active) by transition 11" << endl;

    int numPeers = 0, numStubs = 0;
    bool gotoActive;

    route->setQueryOrigin(1);

    gotoActive = pdm->setReplyStatusTable(route, source, false, &numPeers, &numStubs);
    EV << "DUAL: peers = " << numPeers << ", stubs = " << numStubs << endl;
    if (gotoActive)
    { // Start new diffusion computation
        int srcNeighbor = (neighborId != IEigrpPdm::UNSPEC_RECEIVER) ? neighborId : IEigrpPdm::UNSPEC_SENDER;
        pdm->sendQuery(IEigrpPdm::UNSPEC_RECEIVER, route, source, srcNeighbor);
    }
    else
    { // Go to passive state
        processTransition15(event, source, route, neighborId);
        return;
    }

    // Do not remove unreachable route (this will be done after transition to passive state)
}

void EigrpDual::processTransition12(int event, EigrpRouteSource<IPv4Address> *source, EigrpRoute<IPv4Address> *route, uint64_t dmin, int neighborId)
{
    EV << "DUAL: transit from oij=2 (active) to oij=3 (active) by transition 12" << endl;

    int numPeers = 0, numStubs = 0;
    bool gotoActive;

    route->setQueryOrigin(3);

    gotoActive = pdm->setReplyStatusTable(route, source, false, &numPeers, &numStubs);
    EV << "DUAL: peers = " << numPeers << ", stubs = " << numStubs << endl;
    if (gotoActive)
    { // Start new diffusion computation
        pdm->sendQuery(IEigrpPdm::UNSPEC_RECEIVER, route, source);
    }
    else
    { // Go to passive state
        processTransition13(event, source, route, neighborId);
        return;
    }

    // Do not remove unreachable route (this will be done after transition to passive state)
}

void EigrpDual::processTransition13(int event, EigrpRouteSource<IPv4Address> *source, EigrpRoute<IPv4Address> *route, int neighborId)
{
    EV << "DUAL: transit from oij=3 (active) to oij=1 (passive) by transition 13" << endl;

    EigrpRouteSource<IPv4Address> *successor;
    // Old successor is originator of Query
    EigrpRouteSource<IPv4Address> *oldSuccessor = pdm->getBestSuccessor(route);
    uint64_t oldDij = route->getDij();
    uint64_t dmin;
    bool rtableChanged = false;

    route->setQueryOrigin(1);

    // Set FD to max
    route->setFd(EigrpMetricHelper::METRIC_INF);

    // Find min distance
    dmin = pdm->findRouteDMin(route);

    // Find successor and update distance of the route
    successor = pdm->updateRoute(route, dmin, &rtableChanged, true);
    if (source->isUnreachable())
        invalidateRoute(source);

    // Send Reply to old successor
    if (oldSuccessor != NULL)
    {
        if (successor == NULL) // Send old Successor
        {
            // Route will be removed after router receives Ack from neighbor for Reply
            if (oldSuccessor->isUnreachable())
                pdm->setDelayedRemove(oldSuccessor->getNexthopId(), oldSuccessor);
            pdm->sendReply(route, oldSuccessor->getNexthopId(), oldSuccessor, true);
        }
        else
            pdm->sendReply(route, oldSuccessor->getNexthopId(), successor, true);
    }

    // Send update about change of metric only if there is successor
    if (successor != NULL && pdm->hasNeighborForUpdate(successor))
    {
        if (rtableChanged)
            pdm->sendUpdate(IEigrpPdm::UNSPEC_RECEIVER, route, successor, true, "RT changed");
        else if (route->getDij() != oldDij)
        {
            bool forcePoisonRev = successor->getNexthopId() == IEigrpPdm::CONNECTED_ROUTE;
            pdm->sendUpdate(IEigrpPdm::UNSPEC_RECEIVER, route, successor, forcePoisonRev, "metric changed");
        }
    }

    pdm->sendUpdateToStubs(successor, oldSuccessor, route);
}

void EigrpDual::processTransition14(int event, EigrpRouteSource<IPv4Address> *source, EigrpRoute<IPv4Address> *route, uint64_t dmin, int neighborId)
{
    EV << "DUAL: transit from oij=0 (active) to oij=1 (passive) by transition 14" << endl;

    EigrpRouteSource<IPv4Address> *successor = NULL, *oldSuccessor = NULL;
    uint64_t oldDij = route->getDij();
    bool rtableChanged = false;

    route->setQueryOrigin(1);
    oldSuccessor = pdm->getBestSuccessor(route);

    // Find successor and update distance of the route
    successor = pdm->updateRoute(route, dmin, &rtableChanged, true);
    if (source->isUnreachable())
    { // There is not necessary wait for Ack from neighbor before delete source (Reply is not sent)
        invalidateRoute(source);
    }

    // Do not send Reply (Reply was sent in transition 4)

    // Send update about change of metric
    if (successor != NULL && pdm->hasNeighborForUpdate(successor))
    {
        if (rtableChanged)
            pdm->sendUpdate(IEigrpPdm::UNSPEC_RECEIVER, route, successor, true, "RT changed");
        else if (route->getDij() != oldDij)
        {
            bool forcePoisonRev = successor->getNexthopId() == IEigrpPdm::CONNECTED_ROUTE;
            pdm->sendUpdate(IEigrpPdm::UNSPEC_RECEIVER, route, successor, forcePoisonRev, "metric changed");
        }
    }

    pdm->sendUpdateToStubs(successor, oldSuccessor, route);
}

void EigrpDual::processTransition15(int event, EigrpRouteSource<IPv4Address> *source, EigrpRoute<IPv4Address> *route, int neighborId)
{
    EV << "DUAL: transit from oij=1 (active) to oij=1 (passive) by transition 15" << endl;

    EigrpRouteSource<IPv4Address> *successor = NULL, *oldSuccessor = NULL;
    uint64_t oldDij = route->getDij();
    uint64_t dmin;
    bool rtableChanged = false;

    // Set FD to max
    route->setFd(EigrpMetricHelper::METRIC_INF);
    oldSuccessor = pdm->getBestSuccessor(route);

    // Find min distance
    dmin = pdm->findRouteDMin(route);

    successor = pdm->updateRoute(route, dmin, &rtableChanged, true);
    if (source->isUnreachable())
    { // There is not necessary wait for Ack from neighbor before delete source (Reply is not sent)
        invalidateRoute(source);
    }

    // Do not send Reply (Reply was sent in transition 4)

    // Send update about change
    if (successor != NULL && pdm->hasNeighborForUpdate(successor))
    {
        if (rtableChanged)
            pdm->sendUpdate(IEigrpPdm::UNSPEC_RECEIVER, route, successor, true, "RT changed");
        else if (route->getDij() != oldDij)
        {
            bool forcePoisonRev = successor->getNexthopId() == IEigrpPdm::CONNECTED_ROUTE;
            pdm->sendUpdate(IEigrpPdm::UNSPEC_RECEIVER, route, successor, forcePoisonRev, "metric changed");
        }
    }

    pdm->sendUpdateToStubs(successor, oldSuccessor, route);
}

void EigrpDual::processTransition16(int event, EigrpRouteSource<IPv4Address> *source, EigrpRoute<IPv4Address> *route, uint64_t dmin, int neighborId)
{
    EV << "DUAL: transit from oij=2 (active) to oij=1 (passive) by transition 16" << endl;

    EigrpRouteSource<IPv4Address> *successor;
    // Old successor is originator of Query
    EigrpRouteSource<IPv4Address> *oldSuccessor = pdm->getBestSuccessor(route);
    uint64_t oldDij = route->getDij();
    bool rtableChanged = false;

    route->setQueryOrigin(1);

    // Find successor and update distance of the route
    successor = pdm->updateRoute(route, dmin, &rtableChanged, true);
    ASSERT(successor != NULL);  // There must be successor

    if (source->isUnreachable())
    { // There is not necessary wait for Ack from neighbor for Reply before delete source (successor may not be NULL)
        invalidateRoute(source);
    }

    // Send Reply to old successor
    if (oldSuccessor != NULL)
        pdm->sendReply(route, oldSuccessor->getNexthopId(), successor, true);

    // Send update about change of metric
    if (pdm->hasNeighborForUpdate(successor))
    {
        if (rtableChanged)
            pdm->sendUpdate(IEigrpPdm::UNSPEC_RECEIVER, route, successor, true, "RT changed");
        else if (route->getDij() != oldDij)
        {
            bool forcePoisonRev = successor->getNexthopId() == IEigrpPdm::CONNECTED_ROUTE;
            pdm->sendUpdate(IEigrpPdm::UNSPEC_RECEIVER, route, successor, forcePoisonRev, "metric changed");
        }
    }

    pdm->sendUpdateToStubs(successor, oldSuccessor, route);
}

void EigrpDual::processTransition17(int event, EigrpRouteSource<IPv4Address> *source, EigrpRoute<IPv4Address> *route, int neighborId)
{
    EV << "DUAL: transit from oij=" << route->getQueryOrigin() << " (active) to oij=" << route->getQueryOrigin() << " (active) by transition 17" << endl;

    // Do not actualize Dij of route by new distance via successor (in transition to passive state DUAL can not detect change of Dij)
    // route->setDij(source->getMetric());
}

void EigrpDual::processTransition18(int event, EigrpRouteSource<IPv4Address> *source, EigrpRoute<IPv4Address> *route, int neighborId, bool isSourceNew)
{
    EV << "DUAL: transit from oij=" << route->getQueryOrigin() << " (active) to oij=" << route->getQueryOrigin() << " (active) by transition 18" << endl;

    if (source->isUnreachable() && isSourceNew)
        invalidateRoute(source);
}

