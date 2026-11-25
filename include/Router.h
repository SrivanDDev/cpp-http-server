#pragma once

#include "HttpResponse.h"
#include "Request.h"

struct ServerConfig;

Response route(const Request &request, const ServerConfig &config);
