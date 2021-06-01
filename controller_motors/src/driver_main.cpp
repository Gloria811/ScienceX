#include <xplore_msg/HandlingControl.h>
#include <sensor_msgs/JointState.h>
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
#include <pid/signal_manager.h>

#include <math.h>

#include <xcontrol/network_master.h>
#include <xcontrol/one_axis_slot.h>
#include <xcontrol/three_axis_slot.h>

using namespace std;
using namespace std::chrono;
using namespace ethercatcpp;
using namespace pid;
using namespace xcontrol;

using namespace message_filters;
using namespace sensor_msgs;

#define MOTOR_COUNT 7
#define for_chain(it) for(size_t it=0; it<MOTOR_COUNT; ++it) 

#define PRINT_STATE true
#define PRINT_MSGS false

Epos4::control_mode_t control_mode;
std::string network_interface_name;

double current_value[MOTOR_COUNT] = {0, 0, 0, 0, 0, 0, 0};

double target_value[MOTOR_COUNT] = {0, 0, 0, 0, 0, 0, 0};
double active[MOTOR_COUNT] = {0, 0, 0, 0, 0, 0, 0};

static const int period = 10; // [ms]

static const float step_size[MOTOR_COUNT] = {40.0*period, 40.0*period, 10.0*period, 10.0*period, 100.0*period, 100.0*period, 10.0*period};

static const float max_rads[MOTOR_COUNT] = {2*M_PI, 2*M_PI, 2*M_PI, 2*M_PI, 2*M_PI, 2*M_PI, 2*M_PI};
static const float min_rads[MOTOR_COUNT] = {0, 0, 0, 0, 0, 0, 0};

bool is_scanning = true;

void manual_increment_position_generator_callback(const xplore_msg::HandlingControl::ConstPtr& msg) {
	if(PRINT_MSGS)
		cout << "target value modified\n";
	for(size_t it=0; it<MOTOR_COUNT; it++) {
		active[it] = msg->active[it];
		if(PRINT_MSGS)
			cout << "active value " << it << " is " << active[it] << endl;
	}
}

void manual_position_generator_callback(const std_msgs::Float32::ConstPtr& msg) {
	if(PRINT_MSGS) {
		cout << "target value modified\n";
		cout << "target value is " << msg->data << endl;
	}
	for(size_t it=0; it<MOTOR_COUNT; it++) {
		target_value[it] = msg->data;
		active[it] = 0;
	}
}

void incremental_step_position_generator_callback(const xplore_msg::HandlingControl::ConstPtr& msg) {
	if(PRINT_MSGS)
		cout << "target value modified\n";
	for(size_t it=0; it<MOTOR_COUNT; it++) {
		active[it] = msg->active[it];
	}
}

void incremental_position_generator_callback(const std_msgs::Float32::ConstPtr& msg) {
	if(PRINT_MSGS)
		cout << "target value modified\n";
	target_value[0] += msg->data;
}

void position_generator_callback(const std_msgs::Float32::ConstPtr& msg) {
	if(PRINT_MSGS)
		cout << "target value modified\n";
	target_value[0] = msg->data;
}

void inverse_kinematics_callback(const sensor_msgs::JointState::ConstPtr& msg){
	/*
	* whenever a message of type WheelVeloicity is published on the /wheel_publisher topic
	* this callback is called
	*
	* wheel_callback updates the values of the target_value array which contains the reference 
	* individual wheel speeds expressed in RPMs
	*/
	for(size_t it=0; it<6; ++it) {
		target_value[it] = msg->position[it];
	}
}

void correct_target_values() {
	for_chain(it) {
		if(target_value[it] > max_rads[it]) {
			cout << "Motor " << it << " target_value over limit: " << std::dec <<target_value[it] << " rads (max: " << max_rads[it] << ")\n";
			target_value[it] = max_rads[it];
		}
		if(target_value[it] < min_rads[it]) {
			cout << "Motor " << it << " target_value under limit: " << std::dec <<target_value[it] << " rads (min: " << min_rads[it] << ")\n";
			target_value[it] = min_rads[it];
		}
	}
}

