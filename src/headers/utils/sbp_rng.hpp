#ifndef SBP_RNG_HPP
#define SBP_RNG_HPP

#include "sbp_aliases.hpp"

namespace sbp::utils {

struct RandomNumerGenerator {

    static RandomGenerator& get_generator() {
        static thread_local RandomGenerator generator = [](){
            std::random_device entropy_source;
            RandomGenerator random_generator;
            random_generator.seed(entropy_source());
            return random_generator;
        }();
        return generator;
    }
   
    static int random_int(int low, int high) {
        std::uniform_int_distribution<int> distibution(low, high);
        return distibution(get_generator());
    }

}; // RandomNumberGenerator

} // sbp::utils

#endif // SBP_RNG_HPP

