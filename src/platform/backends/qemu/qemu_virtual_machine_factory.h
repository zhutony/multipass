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
#ifndef MULTIPASS_QEMU_VIRTUAL_MACHINE_FACTORY_H
#define MULTIPASS_QEMU_VIRTUAL_MACHINE_FACTORY_H

#include "dnsmasq_server.h"
#include "iptables_config.h"

#include <multipass/path.h>
#include <shared/base_virtual_machine_factory.h>

#include <QString>

#include <string>
#include <unordered_map>

namespace multipass
{
class QemuVirtualMachineFactory final : public BaseVirtualMachineFactory
{
public:
    explicit QemuVirtualMachineFactory(const Path& data_dir);
    ~QemuVirtualMachineFactory();

    VirtualMachine::UPtr create_virtual_machine(const VirtualMachineDescription& desc,
                                                VMStatusMonitor& monitor) override;
    void remove_resources_for(const std::string& name) override;
    VMImage prepare_source_image(const VMImage& source_image) override;
    void prepare_instance_image(const VMImage& instance_image, const VirtualMachineDescription& desc) override;
    void hypervisor_health_check() override;
    QString get_backend_version_string() override;
    std::vector<NetworkInterfaceInfo> list_networks() override;

private:
    const QString bridge_name;
    const Path network_dir;
    const std::string subnet;
    DNSMasqServer dnsmasq_server;
    IPTablesConfig iptables_config;
    std::unordered_map<std::string, std::string> name_to_mac_map;
};
} // namespace multipass

#endif // MULTIPASS_QEMU_VIRTUAL_MACHINE_FACTORY_H
