/*
 Author: Alexander Knauer <a-x-e@gmx.net>
 License: Apache 2.0
 */
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <pthread.h>

#include <libconfig.h>
#include <curl/curl.h>
#include <json.h>
#include <utlist.h>

#include <digitalSTROM/dsuid.h>
#include <dsvdc/dsvdc.h>

#include "airq.h"

#include "incbin.h"
INCBIN_EXTERN(IconStation16);
INCBIN_EXTERN(IconStation48);

void vdc_ping_cb(dsvdc_t *handle __attribute__((unused)), const char *dsuid, void *userdata __attribute__((unused))) {
  int ret;
  vdc_report(LOG_NOTICE, "received ping for dsuid %s\n", dsuid);
  if (strcasecmp(dsuid, g_vdc_dsuid) == 0) {
    ret = dsvdc_send_pong(handle, dsuid);
    vdc_report(LOG_NOTICE, "sent pong for vdc %s / return code %d\n", dsuid, ret);
    return;
  }
  if (strcasecmp(dsuid, g_lib_dsuid) == 0) {
    ret = dsvdc_send_pong(handle, dsuid);
    vdc_report(LOG_NOTICE, "sent pong for lib-dsuid %s / return code %d\n", dsuid, ret);
    return;
  }
  
  if (strcasecmp(dsuid, airq_device->dsuidstring) == 0) {
      ret = dsvdc_send_pong(handle, airq_device->dsuidstring);
      vdc_report(LOG_NOTICE, "sent pong for device %s / return code %d\n", dsuid, ret);
    return;
  }
  vdc_report(LOG_WARNING, "ping: no matching dsuid %s registered\n", dsuid);
}

void vdc_announce_device_cb(dsvdc_t *handle __attribute__((unused)), int code, void *arg, void *userdata __attribute__((unused))) {
  vdc_report(LOG_INFO, "announcement of device %s returned code: %d\n", (char *) arg, code);
}

void vdc_announce_container_cb(dsvdc_t *handle __attribute__((unused)), int code, void *arg, void *userdata __attribute__((unused))) {
  vdc_report(LOG_INFO, "announcement of container %s returned code: %d\n", (char *) arg, code);
}

void vdc_new_session_cb(dsvdc_t *handle, void *userdata) {
  (void)userdata;
  int ret;
  ret = dsvdc_announce_container(handle,
                                 g_vdc_dsuid,
                                 (void *) g_vdc_dsuid,
                                 vdc_announce_container_cb);
  if (ret != DSVDC_OK) {
    vdc_report(LOG_WARNING, "dsvdc_announce_container returned error %d\n", ret);
    return;
  }

  vdc_report(LOG_INFO, "new session, container announced successfully\n");
}

void vdc_end_session_cb(dsvdc_t *handle, void *userdata) {
  (void)userdata;
  int ret;

  vdc_report(LOG_WARNING, "end of session\n");
}

bool vdc_remove_cb(dsvdc_t *handle __attribute__((unused)), const char *dsuid, void *userdata) {
  (void)userdata;
  vdc_report(LOG_INFO, "received remove for dsuid %s\n", dsuid);

  // TODO: what to do on remove?

  return true;
}

void vdc_request_generic_cb(dsvdc_t *handle __attribute__((unused)), char *dsuid, char *method_name, dsvdc_property_t *property, const dsvdc_property_t *properties,  void *userdata) {
  int ret;
  uint8_t code = DSVDC_ERR_NOT_IMPLEMENTED;
  size_t i;
  
  vdc_report(LOG_INFO, "received request generic for dsuid %s, method name %s\n", dsuid, method_name);
  
  if (strcasecmp(airq_device->dsuidstring, dsuid) == 0) {
  }
}

void vdc_savescene_cb(dsvdc_t *handle __attribute__((unused)), char **dsuid, size_t n_dsuid, int32_t scene, int32_t *group, int32_t *zone_id, void *userdata) {
  vdc_report(LOG_NOTICE, "save scene %d\n", scene);
  if (strcasecmp(airq_device->dsuidstring, *dsuid) == 0) {
  }
}
  
void vdc_callscene_cb(dsvdc_t *handle __attribute__((unused)), char **dsuid, size_t n_dsuid, int32_t scene, bool force, int32_t *group, int32_t *zone_id, void *userdata) {
/**  for(int n = 0; n < n_dsuid; n++)
    {
         vdc_report(LOG_NOTICE,"received %scall scene for device %s\n", force?"forced ":"", *dsuid);
    } **/

  if (strcasecmp(airq_device->dsuidstring, *dsuid) == 0) {
    vdc_report(LOG_NOTICE, "called scene: %d\n", scene);
  }
}

