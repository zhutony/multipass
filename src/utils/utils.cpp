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

#include <multipass/constants.h>
#include <multipass/exceptions/autostart_setup_exception.h>
#include <multipass/exceptions/exitless_sshprocess_exception.h>
#include <multipass/exceptions/sshfs_missing_error.h>
#include <multipass/format.h>
#include <multipass/logging/log.h>
#include <multipass/settings.h>
#include <multipass/ssh/ssh_session.h>
#include <multipass/standard_paths.h>
#include <multipass/utils.h>

#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QProcess>
#include <QRegularExpression>
#include <QUuid>
#include <QtGlobal>

#include <algorithm>
#include <array>
#include <cassert>
#include <cctype>
#include <fstream>
#include <random>
#include <regex>
#include <sstream>

namespace mp = multipass;
namespace mpl = multipass::logging;

namespace
{
constexpr auto category = "utils";

auto quote_for(const std::string& arg, mp::utils::QuoteType quote_type)
{
    if (quote_type == mp::utils::QuoteType::no_quotes)
        return "";
    return arg.find('\'') == std::string::npos ? "'" : "\"";
}

QString find_autostart_target(const QString& subdir, const QString& autostart_filename)
{
    const auto target_subpath = QDir{subdir}.filePath(autostart_filename);
    const auto target_path = MP_STDPATHS.locate(mp::StandardPaths::GenericDataLocation, target_subpath);

    if (target_path.isEmpty())
    {
        QString detail{};
        for (const auto& path : MP_STDPATHS.standardLocations(mp::StandardPaths::GenericDataLocation))
            detail += QStringLiteral("\n  ") + path + "/" + target_subpath;

        throw mp::AutostartSetupException{fmt::format("could not locate the autostart file '{}'", autostart_filename),
                                          fmt::format("Tried: {}", detail.toStdString())};
    }

    return target_path;
}
} // namespace

QDir mp::utils::base_dir(const QString& path)
{
    QFileInfo info{path};
    return info.absoluteDir();
}

bool mp::utils::valid_hostname(const std::string& name_string)
{
    QRegExp matcher("^([a-zA-Z]|[a-zA-Z][a-zA-Z0-9\\-]*[a-zA-Z0-9])");

    return matcher.exactMatch(QString::fromStdString(name_string));
}

bool mp::utils::invalid_target_path(const QString& target_path)
{
    QString sanitized_path{QDir::cleanPath(target_path)};
    QRegExp matcher("/+|/+(dev|proc|sys)(/.*)*|/+home(/*)(/ubuntu/*)*");

    return matcher.exactMatch(sanitized_path);
}

std::string mp::utils::to_cmd(const std::vector<std::string>& args, QuoteType quote_type)
{
    fmt::memory_buffer buf;
    for (auto const& arg : args)
    {
        fmt::format_to(buf, "{0}{1}{0} ", quote_for(arg, quote_type), arg);
    }

    // Remove the last space inserted
    auto cmd = fmt::to_string(buf);
    cmd.pop_back();
    return cmd;
}

bool mp::utils::run_cmd_for_status(const QString& cmd, const QStringList& args, const int timeout)
{
    QProcess proc;
    proc.setProgram(cmd);
    proc.setArguments(args);

    proc.start();
    proc.waitForFinished(timeout);

    return proc.exitStatus() == QProcess::NormalExit && proc.exitCode() == 0;
}

std::string mp::utils::run_cmd_for_output(const QString& cmd, const QStringList& args, const int timeout)
{
    QProcess proc;
    proc.setProgram(cmd);
    proc.setArguments(args);

    proc.start();
    proc.waitForFinished(timeout);

    return proc.readAllStandardOutput().trimmed().toStdString();
}

std::string& mp::utils::trim_end(std::string& s)
{
    auto rev_it = std::find_if(s.rbegin(), s.rend(), [](char ch) { return !std::isspace(ch); });
    s.erase(rev_it.base(), s.end());
    return s;
}

std::string& mp::utils::trim_newline(std::string& s)
{
    assert(!s.empty() && '\n' == s.back());
    s.pop_back();
    return s;
}

std::string mp::utils::escape_char(const std::string& in, char c)
{
    return std::regex_replace(in, std::regex({c}), fmt::format("\\{}", c));
}

// Escape all characters which need to be escaped in the shell.
std::string mp::utils::escape_for_shell(const std::string& in)
{
    std::string ret;
    std::back_insert_iterator<std::string> ret_insert = std::back_inserter(ret);

    for (char c : in)
    {
        // If the character is in one of these code ranges, then it must be escaped.
        if (c < 0x25 || c > 0x7a || (c > 0x25 && c < 0x2b) || (c > 0x5a && c < 0x5f) || 0x2c == c || 0x3b == c ||
            0x3c == c || 0x3e == c || 0x3f == c || 0x60 == c)
        {
            *ret_insert++ = '\\';
        }
        *ret_insert++ = c;
    }

    return ret;
}

