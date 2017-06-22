/*
 * Copyright (C) 2017 Canonical, Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 3.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * Authored by: Alberto Aguirre <alberto.aguirre@canonical.com>
 *
 */

#include "launch.h"

namespace mp = multipass;
namespace cmd = multipass::cmd;
using RpcMethod = mp::Rpc::Stub;

int cmd::Launch::run()
{
    auto on_success = [this](mp::LaunchReply& reply) {

        cout << "launched: " << reply.vm_instance_name();
        cout << std::endl;
        return EXIT_SUCCESS;
    };

    auto on_failure = [this](grpc::Status& status) {
        cerr << "failed to launch: " << status.error_message() << std::endl;
        return EXIT_FAILURE;
    };

    auto streaming_callback = [this](mp::LaunchReply& reply) {
        if (reply.launch_oneof_case() == 2)
        { 
            cout << "Downloaded " << reply.download_progress() << "%" << '\r' << std::flush;
        }
        else if (reply.launch_oneof_case() == 3)
        {
            cout << "\n" << reply.launch_complete() << std::endl;
        }
    };

    mp::LaunchRequest request;

    // Set some defaults
    request.set_mem_size(1024);

    return dispatch(&RpcMethod::launch, request, on_success, on_failure, streaming_callback);
}

std::string cmd::Launch::name() const { return "launch"; }
