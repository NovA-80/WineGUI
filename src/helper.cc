/**
 * Copyright (c) 2019-2022 WineGUI
 *
 * \file    helper.cc
 * \brief   Provide some helper methods for Bottle Manager and CLI interaction
 * \author  Melroy van den Berg <webmaster1989@gmail.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as
 * published by the Free Software Foundation, either version 3 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Affero General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
#include "helper.h"
#include "wine_defaults.h"
#include <algorithm>
#include <array>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <fcntl.h>
#include <fstream>
#include <giomm/file.h>
#include <glibmm/fileutils.h>
#include <glibmm/miscutils.h>
#include <glibmm/timeval.h>
#include <iomanip>
#include <iostream>
#include <memory>
#include <pwd.h>
#include <regex>
#include <stdexcept>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

std::vector<std::string> wineGuiDirs{Glib::get_home_dir(), ".winegui"}; /*!< WineGui config/storage directory path */
static string WineGuiDir = Glib::build_path(G_DIR_SEPARATOR_S, wineGuiDirs);

std::vector<std::string> defaultWineDir{Glib::get_home_dir(), ".wine"}; /*!< Default Wine bottle location */
static string DefaultBottleWineDir = Glib::build_path(G_DIR_SEPARATOR_S, defaultWineDir);

// Wine & Winetricks exec
static const string WineExecutable = "wine";     /*!< Currently expect to be installed globally */
static const string WineExecutable64 = "wine64"; /*!< Currently expect to be installed globally */
static const string WinetricksExecutable =
    Glib::build_filename(WineGuiDir, "winetricks"); /*!< winetricks shall be located within the .winegui folder */

// Reg files
static const string SystemReg = "system.reg";
static const string UserReg = "user.reg";
static const string UserdefReg = "userdef.reg";

// Reg keys
static const string RegKeyName9x = "[Software\\\\Microsoft\\\\Windows\\\\CurrentVersion]";
static const string RegKeyNameNT = "[Software\\\\Microsoft\\\\Windows NT\\\\CurrentVersion]";
static const string RegKeyType = "[System\\\\CurrentControlSet\\\\Control\\\\ProductOptions]";
static const string RegKeyWine = "[Software\\\\Wine]";
static const string RegKeyAudio = "[Software\\\\Wine\\\\Drivers]";
static const string RegKeyVirtualDesktop = "[Software\\\\Wine\\\\Explorer]";
static const string RegKeyVirtualDesktopResolution = "[Software\\\\Wine\\\\Explorer\\\\Desktops]";
static const string RegKeyDllOverrides = "[Software\\\\Wine\\\\DllOverrides]";
static const string RegKeyMenuFiles = "[Software\\\\Wine\\\\MenuFiles]";

// Reg names
static const string RegNameNTVersion = "CurrentVersion";
static const string RegNameNTBuild = "CurrentBuild";
static const string RegNameNTBuildNumber = "CurrentBuildNumber";
static const string RegName9xVersion = "VersionNumber";
static const string RegNameProductType = "ProductType";
static const string RegNameWindowsVersion = "Version";
static const string RegNameAudio = "Audio";
static const string RegNameVirtualDesktop = "Desktop";
static const string RegNameVirtualDesktopDefault = "Default";

// Other files
static const string WineGuiMetaFile = ".winegui.conf";
static const string UpdateTimestamp = ".update-timestamp";

/**
 * \brief Windows version table to convert Windows version in registry to BottleType Windows enum value.
 *  Source: https://github.com/wine-mirror/wine/blob/master/programs/winecfg/appdefaults.c#L51
 * \note Don't forget to update the hardcoded WindowsEnumSize in bottle_types.h!
 */
static const struct
{
  const BottleTypes::Windows windows;
  const string version;
  const string versionNumber;
  const string buildNumber;
  const string productType;
} WindowsVersions[] = {{BottleTypes::Windows::Windows10, "win10", "10.0", "18362", "WinNT"},
                       {BottleTypes::Windows::Windows81, "win81", "6.3", "9600", "WinNT"},
                       {BottleTypes::Windows::Windows8, "win8", "6.2", "9200", "WinNT"},
                       {BottleTypes::Windows::Windows2008R2, "win2008r2", "6.1", "7601", "ServerNT"},
                       {BottleTypes::Windows::Windows7, "win7", "6.1", "7601", "WinNT"},
                       {BottleTypes::Windows::Windows2008, "win2008", "6.0", "6002", "ServerNT"},
                       {BottleTypes::Windows::WindowsVista, "vista", "6.0", "6002", "WinNT"},
                       {BottleTypes::Windows::Windows2003, "win2003", "5.2", "3790", "ServerNT"},
                       {BottleTypes::Windows::WindowsXP, "winxp64", "5.2", "3790", "WinNT"}, // 64-bit
                       {BottleTypes::Windows::WindowsXP, "winxp", "5.1", "2600", "WinNT"},   // 32-bit
                       {BottleTypes::Windows::Windows2000, "win2k", "5.0", "2195", "WinNT"},
                       {BottleTypes::Windows::WindowsME, "winme", "4.90", "3000", ""},
                       {BottleTypes::Windows::Windows98, "win98", "4.10", "2222", ""},
                       {BottleTypes::Windows::Windows95, "win95", "4.0", "950", ""},
                       {BottleTypes::Windows::WindowsNT40, "nt40", "4.0", "1381", "WinNT"},
                       {BottleTypes::Windows::WindowsNT351, "nt351", "3.51", "1057", "WinNT"},
                       {BottleTypes::Windows::Windows31, "win31", "3.10", "0", ""},
                       {BottleTypes::Windows::Windows30, "win30", "3.0", "0", ""},
                       {BottleTypes::Windows::Windows20, "win20", "2.0", "0", ""}};

/// Meyers Singleton
Helper::Helper() = default;
/// Destructor
Helper::~Helper() = default;

/**
 * \brief Get singleton instance
 * \return Helper reference (singleton)
 */
Helper& Helper::get_instance()
{
  static Helper instance;
  return instance;
}

/****************************************************************************
 *  Public methods                                                          *
 ****************************************************************************/

/**
 * \brief Get the bottle directories within the given path. Depending on the input parameter also add the default Wine bottle (at: ~/.wine).
 * Bottles are sorted alphabetically.
 * \param[in] dir_path Path in which we look for the bottles sub-directories
 * \param[in] display_default_wine_machine If set to true, also add the default Wine bottle to the list of bottle paths
 * \return List of full path (string) of the found directories plus ~/.wine
 */
std::vector<std::string> Helper::get_bottles_paths(const string& dir_path, bool display_default_wine_machine)
{
  std::vector<std::string> list;
  list.reserve(5);
  Glib::Dir dir(dir_path);
  auto name = dir.read_name();

  while (!name.empty())
  {
    auto path = Glib::build_filename(dir_path, name);
    if (Glib::file_test(path, Glib::FileTest::FILE_TEST_IS_DIR))
    {
      list.push_back(path);
    }
    name = dir.read_name();
  }
  // Sort alphabetically (case insensitive)
  std::sort(list.begin(), list.end(), Helper::case_insensitive_compare);

  // Add default wine bottle to the end, if enabled by settings and if directory is present
  if (display_default_wine_machine && dir_exists(DefaultBottleWineDir))
  {
    list.push_back(DefaultBottleWineDir);
  }

  return list;
}

/**
 * \brief Run any program with only setting the WINEPREFIX env variable (run this method async).
 * Returns stdout output. Redirect stderr to stdout (2>&1), if you want stderr as well.
 * \param[in] prefix_path The path to wine bottle
 * \param[in] debug_log_level Debug log level
 * \param[in] program Program that gets executed (ideally full path)
 * \param[in] give_error Inform user when application exit with non-zero exit code
 * \param[in] stderr_output Also output stderr (together with stout)
 * \return Terminal stdout output
 */