std::vector<std::string> mp::utils::split(const std::string& string, const std::string& delimiter)
{
    std::regex regex(delimiter);
    return {std::sregex_token_iterator{string.begin(), string.end(), regex, -1}, std::sregex_token_iterator{}};
}

std::string mp::utils::generate_mac_address()
{
    std::default_random_engine gen;
    std::uniform_int_distribution<int> dist{0, 255};

    gen.seed(std::chrono::system_clock::now().time_since_epoch().count());
    std::array<int, 3> octets{{dist(gen), dist(gen), dist(gen)}};
    return fmt::format("52:54:00:{:02x}:{:02x}:{:02x}", octets[0], octets[1], octets[2]);
}

bool mp::utils::valid_mac_address(const std::string& mac)
{
    // A MAC address is a string consisting of six pairs of hyphen-separated hexadecimal digits.
    QStringList separated_mac = QString::fromStdString(mac).toLower().split(':', QString::KeepEmptyParts);

    if (separated_mac.size() == 6)
    {
        for (auto mac_group : separated_mac)
        {
            if (mac_group.size() != 2)
                return false;

            for (auto c : mac_group)
                if ('0' > c || ('9' < c && 'a' > c) || 'f' < c)
                    return false;
        }

        return true;
    }

    return false;
}

void mp::utils::wait_until_ssh_up(VirtualMachine* virtual_machine, std::chrono::milliseconds timeout,
                                  std::function<void()> const& ensure_vm_is_running)
{
    mpl::log(mpl::Level::debug, virtual_machine->vm_name,
             fmt::format("Trying SSH on {}:{}", virtual_machine->ssh_hostname(), virtual_machine->ssh_port()));
    auto action = [virtual_machine, &ensure_vm_is_running] {
        ensure_vm_is_running();
        try
        {
            mp::SSHSession session{virtual_machine->ssh_hostname(), virtual_machine->ssh_port()};

            std::lock_guard<decltype(virtual_machine->state_mutex)> lock{virtual_machine->state_mutex};
            virtual_machine->state = VirtualMachine::State::running;
            virtual_machine->update_state();
            return mp::utils::TimeoutAction::done;
        }
        catch (const std::exception&)
        {
            return mp::utils::TimeoutAction::retry;
        }
    };
    auto on_timeout = [virtual_machine] {
        std::lock_guard<decltype(virtual_machine->state_mutex)> lock{virtual_machine->state_mutex};
        virtual_machine->state = VirtualMachine::State::unknown;
        virtual_machine->update_state();
        throw std::runtime_error(fmt::format("{}: timed out waiting for response", virtual_machine->vm_name));
    };

    mp::utils::try_action_for(on_timeout, timeout, action);
}

void mp::utils::wait_for_cloud_init(mp::VirtualMachine* virtual_machine, std::chrono::milliseconds timeout,
                                    const mp::SSHKeyProvider& key_provider)
{
    auto action = [virtual_machine, &key_provider] {
        virtual_machine->ensure_vm_is_running();
        try
        {
            mp::SSHSession session{virtual_machine->ssh_hostname(), virtual_machine->ssh_port(),
                                   virtual_machine->ssh_username(), key_provider};

            std::lock_guard<decltype(virtual_machine->state_mutex)> lock{virtual_machine->state_mutex};
            auto ssh_process = session.exec({"[ -e /var/lib/cloud/instance/boot-finished ]"});
            return ssh_process.exit_code() == 0 ? mp::utils::TimeoutAction::done : mp::utils::TimeoutAction::retry;
        }
        catch (const std::exception& e)
        {
            std::lock_guard<decltype(virtual_machine->state_mutex)> lock{virtual_machine->state_mutex};
            mpl::log(mpl::Level::warning, virtual_machine->vm_name, e.what());
            return mp::utils::TimeoutAction::retry;
        }
    };
    auto on_timeout = [] { throw std::runtime_error("timed out waiting for initialization to complete"); };
    mp::utils::try_action_for(on_timeout, timeout, action);
}