int main(int argc, char **argv) {

	ros::init(argc, argv, "hd_controller_motors");
	ros::NodeHandle n;


	ros::Subscriber sub1 = n.subscribe<std_msgs::Float32>("motor_position_generator", 10, position_generator_callback);
	ros::Subscriber sub2 = n.subscribe<std_msgs::Float32>("incremental_motor_position_generator", 10, incremental_position_generator_callback);
	ros::Subscriber sub3 = n.subscribe<xplore_msg::HandlingControl>("incremental_step_motor_position_generator", 10, incremental_step_position_generator_callback);
	ros::Subscriber sub4 = n.subscribe<xplore_msg::HandlingControl>("cmd_hd", 10, incremental_step_position_generator_callback);
	ros::Subscriber sub5 = n.subscribe<std_msgs::Float32>("manual_motor_position_generator", 10, manual_position_generator_callback);
	ros::Subscriber sub6 = n.subscribe<xplore_msg::HandlingControl>("manual_increment_motor_position_generator", 10, manual_increment_position_generator_callback);
	// ros::Subscriber sub7 = n.subscribe<sensor_msgs::JointState>("put what u use", 10, inverse_kinematics_callback);


	cout << "ROS node initialized" << endl;

	network_interface_name = "eth2";
	std::string input_target = "0";
	for(size_t i=0; i<MOTOR_COUNT; i++) {
		target_value[i] = atof(input_target.c_str());
		if(PRINT_STATE) {
			cout << "target value:" << target_value[i] << "\n";
		}
	}

	control_mode = Epos4::position_CSP;
    //control_mode = Epos4::profile_position_PPM;

  	// Device definition
	// 3-axis: 1st slot next to ETHERNET-IN
	xcontrol::OneAxisSlot epos_1(true);
	xcontrol::ThreeAxisSlot epos_2(true), epos_3(true), epos_4(true);
	xcontrol::ThreeAxisSlot epos_5(true), epos_6(true), epos_7(true);

	vector<xcontrol::Epos4Extended*> chain = {&epos_1, &epos_2, &epos_3, &epos_4, &epos_5, &epos_6, &epos_7};

	xcontrol::NetworkMaster ethercat_master(chain, network_interface_name);

	ethercat_master.init_network();

	cout << "Ethercat network online" << endl;

	high_resolution_clock::time_point time_point_start_loop;

	while(ros::ok()){ 
		// Get current time
		time_point_start_loop = high_resolution_clock::now();
		// check device status
		ethercat_master.switch_motors_to_enable_op();

		// correct_target_values();

			for(size_t it=0; it<MOTOR_COUNT; it++) {
				target_value[it] += active[it] * step_size[it];
				if(PRINT_STATE) {
					cout << "active" << it << " : " << active[it] << " ";
				}
			}
			if(PRINT_STATE) {
				cout << "\n";
			}
			for(size_t it=0; it<chain.size(); ++it) {
				if(chain[it]->get_has_motor()) {
					chain[it]->set_Control_Mode(control_mode);

					if (chain[it]->get_Device_State_In_String() == "Operation enable") {
						if (control_mode == Epos4::position_CSP) {
							if(PRINT_STATE) {
								cout << "Motor " << it << "\n";
								cout << "Desired position value = " << std::dec <<target_value[it] << " qc" << "\n";
							}
							if(!is_scanning) {
								chain[it]->set_Target_Position_In_Qc(target_value[it]);
							}
						}
					}
				}
			}

		bool wkc = ethercat_master.next_Cycle(); // Function used to launch next cycle of the EtherCat net

		if (wkc == true) {
			for(size_t it=0; it<chain.size(); ++it) {
				if(chain[it]->get_has_motor()) {
					if(PRINT_STATE) {
						cout << "Motor " << it << "\n";
						cout << "State device : " << chain[it]->get_Device_State_In_String() << "\n";
						cout << "Control mode = " << chain[it]->get_Control_Mode_In_String() << "\n";
					}

					current_value[it] = chain[it]->get_Actual_Position_In_Qc();

					if(is_scanning) {
						target_value[it] = current_value[it];
						is_scanning = false;
						// return 0;
					}

					if(PRINT_STATE) {
						cout << "Actual position : " << std::dec << current_value[it] << " qc" << "\n";
						cout << "\n";
					}
				}
			}
		} //end of valid workcounter

		high_resolution_clock::time_point time_point_end_loop = high_resolution_clock::now();

		ros::spinOnce();

		// Wait end of period
		this_thread::sleep_until(time_point_start_loop + chrono::milliseconds(period));
		duration<double> time_duration_loop = duration_cast<duration<double>>(time_point_end_loop - time_point_start_loop);
		// std::cout << "Time loop = " << time_duration_loop.count() << " seconds.\n";


		high_resolution_clock::time_point time_point_after_sleep_loop = high_resolution_clock::now();
		duration<double> time_duration_loop_after_sleep = duration_cast<duration<double>>(time_point_after_sleep_loop - time_point_start_loop);
		// std::cout << "Time loop after sleep = " << time_duration_loop_after_sleep.count() << " seconds.\n";

		if(PRINT_STATE) {
			cout << "\n" << endl;
		}
	}  

	cout << "End program" << endl;
	return 0;
}
