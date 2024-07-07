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
#include <ctype.h>
#include <pthread.h>

#include <curl/curl.h>
#include <json.h>
#include <utlist.h>

#include <digitalSTROM/dsuid.h>
#include <dsvdc/dsvdc.h>

#include <openssl/evp.h>
#include <math.h>
#include <openssl/bio.h>

#include "airq.h"

struct memory_struct {
  char *memory;
  size_t size;
};

struct data {
  char trace_ascii; /* 1 or 0 */
};

struct curl_slist *cookielist;

static size_t WriteMemoryCallback(void *contents, size_t size, size_t nmemb, void *userp) {
  size_t realsize = size * nmemb;
  struct memory_struct *mem = (struct memory_struct *) userp;

  mem->memory = realloc(mem->memory, mem->size + realsize + 1);
  if (mem->memory == NULL) {
    vdc_report(LOG_ERR, "network module: not enough memory (realloc returned NULL)\n");
    return 0;
  }

  memcpy(&(mem->memory[mem->size]), contents, realsize);
  mem->size += realsize;
  mem->memory[mem->size] = 0;
  
  return realsize;
}

static void DebugDump(const char *text, FILE *stream, unsigned char *ptr, size_t size, char nohex) {
  size_t i;
  size_t c;

  unsigned int width = 0x10;

  if (nohex)
    /* without the hex output, we can fit more on screen */
    width = 0x40;

  fprintf(stream, "%s, %10.10ld bytes (0x%8.8lx)\n", text, (long) size, (long) size);

  for (i = 0; i < size; i += width) {
    fprintf(stream, "%4.4lx: ", (long) i);

    if (!nohex) {
      /* hex not disabled, show it */
      for (c = 0; c < width; c++)
        if (i + c < size)
          fprintf(stream, "%02x ", ptr[i + c]);
        else
          fputs("   ", stream);
    }

    for (c = 0; (c < width) && (i + c < size); c++) {
      /* check for 0D0A; if found, skip past and start a new line of output */
      if (nohex && (i + c + 1 < size) && ptr[i + c] == 0x0D && ptr[i + c + 1] == 0x0A) {
        i += (c + 2 - width);
        break;
      }
      fprintf(stream, "%c", (ptr[i + c] >= 0x20) && (ptr[i + c] < 0x80) ? ptr[i + c] : '.');
      /* check again for 0D0A, to avoid an extra \n if it's at width */
      if (nohex && (i + c + 2 < size) && ptr[i + c + 1] == 0x0D && ptr[i + c + 2] == 0x0A) {
        i += (c + 3 - width);
        break;
      }
    }
    fputc('\n', stream); /* newline */
  }
  fflush(stream);
}

static int DebugCallback(CURL *handle, curl_infotype type, char *data, size_t size, void *userp) {
  struct data *config = (struct data *) userp;
  const char *text;
  (void) handle; /* prevent compiler warning */

  switch (type) {
    case CURLINFO_TEXT:
      fprintf(stderr, "== Info: %s", data);
    default: /* in case a new one is introduced to shock us */
      return 0;

    case CURLINFO_HEADER_OUT:
      text = "=> Send header";
      break;
    case CURLINFO_DATA_OUT:
      text = "=> Send data";
      break;
    case CURLINFO_SSL_DATA_OUT:
      text = "=> Send SSL data";
      break;
    case CURLINFO_HEADER_IN:
      text = "<= Recv header";
      break;
    case CURLINFO_DATA_IN:
      text = "<= Recv data";
      break;
    case CURLINFO_SSL_DATA_IN:
      text = "<= Recv SSL data";
      break;
  }
  DebugDump(text, stdout, (unsigned char *) data, size, config->trace_ascii);
  return 0;
}

int decodeURIComponent (char *sSource, char *sDest) {
  int nLength;
  for (nLength = 0; *sSource; nLength++) {
    if (*sSource == '%' && sSource[1] && sSource[2] && isxdigit(sSource[1]) && isxdigit(sSource[2])) {
      sSource[1] -= sSource[1] <= '9' ? '0' : (sSource[1] <= 'F' ? 'A' : 'a')-10;
      sSource[2] -= sSource[2] <= '9' ? '0' : (sSource[2] <= 'F' ? 'A' : 'a')-10;
      sDest[nLength] = 16 * sSource[1] + sSource[2];
      sSource += 3;
      continue;
    }
    sDest[nLength] = *sSource++;
  }
  sDest[nLength] = '\0';
  return nLength;
}

