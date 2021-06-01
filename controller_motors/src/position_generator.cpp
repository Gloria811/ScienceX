#include <std_msgs/Float32.h>
#include "ros/ros.h"
#include "std_msgs/String.h"
#include <string>
#include "message_filters/subscriber.h"
#include "message_filters/time_synchronizer.h"
#include "message_filters/cache.h"


#include <iostream>
#include <chrono>
#include <thread>
#include <pthread.h>
#include <sys/mman.h>
#include <math.h>

using namespace std;
using namespace std::chrono;

static const int rate = 1000; // [1/ms]

static const double sin_demi_period = rate;
static const double sin_amplitude = 100000.0;

#define PI 3.14159265

int main(int argc, char **argv){

	ros::init(argc, argv, "position_generator");
	ros::NodeHandle n;

    ros::Publisher position_generator_pub = n.advertise<std_msgs::Float32>("motor_position_generator", 1000);

    ros::Rate loop_rate(rate);

    int count = 0;
    while (ros::ok())
    {
        std_msgs::Float32 msg;
        msg.data = sin_amplitude * sin(PI * (count / sin_demi_period));

        position_generator_pub.publish(msg);

        cout << "message sent\n";

        ros::spinOnce();

        loop_rate.sleep();
        ++count;
    }

	return 0;
}
