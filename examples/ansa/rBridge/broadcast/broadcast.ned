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

package inet.examples.ansa.rBridge.broadcast;

import ansa.node.ANSA_RBridge;
//import inet.nodes.ethernet.Eth100M;
import inet.node.ethernet.Eth100M;


//
// TODO Auto-generated network
//
network a
{
    @display("bgb=509,354");
    submodules:
        rBridge1: ANSA_RBridge {
            @display("p=87,68");
            gates:
                ethg[1];
        }
        rBridge2: ANSA_RBridge {
            @display("p=243,68");
            gates:
                ethg[1];
        }
        
    connections:
        rBridge1.ethg[0] <--> Eth100M <--> rBridge2.ethg[0];

}
