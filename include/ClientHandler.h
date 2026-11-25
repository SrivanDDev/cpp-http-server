#pragma once

struct ServerConfig;

// The caller owns the socket and closes it after this function returns.
void handleClient(int clientSocket, const ServerConfig &config);
