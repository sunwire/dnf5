/*
Copyright Contributors to the libdnf project.

This file is part of libdnf: https://github.com/rpm-software-management/libdnf/

Libdnf is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 2 of the License, or
(at your option) any later version.

Libdnf is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with libdnf.  If not, see <https://www.gnu.org/licenses/>.
*/

#ifndef LIBDNF5_CLI_UTILS_USERCONFIRM_HPP
#define LIBDNF5_CLI_UTILS_USERCONFIRM_HPP

#include <libdnf5/conf/config_main.hpp>
#include <libdnf5/utils/format.hpp>

#include <iostream>
#include <string>

namespace libdnf5::cli::utils::userconfirm {

/// Asks the user for confirmation. The default answer is taken from the configuration.

template <class Config>
bool userconfirm(
    Config & config,
    std::string question,
    std::string lower_yes,
    std::string lower_no,
    std::string upper_yes,
    std::string upper_no) {
    // "assumeno" takes precedence over "assumeyes"
    if (config.get_assumeno_option().get_value()) {
        return false;
    }
    if (config.get_assumeyes_option().get_value()) {
        return true;
    }
    std::string msg;
    if (config.get_defaultyes_option().get_value()) {
        msg = fmt::format("{} [{}/{}]: ", question, upper_yes, lower_no);
    } else {
        msg = fmt::format("{} [{}/{}]: ", question, lower_yes, upper_no);
    }
    while (true) {
        std::cerr << msg;

        std::string choice;
        std::getline(std::cin, choice);

        if (choice.empty()) {
            return config.get_defaultyes_option().get_value();
        }
        if (choice == lower_yes || choice == upper_yes) {
            return true;
        }
        if (choice == lower_no || choice == upper_no) {
            return false;
        }
    }
}

};  // namespace libdnf5::cli::utils::userconfirm

#endif  // LIBDNF5_CLI_UTILS_USERCONFIRM_HPP
