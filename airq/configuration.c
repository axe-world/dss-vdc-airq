/*
 Author: Alexander Knauer <a-x-e@gmx.net>
 License: Apache 2.0
 */
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <libconfig.h>
#include <utlist.h>
#include <limits.h>

#include <digitalSTROM/dsuid.h>
#include <dsvdc/dsvdc.h>

#include "airq.h"

int read_config() {
  config_t config;
  struct stat statbuf;
  int i;
  char *sval;
  int ivalue;

  if (stat(g_cfgfile, &statbuf) != 0) {
    vdc_report(LOG_ERR, "Could not find configuration file %s\n", g_cfgfile);
    return -1;
  }
  if (!S_ISREG(statbuf.st_mode)) {
    vdc_report(LOG_ERR, "Configuration file \"%s\" is not a regular file", g_cfgfile);
    return -2;
  }

  config_init(&config);
  if (!config_read_file(&config, g_cfgfile)) {
    vdc_report(LOG_ERR, "Error in configuration: l.%d %s\n", config_error_line(&config), config_error_text(&config));
    config_destroy(&config);
    return -3;
  }

  if (config_lookup_string(&config, "vdcdsuid", (const char **) &sval))
    strncpy(g_vdc_dsuid, sval, sizeof(g_vdc_dsuid));
  if (config_lookup_string(&config, "libdsuid", (const char **) &sval))
    strncpy(g_lib_dsuid, sval, sizeof(g_lib_dsuid));
  if (config_lookup_int(&config, "reload_values", (int *) &ivalue))
    g_reload_values = ivalue;
  if (config_lookup_int(&config, "zone_id", (int *) &ivalue))
    g_default_zoneID = ivalue;
  if (config_lookup_int(&config, "debug", (int *) &ivalue)) {
    if (ivalue <= 10) {
      vdc_set_debugLevel(ivalue);
    }
  }
  if (config_lookup_string(&config, "airq.name", (const char **) &sval))
    airq.device.name = strdup(sval);
  if (config_lookup_string(&config, "airq.id", (const char **) &sval)) {
    airq.device.id = strdup(sval);
  } else {
    vdc_report(LOG_ERR, "mandatory parameter 'id' in section airq: is not set in airq.cfg\n");  
    exit(0);
  }
  if (config_lookup_string(&config, "airq.ip", (const char **) &sval)) {
    airq.device.ip = strdup(sval);
  } else {
    vdc_report(LOG_ERR, "mandatory parameter 'ip' is not set in airq.cfg\n");  
    exit(0);
  }  
  if (config_lookup_string(&config, "airq.password", (const char **) &sval)) {
    airq.device.password = strdup(sval);
  } else {
    vdc_report(LOG_ERR, "mandatory parameter 'password' is not set in airq.cfg\n");  
    exit(0);
  }
 
  char path[128];  
  i = 0;
  while(1) {
    sprintf(path, "sensor_values.s%d", i);
    if (i < MAX_SENSOR_VALUES && config_lookup(&config, path)) {
      sensor_value_t* value = &airq.device.sensor_values[i];
      
      sprintf(path, "sensor_values.s%d.value_name", i);
      if (config_lookup_string(&config, path, (const char **) &sval)) {
        value->value_name = strdup(sval);  
      } else {
        value->value_name = strdup("");  
      }
      
      sprintf(path, "sensor_values.s%d.sensor_type", i);
      if (config_lookup_int(&config, path, (int *) &ivalue))
        value->sensor_type = ivalue;  
      
      sprintf(path, "sensor_values.s%d.sensor_usage", i);
      if (config_lookup_int(&config, path, (int *) &ivalue))
        value->sensor_usage = ivalue;  
      
      value->is_active = true;
      
      i++;
    } else {
      while (i < MAX_SENSOR_VALUES) {
        airq.device.sensor_values[i].is_active = false;
        i++;
      }        
      break;
    }
  }

  if (g_cfgfile != NULL) {
    config_destroy(&config);
  }

  airq_device_t* device = &airq.device;
  if (device->id) {
    char buffer[128];
    strcpy(buffer, device->id);
    
    airq_device = malloc(sizeof(airq_vdcd_t));
    if (!airq_device) {
      return AIRQ_OUT_OF_MEMORY;
    }
    memset(airq_device, 0, sizeof(airq_vdcd_t));

    airq_device->announced = false;
    airq_device->present = true; 
    airq_device->device = device;

    dsuid_generate_v3_from_namespace(DSUID_NS_IEEE_MAC, buffer, &airq_device->dsuid);
    dsuid_to_string(&airq_device->dsuid, airq_device->dsuidstring);
  }

	return 0;
}

