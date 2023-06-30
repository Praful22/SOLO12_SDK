
#include "commander/commander.hpp"
#include "commander/config.hpp"

#include <chrono>
#include <cstdlib>
#include <iostream>
#include <math.h>

namespace commander
{
Commander::Commander(const std::string ref_traj_fname, const std::string mb_hostname,
                     const double kp, const double kd)
    : ref_traj_fname(ref_traj_fname), mb(mb_hostname), kp(kp), kd(kd)
{
	initialize();
	initialize_mb();
}

Commander::~Commander()
{
}

void
Commander::initialize()
{
	ref_traj.clear();
	traj.clear();

	ref_traj.reserve(t_dim_expected);
	readmatrix(ref_traj_fprefix + ref_traj_fname, ref_traj);
	t_size = ref_traj.size(); /** determine t_dim */

	traj.reserve(t_size);
	t_index = 0;

}

void
Commander::initialize_mb()
{
	mb.Init();

	for (size_t i = 0; i < driver_count; ++i) {
		mb.motor_drivers[i].motor1->SetCurrentReference(0);
		mb.motor_drivers[i].motor2->SetCurrentReference(0);
		mb.motor_drivers[i].motor1->Enable();
		mb.motor_drivers[i].motor2->Enable();
		mb.motor_drivers[i].EnablePositionRolloverError();
		mb.motor_drivers[i].SetTimeout(masterboard_timeout);
		mb.motor_drivers[i].Enable();

		// compare to example_pd.cpp, we plan to use the on-board PD controller
		mb.motor_drivers[i].motor1->set_kp(kp);
		mb.motor_drivers[i].motor2->set_kp(kp);
		mb.motor_drivers[i].motor1->set_kd(kd);
		mb.motor_drivers[i].motor2->set_kd(kd);
		mb.motor_drivers[i].motor1->set_current_sat(max_current);
		mb.motor_drivers[i].motor2->set_current_sat(max_current);
	}

	const double send_init_delay = 1e-3;
	auto prev_time = std::chrono::high_resolution_clock::now();

	while (!mb.IsTimeout() && !mb.IsAckMsgReceived()) {
		if (((std::chrono::duration<double>)(std::chrono::high_resolution_clock::now() -
		                                     prev_time))
		        .count() > send_init_delay) {
			prev_time = std::chrono::high_resolution_clock::now();
			mb.SendInit();
		}
	}

	if (mb.IsTimeout()) {
		printf("Timeout while waiting for ack.\n");
	}

	// for (size_t i = 0; i < motor_count; i++){
	// 	mb.motors[i].SetPositionOffset(-index_offset[i]);
		
	// 	if (mb.motors[i].HasIndexBeenDetected()) {
	// 		was_index_detected[i] = true;
	// 		// mb.motors[i].set_enable_index_offset_compensation(true);
	// 	}
	// }
}

void
Commander::print_state()
{
	printf("Robot State | %.10s \n", state_to_name[state].c_str());
	if (sweep_done or state == State::sweep){
		printf("Sweeping Done | %.10s \n", (sweep_done) ? "True" : "False");
	}
	if (hard_calibrating){
		printf("Offset Values: {");
		for (size_t i = 0; i < motor_count; i++){
			if (i == motor_count - 1){
				printf("%g", index_pos[i]);
			} else {
				printf("%g, ", index_pos[i]);
			}	
		}
		printf("} \n");
	}
	printf("ERROR Motor |");
	for (size_t i = 0; i < motor_count; i++){
		printf(" [%1d] %1d |", i, !(mb.motor_drivers[i / 2].is_connected));
		
	}
	printf("\n");
	printf("ERROR Spi   |");
	for (size_t i = 0; i < motor_count; i++){
		printf(" [%1d] %1d |", i, mb.motor_drivers[i / 2].error_code == 0xf);
	}
	printf("\n");
	printf("ERROR Timeout | %1d \n", mb.IsTimeout());
}

void
Commander::print_offset()
{
	bool header_printed = false;

	for (size_t i = 0; i < motor_count; ++i) {
		if (!mb.motor_drivers[i / 2].is_connected) {
			continue;
		}

		if (!header_printed) {
			printf("Motor |  offset   |  idx pos  |   flag    | \n");
			header_printed = true;
		}

		printf("%5.2ld | ", i);
		printf("%9.3g | ", index_offset[i]);
		printf("%9.3g | ", index_pos[i]);
		printf("%9d | ", was_index_detected[i]);
		printf("\n");
	}
}

void
Commander::stats()
{
	/* records the max amp from motor */
	for (int i = 0; i < N_SLAVES; i++)
	{
		if (mb.motor_drivers[i].adc[0] > max_amp_stat || mb.motor_drivers[i].adc[1] > max_amp_stat)
		{
			max_amp_stat = (mb.motor_drivers[i].adc[0] > mb.motor_drivers[i].adc[1]) ? mb.motor_drivers[i].adc[0] : mb.motor_drivers[i].adc[1];
		}
	}
	if (max_command_exc_stat < command_time_dur.count()){
		max_command_exc_stat  = command_time_dur.count();
	} 
	if (max_print_exc_stat < print_time_dur.count()){
		max_print_exc_stat = print_time_dur.count();
	}
}

void
Commander::print_stats()
{
	printf("Command time | [cur] %5.2f ms | [max] %5.2f ms \n", command_time_dur.count(), max_command_exc_stat);
	printf("Print time   | [cur] %5.2f ms | [max] %5.2f ms \n", print_time_dur.count(), max_print_exc_stat);
	printf("Max Amp      |   %5.2f        | \n", max_amp_stat);
}

void
Commander::print_traj()
{
	bool header_printed = false;

	for (size_t i = 0; i < motor_count; ++i) {
		if (!mb.motor_drivers[i / 2].is_connected) {
			continue;
		}

		if (!header_printed) {
			printf("Motor | Ref pos   | pos       | Ref vel   | vel       |\n");
			header_printed = true;
		}

		printf("%5.2d | ", i);
		printf("%9.3g | ", pos_ref[i]);
		printf("%9.3g | ", pos[i]);
		printf("%9.3g | ", vel_ref[i]);
		printf("%9.3g | ", vel[i]);
		printf("\n");
	}
}

void
Commander::print_all()
{
	print_state();
	// print_offset();
	print_traj();
	// mb.PrintIMU();
	// mb.PrintADC();
	mb.PrintMotors();
	mb.PrintMotorDrivers();
	mb.PrintStats();
	print_stats();
}

void
Commander::log_traj()
{
	writematrix(fprefix + traj_fname, traj);
}

void
Commander::reset()
{
	initialize();
	initialize_mb();
}

bool
Commander::check_ready()
{
	mb.ParseSensorData();

	bool is_ready = true;

	for (size_t i = 0; i < motor_count; ++i) {
		if (!mb.motor_drivers[i / 2].is_connected) {
			// ignore the motors of a disconnected slave
			continue;
		}

		if (!(mb.motors[i].IsEnabled() && mb.motors[i].IsReady())) {
			is_ready = false;
		}
		init_pos[i] = mb.motors[i].GetPosition(); // initial position

		// not sure if this is needed?
		mb.motors[i].SetCurrentReference(0.);
		mb.motors[i].SetPositionReference(0.);
		mb.motors[i].SetVelocityReference(0.);
	}
	mb.SendCommand();

	return is_ready;
}

void
Commander::track(double (&pos_ref)[motor_count], double (&vel_ref)[motor_count])
{
	mb.ParseSensorData();

	for (size_t i = 0; i < motor_count; ++i) {
		if (i % 2 == 0) {
			if (!mb.motor_drivers[i / 2].is_connected) {
				continue;
			}

			if (mb.motor_drivers[i / 2].error_code == 0xf) {
				continue;
			}
		}

		if (mb.motors[i].IsEnabled()) {
			pos[i] = mb.motors[i].GetPosition();
			vel[i] = mb.motors[i].GetVelocity();

			mb.motors[i].SetPositionReference(pos_ref[i]);
			mb.motors[i].SetVelocityReference(vel_ref[i]);
		}
	}

	mb.SendCommand();
}

void
Commander::track_traj()
{
	for (size_t i = 0; i < motor_count; ++i) {
		pos_ref[i] = gear_ratio[motor2ref_idx[i]] * ref_traj[t_index][motor2ref_idx[i]];
		vel_ref[i] = gear_ratio[motor2ref_idx[i]] *
		    ref_traj[t_index][velocity_shift + motor2ref_idx[i]];
	}

	track(pos_ref, vel_ref);

	if (t_index < t_size - 1) {
		sample_traj();
		++t_index;
	}
}

void
Commander::sweep_traj()
{

	constexpr size_t t_sweep_size = static_cast<size_t>(1. / idx_sweep_freq * command_freq);
	bool all_ready = true;

	for (size_t i = 0; i < motor_count; i++) {
		
		if (was_index_detected[i]){
			continue;
		}

		all_ready = false;

		if (mb.motors[i].HasIndexBeenDetected()) {
			was_index_detected[i] = true;
			index_pos[i] = mb.motors[i].GetPosition();
			continue;
		}
		const double t =
		    static_cast<double>(t_sweep_index) / static_cast<double>(t_sweep_size);
		pos_ref[i] = gear_ratio[motor2ref_idx[i]] * (idx_sweep_ampl - idx_sweep_ampl * cos(2. * M_PI * t)) * sgn(gear_ratio[i]);
		vel_ref[i] = gear_ratio[motor2ref_idx[i]] * (-2. * M_PI * idx_sweep_ampl * sin(2. * M_PI * t)) * sgn(gear_ratio[i]);
		track(pos_ref, vel_ref);
	}
	++t_sweep_index;	
	if (all_ready) {

		is_ready = true;
		sweep_done = true;

		for (size_t i = 0; i < motor_count; i++){
			if (hard_calibrating){
				mb.motors[i].set_enable_index_offset_compensation(true);
			} else {
				mb.motors[i].SetPositionOffset(index_pos[i] - index_offset[i]);
				mb.motors[i].set_enable_index_offset_compensation(true);
			}
		}

		if (hard_calibrating){
			for (size_t i = 0; i < motor_count; i++) {
				pos_ref[i] = -index_pos[i];
				vel_ref[i] = 0.0;
			}
		}  else {
			for (size_t i = 0; i < motor_count; i++) {
				pos_ref[i] = 0.0;
				vel_ref[i] = 0.0;
			}
		}
		track(pos_ref, vel_ref);

	}

}

void
Commander::sample_traj()
{
	Row<traj_dim> state;

	for (size_t i = 0; i < motor_count; ++i) {

		state[ref2motor_idx[i]] = pos[i];
		state[ref2motor_idx[velocity_shift + i]] = vel[i];
	}

	traj.push_back(state);
}

void
Commander::command()
{
	if (!is_ready) {
		is_ready = check_ready();
		return;
	}

	switch (state) {
	case State::hold: {
		/* this does not work the second time? */
		for (size_t i = 0; i < motor_count; ++i) {
			pos_ref[i] =
			     gear_ratio[motor2ref_idx[i]] * ref_traj[0][motor2ref_idx[i]];
			vel_ref[i] = 0.;
		}
		track(pos_ref, vel_ref);
		break;
	}
	case State::sweep: {
		sweep_traj();
		break;
	}
	case State::track: {
		track_traj();
		break;
	}
	}
}

void
Commander::next_state()
{
	switch (state) {
	case State::hold: {
		reset();
		state = State::track;
		break;
	}
	case State::sweep: {
		state = State::hold;
		break;
	}
	case State::track: {
		reset();
		state = State::hold;
		break;
	}
	}
}

void
Commander::set_offset(double (&index_offset)[motor_count])
{
	for (size_t i = 0; i < motor_count; ++i) {
		mb.motors[i].SetPositionOffset(index_offset[i]);
		mb.motors[i].set_enable_index_offset_compensation(true);
	}
}

} // namespace commander
