// examples/uaf/src/client/easiest_client_example.cpp

// Start the demo server ($SDK/bin/uaservercpp) of Unified Automation before running this script!

#include "uaf/client/client.h"

using namespace uaf;
using namespace std;



int main(int argc, char* argv[])
{
    // always initialize the framework first!
    initializeUaf();

    // set-up the client settings, and provide the discovery URL of the demo server (uaservercpp):
    ClientSettings settings;
    settings.applicationName = "myClient";
    settings.discoveryUrls.push_back("opc.tcp://localhost:48010");
    // settings.logToStdOutLevel = loglevels::Debug; // uncomment to see debugging output

    // create the OPC UA client:
    Client myClient(settings);

    // specify the address of the node of which we would like to read its Value attribute:
    // --> here we're using an absolute address, i.e. an NodeId(<identifier>, <namespace URI>)
    //     and a <server URI>:
    NodeId nodeId("Demo.SimulationSpeed", "http://www.unifiedautomation.com/DemoServer");
    string serverUri("urn:UnifiedAutomation:UaServerCpp");

    vector<Address> nodesToBeRead;
    nodesToBeRead.push_back(Address(nodeId, serverUri));

    // read the Value attribute of the node that we just addressed:
    ReadResult result;
    Status status = myClient.read(nodesToBeRead,
                                  attributeids::Value,                       // read the Value attribute
                                  uaf::constants::CLIENTHANDLE_NOT_ASSIGNED, // let the UAF pick a suitable Session
                                  NULL,                                      // default settings for the Read service
                                  NULL,                                      // default settings for the Translate service
                                  NULL,                                      // default session settings
                                  result);

    if (status.isGood())
        cout << "The value is " << result.targets[0].data.toString() << "\n";
    else
        cout << "There was an error: " << status.toString() << "\n";

    return 0;
}