int write_config() {
  config_t config;
  config_setting_t* cfg_root;
  config_setting_t* setting;
  config_setting_t* airqsetting;
  int i;

  config_init(&config);
  cfg_root = config_root_setting(&config);

  setting = config_setting_add(cfg_root, "vdcdsuid", CONFIG_TYPE_STRING);
  if (setting == NULL) {
    setting = config_setting_get_member(cfg_root, "vdcdsuid");
  }
  config_setting_set_string(setting, g_vdc_dsuid);
  
  if (g_lib_dsuid != NULL && strcmp(g_lib_dsuid,"") != 0) { 
    setting = config_setting_add(cfg_root, "libdsuid", CONFIG_TYPE_STRING);
    if (setting == NULL) {
      setting = config_setting_get_member(cfg_root, "libdsuid");
    }  
    config_setting_set_string(setting, g_lib_dsuid);
  }
   
  setting = config_setting_add(cfg_root, "reload_values", CONFIG_TYPE_INT);
  if (setting == NULL) {
    setting = config_setting_get_member(cfg_root, "reload_values");
  }
  config_setting_set_int(setting, g_reload_values);

  setting = config_setting_add(cfg_root, "zone_id", CONFIG_TYPE_INT);
  if (setting == NULL) {
    setting = config_setting_get_member(cfg_root, "zone_id");
  }
  config_setting_set_int(setting, g_default_zoneID);

  setting = config_setting_add(cfg_root, "debug", CONFIG_TYPE_INT);
  if (setting == NULL) {
    setting = config_setting_get_member(cfg_root, "debug");
  }
  config_setting_set_int(setting, vdc_get_debugLevel());

  airqsetting = config_setting_add(cfg_root, "airq", CONFIG_TYPE_GROUP);
    
  setting = config_setting_add(airqsetting, "id", CONFIG_TYPE_STRING);
  if (setting == NULL) {
    setting = config_setting_get_member(airqsetting, "id");
  }
  config_setting_set_string(setting, airq.device.id);

  setting = config_setting_add(airqsetting, "name", CONFIG_TYPE_STRING);
  if (setting == NULL) {
    setting = config_setting_get_member(airqsetting, "name");
  }
  config_setting_set_string(setting, airq.device.name);
  
  setting = config_setting_add(airqsetting, "ip", CONFIG_TYPE_STRING);
  if (setting == NULL) {
    setting = config_setting_get_member(airqsetting, "ip");
  }
  config_setting_set_string(setting, airq.device.ip);
  
  setting = config_setting_add(airqsetting, "password", CONFIG_TYPE_STRING);
  if (setting == NULL) {
    setting = config_setting_get_member(airqsetting, "password");
  }
  config_setting_set_string(setting, airq.device.password); 
   
  
  char path[128];
  sprintf(path, "sensor_values");   
  config_setting_t *sensor_values_path = config_setting_add(cfg_root, path, CONFIG_TYPE_GROUP);

  if (sensor_values_path == NULL) {
    sensor_values_path = config_setting_get_member(setting, path);
  }
  
  i = 0;
  while(1) {
    if (i < MAX_SENSOR_VALUES && airq.device.sensor_values[i].value_name != NULL) {
      sensor_value_t* value = &airq.device.sensor_values[i];
      
      sprintf(path, "s%d", i);   
      config_setting_t *v = config_setting_add(sensor_values_path, path, CONFIG_TYPE_GROUP);
      
      setting = config_setting_add(v, "value_name", CONFIG_TYPE_STRING);
      if (setting == NULL) {
        setting = config_setting_get_member(v, "value_name");
      }
      config_setting_set_string(setting, value->value_name);
      
      setting = config_setting_add(v, "sensor_type", CONFIG_TYPE_INT);
      if (setting == NULL) {
        setting = config_setting_get_member(v, "sensor_type");
      }
      config_setting_set_int(setting, value->sensor_type);
      
      setting = config_setting_add(v, "sensor_usage", CONFIG_TYPE_INT);
      if (setting == NULL) {
        setting = config_setting_get_member(v, "sensor_usage");
      }
      config_setting_set_int(setting, value->sensor_usage);
      
      i++;
    } else {
      break;
    }   
  } 

  char tmpfile[PATH_MAX];
  sprintf(tmpfile, "%s.cfg.new", g_cfgfile);

  int ret = config_write_file(&config, tmpfile);
  if (!ret) {
    vdc_report(LOG_ERR, "Error while writing new configuration file %s\n", tmpfile);
    unlink(tmpfile);
  } else {
    rename(tmpfile, g_cfgfile);
  }

  config_destroy(&config);

  return 0;
}

sensor_value_t* find_sensor_value_by_name(char *key) {
  sensor_value_t* value;
  for (int i = 0; i < MAX_SENSOR_VALUES; i++) {
    value = &airq.device.sensor_values[i];
    if (value->value_name != NULL && strcasecmp(key, value->value_name) == 0) {
        return value;
    }
  }
  
  return NULL;
}