string Helper::run_program(const string& prefix_path, int debug_log_level, const string& program, bool give_error, bool stderr_output)
{
  string output;

  string debug = (debug_log_level != 1) ? "WINEDEBUG=" + Helper::log_level_to_winedebug_string(debug_log_level) + " " : "";
  string exec_program = (stderr_output) ? program + " 2>&1" : program;
  string command = debug + "WINEPREFIX=\"" + prefix_path + "\" " + exec_program;
  if (give_error)
  {
    // Execute the command that also shows an error message to the user when exit code is non-zero
    output = exec_error_message(command.c_str());
  }
  else
  {
    // No error message when exit code is non-zero, but we can still return the output and log to disk (if logging is enabled)
    output = exec(command.c_str());
  }
  return output;
}

/**
 * \brief Run a Windows program under Wine (run this method async).
 * Returns stdout output. Redirect stderr to stdout (2>&1), if you want stderr as well.
 * \param[in] wine_64_bit If true use Wine 64-bit binary, false use 32-bit binary
 * \param[in] prefix_path The path to bottle wine
 * \param[in] debug_log_level Debug log level
 * \param[in] program Program/executable that will be executed (be sure your application executable is between
 * brackets in case of spaces)
 * \param[in] give_error Inform user when application exit with non-zero exit code
 * \param[in] stderr_output Also output stderr (together with stout)
 * \return Terminal stdout output
 */
string Helper::run_program_under_wine(
    bool wine_64_bit, const string& prefix_path, int debug_log_level, const string& program, bool give_error, bool stderr_output)
{
  return run_program(prefix_path, debug_log_level, Helper::get_wine_executable_location(wine_64_bit) + " " + program, give_error, stderr_output);
}

/**
 * \brief Write/append logging to WineGUI log file
 * \param logging_bottle_prefix Wine Bottle prefix location
 * \param logging Logging data
 */
void Helper::write_to_log_file(const string& logging_bottle_prefix, const string& logging)
{
  string log_path = Helper::get_log_file_path(logging_bottle_prefix);
  auto file = Gio::File::create_for_path(log_path);
  try
  {
    auto output = file->append_to(Gio::FileCreateFlags::FILE_CREATE_NONE);
    output->write(logging);
  }
  catch (const Glib::Error& ex)
  {
    std::cout << "Error: Couldn't write debug logging to log file. Error " << ex.what() << std::endl;
  }
}

/**
 * \brief Retrieve wineGUI log file path of provided bottle prefix
 * \param logging_bottle_prefix Wine Bottle prefix location
 * \return log file path
 */
string Helper::get_log_file_path(const string& logging_bottle_prefix)
{
  return Glib::build_filename(logging_bottle_prefix, "winegui.log");
}

/**
 * \brief Blocking wait (with timeout functionality) until wineserver is terminated.
 */
void Helper::wait_until_wineserver_is_terminated(const string& prefix_path)
{
  string exit_code = exec(("WINEPREFIX=\"" + prefix_path + "\" timeout 60 wineserver -w; echo $?").c_str());
  if (exit_code == "124")
  {
    std::cout << "Time-out of wineserver wait command triggered (wineserver is still running..)" << std::endl;
  }
}

/**
 * \brief Determine which type of wine executable to use
 * \return -1 on failure, 0 on 32-bit, 1 on 64-bit wine executable
 */
int Helper::determine_wine_executable()
{
  int return_status = -2;
  // Try wine 32-bit
  string output32 = exec(("command -v " + Helper::get_wine_executable_location(false) + " >/dev/null 2>&1; echo $?").c_str());
  if (!output32.empty())
  {
    // Remove new lines
    output32.erase(std::remove(output32.begin(), output32.end(), '\n'), output32.end());
    if (output32.compare("0") == 0)
    {
      return_status = 0;
    }
  }
  // Try wine 64-bit
  if (return_status == -2)
  {
    string output64 = exec(("command -v " + Helper::get_wine_executable_location(true) + " >/dev/null 2>&1; echo $?").c_str());
    if (!output64.empty())
    {
      // Remove new lines
      output64.erase(std::remove(output64.begin(), output64.end(), '\n'), output64.end());
      if (output64.compare("0") == 0)
      {
        return_status = 1;
      }
    }
  }
  if (return_status == -2)
  {
    return_status = -1;
  }
  return return_status;
}

/**
 * \brief Retrieve the Wine executable (full path if applicable)
 * \param bit64 Use Wine 64 bit or 32 bit binary
 * \return Wine binary location
 */
string Helper::get_wine_executable_location(bool bit64)
{
  if (bit64)
  {
    return WineExecutable64;
  }
  else
  {
    return WineExecutable;
  }
}

/**
 * \brief Get the Winetricks binary location
 * \return the full path to Winetricks
 */
string Helper::get_winetricks_location()
{
  string path = "";
  if (file_exists(WinetricksExecutable))
  {
    path = WinetricksExecutable;
  }
  else
  {
    std::cout << "Could not find winetricks executable!" << std::endl;
  }
  return path;
}

/**
 * \brief Get Wine version from CLI
 * \param[in] wine_64_bit If true use Wine 64-bit binary, false use 32-bit binary
 * \throws runtime_error we could not determine Wine version
 * \return Return the wine version
 */
string Helper::get_wine_version(bool wine_64_bit)
{
  string output = exec((Helper::get_wine_executable_location(wine_64_bit) + " --version").c_str());
  if (!output.empty())
  {
    std::vector<string> results = split(output, '-');
    if (results.size() >= 2)
    {
      string result2 = results.at(1);
      std::vector<string> results2 = split(result2, ' ');
      if (results2.size() >= 1)
      {
        string version = results2.at(0); // just only get the version number (eg. 6.0)
        // Remove new lines
        version.erase(std::remove(version.begin(), version.end(), '\n'), version.end());
        return version;
      }
      else
      {
        throw std::runtime_error("Could not determine Wine version?\nSomething went wrong.");
      }
    }
    else
    {
      throw std::runtime_error("Could not determine Wine version?\nSomething went wrong.");
    }
  }
  else
  {
    throw std::runtime_error("Could not receive Wine version!\n\nIs Wine installed?");
  }
}

/**
 * \brief Read data (file) from uri / url. If the contents is not too big.
 * Ignore errors (returns empty string).
 * \param uri URI/URL
 * \return data
 */
string Helper::open_file_from_uri(const string& uri)
{
  string contents;
  // Maybe also use: Glib::uri_unescape_string() ?
  auto file = Gio::File::create_for_uri(uri);
  try
  {
    gsize size = 0;
    auto stream = file->read();
    std::vector<uint8_t> buffer(stream->query_info()->get_size());
    stream->read_all(buffer.data(), buffer.size(), size);
    contents.assign(buffer.begin(), buffer.end());
  }
  catch (const Glib::Error& ex)
  {
    std::cout << "Error: Could not open and/or read from URI. " << ex.what() << std::endl;
  }
  return contents;
}

/**
 * \brief Create new Wine bottle from prefix
 * \throw Throw an error when something went wrong during the creation of the bottle
 * \param[in] wine_64_bit If true use Wine 64-bit binary, false use 32-bit binary
 * \param[in] prefix_path The path to create a Wine bottle from
 * \param[in] bit Create 32-bit Wine of 64-bit Wine bottle
 * \param[in] disable_gecko_mono Do NOT install Mono & Gecko (by default should be false)
 * \throws runtime_error when we could not not create a new Wine bottle
 */
