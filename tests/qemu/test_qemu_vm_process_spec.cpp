/*
 * Copyright (C) 2019-2020 Canonical, Ltd.
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

#include <src/platform/backends/qemu/qemu_vm_process_spec.h>

#include "tests/mock_environment_helpers.h"
#include <gmock/gmock.h>

#include <QTemporaryDir>

namespace mp = multipass;
namespace mpt = multipass::test;
using namespace testing;

struct TestQemuVMProcessSpec : public Test
{
    const mp::VirtualMachineDescription desc{2 /*cores*/,
                                             mp::MemorySize{"3G"} /*mem_size*/,
                                             mp::MemorySize{"4G"} /*disk_space*/,
                                             "vm_name",
                                             {{"default", "00:11:22:33:44:55"}},
                                             "ssh_username",
                                             {"/path/to/image", "", "", "", "", "", "", {}}, // VMImage
                                             mp::Path{"/path/to/cloud_init.iso"},
                                             {},
                                             {},
                                             {}};
    const QString tap_device_name{"tap_device"};
};

TEST_F(TestQemuVMProcessSpec, default_arguments_correct)
{
    mp::QemuVMProcessSpec spec(desc, tap_device_name, mp::nullopt);

    EXPECT_EQ(spec.arguments(), QStringList({"--enable-kvm",
                                             "-device",
                                             "virtio-scsi-pci,id=scsi0",
                                             "-drive",
                                             "file=/path/to/image,if=none,format=qcow2,discard=unmap,id=hda",
                                             "-device",
                                             "scsi-hd,drive=hda,bus=scsi0.0",
                                             "-smp",
                                             "2",
                                             "-m",
                                             "3072M",
                                             "-device",
                                             "virtio-net-pci,netdev=hostnet0,id=net0,mac=00:11:22:33:44:55",
                                             "-netdev",
                                             "tap,id=hostnet0,ifname=tap_device,script=no,downscript=no",
                                             "-qmp",
                                             "stdio",
                                             "-cpu",
                                             "host",
                                             "-chardev",
                                             "null,id=char0",
                                             "-serial",
                                             "chardev:char0",
                                             "-nographic",
                                             "-cdrom",
                                             "/path/to/cloud_init.iso"}));
}

TEST_F(TestQemuVMProcessSpec, legacy_resume_arguments_correct)
{
    const mp::QemuVMProcessSpec::ResumeData resume_data{"suspend_tag", "machine_type", false, {}};

    mp::QemuVMProcessSpec spec(desc, tap_device_name, resume_data);
    EXPECT_EQ(spec.arguments(), QStringList({"--enable-kvm",
                                             "-hda",
                                             "/path/to/image",
                                             "-smp",
                                             "2",
                                             "-m",
                                             "3072M",
                                             "-device",
                                             "virtio-net-pci,netdev=hostnet0,id=net0,mac=00:11:22:33:44:55",
                                             "-netdev",
                                             "tap,id=hostnet0,ifname=tap_device,script=no,downscript=no",
                                             "-qmp",
                                             "stdio",
                                             "-cpu",
                                             "host",
                                             "-chardev",
                                             "null,id=char0",
                                             "-serial",
                                             "chardev:char0",
                                             "-nographic",
                                             "-drive",
                                             "file=/path/to/cloud_init.iso,if=virtio,format=raw,snapshot=off,read-only",
                                             "-loadvm",
                                             "suspend_tag",
                                             "-machine",
                                             "machine_type"}));
}

TEST_F(TestQemuVMProcessSpec, legacy_use_cdrom_resume_arguments_correct)
{
    const mp::QemuVMProcessSpec::ResumeData resume_data{"suspend_tag", "machine_type", true, {}};

    mp::QemuVMProcessSpec spec(desc, tap_device_name, resume_data);

    EXPECT_EQ(spec.arguments(), QStringList({"--enable-kvm",
                                             "-hda",
                                             "/path/to/image",
                                             "-smp",
                                             "2",
                                             "-m",
                                             "3072M",
                                             "-device",
                                             "virtio-net-pci,netdev=hostnet0,id=net0,mac=00:11:22:33:44:55",
                                             "-netdev",
                                             "tap,id=hostnet0,ifname=tap_device,script=no,downscript=no",
                                             "-qmp",
                                             "stdio",
                                             "-cpu",
                                             "host",
                                             "-chardev",
                                             "null,id=char0",
                                             "-serial",
                                             "chardev:char0",
                                             "-nographic",
                                             "-cdrom",
                                             "/path/to/cloud_init.iso",
                                             "-loadvm",
                                             "suspend_tag",
                                             "-machine",
                                             "machine_type"}));
}

