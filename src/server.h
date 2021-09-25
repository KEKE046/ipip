#pragma once
#include<json/json.h>

namespace ipip{
    void initServer(int port);
    void stopServer();
    bool popQueue(Json::Value & result);
}