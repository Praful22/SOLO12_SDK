#include "commander.hpp"
#include <atomic>
#include <thread>

using commander::Commander;
using commander::State;

int
main(int argc, char const *argv[])
{
#ifndef DRY_BUILD
	/** give the process a high priority */
	nice(-20);
#else
	rt_timer::set_process_priority();
#endif

	Commander com;

	std::atomic_bool is_running = true;
	std::atomic_bool is_changing_state = false;

	/* capture inputs in another thread */
	auto thread = std::thread([&] {
		std::chrono::high_resolution_clock::time_point now_time;
		auto input_time = std::chrono::high_resolution_clock::now();

		while (is_running) {
			now_time = std::chrono::high_resolution_clock::now();

			if (now_time >= input_time) {
				char in = std::getchar();
				if (in != 'q') {
					is_running = false;
				} else {
					is_changing_state = true;
				}
				input_time += std::chrono::nanoseconds(
				    static_cast<size_t>(std::nano::den * commander::input_period));
			}
		}
	});

	/* main loop */
	std::chrono::high_resolution_clock::time_point now_time;
	auto command_time = std::chrono::high_resolution_clock::now();
	auto print_time = std::chrono::high_resolution_clock::now();

	while (is_running) {
		now_time = std::chrono::high_resolution_clock::now();

		if (now_time >= command_time) {
			com.command();
			command_time += std::chrono::nanoseconds(
			    static_cast<size_t>(std::nano::den * commander::command_period));
		}

		if (now_time >= print_time) {
			printf("\33[H\33[2J"); //* clear screen
			com.print_all();
			print_time += std::chrono::nanoseconds(
			    static_cast<size_t>(std::nano::den * commander::print_period));
		}

		if (is_changing_state) {
			com.next_state();
		}
	}
	return 0;
}
