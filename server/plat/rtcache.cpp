/*
 * If not stated otherwise in this file or this component's Licenses.txt file the
 * following copyright and licenses apply:
 *
 * Copyright 2019 RDK Management
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
*/

#include <glib.h>
#include "rtcache.hpp"

rtError rtAppStatusCache::UpdateAppStatusCache(rtValue app_status)
{
     std::unique_lock<std::mutex> lock(CacheMutex);
     g_print("RTCACHE : %s\n",__FUNCTION__);

      rtError err;
      rtObjectRef temp = app_status.toObject();

      std::string cache_name_id = std::string(temp.get<rtString>("applicationName").cString());
      g_print("App Name = %s\nApp ID = %s\nApp State = %s\nError = %s\n",cache_name_id.c_str(),temp.get<rtString>("applicationId").cString(),temp.get<rtString>("state").cString(),temp.get<rtString>("error").cString());

      if(doIdExist(cache_name_id)) {
          g_print("erasing old data\n");
          err = ObjectCache->erase(cache_name_id);
      }

      err = ObjectCache->insert(cache_name_id, temp);
      CacheModified = true;
      CacheCondVar.notify_all(); 
      return err;
}

bool rtAppStatusCache::WaitForAppState(const char * app_name, const char * desired_state, unsigned int timeout_ms) {
     bool result = false;
     bool wait_for_stopped = (strcmp(desired_state, "stopped") == 0);
     std::unique_lock<std::mutex> lock(CacheMutex);
     unsigned int time_left = timeout_ms;
     unsigned long time_elapsed = 0;
     CacheModified = false;
     std::chrono::steady_clock::time_point start_time = std::chrono::steady_clock::now();
     g_print("RTCACHE : %s Enter waiting application: %s state: %s\n", __FUNCTION__, app_name, desired_state);
     while (time_elapsed < timeout_ms) {
          const char * state = SearchAppStatusInCacheLocked(app_name);
          if (strcmp(state, "NOT_FOUND") == 0) {
               g_print("RTCACHE : %s application: %s Not found\n", __FUNCTION__, app_name);
               if (wait_for_stopped) {
                    result = true;
                    g_print("RTCACHE : %s waiting for stopped - leave\n", __FUNCTION__);
                    break;
               }
          }
          if (strcmp(state, desired_state) == 0) {
               g_print("RTCACHE : %s desired state: %s, application: %s - leave\n", __FUNCTION__, desired_state, app_name);
               result = true;
               break;
          }
          g_print("RTCACHE : %s Waiting ... %u ms\n", __FUNCTION__, time_left);
          CacheCondVar.wait_for(lock, std::chrono::milliseconds(time_left), [this] { return CacheModified;});
          CacheModified = false;
          g_print("RTCACHE : %s Waitinig done\n", __FUNCTION__);
          std::chrono::steady_clock::time_point now_time = std::chrono::steady_clock::now();
          time_elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now_time - start_time).count();
          g_print("RTCACHE : %s Elapsed time %ld\n", __FUNCTION__, time_elapsed);
          if (time_elapsed < timeout_ms) {
               time_left = timeout_ms - time_elapsed;
               g_print("RTCACHE : %s time_left: %u\n", __FUNCTION__, time_left);
          }
     }
     g_print("RTCACHE : %s Leave application: %s state: %s %d\n", __FUNCTION__, app_name, desired_state, result);
     return result;
}

const char * rtAppStatusCache::SearchAppStatusInCacheLocked(const char *app_name)
{
     g_print("RTCACHE : %s\n",__FUNCTION__);

      if(doIdExist(app_name))
      {
         rtObjectRef state_param = ObjectCache->findObject(std::string(app_name));

         char *state = strdup(state_param.get<rtString>("state").cString());
         g_print("App Name = %s\nApp ID = %s\nError = %s\n",state_param.get<rtString>("applicationName").cString(),state_param.get<rtString>("applicationId").cString(),state_param.get<rtString>("error").cString());
         g_print("App State = %s\n",state);
         return state;
      }

      return "NOT_FOUND";

}

const char * rtAppStatusCache::SearchAppStatusInCache(const char *app_name)
{
     std::unique_lock<std::mutex> lock(CacheMutex);
     return SearchAppStatusInCacheLocked(app_name);
}

bool rtAppStatusCache::doIdExist(std::string id)
{
    g_print("RTCACHE : %s : \n",__FUNCTION__);
    auto now = std::chrono::steady_clock::now();

    if(ObjectCache->touch(id,now)!= RT_OK)
    {
       g_print("False\n");
       return false;
    }
    g_print("True\n");
    return true;
}
