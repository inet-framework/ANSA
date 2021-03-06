// Copyright (C) 2013 OpenSim Ltd.
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
// Author: Benjamin Martin Seregi

#ifndef __INET_CDPRELAY_H
#define __INET_CDPRELAY_H

#include "inet/common/INETDefs.h"
#include "inet/networklayer/common/InterfaceTable.h"
#include "inet/linklayer/ethernet/switch/IMACAddressTable.h"
#include "inet/linklayer/ethernet/EtherFrame_m.h"
#include "inet/common/lifecycle/NodeOperations.h"
#include "inet/common/lifecycle/NodeStatus.h"

#include "ansa/linklayer/cdp/CDPUpdate.h"
#include "ansa/linklayer/lldp/LLDPUpdate.h"

namespace inet {

//
// This module forward frames (~EtherFrame) based on their destination MAC addresses to appropriate ports.
// See the NED definition for details.
//
class INET_API relayUnit : public cSimpleModule, public ILifecycle
{
  public:
    relayUnit();

  protected:
    MACAddress bridgeAddress;
    MACAddress bridgeGroupCDPAddress;
    MACAddress bridgeGroupLLDPAddress;
    IInterfaceTable *ifTable = nullptr;
    IMACAddressTable *macTable = nullptr;
    InterfaceEntry *ie = nullptr;
    bool isOperational = false;
    bool isCDPAware = false;
    bool isLLDPAware = false;
    unsigned int portCount = 0;    // number of ports in the switch

    // statistics: see finish() for details.
    int numReceivedNetworkFrames = 0;
    int numDroppedFrames = 0;
    int numReceivedUpdatesFromCDP = 0;
    int numDeliveredUpdatesToCDP = 0;
    int numReceivedUpdatesFromLLDP = 0;
    int numDeliveredUpdatesToLLDP = 0;
    int numDispatchedNonUpdateFrames = 0;
    int numDispatchedUpdateFrames = 0;

  protected:
    virtual void initialize(int stage) override;
    virtual int numInitStages() const override { return NUM_INIT_STAGES; }
    virtual void handleMessage(cMessage *msg) override;

    /**
     * Updates address table (if the port is in learning state)
     * with source address, determines output port
     * and sends out (or broadcasts) frame on ports
     * (if the ports are in forwarding state).
     * Includes calls to updateTableWithAddress() and getPortForAddress().
     *
     */
    void handleAndDispatchFrame(EtherFrame *frame);
    void dispatch(EtherFrame *frame, unsigned int portNum);
    void learn(EtherFrame *frame);
    void broadcast(EtherFrame *frame);

    void dispatchCDPUpdate(CDPUpdate *cdpUpdate, int port);
    void deliverCDPUpdate(EtherFrame *frame);

    void dispatchLLDPUpdate(LLDPUpdate *lldpUpdate, int port);
    void deliverLLDPUpdate(EtherFrame *frame);

    // For lifecycle
    virtual void start();
    virtual void stop();
    bool handleOperationStage(LifecycleOperation *operation, int stage, IDoneCallback *doneCallback) override;

    /*
     * Returns the first non-loopback interface.
     */
    virtual InterfaceEntry *chooseInterface();
    virtual void finish() override;
};

} // namespace inet

#endif // ifndef __INET_CDPRELAY_H