struct memory_struct* http_get(const char *url) {
  CURL *curl;
  CURLcode res;
  struct memory_struct *chunk;

  chunk = malloc(sizeof(struct memory_struct));
  if (chunk == NULL) {
    vdc_report(LOG_ERR, "network: not enough memory\n");
    return NULL;
  }
  chunk->memory = malloc(1);
  chunk->size = 0;

  curl = curl_easy_init();
  if (curl == NULL) {
    vdc_report(LOG_ERR, "network: curl init failure\n");
    free(chunk->memory);
    free(chunk);
    return NULL;
  }
      
  struct curl_slist *headers = NULL;

  curl_easy_setopt(curl, CURLOPT_URL, url);
  curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteMemoryCallback);
  curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void * )chunk);
  curl_easy_setopt(curl, CURLOPT_HTTPGET, 1);

  
  if (vdc_get_debugLevel() > LOG_DEBUG) {
    struct data config;
    config.trace_ascii = 1; /* enable ascii tracing */

    curl_easy_setopt(curl, CURLOPT_DEBUGFUNCTION, DebugCallback);
    curl_easy_setopt(curl, CURLOPT_DEBUGDATA, &config);
    /* the DEBUGFUNCTION has no effect until we enable VERBOSE */
    curl_easy_setopt(curl, CURLOPT_VERBOSE, 1L);
  }
  
  res = curl_easy_perform(curl);

  if (res != CURLE_OK) {
    vdc_report(LOG_ERR, "network: curl_easy_perform() failed: %s\n", curl_easy_strerror(res));
    
    if (chunk != NULL) {
      if (chunk->memory != NULL) {
        free(chunk->memory);
      }
      free(chunk);
      chunk = NULL;
    }
  } else {
    //vdc_report(LOG_ERR, "Response: %s\n", chunk->memory);    // results in segmentation fault if response is too long
    res = curl_easy_getinfo(curl, CURLINFO_COOKIELIST, &cookielist);

    long response_code;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response_code);

    if (response_code == 403 || response_code == 404 || response_code == 503) {
      vdc_report(LOG_ERR, "AirQ server response: %d - ignoring response\n", response_code);
      if (chunk != NULL) {
        if (chunk->memory != NULL) {
          free(chunk->memory);
        }
        free(chunk);
        chunk = NULL;
      }
    } 
  }

  curl_slist_free_all(headers);
  curl_easy_cleanup(curl);

  return chunk;
}

int parse_json_data(unsigned char* response ) {
  bool changed_values = FALSE;
  time_t now;
    
  now = time(NULL);
  vdc_report(LOG_DEBUG, "network: airq values response = %s\n", response);
  
  json_object *jobj = json_tokener_parse(response);
  
  if (NULL == jobj) {
    vdc_report(LOG_ERR, "network: parsing json data failed, data:\n%s\n", response);
    return AIRQ_GETMEASURE_FAILED;
  }

  //pthread_mutex_lock(&g_network_mutex);

  json_object_object_foreach(jobj, key, val) {
    enum json_type type = json_object_get_type(val);
    
    sensor_value_t* svalue;
    
    //save all relevant data rettrieved from AirQ as current values in memory; 
    if (strcmp(key, "co2") == 0) {
      //airq_current_values->currentCO2 = json_object_get_string(val);
    } 
    
    
    svalue = find_sensor_value_by_name(key);
    if (svalue == NULL) {
      vdc_report(LOG_WARNING, "value %s is not configured for evaluation - ignoring\n", key);
    } else {
      if (type == json_type_array) {
        json_object* array_obj;
        bool ret = json_object_object_get_ex(jobj,key,&array_obj);
        if (ret) {
          json_object *jvalue;
          jvalue = json_object_array_get_idx(array_obj, 0);
          type = json_object_get_type(jvalue);
        
          if (type == json_type_double) {
            vdc_report(LOG_WARNING, "network: getdata returned key: %s value: %f\n", key, json_object_get_double(jvalue));
            
            //if ((svalue->last_reported == 0) || (svalue->last_value != json_object_get_boolean(val)) || (now - svalue->last_reported) > 180) {
            if ((svalue->last_reported == 0) || (svalue->last_value != json_object_get_double(jvalue))) {
              changed_values = TRUE;
            }
            svalue->last_value = svalue->value;
            svalue->value = json_object_get_double(jvalue);
            svalue->last_query = now;
          } else if (type == json_type_int) {
            vdc_report(LOG_WARNING, "network: getdata returned key: %s value: %d\n", key, json_object_get_int(jvalue));
            
            //if ((svalue->last_reported == 0) || (svalue->last_value != json_object_get_boolean(val)) || (now - svalue->last_reported) > 180) {
            if ((svalue->last_reported == 0) || (svalue->last_value != json_object_get_int(jvalue))) {
              changed_values = TRUE;
            }
            svalue->last_value = svalue->value;
            svalue->value = json_object_get_int(jvalue);
            svalue->last_query = now;
          }
        }
      }
    }
  }

	pthread_mutex_unlock(&g_network_mutex);
  
  json_object_put(jobj);
   
  if (changed_values ) {
    return 0;
  } else return 1;
}

