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

#include <multipass/utils.h>

#include "file_operations.h"
#include "temp_dir.h"
#include "temp_file.h"

#include <QDateTime>
#include <QRegExp>

#include <gmock/gmock.h>
#include <gtest/gtest-death-test.h>
#include <gtest/gtest.h>

#include <sstream>
#include <string>

namespace mp = multipass;
namespace mpt = multipass::test;

using namespace testing;

TEST(Utils, hostname_begins_with_letter_is_valid)
{
    EXPECT_TRUE(mp::utils::valid_hostname("foo"));
}

TEST(Utils, hostname_single_letter_is_valid)
{
    EXPECT_TRUE(mp::utils::valid_hostname("f"));
}

TEST(Utils, hostname_contains_digit_is_valid)
{
    EXPECT_TRUE(mp::utils::valid_hostname("foo1"));
}

TEST(Utils, hostname_contains_hyphen_is_valid)
{
    EXPECT_TRUE(mp::utils::valid_hostname("foo-bar"));
}

TEST(Utils, hostname_begins_with_digit_is_invalid)
{
    EXPECT_FALSE(mp::utils::valid_hostname("1foo"));
}

TEST(Utils, hostname_single_digit_is_invalid)
{
    EXPECT_FALSE(mp::utils::valid_hostname("1"));
}

TEST(Utils, hostname_contains_underscore_is_invalid)
{
    EXPECT_FALSE(mp::utils::valid_hostname("foo_bar"));
}

TEST(Utils, hostname_contains_special_character_is_invalid)
{
    EXPECT_FALSE(mp::utils::valid_hostname("foo!"));
}

TEST(Utils, path_root_invalid)
{
    EXPECT_TRUE(mp::utils::invalid_target_path(QString("/")));
    EXPECT_TRUE(mp::utils::invalid_target_path(QString("//")));
}

TEST(Utils, path_root_foo_valid)
{
    EXPECT_FALSE(mp::utils::invalid_target_path(QString("/foo")));
    EXPECT_FALSE(mp::utils::invalid_target_path(QString("/foo/")));
    EXPECT_FALSE(mp::utils::invalid_target_path(QString("//foo")));
}

TEST(Utils, path_dev_invalid)
{
    EXPECT_TRUE(mp::utils::invalid_target_path(QString("/dev")));
    EXPECT_TRUE(mp::utils::invalid_target_path(QString("/dev/")));
    EXPECT_TRUE(mp::utils::invalid_target_path(QString("//dev/")));
    EXPECT_TRUE(mp::utils::invalid_target_path(QString("/dev//")));
    EXPECT_TRUE(mp::utils::invalid_target_path(QString("//dev//")));
    EXPECT_TRUE(mp::utils::invalid_target_path(QString("/dev/foo")));
}

TEST(Utils, path_devpath_valid)
{
    EXPECT_FALSE(mp::utils::invalid_target_path(QString("/devpath")));
    EXPECT_FALSE(mp::utils::invalid_target_path(QString("/devpath/")));
    EXPECT_FALSE(mp::utils::invalid_target_path(QString("/devpath/foo")));
}

TEST(Utils, path_proc_invalid)
{
    EXPECT_TRUE(mp::utils::invalid_target_path(QString("/proc")));
    EXPECT_TRUE(mp::utils::invalid_target_path(QString("/proc/")));
    EXPECT_TRUE(mp::utils::invalid_target_path(QString("//proc/")));
    EXPECT_TRUE(mp::utils::invalid_target_path(QString("/proc//")));
    EXPECT_TRUE(mp::utils::invalid_target_path(QString("//proc//")));
    EXPECT_TRUE(mp::utils::invalid_target_path(QString("/proc/foo")));
}

TEST(Utils, path_sys_invalid)
{
    EXPECT_TRUE(mp::utils::invalid_target_path(QString("/sys")));
    EXPECT_TRUE(mp::utils::invalid_target_path(QString("/sys/")));
    EXPECT_TRUE(mp::utils::invalid_target_path(QString("//sys/")));
    EXPECT_TRUE(mp::utils::invalid_target_path(QString("/sys//")));
    EXPECT_TRUE(mp::utils::invalid_target_path(QString("//sys//")));
    EXPECT_TRUE(mp::utils::invalid_target_path(QString("/sys/foo")));
}

