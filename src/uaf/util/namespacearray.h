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

#ifndef UAF_NAMESPACEARRAY_H_
#define UAF_NAMESPACEARRAY_H_


// STD
#include <string>
#include <sstream>
#include <map>
#include <algorithm>
#include <stdint.h>
// SDK
#include "uabase/uaarraytemplates.h"
#include "uabase/uastring.h"
#include "uabase/uavariant.h"
// UAF
#include "uaf/util/util.h"
#include "uaf/util/status.h"
#include "uaf/util/variant.h"
#include "uaf/util/address.h"


namespace uaf
{
    // Define a map with key=namespaceIndex value=namespaceURI
    typedef std::map<NameSpaceIndex, std::string> NameSpaceMap;


    /*******************************************************************************************//**
     * A NamespaceArray maps namespace indexes to namespace URIs.
     *
     * A NamespaceArray is a map of a namespaceIndex (uint16_t) to a namespaceURI (string).
     * NamespaceArrays are exposed by servers, so that clients can look up the URI of a namespace
     * for a given namespace index.
     *
     * @ingroup Util
     **********************************************************************************************/
    class UAF_EXPORT NamespaceArray
    {
    public:


        /**
         * Construct an empty NamespaceArray.
         */
        NamespaceArray();


        /**
         * Destruct the NamespaceArray.
         */
        ~NamespaceArray();


        /**
         * Fill the NamespaceArray with the information contained by the given OpcUa_DataValue
         * from the stack.
         *
         * @param value     A data value defined by the OPC UA stack.
         * @return          The Status of this operation.
         */
        uaf::Status fromSdk(const OpcUa_DataValue& value);


        /**
         * Get a string representation of the NamespaceArray.
         *
         * @return  The string representation.
         */
        std::string toString() const;


        /**
         * Find the NamespaceURI for a given NamespaceIndex.
         *
         * @param namespaceIndex    The NamespaceIndex that needs to be looked up.
         * @param namespaceUri      The NamespaceURI that will be overwritten.
         * @return                  True if the URI could be found.
         */
        bool findNamespaceUri(
                NameSpaceIndex  namespaceIndex,
                std::string&    namespaceUri) const;


        /**
         * Find the NamespaceIndex for a given NamespaceURI.
         *
         * @param namespaceUri      The NamespaceURI that needs to be looked up.
         * @param namespaceIndex    The NamespaceIndex that will be overwritten.
         * @return                  True if the URI could be found.
         */
        bool findNamespaceIndex(
                const std::string&  namespaceUri,
                NameSpaceIndex&     namespaceIndex) const;



        /**
         * Fill an OpcUa_NodeId (which is fully resolved!) from a uaf::NodeId (which may not
         * be resolved!).
         *
         * @param address       The uaf::Address for which we must create a UaNodeId.
         * @param opcUaNodeId   The OpcUa_NodeId to be updated.
         * @return              Good if the OpcUa_NodeId could be updated, false if not.
         */
        uaf::Status fillOpcUaNodeId(
                const uaf::Address& address,
                OpcUa_NodeId&       opcUaNodeId) const;


        /**
         * Fill an OpcUa_NodeId (which is fully resolved!) from a uaf::NodeId (which may not
         * be resolved!).
         *
         * @param nodeId        The uaf::NodeId for which we must create a UaNodeId.
         * @param opcUaNodeId   The OpcUa_NodeId to be updated.
         * @return              Good if the OpcUa_NodeId could be updated, false if not.
         */
        uaf::Status fillOpcUaNodeId(
                const uaf::NodeId&  nodeId,
                OpcUa_NodeId&       opcUaNodeId) const;


        /**
         * Fill an OpcUa_ExpandedNodeId (which is fully resolved!) from a uaf::ExpandedNodeId
         * (which may not be resolved!).
         *
         * @param nodeId        The uaf::NodeId for which we must create a UaNodeId.
         * @param opcUaNodeId   The OpcUa_NodeId to be updated.
         * @return              Good if the OpcUa_NodeId could be updated, false if not.
         */
        uaf::Status fillOpcUaExpandedNodeId(
                const uaf::ExpandedNodeId&  nodeId,
                OpcUa_ExpandedNodeId&       opcUaNodeId) const;


