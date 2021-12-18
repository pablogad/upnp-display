// -*- mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; -*-
//  This file is part of UPnP LCD Display
//
//  Copyright (C) 2013 Henner Zeller <h.zeller@acm.org>
//
//  This program is free software; you can redistribute it and/or modify
//  it under the terms of the GNU General Public License as published by
//  the Free Software Foundation; either version 3 of the License, or
//  (at your option) any later version.
//
//  This program is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details.
//
//  You should have received a copy of the GNU General Public License
//  along with this program.  If not, see <http://www.gnu.org/licenses/>.

#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <iostream>

#include <upnp.h>
#include <upnptools.h>
#include <ithread.h>

#include "renderer-state.h"

// Prefix, as these can be followed by changing version number.
static const char kTransportServicePrefix[] =
	"urn:schemas-upnp-org:service:AVTransport:";
static const char kRenderControlPrefix[] =
	"urn:schemas-upnp-org:service:RenderingControl:";

static const char *get_node_content(IXML_Node *node) {
  IXML_Node *text_content = ixmlNode_getFirstChild(node);
  if (!text_content) return NULL;
  return ixmlNode_getNodeValue(text_content);
}

// Get the first node with the given name and return its content.
static const char *find_first_content(IXML_Node *node, const char *name) {
  IXML_NodeList *nlist = NULL;
  nlist = ixmlElement_getElementsByTagName((IXML_Element*)node, name);
  if (nlist == NULL) return NULL;
  const char *result = NULL;
  if (nlist->nodeItem) {
    result = get_node_content(nlist->nodeItem);
  }
  ixmlNodeList_free(nlist);
  return result;
}

static const char *find_first_content(IXML_Document *doc, const char *name) {
  IXML_NodeList *nlist = NULL;
  nlist = ixmlDocument_getElementsByTagName(doc, name);
  if (nlist == NULL) return NULL;
  const char *result = NULL;
  if (nlist->nodeItem) {
    result = get_node_content(nlist->nodeItem);
  }
  ixmlNodeList_free(nlist);
  return result;
}

RendererState::RendererState(UpnpClient_Handle device_, const char *uuid)
  : upnp_controller(device_),
    uuid_(uuid), descriptor_(NULL), subscriptions_(NULL),
    last_event_update_(time(NULL)) {
  ithread_mutex_init(&variable_mutex_, NULL);
}

RendererState::~RendererState() {
  if (descriptor_) ixmlDocument_free(descriptor_);
  for (size_t i = 0; i < subscription_ids_.size(); ++i) {
    subscriptions_->erase(subscription_ids_[i]);
  }
}

bool RendererState::InitDescription(const char *description_url) {
  assert(descriptor_ == NULL);  // call this only once.
  if (UpnpDownloadXmlDoc(description_url, &descriptor_) != UPNP_E_SUCCESS) {
    fprintf(stderr, "Can't read service description: %s\n", description_url);
    return false;
  }

  const char *base_url = find_first_content(descriptor_, "URLBase");
  if (base_url != NULL) {
    base_url_ = base_url;
  } else {
    base_url_ = description_url;
    std::string::size_type slash_pos = base_url_.find_first_of("/", 7);
    if (slash_pos != std::string::npos) {
      base_url_.resize(slash_pos + 1);
    }
  }

  const char* control_url = find_first_content(descriptor_, "controlURL");
  if (control_url)
    control_url_ = control_url;

  const char *friendly_name = find_first_content(descriptor_, "friendlyName");

  if (friendly_name)
    friendly_name_ = friendly_name;

  return true;
}

static bool prefixMatch(const char *str, const char *prefix) {
  return strncmp(str, prefix, strlen(prefix)) == 0;
}
bool RendererState::SubscribeTo(SubscriptionMap *submap) {
  assert(descriptor_ != NULL);    // Needs to be initialized

  assert(subscriptions_ == NULL); // .. but not yet subscribed
  subscriptions_ = submap;

  IXML_NodeList *service_list = NULL;
  service_list = ixmlDocument_getElementsByTagName(descriptor_, "serviceList");

  if (service_list == NULL) {
    fprintf(stderr, "No services found for %s (%s)\n",
            friendly_name_.c_str(), uuid_.c_str());
    return false;
  }
  IXML_NodeList *service_it = NULL;
  service_it = ixmlElement_getElementsByTagName(
                        (IXML_Element*) service_list->nodeItem,
                        "service");
  ixmlNodeList_free(service_list);
  if (service_it == NULL) {
    return false;
  }

  bool success = true;
  for (const IXML_NodeList *it = service_it; it; it = it->next) {
    const char *service_type = find_first_content(it->nodeItem, "serviceType");
    if (service_type == NULL) continue;
    const char *event_url = find_first_content(it->nodeItem, "eventSubURL");

    if (prefixMatch(service_type, kTransportServicePrefix) ||
        prefixMatch(service_type, kRenderControlPrefix)) {
      success &= Subscribe(upnp_controller, service_type, event_url);
    }
  }
  ixmlNodeList_free(service_it);

  return success;
}