void mp::utils::install_sshfs_for(const std::string& name, mp::SSHSession& session,
                                  const std::chrono::milliseconds timeout)
{
    mpl::log(mpl::Level::info, category, fmt::format("Installing the multipass-sshfs snap in \'{}\'", name));

    // Check if snap support is installed in the instance
    auto which_proc = session.exec("which snap");
    if (which_proc.exit_code() != 0)
    {
        mpl::log(mpl::Level::warning, category, fmt::format("Snap support is not installed in \'{}\'", name));
        throw std::runtime_error(
            fmt::format("Snap support needs to be installed in \'{}\' in order to support mounts.\n"
                        "Please see https://docs.snapcraft.io/installing-snapd for information on\n"
                        "how to install snap support for your instance's distribution.\n\n"
                        "If your distribution's instructions specify enabling classic snap support,\n"
                        "please do that as well.\n\n"
                        "Alternatively, install `sshfs` manually inside the instance.",
                        name));
    }

    // Check if /snap exists for "classic" snap support
    auto test_file_proc = session.exec("[ -e /snap ]");
    if (test_file_proc.exit_code() != 0)
    {
        mpl::log(mpl::Level::warning, category, fmt::format("Classic snap support symlink is needed in \'{}\'", name));
        throw std::runtime_error(
            fmt::format("Classic snap support is not enabled for \'{}\'!\n\n"
                        "Please see https://docs.snapcraft.io/installing-snapd for information on\n"
                        "how to enable classic snap support for your instance's distribution.",
                        name));
    }

    try
    {
        auto proc = session.exec("sudo snap install multipass-sshfs");
        if (proc.exit_code(timeout) != 0)
        {
            auto error_msg = proc.read_std_error();
            mpl::log(mpl::Level::warning, category,
                     fmt::format("Failed to install \'multipass-sshfs\', error message: \'{}\'",
                                 mp::utils::trim_end(error_msg)));
            throw mp::SSHFSMissingError();
        }
    }
    catch (const mp::ExitlessSSHProcessException&)
    {
        mpl::log(mpl::Level::info, category, fmt::format("Timeout while installing 'sshfs' in '{}'", name));
    }
}

void mp::utils::link_autostart_file(const QDir& link_dir, const QString& autostart_subdir,
                                    const QString& autostart_filename)
{
    const auto link_path = link_dir.absoluteFilePath(autostart_filename);
    const auto target_path = find_autostart_target(autostart_subdir, autostart_filename);

    const auto link_info = QFileInfo{link_path};
    const auto target_info = QFileInfo{target_path};
    auto target_file = QFile{target_path};
    auto link_file = QFile{link_path};

    if (link_info.isSymLink() && link_info.symLinkTarget() != target_info.absoluteFilePath())
        link_file.remove(); // get rid of outdated and broken links

    if (!link_file.exists())
    {
        link_dir.mkpath(".");
        if (!target_file.link(link_path))

            throw mp::AutostartSetupException{fmt::format("failed to link file '{}' to '{}'", link_path, target_path),
                                              fmt::format("Detail: {} (error code {})", strerror(errno), errno)};
    }
}

mp::Path mp::utils::make_dir(const QDir& a_dir, const QString& name)
{
    mp::Path dir_path;
    bool success{false};

    if (name.isEmpty())
    {
        success = a_dir.mkpath(".");
        dir_path = a_dir.absolutePath();
    }
    else
    {
        success = a_dir.mkpath(name);
        dir_path = a_dir.filePath(name);
    }

    if (!success)
    {
        throw std::runtime_error(fmt::format("unable to create directory '{}'", dir_path));
    }
    return dir_path;
}

QString mp::utils::backend_directory_path(const mp::Path& path, const QString& subdirectory)
{
    if (subdirectory.isEmpty())
        return path;

    return mp::Path("%1/%2").arg(path).arg(subdirectory);
}

QString mp::utils::get_driver_str()
{
    auto driver = qgetenv(mp::driver_env_var);
    if (!driver.isEmpty())
    {
        mpl::log(mpl::Level::warning, "platform",
                 fmt::format("{} is now ignored, please use `multipass set {}` instead.", mp::driver_env_var,
                             mp::driver_key));
    }
    return MP_SETTINGS.get(mp::driver_key);
}

QString mp::utils::make_uuid()
{
    auto uuid = QUuid::createUuid().toString();

    // Remove curly brackets enclosing uuid
    return uuid.mid(1, uuid.size() - 2);
}

std::string mp::utils::contents_of(const multipass::Path& file_path)
{
    const std::string name{file_path.toStdString()};
    std::ifstream in(name, std::ios::in | std::ios::binary);
    if (!in)
        throw std::runtime_error(fmt::format("failed to open file '{}': {}({})", name, strerror(errno), errno));

    std::stringstream stream;
    stream << in.rdbuf();
    return stream.str();
}

bool mp::utils::has_only_digits(const std::string& value)
{
    return std::all_of(value.begin(), value.end(), [](char c) { return std::isdigit(c); });
}