        /**
         * Fill an OpcUa_QualifiedName (which is fully resolved!) from a uaf::QualifiedName
         * (which may not be resolved!).
         *
         * @param qualifiedName         The uaf::QualifiedName for which we must create a UaNodeId.
         * @param opcUaQualifiedName    The OpcUa_QualifiedName to be updated.
         * @return                      Good if the OpcUa_QualifiedName could be update, false if
         *                              not.
         */
        uaf::Status fillOpcUaQualifiedName(
                const uaf::QualifiedName& qualifiedName,
                OpcUa_QualifiedName&      opcUaQualifiedName) const;


        /**
         * Fill an OpcUa_RelativePathElement (which is fully resolved!) from a
         * uaf::RelativePathElement (which may not be resolved!).
         *
         * @param element         The uaf::RelativePathElement to extract the contents from.
         * @param opcUaElement    The OpcUa_RelativePathElement to be updated.
         * @return                Good if the OpcUa_RelativePathElement could be updated.
         */
        uaf::Status fillOpcUaRelativePathElement(
                const uaf::RelativePathElement& element,
                OpcUa_RelativePathElement&      opcUaElement) const;


        /**
         * Fill a NodeId from an OpcUa_NodeId (which includes filling out the namespace URI).
         *
         * @param opcUaNodeId   The OpcUa_NodeId of which we want to extract the information.
         * @param nodeId        The uaf::NodeId which we want to update.
         * @return              Good if the uaf::NodeId could be updated without problems
         *                      (i.e. if the namespace URI could be set).
         */
        uaf::Status fillNodeId(
                const OpcUa_NodeId& opcUaNodeId,
                uaf::NodeId&        nodeId) const;


        /**
         * Fill an ExpandedNodeId from an OpcUa_ExpandedNodeId (which includes filling out the
         * namespace URI).
         *
         * @param opcUaExpandedNodeId   The OpcUa_ExpandedNodeId of which we want to extract
         *                              the information.
         * @param expandedNodeId        The uaf::ExpandedNodeId which we want to update.
         * @return                      Good if the uaf::ExpandedNodeId could be updated without
         *                              problems (i.e. if the namespace URI could be set).
         */
        uaf::Status fillExpandedNodeId(
                const OpcUa_ExpandedNodeId& opcUaExpandedNodeId,
                uaf::ExpandedNodeId&        expandedNodeId) const;


        /**
         * Fill a QualifiedName from an OpcUa_QualifiedName (which includes filling out the
         * namespace URI).
         *
         * @param opcUaQualifiedName    The OpcUa_QualifiedName of which we want to extract
         *                              the information.
         * @param qualifiedName         The uaf::QualifiedName which we want to update.
         * @return                      Good if the uaf::QualifiedName could be updated without
         *                              problems (i.e. if the namespace URI could be set).
         */
        uaf::Status fillQualifiedName(
                const OpcUa_QualifiedName& opcUaQualifiedName,
                uaf::QualifiedName&        qualifiedName) const;


        /**
         * Fill an ExtensionObject from an OpcUa_ExtensionObject (which includes filling out the
         * namespace URI).
         *
         * @param opcUaExtensionObject  The OpcUa_ExtensionObject of which we want to extract
         *                              the information.
         * @param extensionObject       The uaf::ExtensionObject which we want to update.
         * @return                      Good if the uaf::ExtensionObject could be updated without
         *                              problems (i.e. if the namespace URI could be set).
         */
        uaf::Status fillExtensionObject(
                const OpcUa_ExtensionObject& opcUaExtensionObject,
                uaf::ExtensionObject&        extensionObject) const;


        /**
         * Fill a Variant, which *may* include filling out the namespace URI, if the variant
         * represents a NodeId or ExpandedNodeId or QualifiedName.
         *
         * @param variant         The uaf::Variant which we want to update.
         * @return                Good if the uaf::Variant could be updated without
         *                        problems (i.e. if the namespace URI could be set).
         */
        uaf::Status fillVariant(uaf::Variant& variant) const;


        /**
         * A static function to create a string from the given namespace map.
         *
         * @param uri   URI that may contain capitals, a trailing forward slash, ...
         * @return      A simplified "plain" string.
         */
        static std::string toString(const NameSpaceMap& map);



    private:

        // the internal NameSpaceIndex:NameSpaceURI map
        NameSpaceMap nameSpaceMap_;

        // the original array
        UaStringArray namespaceArray_;

        /**
         * A static function to convert the URI to a "plain" one (no trailing forward slash,
         * no capitals) in order to simplify comparison.
         *
         * @param uri   URI that may contain capitals, a trailing forward slash, ...
         * @return      A simplified "plain" string.
         */
        static std::string getSimplifiedUri(const std::string& uri);
    };
}


#endif /* UAF_NAMESPACEARRAY_H_ */
