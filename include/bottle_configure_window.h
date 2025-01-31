/**
 * Copyright (c) 2019-2023 WineGUI
 *
 * \file    bottle_configure_window.h
 * \brief   Wine bottle configure window
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

#include <gtkmm.h>

using std::string;

// Forward declaration
class BottleItem;

/**
 * \class BottleConfigureWindow
 * \brief Wine Bottle Configure GTK Window class
 */
class BottleConfigureWindow : public Gtk::Window
{
public:
  // Signals
  sigc::signal<void, Gtk::Window&, Glib::ustring&> directx9;           /*!< Install d3dx9 for Direct3D 9 signal */
  sigc::signal<void, Gtk::Window&, Glib::ustring&> vulkan;             /*!< Install DXVK for Direct3D 9/10/11 using Vulkan signal */
  sigc::signal<void, Gtk::Window&> liberation_fonts;                   /*!< Install Liberation fonts signal */
  sigc::signal<void, Gtk::Window&> corefonts;                          /*!< Install Core fonts signal */
  sigc::signal<void, Gtk::Window&, Glib::ustring&> visual_cpp_package; /*!< Install Visual C++ package signal */
  sigc::signal<void, Gtk::Window&, Glib::ustring&> dotnet;             /*!< Install .NET signal */

  explicit BottleConfigureWindow(Gtk::Window& parent);
  virtual ~BottleConfigureWindow();

  void show();
  void set_active_bottle(BottleItem* bottle);
  void reset_active_bottle();
  void update_installed();

protected:
  // Child widgets
  Gtk::Grid configure_grid; /*!< The overall grid */

  Gtk::Toolbar first_toolbar;  /*!< 1st row toolbar */
  Gtk::Toolbar second_toolbar; /*!< 2nd row toolbar */
  // Gtk::Toolbar third_toolbar;  /*!< 3rd row toolbar */

  Gtk::Label first_row_label;  /*!< 1st row label */
  Gtk::Label hint_label;       /*!< Extra hint label info for user */
  Gtk::Label second_row_label; /*!< 2nd row label */
  // Gtk::Label third_row_label;  /*!< 3rd row label */

  // Buttons First row (Gaming)
  Gtk::ToolButton install_d3dx9_button; /*!< d3dx9 install button */
  Gtk::ToolButton install_dxvk_button;  /*!< DXVK install button */

  // Buttons Second row
  Gtk::ToolButton install_liberation_fonts_button; /*!< Liberation fonts install button */
  Gtk::ToolButton install_core_fonts_button;       /*!< Core fonts install button */
  Gtk::ToolButton install_visual_cpp_button;       /*!< MS Visual C++ Redistributable Package install button */
  Gtk::ToolButton install_dotnet4_0_button;        /*!< .NET v4.0 install button */
  Gtk::ToolButton install_dotnet4_5_2_button;      /*!< .NET v4.5.2 install button */

private:
  BottleItem* active_bottle_; /*!< Current active bottle */

  bool is_d3dx9_installed();
  bool is_dxvk_installed();
  bool is_liberation_installed();
  bool is_core_fonts_installed();
  bool is_visual_cpp_installed();
  bool is_dotnet_4_0_installed();
  bool is_dotnet_4_5_2_installed();
};
