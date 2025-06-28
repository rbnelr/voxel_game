#include "random.hpp"
#include "timer.hpp"

// seed global rng with time
uint64_t get_initial_random_seed () {
	return std::hash<uint64_t>()(kiss::get_timestamp());
}