bool RendererState::Subscribe(UpnpClient_Handle upnp_controller,
                              const char *service_type,
                              const char *event_url) {
  std::string url = base_url_ + (event_url + 1);
  int timeout;
  Upnp_SID sid;
  int rc = UpnpSubscribe(upnp_controller, url.c_str(), &timeout, sid);
  if (rc == UPNP_E_SUCCESS) {
    subscriptions_->insert(std::make_pair(sid, this));
    subscription_ids_.push_back(sid);
  } else {
    fprintf(stderr, "Subscribe: %s %s %s rc=%d\n",
            friendly_name_.c_str(),
            service_type, UpnpGetErrorMessage(rc), rc);
    return false;
  }
  return true;
}

std::string RendererState::GetVar(const std::string &name) const {
  std::string result;
  ithread_mutex_lock(&variable_mutex_);
  VariableMap::const_iterator found = variables_.find(name);
  if (found != variables_.end()) {
    result = found->second;
  }
  ithread_mutex_unlock(&variable_mutex_);
  return result;
}

time_t RendererState::last_event_update() const {
  ithread_mutex_lock(&variable_mutex_);
  time_t result = last_event_update_;
  ithread_mutex_unlock(&variable_mutex_);
  return result;
}

void RendererState::DecodeMetaAndInsertData_Locked(const char *didl_xml) {
  variables_["Meta_Title"] = "";
  variables_["Meta_Artist"] = "";
  variables_["Meta_Composer"] = "";
  variables_["Meta_Creator"] = "";
  variables_["Meta_Album"] = "";
  variables_["Meta_Genre"] = "";
  variables_["Meta_Year"] = "";

  IXML_Document *doc = ixmlParseBuffer(didl_xml);
  if (doc == NULL)
    return;

  IXML_NodeList *list = NULL;
  list = ixmlDocument_getElementsByTagName(doc, "DIDL-Lite");
  if (list == NULL) return;
  IXML_Node *toplevel = list->nodeItem;
  ixmlNodeList_free(list);

  list = ixmlElement_getElementsByTagName((IXML_Element*)toplevel, "item");
  if (list == NULL) return;
  IXML_Node *item_element = list->nodeItem;
  ixmlNodeList_free(list);

  IXML_NodeList *variable_list = ixmlNode_getChildNodes(item_element);

  std::string album_artist;
  for (const IXML_NodeList *it = variable_list; it; it = it->next) {
    const char *name = ixmlNode_getNodeName(it->nodeItem);
    const char *value = get_node_content(it->nodeItem);
    if (!value) continue;
    if (strcmp("dc:title", name) == 0) {
      variables_["Meta_Title"] = value;
    } else if (strcmp("upnp:artist", name) == 0) {
      const char *qualifier
        = ixmlElement_getAttribute((IXML_Element*) it->nodeItem, "role");
      if (qualifier != NULL && strcmp(qualifier, "Composer") == 0) {
        variables_["Meta_Composer"] = value;
      } else if (qualifier != NULL && strcmp(qualifier, "AlbumArtist") == 0) {
        album_artist = value;
      } else {
        variables_["Meta_Artist"] = value;
      }
    } else if (strcmp("upnp:album", name) == 0) {
      variables_["Meta_Album"] = value;
    } else if (strcmp("upnp:genre", name) == 0) {
      variables_["Meta_Genre"] = value;
    } else if (strcmp("upnp:composer", name) == 0) {
      variables_["Meta_Composer"] = value;
    } else if (strcmp("dc:creator", name) == 0) {
      variables_["Meta_Creator"] = value;
    } else if (strcmp("dc:date", name) == 0) {
      variables_["Meta_Year"] = value;
      if (variables_["Meta_Year"].size() == 10) {  // proper ISO8601
        variables_["Meta_Year"].resize(4);
      }
    }
  }

  // If we don't have a specific artist, take the generic artist of the album.
  if (variables_["Meta_Artist"].empty() && !album_artist.empty()) {
    variables_["Meta_Artist"] = album_artist;
  }

  ixmlNodeList_free(variable_list);
  ixmlDocument_free(doc);
}