void vdc_setprop_cb(dsvdc_t *handle, const char *dsuid, dsvdc_property_t *property, const dsvdc_property_t *properties, void *userdata) {
  (void) userdata;
  int ret;
  uint8_t code = DSVDC_ERR_NOT_IMPLEMENTED;
  size_t i;
  vdc_report(LOG_INFO, "set property request for dsuid \"%s\"\n", dsuid);

  /*
   * Properties for the VDC
   */
  if (strcasecmp(g_vdc_dsuid, dsuid) == 0) {
    for (i = 0; i < dsvdc_property_get_num_properties(properties); i++) {
      char *name;
      ret = dsvdc_property_get_name(properties, i, &name);
      if (ret != DSVDC_OK) {
        vdc_report(LOG_ERR, "setprop_cb: error getting property name\n");
        code = DSVDC_ERR_MISSING_DATA;
        break;
      }
      if (!name) {
        vdc_report(LOG_ERR, "setprop_cb: not handling wildcard properties\n");
        code = DSVDC_ERR_NOT_IMPLEMENTED;
        break;
      }

      vdc_report(LOG_INFO, "set request for name=\"%s\"\n", name);

      if (strcmp(name, "zoneID") == 0) {
        uint64_t zoneID;
        ret = dsvdc_property_get_uint(properties, i, &zoneID);
        if (ret != DSVDC_OK) {
          vdc_report(LOG_ERR, "setprop_cb: error getting property value from property %s\n", name);
          code = DSVDC_ERR_INVALID_VALUE_TYPE;
          break;
        }
        vdc_report(LOG_NOTICE, "setprop_cb: \"%s\" = %d\n", name, zoneID);
        g_default_zoneID = zoneID;
        code = DSVDC_OK;
      } else {
        code = DSVDC_ERR_NOT_FOUND;
        break;
      }

      free(name);
    }

    if (code == DSVDC_OK) {
      write_config();
    }

    dsvdc_send_set_property_response(handle, property, code);
    return;
  } 
  
  if (strcasecmp(airq_device->dsuidstring, dsuid) != 0) {	  
    vdc_report(LOG_WARNING, "set property: unhandled dsuid %s\n", dsuid);
    dsvdc_property_free(property);
    return;
  }

  /*
   * Properties for the VDSD's
   */
  pthread_mutex_lock(&g_network_mutex);
  for (i = 0; i < dsvdc_property_get_num_properties(properties); i++) {
    char *name;

    int ret = dsvdc_property_get_name(properties, i, &name);
    if (ret != DSVDC_OK) {
      vdc_report(LOG_ERR, "getprop_cb: error getting property name, abort\n");
      dsvdc_send_get_property_response(handle, property);
      pthread_mutex_unlock(&g_network_mutex);
      return;
    }
    if (!name) {
      vdc_report(LOG_ERR, "getprop_cb: not yet handling wildcard properties\n");
      //dsvdc_send_property_response(handle, property);
      continue;
    }
    vdc_report(LOG_NOTICE, "get request name!!=\"%s\"\n", name);

    if (strcmp(name, "zoneID") == 0) {
      uint64_t zoneID;
      ret = dsvdc_property_get_uint(properties, i, &zoneID);
      if (ret != DSVDC_OK) {
        vdc_report(LOG_ERR, "setprop_cb: error getting property value from property %s\n", name);
        code = DSVDC_ERR_INVALID_VALUE_TYPE;
        break;
      }
      vdc_report(LOG_NOTICE, "setprop_cb: \"%s\" = %d\n", name, zoneID);
      airq_device->device->zoneID = zoneID;
      code = DSVDC_OK;
    } else {
      code = DSVDC_OK;
    }
      

    free(name);
  }
  pthread_mutex_unlock(&g_network_mutex);

  dsvdc_send_set_property_response(handle, property, code);
}