void Helper::create_wine_bottle(bool wine_64_bit, const string& prefix_path, BottleTypes::Bit bit, const bool disable_gecko_mono)
{
  string wine_arch = "";
  switch (bit)
  {
  case BottleTypes::Bit::win32:
    wine_arch = " WINEARCH=win32";
    break;
  case BottleTypes::Bit::win64:
    wine_arch = " WINEARCH=win64";
    break;
  }
  string wine_dll_overrides = (disable_gecko_mono) ? " WINEDLLOVERRIDES=\"mscoree=d;mshtml=d\"" : "";
  string wine_command =
      "WINEPREFIX=\"" + prefix_path + "\"" + wine_arch + wine_dll_overrides + " " + Helper::get_wine_executable_location(wine_64_bit) + " wineboot";
  string command = wine_command + ">/dev/null 2>&1; echo $?";
  string output = exec(command.c_str());
  if (!output.empty())
  {
    // Remove new lines
    output.erase(std::remove(output.begin(), output.end(), '\n'), output.end());
    if (!(output.compare("0") == 0))
    {
      throw std::runtime_error("Something went wrong when creating a new Windows machine. Wine prefix: " + get_folder_name(prefix_path) +
                               "\n\nCommand executed: " + wine_command + "\nFull path location: " + prefix_path);
    }
  }
  else
  {
    throw std::runtime_error("Something went wrong when creating a new Windows machine. Wine prefix: " + get_folder_name(prefix_path) +
                             "\n\nCommand executed: " + wine_command + "\nFull location: " + prefix_path);
  }
}

/**
 * \brief Remove existing Wine bottle using prefix
 * \param[in] prefix_path - The wine bottle path which will be removed
 * \throws runtime_error when we could not remove the Wine bottle
 */
void Helper::remove_wine_bottle(const string& prefix_path)
{
  if (Helper::dir_exists(prefix_path))
  {
    string output = exec(("rm -rf \"" + prefix_path + "\"; echo $?").c_str());
    if (!output.empty())
    {
      // Remove new lines
      output.erase(std::remove(output.begin(), output.end(), '\n'), output.end());
      if (!(output.compare("0") == 0))
      {
        throw std::runtime_error("Something went wrong when removing the Windows Machine. Wine machine: " + get_folder_name(prefix_path) +
                                 "\n\nFull path location: " + prefix_path);
      }
    }
    else
    {
      throw std::runtime_error("Could not remove Windows Machine, no result. Wine machine: " + get_folder_name(prefix_path) +
                               "\n\nFull path location: " + prefix_path);
    }
  }
  else
  {
    throw std::runtime_error("Could not remove Windows Machine, prefix is not a directory. Wine machine: " + get_folder_name(prefix_path) +
                             "\n\nFull path location: " + prefix_path);
  }
}

/**
 * \brief Rename Wine bottle folder
 * \param[in] current_prefix_path - Current wine bottle path
 * \param[in] new_prefix_path - New wine bottle path
 * \throws runtime_error when we could not rename the Wine Bottle
 */
void Helper::rename_wine_bottle_folder(const string& current_prefix_path, const string& new_prefix_path)
{
  if (Helper::dir_exists(current_prefix_path))
  {
    string output = exec(("mv \"" + current_prefix_path + "\" \"" + new_prefix_path + "\"; echo $?").c_str());
    if (!output.empty())
    {
      // Remove new lines
      output.erase(std::remove(output.begin(), output.end(), '\n'), output.end());
      if (!(output.compare("0") == 0))
      {
        throw std::runtime_error("Something went wrong when renaming the Windows Machine. Wine machine: " + get_folder_name(current_prefix_path) +
                                 "\n\nCurrent full path location: " + current_prefix_path + ". Tried to rename to: " + new_prefix_path);
      }
    }
    else
    {
      throw std::runtime_error("Could not rename Windows Machine, no result. Current Wine machine: " + get_folder_name(current_prefix_path) +
                               "\n\nCurrent full path location: " + current_prefix_path + ". Tried to rename to: " + new_prefix_path);
    }
  }
  else
  {
    throw std::runtime_error("Could not rename Windows Machine, prefix is not a directory. Wine machine: " + get_folder_name(current_prefix_path) +
                             "\n\nCurrent full path location: " + current_prefix_path + ". Tried to rename to: " + new_prefix_path);
  }
}

/**
 * \brief Get Wine bottle folder name
 * \param[in] prefix_path - Bottle prefix
 * \return Folder name
 */
string Helper::get_folder_name(const string& prefix_path)
{
  // Retrieve bottle folder from the directory path
  return get_bottle_dir_from_prefix(prefix_path);
}

/**
 * \brief Get current Windows OS version
 * \param[in] prefix_path Bottle prefix
 * \throws runtime_error when Windows registry could not be opened or could not determine Windows version
 * \return Return the Windows OS version
 */
BottleTypes::Windows Helper::get_windows_version(const string& prefix_path)
{
  // Trying user registry first
  string user_reg_file_path = Glib::build_filename(prefix_path, UserReg);

  string win_version = Helper::get_reg_value(user_reg_file_path, RegKeyWine, RegNameWindowsVersion);
  if (!win_version.empty())
  {
    for (unsigned int i = 0; i < BottleTypes::WindowsEnumSize; i++)
    {
      // Check if Windows version matches the win_version string
      if (((WindowsVersions[i].version).compare(win_version) == 0))
      {
        return WindowsVersions[i].windows;
      }
    }
  }

  // Trying system registry
  string system_reg_file_path = Glib::build_filename(prefix_path, SystemReg);
  string version = "";
  if (!(version = Helper::get_reg_value(system_reg_file_path, RegKeyNameNT, RegNameNTVersion)).empty())
  {
    string build_number_nt = Helper::get_reg_value(system_reg_file_path, RegKeyNameNT, RegNameNTBuildNumber);
    string type_nt = Helper::get_reg_value(system_reg_file_path, RegKeyType, RegNameProductType);
    // Find the correct Windows version, comparing the version, build number as well as NT type (if present)
    for (unsigned int i = 0; i < BottleTypes::WindowsEnumSize; i++)
    {
      // Check if version + build number matches
      if (((WindowsVersions[i].versionNumber).compare(version) == 0) && ((WindowsVersions[i].buildNumber).compare(build_number_nt) == 0))
      {
        if (!type_nt.empty())
        {
          if ((WindowsVersions[i].productType).compare(type_nt) == 0)
          {
            return WindowsVersions[i].windows;
          }
        }
        else
        {
          return WindowsVersions[i].windows;
        }
      }
    }

    // Fall-back - return the Windows version; even if the build NT number doesn't exactly match
    for (unsigned int i = 0; i < BottleTypes::WindowsEnumSize; i++)
    {
      // Check if version + build number matches
      if ((WindowsVersions[i].versionNumber).compare(version) == 0)
      {
        if (!type_nt.empty())
        {
          if ((WindowsVersions[i].productType).compare(type_nt) == 0)
          {
            return WindowsVersions[i].windows;
          }
        }
        else
        {
          return WindowsVersions[i].windows;
        }
      }
    }
  }
  else if (!(version = Helper::get_reg_value(system_reg_file_path, RegKeyName9x, RegName9xVersion)).empty())
  {
    string current_version = "";
    string current_build_number = "";
    std::vector<string> version_list = split(version, '.');
    // Only get minor & major
    if (sizeof(version_list) >= 2)
    {
      current_version = version_list.at(0) + '.' + version_list.at(1);
    }
    // Get build number
    if (sizeof(version_list) >= 3)
    {
      current_build_number = version_list.at(2);
    }

    // Find Windows version
    for (unsigned int i = 0; i < BottleTypes::WindowsEnumSize; i++)
    {
      // Check if version + build number matches
      if (((WindowsVersions[i].versionNumber).compare(current_version) == 0) && ((WindowsVersions[i].buildNumber).compare(current_build_number) == 0))
      {
        return WindowsVersions[i].windows;
      }
    }
    // Fall-back to default Windows version, even if the build number doesn't match
    return WineDefaults::WindowsOs;
  }
  else
  {
    throw std::runtime_error("Could not determine Windows version, we assume " + BottleTypes::to_string(WineDefaults::WindowsOs) +
                             ". Wine machine: " + get_folder_name(prefix_path) + "\n\nFull location: " + prefix_path);
  }
  // Function didn't return before (meaning no match found)
  throw std::runtime_error("Could not determine Windows version, we assume " + BottleTypes::to_string(WineDefaults::WindowsOs) +
                           ". Wine machine: " + get_folder_name(prefix_path) + "\n\nFull location: " + prefix_path);
}

