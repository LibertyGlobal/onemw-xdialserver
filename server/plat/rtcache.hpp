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

#ifndef _RT_CACHE_H_
#define _RT_CACHE_H_

#include "rtRemoteObjectCache.hpp"
#include "rtObject.h"
#include "rtRemote.h"
#include <sstream>
#include <iostream>
#include <chrono>
#include <stdbool.h>
#include <mutex>
#include <condition_variable>
#include <string>

using namespace std;

class rtAppStatusCache : public rtObject
{
public:
    rtAppStatusCache(rtRemoteEnvironment* env) {ObjectCache = new rtRemoteObjectCache(env);};
    ~rtAppStatusCache() {delete(ObjectCache); };
    rtError UpdateAppStatusCache(rtValue app_status);
    std::string SearchAppStatusInCache(const char *app_name);
    bool WaitForAppState(const char * app_name, const char * desired_state, unsigned int timeout_ms);
private:
    bool doIdExist(std::string id);
    std::string SearchAppStatusInCacheLocked(const char *app_name);
    rtRemoteObjectCache* ObjectCache;
    std::mutex CacheMutex;
    std::condition_variable CacheCondVar;
    bool CacheModified = false;
};

#endif