void mp::utils::validate_server_address(const std::string& address)
{
    if (address.empty())
        throw std::runtime_error("empty server address");

    const auto tokens = mp::utils::split(address, ":");
    const auto server_name = tokens[0];
    if (tokens.size() == 1u)
    {
        if (server_name == "unix")
            throw std::runtime_error(fmt::format("missing socket file in address '{}'", address));
        else
            throw std::runtime_error(fmt::format("missing port number in address '{}'", address));
    }

    const auto port = tokens[1];
    if (server_name != "unix" && !mp::utils::has_only_digits(port))
        throw std::runtime_error(fmt::format("invalid port number in address '{}'", address));
}

std::string mp::utils::filename_for(const std::string& path)
{
    return QFileInfo(QString::fromStdString(path)).fileName().toStdString();
}

bool mp::utils::is_dir(const std::string& path)
{
    return QFileInfo(QString::fromStdString(path)).isDir();
}

std::string mp::utils::timestamp()
{
    auto time = QDateTime::currentDateTime();
    return time.toString(Qt::ISODateWithMs).toStdString();
}

std::string mp::utils::match_line_for(const std::string& output, const std::string& matcher)
{
    std::istringstream ss{output};
    std::string line;

    while (std::getline(ss, line, '\n'))
    {
        if (line.find(matcher) != std::string::npos)
        {
            return line;
        }
    }

    return std::string{};
}

bool mp::utils::is_running(const VirtualMachine::State& state)
{
    return state == VirtualMachine::State::running || state == VirtualMachine::State::delayed_shutdown;
}

void mp::utils::check_and_create_config_file(const QString& config_file_path)
{
    QFile config_file{config_file_path};

    if (!config_file.exists())
    {
        make_dir({}, QFileInfo{config_file_path}.dir().path()); // make sure parent dir is there
        config_file.open(QIODevice::WriteOnly);
    }
}

void mp::utils::process_throw_on_error(const QString& program, const QStringList& arguments, const QString& message,
                                       const QString& category, const int timeout)
{
    QProcess process;
    mpl::log(mpl::Level::debug, category.toStdString(),
             fmt::format("Running: {}, {}", program.toStdString(), arguments.join(", ").toStdString()));
    process.setProcessChannelMode(QProcess::MergedChannels);
    process.start(program, arguments);
    auto success = process.waitForFinished(timeout);

    if (!success || process.exitStatus() != QProcess::NormalExit || process.exitCode() != 0)
    {
        mpl::log(mpl::Level::debug, category.toStdString(),
                 fmt::format("{} failed - errorString: {}, exitStatus: {}, exitCode: {}", program.toStdString(),
                             process.errorString().toStdString(), process.exitStatus(), process.exitCode()));

        auto output = process.readAllStandardOutput();
        throw std::runtime_error(fmt::format(
            message.toStdString(), output.isEmpty() ? process.errorString().toStdString() : output.toStdString()));
    }
}

bool mp::utils::process_log_on_error(const QString& program, const QStringList& arguments, const QString& message,
                                     const QString& category, mpl::Level level, const int timeout)
{
    QProcess process;
    mpl::log(mpl::Level::debug, category.toStdString(),
             fmt::format("Running: {}, {}", program.toStdString(), arguments.join(", ").toStdString()));
    process.setProcessChannelMode(QProcess::MergedChannels);
    process.start(program, arguments);
    auto success = process.waitForFinished(timeout);

    if (!success || process.exitStatus() != QProcess::NormalExit || process.exitCode() != 0)
    {
        mpl::log(mpl::Level::debug, category.toStdString(),
                 fmt::format("{} failed - errorString: {}, exitStatus: {}, exitCode: {}", program.toStdString(),
                             process.errorString().toStdString(), process.exitStatus(), process.exitCode()));

        auto output = process.readAllStandardOutput();
        mpl::log(level, category.toStdString(),
                 fmt::format(message.toStdString(),
                             output.isEmpty() ? process.errorString().toStdString() : output.toStdString()));
        return false;
    }

    return true;
}

std::string mp::utils::emit_yaml(const YAML::Node& node)
{
    YAML::Emitter emitter;
    emitter.SetIndent(2);
    emitter << node;
    if (!emitter.good())
        throw std::runtime_error{fmt::format("Failed to emit YAML: {}", emitter.GetLastError())};

    emitter << YAML::Newline;
    return emitter.c_str();
}

std::string mp::utils::emit_cloud_config(const YAML::Node& node)
{
    return fmt::format("#cloud-config\n{}\n", emit_yaml(node));
}