/**
 * \brief Get system processor bit (32/64). *Throw runtime_error* when not found.
 * \param[in] prefix_path Bottle prefix
 * \throws runtime_error when Windows registry could not be opened or could not determine Windows version
 * \return 32-bit or 64-bit
 */
BottleTypes::Bit Helper::get_windows_bitness(const string& prefix_path)
{
  string file_path = Glib::build_filename(prefix_path, UserReg);

  string meta_value_name = "arch";
  string value = Helper::Helper::get_reg_meta_data(file_path, meta_value_name);
  if (!value.empty())
  {
    if (value.compare("win32") == 0)
    {
      return BottleTypes::Bit::win32;
    }
    else if (value.compare("win64") == 0)
    {
      return BottleTypes::Bit::win64;
    }
    else
    {
      throw std::runtime_error("Could not determine Windows system bit (not win32 and not win64, value: " + value +
                               "), for Wine machine: " + get_folder_name(prefix_path) + "\n\nFull location: " + prefix_path);
    }
  }
  else
  {
    throw std::runtime_error("Could not determine Windows system bit, for Wine machine: " + get_folder_name(prefix_path) +
                             "\n\nFull location: " + prefix_path);
  }
}

/**
 * \brief Get Audio driver
 * \param[in] prefix_path Bottle prefix
 * \throws runtime_error when Windows registry could not be opened
 * \return Audio Driver (eg. alsa/coreaudio/oss/pulse)
 */
BottleTypes::AudioDriver Helper::get_audio_driver(const string& prefix_path)
{
  string file_path = Glib::build_filename(prefix_path, UserReg);
  string value = Helper::get_reg_value(file_path, RegKeyAudio, RegNameAudio);
  if (!value.empty())
  {
    if (value.compare("pulse") == 0)
    {
      return BottleTypes::AudioDriver::pulseaudio;
    }
    else if (value.compare("alsa") == 0)
    {
      return BottleTypes::AudioDriver::alsa;
    }
    else if (value.compare("oss") == 0)
    {
      return BottleTypes::AudioDriver::oss;
    }
    else if (value.compare("coreaudio") == 0)
    {
      return BottleTypes::AudioDriver::coreaudio;
    }
    else if (value.compare("disabled") == 0)
    {
      return BottleTypes::AudioDriver::disabled;
    }
    else
    {
      // Otherwise just return PulseAudio
      return BottleTypes::AudioDriver::pulseaudio;
    }
  }
  else
  {
    // If not found, it is set to PulseAudio
    return BottleTypes::AudioDriver::pulseaudio;
  }
}

/**
 * \brief Get emulation resolution
 * \param[in] prefix_path Bottle prefix
 * \throws runtime_error when Windows registry could not be opened
 * \return Return the virtual desktop resolution or empty string when disabled fully.
 */
string Helper::get_virtual_desktop(const string& prefix_path)
{
  string file_path = Glib::build_filename(prefix_path, UserReg);
  // Check if emulate desktop is enabled. Eg. "Desktop"="Default"
  string emulate_desktop_value = Helper::get_reg_value(file_path, RegKeyVirtualDesktop, RegNameVirtualDesktop);
  string resolution;
  if (!emulate_desktop_value.empty())
  {
    // The resolution can be found in Key: Software\\Wine\\Explorer\\Desktops with the Value name set as value
    // (see above, "Default" is the default value). eg. "Default"="1024x768"
    string resolution_value = Helper::get_reg_value(file_path, RegKeyVirtualDesktopResolution, RegNameVirtualDesktopDefault);
    if (!resolution_value.empty())
    {
      resolution = resolution_value;
    }
  }
  return resolution;
}

/**
 * \brief Get the date/time of the last time the Wine Inf file was updated
 * \param[in] prefix_path Bottle prefix
 * \throws runtime_error when we could not determine last wine update timestamp
 * \return Date/time of last update
 */
string Helper::get_last_wine_updated(const string& prefix_path)
{
  string file_path = Glib::build_filename(prefix_path, UpdateTimestamp);
  if (Helper::file_exists(file_path))
  {
    string epoch_time = read_file(file_path);
    epoch_time.erase(std::remove(epoch_time.begin(), epoch_time.end(), '\n'), epoch_time.end());
    if (!epoch_time.empty())
    {
      time_t secsSinceEpoch = strtoul(epoch_time.c_str(), NULL, 0);
      std::stringstream stringStream;
      stringStream << std::put_time(localtime(&secsSinceEpoch), "%c");
      return stringStream.str();
    }
    else
    {
      throw std::runtime_error("Could not determine last time wine update timestamp, for Wine machine: " + get_folder_name(prefix_path) +
                               "\n\nFull location: " + prefix_path);
    }
  }
  else
  {
    throw std::runtime_error("Could not determine last time wine update timestamp, for Wine machine: " + get_folder_name(prefix_path) +
                             "\n\nFull location: " + prefix_path);
  }
}

/**
 * \brief Get Bottle Status, to validate some bear minimal Wine stuff
 * Hint: use wait_until_wineserver_is_terminated, if you want to wait until the Bottle is fully created
 * \param[in] prefix_path Bottle prefix
 * \return True if everything is OK, otherwise false
 */
bool Helper::get_bottle_status(const string& prefix_path)
{
  // Check if some directories exists, and system registry file,
  // and finally, if we can read-out the Windows OS version without errors
  if (Helper::dir_exists(prefix_path) && Helper::dir_exists(Glib::build_filename(prefix_path, "dosdevices")) &&
      Helper::file_exists(Glib::build_filename(prefix_path, SystemReg)))
  {
    try
    {
      Helper::get_windows_version(prefix_path);
      return true;
    }
    catch (const std::runtime_error& error)
    {
      // Not good!
      return false;
    }
  }
  else
  {
    return false;
  }
}

/**
 * \brief Retrieve Linux icon path from Windows lnk path.
 * Trying to find desktop file in: ~/.local/share/applications/wine. And then search for the icon in: ~/.local/share/icons.
 * \param shortcut_path Path of lnk file under Windows
 * \throws runtime_error when we could not find the file extension or application menu item
 * \return Icon path under Linux (empty string is possible)
 */
string Helper::get_program_icon_path(const string& shortcut_path)
{
  string icon;
  std::size_t pos = shortcut_path.find("Start Menu");
  if (pos != std::string::npos)
  {
    const char* homedir;
    string path = shortcut_path.substr(pos + 11); // 11 is "Start Menu\" length
    // Get home directory under Linux
    if ((homedir = getenv("HOME")) == NULL)
    {
      homedir = getpwuid(getuid())->pw_dir;
    }
    string home_dir = std::string(homedir);
    // Convert backslash to single forward slash
    std::replace(path.begin(), path.end(), '\\', '/');
    // Add prefix
    path = home_dir + "/.local/share/applications/wine/" + path;
    // Change .lnk to .desktop extension
    std::size_t dot_pos = path.find_last_of(".");
    if (dot_pos != std::string::npos)
    {
      path.replace(dot_pos + 1, std::string::npos, "desktop");
      string file_content = Helper::read_file(path);
      // Get icon
      std::size_t icon_pos = file_content.find("Icon=");
      if (icon_pos != std::string::npos)
      {
        file_content = file_content.substr(icon_pos + 5);
        file_content.resize(file_content.find_first_of('\n'));
        //  Use the 32x32 png image
        icon = home_dir + "/.local/share/icons/hicolor/32x32/apps/" + file_content + ".png";
      }
    }
    else
    {
      throw std::runtime_error("Could not find extension in application menu item: " + shortcut_path);
    }
  }
  else
  {
    throw std::runtime_error("Application menu item is not part of the start menu: " + shortcut_path);
  }
  return icon;
}

