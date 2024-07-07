/*
 Author: Alexander Knauer <a-x-e@gmx.net>
 License: Apache 2.0
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <syslog.h>

#include <digitalSTROM/dsuid.h>
#include <dsvdc/dsvdc.h>

#define MAX_SENSOR_VALUES 20

typedef struct scene {
  int dsId;
  double currentTemperature;
  double currentHumidity;
  double currentCO;
  double currentCO2;
  double currentAirPressure;
  double currentNoise;
} scene_t;

typedef struct sensor_value {
  bool is_active;
  char *value_name;
  int sensor_type;
  int sensor_usage;
  double value;
  double last_value;
  time_t last_query;
  time_t last_reported;
} sensor_value_t;

typedef struct airq_device {
  dsuid_t dsuid;
  char *id;
  char *name;
  char *ip;
  char *password;
  
  sensor_value_t sensor_values[MAX_SENSOR_VALUES];
  uint16_t zoneID;
} airq_device_t;

typedef struct airq_data {
  airq_device_t device;
} airq_data_t;

typedef struct airq_vdcd {
  struct airq_vdcd* next;
  dsuid_t dsuid;
  char dsuidstring[36];
  bool announced;
  bool presentSignaled;
  bool present;
  airq_device_t* device;
} airq_vdcd_t;

#define AIRQ_OK 0
#define AIRQ_OUT_OF_MEMORY -1
#define AIRQ_AUTH_FAILED -10
#define AIRQ_BAD_CONFIG -12
#define AIRQ_CONNECT_FAILED -13
#define AIRQ_GETMEASURE_FAILED -14

extern const char *g_cfgfile;
extern int g_shutdown_flag;
extern airq_data_t airq;
extern airq_vdcd_t* airq_device;
extern pthread_mutex_t g_network_mutex;
extern scene_t* airq_current_values;

extern char g_vdc_modeluid[33];
extern char g_vdc_dsuid[35];
extern char g_lib_dsuid[35];

extern time_t g_reload_values;
extern int g_default_zoneID;

extern void vdc_new_session_cb(dsvdc_t *handle __attribute__((unused)), void *userdata);
extern void vdc_ping_cb(dsvdc_t *handle __attribute__((unused)), const char *dsuid, void *userdata __attribute__((unused)));
extern void vdc_announce_device_cb(dsvdc_t *handle __attribute__((unused)), int code, void *arg, void *userdata __attribute__((unused)));
extern void vdc_announce_container_cb(dsvdc_t *handle __attribute__((unused)), int code, void *arg, void *userdata __attribute__((unused)));
extern void vdc_end_session_cb(dsvdc_t *handle __attribute__((unused)), void *userdata);
extern bool vdc_remove_cb(dsvdc_t *handle __attribute__((unused)), const char *dsuid, void *userdata);
extern void vdc_blink_cb(dsvdc_t *handle __attribute__((unused)), char **dsuid, size_t n_dsuid, int32_t *group, int32_t *zone_id, void *userdata);
extern void vdc_getprop_cb(dsvdc_t *handle, const char *dsuid, dsvdc_property_t *property, const dsvdc_property_t *query, void *userdata);
extern void vdc_setprop_cb(dsvdc_t *handle, const char *dsuid, dsvdc_property_t *property, const dsvdc_property_t *properties, void *userdata);
extern void vdc_callscene_cb(dsvdc_t *handle __attribute__((unused)), char **dsuid, size_t n_dsuid, int32_t scene, bool force, int32_t *group, int32_t *zone_id, void *userdata);
extern void vdc_savescene_cb(dsvdc_t *handle __attribute__((unused)), char **dsuid, size_t n_dsuid, int32_t scene, int32_t *group, int32_t *zone_id, void *userdata);
extern void vdc_request_generic_cb(dsvdc_t *handle __attribute__((unused)), char *dsuid, char *method_name, dsvdc_property_t *property, const dsvdc_property_t *properties,  void *userdata);

int airq_get_values();
void push_sensor_data();
int decodeURIComponent (char *sSource, char *sDest);
sensor_value_t* find_sensor_value_by_name(char *key);

int write_config();
int read_config();

void vdc_init_report();
void vdc_set_debugLevel(int debug);
int vdc_get_debugLevel();
void vdc_report(int errlevel, const char *fmt, ... );
void vdc_report_extraLevel(int errlevel, int maxErrlevel, const char *fmt, ... );
