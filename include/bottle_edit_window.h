/**
 * Copyright (c) 2019-2022 WineGUI
 *
 * \file    bottle_edit_window.h
 * \brief   Wine bottle edit window
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
#pragma once

#include "bottle_types.h"
#include "busy_dialog.h"
#include <gtkmm.h>

using std::string;

// Forward declaration
class BottleItem;

/**
 * \class BottleEditWindow
 * \brief Edit Wine bottle GTK Window class
 */
class BottleEditWindow : public Gtk::Window
{
public:
  // Signals
  sigc::signal<void, Glib::ustring&, Glib::ustring&, Glib::ustring&, BottleTypes::Windows, Glib::ustring&, BottleTypes::AudioDriver, int>
      update_bottle;                /*!< save button clicked signal */
  sigc::signal<void> remove_bottle; /*!< remove button clicked signal */

  explicit BottleEditWindow(Gtk::Window& parent);
  virtual ~BottleEditWindow();

  void show();
  void set_active_bottle(BottleItem* bottle);
  void reset_active_bottle();
  void bottle_removed();

  // Signal handlers
  virtual void on_bottle_updated();

protected:
  // Child widgets
  Gtk::Box vbox;         /*!< main vertical box */
  Gtk::Box hbox_buttons; /*!< box for buttons */
  Gtk::Grid edit_grid;   /*!< grid layout for form */

  Gtk::Label header_edit_label;                    /*!< header edit label */
  Gtk::Label name_label;                           /*!< name label */
  Gtk::Label folder_name_label;                    /*!< folder name label */
  Gtk::Label windows_version_label;                /*!< windows version label */
  Gtk::Label audio_driver_label;                   /*!< audio driver label */
  Gtk::Label virtual_desktop_resolution_label;     /*!< virtual desktop resolution label */
  Gtk::Label log_level_label;                      /*!< log level label */
  Gtk::Label description_label;                    /*!< description label */
  Gtk::Entry name_entry;                           /*!< name input field */
  Gtk::Entry folder_name_entry;                    /*!< folder name input field */
  Gtk::Entry virtual_desktop_resolution_entry;     /*!< virtual desktop resolution input field */
  Gtk::ComboBoxText windows_version_combobox;      /*!< windows version combobox */
  Gtk::ComboBoxText audio_driver_combobox;         /*!< audio driver combobox */
  Gtk::CheckButton virtual_desktop_check;          /*!< virtual desktop checkbox */
  Gtk::ComboBoxText log_level_combobox;            /*!< log level combobox */
  Gtk::ScrolledWindow description_scrolled_window; /*!< description scrolled window */
  Gtk::TextView description_text_view;             /*!< description text view */
  Gtk::Button save_button;                         /*!< save button */
  Gtk::Button cancel_button;                       /*!< cancel button */
  Gtk::Button delete_button;                       /*!< delete button */

  // Busy dialog
  BusyDialog busy_dialog; /*!< Busy dialog, when the user should wait until install is finished */
private:
  // Signal handlers
  void on_cancel_button_clicked();
  void on_save_button_clicked();
  void on_virtual_desktop_toggle();

  // Member functions
  void virtual_desktop_resolution_sensitive(bool sensitive);

  BottleItem* active_bottle_; /*!< Current active bottle */
};