/**
 * \brief Get C:\ Drive location
 * \param[in] prefix_path Bottle prefix
 * \throws runtime_error when we could not determine C:\ drive location
 * \return Location of C:\ location under unix
 */
string Helper::get_c_letter_drive(const string& prefix_path)
{
  // Determ C location
  string c_drive_location = Glib::build_filename(prefix_path, "dosdevices", "c:");
  if (Helper::dir_exists(prefix_path) && Helper::dir_exists(c_drive_location))
  {
    return c_drive_location;
  }
  else
  {
    throw std::runtime_error("Could not determine C:\\ drive location, for Wine machine: " + get_folder_name(prefix_path) +
                             "\n\nFull location: " + prefix_path);
  }
}

/**
 * \brief Check if *directory* exists or not
 * \param[in] dir_path The directory to be checked for existence
 * \return true if exists, otherwise false
 */
bool Helper::dir_exists(const string& dir_path)
{
  return Glib::file_test(dir_path, Glib::FileTest::FILE_TEST_IS_DIR);
}

/**
 * \brief Create directory (and intermediate parent directories if needed)
 * \param[in] dir_path The directory to be created
 * \return true if successfully created, otherwise false
 */
bool Helper::create_dir(const string& dir_path)
{
  return (g_mkdir_with_parents(dir_path.c_str(), 0775) == 0);
}

/**
 * \brief Check if regular file exists or not. True if exists.
 * \param[in] file_path The file to be checked for existence
 * \return true if exists, otherwise false
 */
bool Helper::file_exists(const string& file_path)
{
  return Glib::file_test(file_path, Glib::FileTest::FILE_TEST_IS_REGULAR);
}

/**
 * \brief Install or update Winetricks (eg. when not found locally yet)
 * Throws an error if the download and/or install was not successful.
 * \throws runtime_error when we have incorrect file permissions or the winetrick script could not be found
 */
void Helper::install_or_update_winetricks()
{
  // Check if ~/.winegui directory is created
  if (!dir_exists(WineGuiDir))
  {
    bool created = create_dir(WineGuiDir);
    if (!created)
    {
      throw std::runtime_error("Incorrect permissions to create a .winegui configuration folder! Abort.");
    }
  }

  exec(("cd \"$(mktemp -d)\" && wget -q https://raw.githubusercontent.com/Winetricks/winetricks/master/src/winetricks "
        "&& chmod +x winetricks && mv winetricks " +
        WinetricksExecutable)
           .c_str());
  // Winetricks script should exists now...
  if (!file_exists(WinetricksExecutable))
  {
    throw std::runtime_error("Winetrick helper script can not be found / installed. This could/will result into issues with WineGUI!");
  }
}

/**
 * \brief Update an existing local Winetricks, only useful if winetricks is already deployed.
 * \throws invalid_argument when we could not update the winetrick script
 * \throws runtime_error when we could not update the winetrick script
 */
void Helper::self_update_winetricks()
{
  if (file_exists(WinetricksExecutable))
  {
    string output = exec((WinetricksExecutable + " --self-update >/dev/null 2>&1; echo $?").c_str());
    if (!output.empty())
    {
      output.erase(std::remove(output.begin(), output.end(), '\n'), output.end());
      if (output.compare("0") != 0)
      {
        throw std::invalid_argument("Could not update Winetricks, keep using the v" + Helper::get_winetricks_version());
      }
    }
    else
    {
      throw std::invalid_argument("Could not update Winetricks, keep using the v" + Helper::get_winetricks_version());
    }
  }
  else
  {
    throw std::runtime_error("Try to update the Winetricks script, while there is no winetricks installed/not found!");
  }
}

/**
 * \brief Set Windows OS version by using Winetricks
 * \param[in] prefix_path Bottle prefix
 * \param[in] windows Windows version (enum)
 * \throws runtime_error when we could not set the Windows OS version
 */
void Helper::set_windows_version(const string& prefix_path, BottleTypes::Windows windows)
{
  if (file_exists(WinetricksExecutable))
  {
    string win = BottleTypes::get_winetricks_string(windows);
    string output = exec(("WINEPREFIX=\"" + prefix_path + "\" " + WinetricksExecutable + " " + win + ">/dev/null 2>&1; echo $?").c_str());
    if (!output.empty())
    {
      output.erase(std::remove(output.begin(), output.end(), '\n'), output.end());
      if (output.compare("0") != 0)
      {
        throw std::runtime_error("Could not set Windows OS version");
      }
    }
    else
    {
      throw std::runtime_error("Could not set Windows OS version");
    }
  }
}

/**
 * \brief Set custom virtual desktop resolution by using Winetricks
 * \param[in] prefix_path Bottle prefix
 * \param[in] resolution New screen resolution (eg. 1920x1080)
 * \throws runtime_error when we could not set the virtual desktop resolution
 */
void Helper::set_virtual_desktop(const string& prefix_path, string resolution)
{
  if (file_exists(WinetricksExecutable))
  {
    std::vector<string> res = split(resolution, 'x');
    if (res.size() >= 2)
    {
      int x = 0, y = 0;
      try
      {
        x = std::atoi(res.at(0).c_str());
        y = std::atoi(res.at(1).c_str());
      }
      catch (std::exception const& e)
      {
        throw std::runtime_error("Could not set virtual desktop resolution (invalid input)");
      }

      if (x < 640 || y < 480)
      {
        // Set to minimum resolution
        resolution = "640x480";
      }

      string output = exec(("WINEPREFIX=\"" + prefix_path + "\" " + WinetricksExecutable + " vd=" + resolution + ">/dev/null 2>&1; echo $?").c_str());
      if (!output.empty())
      {
        output.erase(std::remove(output.begin(), output.end(), '\n'), output.end());
        if (output.compare("0") != 0)
        {
          throw std::runtime_error("Could not set virtual desktop resolution");
        }
      }
      else
      {
        throw std::runtime_error("Could not set virtual desktop resolution");
      }
    }
    else
    {
      throw std::runtime_error("Could not set virtual desktop resolution (invalid input)");
    }
  }
}

/**
 * \brief Disable Virtual Desktop fully by using Winetricks
 * \param[in] prefix_path Bottle prefix
 * \throws runtime_error when we could not disable the virtual desktop
 */
void Helper::disable_virtual_desktop(const string& prefix_path)
{
  if (file_exists(WinetricksExecutable))
  {
    string output = exec(("WINEPREFIX=\"" + prefix_path + "\" " + WinetricksExecutable + " vd=off>/dev/null 2>&1; echo $?").c_str());
    if (!output.empty())
    {
      output.erase(std::remove(output.begin(), output.end(), '\n'), output.end());
      if (output.compare("0") != 0)
      {
        throw std::runtime_error("Could not Disable Virtual Desktop");
      }
    }
    else
    {
      throw std::runtime_error("Could not Disable Virtual Desktop");
    }
  }
}