TEST(Utils, path_home_proper_invalid)
{
    EXPECT_TRUE(mp::utils::invalid_target_path(QString("/home")));
    EXPECT_TRUE(mp::utils::invalid_target_path(QString("/home/")));
    EXPECT_TRUE(mp::utils::invalid_target_path(QString("//home/")));
    EXPECT_TRUE(mp::utils::invalid_target_path(QString("/home//")));
    EXPECT_TRUE(mp::utils::invalid_target_path(QString("//home//")));
    EXPECT_TRUE(mp::utils::invalid_target_path(QString("/home/foo/..")));
}

TEST(Utils, path_home_ubuntu_invalid)
{
    EXPECT_TRUE(mp::utils::invalid_target_path(QString("/home/ubuntu")));
    EXPECT_TRUE(mp::utils::invalid_target_path(QString("/home/ubuntu/")));
    EXPECT_TRUE(mp::utils::invalid_target_path(QString("//home/ubuntu/")));
    EXPECT_TRUE(mp::utils::invalid_target_path(QString("/home//ubuntu/")));
    EXPECT_TRUE(mp::utils::invalid_target_path(QString("/home/ubuntu//")));
    EXPECT_TRUE(mp::utils::invalid_target_path(QString("//home//ubuntu//")));
    EXPECT_TRUE(mp::utils::invalid_target_path(QString("/home/ubuntu/foo/..")));
}

TEST(Utils, path_home_foo_valid)
{
    EXPECT_FALSE(mp::utils::invalid_target_path(QString("/home/foo")));
    EXPECT_FALSE(mp::utils::invalid_target_path(QString("/home/foo/")));
    EXPECT_FALSE(mp::utils::invalid_target_path(QString("//home/foo/")));
}

TEST(Utils, path_home_ubuntu_foo_valid)
{
    EXPECT_FALSE(mp::utils::invalid_target_path(QString("/home/ubuntu/foo")));
    EXPECT_FALSE(mp::utils::invalid_target_path(QString("/home/ubuntu/foo/")));
    EXPECT_FALSE(mp::utils::invalid_target_path(QString("//home/ubuntu/foo")));
}

TEST(Utils, to_cmd_output_has_no_quotes)
{
    std::vector<std::string> args{"hello", "world"};
    auto output = mp::utils::to_cmd(args, mp::utils::QuoteType::no_quotes);
    EXPECT_THAT(output, ::testing::StrEq("hello world"));
}

TEST(Utils, to_cmd_arguments_are_single_quoted)
{
    std::vector<std::string> args{"hello", "world"};
    auto output = mp::utils::to_cmd(args, mp::utils::QuoteType::quote_every_arg);
    EXPECT_THAT(output, ::testing::StrEq("'hello' 'world'"));
}

TEST(Utils, to_cmd_arguments_are_double_quoted_when_needed)
{
    std::vector<std::string> args{"it's", "me"};
    auto output = mp::utils::to_cmd(args, mp::utils::QuoteType::quote_every_arg);
    EXPECT_THAT(output, ::testing::StrEq("\"it's\" 'me'"));
}

TEST(Utils, to_cmd_arguments_are_single_quoted_when_needed)
{
    std::vector<std::string> args{"they", "said", "\"please\""};
    auto output = mp::utils::to_cmd(args, mp::utils::QuoteType::quote_every_arg);
    EXPECT_THAT(output, ::testing::StrEq("'they' 'said' '\"please\"'"));
}

TEST(Utils, trim_end_actually_trims_end)
{
    std::string s{"I'm a great\n\t string \n \f \n \r \t   \v"};
    mp::utils::trim_end(s);

    EXPECT_THAT(s, ::testing::StrEq("I'm a great\n\t string"));
}

TEST(Utils, trim_newline_works)
{
    std::string s{"correct\n"};
    mp::utils::trim_newline(s);

    EXPECT_THAT(s, ::testing::StrEq("correct"));
}

TEST(Utils, trim_newline_assertion_works)
{
    std::string s{"wrong"};
    ASSERT_DEBUG_DEATH(mp::utils::trim_newline(s), "[Aa]ssert");
}

TEST(Utils, escape_char_actually_escapes)
{
    std::string s{"I've got \"quotes\""};
    auto res = mp::utils::escape_char(s, '"');
    EXPECT_THAT(res, ::testing::StrEq("I've got \\\"quotes\\\""));
}

TEST(Utils, escape_for_shell_actually_escapes)
{
    std::string s{"I've got \"quotes\""};
    auto res = mp::utils::escape_for_shell(s);
    EXPECT_THAT(res, ::testing::StrEq("I\\'ve\\ got\\ \\\"quotes\\\""));
}