void RendererState::ReceiveEvent(const UpnpEvent *data) {
    const char *as_string = find_first_content(
        UpnpEvent_get_ChangedVariables(data), "LastChange");
  //fprintf(stderr, "Got variable changes: %s\n", as_string);
  IXML_Document *doc = ixmlParseBuffer(as_string);
  if (doc == NULL) {
    fprintf(stderr, "Invalid XML\n");
    return;
  }
  IXML_NodeList *instance_list = NULL;
  instance_list = ixmlDocument_getElementsByTagName(doc, "InstanceID");
  if (instance_list == NULL) return;
  IXML_Node *instance_element = instance_list->nodeItem;  // interested in 1st
  ixmlNodeList_free(instance_list);
  IXML_NodeList *variable_list = ixmlNode_getChildNodes(instance_element);
  ithread_mutex_lock(&variable_mutex_);
  for (const IXML_NodeList *it = variable_list; it; it = it->next) {
    const char *name = ixmlNode_getNodeName(it->nodeItem);
    const char *value
      = ixmlElement_getAttribute((IXML_Element*) it->nodeItem, "val");
    variables_[name] = value;
    if (strcmp(name, "CurrentTrackMetaData") == 0) {
      DecodeMetaAndInsertData_Locked(value);
    }
  }
  last_event_update_ = time(NULL);
  ithread_mutex_unlock(&variable_mutex_);
  ixmlNodeList_free(variable_list);
  ixmlDocument_free(doc);
}

// Parameters
static std::vector<std::pair<std::string, std::string>> paramInstance = {
        { "InstanceID", "0" } };
static std::vector<std::pair<std::string, std::string>> paramsPlay = {
        { "InstanceID", "0" },
        { "Speed", "1" } };

static IXML_Document* response;

void RendererState::GetPositionInfo() {

   auto play_state = GetVar("TransportState");
   if (play_state != "PLAYING") return;

   int rc = SendActionTest( 0, 0, paramInstance );

   if( rc == UPNP_E_SUCCESS ) {

      const char* relTime = find_first_content( response, "RelTime" );
      variables_["RelTime"] = relTime;
   }

   ixmlDocument_free(response);
}

void RendererState::SendActionParamInstance( int actionIndex ) {

   SendActionTest( 0, actionIndex, paramInstance );

   ixmlDocument_free(response);
}

void RendererState::Play() {

   auto play_state = GetVar("TransportState");
   if (play_state == "PLAYING") return;

   SendActionParamInstance( 1 );
}

void RendererState::Pause() {

   auto play_state = GetVar("TransportState");
   if (play_state != "PLAYING") return;

   SendActionParamInstance( 2 );
}

void RendererState::Stop() {

   auto play_state = GetVar("TransportState");
   if (play_state != "PLAYING") return;

   SendActionParamInstance( 3 );
}

static std::array<std::string, 1> service_types = { "urn:schemas-upnp-org:service:AVTransport:1" };
static std::array<std::string, 4> actions = { "GetPositionInfo", "Play", "Pause", "Stop" };

int RendererState::SendActionTest( int service_type, int action,
		std::vector<std::pair<std::string, std::string>>& params ) {

   IXML_Document* doc = nullptr;
   response = nullptr;

   const char* service_str = service_types[ service_type ].c_str();
   const char* action_str = actions[ action ].c_str();

   if (params.empty())
      doc = UpnpMakeAction(action_str, service_str, 0, nullptr);
   else
   for (std::pair<std::string,std::string>& param : params ) {

      const char* paramName = param.first.c_str();
      const char* paramValue = param.second.c_str();

      if (UpnpAddToAction(&doc,
               action_str,
               service_str,
               paramName,
               paramValue) != UPNP_E_SUCCESS) {

          printf("ERROR param %s,%s!\n", paramName, paramValue);
      }
   }

   int rc = UPNP_E_INTERNAL_ERROR;

   if( doc ) {

      std::string action_url (base_url_);

      // Remove last '/' if present to avoid duplication
      if (action_url[ action_url.length() - 1] == '/')
         action_url.pop_back();

      action_url.append(control_url_);

      rc = UpnpSendAction(upnp_controller,
                    action_url.c_str(), service_str,
                    nullptr, doc, &response);

      ixmlDocument_free(doc);
   }

   //std::cout << "SendAction[" << action_str << "] : " << rc  << std::endl;

   return rc;
}

