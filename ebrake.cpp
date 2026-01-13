#include <iostream>
#include <stdio.h>
#include <Winsock2.h>
#include <Ws2tcpip.h>
#include <Mswsock.h>
#include "follower.cpp"
#include "lead.cpp"
#include <cmath>

SOCKET trucksocket[4];
void propagate_warning(SOCKET trucksock[]) {
	char warnmessage[1024] = "EBRAKE";
	for (int i = 0; i < sizeof(trucksock); i++) {
		send(trucksock[i], warnmessage, sizeof(warnmessage), 0);
	}
	return;
}

void emergencybraking(bool warning) {
	propagate_warning(trucksocket);
	float deceleration,actspeed;
	actspeed=getspeed()*(1000/3600);//from km/h to m/s

	while(warning){
		if (speed > 0) {
			deceleration= pow(actspeed,2)/(getdistance()*2);// m/s^2
			setbrake(deceleration);
		} else {
			setspeed(0); //park
		}
	}
	return;
}