TEST(Utils, try_action_actually_times_out)
{
    bool on_timeout_called{false};
    auto on_timeout = [&on_timeout_called] { on_timeout_called = true; };
    auto retry_action = [] { return mp::utils::TimeoutAction::retry; };
    mp::utils::try_action_for(on_timeout, std::chrono::milliseconds(1), retry_action);

    EXPECT_TRUE(on_timeout_called);
}

TEST(Utils, try_action_does_not_timeout)
{
    bool on_timeout_called{false};
    auto on_timeout = [&on_timeout_called] { on_timeout_called = true; };

    bool action_called{false};
    auto successful_action = [&action_called] {
        action_called = true;
        return mp::utils::TimeoutAction::done;
    };
    mp::utils::try_action_for(on_timeout, std::chrono::seconds(1), successful_action);

    EXPECT_FALSE(on_timeout_called);
    EXPECT_TRUE(action_called);
}

TEST(Utils, uuid_has_no_curly_brackets)
{
    auto uuid = mp::utils::make_uuid();
    EXPECT_FALSE(uuid.contains(QRegExp("[{}]")));
}

TEST(Utils, contents_of_actually_reads_contents)
{
    mpt::TempDir temp_dir;
    auto file_name = temp_dir.path() + "/test-file";
    std::string expected_content{"just a bit of test content here"};
    mpt::make_file_with_content(file_name, expected_content);

    auto content = mp::utils::contents_of(file_name);
    EXPECT_THAT(content, StrEq(expected_content));
}

TEST(Utils, contents_of_throws_on_missing_file)
{
    EXPECT_THROW(mp::utils::contents_of("this-file-does-not-exist"), std::runtime_error);
}

TEST(Utils, contents_of_empty_contents_on_empty_file)
{
    mpt::TempDir temp_dir;
    auto file_name = temp_dir.path() + "/empty_test_file";
    mpt::make_file_with_content(file_name, "");

    auto content = mp::utils::contents_of(file_name);
    EXPECT_TRUE(content.empty());
}

TEST(Utils, split_returns_token_list)
{
    std::vector<std::string> expected_tokens;
    expected_tokens.push_back("Hello");
    expected_tokens.push_back("World");
    expected_tokens.push_back("Bye!");

    const std::string delimiter{":"};

    std::stringstream content;
    for (const auto& token : expected_tokens)
    {
        content << token;
        content << delimiter;
    }

    const auto tokens = mp::utils::split(content.str(), delimiter);
    EXPECT_THAT(tokens, ContainerEq(expected_tokens));
}

TEST(Utils, split_returns_one_token_if_no_delimiter)
{
    const std::string content{"no delimiter here"};
    const std::string delimiter{":"};

    const auto tokens = mp::utils::split(content, delimiter);
    ASSERT_THAT(tokens.size(), Eq(1u));

    EXPECT_THAT(tokens[0], StrEq(content));
}

TEST(Utils, valid_mac_address_works)
{
    EXPECT_TRUE(mp::utils::valid_mac_address("00:11:22:33:44:55"));
    EXPECT_TRUE(mp::utils::valid_mac_address("aa:bb:cc:dd:ee:ff"));
    EXPECT_TRUE(mp::utils::valid_mac_address("AA:BB:CC:DD:EE:FF"));
    EXPECT_TRUE(mp::utils::valid_mac_address("52:54:00:dd:ee:ff"));
    EXPECT_TRUE(mp::utils::valid_mac_address("52:54:00:AB:CD:EF"));
    EXPECT_FALSE(mp::utils::valid_mac_address("01:23:45:67:89:AG"));
    EXPECT_FALSE(mp::utils::valid_mac_address("012345678901"));
    EXPECT_FALSE(mp::utils::valid_mac_address("1:23:45:65:89:ab"));
}

TEST(Utils, has_only_digits_works)
{
    EXPECT_FALSE(mp::utils::has_only_digits("124ft:,"));
    EXPECT_TRUE(mp::utils::has_only_digits("0123456789"));
    EXPECT_FALSE(mp::utils::has_only_digits("0123456789:'`'"));
}

TEST(Utils, validate_server_address_throws_on_invalid_address)
{
    EXPECT_THROW(mp::utils::validate_server_address("unix"), std::runtime_error);
    EXPECT_THROW(mp::utils::validate_server_address("unix:"), std::runtime_error);
    EXPECT_THROW(mp::utils::validate_server_address("test:test"), std::runtime_error);
    EXPECT_THROW(mp::utils::validate_server_address(""), std::runtime_error);
}

