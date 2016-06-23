/* This file is part of the UAF (Unified Architecture Framework) project.
 *
 * Copyright (C) 2012 Wim Pessemier (Institute of Astronomy, KULeuven)
 *
 * Project website: http://www.ster.kuleuven.be/uaf
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation, either version 3 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "uaf/client/client.h"


namespace uaf
{
    using namespace uaf;
    using std::string;
    using std::map;
    using std::vector;
    using std::size_t;
    using std::pair;


    // Constructor
    // =============================================================================================
    Client::Client()
    {
        logger_ = new Logger("", "Client");
        logger_->loggerFactory()->registerCallbackInterface(this);
        construct();
    }

    // Constructor
    // =============================================================================================
    Client::Client(const string& applicationName)
    {
        logger_ = new Logger(applicationName, "Client");
        logger_->loggerFactory()->registerCallbackInterface(this);
        construct();
        // update the settings
        setClientSettings(ClientSettings(applicationName));
    }


    // Constructor
    // =============================================================================================
    Client::Client(const uaf::ClientSettings& settings)
    {
        logger_ = new Logger(settings.applicationName, "Client");
        logger_->loggerFactory()->registerCallbackInterface(this);
        construct();
        setClientSettings(settings);
    }


    // Constructor
    // =============================================================================================
    Client::Client(const uaf::ClientSettings& settings, LoggerFactory* loggerFactory)
    {
        logger_ = loggerFactory->createLogger("Client");
        construct();
        // update the name
        setClientSettings(settings);
    }


    // Common constructor code
    // =============================================================================================
    void Client::construct()
    {
        currentRequestHandle_ = 0;
        doFinishThread_ = false;

        database_       = new Database(logger_->loggerFactory());
        discoverer_     = new Discoverer(logger_->loggerFactory(), database_);
        sessionFactory_ = new SessionFactory(logger_->loggerFactory(), this, discoverer_, database_);
        resolver_       = new Resolver(logger_->loggerFactory(), sessionFactory_, database_);

        logger_->debug("Now starting the thread to periodically check the requests");

        // start the thread
        start();

        logger_->debug("The client has been constructed");
    }


    // Destructor
    // =============================================================================================
    Client::~Client()
    {
        logger_->debug("Destructing the client");

        // make sure the thread isn't doing anything
        //UaMutexLocker locker(&threadStoppingMutex_);

        doFinishThread_ = true;

        wait();


        delete resolver_;
        resolver_ = 0;

        delete sessionFactory_;
        sessionFactory_ = 0;

        delete discoverer_;
        discoverer_ = 0;

        delete database_;
        database_ = 0;

        delete logger_;
        logger_ = 0;


    }


    // Get a copy of the client config
    //==============================================================================================
    ClientSettings Client::clientSettings() const
    {
        return database_->clientSettings;
    }


    // Set the client config
    //==============================================================================================
    void Client::setClientSettings(const ClientSettings& settings)
    {
        logger_->debug("Now updating the ClientSettings");

        logger_->loggerFactory()->setStdOutLevel(settings.logToStdOutLevel);
        logger_->loggerFactory()->setCallbackLevel(settings.logToCallbackLevel);

        bool doFindServers = (settings.discoveryUrls != database_->clientSettings.discoveryUrls);
        database_->clientSettings = settings;

        if (doFindServers)
        {
            logger_->debug("The discoveryUrls were changed, so we rediscover the system");
            findServersNow();
        }
    }




    // Find the servers now
    // =============================================================================================
    Status Client::findServersNow()
    {
        return discoverer_->findServers();
    }


    // Get the servers that were found
    // =============================================================================================
    const vector<ApplicationDescription>& Client::serversFound() const
    {
        return discoverer_->serversFound();
    }


    // Get the endpoints that were found
    // =============================================================================================
    Status Client::getEndpoints(
            const string&                   discoveryUrl,
            vector<EndpointDescription>&    endpointDescriptions)
    {
        return discoverer_->getEndpoints(discoveryUrl, endpointDescriptions);
    }


    // Read a number of node attributes
    //==============================================================================================
    Status Client::read(
            const std::vector<uaf::Address>&                    addresses,
            uaf::attributeids::AttributeId                      attributeId,
            uaf::ClientConnectionId                             clientConnectionId,
            const uaf::ReadSettings*                            serviceSettings,
            const uaf::TranslateBrowsePathsToNodeIdsSettings*   translateSettings,
            const uaf::SessionSettings*                         sessionSettings,
            uaf::ReadResult&                                    result)
    {
        // log read request
        logger_->debug("Reading %d node attributes", addresses.size());

        ReadRequest request(0, clientConnectionId, serviceSettings, translateSettings, sessionSettings);

        request.targets.reserve(addresses.size());

        for (vector<Address>::const_iterator it = addresses.begin(); it != addresses.end(); ++it)
            request.targets.push_back(ReadRequestTarget(*it, attributeId));

        // perform the read request
        return processRequest(request, result);
    }


    // Read a number of node attributes asynchronously
    //==============================================================================================
    Status Client::beginRead(
            const std::vector<uaf::Address>&                    addresses,
            const uaf::attributeids::AttributeId                attributeId,
            uaf::ClientConnectionId                             clientConnectionId,
            const uaf::ReadSettings*                            serviceSettings,
            const uaf::TranslateBrowsePathsToNodeIdsSettings*   translateSettings,
            const uaf::SessionSettings*                         sessionSettings,
            uaf::AsyncReadResult&                               result)
    {
        // log read request
        logger_->debug("Reading %d node attributes", addresses.size());

        AsyncReadRequest request(0, clientConnectionId, serviceSettings, translateSettings, sessionSettings);

        request.targets.reserve(addresses.size());

        for (vector<Address>::const_iterator it = addresses.begin(); it != addresses.end(); ++it)
            request.targets.push_back(ReadRequestTarget(*it, attributeId));

        // perform the read request
        return processRequest(request, result);
    }


    // Write a number of node attributes
    //==============================================================================================
    Status Client::write(
            const std::vector<uaf::Address>&                    addresses,
            const std::vector<uaf::Variant>&                    data,
            const uaf::attributeids::AttributeId                attributeId,
            uaf::ClientConnectionId                             clientConnectionId,
            const uaf::WriteSettings*                           serviceSettings,
            const uaf::TranslateBrowsePathsToNodeIdsSettings*   translateSettings,
            const uaf::SessionSettings*                         sessionSettings,
            uaf::WriteResult&                                   result)
    {
        // log write request
        logger_->debug("Writing %d node attributes", addresses.size());

        // create a write request
        WriteRequest request(0, clientConnectionId, serviceSettings, translateSettings, sessionSettings);

        // check if the addresses and the data match
        if (addresses.size() != data.size())
            return DataDontMatchAddressesError();

        // reserve some space to speed up the adding of the targets
        request.targets.reserve(addresses.size());

        // fill the targets
        for (std::size_t i = 0; i < addresses.size(); i++)
            request.targets.push_back(WriteRequestTarget(addresses[i], data[i], attributeId));

        // perform the write request
        return processRequest(request, result);
    }


    // Write a number of node attributes asynchronously
    //==============================================================================================
    Status Client::beginWrite(
            const std::vector<uaf::Address>&                    addresses,
            const std::vector<uaf::Variant>&                    data,
            const uaf::attributeids::AttributeId                attributeId,
            uaf::ClientConnectionId                             clientConnectionId,
            const uaf::WriteSettings*                           serviceSettings,
            const uaf::TranslateBrowsePathsToNodeIdsSettings*   translateSettings,
            const uaf::SessionSettings*                         sessionSettings,
            uaf::AsyncWriteResult&                              result)
    {
        // log write request
        logger_->debug("Writing %d node attributes", addresses.size());

        // create a write request
        AsyncWriteRequest request(0, clientConnectionId, serviceSettings, translateSettings, sessionSettings);

        // check if the addresses and the data match
        if (addresses.size() != data.size())
            return uaf::DataDontMatchAddressesError();

        // reserve some space to speed up the adding of the targets
        request.targets.reserve(addresses.size());

        // fill the targets
        for (std::size_t i = 0; i < addresses.size(); i++)
            request.targets.push_back(WriteRequestTarget(addresses[i], data[i], attributeId));

        // perform the write request
        return processRequest(request, result);
    }


    // Call a method synchronously
    //==============================================================================================
    Status Client::call(
            const uaf::Address&                                 objectAddress,
            const uaf::Address&                                 methodAddress,
            const std::vector<uaf::Variant>&                    inputArguments,
            uaf::ClientConnectionId                             clientConnectionId,
            const uaf::MethodCallSettings*                      serviceSettings,
            const uaf::TranslateBrowsePathsToNodeIdsSettings*   translateSettings,
            const uaf::SessionSettings*                         sessionSettings,
            uaf::MethodCallResult&                              result)
    {
        MethodCallRequest request(0,
                                  clientConnectionId,
                                  serviceSettings,
                                  translateSettings,
                                  sessionSettings);

        request.targets.resize(1);
        request.targets[0].objectAddress  = objectAddress;
        request.targets[0].methodAddress  = methodAddress;
        request.targets[0].inputArguments = inputArguments;

        // perform the write request
        return processRequest(request, result);
    }




    // Call a method asynchronously
    //==============================================================================================
    Status Client::beginCall(
            const uaf::Address&                                 objectAddress,
            const uaf::Address&                                 methodAddress,
            const std::vector<uaf::Variant>&                    inputArguments,
            uaf::ClientConnectionId                             clientConnectionId,
            const uaf::MethodCallSettings*                      serviceSettings,
            const uaf::TranslateBrowsePathsToNodeIdsSettings*   translateSettings,
            const uaf::SessionSettings*                         sessionSettings,
            uaf::AsyncMethodCallResult&                         result)
    {
        AsyncMethodCallRequest request(0,
                                       clientConnectionId,
                                       serviceSettings,
                                       translateSettings,
                                       sessionSettings);

        request.targets.resize(1);
        request.targets[0].objectAddress  = objectAddress;
        request.targets[0].methodAddress  = methodAddress;
        request.targets[0].inputArguments = inputArguments;

        // perform the write request
        return processRequest(request, result);
    }


    // Browse a number of nodes
    //==============================================================================================
    uaf::Status Client::browse(
            const std::vector<uaf::Address>&                    addresses,
            uint32_t                                            maxAutoBrowseNext,
            uaf::ClientConnectionId                             clientConnectionId,
            const uaf::BrowseSettings*                          serviceSettingsPtr,
            const uaf::TranslateBrowsePathsToNodeIdsSettings*   translateSettings,
            const uaf::SessionSettings*                         sessionSettings,
            uaf::BrowseResult&                  result)
    {
        // log read request
        logger_->debug("Browsing %d nodes", addresses.size());

        // override the necessary parameters
        BrowseSettings serviceSettingsCopy;

        if (serviceSettingsPtr == NULL)
            serviceSettingsCopy = database_->clientSettings.defaultBrowseSettings;
        else
            serviceSettingsCopy = *serviceSettingsPtr;

        serviceSettingsCopy.maxAutoBrowseNext = maxAutoBrowseNext;

        BrowseRequest request(0,
                              clientConnectionId,
                              &serviceSettingsCopy,
                              translateSettings,
                              sessionSettings);

        request.targets.reserve(addresses.size());

        for (vector<Address>::const_iterator it = addresses.begin(); it != addresses.end(); ++it)
            request.targets.push_back(BrowseRequestTarget(*it));

        // perform the browse request
        return processRequest(request, result);
    }


    // Read raw historical data
    //==============================================================================================
    uaf::Status Client::historyReadRaw(
            const std::vector<uaf::Address>&                    addresses,
            const uaf::DateTime&                                startTime,
            const uaf::DateTime&                                endTime,
            uint32_t                                            numValuesPerNode,
            uint32_t                                            maxAutoReadMore,
            const std::vector<uaf::ByteString>&                 continuationPoints,
            uaf::ClientConnectionId                             clientConnectionId,
            const uaf::HistoryReadRawModifiedSettings*          serviceSettingsPtr,
            const uaf::TranslateBrowsePathsToNodeIdsSettings*   translateSettings,
            const uaf::SessionSettings*                         sessionSettings,
            uaf::HistoryReadRawModifiedResult&                  result)
    {
        // log read request
        logger_->debug("Reading the raw historical data of %d nodes", addresses.size());

        // override the necessary parameters
        HistoryReadRawModifiedSettings serviceSettingsCopy;

        if (serviceSettingsPtr == NULL)
            serviceSettingsCopy = database_->clientSettings.defaultHistoryReadRawModifiedSettings;
        else
            serviceSettingsCopy = *serviceSettingsPtr;

        serviceSettingsCopy.isReadModified    = false;
        serviceSettingsCopy.numValuesPerNode  = numValuesPerNode;
        serviceSettingsCopy.maxAutoReadMore   = maxAutoReadMore;
        serviceSettingsCopy.startTime         = startTime;
        serviceSettingsCopy.endTime           = endTime;

        HistoryReadRawModifiedRequest request(0,
                                              clientConnectionId,
                                              &serviceSettingsCopy,
                                              translateSettings,
                                              sessionSettings);

        bool noContinuationPoints = (continuationPoints.size() == 0);

        request.targets.reserve(addresses.size());
        for (size_t i = 0; i < addresses.size(); i++)
            if (noContinuationPoints)
                request.targets.push_back(HistoryReadRawModifiedRequestTarget(addresses[i]));
            else
                request.targets.push_back(HistoryReadRawModifiedRequestTarget(
                        addresses[i],
                        continuationPoints[i]));

        // perform the request
        return processRequest(request, result);
    }


    // Read modification info of historical data
    //==============================================================================================
    uaf::Status Client::historyReadModified(
            const std::vector<uaf::Address>&                    addresses,
            const uaf::DateTime&                                startTime,
            const uaf::DateTime&                                endTime,
            uint32_t                                            numValuesPerNode,
            uint32_t                                            maxAutoReadMore,
            const std::vector<uaf::ByteString>&                 continuationPoints,
            uaf::ClientConnectionId                             clientConnectionId,
            const uaf::HistoryReadRawModifiedSettings*          serviceSettingsPtr,
            const uaf::TranslateBrowsePathsToNodeIdsSettings*   translateSettings,
            const uaf::SessionSettings*                         sessionSettings,
            uaf::HistoryReadRawModifiedResult&                  result)
    {
        // log read request
        logger_->debug("Reading the historical data modifications of %d nodes", addresses.size());

        // override the necessary parameters
        HistoryReadRawModifiedSettings serviceSettingsCopy;

        if (serviceSettingsPtr == NULL)
            serviceSettingsCopy = database_->clientSettings.defaultHistoryReadRawModifiedSettings;
        else
            serviceSettingsCopy = *serviceSettingsPtr;

        serviceSettingsCopy.isReadModified    = true;
        serviceSettingsCopy.numValuesPerNode  = numValuesPerNode;
        serviceSettingsCopy.maxAutoReadMore   = maxAutoReadMore;
        serviceSettingsCopy.startTime         = startTime;
        serviceSettingsCopy.endTime           = endTime;

        HistoryReadRawModifiedRequest request(0,
                                              clientConnectionId,
                                              &serviceSettingsCopy,
                                              translateSettings,
                                              sessionSettings);

        bool noContinuationPoints = (continuationPoints.size() == 0);

        request.targets.reserve(addresses.size());
        for (size_t i = 0; i < addresses.size(); i++)
            if (noContinuationPoints)
                request.targets.push_back(HistoryReadRawModifiedRequestTarget(addresses[i]));
            else
                request.targets.push_back(HistoryReadRawModifiedRequestTarget(
                        addresses[i],
                        continuationPoints[i]));

        // perform the request
        return processRequest(request, result);
    }


    // Browse a number of nodes
    //==============================================================================================
    uaf::Status Client::browseNext(
            const std::vector<uaf::Address>&                    addresses,
            const std::vector<uaf::ByteString>&                 continuationPoints,
            uaf::ClientConnectionId                             clientConnectionId,
            const uaf::BrowseNextSettings*                      serviceSettings,
            const uaf::TranslateBrowsePathsToNodeIdsSettings*   translateSettings,
            const uaf::SessionSettings*                         sessionSettings,
            uaf::BrowseNextResult&                              result)
    {
        // log read request
        logger_->debug("BrowseNext %d continuation points", continuationPoints.size());

        Status ret;

        if (addresses.size() == continuationPoints.size())
            ret = statuscodes::Good;
        else
            ret = ContinuationPointsDontMatchAddressesError();

        if (ret.isGood())
        {
            BrowseNextRequest request(0,
                                      clientConnectionId,
                                      serviceSettings,
                                      translateSettings,
                                      sessionSettings);

            request.targets.reserve(addresses.size());

            for (size_t i = 0; i < addresses.size(); i++)
                request.targets.push_back(BrowseNextRequestTarget(addresses[i], continuationPoints[i]));

            // perform the browse request
            ret = processRequest(request, result);
        }

        return ret;
    }




    // Start monitoring data items
    //==============================================================================================
    Status Client::createMonitoredData(
            const std::vector<uaf::Address>&                    addresses,
            uaf::ClientConnectionId                             clientConnectionId,
            const uaf::CreateMonitoredDataSettings*             serviceSettings,
            const uaf::TranslateBrowsePathsToNodeIdsSettings*   translateSettings,
            const uaf::SessionSettings*                         sessionSettings,
            uaf::ClientSubscriptionHandle                       clientSubscriptionHandle,
            const uaf::SubscriptionSettings*                    subscriptionSettings,
            uaf::CreateMonitoredDataResult&                     result)
    {
        CreateMonitoredDataRequest request(0,
                                           clientConnectionId,
                                           serviceSettings,
                                           translateSettings,
                                           sessionSettings,
                                           clientSubscriptionHandle,
                                           subscriptionSettings);

        for (vector<Address>::const_iterator it = addresses.begin(); it != addresses.end(); ++it)
        {
            request.targets.push_back(CreateMonitoredDataRequestTarget(*it));
        }

        return processRequest(request, result);
    }




    // Start monitoring event items
    //==============================================================================================
    Status Client::createMonitoredEvents(
            const std::vector<uaf::Address>&                    addresses,
            const uaf::EventFilter&                             eventFilter,
            uaf::ClientConnectionId                             clientConnectionId,
            const uaf::CreateMonitoredEventsSettings*           serviceSettings,
            const uaf::TranslateBrowsePathsToNodeIdsSettings*   translateSettings,
            const uaf::SessionSettings*                         sessionSettings,
            uaf::ClientSubscriptionHandle                       clientSubscriptionHandle,
            const uaf::SubscriptionSettings*                    subscriptionSettings,
            uaf::CreateMonitoredEventsResult&                   result)
    {
        CreateMonitoredEventsRequest request(0,
                                             clientConnectionId,
                                             serviceSettings,
                                             translateSettings,
                                             sessionSettings,
                                             clientSubscriptionHandle,
                                             subscriptionSettings);

        for (size_t i=0; i<addresses.size(); i++)
            request.targets.push_back(CreateMonitoredEventsRequestTarget(addresses[i], eventFilter));

        return processRequest(request, result);
    }


    // Manual connection
    //==============================================================================================
    Status Client::manuallyConnect(
            const string&           serverUri,
            const SessionSettings*  settings,
            ClientConnectionId&     clientConnectionId)
    {
        return sessionFactory_->manuallyConnect(serverUri, settings, clientConnectionId);
    }


    // Manual connection to a specific endpoint
    //==============================================================================================
    Status Client::manuallyConnectToEndpoint(
            const string&           endpointUrl,
            const SessionSettings*  settings,
            const PkiCertificate*   serverCertificate,
            ClientConnectionId&     clientConnectionId)
    {
        return sessionFactory_->manuallyConnectToEndpoint(
                endpointUrl, settings, serverCertificate, clientConnectionId);
    }


    // Manual disconnection
    //==============================================================================================
    Status Client::manuallyDisconnect(ClientConnectionId clientConnectionId)
    {
        return sessionFactory_->manuallyDisconnect(clientConnectionId);
    }


    // Manual disconnection of all sessions
    //==============================================================================================
    void Client::manuallyDisconnectAllSessions()
    {
        return sessionFactory_->deleteAllSessions();
    }


    // Manual subscription
    //==============================================================================================
    Status Client::manuallySubscribe(
            ClientConnectionId          clientConnectionId,
            const SubscriptionSettings* settings,
            ClientSubscriptionHandle&   clientSubscriptionHandle)
    {
        return sessionFactory_->manuallySubscribe(
                clientConnectionId, settings, clientSubscriptionHandle);
    }

    // Manual unsubscription
    //==============================================================================================
    Status Client::manuallyUnsubscribe(
            ClientConnectionId          clientConnectionId,
            ClientSubscriptionHandle    clientSubscriptionHandle)
    {
        return sessionFactory_->manuallyUnsubscribe(clientConnectionId, clientSubscriptionHandle);
    }


    // Get information about the session
    // =============================================================================================
    Status Client::sessionInformation(
            ClientConnectionId  clientConnectionId,
            SessionInformation& sessionInformation)
    {
        return sessionFactory_->sessionInformation(clientConnectionId, sessionInformation);
    }


    // Get information about the session
    // =============================================================================================
    vector<SessionInformation> Client::allSessionInformations()
    {
        return sessionFactory_->allSessionInformations();
    }


    // Get information about the subscription
    // =============================================================================================
    Status Client::subscriptionInformation(
            uaf::ClientSubscriptionHandle  clientSubscriptionHandle,
            SubscriptionInformation&        subscriptionInformation)
    {
        return sessionFactory_->subscriptionInformation(
                clientSubscriptionHandle, subscriptionInformation);
    }


    // Get information about the subscriptions
    // =============================================================================================
    vector<SubscriptionInformation> Client::allSubscriptionInformations()
    {
        return sessionFactory_->allSubscriptionInformations();
    }


    // Get information about the monitored item
    // =============================================================================================
    Status Client::monitoredItemInformation(
            ClientHandle                clientHandle,
            MonitoredItemInformation&   monitoredItemInformation)
    {
        Status ret;

        // first try to find the client handle in the session factory
        ret = sessionFactory_->monitoredItemInformation(clientHandle, monitoredItemInformation);

        // if the client handle was not found, we can check if it was assigned once
        if (ret.isNotGood())
        {
            bool isFound = std::find(database_->allClientHandles.begin(),
                                     database_->allClientHandles.end(),
                                     clientHandle) != database_->allClientHandles.end();
            if (isFound)
            {
                monitoredItemInformation.monitoredItemState = monitoreditemstates::NotCreated;
                ret = statuscodes::Good;
            }
        }

        return ret;
    }


    // Set the publishing mode.
    // =============================================================================================
    Status Client::setPublishingMode(
            ClientSubscriptionHandle  clientSubscriptionHandle,
            bool                      publishingEnabled,
            const ServiceSettings*    serviceSettings)
    {
        return sessionFactory_->setPublishingMode(clientSubscriptionHandle,
                                                  publishingEnabled,
                                                  serviceSettings);
    }


    // Set the monitoring mode
    // =============================================================================================
    Status Client::setMonitoringMode(
            vector<ClientHandle>            clientHandles,
            monitoringmodes::MonitoringMode monitoringMode,
            const ServiceSettings*          serviceSettings,
            vector<Status>&                 results)
    {
        return sessionFactory_->setMonitoringMode(clientHandles,
                                                  monitoringMode,
                                                  serviceSettings,
                                                  results);
    }


    // Run the thread
    // =============================================================================================
    void Client::run()
    {
        logger_->debug("Thread has started to periodically check the requests");

        // declare the current time and the time of the last actions
        time_t currentTime;
        time_t lastTime;

        // initialize the time values
        time(&currentTime);
        time(&lastTime);


        double updateInterval;

        while (!doFinishThread_)
        {
            updateInterval = database_->clientSettings.discoveryIntervalSec;

            msleep(100);

            time(&currentTime);

            if (difftime(currentTime, lastTime) > updateInterval)
            {
                logger_->debug("Now discovering the system and checking the persistent requests");

                // update the lastTime
                time(&lastTime);

                if (!doFinishThread_)
                    discoverer_->findServers();

                if (!doFinishThread_)
                    sessionFactory_->doHouseKeeping();

                if (!doFinishThread_)
                    processPersistedRequests(database_->createMonitoredDataRequestStore);

                if (!doFinishThread_)
                    processPersistedRequests(database_->createMonitoredEventsRequestStore);
            }
        }
    }




    // Process a ReadRequest
    // =============================================================================================
    Status Client::processRequest(const uaf::ReadRequest& request, uaf::ReadResult& result)
    {
        return processRequest<ReadService>(request, result);
    }

    // Process a ReadRequest
    // =============================================================================================
    Status Client::processRequest(const uaf::AsyncReadRequest& request, uaf::AsyncReadResult& result)
    {
        return processRequest<AsyncReadService>(request, result);
    }


    // Process a WriteRequest
    // =============================================================================================
    Status Client::processRequest(const uaf::WriteRequest& request, uaf::WriteResult& result)
    {
        return processRequest<uaf::WriteService>(request, result);
    }

    // Process a ReadRequest
    // =============================================================================================
    Status Client::processRequest(const uaf::AsyncWriteRequest& request, uaf::AsyncWriteResult& result)
    {
        return processRequest<AsyncWriteService>(request, result);
    }


    // Process a MethodCallRequest
    // =============================================================================================
    Status Client::processRequest(const uaf::MethodCallRequest& request, uaf::MethodCallResult& result)
    {
        return processRequest<uaf::MethodCallService>(request, result);
    }


    // Process an AsyncMethodCallRequest
    // =============================================================================================
    Status Client::processRequest(
            const uaf::AsyncMethodCallRequest&   request,
            uaf::AsyncMethodCallResult&          result)
    {
        return processRequest<uaf::AsyncMethodCallService>(request, result);
    }


    // Process a BrowseRequest
    // =============================================================================================
    Status Client::processRequest(const uaf::BrowseRequest& request, uaf::BrowseResult& result)
    {
        return processRequest<uaf::BrowseService>(request, result);
    }


    // Process a BrowseNextRequest
    // =============================================================================================
    Status Client::processRequest(
            const uaf::BrowseNextRequest&  request,
            uaf::BrowseNextResult&         result)
    {
        return processRequest<uaf::BrowseNextService>(request, result);
    }


    // Process a TranslateBrowsePathsToNodeIdsRequest
    // =============================================================================================
    Status Client::processRequest(
            const uaf::TranslateBrowsePathsToNodeIdsRequest&  request,
            uaf::TranslateBrowsePathsToNodeIdsResult&         result)
    {
        return processRequest<uaf::TranslateBrowsePathsToNodeIdsService>(request, result);
    }


    // Process a CreateMonitoredDataRequest
    // =============================================================================================
    Status Client::processRequest(
            const uaf::CreateMonitoredDataRequest&  request,
            uaf::CreateMonitoredDataResult&         result)
    {
        return processRequest<uaf::CreateMonitoredDataService>(request, result);
    }


    // Process a CreateMonitoredEventsRequest
    // =============================================================================================
    Status Client::processRequest(
            const uaf::CreateMonitoredEventsRequest&  request,
            uaf::CreateMonitoredEventsResult&         result)
    {
        return processRequest<uaf::CreateMonitoredEventsService>(request, result);
    }


    // Process a HistoryReadRawModifiedRequest
    // =============================================================================================
    Status Client::processRequest(
            const uaf::HistoryReadRawModifiedRequest&  request,
            uaf::HistoryReadRawModifiedResult&         result)
    {
        return processRequest<uaf::HistoryReadRawModifiedService>(request, result);
    }


    // Get a structure definition
    // =============================================================================================
    Status Client::structureDefinition(const uaf::NodeId &dataTypeId, uaf::StructureDefinition& definition)
    {
        return sessionFactory_->structureDefinition(dataTypeId, definition);
    }


    // Private template function implementation: assign a UAF handle
    // =============================================================================================
    template <typename _Service>
    Status Client::assignRequestHandle(
            typename _Service::Request& request,
            typename _Service::Result&  result)
    {
        Status ret;

        // lock the mutex until the locker is out of scope
        UaMutexLocker locker(&requestHandleMutex_);

        // check if we still can increment the handle
        if (currentRequestHandle_ < uaf::constants::REQUESTHANDLE_MAX)
        {
            // increment the handle, assign it to the request and result, and update the status
            currentRequestHandle_++;
            request.requestHandle_ = currentRequestHandle_;
            result.requestHandle   = currentRequestHandle_;
            ret = statuscodes::Good;
        }
        else
        {
            // more than 2**64 assigned is virtually impossible, we must have discovered a bug!
            ret = uaf::UnexpectedError("Maximum number of UAF handles (2**64!) reached, this must be a bug");
        }

        if (ret.isGood())
            logger_->debug("Assigning handle %d to the request", currentRequestHandle_);
        else
            logger_->error(ret.toString());

        return ret;
    }


    // Private template function implementation: process persistent requests
    // =============================================================================================
    template<typename _Store>
    void Client::processPersistedRequests(_Store& store)
    {
        // create a typedef for the vector holding the correct type of items
        typedef std::vector<typename _Store::Item> Items;

        // get the items that need to be reconstructed
        Items items = store.getBadItems();

        if (items.size() > 0)
            logger_->debug("A total of %d persistent requests need to be re-processed",
                           items.size());

        // loop through the items and process them
        // (We don't have to care about the Status result of the processRequest function,
        // since processPersistentRequests() is called automatically by a thread, and this
        // thread doesn't perform any actions in case of failures.)
        for (typename Items::iterator it = items.begin(); it != items.end(); ++it)
            processRequest<typename _Store::ServiceType>(
                    it->request,
                    it->badTargetsMask,
                    it->result);
    }


    // Private template function implementation: process a request
    // =============================================================================================
    template<typename _Service>
    uaf::Status Client::processRequest(
            const typename _Service::Request&   request,
            typename _Service::Result&          result)
    {
        return processRequest<_Service>(request, uaf::Mask(request.targets.size(), true), result);
    }


    // Private template function implementation: process a masked request
    // =============================================================================================
    template<typename _Service>
    uaf::Status Client::processRequest(
            const typename _Service::Request&   request,
            const uaf::Mask&                    mask,
            typename _Service::Result&          result)
    {
        // declare the return Status
        uaf::Status ret;

        logger_->debug("Processing the following %sRequest:", _Service::name().c_str());
        logger_->debug(request.toString());

        // resize the result
        result.targets.resize(request.targets.size());

        // create a copy of the request (so we can assign a handle, resolve the targets, ...)
        typename _Service::Request copiedRequest(request);

        // get a new unique request handle if necessary,
        // and update the copied request and result with it
        if (request.requestHandle() == uaf::constants::REQUESTHANDLE_NOT_ASSIGNED)
            ret = assignRequestHandle<_Service>(copiedRequest, result);
        else
            ret = uaf::statuscodes::Good;

        // assign client handles if necessary
        // (this is only needed for CreateMonitoredDataRequests and CreateMonitoredEventsRequests)
        std::vector<uaf::ClientHandle> clientHandles;
        bool assigned;
        if (ret.isGood())
            ret = uaf::assignClientHandlesIfNeeded<_Service>(result, mask, database_, assigned, clientHandles);

        // if no error occurred, store the copied request if needed
        // (this is only needed for 'persistent' requests such as CreateMonitoredDataRequests)
        if (ret.isGood())
            ret = uaf::storeIfNeeded<_Service>(
                    copiedRequest,
                    result,
                    result.getBadTargetsMask(),
                    database_);

        // if no error occurred, resolve the unmasked targets of the copied request
        if (ret.isGood())
            ret = resolver_->resolve<_Service>(copiedRequest, mask, result);

        // if no error occurred, mask out the unresolved addresses and invoke the service request
        if (ret.isGood())
        {
            uaf::Mask resolvedMask = mask && result.getGoodTargetsMask();
            ret = sessionFactory_->invokeRequest<_Service>(copiedRequest, resolvedMask, result);
        }

        // finally, update the overall status
        result.updateOverallStatus();

        // if no error occurred, update the result if needed
        // (this is only needed for 'persistent' results such as CreateMonitoredDataResults)
        if (ret.isGood())
            ret = updateResultIfNeeded<_Service>(result, mask, database_);

        // log the result, if good
        if (ret.isGood())
        {
            logger_->debug("%sResult %d:", _Service::name().c_str(), result.requestHandle);
            logger_->debug(result.toString());
        }

        // if client handles were assigned, copy them to the diagnostics of the Status object
        if (assigned && ret.isBad())
        {
            Status newRet = uaf::CreateMonitoredItemsError(clientHandles);
            newRet.setRaisedBy(ret);
            ret = newRet;
        }

        return ret;
    }


}