/**
 * \brief Set Audio Driver by using Winetricks
 * \param[in] prefix_path Bottle prefix
 * \param[in] audio_driver Audio driver to be set
 * \throws runtime_error when we could not set the auditor driver
 */
void Helper::set_audio_driver(const string& prefix_path, BottleTypes::AudioDriver audio_driver)
{
  if (file_exists(WinetricksExecutable))
  {
    string audio = BottleTypes::get_winetricks_string(audio_driver);
    //
    string output = exec(("WINEPREFIX=\"" + prefix_path + "\" " + WinetricksExecutable + " sound=" + audio + ">/dev/null 2>&1; echo $?").c_str());
    if (!output.empty())
    {
      output.erase(std::remove(output.begin(), output.end(), '\n'), output.end());
      if (output.compare("0") != 0)
      {
        throw std::runtime_error("Could not set Audio driver");
      }
    }
    else
    {
      throw std::runtime_error("Could not set Audio driver");
    }
  }
}

/**
 * \brief Get menu items/links from Wine bottle
 * \param prefix_path Bottle prefix
 * \throws runtime_error when Windows registry could not be opened
 * \return vector array of menu items (links)
 */
std::vector<string> Helper::get_menu_items(const string& prefix_path)
{
  string file_path = Glib::build_filename(prefix_path, UserReg);
  // Key menu items from registry, only get the data keys containing "Start Menu" and ignore key values containing "applications-merged"
  return Helper::get_reg_keys_data_filter_ignore(file_path, RegKeyMenuFiles, "Start Menu", "applications-merged");
}

/**
 * \brief Retrieve WINEDEBUG string from debug log level
 * \param[in] log_level Log level
 * \return Wine Debug string (can be used in WINEDEBUG)
 */
string Helper::log_level_to_winedebug_string(int log_level)
{
  switch (log_level)
  {
  case 0: // Off
    return "-all";
  case 1:      // Default
    return ""; // Do nothing (default)
  case 2:      // Only errors
    return "fixme-all";
  case 3: // Warning + all (recommended for debugging)
    return "warn+all";
  case 4: // Log Frames per seconds
    return "+fps";
  case 5: // Disable D3D messages / checking for GL errors
    return "-d3d";
  case 6: // Relay + Heap
    return "+relay,+heap";
  case 7: // Relay + message box
    return "+relay,+msgbox";
  case 8: // All except relay
    return "+all,-relay";
  case 9: // Log all
    return "+all";
  default:
    return "- Unknown Log Level -";
  }
}

/**
 * \brief Get a Wine GUID based on the application name (if installed)
 * \param[in] wine_64_bit If true use Wine 64-bit binary, false use 32-bit binary
 * \param[in] prefix_path Bottle prefix
 * \param[in] application_name Application name to search for
 * \return GUID or empty string when not installed/found
 */
string Helper::get_wine_guid(bool wine_64_bit, const string& prefix_path, const string& application_name)
{
  string output = exec(("WINEPREFIX=\"" + prefix_path + "\" " + Helper::get_wine_executable_location(wine_64_bit) + " uninstaller --list | grep \"" +
                        application_name + "\" | cut -d \"{\" -f2 | cut -d \"}\" -f1")
                           .c_str());
  if (!output.empty())
  {
    output.erase(std::remove(output.begin(), output.end(), '\n'), output.end());
  }
  return output;
}

/**
 * \brief Check DLL can be found in overrides and set to a specific load order
 * \param[in] prefix_path Bottle prefix
 * \param[in] dll_name DLL Name
 * \param[in] load_order (Optional) DLL load order enum value (Default 'native')
 * \throws runtime_error when Windows registry could not be opened
 * \return True if specified load order matches the DLL overrides registry value
 */
bool Helper::get_dll_override(const string& prefix_path, const string& dll_name, DLLOverride::LoadOrder load_order)
{
  string file_path = Glib::build_filename(prefix_path, UserReg);
  string value_name = dll_name;
  string value = Helper::get_reg_value(file_path, RegKeyDllOverrides, value_name);
  return DLLOverride::to_string(load_order) == value;
}

/**
 * \brief Retrieve the uninstaller from GUID (if available)
 * \param[in] prefix_path Bottle prefix
 * \param[in] uninstallerKey GUID or application name of the uninstaller (can also be found by running: wine
 * uninstaller --list)
 * \throws runtime_error when Windows registry could not be opened
 * \return Uninstaller display name or empty string if not found
 */
string Helper::get_uninstaller(const string& prefix_path, const string& uninstallerKey)
{
  string file_path = Glib::build_filename(prefix_path, SystemReg);
  string key_name = "[Software\\\\Microsoft\\\\Windows\\\\CurrentVersion\\\\Uninstall\\\\" + uninstallerKey;
  return Helper::get_reg_value(file_path, key_name, "DisplayName");
}

/**
 * \brief Retrieve a font file_path from the system registry
 * \param[in] prefix_path Bottle prefix
 * \param[in] bit Bottle bit (32 or 64) enum
 * \param[in] fontName Font name
 * \throws runtime_error when Windows registry could not be opened
 * \return Font file_path (or empty string if not found)
 */
string Helper::get_font_filename(const string& prefix_path, BottleTypes::Bit bit, const string& fontName)
{
  string file_path = Glib::build_filename(prefix_path, SystemReg);
  string key_name = "";
  switch (bit)
  {
  case BottleTypes::Bit::win32:
    key_name = "[Software\\\\Microsoft\\\\Windows\\\\CurrentVersion\\\\Fonts]";
    break;
  case BottleTypes::Bit::win64:
    key_name = "[Software\\\\Wow6432Node\\\\Microsoft\\\\Windows\\\\CurrentVersion\\\\Fonts]";
    break;
  }
  return Helper::get_reg_value(file_path, key_name, fontName);
}

/**
 * \brief Get path to an image resource located in a global data directory (like /usr/share)
 * \param[in] filename Name of image
 * \return Path to the requested image (or empty string if not found)
 */
string Helper::get_image_location(const string& filename)
{
  // Try absolute path first
  for (string data_dir : Glib::get_system_data_dirs())
  {
    std::vector<std::string> path_builder{data_dir, "winegui", "images", filename};
    string file_path = Glib::build_path(G_DIR_SEPARATOR_S, path_builder);
    if (file_exists(file_path))
    {
      return file_path;
    }
  }

  // Try local path if the images are not installed (yet)
  // When working directory is in the build folder (relative path)
  string file_path = Glib::build_filename("../images", filename);
  // When working directory is in the build/bin folder (relative path)
  string file_path2 = Glib::build_filename("../../images", filename);
  if (file_exists(file_path))
  {
    return file_path;
  }
  else if (file_exists(file_path2))
  {
    return file_path2;
  }
  else
  {
    return "";
  }
}

/**
 * \brief Check if the prefix is equal to the default wine bottle path (~/.wine)
 * \return True if it's the default wine bottle path, otherwise false
 */
bool Helper::is_default_wine_bottle(const string& prefix_path)
{
  return (prefix_path.compare(DefaultBottleWineDir) == 0);
}

/****************************************************************************
 *  Private methods                                                         *
 ****************************************************************************/

/**
 * \brief Execute command on terminal. Returns stdout output. Redirect stderr to stdout (2>&1), if you want stderr as well.
 * \param[in] cmd The command to be executed
 * \throws runtime_error when popen failed (empty pipe pointer)
 * \return Terminal stdout output
 */
string Helper::exec(const char* cmd)
{
  // Max 128 characters
  std::array<char, 128> buffer;
  string output = "";

  // Execute command using popen,
  // And use the standard C pclose method during stream closure.
  std::unique_ptr<FILE, decltype(&pclose)> pipe(popen(cmd, "r"), &pclose);
  if (!pipe)
  {
    throw std::runtime_error("popen() failed!");
  }
  while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr)
  {
    output += buffer.data();
  }
  return output;
}

