/**
 * Copyright (c) 2019-2022 WineGUI
 *
 * \file    main.cc
 * \brief   Main, where it all begins
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
#include "about_dialog.h"
#include "bottle_edit_window.h"
#include "bottle_manager.h"
#include "bottle_settings_window.h"
#include "main_window.h"
#include "menu.h"
#include "preferences_window.h"
#include "signal_dispatcher.h"

#include <gtkmm/application.h>
#include <iostream>

// Prototype
static MainWindow& setupApplication();

/**
 * \brief Main function, setup and starting the app main loop
 * \return Status code
 */
int main(int argc, char* argv[])
{
  if (argc > 1)
  {
    for (int i = 1; i < argc; ++i)
    {
      std::string arg = argv[i];
      if (arg == "--version")
      {
        // Retrieve version and print it
        std::string version = AboutDialog::get_version();
        std::cout << "WineGUI " << version << std::endl;
        return 0;
      }
    }
    std::cerr << "Error: Parameter not understood (only --version is an accepted parameter)!" << std::endl;
    return 1;
  }
  else
  {
    auto app = Gtk::Application::create("org.melroy.winegui");
    // Setup
    MainWindow& main_window = setupApplication();
    // Start main loop of GTK
    return app->run(main_window, argc, argv);
  }
}

static MainWindow& setupApplication()
{
  // Constructing the top level objects:
  static Menu menu;
  static MainWindow main_window(menu);
  static PreferencesWindow preferences_window(main_window);
  static AboutDialog about_dialog(main_window);
  static BottleEditWindow edit_window(main_window);
  static BottleSettingsWindow settings_window(main_window);
  static BottleManager manager(main_window);
  static SignalDispatcher signal_dispatcher(manager, menu, preferences_window, about_dialog, edit_window, settings_window);

  signal_dispatcher.set_main_window(&main_window);
  // Do all the signal connections of the life-time of the app
  signal_dispatcher.dispatch_signals();

  // Call the Bottle Manager prepare method,
  // it will prepare Winetricks & retrieve Wine Bottles
  manager.prepare();
  return main_window;
}