TEST(Utils, validate_server_address_does_not_throw_on_good_address)
{
    EXPECT_NO_THROW(mp::utils::validate_server_address("unix:/tmp/a_socket"));
    EXPECT_NO_THROW(mp::utils::validate_server_address("test-server.net:123"));
}

TEST(Utils, dir_is_a_dir)
{
    mpt::TempDir temp_dir;
    EXPECT_TRUE(mp::utils::is_dir(temp_dir.path().toStdString()));
}

TEST(Utils, file_is_not_a_dir)
{
    mpt::TempDir temp_dir;
    auto file_name = temp_dir.path() + "/empty_test_file";
    mpt::make_file_with_content(file_name, "");

    EXPECT_FALSE(mp::utils::is_dir(file_name.toStdString()));
}

TEST(Utils, filename_only_is_returned)
{
    std::string file_name{"my_file"};
    std::string full_path{"/tmp/foo/" + file_name};

    EXPECT_THAT(mp::utils::filename_for(full_path), Eq(file_name));
}

TEST(Utils, no_subdirectory_returns_same_path)
{
    mp::Path original_path{"/tmp/foo"};
    QString empty_subdir{};

    EXPECT_THAT(mp::utils::backend_directory_path(original_path, empty_subdir), Eq(original_path));
}

TEST(Utils, subdirectory_returns_new_path)
{
    mp::Path original_path{"/tmp/foo"};
    QString subdir{"bar"};

    EXPECT_THAT(mp::utils::backend_directory_path(original_path, subdir), Eq(mp::Path{"/tmp/foo/bar"}));
}

TEST(Utils, vm_running_returns_true)
{
    mp::VirtualMachine::State state = mp::VirtualMachine::State::running;

    EXPECT_TRUE(mp::utils::is_running(state));
}

TEST(Utils, vm_delayed_shutdown_returns_true)
{
    mp::VirtualMachine::State state = mp::VirtualMachine::State::delayed_shutdown;

    EXPECT_TRUE(mp::utils::is_running(state));
}

TEST(Utils, vm_stopped_returns_false)
{
    mp::VirtualMachine::State state = mp::VirtualMachine::State::stopped;

    EXPECT_FALSE(mp::utils::is_running(state));
}

TEST(Utils, absent_config_file_and_dir_are_created)
{
    mpt::TempDir temp_dir;
    const QString config_file_path{QString("%1/config_dir/config").arg(temp_dir.path())};

    mp::utils::check_and_create_config_file(config_file_path);

    EXPECT_TRUE(QFile::exists(config_file_path));
}

TEST(Utils, existing_config_file_is_untouched)
{
    mpt::TempFile config_file;
    QFileInfo config_file_info{config_file.name()};

    auto original_last_modified = config_file_info.lastModified();

    mp::utils::check_and_create_config_file(config_file.name());

    auto new_last_modified = config_file_info.lastModified();

    EXPECT_THAT(new_last_modified, Eq(original_last_modified));
}

TEST(Utils, line_matcher_returns_expected_line)
{
    std::string data{"LD_LIBRARY_PATH=/foo/lib\nSNAP=/foo/bin\nDATA=/bar/baz\n"};
    std::string matcher{"SNAP="};

    auto snap_data = mp::utils::match_line_for(data, matcher);

    EXPECT_THAT(snap_data, Eq("SNAP=/foo/bin"));
}

TEST(Utils, line_matcher_no_match_returns_empty_string)
{
    std::string data{"LD_LIBRARY_PATH=/foo/lib\nSNAP=/foo/bin\nDATA=/bar/baz\n"};
    std::string matcher{"FOO="};

    auto snap_data = mp::utils::match_line_for(data, matcher);

    EXPECT_TRUE(snap_data.empty());
}

TEST(Utils, make_dir_creates_correct_dir)
{
    mpt::TempDir temp_dir;
    QString new_dir{"foo"};

    auto new_path = mp::utils::make_dir(QDir(temp_dir.path()), new_dir);

    EXPECT_TRUE(QFile::exists(new_path));
    EXPECT_EQ(new_path, temp_dir.path() + "/" + new_dir);
}

TEST(Utils, make_dir_with_no_new_dir)
{
    mpt::TempDir temp_dir;

    auto new_path = mp::utils::make_dir(QDir(temp_dir.path()), "");

    EXPECT_TRUE(QFile::exists(new_path));
    EXPECT_EQ(new_path, temp_dir.path());
}