/**
 * \brief Execute command on terminal, give user an error message when exit code is non-zero.
 * Returns stdout output. Redirect stderr to stdout (2>&1), if you want stderr as well.
 * \param[in] cmd The command to be executed
 * \throws runtime_error when popen failed (empty pipe pointer)
 * \return Terminal stdout output
 */
string Helper::exec_error_message(const char* cmd)
{
  // Max 128 characters
  std::array<char, 128> buffer;
  string output = "";

  // Execute command using popen
  // Use a custom close file function during the pipe close (close_exec_stream method, see below)
  std::unique_ptr<FILE, decltype(&close_exec_stream)> pipe(popen(cmd, "r"), &close_exec_stream);
  if (!pipe)
  {
    throw std::runtime_error("popen() failed!");
  }
  while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr)
  {
    output += buffer.data();
  }
  return output;
}

/**
 * Custom fclose method, which is executed during the stream closure of C popen command.
 * Check on fclose return value, signal a failure/pop-up to the user, when exit-code is non-zero.
 */
int Helper::close_exec_stream(std::FILE* file)
{
  if (file)
  {
    if (std::fclose(file) != 0)
    {
      // Dispatcher will run the connected slot in the main loop,
      // instead of the same context/thread in case of a signal.emit() call.
      // This is needed because the CloseFile is called in a different context then usual!
      // Signal error message to the user:
      Helper::get_instance().failure_on_exec.emit();
    }
  }
  return 0; // Just always return OK
}

/**
 * \brief Write C buffer (gchar *) to file
 * \param[in] filename Filename
 * \param[in] contents - File data
 * \throw Glib::FileError
 * \return True when successful otherwise False
 */
void Helper::write_file(const string& filename, const string& contents)
{
  Glib::file_set_contents(filename, contents);
}

/**
 * \brief Read file from disk
 * \param[in] filename Filename
 * \throw Glib::FileError
 * \return The file contents
 */
string Helper::read_file(const string& filename)
{
  return Glib::file_get_contents(filename);
}

/**
 * \brief Get the Winetrick version
 * \return The version of Winetricks
 */
string Helper::get_winetricks_version()
{
  string version = "";
  if (file_exists(WinetricksExecutable))
  {
    string output = exec((WinetricksExecutable + " --version").c_str());
    if (!output.empty())
    {
      if (output.length() >= 8)
      {
        version = output.substr(0, 8); // Retrieve YYYYMMDD
      }
    }
  }
  return version;
}

/**
 * \brief Get a specific value from the Wine registry from disk
 * \param[in] file_path  File path of registry
 * \param[in] key_name   Full or part of the path of the key, always starting with '[' (eg. [Software\\\\Wine\\\\Explorer])
 * \param[in] value_name Specifies the registry value name (eg. Desktop)
 * \throws runtime_error when we couldn't load the Windows registry
 * \return Data of value name
 */
string Helper::get_reg_value(const string& file_path, const string& key_name, const string& value_name)
{
  string output;
  string value_pattern = '"' + value_name + "\"=";
  std::ifstream reg_file(file_path);
  if (reg_file.is_open())
  {
    std::string line;
    line.reserve(128);
    bool match = false;
    while (std::getline(reg_file, line))
    {
      if (!match)
      {
        match = line.starts_with(key_name);
      }
      else
      {
        std::size_t pos = line.find(value_pattern);
        if (pos != std::string::npos)
        {
          output = line.substr(pos + value_pattern.size());
          // Remove quotes
          output.erase(std::remove(output.begin(), output.end(), '\"'), output.end());
          break;
        }
      }
    }
    reg_file.close();
  }
  else
  {
    throw std::runtime_error("Could not open registry file!");
  }
  return output;
}

/**
 * \brief Get subkeys from a specific key from the Wine registry from disk
 * \param[in] file_path  File path of registry
 * \param[in] key_name   Full or part of the path of the key, always starting with '[' (eg. [Software\\\\Wine\\\\Explorer])
 * \throws runtime_error when we couldn't load the Windows registry
 * \return List all the sub keys
 */
std::vector<string> Helper::get_reg_keys(const string& file_path, const string& key_name)
{
  std::vector<string> keys;
  std::ifstream reg_file(file_path);
  if (reg_file.is_open())
  {
    std::string line;
    line.reserve(128);
    bool match = false;
    while (std::getline(reg_file, line))
    {
      if (!match)
      {
        match = line.starts_with(key_name);
      }
      else
      {
        if (line.empty() || reg_file.eof())
          break; // End of key section in registry
        if (!line.starts_with('#'))
          keys.push_back(line);
      }
    }
    reg_file.close();
  }
  else
  {
    throw std::runtime_error("Could not open registry file!");
  }
  return keys;
}

/**
 * \brief Get subkeys data from a specific key from the Wine registry from disk
 * \param[in] file_path  File path of registry
 * \param[in] key_name   Full or part of the path of the key, always starting with '[' (eg. [Software\\\\Wine\\\\Explorer])
 * \throws runtime_error when Windows registry could not be opened
 * \return List all the sub keys data (so everything after the = sign)
 */
std::vector<string> Helper::get_reg_keys_data(const string& file_path, const string& key_name)
{
  return get_reg_keys_data_filter(file_path, key_name, "");
}

/**
 * \brief Get subkeys data from a specific key and filter on a specific value from the Wine registry from disk
 * \param[in] file_path  File path of registry
 * \param[in] key_name   Full or part of the path of the key, always starting with '[' (eg. [Software\\\\Wine\\\\Explorer])
 * \param[in] key_value_filter (Optionally) Return only key value data that contains the filter (default: "", meaning no filtering)
 * \throws runtime_error when Windows registry could not be opened
 * \return List all the sub keys data (so everything after the = sign)
 */
std::vector<string> Helper::get_reg_keys_data_filter(const string& file_path, const string& key_name, const string& key_value_filter)
{
  return get_reg_keys_data_filter_ignore(file_path, key_name, key_value_filter, "");
}

/**
 * \brief Get subkeys data from a specific key and filter on a specific value from the Wine registry from disk
 * \param[in] file_path  File path of registry
 * \param[in] key_name   Full or part of the path of the key, always starting with '[' (eg. [Software\\\\Wine\\\\Explorer])
 * \param[in] key_value_filter (Optionally) Return only key value data that contains the filter (default: "", meaning no additional filtering)
 * \param[in] key_name_ignore_filter (Optionally) Filter-out the key names that contains the ignore filter (default: "", meaning everything will be
 * returned)
 * \throws runtime_error when we couldn't load the Windows registry
 * \return List all the sub keys data (so everything after the = sign)
 */
std::vector<string> Helper::get_reg_keys_data_filter_ignore(const string& file_path,
                                                            const string& key_name,
                                                            const string& key_value_filter,
                                                            const string& key_name_ignore_filter)
{
  std::vector<string> keys;
  std::ifstream reg_file(file_path);
  if (reg_file.is_open())
  {
    std::string line;
    line.reserve(128);
    bool match = false;
    while (std::getline(reg_file, line))
    {
      if (!match)
      {
        match = line.starts_with(key_name);
      }
      else
      {
        if (line.empty() || reg_file.eof())
          break; // End of key section in registry

        line = unescape_reg_key_data(line);
        // Skip '#' elements and if filter is not empty it will only continue if the line contains the filter string
        if (!line.starts_with('#') && (key_value_filter.empty() || line.find(key_value_filter) != string::npos) &&
            (key_name_ignore_filter.empty() || line.find(key_name_ignore_filter) == string::npos))
        {
          auto results = split(line, '=');
          if (results.size() > 1)
          {
            line = results.at(1);
            line.erase(std::remove(line.begin(), line.end(), '\"'), line.end());
            keys.push_back(line);
          }
        }
      }
    }
    reg_file.close();
  }
  else
  {
    throw std::runtime_error("Could not open registry file!");
  }
  return keys;
}