uint8_t* base64_decode(const char* input) {
    BIO* bio, * b64;
    int decodeLen = (int)(strlen(input) * 3 / 4 + 1);
    uint8_t* buffer = (uint8_t *)malloc(decodeLen);
    memset(buffer, 0, decodeLen);
       
    bio = BIO_new_mem_buf(input, -1);
    b64 = BIO_new(BIO_f_base64());
    bio = BIO_push(b64, bio);
    BIO_set_flags(bio, BIO_FLAGS_BASE64_NO_NL);
    decodeLen = BIO_read(bio, buffer, decodeLen);

    BIO_free_all(bio);

    return buffer;
}

unsigned char* decrypt(const char* msgb64, char* password) {
    char airqpass[33] = {0};
    strncpy(airqpass, password, 32);
    if (strlen(airqpass) < 32) {
        for (int i = strlen(airqpass); i < 32; i++) {
            airqpass[i] = '0';
        }
    } else if (strlen(airqpass) > 32) {
        airqpass[32] = '\0';
    }

    char* key = (char*)airqpass;

    uint8_t* buffer = base64_decode(msgb64);

    char* iv = malloc(16);
    strncpy(iv, (char*)buffer, 16);
    
    uint8_t* ciphertext = (uint8_t*)(buffer + 16);
    int ciphertext_len = (strlen(msgb64) / 4 * 3 ) - 16;

    char* decrypted = malloc(ciphertext_len);
    int decrypted_len = 0;
    EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
    EVP_DecryptInit_ex(ctx, EVP_aes_256_cbc(), NULL, key, iv);
    EVP_DecryptUpdate(ctx, decrypted, &decrypted_len, ciphertext, ciphertext_len);
    EVP_CIPHER_CTX_free(ctx);

    decrypted[decrypted_len] = '\0';
    free(buffer);
      
    return decrypted;
}

int airq_get_values() {
  int rc;
  
  vdc_report(LOG_NOTICE, "network: reading AirQ values\n");

  char request_url[30];
  strcpy(request_url, "http://");
  strcat(request_url, airq.device.ip);
  strcat(request_url, "/data");  
  
  struct memory_struct *response = http_get(request_url);
  
  if (response == NULL) {
    vdc_report(LOG_ERR, "network: getting airq values failed\n");
    return AIRQ_CONNECT_FAILED;
  }
  
  vdc_report(LOG_INFO, "network: response: %s\n", response->memory);
  json_object *jobj = json_tokener_parse(response->memory);
  
  if (NULL == jobj) {
    vdc_report(LOG_ERR, "network: parsing json data failed, data:\n%s\n", response);
    return AIRQ_GETMEASURE_FAILED;
  }

  json_object_object_foreach(jobj, key, val) {
    if (strcmp(key, "content") == 0) {
      unsigned char* decrypted = decrypt(json_object_get_string(val), airq.device.password);
      vdc_report(LOG_INFO, "network: decrypted: %s\n", decrypted);
      rc = parse_json_data(decrypted);
    } 
  }
  

  free(response->memory);
  free(response);
  
  return rc;  
}