TEST_F(TestQemuVMProcessSpec, resume_arguments_taken_from_resumedata)
{
    const mp::QemuVMProcessSpec::ResumeData resume_data{"suspend_tag", "machine_type", false, {"-one", "-two"}};

    mp::QemuVMProcessSpec spec(desc, tap_device_name, resume_data);

    EXPECT_EQ(spec.arguments(), QStringList({"-one", "-two", "-loadvm", "suspend_tag", "-machine", "machine_type"}));
}

TEST_F(TestQemuVMProcessSpec, resume_with_missing_machine_type_guesses_correctly)
{
    mp::QemuVMProcessSpec::ResumeData resume_data_missing_machine_info;
    resume_data_missing_machine_info.suspend_tag = "suspend_tag";
    resume_data_missing_machine_info.arguments = QStringList{"-args"};

    mp::QemuVMProcessSpec spec(desc, tap_device_name, resume_data_missing_machine_info);

    EXPECT_EQ(spec.arguments(), QStringList({"-args", "-loadvm", "suspend_tag"}));
}

TEST_F(TestQemuVMProcessSpec, apparmor_profile_has_correct_name)
{
    mp::QemuVMProcessSpec spec(desc, tap_device_name, mp::nullopt);

    EXPECT_TRUE(spec.apparmor_profile().contains("profile multipass.vm_name.qemu-system-"));
}

TEST_F(TestQemuVMProcessSpec, apparmor_profile_includes_disk_images)
{
    mp::QemuVMProcessSpec spec(desc, tap_device_name, mp::nullopt);

    EXPECT_TRUE(spec.apparmor_profile().contains("/path/to/image rwk,"));
    EXPECT_TRUE(spec.apparmor_profile().contains("/path/to/cloud_init.iso rk,"));
}

TEST_F(TestQemuVMProcessSpec, apparmor_profile_identifier)
{
    mp::QemuVMProcessSpec spec(desc, tap_device_name, mp::nullopt);

    EXPECT_EQ(spec.identifier(), "vm_name");
}

TEST_F(TestQemuVMProcessSpec, apparmor_profile_running_as_snap_correct)
{
    const QByteArray snap_name{"multipass"};
    QTemporaryDir snap_dir;

    mpt::SetEnvScope e("SNAP", snap_dir.path().toUtf8());
    mpt::SetEnvScope e2("SNAP_NAME", snap_name);
    mp::QemuVMProcessSpec spec(desc, tap_device_name, mp::nullopt);

    EXPECT_TRUE(spec.apparmor_profile().contains("signal (receive) peer=snap.multipass.multipassd"));
    EXPECT_TRUE(spec.apparmor_profile().contains(QString("%1/qemu/* r,").arg(snap_dir.path())));
    EXPECT_TRUE(spec.apparmor_profile().contains(QString("%1/usr/bin/qemu-system-").arg(snap_dir.path())));
}

TEST_F(TestQemuVMProcessSpec, apparmor_profile_running_as_symlinked_snap_correct)
{
    const QByteArray snap_name{"multipass"};
    QTemporaryDir snap_dir, link_dir;

    link_dir.remove();
    QFile::link(snap_dir.path(), link_dir.path());

    mpt::SetEnvScope e("SNAP", link_dir.path().toUtf8());
    mpt::SetEnvScope e2("SNAP_NAME", snap_name);
    mp::QemuVMProcessSpec spec(desc, tap_device_name, mp::nullopt);

    EXPECT_TRUE(spec.apparmor_profile().contains(QString("%1/qemu/* r,").arg(snap_dir.path())));
    EXPECT_TRUE(spec.apparmor_profile().contains(QString("%1/usr/bin/qemu-system-").arg(snap_dir.path())));
}

TEST_F(TestQemuVMProcessSpec, apparmor_profile_not_running_as_snap_correct)
{
    const QByteArray snap_name{"multipass"};

    mpt::UnsetEnvScope e("SNAP");
    mpt::SetEnvScope e2("SNAP_NAME", snap_name);
    mp::QemuVMProcessSpec spec(desc, tap_device_name, mp::nullopt);

    EXPECT_TRUE(spec.apparmor_profile().contains("signal (receive) peer=unconfined"));
    EXPECT_TRUE(spec.apparmor_profile().contains("/usr/share/seabios/* r,"));
    EXPECT_TRUE(spec.apparmor_profile().contains(" /usr/bin/qemu-system-")); // space wanted
}