/**
 * \brief Get a meta value from the registry from disk
 * \param[in] file_path      File of registry
 * \param[in] meta_value_name Specifies the registry value name (eg. arch)
 * \throws runtime_error when we couldn't load the Windows registry
 * \return Data of value name
 */
string Helper::get_reg_meta_data(const string& file_path, const string& meta_value_name)
{
  string output;
  string meta_pattern = "#" + meta_value_name + "=";
  std::ifstream reg_file(file_path);
  if (reg_file.is_open())
  {
    std::string line;
    line.reserve(80);
    while (std::getline(reg_file, line))
    {
      std::size_t pos = line.find(meta_pattern);
      if (pos != std::string::npos)
      {
        output = line.substr(pos + meta_pattern.size());
        // Remove quotes
        output.erase(std::remove(output.begin(), output.end(), '\"'), output.end());
        break;
      }
    }
    reg_file.close();
  }
  else
  {
    throw std::runtime_error("Could not open registry file!");
  }
  return output;
}

/**
 * \brief Get the 'Bottle Name' (directory) from the full prefix path
 *  Can be used as fall-back.
 * \param[in] prefix_path   Full bottle prefix path
 * \return Bottle directory name
 */
string Helper::get_bottle_dir_from_prefix(const string& prefix_path)
{
  string name = "- Unknown -";
  std::size_t last_index = prefix_path.find_last_of("/\\");
  if (last_index != string::npos)
  {
    // Get only the last directory name from path (+ remove slash)
    name = prefix_path.substr(last_index + 1);
    // Remove dot if present (=hidden dir)
    size_t dot_index = name.find_first_of('.');
    if (dot_index == 0)
    {
      // Remove dot at start
      name = name.substr(1);
    }
  }
  return name;
}

/**
 * \brief Read data from file and returns it.
 * \param[in] file_path File location to be read
 * \throws runtime_error when file could not be opened
 * \return Data from file
 */
std::vector<string> Helper::read_file_lines(const string& file_path)
{
  std::vector<string> output;
  std::ifstream myfile(file_path);
  if (myfile.is_open())
  {
    std::string line;
    line.reserve(100);
    while (std::getline(myfile, line))
    {
      output.push_back(line);
    }
    myfile.close();
  }
  else
  {
    throw std::runtime_error("Could not open file!");
  }
  return output;
}

/**
 * \brief Split string by delimiter
 * \param[in] s         String to be splitted
 * \param[in] delimiter Delimiter character
 * \return Array/vector of strings
 */
std::vector<string> Helper::split(const string& s, const char delimiter)
{
  size_t start = 0;
  size_t end = s.find_first_of(delimiter);
  std::vector<string> output;
  while (end <= string::npos)
  {
    output.emplace_back(s.substr(start, end - start));
    if (end == string::npos)
      break;
    start = end + 1;
    end = s.find_first_of(delimiter, start);
  }
  return output;
}

/**
 * \brief Case-insensitive compare method, use together with sort()
 */
bool Helper::case_insensitive_compare(const std::string& a, const std::string& b)
{
  struct case_insensitive_less : public std::binary_function<char, char, bool>
  {
    bool operator()(char x, char y) const
    {
      return toupper(static_cast<unsigned char>(x)) < toupper(static_cast<unsigned char>(y));
    }
  };
  return std::lexicographical_compare(a.begin(), a.end(), b.begin(), b.end(), case_insensitive_less());
}

/**
 * \brief Parse an escaped Wine registry key data back into an UTF-8 string
 * The code is adopted from the parse_strW() method:
 * https://source.winehq.org/git/wine.git/blob/refs/heads/master:/server/unicode.c#l101
 *
 * \param[in] src Key data to be unescaped
 * \return UTF-8 string
 */
string Helper::unescape_reg_key_data(const string& src)
{
  auto to_hex = [](char ch) -> char { return std::isdigit(ch) ? ch - '0' : std::tolower(ch) - 'a' + 10; };

  auto wchar_to_utf8 = [](wchar_t wc) -> string {
    string s;
    if (0 <= wc && wc <= 0x7f)
    {
      s += (char)wc;
    }
    else if (0x80 <= wc && wc <= 0x7ff)
    {
      s += (0xc0 | (wc >> 6));
      s += (0x80 | (wc & 0x3f));
    }
    else if (0x800 <= wc && wc <= 0xffff)
    {
      s += (0xe0 | (wc >> 12));
      s += (0x80 | ((wc >> 6) & 0x3f));
      s += (0x80 | (wc & 0x3f));
    }
    else if (0x10000 <= wc && wc <= 0x1fffff)
    {
      s += (0xf0 | (wc >> 18));
      s += (0x80 | ((wc >> 12) & 0x3f));
      s += (0x80 | ((wc >> 6) & 0x3f));
      s += (0x80 | (wc & 0x3f));
    }
    else if (0x200000 <= wc && wc <= 0x3ffffff)
    {
      s += (0xf8 | (wc >> 24));
      s += (0x80 | ((wc >> 18) & 0x3f));
      s += (0x80 | ((wc >> 12) & 0x3f));
      s += (0x80 | ((wc >> 6) & 0x3f));
      s += (0x80 | (wc & 0x3f));
    }
    else if (0x4000000 <= wc && wc <= 0x7fffffff)
    {
      s += (0xfc | (wc >> 30));
      s += (0x80 | ((wc >> 24) & 0x3f));
      s += (0x80 | ((wc >> 18) & 0x3f));
      s += (0x80 | ((wc >> 12) & 0x3f));
      s += (0x80 | ((wc >> 6) & 0x3f));
      s += (0x80 | (wc & 0x3f));
    }
    return s;
  };

  string dest;
  dest.reserve(src.length());

  const char* p = src.c_str();
  while (*p)
  {
    if (*p == '\\')
    {
      p++;
      if (!*p)
        break;

      switch (*p)
      {
      case 'a':
        dest += '\a';
        p++;
        continue;
      case 'b':
        dest += '\b';
        p++;
        continue;
      case 'e':
        dest += '\e';
        p++;
        continue;
      case 'f':
        dest += '\f';
        p++;
        continue;
      case 'n':
        dest += '\n';
        p++;
        continue;
      case 'r':
        dest += '\r';
        p++;
        continue;
      case 't':
        dest += '\t';
        p++;
        continue;
      case 'v':
        dest += '\v';
        p++;
        continue;

      // hex escape
      case 'x':
        p++;
        if (!std::isxdigit(*p))
          dest += 'x';
        else
        {
          wchar_t wch = to_hex(*p++);
          if (std::isxdigit(*p))
            wch = (wch * 16) + to_hex(*p++);
          if (std::isxdigit(*p))
            wch = (wch * 16) + to_hex(*p++);
          if (std::isxdigit(*p))
            wch = (wch * 16) + to_hex(*p++);
          dest += wchar_to_utf8(wch);
        }
        continue;

      // octal escape
      case '0':
      case '1':
      case '2':
      case '3':
      case '4':
      case '5':
      case '6':
      case '7':
      {
        wchar_t wch = *p++ - '0';
        if (*p >= '0' && *p <= '7')
          wch = (wch * 8) + (*p++ - '0');
        if (*p >= '0' && *p <= '7')
          wch = (wch * 8) + (*p++ - '0');
        dest += wchar_to_utf8(wch);
        continue;
      }
      }
      // unrecognized escape: fall through to normal char handling
    }

    dest += *p++;
  }
  return dest;
}
