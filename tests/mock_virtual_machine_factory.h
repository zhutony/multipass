/*
 * Copyright (C) 2017-2020 Canonical, Ltd.
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
 */

#ifndef MULTIPASS_MOCK_VIRTUAL_MACHINE_FACTORY_H
#define MULTIPASS_MOCK_VIRTUAL_MACHINE_FACTORY_H

#include <multipass/virtual_machine_description.h>
#include <multipass/virtual_machine_factory.h>
#include <multipass/vm_status_monitor.h>

#include <gmock/gmock.h>

namespace multipass
{
namespace test
{
struct MockVirtualMachineFactory : public VirtualMachineFactory
{
    MOCK_METHOD2(create_virtual_machine, VirtualMachine::UPtr(const VirtualMachineDescription&, VMStatusMonitor&));
    MOCK_METHOD1(remove_resources_for, void(const std::string&));

    MOCK_METHOD0(fetch_type, FetchType());
    MOCK_METHOD1(prepare_source_image, VMImage(const VMImage&));
    MOCK_METHOD2(prepare_instance_image, void(const VMImage&, const VirtualMachineDescription&));
    MOCK_METHOD0(hypervisor_health_check, void());
    MOCK_METHOD0(get_backend_directory_name, QString());
    MOCK_METHOD0(get_backend_version_string, QString());
    MOCK_METHOD5(create_image_vault,
                 VMImageVault::UPtr(std::vector<VMImageHost*>, URLDownloader*, const Path&, const Path&, const days&));
    MOCK_CONST_METHOD0(list_networks, std::vector<NetworkInterfaceInfo>());
    MOCK_CONST_METHOD5(make_cloud_init_image, Path(const QDir&, YAML::Node&, YAML::Node&, YAML::Node&, YAML::Node&));
};
}
}
#endif // MULTIPASS_MOCK_VIRTUAL_MACHINE_FACTORY_H