void vdc_getprop_cb(dsvdc_t *handle, const char *dsuid, dsvdc_property_t *property, const dsvdc_property_t *query, void *userdata) {
  (void) userdata;
  int ret;
  size_t i;
  char *name;
  
  vdc_report(LOG_INFO, "get property for dsuid: %s\n", dsuid);

  /*
   * Properties for the VDC
   */
  if (strcasecmp(g_vdc_dsuid, dsuid) == 0) {
    for (i = 0; i < dsvdc_property_get_num_properties(query); i++) {

      int ret = dsvdc_property_get_name(query, i, &name);
      if (ret != DSVDC_OK) {
        vdc_report(LOG_ERR, "getprop_cb: error getting property name, abort\n");
        dsvdc_send_get_property_response(handle, property);
        return;
      }
      if (!name) {
        vdc_report(LOG_ERR, "getprop_cb: not yet handling wildcard properties\n");
        dsvdc_send_get_property_response(handle, property);
        return;
      }
      vdc_report(LOG_NOTICE, "get request name=\"%s\"\n", name);

      if ((strcmp(name, "hardwareGuid") == 0) || (strcmp(name, "displayId") == 0)) {
        char info[256];
        char buffer[32];
        size_t n;

        memset(info, 0, sizeof(info));
        if (strcmp(name, "hardwareGuid") == 0) {
          strcpy(info, "airq-id:");
        }
        sprintf(buffer, "%d", airq.device.id);
        strcat(info, buffer);
        dsvdc_property_add_string(property, name, info);

      } else if (strcmp(name, "vendorId") == 0) {
      } else if (strcmp(name, "oemGuid") == 0) {

      } else if (strcmp(name, "implementationId") == 0) {
        dsvdc_property_add_string(property, name, "AirQ");

      } else if (strcmp(name, "modelUID") == 0) {
        dsvdc_property_add_string(property, name, "AirQ");

      } else if (strcmp(name, "modelGuid") == 0) {
        dsvdc_property_add_string(property, name, "AirQ");

      } else if (strcmp(name, "name") == 0) {
        char info[256];
        strcpy(info, "AirQ ");
        strcat(info, airq.device.name);
        dsvdc_property_add_string(property, name, info);

      } else if (strcmp(name, "model") == 0) {
        char hostname[HOST_NAME_MAX];
        gethostname(hostname, HOST_NAME_MAX);
        char servicename[HOST_NAME_MAX + 32];
        strcpy(servicename, "AirQ Controller @");
        strcat(servicename, hostname);
        dsvdc_property_add_string(property, name, servicename);

      } else if (strcmp(name, "capabilities") == 0) {
        dsvdc_property_t *reply;
        ret = dsvdc_property_new(&reply);
        if (ret != DSVDC_OK) {
          vdc_report(LOG_ERR, "failed to allocate reply property for %s\n", name);
          free(name);
          continue;
        }
        dsvdc_property_add_bool(reply, "metering", false);
        dsvdc_property_add_bool(reply, "dynamicDefinitions", true);
        dsvdc_property_add_property(property, name, &reply);

      } else if (strcmp(name, "configURL") == 0) {


      } else if (strcmp(name, "zoneID") == 0) {
        dsvdc_property_add_uint(property, "zoneID", g_default_zoneID);

      /* user properties: user name, client_id, status */

      }
      free(name);
    }

    dsvdc_send_get_property_response(handle, property);
    return;
  } 

  if (strcasecmp(airq_device->dsuidstring, dsuid) != 0) {	  
    vdc_report(LOG_WARNING, "get property: unhandled dsuid %s\n", dsuid);
    dsvdc_property_free(property);
    return;
  }

  /*
   * Properties for the VDSD's
   */
  pthread_mutex_lock(&g_network_mutex);
  for (i = 0; i < dsvdc_property_get_num_properties(query); i++) {

    int ret = dsvdc_property_get_name(query, i, &name);
    if (ret != DSVDC_OK) {
      vdc_report(LOG_ERR, "getprop_cb: error getting property name, abort\n");
      dsvdc_send_get_property_response(handle, property);
      pthread_mutex_unlock(&g_network_mutex);
      return;
    }
    if (!name) {
      vdc_report(LOG_ERR, "getprop_cb: not yet handling wildcard properties\n");
      //dsvdc_send_property_response(handle, property);
      continue;
    }
    vdc_report(LOG_NOTICE, "get request name=\"%s\"\n", name);

    if (strcmp(name, "primaryGroup") == 0) {
      dsvdc_property_add_uint(property, "primaryGroup", 9);
    } else if (strcmp(name, "zoneID") == 0) {
      dsvdc_property_add_uint(property, "zoneID", airq_device->device->zoneID);
    } else if (strcmp(name, "buttonInputDescriptions") == 0) {
     

    } else if (strcmp(name, "buttonInputSettings") == 0) {
      
    } else if (strcmp(name, "dynamicActionDescriptions") == 0) { 


    } else if (strcmp(name, "outputDescription") == 0) {

    } else if (strcmp(name, "outputSettings") == 0) {

    } else if (strcmp(name, "channelDescriptions") == 0) {
    } else if (strcmp(name, "channelSettings") == 0) {
    } else if (strcmp(name, "channelStates") == 0) {
    } else if (strcmp(name, "deviceStates") == 0) {
      
    } else if (strcmp(name, "deviceProperties") == 0) {
      
    } else if (strcmp(name, "devicePropertyDescriptions") == 0) {
    
    } else if (strcmp(name, "customActions") == 0) {

    } else if (strcmp(name, "binaryInputDescriptions") == 0) {      
        
    } else if (strcmp(name, "binaryInputSettings") == 0) {      
      
    } else if (strcmp(name, "sensorDescriptions") == 0) {
      dsvdc_property_t *reply;
      ret = dsvdc_property_new(&reply);
      if (ret != DSVDC_OK) {
        vdc_report(LOG_ERR, "failed to allocate reply property for %s\n", name);
        free(name);
        continue;
      }

      int i = 0;
      char sensorName[64];
      char sensorIndex[64];
	    
      while(1) {
        if (airq_device->device->sensor_values[i].is_active) {
          vdc_report(LOG_ERR, "************* %d %s\n", i, airq_device->device->sensor_values[i].value_name);
        
          snprintf(sensorName, 64, "%s-%s", airq_device->device->name, airq_device->device->sensor_values[i].value_name);

          dsvdc_property_t *nProp;
          if (dsvdc_property_new(&nProp) != DSVDC_OK) {
            vdc_report(LOG_ERR, "failed to allocate reply property for %s/%s\n", name, sensorName);
            break;
          }
          dsvdc_property_add_string(nProp, "name", sensorName);
          dsvdc_property_add_uint(nProp, "sensorType", airq_device->device->sensor_values[i].sensor_type);
          dsvdc_property_add_uint(nProp, "sensorUsage", airq_device->device->sensor_values[i].sensor_usage);
          dsvdc_property_add_double(nProp, "aliveSignInterval", 300);

          snprintf(sensorIndex, 64, "%d", i);
          dsvdc_property_add_property(reply, sensorIndex, &nProp);

          vdc_report(LOG_INFO, "sensorDescription: dsuid %s sensorIndex %s: %s type %d usage %d\n", dsuid, sensorIndex, sensorName, airq_device->device->sensor_values[i].sensor_type, airq_device->device->sensor_values[i].sensor_usage); 
          
          i++;
        } else {
          break;
        }
      }

		  dsvdc_property_add_property(property, name, &reply);  
    } else if (strcmp(name, "sensorSettings") == 0) {
      dsvdc_property_t *reply;
      ret = dsvdc_property_new(&reply);
      if (ret != DSVDC_OK) {
        vdc_report(LOG_ERR, "failed to allocate reply property for %s\n", name);
        free(name);
        continue;
      }

      char sensorIndex[64];
      int i = 0;
      while (1) {
        if (airq_device->device->sensor_values[i].is_active) {
          dsvdc_property_t *nProp;
          if (dsvdc_property_new(&nProp) != DSVDC_OK) {
            vdc_report(LOG_ERR, "failed to allocate reply property for %s\n", name);
            break;
          }
          dsvdc_property_add_uint(nProp, "group", 8);
          dsvdc_property_add_uint(nProp, "minPushInterval", 5);
          dsvdc_property_add_double(nProp, "changesOnlyInterval", 5);

          snprintf(sensorIndex, 64, "%d", i);
          dsvdc_property_add_property(reply, sensorIndex, &nProp);
          
          i++;
        } else {
          break;
        }
      }
      dsvdc_property_add_property(property, name, &reply); 
      
      vdc_report(LOG_INFO, "sensorSettings: dsuid %s sensorIndex %s\n", dsuid, sensorIndex); 

    } else if (strcmp(name, "sensorStates") == 0) {
      dsvdc_property_t *reply;
      ret = dsvdc_property_new(&reply);
      if (ret != DSVDC_OK) {
        vdc_report(LOG_ERR, "failed to allocate reply property for %s\n", name);
        free(name);
        continue;
      }

      int idx;
      char* sensorIndex;
      dsvdc_property_t *sensorRequest;
      dsvdc_property_get_property_by_index(query, 0, &sensorRequest);
      if (dsvdc_property_get_name(sensorRequest, 0, &sensorIndex) != DSVDC_OK) {
        vdc_report(LOG_DEBUG, "sensorStates: no index in request\n");
        idx = -1;
      } else {
        idx = strtol(sensorIndex, NULL, 10);
      }
      dsvdc_property_free(sensorRequest);

      time_t now = time(NULL);
      
      int i = 0;
      while (1) {
        if (airq_device->device->sensor_values[i].is_active) {
          if (idx >= 0 && idx != i) {
            i++;
            continue;
          }

          dsvdc_property_t *nProp;
          if (dsvdc_property_new(&nProp) != DSVDC_OK) {
            vdc_report(LOG_ERR, "failed to allocate reply property for %s\n", name);
            break;
          }

          double val = airq_device->device->sensor_values[i].value;

          dsvdc_property_add_double(nProp, "value", val);
          dsvdc_property_add_int(nProp, "age", now - airq_device->device->sensor_values[i].last_query);
          dsvdc_property_add_int(nProp, "error", 0);

          char replyIndex[64];
          snprintf(replyIndex, 64, "%d", i);
          dsvdc_property_add_property(reply, replyIndex, &nProp);
          
          i++;
        } else {
          break;
        }
      }
      dsvdc_property_add_property(property, name, &reply);  

    } else if (strcmp(name, "binaryInputStates") == 0) {      

    } else if (strcmp(name, "name") == 0) {
      dsvdc_property_add_string(property, name, airq_device->device->name);

    } else if (strcmp(name, "type") == 0) {
      dsvdc_property_add_string(property, name, "vDSD");

    } else if (strcmp(name, "model") == 0) {
      char info[256];
      strcpy(info, "AirQ");
     
      dsvdc_property_add_string(property, name, info);
    } else if (strcmp(name, "modelFeatures") == 0) {
      dsvdc_property_t *nProp;
      dsvdc_property_new(&nProp);
      dsvdc_property_add_bool(nProp, "dontcare", false);
      dsvdc_property_add_bool(nProp, "blink", false);
      dsvdc_property_add_bool(nProp, "outmode", false);
      dsvdc_property_add_bool(nProp, "jokerconfig", true);
      dsvdc_property_add_property(property, name, &nProp);
    } else if (strcmp(name, "modelUID") == 0) {      
      dsvdc_property_add_string(property, name, "AirQ");

    } else if (strcmp(name, "modelVersion") == 0) {
      dsvdc_property_add_string(property, name, "0");

    } else if (strcmp(name, "deviceClass") == 0) {
    } else if (strcmp(name, "deviceClassVersion") == 0) {
    } else if (strcmp(name, "oemGuid") == 0) {
    } else if (strcmp(name, "oemModelGuid") == 0) {

    } else if (strcmp(name, "vendorId") == 0) {
      dsvdc_property_add_string(property, name, "vendor: airq");

    } else if (strcmp(name, "vendorName") == 0) {
      dsvdc_property_add_string(property, name, "AirQ");

    } else if (strcmp(name, "vendorGuid") == 0) {
      char info[256];
      strcpy(info, "AirQ vDC ");
      strcat(info, airq_device->device->id);
      dsvdc_property_add_string(property, name, info);

    } else if (strcmp(name, "hardwareVersion") == 0) {
      dsvdc_property_add_string(property, name, "0.0.0");

    } else if (strcmp(name, "configURL") == 0) {
      dsvdc_property_add_string(property, name, "");

    } else if (strcmp(name, "hardwareModelGuid") == 0) {
      dsvdc_property_add_string(property, name, "");

    } else if (strcmp(name, "deviceIcon16") == 0) {
        dsvdc_property_add_bytes(property, name, gIconStation16Data, gIconStation16Size);

    } else if (strcmp(name, "deviceIcon48") == 0) {
        dsvdc_property_add_bytes(property, name, gIconStation48Data, gIconStation48Size);

    } else if (strcmp(name, "deviceIconName") == 0) {
      char info[256];
      strcpy(info, "airq-airq-16.png");
      
      dsvdc_property_add_string(property, name, info);

    } else {
      vdc_report(LOG_WARNING, "get property handler: unhandled name=\"%s\"\n", name);
    }

    free(name);
  }

  pthread_mutex_unlock(&g_network_mutex);
  dsvdc_send_get_property_response(handle, property);
}
