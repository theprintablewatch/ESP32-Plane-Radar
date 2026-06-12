#pragma once

void statusScreenPortal();
void statusScreenConnectFailed();
void statusScreenWifiReset();

/** Static "IP address" screen (one of the tap-cycle views). */
void statusScreenIp(const char* ip);

/** Saved-network connect animation (call Tick until connect finishes). */
void statusScreenConnectingBegin(const char* ssid);
void statusScreenConnectingTick();
