#include <iostream>
#include <stdio.h>
#include "follower.cpp"
#include "lead.cpp"
#include <cmath>


// bool distance_monitoring (float truckdistance, deque<float> frontdistance){
// 	const float safedistance=20;//m
// 	float lastdistance;
// 	bool obstacle_detected = false;

// 	if(frontdistance.front()<truckdistance){
// 		//wait for new reading
// 		if(frontdistance.size()>1){
// 			if(frontdistance[1]>frontdistance[0]){
// 				obstacle_detected=false;
// 				//normal braking for distance control
// 			} else if(frontdistance[1]<safedistance){
// 					obstacle_detected=true;
// 				}
// 			frontdistance.pop_front();//x0=x1
// 		}
// 	}
// 	return obstacle_detected;
// }


void emergencybraking(bool warning, truck follower,auto frontdistance) {
	//propagate_warning();
	//string warningcmd="EBRAKE";

	float deceleration, actspeed;

	while(warning){
		actspeed=truck.getSpeed()*(1000/3600);//from km/h to m/s
		if (truck.getSpeed() > 0) {
			deceleration= pow(actspeed,2)/(frontdistance*2);// m/s^2
			follower.setAccel(-decelaration);
		} else {
			follower.setAccel(0); //stopped
		}
	}
	return;
}

